// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#include "Engine.h"
#include "U256.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

#ifdef Q_OS_WIN
const QString kExeName = QStringLiteral("orion.exe");
#else
const QString kExeName = QStringLiteral("orion");
#endif

// secp256k1 curve order - 1: the largest valid private key
const char *kMaxKey = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364140";

QString settingsFile(const QString &dataDir)
{
    return dataDir + QStringLiteral("/settings.ini");
}

// Base58 string -> bytes (leading '1's become zero bytes); false on a character outside the alphabet
bool decodeBase58(const QString &s, QByteArray &out)
{
    static const QByteArray alphabet("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");
    QByteArray num;                                  // big-endian
    int zeros = 0;
    while (zeros < s.size() && s.at(zeros) == QLatin1Char('1'))
        zeros++;
    for (int i = zeros; i < s.size(); i++) {
        int carry = alphabet.indexOf(s.at(i).toLatin1());
        if (carry < 0 || s.at(i).unicode() > 127)
            return false;
        for (int j = num.size() - 1; j >= 0; j--) {
            carry += 58 * (unsigned char)num[j];
            num[j] = (char)(carry & 0xFF);
            carry >>= 8;
        }
        while (carry) {
            num.prepend((char)(carry & 0xFF));
            carry >>= 8;
        }
    }
    out = QByteArray(zeros, '\0') + num;
    return true;
}

QByteArray sha256d(const QByteArray &data)
{
    return QCryptographicHash::hash(QCryptographicHash::hash(data, QCryptographicHash::Sha256),
                                    QCryptographicHash::Sha256);
}

const char *kBech32Charset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

uint32_t bech32Polymod(const QList<int> &values)
{
    static const uint32_t gen[5] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
    uint32_t chk = 1;
    for (int v : values) {
        uint32_t top = chk >> 25;
        chk = ((chk & 0x1ffffff) << 5) ^ (uint32_t)v;
        for (int i = 0; i < 5; i++)
            if ((top >> i) & 1)
                chk ^= gen[i];
    }
    return chk;
}

// Data part (after "bc1") of a lower-case bech32 string as 5-bit values, false on a bad character
bool bech32Values(const QString &data, QList<int> &values)
{
    for (QChar c : data) {
        const char *p = c.unicode() < 128 ? std::strchr(kBech32Charset, c.toLatin1()) : nullptr;
        if (!p || c.unicode() == 0)
            return false;
        values.append((int)(p - kBech32Charset));
    }
    return true;
}

// Mirrors OrionSearch::initAddress(): the type of one line, or an error message
QString classify(QString a, QString &type, bool &full)
{
    full = false;
    if (a.size() < 2)
        return QStringLiteral("too short");
    const QChar c0 = a.at(0);
    if (c0 == QLatin1Char('3'))
        return QStringLiteral("P2SH (3...) addresses cannot be searched, use 1... or bc1q...");
    if (c0 == QLatin1Char('1')) {
        type = QStringLiteral("p2pkh");
        QByteArray raw;
        if (!decodeBase58(a, raw))
            return QStringLiteral("not Base58: 0, O, I and l are not allowed");
        if (a.count(QLatin1Char('1')) == a.size())
            return a.size() > 21 ? QStringLiteral("too many leading 1s") : QString();
        if (raw.size() > 21) {
            if (raw.size() != 25 || raw.at(0) != 0)
                return QStringLiteral("too long for a prefix, not a valid P2PKH address");
            if (sha256d(raw.left(21)).left(4) != raw.mid(21))
                return QStringLiteral("wrong checksum, mistyped address?");
            full = true;
        }
        return QString();
    }
    if (c0 == QLatin1Char('b') || c0 == QLatin1Char('B')) {
        if (a != a.toLower() && a != a.toUpper())
            return QStringLiteral("mixed case is invalid in bech32");
        a = a.toLower();
        if (!a.startsWith(QLatin1String("bc1q")))
            return QStringLiteral("must start with 1 or bc1q");
        type = QStringLiteral("bech32");
        QList<int> values;
        if (!bech32Values(a.mid(3), values))
            return QStringLiteral("only the characters 023456789acdefghjklmnpqrstuvwxyz are allowed after bc1");
        if (a.size() == 62)
            return QStringLiteral("P2WSH addresses are not supported, use a 42-character bc1q address");
        if (a.size() == 42) {
            QList<int> chk = {3, 3, 0, 2, 3};   // expanded human-readable part "bc"
            chk.append(values);
            if (bech32Polymod(chk) != 1)
                return QStringLiteral("wrong checksum, mistyped address?");
            full = true;
            return QString();
        }
        if (a.size() < 5)
            return QStringLiteral("too short, give at least one character after bc1q");
        if (a.size() >= 36)
            return QStringLiteral("too long for a prefix");
        return QString();
    }
    return QStringLiteral("must start with 1 or bc1q");
}

