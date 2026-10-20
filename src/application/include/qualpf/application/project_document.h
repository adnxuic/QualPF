#pragma once
#include "qualpf/application/product_session.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace qualpf::application {
// Portable presentation values. Coordinates are in the selected display axis;
// visual edits have their own revision and never change a scientific source.
struct ProjectPen {
    std::uint32_t rgba = 0x000000ff;
    double width = 1;
    std::string style = "solid";
    std::vector<double> dash_pattern;
    std::string cap = "square", join = "bevel";
    bool cosmetic = false;
    friend bool operator==(const ProjectPen &, const ProjectPen &) = default;
};
struct ProjectCurveStyle {
    ProjectPen pen;
    ProjectPen scatter_pen;
    std::uint32_t scatter_rgba = 0x000000ff;
    bool scatter_fill = false, scatter_pen_defined = false;
    std::string scatter = "none", connection = "line";
    double scatter_size = 6;
    bool visible = true;
    std::string label;
    friend bool operator==(const ProjectCurveStyle &, const ProjectCurveStyle &) = default;
};
struct ProjectViewState {
    std::string axis = "two_theta", y_scale = "linear";
    std::optional<std::array<double, 4>> bounds;
    bool subtract_background = false, offset_enabled = false, legend = true;
    bool x_tick_labels = true, y_tick_labels = true;
    double offset = 1000;
    std::string background_style = "white", layer_style = "white";
    std::array<std::uint32_t, 3> background_colors{0xffffffff, 0xffffffff, 0x000000ff};
    std::array<std::uint32_t, 3> layer_colors{0xffffffff, 0xffffffff, 0x000000ff};
    std::map<std::string, ProjectPen, std::less<>> pens;
    std::map<std::string, ProjectCurveStyle, std::less<>> curves;
    std::map<std::string, std::string, std::less<>> layers;
    std::map<std::string, std::uint32_t, std::less<>> card_colors;
    std::vector<std::string> selected_cards, result_order;
    friend bool operator==(const ProjectViewState &, const ProjectViewState &) = default;
};
struct ProjectIdentity {
    std::string uuid, created_utc;
    friend bool operator==(const ProjectIdentity &, const ProjectIdentity &) = default;
};
// Opaque file-boundary values retain unknown optional protocol extensions. They
// are owned bytes, not a live scientific mirror or a connection to an open file.
struct ProjectOpaqueObject {
    std::string kind;
    std::uint64_t version = 1;
    std::vector<std::uint8_t> payload;
};
struct ProjectExtensions {
    std::vector<std::uint8_t> manifest_fields;
    std::map<std::string, ProjectOpaqueObject, std::less<>> objects;
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> blocks;
    std::vector<std::string> decoded_objects;
};
struct ProjectDocument {
    ProductState state;
    ProjectViewState view;
    ProjectIdentity identity;
    std::shared_ptr<const ProjectExtensions> extensions;
};
struct ProjectReadOptions {
    std::uint64_t maximum_decoded_bytes = 2ull * 1024 * 1024 * 1024;
    unsigned maximum_depth = 64;
};
struct ProjectSaveSnapshot {
    ProductHandle state;
    std::shared_ptr<const ProjectViewState> view;
    ProjectIdentity identity;
    std::shared_ptr<const ProjectExtensions> extensions;
    std::uint64_t view_revision = 0;
};
class ProjectTargetWriter {
  public:
    virtual ~ProjectTargetWriter() = default;
    virtual Result<ProjectIdentity> writeAtomic(const std::string &, const ProjectSaveSnapshot &) const = 0;
};
Status validateProjectSaveSource(ProductHandle);
Status validateProjectView(const ProjectViewState &);
} // namespace qualpf::application
