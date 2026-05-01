#include "runtime/LocalSttProcess.h"

#include <QDir>
#include <QProcessEnvironment>
#include <utility>

LocalSttProcess::LocalSttProcess(QString repoRoot, ProcessRunner *runner)
    : m_repoRoot(std::move(repoRoot))
    , m_runner(runner)
{
}

ProcessResult LocalSttProcess::start(const QString &modelDir)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!modelDir.isEmpty()) {
        environment.insert(QStringLiteral("KWISPR_MODEL_DIR"), modelDir);
    }

    return m_runner->run(binaryPath(),
                         {QStringLiteral("--host"),
                          QStringLiteral("127.0.0.1"),
                          QStringLiteral("--port"),
                          QStringLiteral("9000"),
                          QStringLiteral("--catalog"),
                          catalogPath()},
                         environment);
}

QString LocalSttProcess::binaryPath() const
{
    return QDir(m_repoRoot).filePath(QStringLiteral("rust-local-stt/target/release/kwispr-local-stt"));
}

QString LocalSttProcess::catalogPath() const
{
    return QDir(m_repoRoot).filePath(QStringLiteral("models/local-stt-catalog.json"));
}
