#pragma once
#include "qualpf/profile/pfsm_candidate_recall.h"
#include "qualpf/application/pfsm.h"
#include <QJsonObject>
QJsonObject pfsmCandidateRecallLimitsJson(const qualpf::profile::PfsmCandidateRecallLimits &);
QJsonObject pfsmCandidateRecallCandidateJson(const qualpf::profile::PfsmCandidateRecallCandidate &);
QJsonObject pfsmCandidateRecallSelectionJson(const qualpf::profile::PfsmCandidateRecallSelection &);
QJsonObject pfsmIntegralRecallReportJson(const qualpf::application::PfsmIntegralRecallReport &);
QJsonObject pfsmIntegralRecallCandidateJson(const qualpf::profile::PfsmScreenEvidence &);
