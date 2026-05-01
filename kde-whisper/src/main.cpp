#include "AppMetadata.h"
#include "ui/TrayApp.h"

#include <KAboutData>

#include <QApplication>
#include <QDir>

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

    const QString repoRoot = QDir::currentPath();
    const QString cacheDir = QDir::homePath() + QStringLiteral("/.cache/kwispr");
    TrayApp tray(repoRoot, cacheDir);

    return app.exec();
}
