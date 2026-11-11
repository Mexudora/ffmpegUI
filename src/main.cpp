#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QTranslator>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ffmpegUI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // The translator must outlive the QML engine and be installed before loading QML.
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("ffmpegui"),
                        QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&translator);
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() {
            qCritical().noquote() << QCoreApplication::translate(
                "main", "Failed to load the application interface.");
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("FFmpegUI", "Main");

    return app.exec();
}
