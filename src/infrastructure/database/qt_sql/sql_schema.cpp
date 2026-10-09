#include "sql_schema.h"
namespace qualpf::infrastructure {
QStringList mainSchema(bool isPdf2) {
    // infodb.id is INTEGER for Pdf2, VARCHAR(100) for CifFiles
    const QString infodbIdType = isPdf2 ? QStringLiteral("INTEGER") : QStringLiteral("VARCHAR(100)");

    // id table: Pdf2 has bestdval; CifFiles has natoms + nreflections
    const QString idExtraColumns =
        isPdf2 ? QStringLiteral(R"(            "bestdval"         VARCHAR(300) DEFAULT NULL,)")
               : QStringLiteral(R"(            "natoms"           INTEGER      DEFAULT NULL,
            "nreflections"     INTEGER      DEFAULT NULL,)");

    QStringList stmts = {
        // Database metadata
        QStringLiteral("CREATE TABLE IF NOT EXISTS \"infodb\" ("
                       "  \"id\"     %1          NOT NULL,"
                       "  \"date\"   VARCHAR(30) NOT NULL,"
                       "  \"ncard\"  INTEGER     NOT NULL,"
                       "  \"type\"   VARCHAR(10) NOT NULL,"
                       "  \"source\" VARCHAR(50) NOT NULL"
                       ")")
            .arg(infodbIdType),

        // Main card table
        QStringLiteral("CREATE TABLE IF NOT EXISTS \"id\" ("
                       "  \"id\"               INTEGER      DEFAULT NULL,"
                       "  \"name\"             VARCHAR(300) DEFAULT NULL,"
                       "  \"mineralname\"      VARCHAR(300) DEFAULT NULL,"
                       "  \"chemical_formula\" VARCHAR(380) DEFAULT NULL,"
                       "  \"spacegroup\"       VARCHAR(30)  DEFAULT NULL,"
                       "  \"quality\"          VARCHAR(2)   DEFAULT NULL,"
                       "  \"rir\"              REAL         DEFAULT NULL,"
                       "  \"nrec\"             INTEGER      DEFAULT NULL,"
                       "  %1"
                       "  \"nd\"               INTEGER      DEFAULT 0,"
                       "  \"dvalue\"           BLOB,"
                       "  \"intensita\"        BLOB,"
                       "  \"n\"                INTEGER      NOT NULL,"
                       "  PRIMARY KEY (\"n\")"
                       ")")
            .arg(idExtraColumns),

        R"(CREATE INDEX IF NOT EXISTS "id_id_" ON "id" ("id"))",

        // Chemical elements
        R"(CREATE TABLE IF NOT EXISTS "chemical" (
            "id"               INTEGER     NOT NULL DEFAULT 0,
            "chemical_element" VARCHAR(10) NOT NULL DEFAULT '',
            PRIMARY KEY ("id", "chemical_element")
        ))",
        R"(CREATE INDEX IF NOT EXISTS "chemical_id" ON "chemical" ("id", "chemical_element"))",

        // Subfile codes
        R"(CREATE TABLE IF NOT EXISTS "subfiles" (
            "id"      INTEGER    NOT NULL DEFAULT 0,
            "subfile" VARCHAR(5) NOT NULL DEFAULT '',
            PRIMARY KEY ("id", "subfile")
        ))",
        R"(CREATE INDEX IF NOT EXISTS "subfiles_id" ON "subfiles" ("id", "subfile"))",
    };

    // CifFiles only: warning tables
    if (!isPdf2) {
        stmts += {
            R"(CREATE TABLE IF NOT EXISTS "warn" (
                "id"      INTEGER      NOT NULL,
                "stringa" VARCHAR(200) NOT NULL,
                PRIMARY KEY ("id")
            ))",
            R"(CREATE TABLE IF NOT EXISTS "warningcif" (
                "id"      INTEGER NOT NULL,
                "idw"     INTEGER NOT NULL,
                "idlabel" INTEGER NOT NULL
            ))",
            R"(CREATE INDEX IF NOT EXISTS "warningcif_idlabel" ON "warningcif" ("idw", "id"))",
        };
    }

    return stmts;
}

