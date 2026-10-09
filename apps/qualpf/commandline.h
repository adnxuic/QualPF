#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <QCommandLineParser>
#include <QString>

#include "qualpf/application/startup/startup_options.h"
#include "gui/model/preprocessing_options.h"

#include "gui/search/search_options.h"

// Options for database creation from command line
struct DbBuildOptions
{
    bool enabled = false;       // --createdb was specified

    enum class Source { None, Pdf2, Cif } source = Source::None;

    QString pdf2File;           // --pdf2 <path>    : path to pdf2.dat
    QString cifDir;             // --cifdir <folder>: folder with .cif files
    bool    recursive = false;  // --recursive      : scan cifDir recursively
    bool    inorganic = false;  // --inorganic      : include only inorganic structures
    QString outputDb;           // --dbout <path>   : output database base path
};

enum CommandLineParseResult
{
    CommandLineOk,
    CommandLineError,
    CommandLineVersionRequested,
    CommandLineHelpRequested
};

CommandLineParseResult parseCommandLine(QCommandLineParser &parser, QString &filein, QString &fileout,
                                        qualpf::application::StartupOptions &popt, DbBuildOptions &dbopt, SearchOptions &searchopt,
                                        QString &errorMessage, bool &test, QString &testFolder);

#endif // COMMANDLINE_H
