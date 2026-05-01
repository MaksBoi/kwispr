#include "ui/TrayApp.h"

#include "config/KwisprSettings.h"
#include "models/ModelCatalog.h"
#include "runtime/KwisprController.h"
#include "runtime/LocalSttClient.h"
#include "runtime/LocalSttProcess.h"
#include "runtime/ProcessRunner.h"
#include "ui/SettingsDialog.h"

#include <KStatusNotifierItem>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>

TrayApp::TrayApp(QString repoRoot, QString cacheDir, QObject *parent)
    : QObject(parent)
    , m_repoRoot(std::move(repoRoot))
    , m_cacheDir(std::move(cacheDir))
    , m_controller(std::make_unique<TrayController>(this, m_cacheDir, this))
    , m_notifier(new KStatusNotifierItem(QStringLiteral("org.kwispr.KdeWhisper"), this))
{
    m_notifier->setTitle(QStringLiteral("KDE Whisper"));
    m_notifier->setIconByName(QStringLiteral("audio-input-microphone"));
    m_notifier->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_notifier->setContextMenu(m_controller->menu());
    m_notifier->setStatus(KStatusNotifierItem::Active);
}

TrayApp::~TrayApp() = default;

void TrayApp::toggleRecording()
{
    ProcessRunner runner;
    KwisprController controller(m_repoRoot, &runner);
    const ProcessResult result = controller.toggleRecording();
    if (result.exitCode != 0) {
        QMessageBox::warning(nullptr, QStringLiteral("KDE Whisper"), result.stderrText.isEmpty() ? QStringLiteral("Toggle recording failed.") : result.stderrText);
    }
}

void TrayApp::openSettings()
{
    KwisprSettings settings;
    const ModelCatalog catalog = ModelCatalog::load(m_repoRoot + QStringLiteral("/models/local-stt-catalog.json"));
    SettingsDialog dialog(settings, catalog, {}, nullptr);
    dialog.exec();
}

void TrayApp::startLocalStt()
{
    ProcessRunner runner;
    LocalSttProcess process(m_repoRoot, &runner);
    const ProcessResult result = process.start();
    if (result.exitCode != 0) {
        QMessageBox::warning(nullptr, QStringLiteral("KDE Whisper"), result.stderrText.isEmpty() ? QStringLiteral("Failed to start local STT.") : result.stderrText);
    }
    m_controller->refreshState();
}

void TrayApp::stopLocalStt()
{
    QMessageBox::information(nullptr, QStringLiteral("KDE Whisper"), QStringLiteral("Stopping managed local STT processes will be implemented in a later task."));
    m_controller->refreshState();
}

void TrayApp::downloadVerifyModels()
{
    QMessageBox::information(nullptr, QStringLiteral("KDE Whisper"), QStringLiteral("Model download/verify UI will be implemented in a later task."));
}

void TrayApp::retryLastFailed()
{
    const QString path = m_cacheDir + QStringLiteral("/last-failed.txt");
    ProcessRunner runner;
    KwisprController controller(m_repoRoot, &runner);
    const ProcessResult result = controller.retry(path);
    if (result.exitCode != 0) {
        QMessageBox::warning(nullptr, QStringLiteral("KDE Whisper"), result.stderrText.isEmpty() ? QStringLiteral("Retry failed.") : result.stderrText);
    }
    m_controller->refreshState();
}

void TrayApp::quitApplication()
{
    qApp->quit();
}

LocalSttState TrayApp::localSttState() const
{
    LocalSttClient client(QUrl(QStringLiteral("http://127.0.0.1:9000")));
    return client.checkHealth(500).state;
}
