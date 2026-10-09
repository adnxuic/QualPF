#include "gui/state/peak_operations.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::gui {
using namespace application;
Result<Pattern> preparePeakMeasurementContext(const SessionStateView &state,WavelengthAngstrom wave) {
    return preparePeakMeasurementContext(productState(state),wave);
}
Status preparePeakOperation(SessionStateView &next,const PreprocessingRequest &request) {
    auto draft=productState(next); auto status=preparePeakOperation(draft,request);
    if (status) next=searchView(std::make_shared<const ProductState>(std::move(draft)));
    return status;
}
}
