#pragma once
#include "qualpf/application/database/cif_batch.h"
#include <memory>

namespace qualpf::application {
using PdfBuildProgress = std::function<void(int, std::int64_t, std::int64_t)>;
class PdfBuildSource {
public:
    virtual ~PdfBuildSource() = default;
    virtual std::int64_t size() const = 0;
    virtual bool parse(PdfBuildProgress) = 0;
    virtual void cancel() = 0;
};
class PdfDatabaseBuild {
public:
    explicit PdfDatabaseBuild(std::unique_ptr<PdfBuildSource> source) : source_(std::move(source)) {}
    std::int64_t size() const { return source_->size(); }
    bool run(PdfBuildProgress progress = {}) { return source_->parse(std::move(progress)) && !cancelled_; }
    void cancel() { cancelled_ = true; source_->cancel(); }
private:
    std::unique_ptr<PdfBuildSource> source_;
    bool cancelled_ = false;
};
class DatabaseBuildCancellation {
public:
    void cancel() { source_.request_stop(); }
    std::stop_token token() const { return source_.get_token(); }
private:
    std::stop_source source_;
};
using CifDiscovery = std::variant<std::vector<CifBatchRequest>, CifImportFailure>;
class DatabaseBuildProvider {
public:
    virtual ~DatabaseBuildProvider() = default;
    virtual std::unique_ptr<PdfBuildSource> preparePdf(const std::string &base, const std::string &source) = 0;
    virtual CifDiscovery discoverCif(const std::string &directory, bool recursive) const = 0;
    virtual CifBackendResult backend() const = 0;
    virtual std::unique_ptr<CifRecordWriter> makeWriter() = 0;
    virtual std::unique_ptr<CifSourceReader> makeReader() = 0;
    virtual std::string describe(const CifImportFailure &) const = 0;
    virtual void diagnostic(const CifImportFailure &) const = 0;
};
class DatabaseBuildUseCases {
public:
    explicit DatabaseBuildUseCases(DatabaseBuildProvider &provider) : provider_(provider) {}
    std::unique_ptr<PdfDatabaseBuild> preparePdf(const std::string &, const std::string &);
    CifDiscovery discoverCif(const std::string &path, bool recursive) const { return provider_.discoverCif(path, recursive); }
    CifBatchReport buildCif(const std::string &, const std::vector<CifBatchRequest> &,
                           CifBatchOptions = {}, CifBatchProgress = {});
    std::string describe(const CifImportFailure &error) const { return provider_.describe(error); }
private:
    DatabaseBuildProvider &provider_;
};
std::string cifClassificationName(io::cif::BlockClassification);
} // namespace qualpf::application
