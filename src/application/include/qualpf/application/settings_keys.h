#pragma once
#ifndef PROGKEYSETTINGS_H
#define PROGKEYSETTINGS_H

#define DEFAULT_DIR_KEY "default_dir"
#define QUALX_GEOMETRY_KEY "qualxgeometry"
#define QUALX_STATE_KEY "qualxstate"
#define QUALX_IMAGE_FILTER_KEY "qualximagefilter"
#define QUALX_USER_WAVE "qualxuserwave"
#define QUALX_WAVE_INDEX "qaulxwaveindex"
#define XPDVIEW_ACTION_KEY "xpdviewaction"
#define QUALX_RECENT_FILES_KEY "qualxrecentfiles"

// Database registry keys.
// DB_INUSE_KEY / DB_NAME_KEY / DB_PATH_KEY expect a single %1 = zero-based index.
#define DB_COUNT_KEY       "databases/count"
#define DB_INUSE_KEY       "databases/%1/inUse"
#define DB_NAME_KEY        "databases/%1/name"
#define DB_PATH_KEY        "databases/%1/path"
#define DB_DEFAULT_DIR_KEY "databases/defaultDir"

#endif // PROGKEYSETTINGS_H

namespace qualpf::application::settings {
inline constexpr char SK_FOM[] = "SearchOptions/minFom";
inline constexpr char SK_2THETA[] = "SearchOptions/weight2thetaD";
inline constexpr char SK_INT[] = "SearchOptions/weightIntensity";
inline constexpr char SK_PHASES[] = "SearchOptions/weightPhases";
inline constexpr char SK_DELTA[] = "SearchOptions/delta2theta";
inline constexpr char SK_AUTO[] = "SearchOptions/delta2thetaAuto";
inline constexpr char SK_RESIDUAL[] = "SearchOptions/residualSearching";
inline constexpr char SK_STRONGEST[] = "SearchOptions/checkStrongest";
inline constexpr char SK_DELETED[] = "SearchOptions/checkDeleted";
inline constexpr char SK_MAXENT[] = "SearchOptions/maxEntries";
}
