#include "runtime/LocalSttClient.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

LocalSttClient::LocalSttClient(QUrl baseUrl)
    : baseUrl_(std::move(baseUrl))
{
}

LocalSttStatus LocalSttClient::checkHealth(int timeoutMs) const
{
    LocalSttStatus status;

    QNetworkAccessManager manager;
    QUrl url = baseUrl_;
    url.setPath(QStringLiteral("/health"));
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        status.state = LocalSttState::Unhealthy;
        status.errorText = QStringLiteral("Timed out checking local STT health");
        reply->deleteLater();
        return status;
    }

    if (reply->error() != QNetworkReply::NoError) {
        status.errorText = reply->errorString();
        if (reply->error() == QNetworkReply::ConnectionRefusedError ||
            reply->error() == QNetworkReply::HostNotFoundError) {
            status.state = LocalSttState::Stopped;
        } else {
            status.state = LocalSttState::Unhealthy;
        }
        reply->deleteLater();
        return status;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        status.state = LocalSttState::Unhealthy;
        status.errorText = QStringLiteral("Malformed JSON health response: %1").arg(parseError.errorString());
        return status;
    }

    const QJsonObject root = doc.object();
    const QString statusText = root.value(QStringLiteral("status")).toString();
    if (statusText != QStringLiteral("ok")) {
        status.state = LocalSttState::Unhealthy;
        status.statusText = statusText;
        status.errorText = QStringLiteral("Local STT health status is not ok");
        return status;
    }

    const QJsonObject vad = root.value(QStringLiteral("vad")).toObject();
    status.state = LocalSttState::Healthy;
    status.statusText = statusText;
    status.vad.enabled = vad.value(QStringLiteral("enabled")).toBool(false);
    status.vad.provider = vad.value(QStringLiteral("provider")).toString();
    status.vad.modelPath = vad.value(QStringLiteral("model_path")).toString();
    status.vad.threshold = vad.value(QStringLiteral("threshold")).toDouble(0.0);
    status.vad.frameMs = static_cast<unsigned int>(vad.value(QStringLiteral("frame_ms")).toInt(0));
    status.vad.minSpeechMs = static_cast<unsigned int>(vad.value(QStringLiteral("min_speech_ms")).toInt(0));
    status.vad.paddingMs = static_cast<unsigned int>(vad.value(QStringLiteral("padding_ms")).toInt(0));
    return status;
}
