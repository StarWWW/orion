// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Orion"));
    QGuiApplication::setApplicationVersion(QStringLiteral("2.0"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Orion/icon.svg")));

    // Smooth edges on the gauge arcs and charts
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Orion desktop front end"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption pageOpt(QStringLiteral("page"),
        QStringLiteral("Page to open: search, monitor, found, learn, or puzzles (the puzzle list)."), QStringLiteral("name"));
    const QCommandLineOption startOpt(QStringLiteral("start"),
        QStringLiteral("Start the last search set up in the window right away."));
    const QCommandLineOption grabOpt(QStringLiteral("grab"),
        QStringLiteral("Save a screenshot of the window to <file> after --grab-delay ms, then quit."), QStringLiteral("file"));
    const QCommandLineOption delayOpt(QStringLiteral("grab-delay"),
        QStringLiteral("Delay before --grab, default 3000."), QStringLiteral("ms"), QStringLiteral("3000"));
    parser.addOptions({pageOpt, startOpt, grabOpt, delayOpt});
    parser.process(app);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("startPage"), parser.value(pageOpt)},
                                 {QStringLiteral("autoStart"), parser.isSet(startOpt)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Orion", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    if (parser.isSet(grabOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString file = parser.value(grabOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [window, file]() {
            window->grabWindow().save(file);
            window->close();
        });
    }

    return app.exec();
}
