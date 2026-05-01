#pragma once

#include "config/EnvFile.h"
#include "config/KwisprSettings.h"
#include "models/ModelCatalog.h"

#include <QDialog>
#include <QSet>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const KwisprSettings &settings,
                            const ModelCatalog &catalog,
                            const QStringList &installedModelIds,
                            EnvFile *env = nullptr,
                            QWidget *parent = nullptr);

    bool save();
    QString lastError() const;
    KwisprSettings currentSettings() const;

signals:
    void settingsSaved(const KwisprSettings &settings);

private slots:
    void applyBackendPreset(const QString &backendLabel);

private:
    void buildUi();
    void loadFromSettings(const KwisprSettings &settings);
    void populateModels();
    KwisprSettings settingsFromWidgets() const;
    QString backendLabelForSettings(const KwisprSettings &settings) const;

    ModelCatalog m_catalog;
    QSet<QString> m_installedModelIds;
    EnvFile *m_env = nullptr;
    KwisprSettings m_settings;
    QString m_lastError;

    QComboBox *m_backendCombo = nullptr;
    QLineEdit *m_apiUrlEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QComboBox *m_localModelCombo = nullptr;
    QLineEdit *m_languageEdit = nullptr;
    QPlainTextEdit *m_promptEdit = nullptr;
    QCheckBox *m_autopasteCheck = nullptr;
    QComboBox *m_pasteHotkeyCombo = nullptr;
    QDoubleSpinBox *m_autopasteDelaySpin = nullptr;
    QCheckBox *m_vadEnabledCheck = nullptr;
    QComboBox *m_vadProviderCombo = nullptr;
    QLineEdit *m_vadModelPathEdit = nullptr;
    QDoubleSpinBox *m_vadThresholdSpin = nullptr;
    QLineEdit *m_vadFrameMsEdit = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};
