#pragma once
#include "qualpf/application/product_session.h"
#include <QString>
#include <cstdint>
#include <vector>

class QWidget;

namespace qualpf::gui {
bool confirmWorkflowImpact(QWidget *, application::ProductHandle, const std::vector<std::uint64_t> &, const QString &action);
}
