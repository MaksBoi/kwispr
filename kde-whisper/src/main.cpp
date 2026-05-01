#include "AppMetadata.h"

#include <KAboutData>

#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KAboutData aboutData(
        AppMetadata::appId(),
        AppMetadata::displayName(),
        AppMetadata::version(),
        QStringLiteral("KDE tray and settings shell for Kwispr"),
        KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(aboutData);

    QLabel placeholder(QStringLiteral("KDE Whisper settings shell"));
    placeholder.setWindowTitle(AppMetadata::displayName());
    placeholder.resize(360, 120);
    placeholder.show();

    return app.exec();
}
