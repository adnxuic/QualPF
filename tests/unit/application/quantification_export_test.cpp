#include "qualpf/application/export/quantification_export.h"
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::application::exporting;
static void check(bool ok, std::source_location at=std::source_location::current()) {
    if (!ok) throw std::runtime_error("Quantification CSV line "+std::to_string(at.line()));
}
// Independent RFC4180 reader, including embedded newlines and doubled quotes.
static std::vector<std::vector<std::string>> parse(const std::string &text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row; std::string cell; bool quotes=false;
    for (std::size_t i=0;i<text.size();++i) {
        const char c=text[i];
        if (c=='"') {
            if (quotes && i+1<text.size() && text[i+1]=='"') { cell+='"'; ++i; }
            else quotes=!quotes;
        } else if (!quotes && c==',') { row.push_back(cell); cell.clear(); }
        else if (!quotes && c=='\r') {
            check(i+1<text.size() && text[++i]=='\n'); row.push_back(cell);
            rows.push_back(row); row.clear(); cell.clear();
        } else cell+=c;
    }
    check(!quotes && cell.empty() && row.empty()); return rows;
}
struct Target final : AtomicExportTarget {
    bool fail_open=false, short_write=false, fail_commit=false;
    int opens=0,cancels=0,commits=0; std::string previous="Original target bytes",draft;
    bool open() override { ++opens; return !fail_open; }
    std::int64_t write(std::string_view bytes) override { draft=bytes; return bytes.size()-(short_write?1:0); }
    bool commit() override { ++commits; if (fail_commit) return false; previous=draft; return true; }
    void cancel() noexcept override { ++cancels; draft.clear(); }
    std::string error_message() const override { return "Injected atomic write failure"; }
};
int main() try {
    ProductState state; state.revision=19; ProductCard a,b; a.id="A"; b.id="B"; state.phases={a,b};
    QuantificationResult q; q.source=std::make_shared<const ProductState>(state); q.revision=19;
    q.generation=7; q.database_version=3; q.method=QuantificationMethod::pfsm_rir;
    q.availability=QuantificationAvailability::available; q.rwp_net=.012345678901234567;
    QuantificationPhase p; p.id="A"; p.name="Phase, \"A\"\r\nSecond line"; p.scale=1./7;
    p.weight_percent=25; p.rir=2; p.reference_maximum=100; p.shift_degrees=-.0123456789012345;
    p.rir_source="文献, \"calibration\"\nCu Kalpha"; p.rir_revised=true; p.original_rir="2.000";
    q.phases.push_back(p); p.id="B"; p.name="Phase B"; p.weight_percent=75; q.phases.push_back(p);
    q.qualifications={"Selected crystalline phases only","No amorphous estimate"};
    auto csv=prepareQuantificationCsv(q); check(bool(csv)); const auto rows=parse(csv.value());
    check(rows.size()==3 && rows[0].size()==25 && rows[1].size()==25 && rows[2].size()==25);
    check(rows[0][3]=="weight_percent" && rows[1][2]==q.phases[0].name && rows[1][7]==q.phases[0].rir_source);
    check(rows[1][3]=="25" && rows[2][3]=="75" && rows[1][8]=="Cu Kalpha");
    check(std::stod(rows[1][4])==q.phases[0].scale && std::stod(rows[1][5])==*q.phases[0].shift_degrees);
    check(std::stod(rows[1][13])==*q.rwp_net && rows[1][14]=="19" && rows[1][15]=="7");
    check(rows[1][24]=="Selected crystalline phases only\nNo amorphous estimate");
    Target good; check(bool(write_quantification_csv_to(good,q)) && good.previous==csv.value() && good.cancels==0);
    for (int failure=0;failure<3;++failure) {
        Target bad; bad.fail_open=failure==0; bad.short_write=failure==1; bad.fail_commit=failure==2;
        check(!write_quantification_csv_to(bad,q) && bad.previous=="Original target bytes");
        check(bad.cancels==(failure==0?0:1));
    }
    q.availability=QuantificationAvailability::unavailable; q.issues={{"B","missing_rir","Missing RIR, entire composition unavailable"}};
    auto unavailable=prepareQuantificationCsv(q); check(bool(unavailable)); const auto invalidRows=parse(unavailable.value());
    check(invalidRows[1][3].empty() && invalidRows[2][3].empty() && invalidRows[1][11]=="unavailable");
    check(invalidRows[1][12].find("B: missing_rir")!=std::string::npos);
    q.revision=20; Target stale; check(!write_quantification_csv_to(stale,q) && stale.opens==0);
    q.revision=19; q.phases[0].id="wrong"; check(!prepareQuantificationCsv(q));
    check(!prepareQuantificationCsv({}));
    std::cout<<"PASS: UTF-8 RFC4180 precision, source binding, unavailable rows and atomic failures\n";
    return 0;
} catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
