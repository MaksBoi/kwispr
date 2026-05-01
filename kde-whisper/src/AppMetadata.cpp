#include "AppMetadata.h"

namespace AppMetadata {
QString appId()
{
    return QStringLiteral("org.kwispr.KdeWhisper");
}

QString binaryName()
{
    return QStringLiteral("kde-whisper");
}

QString displayName()
{
    return QStringLiteral("KDE Whisper");
}

QString version()
{
    return QStringLiteral("0.1.0");
}
}
