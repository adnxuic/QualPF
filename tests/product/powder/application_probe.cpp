#include "infrastructure/serialization/metadata_values.h"
#include "infrastructure/io/powder_source_reader.h"
#include "gui/import/powder_cif_import_service.h"
#include "observation.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    using namespace qualpf::application; using namespace qualpf::gui;
    using namespace qualpf::io::powder_cif;
    try {
        phase25b::check(argc>=3,"Input and exclusive output");
        qualpf::application::Workspace sessionOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView session(sessionOwner.session());
        phase25b::seed(session);
        const auto before=session.snapshot();
        qualpf::infrastructure::PowderSourceReader reader;
        PowderCifImportService service(session,reader);
        auto draft=service.prepare(QString::fromLocal8Bit(argv[1]));
        QJsonObject observation;
        auto recordError=[&](const ImportError &error) {
            QJsonArray locations;
            for (const auto &field : error.locations)
                locations.append(QJsonObject{{"tag",QString::fromStdString(field.tag)},
                    {"offset",double(field.location.offset)},{"line",double(field.location.line)},
                    {"column",double(field.location.column)},{"begin",double(field.span.begin)},{"end",double(field.span.end)}});
            observation["error"]=QJsonObject{{"reason",QString::fromStdString(error.error.reason)},
                {"field",QString::fromStdString(error.error.field)},{"code",int(error.error.code)},
                {"hash",error.source ? QString::fromStdString(error.source->sha256) : QString{}},
                {"block",QString::fromStdString(error.block_id)},{"locations",locations},
                {"axis_count",error.axis_count ? double(*error.axis_count) : -1},
                {"intensity_count",error.intensity_count ? double(*error.intensity_count) : -1}};
        };
        if (auto error=std::get_if<ImportError>(&draft)) recordError(*error);
        else {
            const auto &prepared=std::get<PowderCifImportDraft>(draft);
            observation["wave_state"]=int(prepared.preparation.selected().wavelength_state);
            std::optional<UserSuppliedRadiation> supplied;
            if (argc>3) supplied=UserSuppliedRadiation{std::uint32_t(std::stoul(argv[3],nullptr,16)),UserSuppliedRadiation::Kind::x_ray};
            auto complete=service.complete(prepared,supplied);
            if (auto error=std::get_if<ImportError>(&complete)) recordError(*error);
            else {
                int preflight=0,effects=0;
                auto committed=service.commit(std::move(std::get<PreparedPowderCifImport>(complete)),
                    [&](const SessionStateView &){ ++preflight; return qualpf::Status{std::monostate{}}; },
                    [&](const SessionViewHandle &){ ++effects; });
                phase25b::check(bool(committed),"Production Application commit");
                observation["preflight"]=preflight; observation["effects"]=effects;
            }
        }
        const auto after=session.snapshot();
        observation["changed"]=after!=before;
        observation["revision_delta"]=double(after->revision-before->revision);
        observation["peaks"]=after->peaks.tth.size(); observation["candidates"]=after->candidates.size();
        observation["phases"]=after->phases.size(); observation["smoothing_applied"]=after->preprocessing.smoothing_applied.value_or(true);
        if (after!=before) { observation["pattern"]=phase25b::pattern(after->pattern); observation["metadata"]=qualpf::infrastructure::jsonValue(after->projectMetadata); }
        phase25b::save(QString::fromLocal8Bit(argv[2]),observation);
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