QVariantMap rangeResult(const U256 &start, U256 end, bool clipped)
{
    const U256 size = end.sub(start).addOne();
    const double d = size.toDouble();
    return {{"ok", true}, {"error", QString()}, {"start", start.toHex()}, {"end", end.toHex()},
            {"size", d}, {"log2", std::log2(d)}, {"clipped", clipped}};
}

QVariantMap rangeError(const QString &e)
{
    return {{"ok", false}, {"error", e}};
}

bool parseStart(const QString &startHex, U256 &start, QString &error)
{
    U256 maxKey;
    U256::fromHex(QString::fromLatin1(kMaxKey), maxKey);
    if (!U256::fromHex(startHex, start)) {
        error = QStringLiteral("Start: 1 to 64 hex digits");
        return false;
    }
    if (start.isZero()) {
        error = QStringLiteral("Start: 0 is not a valid private key");
        return false;
    }
    if (start.compare(maxKey) > 0) {
        error = QStringLiteral("Start is beyond the secp256k1 curve order");
        return false;
    }
    return true;
}

} // namespace

Engine::Engine(QObject *parent) : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(m_dataDir);
    locateExecutable();
    refreshGpus();
}

QString Engine::checkExecutable(const QString &path)
{
    if (path.isEmpty() || !QFileInfo(path).isExecutable())
        return QString();
    // A build without -json rejects the option, so it is skipped like a missing one
    QProcess p;
    p.start(path, {QStringLiteral("-json"), QStringLiteral("-v")});
    if (!p.waitForFinished(5000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        p.kill();
        return QString();
    }
    return QString::fromLatin1(p.readAllStandardOutput()).trimmed();
}

void Engine::locateExecutable()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QSettings(settingsFile(m_dataDir), QSettings::IniFormat).value(QStringLiteral("engine/path")).toString(),
        qEnvironmentVariable("ORION_EXE"),
        appDir + QLatin1Char('/') + kExeName,
        appDir + QStringLiteral("/../opt/") + kExeName,     // build/gui next to build/opt (bench/build.py)
        appDir + QStringLiteral("/../../") + kExeName,      // build/gui inside a Linux checkout (make)
        QStandardPaths::findExecutable(QStringLiteral("orion")),
    };
    m_executable.clear();
    m_version.clear();
    m_problem.clear();
    for (const QString &c : candidates) {
        const QString v = checkExecutable(c);
        if (!v.isEmpty()) {
            m_executable = QDir::cleanPath(QFileInfo(c).absoluteFilePath());
            m_version = v;
            break;
        }
        if (m_problem.isEmpty() && !c.isEmpty() && QFileInfo(c).isFile())
            m_problem = QStringLiteral("%1 does not answer to -json: it is an older or different build. "
                                       "Rebuild it with the current sources.")
                                        .arg(QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(c).absoluteFilePath())));
    }
    emit executableChanged();
}

void Engine::setExecutable(const QString &pathOrUrl)
{
    QString path = pathOrUrl;
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    QSettings s(settingsFile(m_dataDir), QSettings::IniFormat);
    if (path.isEmpty())
        s.remove(QStringLiteral("engine/path"));
    else
        s.setValue(QStringLiteral("engine/path"), path);
    s.sync();
    locateExecutable();
    refreshGpus();
}

