// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#include "Search.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {
const int kHistory = 240;   // speed samples kept for the chart
}

Search::Search(QObject *parent) : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            [this]() { readOutput(QProcess::StandardOutput); });
    connect(&m_process, &QProcess::readyReadStandardError, this,
            [this]() { readOutput(QProcess::StandardError); });

    connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        // Whatever is left without a final newline
        for (int c = 0; c < 2; c++) {
            if (!m_buffer[c].isEmpty())
                handleLine(m_buffer[c], c == 1);
            m_buffer[c].clear();
        }
        if (status == QProcess::NormalExit && !m_endStatus.isEmpty() && m_endStatus != QLatin1String("error")) {
            setState(m_endStatus);
        } else {
            m_errorText = m_errorLines.join(QLatin1Char('\n'));
            if (m_errorText.isEmpty())
                m_errorText = status == QProcess::CrashExit
                    ? QStringLiteral("The engine stopped unexpectedly.")
                    : QStringLiteral("The engine exited with code %1.").arg(exitCode);
            setState(QStringLiteral("error"));
        }
        emit ended(m_state);
    });

    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;   // finished() reports the other cases
        m_errorText = QStringLiteral("The engine could not be started: %1").arg(m_process.errorString());
        setState(QStringLiteral("error"));
        emit ended(m_state);
    });
}

Search::~Search()
{
    // Closing the window saves the progress, like Ctrl+C in a terminal
    if (active()) {
        send("stop");
        m_process.closeWriteChannel();
        if (!m_process.waitForFinished(20000))
            m_process.kill();
    }
}

void Search::setState(const QString &s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged();
}

bool Search::start(const QVariantMap &config)
{
    if (active())
        return false;

    const QString engine = config.value(QStringLiteral("engine")).toString();
    const QString dataDir = config.value(QStringLiteral("dataDir")).toString();
    const QStringList addresses = config.value(QStringLiteral("addresses")).toStringList();
    m_errorText.clear();
    if (engine.isEmpty() || addresses.isEmpty()) {
        m_errorText = engine.isEmpty() ? QStringLiteral("The orion engine was not found.")
                                       : QStringLiteral("Give at least one address or prefix.");
        setState(QStringLiteral("error"));
        return false;
    }

    // Through a file: no command-line length limit, and no non-ASCII path in the arguments
    QFile targets(dataDir + QStringLiteral("/targets.txt"));
    if (!targets.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_errorText = QStringLiteral("Cannot write %1: %2").arg(targets.fileName(), targets.errorString());
        setState(QStringLiteral("error"));
        return false;
    }
    targets.write(addresses.join(QLatin1Char('\n')).toUtf8() + '\n');
    targets.close();

    QStringList args = {QStringLiteral("-json"),
                        QStringLiteral("-gpuId"), QString::number(config.value(QStringLiteral("gpuId")).toInt()),
                        QStringLiteral("-start"), config.value(QStringLiteral("start")).toString(),
                        QStringLiteral("-end"), config.value(QStringLiteral("end")).toString(),
                        QStringLiteral("-i"), QStringLiteral("targets.txt"),
                        QStringLiteral("-o"), QStringLiteral("found.txt")};
    if (config.value(QStringLiteral("random")).toBool())
        args << QStringLiteral("-random");
    else if (config.value(QStringLiteral("resume")).toBool())
        args << QStringLiteral("-backup");
    if (config.value(QStringLiteral("stop")).toBool())
        args << QStringLiteral("-stop");
    const int maxFound = config.value(QStringLiteral("maxFound")).toInt();
    if (maxFound > 0)
        args << QStringLiteral("-m") << QString::number(maxFound);

    m_buffer[0].clear();
    m_buffer[1].clear();
    m_endStatus.clear();
    m_errorLines.clear();
    m_gpuName.clear();
    m_startHex = config.value(QStringLiteral("start")).toString();
    m_endHex = config.value(QStringLiteral("end")).toString();
    m_range = 0;
    m_random = config.value(QStringLiteral("random")).toBool();
    m_backups = !m_random;
    m_targets = (int)addresses.size();
    m_threads = 0;
    m_keysPerLaunch = 0;
    m_speed = m_peakSpeed = m_keysDone = m_percent = m_elapsed = 0;
    m_eta = -1;
    m_found = 0;
    m_speedHistory.clear();
    emit startedChanged();
    emit statusChanged();

    emit logLine(QStringLiteral("$ orion ") + args.join(QLatin1Char(' ')), false);
    m_process.setWorkingDirectory(dataDir);
    m_process.start(engine, args);
    setState(QStringLiteral("starting"));
    return true;
}

