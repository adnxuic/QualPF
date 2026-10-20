#include "gui/mainwindow.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/reportwidget.h"
#include "gui/widgets/piechartwidget.h"
#include "gui/widgets/dbresultswidget.h"
#include "tests/support/workspace_fixture.h"
#include "qualpf/domain/radiation.h"
#include "qualpf/application/result_management.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPrinter>
#include "qualpf/application/export/quantification_export.h"
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at=std::source_location::current()) {
    if (!ok) throw std::runtime_error("Quantification GUI line "+std::to_string(at.line()));
}
template<class T> static T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().field+": "+r.error().message);
    return std::move(r).value();
}
template<class T> static T *child(QObject &o, const char *name="") {
    auto *p=*name?o.findChild<T*>(QString::fromLatin1(name)):o.findChild<T*>();
    check(p!=nullptr); return p;
}
static double il(double x, double center) {
    return std::pow(1+4*(std::pow(2.,2./3)-1)*std::pow((x-center)/.2,2),-1.5);
}
struct Reader final : CandidateReader {
    int calls = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        ++calls;
        std::vector<ProductCard> cards;
        for (int i=0;i<2;++i) {
            ProductCard c; c.id = i ? "B" : "A"; c.chemicalName = i ? "Phase B" : "Phase A";
            c.rir = i ? "4" : "2"; c.referenceRir = i ? 4 : 2;
            for (double angle : i ? std::vector<double>{33,34} : std::vector<double>{30,31})
                c.d.push_back(radiation::CU_KA1_WAVE/(2*std::sin(angle*std::numbers::pi/360)));
            c.intensity = i ? std::vector<double>{1000,250} : std::vector<double>{100,50};
            cards.push_back(std::move(c));
        }
        ProductCard decoy; decoy.id="C"; decoy.chemicalName="Unselected candidate";
        decoy.rir="3"; decoy.referenceRir=3; decoy.intensity={100};
        decoy.d={radiation::CU_KA1_WAVE/(2*std::sin(35.5*std::numbers::pi/360))};
        cards.push_back(std::move(decoy));
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
static ProductState initial() {
    ProductState s; s.algorithms = take(resolveAlgorithms(AlgorithmSpec{{},SearchMethod::pfsm}));
    auto &p = s.pattern; p.id={44}; p.source_id="Independent 25/75 mixture";
    p.radiation.kind=RadiationKind::x_ray;
    p.radiation.lines={{WavelengthAngstrom{radiation::CU_KA1_WAVE},1}};
    p.processing={false,false,false};
    for (int i=0;i<=800;++i) {
        double x=28+i*.01; p.two_theta.emplace_back(x);
        p.observed.push_back(7+5*(100*il(x,30)+50*il(x,31))+3*(1000*il(x,33)+250*il(x,34)));
    }
    p.calculation_range=AngularRange{p.two_theta.front(),p.two_theta.back()};
    return s;
}
static void execute(Workspace &w, Reader &r, SearchInput in) {
    auto task=take(w.searches().begin({"fixture",3}));
    in.source=task.source; in.method=SearchMethod::pfsm; in.taskId=task.id; in.databaseVersion=3;
    auto result=SearchEngine{}.run(in,r,task.context);
    take(w.searches().complete(task.id,std::move(result)));
}
int main(int argc, char **argv) try {
    QApplication app(argc,argv); QTemporaryDir temporary; check(temporary.isValid());
    app.setOrganizationName("QualPF-tests"); app.setApplicationName("quantification-gui");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    auto &w=AppState::workspace(); auto &session=w.session();
    check(bool(session.commit(session.snapshot(),initial())));
    Reader reader;
    auto request=std::make_shared<PfsmRequest>(); request->threads=1;
    request->lower_shift=0; request->upper_shift=0; request->observations.emplace();
    auto &o=*request->observations; o.signal_source="Independent analytic mixture";
    o.intensity_units="analytic units"; o.unclipped_signal=true; o.equal_weights=true;
    o.noise_source="Independently prescribed analytic observations";
    o.background=std::vector<double>(801,7);
    FwhmUsage width; width.width=AngularWidthDegrees{.2}; width.origin=FwhmOrigin::fixed;
    width.current=true; width.algorithm={"qualpf.fwhm.fixed",1}; request->supplied_width=width;
    SearchInput in; in.pfsm=request; execute(w,reader,in);
    SearchParameters acceptanceParameters;
    for (const char *id : {"A","B"}) {
        auto intent=take(phaseAcceptanceInput(session.snapshot(),id,acceptanceParameters,false));
        check(bool(intent)); execute(w,reader,*intent);
    }
    MainWindow window(AppState::context()); window.resize(1440,900); window.show();
    auto *quant=child<QuantWidget>(window); quant->refresh();
    auto *dock=child<QDockWidget>(window,"dockWidgetQuant"); dock->show(); dock->raise();
    auto *table=child<QTableView>(*quant,"tableView");
    auto *results=child<DbResultsWidget>(window); results->refresh();
    auto *candidateTable=child<QTableView>(*results,"table");
    check(candidateTable->model()->rowCount()==1);
    {
        QSignalBlocker block(candidateTable->selectionModel());
        candidateTable->setCurrentIndex(candidateTable->model()->index(0,2));
    }
    auto *report=child<ReportWidget>(window); auto *browser=child<QTextBrowser>(*report);
    check(dock->windowTitle().contains("PFSM/RIR"));
    check(quant->quantificationResult().available() && table->model()->columnCount()==7);
    check(std::abs(table->model()->index(0,3).data().toDouble()-25)<1e-9);
    check(table->model()->index(0,4).data().toDouble()==quant->quantificationResult().phases[0].scale);
    check(!child<PieChartWidget>(*quant)->isHidden());
    check(browser->toPlainText().contains("25.00") && browser->toPlainText().contains("Joint Rwp"));
    const auto capture=[&](const char *name) {
        if (argc==2) {
            QApplication::processEvents();
            const QString directory=QString::fromLocal8Bit(argv[1]); check(QDir().mkpath(directory));
            check(quant->grab().save(directory+"/"+QString::fromLatin1(name)));
        }
    };
    capture("valid.png");
    auto *edit=child<QPushButton>(*quant,"editRir"); auto *restore=child<QPushButton>(*quant,"restoreRir");
    table->setCurrentIndex(table->model()->index(1,1));
    const auto before=session.snapshot(); bool rejected=false;
    QTimer::singleShot(0,[&] {
        auto *dialog=child<QDialog>(window,"rirDialog");
        auto *buttons=child<QDialogButtonBox>(*dialog);
        child<QLineEdit>(*dialog,"rirValue")->setText("0"); buttons->button(QDialogButtonBox::Ok)->click();
        rejected=session.snapshot()==before && !child<QLabel>(*dialog,"rirError")->text().isEmpty();
        child<QLineEdit>(*dialog,"rirValue")->setText("6");
        child<QLineEdit>(*dialog,"rirSource")->setText("Independent source <verified>, Cu calibration");
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    edit->click(); check(rejected);
    const auto changed=session.snapshot(); const auto &q=quant->quantificationResult();
    check(changed!=before && q.available() && q.source==changed && q.generation==before->pfsm->generation);
    check(candidateTable->currentIndex().isValid() && candidateTable->currentIndex().data().toString()=="C");
    check(!w.searches().running() && changed->candidates==before->candidates);
    check(std::abs(*q.phases[0].weight_percent-100./3)<1e-9 && reader.calls==1);
    check(browser->toPlainText().contains("33.33") && browser->toPlainText().contains("Independent source <verified>"));
    const auto csvPath=temporary.path()+"/量化.csv";
    check(bool(qualpf::application::exporting::writeQuantificationCsv(AppState::context().files().exports,csvPath.toStdString(),q)));
    QFile csv(csvPath); check(csv.open(QIODevice::ReadOnly)); const auto bytes=csv.readAll();
    check(bytes==QByteArray::fromStdString(qualpf::application::exporting::prepareQuantificationCsv(q).value()));
    QPrinter printer; printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(temporary.path()+"/report.pdf"); report->print(&printer);
    check(QFileInfo(printer.outputFileName()).size()>1000);
    capture("revised.png");
    table->setCurrentIndex(table->model()->index(1,1));
    check(restore->isEnabled());
    QTimer::singleShot(0,[&] { child<QDialog>(window,"rirDialog")->reject(); }); edit->click();
    check(session.snapshot()==changed);
    bool stale=false;
    QTimer::singleShot(0,[&] {
        auto *dialog=child<QDialog>(window,"rirDialog");
        check(bool(session.commit(session.snapshot(),*session.snapshot())));
        const auto source=session.snapshot();
        child<QDialogButtonBox>(*dialog)->button(QDialogButtonBox::Ok)->click();
        stale=session.snapshot()==source && !child<QLabel>(*dialog,"rirError")->text().isEmpty();
        dialog->reject();
    });
    edit->click(); check(stale);
    table->setCurrentIndex(table->model()->index(1,1)); restore->click();
    check(quant->quantificationResult().available() && !quant->quantificationResult().phases[1].rir_revised);
    // Non-widget callers publish the same checked revision; candidate UI stays intact.
    {
        QSignalBlocker block(candidateTable->selectionModel());
        candidateTable->setCurrentIndex(candidateTable->model()->index(0,2));
    }
    check(bool(w.revisePfsmRir(session.snapshot(),"B",PfsmRirRevision{6,"Application API source; Cu Kalpha"})));
    check(candidateTable->currentIndex().isValid() && candidateTable->currentIndex().data().toString()=="C");
    check(std::abs(*quant->quantificationResult().phases[0].weight_percent-100./3)<1e-9 && reader.calls==1);
    check(bool(w.revisePfsmRir(session.snapshot(),"B",std::nullopt)));
    check(candidateTable->currentIndex().isValid() && candidateTable->currentIndex().data().toString()=="C");
    // A missing selected reference makes the complete composition unavailable.
    const auto source=session.snapshot(); auto missing=*source;
    auto analysis=std::make_shared<PfsmAnalysis>(*source->pfsm);
    auto library=std::make_shared<PfsmQuantificationLibrary>(*analysis->quantification_library);
    library->references["B"].rir.reset(); library->references["B"].original_rir.clear();
    analysis->quantification_library=library; missing.pfsm=analysis;
    check(bool(session.commit(source,std::move(missing))));
    check(!quant->quantificationResult().available() && child<PieChartWidget>(*quant)->isHidden());
    check(child<QLabel>(*quant,"quantificationStatus")->text().contains("B:"));
    check(table->model()->index(0,3).data().toString()=="-");
    check(browser->toPlainText().contains("missing_rir") && !browser->toHtml().contains("data:image/png"));
    capture("missing.png");
    check(reader.calls==1);
    table->setCurrentIndex(table->model()->index(1,1));
    const auto closingSource=session.snapshot();
    QTimer::singleShot(0,[&] {
        QTimer::singleShot(0,[&] {
            for (auto *dialog : window.findChildren<QMessageBox*>())
                dialog->button(QMessageBox::Yes)->click();
        });
        window.close(); // The owning window must discard the private RIR draft.
    });
    edit->click();
    check(window.isClosing() && session.snapshot()==closingSource);
    std::cout<<"PASS: PFSM/RIR panel, report, revisions, cancel, stale source and unavailable pie\n";
    return 0;
} catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
