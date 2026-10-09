#pragma once
#include "qualpf/application/preferences.h"
#include <QVariant>
namespace qualpf::infrastructure {
class QtPreferenceStore final : public application::PreferenceStore {
  public:
    static application::PreferencePayload encode(const QVariant &);
    static QVariant decode(const application::PreferencePayload &);
    application::PreferencePayload read(const std::string &, const application::PreferencePayload &) const override;
    void write(const std::string &, const application::PreferencePayload &) override;
    bool contains(const std::string &) const override;
    void remove(const std::string &) override;
    std::string fileName() const override;
};
} // namespace qualpf::infrastructure