void Search::send(const char *command)
{
    if (m_process.state() == QProcess::Running) {
        m_process.write(command);
        m_process.write("\n");
    }
}

void Search::pause()
{
    if (m_state == QLatin1String("running")) {
        send("pause");
        setState(QStringLiteral("pausing"));
    }
}

void Search::resume()
{
    if (m_state == QLatin1String("paused")) {
        send("resume");
        setState(QStringLiteral("resuming"));
    }
}

void Search::stop()
{
    if (active() && m_state != QLatin1String("stopping")) {
        send("stop");
        setState(QStringLiteral("stopping"));
    }
}

void Search::readOutput(QProcess::ProcessChannel channel)
{
    const int c = channel == QProcess::StandardError ? 1 : 0;
    m_process.setReadChannel(channel);
    m_buffer[c] += m_process.readAll();
    // The tool also ends lines with \r alone (progress lines in a terminal)
    for (;;) {
        int i = 0;
        while (i < m_buffer[c].size() && m_buffer[c][i] != '\n' && m_buffer[c][i] != '\r')
            i++;
        if (i == m_buffer[c].size())
            break;
        const QByteArray line = m_buffer[c].left(i);
        m_buffer[c].remove(0, i + 1);
        handleLine(line, c == 1);
    }
}

void Search::handleLine(const QByteArray &raw, bool error)
{
    const QByteArray line = raw.trimmed();
    if (line.isEmpty())
        return;
    if (!error && line.startsWith('@')) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.mid(1));
        if (doc.isObject()) {
            handleEvent(doc.object().toVariantMap());
            return;
        }
    }
    const QString text = QString::fromLocal8Bit(line);
    if (error || text.startsWith(QLatin1String("[ERROR]"))) {
        m_errorLines << text;
        while (m_errorLines.size() > 6)
            m_errorLines.removeFirst();
    }
    emit logLine(text, error || text.startsWith(QLatin1String("[ERROR]")));
}

void Search::handleEvent(const QVariantMap &e)
{
    const QString ev = e.value(QStringLiteral("event")).toString();

    if (ev == QLatin1String("start")) {
        m_gpuName = e.value(QStringLiteral("gpu")).toString();
        m_startHex = e.value(QStringLiteral("start")).toString();
        m_endHex = e.value(QStringLiteral("end")).toString();
        m_range = e.value(QStringLiteral("range")).toDouble();
        m_random = e.value(QStringLiteral("random")).toBool();
        m_backups = e.value(QStringLiteral("backups")).toBool();
        m_targets = e.value(QStringLiteral("targets")).toInt();
        m_threads = e.value(QStringLiteral("threads")).toInt();
        m_keysPerLaunch = e.value(QStringLiteral("keysPerLaunch")).toDouble();
        m_keysDone = e.value(QStringLiteral("keysDone")).toDouble();
        m_elapsed = e.value(QStringLiteral("elapsed")).toDouble();
        m_percent = m_range > 0 && !m_random ? std::min(100.0, 100.0 * m_keysDone / m_range) : 0.0;
        emit startedChanged();
        emit statusChanged();
        setState(QStringLiteral("running"));
    }
    else if (ev == QLatin1String("status")) {
        m_speed = e.value(QStringLiteral("speed")).toDouble();
        m_keysDone = e.value(QStringLiteral("keysDone")).toDouble();
        m_percent = e.value(QStringLiteral("percent")).toDouble();
        m_elapsed = e.value(QStringLiteral("elapsed")).toDouble();
        m_eta = e.value(QStringLiteral("eta")).toDouble();
        m_found = e.value(QStringLiteral("found")).toInt();
        m_peakSpeed = std::max(m_peakSpeed, m_speed);
        m_speedHistory.append(m_speed);
        while (m_speedHistory.size() > kHistory)
            m_speedHistory.removeFirst();
        emit statusChanged();
        if (m_state == QLatin1String("starting") || m_state == QLatin1String("resuming"))
            setState(QStringLiteral("running"));
    }
    else if (ev == QLatin1String("found")) {
        m_found++;
        emit statusChanged();
        emit keyFound(e);
    }
    else if (ev == QLatin1String("state")) {
        const QString s = e.value(QStringLiteral("state")).toString();
        if (s == QLatin1String("paused") && m_state != QLatin1String("stopping")) {
            m_speed = 0;
            emit statusChanged();
            setState(s);
        }
        else if (s == QLatin1String("resuming") && m_state != QLatin1String("stopping")) {
            setState(s);
        }
    }
    else if (ev == QLatin1String("end")) {
        m_endStatus = e.value(QStringLiteral("status")).toString();
        m_found = e.value(QStringLiteral("found")).toInt();
        m_speed = 0;
        emit statusChanged();
    }
}
