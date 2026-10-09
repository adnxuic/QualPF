#pragma once
#include <QString>
// Options for database search from command line
struct SearchOptions
{
    bool    enabled          = false;  // --search was specified
    QString composition;               // --composition "Al AND Si OR O"
    bool    exactComposition = false;  // --exact  (ONLY_OP: all elements, no others)
    bool    containsAny      = false;  // --contains-any (JUST_OP: only these elements allowed)
};
