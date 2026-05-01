#include "config/KwisprSettings.h"

#include "config/EnvFile.h"

#include <cmath>

namespace {
constexpr const char *LocalTranscriptionsUrl = "http://127.0.0.1:9000/v1/audio/transcriptions";
constexpr const char *OpenAiTranscriptionsUrl = "https://api.openai.com/v1/audio/transcriptions";
constexpr const char *OpenRouterChatUrl = "https://openrouter.ai/api/v1/chat/completions";

void addError(QStringList *errors, const QString &message)
{
    if (errors) {
        errors->append(message);
    }
}
}

void KwisprSettings::applyLocalPreset(const QString &localModel, const QString &localModelDir, const QString &lang)
{
    backend = "openai-transcriptions";
    apiUrl = LocalTranscriptionsUrl;
    apiKey.clear();
    model = localModel;
    language = lang;
    modelDir = localModelDir;
}

void KwisprSettings::applyOpenAiPreset(const QString &key, const QString &openAiModel, const QString &lang)
{
    backend = "openai-transcriptions";
    apiUrl = OpenAiTranscriptionsUrl;
    apiKey = key;
    model = openAiModel;
    language = lang;
}

void KwisprSettings::applyOpenRouterPreset(const QString &key, const QString &openRouterModel, const QString &prompt, const QString &format)
{
    backend = "openrouter-chat";
    apiUrl = OpenRouterChatUrl;
    apiKey = key;
    model = openRouterModel;
    transcriptionPrompt = prompt;
    audioFormat = format;
    openRouterReferer = "https://github.com/blockedby/kwispr";
    openRouterAppTitle = "KDE Whisper";
}

void KwisprSettings::writeTo(EnvFile &env) const
{
    env.setValue("KWISPR_BACKEND", backend);
    env.setValue("KWISPR_API_URL", apiUrl);
    env.setValue("KWISPR_API_KEY", apiKey);
    env.setValue("KWISPR_MODEL", model);
    env.setValue("KWISPR_LANGUAGE", language);
    env.setValue("KWISPR_PULSE_SOURCE", pulseSource);
    env.setValue("KWISPR_AUDIO_FORMAT", audioFormat);
    env.setValue("KWISPR_AUTOPASTE", autopaste ? "1" : "0");
    env.setValue("KWISPR_PASTE_HOTKEY", pasteHotkey);
    env.setValue("KWISPR_AUTOPASTE_DELAY", QString::number(autopasteDelay, 'f', 2));
    env.setValue("KWISPR_SOUNDS", sounds ? "1" : "0");

    if (!modelDir.isEmpty()) {
        env.setValue("KWISPR_MODEL_DIR", modelDir);
    }
    if (!transcriptionPrompt.isEmpty()) {
        env.setValue("KWISPR_TRANSCRIPTION_PROMPT", transcriptionPrompt);
    }
    if (!openRouterReferer.isEmpty()) {
        env.setValue("KWISPR_OPENROUTER_HTTP_REFERER", openRouterReferer);
    }
    if (!openRouterAppTitle.isEmpty()) {
        env.setValue("KWISPR_OPENROUTER_APP_TITLE", openRouterAppTitle);
    }

    env.setValue("KWISPR_VAD", vadEnabled ? "1" : "0");
    env.setValue("KWISPR_VAD_PROVIDER", vadProvider);
    env.setValue("KWISPR_VAD_MODEL", vadModelPath);
    env.setValue("KWISPR_VAD_THRESHOLD", QString::number(vadThreshold, 'g', 12));
    env.setValue("KWISPR_VAD_FRAME_MS", QString::number(vadFrameMs));
}

bool KwisprSettings::validate(QStringList *errors) const
{
    bool ok = true;

    if (apiUrl.startsWith(OpenAiTranscriptionsUrl) && apiKey.trimmed().isEmpty()) {
        ok = false;
        addError(errors, "API key is required for the official OpenAI transcription endpoint.");
    }

    const QString normalizedHotkey = pasteHotkey.trimmed().toLower();
    if (normalizedHotkey != "ctrl-v" && normalizedHotkey != "ctrl-shift-v" && normalizedHotkey != "shift-insert") {
        ok = false;
        addError(errors, "Unsupported paste hotkey. Use ctrl-v, ctrl-shift-v, or shift-insert.");
    }

    if (vadEnabled) {
        if (vadProvider.compare("silero", Qt::CaseInsensitive) == 0 && vadModelPath.trimmed().isEmpty()) {
            ok = false;
            addError(errors, "Silero VAD requires a model path.");
        }
        if (!std::isfinite(vadThreshold) || vadThreshold < 0.0) {
            ok = false;
            addError(errors, "VAD threshold must be finite and non-negative.");
        }
        if (vadFrameMs <= 0) {
            ok = false;
            addError(errors, "VAD frame duration must be greater than zero.");
        }
    }

    return ok;
}
