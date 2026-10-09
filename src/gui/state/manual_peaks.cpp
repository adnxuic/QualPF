#include "gui/state/manual_peaks.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::gui {
using namespace application;
Status prepareManualPeaks(SessionStateView &next,const ManualPeakChange &change,WavelengthAngstrom wave,AngularWidthDegrees tolerance) {
    auto draft=productState(next); auto status=prepareManualPeaks(draft,change,wave,tolerance);
    if (status) next=searchView(std::make_shared<const ProductState>(std::move(draft)));
    return status;
}
}
