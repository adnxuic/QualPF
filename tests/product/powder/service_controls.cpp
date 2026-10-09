#include "infrastructure/serialization/metadata_values.h"
#include "infrastructure/io/powder_source_reader.h"
#include "gui/import/powder_cif_import_service.h"
#include "infrastructure/serialization/powder_cif_provenance.h"
#include "observation.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    using namespace qualpf::application; using namespace qualpf::gui;
    using namespace qualpf::io::powder_cif;
    try {
        const std::string raw="data_first\n_diffrn_radiation_wavelength 1.2\nloop_\n_pd_meas_2theta_scan\n_pd_meas_counts_total\n10 2\n11 4\n12 8\n";
        qualpf::application::Workspace sessionOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView session(sessionOwner.session()); phase25b::seed(session);
        qualpf::infrastructure::PowderSourceReader reader;
        PowderCifImportService service(session,reader);
        const auto before=session.snapshot();
        auto draft=service.prepareBytes(raw,"fixture.cif");
        phase25b::check(std::holds_alternative<PowderCifImportDraft>(draft),"Owning preparation");
        const auto &privateDraft=std::get<PowderCifImportDraft>(draft);
        phase25b::check(privateDraft.source==before && session.snapshot()==before,"Preparation publishes nothing");
        auto completed=service.complete(privateDraft);
        phase25b::check(std::holds_alternative<PreparedPowderCifImport>(completed),"Source wavelength complete");
        auto pending=std::get<PreparedPowderCifImport>(completed);
        phase25b::check(session.snapshot()==before && pending.next.powderCifProvenance,"Private canonical/provenance");
        const auto saved=qualpf::infrastructure::jsonValue(pending.next.projectMetadata["powder_cif_provenance"]);
        auto decoded=decodePowderCifProvenance(saved,pending.next.pattern.source_id);
        phase25b::check(bool(decoded) && decoded.value() && decoded.value()->source->bytes==raw,"Owning source-byte roundtrip");
        phase25b::check(encodePowderCifProvenance(*decoded.value())==saved,"Exact provenance roundtrip");
        int preflight=0,effects=0;
        auto rejected=service.commit(pending,[&](const SessionStateView &) {
            ++preflight; return qualpf::Status{qualpf::Error{qualpf::ErrorCode::invalid_input,"presentation","Injected preflight rejection"}};
        },[&](const SessionViewHandle &) { ++effects; });
        phase25b::check(!rejected && preflight==1 && effects==0 && session.snapshot()==before,"Preflight failure atomicity");
        auto success=service.commit(pending,{},[&](const SessionViewHandle &) { ++effects; });
        phase25b::check(bool(success) && effects==1 && session.snapshot()->revision==before->revision+1,"One commit/one effect");
        const auto committed=session.snapshot();
        phase25b::check(committed->peaks.tth.empty() && committed->candidates.empty() && committed->phases.empty() &&
            committed->preprocessing.smoothing_applied==false,"Existing complete reset contract");
        phase25b::check(!service.commit(pending,{},[&](const SessionViewHandle &) { ++effects; }) && effects==1 &&
            session.snapshot()==committed,"Repeated stale transaction rejection");
        int corrupt=0;
        for (const auto &key : {"version","source_sha256","source_bytes_base64","source_id","radiation_origin","selected_block_index"}) {
            auto damaged=saved.toObject();
            damaged[key]=QStringLiteral("corrupt");
            phase25b::check(!decodePowderCifProvenance(damaged,pending.next.pattern.source_id),"Corrupt companion rejected");
            ++corrupt;
        }
        auto absent=decodePowderCifProvenance(QJsonValue{},"ordinary.xy");
        phase25b::check(bool(absent) && !absent.value(),"Old projects retain unknown provenance");
        auto missing=service.prepareBytes("data_d\nloop_\n_pd_proc_d_spacing\n_pd_meas_counts_total\n1 1\n2 2\n","missing.cif");
        phase25b::check(std::holds_alternative<PowderCifImportDraft>(missing),"Requires-wave preparation");
        const auto &waveDraft=std::get<PowderCifImportDraft>(missing);
        phase25b::check(waveDraft.preparation.selected().wavelength_state==WavelengthState::WAVELENGTH_REQUIRED,"Typed requires-wave");
        phase25b::check(std::holds_alternative<ImportError>(service.complete(waveDraft)),"No hidden wavelength default");
        auto resolved=service.complete(waveDraft,UserSuppliedRadiation{0x3f800000,UserSuppliedRadiation::Kind::neutron});
        phase25b::check(std::holds_alternative<PreparedPowderCifImport>(resolved),"Explicit accepted resolution");
        const auto &value=std::get<PreparedPowderCifImport>(resolved);
        phase25b::check(value.next.powderCifProvenance->radiation.origin==RadiationOrigin::user_supplied &&
            value.next.pattern.radiation.kind==qualpf::RadiationKind::neutron &&
            value.next.pattern.radiation.lines.size()==1 && value.next.pattern.radiation.lines.front().relative_weight==1,
            "First-only explicit user provenance/radtype/ratio");
        phase25b::check(session.snapshot()==committed,"Resolution still private");
        std::cout<<"PASS Application transaction, six corrupt-provenance controls, owning roundtrip, explicit resolution "<<corrupt<<'\n';
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
