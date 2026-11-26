// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QProcess;

// Everything the front end needs from the orion command-line tool apart from the search itself:
// where it is, its version, the GPUs, saved progress, and checks of the user's input.
class Engine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString executable READ executable NOTIFY executableChanged)
    Q_PROPERTY(bool executableFound READ executableFound NOTIFY executableChanged)
    Q_PROPERTY(QString version READ version NOTIFY executableChanged)
    // Why a candidate engine was rejected, when none was usable
    Q_PROPERTY(QString problem READ problem NOTIFY executableChanged)
    Q_PROPERTY(QVariantList gpus READ gpus NOTIFY gpusChanged)
    Q_PROPERTY(bool listingGpus READ listingGpus NOTIFY gpusChanged)
    Q_PROPERTY(QString gpuError READ gpuError NOTIFY gpusChanged)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    explicit Engine(QObject *parent = nullptr);

    QString executable() const { return m_executable; }
    bool executableFound() const { return !m_executable.isEmpty(); }
    QString version() const { return m_version; }
    QString problem() const { return m_executable.isEmpty() ? m_problem : QString(); }
    QVariantList gpus() const { return m_gpus; }
    bool listingGpus() const { return m_gpuProcess != nullptr; }
    QString gpuError() const { return m_gpuError; }
    QString dataDir() const { return m_dataDir; }

    // Path chosen by the user (empty: search the usual places again)
    Q_INVOKABLE void setExecutable(const QString &pathOrUrl);
    Q_INVOKABLE void refreshGpus();

    // Saved progress of a sequential search on this GPU: {exists, start, end, launches, elapsed,
    // keysDone, percent} or {exists: false}
    Q_INVOKABLE QVariantMap readBackup(int gpuId) const;

    // Range checks: {ok, error, start, end, size, log2, clipped}. Hex is normalised (upper case, no 0x).
    Q_INVOKABLE QVariantMap rangeFromEnd(const QString &startHex, const QString &endHex) const;
    Q_INVOKABLE QVariantMap rangeFromBits(const QString &startHex, int bits) const;
    // First key of Bitcoin puzzle n: 2^(n-1)
    Q_INVOKABLE QString puzzleStart(int n) const;

    // One address or prefix per line ('#' comments): {addresses, type, full, prefixes, problems}
    Q_INVOKABLE QVariantMap checkTargets(const QString &text) const;

    // Keys found by earlier searches, read back from found.txt: [{address, type, wif, hex, pubkey, time}]
    Q_INVOKABLE QVariantList readFoundFile() const;

    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE void openDataDir() const;
    Q_INVOKABLE QString readTextFile(const QUrl &url) const;

    // The last search set up in the window, restored at the next start
    Q_INVOKABLE QVariantMap loadConfig() const;
    Q_INVOKABLE void saveConfig(const QVariantMap &config) const;

signals:
    void executableChanged();
    void gpusChanged();

private:
    void locateExecutable();
    QString checkExecutable(const QString &path);

    QString m_executable;
    QString m_version;
    QString m_problem;
    QVariantList m_gpus;
    QString m_gpuError;
    QString m_dataDir;
    QProcess *m_gpuProcess = nullptr;
};
