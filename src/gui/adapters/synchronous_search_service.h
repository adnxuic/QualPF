#include "gui/search/search_request.h"
#pragma once

#include "gui/state/search_session.h"
#include "qualpf/application/reference_database.h"
#include "qualpf/application/search_coordinator.h"

namespace qualpf::gui {
// CLI and compatibility-test request/projection adapter. It owns no numerical
// state, scorer or cancellation source. Interactive GUI search uses queued tasks.
class SynchronousSearchService {
  public:
    using ProgressCallback = std::function<void(int, int)>;
    SynchronousSearchService(application::SearchCoordinator &coordinator, application::CandidateReader &reader,
                             std::function<application::ReferenceDatabase()> database = {})
        : execution_(coordinator), reader_(reader), database_(std::move(database)) {}
    QVector<CardType> makeQuery(const qualpf::gui::SearchRequest &, ProgressCallback = {});
    QVector<CardType> makeMatch(const qualpf::gui::SearchRequest &, std::size_t maximum, bool strongest, ProgressCallback = {});
    void makeQueryStrongest(const qualpf::gui::SearchRequest &, QVector<CardType> &, ProgressCallback = {});
    void makeQueryWithoutStrongest(const qualpf::gui::SearchRequest &, QVector<CardType> &, ProgressCallback = {});
    void cancelSearch() { execution_.cancel(); }
    bool isCanceled() const { return execution_.cancelled(); }
    bool isSearchRunning() const { return execution_.running(); }
    QString searchError() const { return execution_.error() ? QString::fromStdString(execution_.error()->message) : QString{}; }

  private:
    QVector<CardType> run(const qualpf::gui::SearchRequest &, application::CandidateQuery::Route, ProgressCallback,
        application::SynchronousSearchPurpose = application::SynchronousSearchPurpose::browse,
        std::size_t maximum = std::numeric_limits<std::size_t>::max());
    application::SearchCoordinator &execution_;
    std::function<application::ReferenceDatabase()> database_;
    application::CandidateReader &reader_;
    QString error_;
};
} // namespace qualpf::gui
