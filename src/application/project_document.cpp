#include "qualpf/application/workspace_ports.h"
#include "qualpf/application/search_coordinator.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace qualpf::application {
namespace {
Status unavailable() {
    return Status{Error{ErrorCode::closed_edit, "project",
                        "Finish the active operation before opening or saving a project",
                        "workspace_unavailable"}};
}
bool oneOf(const std::string &s, std::initializer_list<std::string> values) {
    return std::find(values.begin(), values.end(), s) != values.end();
}
bool penValid(const ProjectPen &p) {
    if (!std::isfinite(p.width) || p.width < 0 ||
        !oneOf(p.style,
               {std::string("none"), "solid", "dash", "dot", "dash_dot", "dash_dot_dot", "custom"}) ||
        !oneOf(p.cap, {std::string("flat"), "square", "round"}) ||
        !oneOf(p.join, {std::string("miter"), "bevel", "round", "svg_miter"}))
        return false;
    return std::all_of(p.dash_pattern.begin(), p.dash_pattern.end(),
                       [](double n) { return std::isfinite(n) && n > 0; }) &&
           (p.style != "custom" || (!p.dash_pattern.empty() && p.dash_pattern.size() % 2 == 0));
}
} // namespace
Status validateProjectSaveSource(ProductHandle source) {
    if (!source)
        return Status{Error{ErrorCode::missing_data, "project.source", "Missing project source"}};
    if (source->editing)
        return Status{Error{ErrorCode::closed_edit, "project", "Finish the active edit first"}};
    if (!source->algorithms)
        return Status{Error{ErrorCode::missing_data, "project.algorithms", "Missing project algorithms"}};
    return validateProductState(*source);
}

