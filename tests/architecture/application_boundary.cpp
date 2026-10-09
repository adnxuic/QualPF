#include "qualpf/application/database_build.h"
#include "qualpf/application/domain_inputs.h"
#include "qualpf/application/environment.h"
#include "qualpf/application/kalpha2.h"
#include "qualpf/application/platform_services.h"
#include "qualpf/application/plot_coordinates.h"
#include "qualpf/application/preferences.h"
#include "qualpf/application/product_files.h"
#include "qualpf/application/reference_database.h"
#include "qualpf/application/update_service.h"
#include "qualpf/application/database/cif_batch.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "qualpf/application/import/powder_cif_import_service.h"
#include "qualpf/application/presentation/plot_snapshot.h"
#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/startup/application_initialization_service.h"
#include "qualpf/application/powder_cif_source.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/workspace_ports.h"
#include <iostream>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
struct Reader : PatternSourceReader {
    Result<Pattern> read(const std::string &) const override {
        return Result<Pattern>{Error{ErrorCode::missing_data, "reader", "controlled failure"}};
    }
};
struct CifReader : CifSourceReader {
    int reads = 0;
    std::variant<std::string, CifImportFailure> read(const std::string &path, std::size_t) override {
        ++reads;
        return CifImportFailure{
            "read", "controlled_read_failure", path, {}, "file", "Controlled reader failure", {}, {}, {}, {}};
    }
};
struct CifWriter : CifRecordWriter {
    int creates = 0, writes = 0, closes = 0;
    std::optional<database::WriteError> create(const std::string &) override {
        ++creates;
        return {};
    }
    std::optional<database::WriteError> write(const database::CifRecord &) override {
        ++writes;
        return {};
    }
    void close() override { ++closes; }
};
static void cifPorts() {
    CifReader reader;
    CifWriter writer;
    const std::vector<CifBatchRequest> sources{{"owned-source", 7, {}}};
    auto report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp);
    if (!report.completed || report.failures.size() != 1 || report.committed || writer.writes ||
        reader.reads != 1 || writer.closes != 1)
        throw std::runtime_error("CIF failed read published a card or violated batch policy");
    std::stop_source stop;
    CifBatchOptions options;
    options.stop = stop.get_token();
    report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, options,
                         [&](const auto &, const auto &) { stop.request_stop(); });
    if (!report.cancelled || report.completed || reader.reads != 1 || writer.writes || writer.closes != 2)
        throw std::runtime_error("CIF cancellation boundary read or committed data");
    report = runCifBatch(
        writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, {},
        [](const auto &, const auto &) { throw std::runtime_error("controlled callback failure"); });
    if (!report.fatal || report.fatal->code != "cif_callback_or_operation_failure" || report.completed ||
        writer.closes != 3)
        throw std::runtime_error("CIF callback exception escaped cleanup");
    stop.request_stop();
    options.stop = stop.get_token();
    const int before = writer.creates;
    report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, options);
    if (!report.cancelled || writer.creates != before)
        throw std::runtime_error("pre-cancel CIF created a database");
}
int main() {
    cifPorts();
    const std::vector<double> intensity{25, 100};
    const std::vector<QuantificationInput> phases{{true, intensity, 1.5, 2.0}};
    const auto percentages = quantify(phases);
    if (percentages.size() != 1 || percentages[0] != 100)
        throw std::runtime_error("pure quantification interface");
    Workspace workspace;
    auto source = workspace.session().snapshot();
    if (openPatternInput(workspace.session(), Reader{}, "fixture") ||
        workspace.session().snapshot() != source)
        throw std::runtime_error("failed IO published state");
    SearchSettingsSnapshot settings;
    settings.parameters.maximum_entries = 0;
    auto input = matchingInput(source, settings);
    if (!input || input.value().parameters.maximum_entries != 0 || input.value().source != source)
        throw std::runtime_error("owned input contract");
    auto standalone = ApplicationInitializationService{}.createWorkspace();
    auto task = standalone->searches().begin();
    if (!task || task.value().source != standalone->session().snapshot())
        throw std::runtime_error("composition contract");
    std::cout << "PASS: complete Application consumed by a Qt-free C++20 client\n";
}
