#include "infrastructure/settings/preference_store.h"
#include "search_settings.h"
#include "infrastructure/serialization/metadata_values.h"
#include <QSettings>
namespace qualpf::infrastructure::settings {
application::PreferencePayload SearchSettings::option(application::ProductHandle source,const std::string &key,const application::PreferencePayload &fallback) const {
    return QtPreferenceStore::encode(value(*source,key.c_str(),QtPreferenceStore::decode(fallback)));
}
QVariant SearchSettings::value(const application::ProductState &source, const char *key,
                               const QVariant &fallback) {
    const auto options = jsonValue(application::MetadataValue{source.searchOptions}).toObject();
    if (options.contains(key))
        return options.value(key).toVariant();
    return QSettings().value(key, fallback);
}
Result<application::SearchSettingsSnapshot> SearchSettings::load(application::ProductHandle source) const {
    if (!source)
        return Result<application::SearchSettingsSnapshot>{
            Error{ErrorCode::missing_data, "settings.source", "Missing settings input"}};
    application::SearchSettingsSnapshot out;
    auto setting = [&](const char *key, const QVariant &fallback) { return value(*source, key, fallback); };
    auto &p = out.parameters;
    p.minimum_score = setting(SK_FOM, .35).toDouble();
    p.weight_position = setting(SK_2THETA, .50).toDouble();
    p.weight_intensity = setting(SK_INT, .50).toDouble();
    p.weight_phases = setting(SK_PHASES, .50).toDouble();
    const double delta = setting(SK_DELTA, .08).toDouble();
    const bool automatic = setting(SK_AUTO, true).toBool();
    p.delta_two_theta.value = automatic ? source->automaticDelta : delta;
    const int maximum = setting(SK_MAXENT, 3000).toInt();
    p.maximum_entries = maximum;
    p.filter_strongest = setting(SK_STRONGEST, true).toBool();
    p.strongest_count = 3;
    p.include_deleted_cards = !setting(SK_DELETED, true).toBool();
    p.residual_search = setting(SK_RESIDUAL, true).toBool();
    const QJsonObject persisted{{SK_FOM, p.minimum_score},
                                {SK_2THETA, p.weight_position},
                                {SK_INT, p.weight_intensity},
                                {SK_PHASES, p.weight_phases},
                                {SK_DELTA, delta},
                                {SK_AUTO, automatic},
                                {SK_RESIDUAL, p.residual_search},
                                {SK_STRONGEST, p.filter_strongest},
                                {SK_DELETED, !p.include_deleted_cards},
                                {SK_MAXENT, maximum}};
    out.persistedOptions = std::get<application::MetadataObject>(metadataValue(persisted).data);
    return Result<application::SearchSettingsSnapshot>{std::move(out)};
}
} // namespace qualpf::infrastructure::settings
