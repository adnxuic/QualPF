#pragma once
#include "qualpf/application/database/cif_record.h"
#include <optional>
namespace qualpf::application {
class CifRecordWriter {
  public:
    virtual ~CifRecordWriter() = default;
    virtual std::optional<database::WriteError> create(const std::string &base) = 0;
    virtual std::optional<database::WriteError> write(const database::CifRecord &) = 0;
    virtual void close() = 0;
};
} // namespace qualpf::application