Status validateProjectView(const ProjectViewState &v) {
    const auto bad = [] {
        return Status{Error{ErrorCode::invalid_input, "project.view", "Invalid project presentation",
                            "invalid_project_view"}};
    };
    if (!oneOf(v.axis, {std::string("two_theta"), "d_spacing", "inverse_d", "inverse_d_squared"}) ||
        !oneOf(v.y_scale, {std::string("linear"), "logarithmic"}) || !std::isfinite(v.offset))
        return bad();
    for (const auto &style : {v.background_style, v.layer_style})
        if (!oneOf(style, {std::string("white"), "black", "flat", "diagonal", "reverse_diagonal",
                           "horizontal", "vertical", "radial"}))
            return bad();
    if (v.bounds) {
        for (double n : *v.bounds)
            if (!std::isfinite(n))
                return bad();
        if ((*v.bounds)[0] >= (*v.bounds)[1] || (*v.bounds)[2] >= (*v.bounds)[3] ||
            (v.y_scale == "logarithmic" && (*v.bounds)[2] <= 0))
            return bad();
    }
    for (const auto &[key, p] : v.pens)
        if (key.empty() || !penValid(p))
            return bad();
    for (const auto &[key, c] : v.curves)
        if (key.empty() || (!penValid(c.pen) || !penValid(c.scatter_pen)) || !std::isfinite(c.scatter_size) ||
            c.scatter_size < 0 ||
            !oneOf(c.connection,
                   {std::string("none"), "line", "step_left", "step_right", "step_center", "impulse"}) ||
            !oneOf(c.scatter, {std::string("none"), "dot", "cross", "plus", "circle", "disc", "square",
                               "diamond", "star", "triangle", "triangle_inverted", "cross_square",
                               "plus_square", "cross_circle", "plus_circle", "peace"}))
            return bad();
    for (const auto &[key, visible] : v.layers)
        if (key.empty() || !oneOf(visible, {std::string("disabled"), "hidden", "visible"}))
            return bad();
    for (const auto *ids : {&v.result_order, &v.selected_cards}) {
        std::set<std::string> unique;
        for (const auto &id : *ids)
            if (id.empty() || !unique.insert(id).second)
                return bad();
    }
    return Status{std::monostate{}};
}
Status Workspace::updateProjectView(ProjectViewState next) {
    if (std::this_thread::get_id() != owner_ || coordinator_.closing())
        return unavailable();
    auto valid = validateProjectView(next);
    if (!valid)
        return valid;
    if (next == *projectView_)
        return Status{std::monostate{}};
    if (viewRevision_ == UINT64_MAX)
        return Status{
            Error{ErrorCode::exhausted_revision, "project.view", "Presentation revision exhausted"}};
    auto prepared = std::make_shared<const ProjectViewState>(std::move(next));
    projectView_ = std::move(prepared);
    ++viewRevision_;
    return Status{std::monostate{}};
}
Result<ProjectSaveSnapshot> Workspace::projectSnapshot() const {
    if (std::this_thread::get_id() != owner_ || coordinator_.running() || coordinator_.closing() ||
        session_.snapshot()->editing)
        return Result<ProjectSaveSnapshot>{unavailable().error()};
    auto valid = validateProjectSaveSource(session_.snapshot());
    if (!valid)
        return Result<ProjectSaveSnapshot>{valid.error()};
    return Result<ProjectSaveSnapshot>{ProjectSaveSnapshot{
        session_.snapshot(), projectView_, projectIdentity_, projectExtensions_, viewRevision_}};
}
Status Workspace::restoreProject(const ProjectSaveSnapshot &source, ProjectDocument doc) {
    if (std::this_thread::get_id() != owner_ || coordinator_.running() || coordinator_.closing() ||
        session_.snapshot()->editing)
        return unavailable();
    if (source.state != session_.snapshot() || source.view_revision != viewRevision_)
        return Status{Error{ErrorCode::stale_snapshot, "project.source", "Project changed while opening",
                            "stale_project"}};
    auto valid = validateProjectView(doc.view);
    if (!valid)
        return valid;
    if (viewRevision_ == UINT64_MAX)
        return Status{
            Error{ErrorCode::exhausted_revision, "project.view", "Presentation revision exhausted"}};
    auto view = std::make_shared<const ProjectViewState>(std::move(doc.view));
    auto status = session_.restoreProjectWithPresentation(source.state, std::move(doc.state), [&]() noexcept {
        projectView_.swap(view);
        std::swap(projectIdentity_, doc.identity);
        projectExtensions_.swap(doc.extensions);
        ++viewRevision_;
    });
    if (status) {
        savedProduct_ = session_.snapshot();
        projectSource_ = savedProduct_;
        savedViewRevision_ = viewRevision_;
    }
    return status;
}
Status Workspace::markProjectSaved(const ProjectSaveSnapshot &source, ProjectIdentity identity) {
    if (std::this_thread::get_id() != owner_ || source.state != session_.snapshot() ||
        source.view_revision != viewRevision_)
        return Status{Error{ErrorCode::stale_snapshot, "project.source", "Project changed while saving",
                            "stale_project"}};
    std::swap(projectIdentity_, identity);
    savedProduct_ = source.state;
    projectSource_ = source.state;
    savedViewRevision_ = viewRevision_;
    return Status{std::monostate{}};
}
bool Workspace::projectModified() const {
    return savedProduct_ != session_.snapshot() || savedViewRevision_ != viewRevision_;
}
void Workspace::resetProjectPresentation() {
    if (std::this_thread::get_id() != owner_)
        return;
    projectIdentity_ = {};
    projectExtensions_.reset();
    savedProduct_.reset();
    projectSource_.reset();
    // New/import retains the user's current graph style but drops data-bound selection.
    auto view = *projectView_;
    view.bounds.reset();
    view.selected_cards.clear();
    view.result_order.clear();
    auto status = updateProjectView(std::move(view));
    (void)status;
}
Status openProjectInput(Workspace &workspace, const ProjectSourceReader &reader, const std::string &path,
                        const ProjectReadOptions &options) try {
    auto source = workspace.projectSnapshot();
    if (!source)
        return Status{source.error()};
    auto loaded = reader.read(path, source.value().state->backend, options);
    if (!loaded)
        return Status{loaded.error()};
    return workspace.restoreProject(source.value(), std::move(loaded).value());
} catch (const std::bad_alloc &) {
    return Status{Error{ErrorCode::invalid_input, "project.size", "Project memory budget exhausted",
                        "project_resource_limit"}};
} catch (const std::exception &e) {
    return Status{Error{ErrorCode::invalid_input, "project", e.what(), "project_read_failed"}};
}
Status saveProjectInput(Workspace &workspace, const ProjectTargetWriter &writer,
                        const std::string &path) try {
    auto source = workspace.projectSnapshot();
    if (!source)
        return Status{source.error()};
    auto saved = writer.writeAtomic(path, source.value());
    if (!saved)
        return Status{saved.error()};
    return workspace.markProjectSaved(source.value(), std::move(saved).value());
} catch (const std::bad_alloc &) {
    return Status{Error{ErrorCode::invalid_input, "project.size", "Project memory budget exhausted",
                        "project_resource_limit"}};
} catch (const std::exception &e) {
    return Status{Error{ErrorCode::invalid_input, "project", e.what(), "project_save_failed"}};
}
} // namespace qualpf::application
