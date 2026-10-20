#pragma once
#include "gui/mainwindow.h"
#include "gui/state/workspace_context.h"
#include "infrastructure/serialization/qpf_project.h"
#include "tests/support/legacy_qxp/project_codec.h"
#include <QTemporaryDir>
#include <stdexcept>

namespace qualpf::test {
// Only the test driver decodes the independently frozen QXP numerical input.
// Production opens an equivalent complete QPF, with the historical setup kept
// in test-only support. No scientific values or expected results are generated.
inline void loadFrozenQpfReference(MainWindow &window, const char *reference) {
    using namespace application;
    auto &owner = window.context().workspace().session();
    ProductSearchSession frozen(SearchBackend::cpp, owner.snapshot()->algorithms,
                                owner.defaultSettingsStore());
    const auto prepared = infrastructure::legacy_fixture::openProjectInput(
        frozen, infrastructure::legacy_fixture::ProjectFileReader{}, reference, {});
    if (!prepared)
        throw std::runtime_error("Frozen reference: " + prepared.error().message);
    QTemporaryDir files;
    const auto path = files.path() + "/current-preprocessing-pool.qpf";
    ProjectSaveSnapshot snapshot{frozen.snapshot(), std::make_shared<const ProjectViewState>(), {}, {}, 0};
    const auto written = infrastructure::QpfProjectWriter{}.writeAtomic(path.toStdString(), snapshot);
    if (!written)
        throw std::runtime_error("QPF reference: " + written.error().message);
    if (!window.loadProject(path))
        throw std::runtime_error("QPF reference load rejected");
}
} // namespace qualpf::test