void Engine::refreshGpus()
{
    if (m_gpuProcess)
        return;
    m_gpus.clear();
    m_gpuError.clear();
    if (m_executable.isEmpty()) {
        m_gpuError = QStringLiteral("The orion engine was not found.");
        emit gpusChanged();
        return;
    }
    m_gpuProcess = new QProcess(this);
    m_gpuProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_gpuProcess, &QProcess::finished, this, [this]() {
        const QList<QByteArray> lines = m_gpuProcess->readAll().split('\n');
        QString text;
        for (QByteArray l : lines) {
            l = l.trimmed();
            if (l.startsWith('@')) {
                const QJsonObject o = QJsonDocument::fromJson(l.mid(1)).object();
                if (o.value(QStringLiteral("event")).toString() == QLatin1String("gpu"))
                    m_gpus.append(o.toVariantMap());
            } else if (!l.isEmpty()) {
                text += QString::fromLocal8Bit(l) + QLatin1Char('\n');
            }
        }
        if (m_gpus.isEmpty())
            m_gpuError = text.trimmed().isEmpty() ? QStringLiteral("No CUDA GPU found.") : text.trimmed();
        m_gpuProcess->deleteLater();
        m_gpuProcess = nullptr;
        emit gpusChanged();
    });
    connect(m_gpuProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;   // finished() follows the other errors
        m_gpuError = m_gpuProcess->errorString();
        m_gpuProcess->deleteLater();
        m_gpuProcess = nullptr;
        emit gpusChanged();
    });
    m_gpuProcess->start(m_executable, {QStringLiteral("-json"), QStringLiteral("-l")});
    emit gpusChanged();
}

QVariantMap Engine::readBackup(int gpuId) const
{
    // Layout written by writeBackup() in Orion.cpp
    QFile f(m_dataDir + QStringLiteral("/orion_backup_gpu%1.dat").arg(gpuId));
    if (!f.open(QIODevice::ReadOnly))
        return {{"exists", false}};
    const QByteArray b = f.read(96);
    if (b.size() != 96 || !b.startsWith(QByteArrayLiteral("ORIONB2\n")))
        return {{"exists", false}};
    const uchar *p = reinterpret_cast<const uchar *>(b.constData());
    const quint64 launches = qFromLittleEndian<quint64>(p + 8);
    double elapsed;
    std::memcpy(&elapsed, p + 16, sizeof(elapsed));
    U256 start, end;
    for (int i = 0; i < 4; i++) {
        start.w[i] = qFromLittleEndian<quint64>(p + 24 + 8 * i);
        end.w[i] = qFromLittleEndian<quint64>(p + 56 + 8 * i);
    }
    const quint32 threads = qFromLittleEndian<quint32>(p + 88);
    const quint32 step = qFromLittleEndian<quint32>(p + 92);
    if (end.compare(start) < 0)
        return {{"exists", false}};
    const double size = end.sub(start).addOne().toDouble();
    const double done = std::min((double)launches * step * threads, size);
    return {{"exists", true}, {"start", start.toHex()}, {"end", end.toHex()},
            {"launches", (double)launches}, {"elapsed", elapsed}, {"keysDone", done},
            {"percent", size > 0 ? 100.0 * done / size : 0.0}, {"log2", std::log2(size)}};
}

QVariantMap Engine::rangeFromEnd(const QString &startHex, const QString &endHex) const
{
    U256 start, end, maxKey;
    QString error;
    if (!parseStart(startHex, start, error))
        return rangeError(error);
    U256::fromHex(QString::fromLatin1(kMaxKey), maxKey);
    if (!U256::fromHex(endHex, end))
        return rangeError(QStringLiteral("End: 1 to 64 hex digits"));
    if (end.compare(maxKey) > 0)
        return rangeError(QStringLiteral("End is beyond the secp256k1 curve order"));
    if (end.compare(start) < 0)
        return rangeError(QStringLiteral("End is lower than start"));
    return rangeResult(start, end, false);
}

