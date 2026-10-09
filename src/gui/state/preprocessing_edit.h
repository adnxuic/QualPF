#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::gui {
using namespace application;
class PreprocessingEdit {
  public:
    using Stripper = PreprocessingEvaluation::Stripper;
    explicit PreprocessingEdit(SessionView &, bool interactive = false,
                               Stripper = application::stripKalpha2Cpp);
    Status preview(const PreprocessingRequest &);
    Status previewRange(const PreprocessingRequest &);
    const SessionStateView *preview() const { return view_ ? &*view_ : nullptr; }
    std::size_t candidateCount() const { return edit_.candidateCount(); }
    Status commit();
    void cancel();

  private:
    void refresh();
    ProductPreprocessingEdit edit_;
    std::optional<SessionStateView> view_;
};
} // namespace qualpf::gui
