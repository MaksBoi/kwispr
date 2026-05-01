#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

class EnvFile {
public:
    bool load(const QString &path);
    bool save(const QString &path);

    QString value(const QString &key, const QString &fallback = QString()) const;
    void setValue(const QString &key, const QString &value);
    bool contains(const QString &key) const;
    QString errorString() const;

    static QMap<QString, QString> defaults();

private:
    enum class LineKind { Raw, Assignment };
    struct Line {
        LineKind kind = LineKind::Raw;
        QString raw;
        QString key;
        QString value;
    };

    static bool parseAssignment(const QString &line, QString *key, QString *value);
    static QString parseValue(QString text);
    static QString formatAssignment(const QString &key, const QString &value);
    static bool isBareValueSafe(const QString &value);
    static QString shellQuote(const QString &value);

    QList<Line> m_lines;
    QMap<QString, int> m_indexByKey;
    QString m_error;
};
