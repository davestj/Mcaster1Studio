#include "MainWindow.h"
#include "version.h"
#include <QApplication>
#include <QStyleFactory>
#include <QSettings>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QMutex>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

// ─── File-based debug logger (captures qInfo/qWarning/qCritical to disk) ─────
static QFile* g_logFile = nullptr;
static QMutex g_logMutex;

static void fileMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    QMutexLocker lock(&g_logMutex);
    if (!g_logFile || !g_logFile->isOpen()) return;
    const char* level = "DBG";
    switch (type) {
        case QtDebugMsg:    level = "DBG"; break;
        case QtInfoMsg:     level = "INF"; break;
        case QtWarningMsg:  level = "WRN"; break;
        case QtCriticalMsg: level = "CRT"; break;
        case QtFatalMsg:    level = "FAT"; break;
    }
    const QString line = QString("[%1] %2: %3\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"), level, msg);
    g_logFile->write(line.toUtf8());
    g_logFile->flush();
    // Also forward to default handler (OutputDebugString on Windows)
    (void)ctx;
}

#ifdef _WIN32
// ─── Windows crash handler — writes minidump + log entry on segfault ──────────
static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    // Write crash info to log
    {
        QMutexLocker lock(&g_logMutex);
        if (g_logFile && g_logFile->isOpen()) {
            const QString line = QString("[%1] FAT: *** CRASH *** Exception 0x%2 at address 0x%3\n")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                .arg((unsigned long)ep->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'))
                .arg((quintptr)ep->ExceptionRecord->ExceptionAddress, 16, 16, QChar('0'));
            g_logFile->write(line.toUtf8());
            g_logFile->flush();
        }
    }
    // Write minidump (portable: logs/ subdirectory)
    const QString dumpPath = QDir::currentPath() + "/logs/mcaster1studio_crash.dmp";
    HANDLE hFile = CreateFileW(dumpPath.toStdWString().c_str(), GENERIC_WRITE,
                               0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpWithDataSegs, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(crashHandler);
#endif

    // Force FFmpeg multimedia backend — the default Windows Media Foundation
    // backend cannot decode OGG/Vorbis and has unreliable QAudioDecoder support.
    // Must be set BEFORE QApplication is constructed.
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // Install file-based debug logger (portable: logs/ subdirectory)
    {
        const QString logDir = QDir::currentPath() + "/logs";
        QDir().mkpath(logDir);
        g_logFile = new QFile(logDir + "/mcaster1studio_debug.log");
    }
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qInstallMessageHandler(fileMessageHandler);
    }

    QApplication app(argc, argv);

    // Application identity
    app.setApplicationName(MCASTER1STUDIO_APP_NAME);
    app.setApplicationVersion(MCASTER1STUDIO_VERSION_STRING);
    app.setOrganizationName(MCASTER1STUDIO_ORG_NAME);
    app.setOrganizationDomain(MCASTER1STUDIO_ORG_DOMAIN);

    // Use Fusion style for full QSS theming support.
    // Windows11 style does not honor custom QSS backgrounds.
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set working dir to executable location so relative paths work
    // (plugins/, themes/, etc.)
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // ── Portable mode ─────────────────────────────────────────────────
    // Store ALL settings as INI files inside the install directory.
    // No Windows Registry, no AppData, no Roaming profile.
    // QSettings("Mcaster1","Mcaster1Studio") → <appDir>/config/Mcaster1/Mcaster1Studio.ini
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString configDir = appDir + "/config";
    QDir().mkpath(configDir);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, configDir);

    qInfo() << "Starting" << MCASTER1STUDIO_APP_NAME << "v" << MCASTER1STUDIO_VERSION_STRING;
    qInfo() << "Portable config:" << configDir;
    qInfo() << "Qt version:" << qVersion();

    MainWindow win;
    win.show();

    return app.exec();
}