QVariantMap Engine::rangeFromBits(const QString &startHex, int bits) const
{
    U256 start, maxKey;
    QString error;
    if (!parseStart(startHex, start, error))
        return rangeError(error);
    if (bits < 0 || bits > 256)          // 0: the start key alone (puzzle #1)
        return rangeError(QStringLiteral("Bits: 1 to 256"));
    U256::fromHex(QString::fromLatin1(kMaxKey), maxKey);
    U256 end = maxKey;
    bool clipped = true;
    if (bits < 256) {
        U256 last;                                   // start + 2^bits - 1
        if (U256::pow2(bits).add(start, last)) {
            last = last.sub(U256::pow2(0));
            if (last.compare(maxKey) <= 0) {
                end = last;
                clipped = false;
            }
        }
    }
    return rangeResult(start, end, clipped);
}

QString Engine::puzzleStart(int n) const
{
    if (n < 1 || n > 256)
        return QString();
    return U256::pow2(n - 1).toHex();
}

QVariantMap Engine::checkTargets(const QString &text) const
{
    QStringList addresses, problems;
    QString type;
    int full = 0, prefixes = 0, lineNo = 0;
    for (QString line : text.split(QLatin1Char('\n'))) {
        lineNo++;
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        QString t;
        bool isFull = false;
        const QString error = classify(line, t, isFull);
        if (!error.isEmpty()) {
            problems << QStringLiteral("Line %1: %2").arg(lineNo).arg(error);
            continue;
        }
        if (!type.isEmpty() && t != type) {
            problems << QStringLiteral("Line %1: 1... and bc1q... targets cannot be mixed in one search").arg(lineNo);
            continue;
        }
        type = t;
        addresses << (t == QLatin1String("bech32") ? line.toLower() : line);
        isFull ? full++ : prefixes++;
    }
    addresses.removeDuplicates();
    return {{"addresses", addresses}, {"type", type}, {"full", full}, {"prefixes", prefixes},
            {"problems", problems}};
}

QVariantList Engine::readFoundFile() const
{
    // Format written by OrionSearch::output(): blocks of "Label: value" lines
    QFile f(m_dataDir + QStringLiteral("/found.txt"));
    QVariantList keys;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return keys;
    QVariantMap cur;
    auto flush = [&]() {
        if (!cur.value(QStringLiteral("address")).toString().isEmpty() && !cur.value(QStringLiteral("hex")).toString().isEmpty())
            keys.append(cur);
        cur.clear();
    };
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString label = line.left(colon).simplified();
        const QString value = line.mid(colon + 1).trimmed();
        if (label == QLatin1String("Public Addr")) {
            flush();
            cur[QStringLiteral("address")] = value;
        } else if (label == QLatin1String("Priv (WIF)")) {
            const int p = value.indexOf(QLatin1Char(':'));
            cur[QStringLiteral("type")] = p > 0 ? value.left(p) : QString();
            cur[QStringLiteral("wif")] = p > 0 ? value.mid(p + 1) : value;
        } else if (label == QLatin1String("Priv (HEX)")) {
            cur[QStringLiteral("hex")] = value.startsWith(QLatin1String("0x")) ? value.mid(2) : value;
        } else if (label == QLatin1String("Pub Key")) {
            cur[QStringLiteral("pubkey")] = value;
        } else if (label == QLatin1String("Found at")) {
            cur[QStringLiteral("time")] = value;
        }
    }
    flush();
    return keys;
}

void Engine::copyText(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

void Engine::openDataDir() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDir));
}

QString Engine::readTextFile(const QUrl &url) const
{
    QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.read(16 * 1024 * 1024));
}

// Kept as JSON text, readable and editable in settings.ini
QVariantMap Engine::loadConfig() const
{
    const QString json = QSettings(settingsFile(m_dataDir), QSettings::IniFormat).value(QStringLiteral("search/last")).toString();
    return QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap();
}

void Engine::saveConfig(const QVariantMap &config) const
{
    QSettings s(settingsFile(m_dataDir), QSettings::IniFormat);
    s.setValue(QStringLiteral("search/last"),
               QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact)));
}
