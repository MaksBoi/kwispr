#include "runtime/KwisprController.h"

#include <QDir>
#include <utility>

KwisprController::KwisprController(QString repoRoot, ProcessRunner *runner)
    : m_repoRoot(std::move(repoRoot))
    , m_runner(runner)
{
}

ProcessResult KwisprController::toggleRecording()
{
    return m_runner->run(scriptPath(), {QStringLiteral("toggle")});
}

ProcessResult KwisprController::retry(const QString &path)
{
    return m_runner->run(scriptPath(), {QStringLiteral("retry"), path});
}

QString KwisprController::scriptPath() const
{
    return QDir(m_repoRoot).filePath(QStringLiteral("kwispr.sh"));
}
