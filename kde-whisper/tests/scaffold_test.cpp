#include <QtTest/QtTest>

#include "AppMetadata.h"

class ScaffoldTest : public QObject {
    Q_OBJECT

private slots:
    void exposesExpectedMetadata()
    {
        QCOMPARE(AppMetadata::appId(), QStringLiteral("org.kwispr.KdeWhisper"));
        QCOMPARE(AppMetadata::binaryName(), QStringLiteral("kde-whisper"));
        QCOMPARE(AppMetadata::displayName(), QStringLiteral("KDE Whisper"));
    }
};

QTEST_MAIN(ScaffoldTest)
#include "scaffold_test.moc"
