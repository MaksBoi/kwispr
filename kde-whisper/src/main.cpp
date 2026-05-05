#include "AppMetadata.h"
#include "ui/TrayApp.h"

#include <KAboutData>

#include <QApplication>
#include <QDir>
#include <QFileInfo>

namespace {
QString detectRepoRoot(const QString &executablePath)
{
    QDir dir(QFileInfo(executablePath).absoluteDir());
    for (int i = 0; i < 5; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("kwispr.sh")))
            && QFileInfo::exists(dir.filePath(QStringLiteral("models/local-stt-catalog.json")))) {
            return dir.canonicalPath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    KAboutData aboutData(
        AppMetadata::appId(),
        AppMetadata::displayName(),
        AppMetadata::version(),
        QStringLiteral("KDE tray and settings shell for Kwispr"),
        KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(aboutData);

    const QString repoRoot = detectRepoRoot(QCoreApplication::applicationFilePath());
    const QString cacheDir = QDir::homePath() + QStringLiteral("/.cache/kwispr");
    TrayApp tray(repoRoot, cacheDir);

    return app.exec();
}
