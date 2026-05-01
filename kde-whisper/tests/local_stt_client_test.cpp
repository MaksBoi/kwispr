#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>

#include "runtime/LocalSttClient.h"

class FakeHttpServer : public QTcpServer
{
public:
    explicit FakeHttpServer(QByteArray body, int statusCode = 200)
        : body_(std::move(body)), statusCode_(statusCode) {}

protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        auto *socket = new QTcpSocket(this);
        QVERIFY(socket->setSocketDescriptor(socketDescriptor));
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            socket->readAll();
            const QByteArray status = statusCode_ == 200 ? "OK" : "ERROR";
            QByteArray response = "HTTP/1.1 " + QByteArray::number(statusCode_) + " " + status + "\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: " + QByteArray::number(body_.size()) + "\r\n"
                                  "Connection: close\r\n\r\n" + body_;
            socket->write(response);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

private:
    QByteArray body_;
    int statusCode_;
};

class LocalSttClientTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesHealthyVadStatus()
    {
        const QByteArray health = R"json({
            "status":"ok",
            "vad":{
                "enabled":true,
                "provider":"silero",
                "model_path":"/tmp/silero.onnx",
                "threshold":0.42,
                "frame_ms":32,
                "min_speech_ms":250,
                "padding_ms":120
            }
        })json";
        FakeHttpServer server(health);
        QVERIFY(server.listen(QHostAddress::LocalHost));

        LocalSttClient client(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
        const LocalSttStatus status = client.checkHealth();

        QCOMPARE(status.state, LocalSttState::Healthy);
        QCOMPARE(status.statusText, QStringLiteral("ok"));
        QVERIFY(status.vad.enabled);
        QCOMPARE(status.vad.provider, QStringLiteral("silero"));
        QCOMPARE(status.vad.modelPath, QStringLiteral("/tmp/silero.onnx"));
        QCOMPARE(status.vad.threshold, 0.42);
        QCOMPARE(status.vad.frameMs, 32u);
        QCOMPARE(status.vad.minSpeechMs, 250u);
        QCOMPARE(status.vad.paddingMs, 120u);
        QVERIFY(status.errorText.isEmpty());
    }

    void connectionRefusedIsStopped()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 port = server.serverPort();
        server.close();

        LocalSttClient client(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port)));
        const LocalSttStatus status = client.checkHealth();

        QCOMPARE(status.state, LocalSttState::Stopped);
        QVERIFY(!status.errorText.isEmpty());
    }

    void malformedJsonIsUnhealthy()
    {
        FakeHttpServer server(QByteArrayLiteral("not-json"));
        QVERIFY(server.listen(QHostAddress::LocalHost));

        LocalSttClient client(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
        const LocalSttStatus status = client.checkHealth();

        QCOMPARE(status.state, LocalSttState::Unhealthy);
        QVERIFY(status.errorText.contains(QStringLiteral("JSON"), Qt::CaseInsensitive));
    }
};

QTEST_MAIN(LocalSttClientTest)
#include "local_stt_client_test.moc"
