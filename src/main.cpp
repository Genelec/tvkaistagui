#include <QApplication>
#include <QFileInfo>
#include <QtDebug>
#include <QtGlobal>
#include <QLocale>
#include <QTranslator>
#include <stdio.h>
#include <stdlib.h>
#include "mainwindow.h"

void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();

    bool debugprintstate = false;

    switch (type) {
    case QtDebugMsg:
        if (debugprintstate) fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

void defaultMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    handleMessage(type, context, msg);
}

void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    handleMessage(type, context, msg);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TVkaistaGUI");

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::applicationName(),
                       QCoreApplication::applicationName());

    QStringList arguments = app.arguments();
    int count = arguments.size();
    bool invalidArgs = false;

    qInstallMessageHandler(defaultMessageHandler);

    for (int i = 1; i < count; i++) {
        QString arg = arguments.at(i);

        if (arg == "-d" || arg == "--debug") {
            qInstallMessageHandler(debugMessageHandler);
        }
        else if (arg == "-p" || arg == "--http-proxy") {
            if (i + 1 >= count) {
                invalidArgs = true;
                continue;
            }

            QString proxyString = arguments.at(++i);
            QString proxyHost = proxyString;
            int proxyPort = 8080;
            int pos = proxyString.lastIndexOf(':');

            if (pos >= 0) {
                proxyHost = proxyString.mid(0, pos);
                proxyPort = proxyString.mid(pos + 1).toInt();
            }

            settings.beginGroup("client");
            settings.beginGroup("proxy");
            settings.setValue("host", proxyHost);
            settings.setValue("port", proxyPort);
            settings.endGroup();
            settings.endGroup();
        }
        else {
            invalidArgs = true;
        }
    }

    if (invalidArgs) {
        fprintf(stderr, "Usage: tvkaistagui [-d|--debug]");
        return 1;
    }

#ifdef TVKAISTAGUI_TRANSLATIONS_DIR
    QString translationsDir = TVKAISTAGUI_TRANSLATIONS_DIR;
#else
    QString translationsDir = app.applicationDirPath();
    translationsDir.append("/translations");
#endif

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(), translationsDir);
    app.installTranslator(&qtTranslator);
    MainWindow window;
    window.show();
    return app.exec();
}