QStringList infoSchema() {
    QStringList stmts;

    // Full bibliographic and crystallographic record. Identical schema for
    // both Pdf2 and CifFiles; fields the source data doesn't provide are
    // populated with empty-string/zero placeholders by the respective
    // populator (see Pdf2DatabaseWriter / CifDbPopulator).
    // Uses R"sql(...)sql" delimiter because the column name "mu(CuKa)"
    // contains )" which would terminate a plain R"(...)".
    stmts += {
        R"sql(CREATE TABLE IF NOT EXISTS "info" (
            "id"              VARCHAR(15)  NOT NULL,
            "authors"         VARCHAR(200) NOT NULL,
            "journal"         VARCHAR(200) NOT NULL,
            "journal_year"    VARCHAR(5)   NOT NULL,
            "journal_volume"  VARCHAR(5)   NOT NULL,
            "journal_issue"   VARCHAR(5)   NOT NULL,
            "page_start"      VARCHAR(10)  NOT NULL,
            "page_end"        VARCHAR(10)  NOT NULL,
            "color"           VARCHAR(40)  NOT NULL,
            "crystal_density" REAL         NOT NULL,
            "z"               VARCHAR(20)  NOT NULL,
            "spacegroup"      VARCHAR(30)  NOT NULL,
            "type"            VARCHAR(30)  NOT NULL,
            "volume"          REAL         NOT NULL,
            "density"         REAL         NOT NULL,
            "mu(CuKa)"        VARCHAR(20)  NOT NULL,
            "natoms"          INTEGER      NOT NULL,
            "nreflections"    INTEGER      NOT NULL,
            "a"               REAL         NOT NULL,
            "b"               REAL         NOT NULL,
            "c"               REAL         NOT NULL,
            "alpha"           REAL         NOT NULL,
            "beta"            REAL         NOT NULL,
            "gamma"           REAL         NOT NULL,
            "rir"             VARCHAR(10)  NOT NULL,
            "h"               BLOB         NOT NULL,
            "k"               BLOB         NOT NULL,
            "l"               BLOB         NOT NULL,
            "mul"             BLOB         NOT NULL
        ))sql",
        R"(CREATE INDEX IF NOT EXISTS "info_id_" ON "info" ("id"))",
    };

    // spgrstat is present in both variants
    stmts += R"(CREATE TABLE IF NOT EXISTS "spgrstat" (
        "spacegroup" VARCHAR(20) DEFAULT NULL,
        "type"       VARCHAR(50) DEFAULT NULL,
        "ids"        TEXT,
        "n"          INTEGER     DEFAULT NULL
    ))";

    return stmts;
}

QStringList infoStatSchema() {
    auto numTable = [](const QString &name) -> QString {
        return QStringLiteral("CREATE TABLE IF NOT EXISTS \"%1\" ("
                              "  \"val\" REAL    NOT NULL,"
                              "  \"ids\" TEXT,"
                              "  \"n\"   INTEGER DEFAULT NULL,"
                              "  PRIMARY KEY (\"val\")"
                              ")")
            .arg(name);
    };

    auto txtTable = [](const QString &name) -> QString {
        return QStringLiteral("CREATE TABLE IF NOT EXISTS \"%1\" ("
                              "  \"val\" VARCHAR(50) NOT NULL,"
                              "  \"ids\" TEXT,"
                              "  \"n\"   INTEGER     DEFAULT NULL,"
                              "  PRIMARY KEY (\"val\")"
                              ")")
            .arg(name);
    };

    const QStringList stmts = {
        numTable("stat_a"),     numTable("stat_b"),     numTable("stat_c"),    numTable("stat_alpha"),
        numTable("stat_beta"),  numTable("stat_gamma"), numTable("stat_vol"),  numTable("stat_dens"),
        numTable("stat_cdens"), txtTable("stat_color"), txtTable("stat_spgr"), txtTable("stat_type"),
    };

    return stmts;
}

QStringList searchSchema() {
    const QStringList stmts = {
        R"(CREATE TABLE IF NOT EXISTS "top" (
            "id"   INTEGER     NOT NULL DEFAULT 0,
            "n"    VARCHAR(5)  NOT NULL DEFAULT '',
            "dval" VARCHAR(50) NOT NULL DEFAULT '',
            PRIMARY KEY ("id")
        ))",
    };

    return stmts;
}
} // namespace qualpf::infrastructure
