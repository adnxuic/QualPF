#pragma once
#include "qualpf/application/product_session.h"
#include "qualpf/application/environment.h"
#include <string>
#include <string_view>
namespace qualpf::application {
enum class Kalpha2Backend { cpp, legacy, invalid };
Kalpha2Backend selectKalpha2Backend(std::string_view dedicated, std::string_view preprocessing);
Result<std::vector<double>> stripKalpha2Cpp(const Pattern &);
Result<std::vector<double>> stripKalpha2Compatible(const Pattern &);
Status stripKalpha2Operation(ProductSearchSession &, const EnvironmentReader &);
} // namespace qualpf::application
