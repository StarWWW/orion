// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#pragma once

#include <QObject>
#include <QProcess>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// One search: runs "orion -json ..." and turns its event lines into properties and signals.
class Search : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // idle, starting, running, pausing, paused, resuming, stopping,
    // finished, allfound, stopped, error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

    Q_PROPERTY(QString gpuName READ gpuName NOTIFY startedChanged)
    Q_PROPERTY(QString startHex READ startHex NOTIFY startedChanged)
    Q_PROPERTY(QString endHex READ endHex NOTIFY startedChanged)
    Q_PROPERTY(double range READ range NOTIFY startedChanged)
    Q_PROPERTY(bool random READ random NOTIFY startedChanged)
    Q_PROPERTY(bool backups READ backups NOTIFY startedChanged)
    Q_PROPERTY(int targets READ targets NOTIFY startedChanged)
    Q_PROPERTY(int threads READ threads NOTIFY startedChanged)
    Q_PROPERTY(double keysPerLaunch READ keysPerLaunch NOTIFY startedChanged)

    Q_PROPERTY(double speed READ speed NOTIFY statusChanged)          // MK/s
    Q_PROPERTY(double peakSpeed READ peakSpeed NOTIFY statusChanged)
    Q_PROPERTY(double keysDone READ keysDone NOTIFY statusChanged)
    Q_PROPERTY(double percent READ percent NOTIFY statusChanged)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY statusChanged)      // seconds
    Q_PROPERTY(double eta READ eta NOTIFY statusChanged)              // seconds, -1 unknown
    Q_PROPERTY(int found READ found NOTIFY statusChanged)
    Q_PROPERTY(QVariantList speedHistory READ speedHistory NOTIFY statusChanged)

public:
    explicit Search(QObject *parent = nullptr);
    ~Search() override;

    QString state() const { return m_state; }
    bool active() const { return m_process.state() != QProcess::NotRunning; }
    QString errorText() const { return m_errorText; }
    QString gpuName() const { return m_gpuName; }
    QString startHex() const { return m_startHex; }
    QString endHex() const { return m_endHex; }
    double range() const { return m_range; }
    bool random() const { return m_random; }
    bool backups() const { return m_backups; }
    int targets() const { return m_targets; }
    int threads() const { return m_threads; }
    double keysPerLaunch() const { return m_keysPerLaunch; }
    double speed() const { return m_speed; }
    double peakSpeed() const { return m_peakSpeed; }
    double keysDone() const { return m_keysDone; }
    double percent() const { return m_percent; }
    double elapsed() const { return m_elapsed; }
    double eta() const { return m_eta; }
    int found() const { return m_found; }
    QVariantList speedHistory() const { return m_speedHistory; }

    // config: engine (path), dataDir, addresses (list), start, end (hex), random, stop, resume (-backup),
    // gpuId, maxFound. False (with errorText) when it could not be started.
    Q_INVOKABLE bool start(const QVariantMap &config);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    // Saves the progress (sequential mode) and ends the search
    Q_INVOKABLE void stop();

signals:
    void stateChanged();
    void startedChanged();
    void statusChanged();
    void keyFound(const QVariantMap &key);
    void logLine(const QString &text, bool error);
    void ended(const QString &status);

private:
    void setState(const QString &s);
    void readOutput(QProcess::ProcessChannel channel);
    void handleLine(const QByteArray &line, bool error);
    void handleEvent(const QVariantMap &e);
    void send(const char *command);

    QProcess m_process;
    QByteArray m_buffer[2];
    QString m_state = QStringLiteral("idle");
    QString m_errorText;
    QString m_endStatus;
    QStringList m_errorLines;

    QString m_gpuName, m_startHex, m_endHex;
    double m_range = 0, m_keysPerLaunch = 0;
    bool m_random = false, m_backups = false;
    int m_targets = 0, m_threads = 0;

    double m_speed = 0, m_peakSpeed = 0, m_keysDone = 0, m_percent = 0, m_elapsed = 0, m_eta = -1;
    int m_found = 0;
    QVariantList m_speedHistory;
};
