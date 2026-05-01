#include "models/ModelCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ModelCatalog ModelCatalog::load(const QString &path)
{
    ModelCatalog catalog;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        catalog.error = file.errorString();
        return catalog;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        catalog.error = parseError.errorString();
        return catalog;
    }

    const QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();
    for (const QJsonValue &value : models) {
        const QJsonObject object = value.toObject();
        const QJsonObject artifact = object.value(QStringLiteral("artifact")).toObject();
        LocalModel model;
        model.id = object.value(QStringLiteral("id")).toString();
        model.name = object.value(QStringLiteral("name")).toString();
        model.engineType = object.value(QStringLiteral("engine_type")).toString();
        model.artifactIsDirectory = artifact.value(QStringLiteral("is_directory")).toBool(false);
        model.supportsLanguageSelection = object.value(QStringLiteral("supports_language_selection")).toBool(false);
        for (const QJsonValue &language : object.value(QStringLiteral("languages")).toArray()) {
            model.languages.append(language.toString());
        }
        if (!model.id.isEmpty()) {
            catalog.models.append(model);
        }
    }

    catalog.isValid = true;
    return catalog;
}

std::optional<LocalModel> ModelCatalog::modelById(const QString &id) const
{
    for (const LocalModel &model : models) {
        if (model.id == id) {
            return model;
        }
    }
    return std::nullopt;
}
