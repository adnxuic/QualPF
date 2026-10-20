#pragma once
#include "qualpf/profile/pfsm_candidate_recall.h"
#include <QJsonObject>
QJsonObject pfsmCandidateRecallLimitsJson(const qualpf::profile::PfsmCandidateRecallLimits &);
QJsonObject pfsmCandidateRecallCandidateJson(const qualpf::profile::PfsmCandidateRecallCandidate &);
QJsonObject pfsmCandidateRecallSelectionJson(const qualpf::profile::PfsmCandidateRecallSelection &);
