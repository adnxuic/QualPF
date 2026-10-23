#!/usr/bin/env python3
"""Check distribution inputs, inventory actual payloads and export corresponding source.

Ordinary checks validate consistency, not legal clearance. --release additionally
requires resolved rights, a clean revision, exact source and package runtime review.
No command publishes, fetches references, changes scientific inputs or edits Git.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = "deploy/distribution.json"
STAMP = "distribution-source.json"
INVENTORY = "distribution-inventory.json"
LICENSE_FILES = ("LICENSE", "NOTICE.md", "THIRD_PARTY_NOTICES.md")
FIELDS = {"id", "paths", "source", "version", "copyright", "license",
          "modifications", "license_files", "status", "obligations"}


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(value, indent=2, ensure_ascii=True) + "\n")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(stream.read()).hexdigest()


def relative(root, name):
    path = Path(name)
    if not name or path.is_absolute() or re.match(r"^[A-Za-z]:", name) or ".." in path.parts or "\\" in name:
        raise ValueError("unsafe distribution path: " + name)
    target = root / path
    if not target.resolve().is_relative_to(root.resolve()):
        raise ValueError("distribution path escapes its root: " + name)
    return target


def version(root):
    found = re.search(r"project\(QualPF\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
                      (root / "CMakeLists.txt").read_text(encoding="utf-8-sig"))
    if not found:
        raise ValueError("QualPF project version missing")
    return found[1]


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.PIPE)


def extra_product_sources(root, files):
    return [path.relative_to(root).as_posix()
            for folder in ("apps", "src", "third_party", "cmake")
            for path in (root / folder).rglob("*")
            if path.is_file() and path.suffix not in {".pyc", ".pyo"}
            and path.relative_to(root).as_posix() not in files]


def source_identity(root):
    if (root / ".git").exists():
        revision = git(root, "rev-parse", "HEAD").decode().strip()
        dirty = bool(git(root, "status", "--porcelain", "--untracked-files=normal").strip())
        return revision, dirty
    if (root / STAMP).is_file():
        stamp = read_json(root / STAMP)
        dirty = bool(stamp["source_dirty"]) or bool(extra_product_sources(root, stamp["files"])) or any(
            not relative(root, name).is_file() or digest(relative(root, name)) != expected
            for name, expected in stamp["files"].items())
        return stamp["source_revision"], dirty
    return "unknown", True


def build_info(root, compiler="unknown", qt="unknown", audit=False, eigen="unknown", boost="unknown",
               cmake="unknown", generator="unknown", build_tool="unknown"):
    revision, dirty = source_identity(root)
    release_version = version(root)
    product = read_json(root / MANIFEST)["product"]
    return dict(schema="qualpf.distribution_build.v1", product="QualPF", version=release_version,
                license=product["license"], publisher=product["publisher"], source_revision=revision,
                source_dirty=dirty, compiler=compiler, qt=qt, eigen=eigen, boost=boost,
                cmake=cmake, generator=generator, build_tool=build_tool,
                audit_only=audit, source_artifact=f"qualpf-{release_version}-Source{'-audit' if audit else ''}.tar.gz")


def check_inputs(root):
    data = read_json(root / MANIFEST)
    errors = []
    if data.get("schema") != "qualpf.distribution.v1":
        errors.append("unsupported distribution manifest")
    product = data["product"]
    if product["publisher"] != "adnxuic" or product["license"] != "GPL-3.0-only":
        errors.append("application publisher/license mismatch")
    ids = set()
    for entry in data["components"]:
        if not FIELDS.issubset(entry) or not all(entry.get(key) for key in FIELDS - {"license_files"}):
            errors.append("incomplete component provenance: " + str(entry.get("id")))
        if entry["id"] in ids or entry["status"] not in {"reviewed", "blocked", "package-review"}:
            errors.append("duplicate component or invalid review status: " + entry["id"])
        ids.add(entry["id"])
        if entry["status"] == "reviewed" and not entry["license_files"]:
            errors.append("reviewed component lacks license text: " + entry["id"])
        for name in entry["license_files"]:
            if not relative(root, name).is_file():
                errors.append("missing license text: " + name)
    for name in LICENSE_FILES:
        if not (root / name).is_file():
            errors.append("missing application notice: " + name)
    raw = (root / "LICENSE").read_bytes().replace(b"\r\n", b"\n")
    blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
    if blob != data["upstream"]["license_git_blob"]:
        errors.append("canonical GPL text differs from the pinned upstream license")
    for name, expected in data.get("license_text_sha256_lf", {}).items():
        raw_text = relative(root, name).read_bytes().replace(b"\r\n", b"\n")
        if hashlib.sha256(raw_text).hexdigest() != expected:
            errors.append("supplemental license text changed: " + name)
    archive_manifest = read_json(relative(root, data["source_inputs"]["archive_manifest"]))
    archives = {entry["path"]: entry for entry in archive_manifest["files"]}
    for name in data["source_inputs"]["generator_archives"]:
        if name not in archives:
            errors.append("generator archive has no independent identity: " + name)
        elif relative(root, name).is_file() and digest(relative(root, name)) != archives[name]["sha256"]:
            errors.append("generator archive differs from frozen reference: " + name)
    for name in data["source_inputs"]["tracked_generator_inputs"]:
        if not relative(root, name).is_file():
            errors.append("missing preferred table modification input: " + name)
    preferred = set(data["source_inputs"]["tracked_generator_inputs"] + data["source_inputs"]["generator_archives"])
    if preferred.intersection(data.get("excluded_distribution_materials", [])):
        errors.append("required preferred modification inputs cannot be excluded to bypass their grants")
    errors.extend(rights_review_errors(root, data))
    return errors, data


def rights_review_errors(root, data):
    if not data.get("rights_review"):
        return []  # Isolated fixtures and older portable metadata have no review.
    review = read_json(relative(root, data["rights_review"]))
    errors = []
    if review.get("schema") != "qualpf.distribution_rights_review.v1":
        return ["unsupported material rights review"]
    images = review.get("inherited_images", [])
    science = review.get("scientific_inputs", [])
    qrc = ET.parse(root / "src/gui/resources/resource.qrc").getroot()
    active_images = {"src/gui/resources/" + file.text for resource in qrc.findall("qresource")
                     if resource.get("prefix") == "/images" for file in resource.findall("file")}
    if active_images != {entry["path"] for entry in images if entry.get("delivery") != "excluded"}:
        errors.append("active inherited images differ from the rights inventory")
    registered = {"src/gui/resources/" + file.text for resource in qrc.findall("qresource")
                  for file in resource.findall("file")}
    aliases = {(resource.get("prefix"), file.get("alias")): "src/gui/resources/" + file.text
               for resource in qrc.findall("qresource") for file in resource.findall("file")}
    required = set(data["source_inputs"]["tracked_generator_inputs"] + data["source_inputs"]["generator_archives"])
    required.update(path.relative_to(root).as_posix() for path in (root / "share/qualx/files").rglob("*") if path.is_file())
    if not required.issubset({entry["path"] for entry in science}):
        errors.append("scientific preferred/runtime inputs omitted from the rights inventory")
    seen = set()
    for entry in images + science:
        path = relative(root, entry["path"])
        if entry["path"] in seen or entry.get("status") not in {"reviewed", "blocked", "partial"}:
            errors.append("duplicate/invalid material rights entry: " + entry["path"])
        seen.add(entry["path"])
        excluded = entry.get("delivery") == "excluded"
        if excluded and entry["path"] not in data.get("excluded_distribution_materials", []):
            errors.append("material claims exclusion without a distribution rule: " + entry["path"])
        if excluded and entry["path"] in registered:
            errors.append("excluded inherited image registered in runtime: " + entry["path"])
        if entry.get("removed_from_source"):
            if entry not in images or not excluded or not re.fullmatch(r"[0-9a-f]{40}", entry.get("retained_revision", "")):
                errors.append("removed image lacks exclusion/history identity: " + entry["path"])
            if path.exists():
                errors.append("removed inherited image reintroduced into source: " + entry["path"])
            replacement = relative(root, entry.get("replacement", ""))
            if not replacement.is_relative_to(root / "src/gui/resources/icons") or replacement.suffix != ".svg" or not replacement.is_file():
                errors.append("removed image lacks independent SVG replacement: " + entry["path"])
            else:
                for suffix in (".png", "@2x.png"):
                    png = replacement.with_name(replacement.stem + suffix)
                    if not png.is_file() or aliases.get(("/icons", png.name)) != png.relative_to(root).as_posix():
                        errors.append("replacement PNG missing/unregistered: " + png.relative_to(root).as_posix())
        if excluded and not path.exists():
            continue  # A portable source export deliberately omits this original.
        raw = path.read_bytes()
        if entry.get("hash_mode") == "lf":
            raw = raw.replace(b"\r\n", b"\n")
        if hashlib.sha256(raw).hexdigest() != entry["sha256"]:
            errors.append("material changed since its rights review: " + entry["path"])
        if entry.get("status") == "reviewed" and not all(entry.get(key) for key in
                ("license", "copyright", "primary_sources", "license_files")):
            errors.append("material grant is not documented: " + entry["path"])
        for name in entry.get("license_files", []):
            if not relative(root, name).is_file():
                errors.append("material license missing: " + name)
    statuses = {entry["id"]: entry["status"] for entry in data["components"]}
    for component, entries in (("scientific-data", science), ("inherited-artwork-docs", images)):
        if statuses.get(component) == "reviewed" and any(entry["status"] != "reviewed" and
                entry.get("delivery") != "excluded" for entry in entries):
            errors.append("unresolved material grants cannot clear component: " + component)
    return errors


def email_error(email):
    if not email or not re.fullmatch(r"[^\s<>@;\"\\]+@[^\s<>@;\"\\]+\.[A-Za-z]{2,}", email):
        return "DEB requires QUALPF_RELEASE_CONTACT_EMAIL with a real supplied maintainer email"
    domain = email.rsplit("@", 1)[1].lower()
    if domain in {"example.com", "example.org", "example.net"} or domain.endswith((".invalid", ".example", ".test")):
        return "DEB contact email must not be a placeholder"
    return None


def manual_sources(root):
    files = [root / "docs/make_pdf.sh", root / "docs/mkdocs.yml"]
    files += sorted(path for path in (root / "docs/user").rglob("*") if path.is_file())
    return {path.relative_to(root).as_posix(): digest(path) for path in files}


def manual_errors(root, pdf, proof):
    if not pdf.is_file() or not proof.is_file():
        return ["PDF manual has no matching generated-source provenance"]
    value = read_json(proof)
    if value.get("schema") != "qualpf.manual_provenance.v1" or value.get("pdf_sha256") != digest(pdf) or value.get("sources") != manual_sources(root):
        return ["PDF manual or its documentation inputs changed after generation"]
    return []


def release_errors(data, info, kind=None):
    errors = ["unresolved distribution rights: " + entry["id"] + ": " + entry["obligations"]
              for entry in data["components"] if entry["status"] == "blocked"]
    if info["audit_only"]:
        errors.append("audit package is not cleared for distribution")
    if info["source_dirty"] or not re.fullmatch(r"[0-9a-f]{40}", info["source_revision"]):
        errors.append("release requires one clean committed source revision")
    if kind == "IFW" and data.get("release_policy", {}).get("windows_binary_compilers"):
        compiler = info.get("compiler", "unknown").split()[0]
        if compiler not in data["release_policy"]["windows_binary_compilers"]:
            errors.append("Windows formal distribution requires the approved MinGW/GNU kit; MSVC is development-only")
    return errors


def build_identity_errors(info, revision, current_version):
    if info.get("source_revision") != revision or info.get("version") != current_version:
        return ["build metadata is stale for the current revision/version; reconfigure and rebuild before packaging"]
    return []


def excluded_source(name, data):
    if name in data["excluded_legacy_assets"] or name in data.get("excluded_distribution_materials", []):
        return True
    if name.startswith("tests/real_example_data/"):
        return True
    parts = Path(name).parts
    return any(part in {".git", "__pycache__"} for part in parts) or name.endswith((".pyc", ".pyo"))


def source_names(root, data):
    if (root / ".git").exists():
        names = git(root, "ls-files", "-z").decode("utf-8").strip("\0").split("\0")
    elif (root / STAMP).is_file():
        names = list(read_json(root / STAMP)["files"])
    else:
        raise ValueError("source export needs Git or the delivered source identity manifest")
    names = {name for name in names if name and not excluded_source(name, data)}
    names.update(data["source_inputs"]["generator_archives"])
    archives = {entry["path"]: entry for entry in read_json(root / data["source_inputs"]["archive_manifest"])["files"]}
    names.difference_update(set(archives) - set(data["source_inputs"]["generator_archives"]))
    return names, archives


def prepare_source(root, destination, info, data):
    destination = destination.resolve()
    if not destination.is_relative_to((root / "build").resolve()) or destination == (root / "build").resolve():
        raise ValueError("source export must be a new directory inside this project's build directory")
    if destination.exists():
        raise ValueError("refusing to overwrite an existing source export")
    names, archives = source_names(root, data)
    for name in data["source_inputs"]["generator_archives"]:
        source = relative(root, name)
        if not source.is_file() or digest(source) != archives[name]["sha256"]:
            raise ValueError("provision the unchanged preferred generator input before packaging: " + name)
    # Validate every source path before creating the export. No repository data is removed.
    for name in names:
        if not relative(root, name).is_file():
            raise ValueError("tracked source input is missing: " + name)
    destination.mkdir(parents=True)
    hashes = {}
    for name in sorted(names):
        target = relative(destination, name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(relative(root, name), target)
        hashes[name] = digest(target)
    write_json(destination / STAMP, dict(schema="qualpf.distribution_source.v1",
               version=info["version"], source_revision=info["source_revision"],
               source_dirty=info["source_dirty"], audit_only=info["audit_only"], files=hashes))


def check_source_tree(root, data):
    errors = []
    stamp = read_json(root / STAMP)
    for name, expected in stamp["files"].items():
        file = relative(root, name)
        if not file.is_file() or digest(file) != expected:
            errors.append("delivered source file changed/missing: " + name)
    for name in data["source_inputs"]["generator_archives"] + data["source_inputs"]["tracked_generator_inputs"]:
        if name not in stamp["files"]:
            errors.append("preferred modification input omitted from source manifest: " + name)
    for name in data["excluded_legacy_assets"]:
        if relative(root, name).exists():
            errors.append("excluded legacy input in source delivery: " + name)
    for name in data.get("excluded_distribution_materials", []):
        if relative(root, name).exists():
            errors.append("excluded uncleared material in source delivery: " + name)
    if (root / ".git").exists():
        errors.append("source delivery must not depend on Git metadata")
    errors.extend("unlisted product input in source delivery: " + name
                  for name in extra_product_sources(root, stamp["files"]))
    return errors


def runtime_component(name):
    lower = name.lower()
    base = Path(lower).name
    if base in {"qualpf", "qualpf.exe"}:
        return "qualpf-additions"
    if any(token in base for token in ("vcruntime", "msvcp", "vc_redist", "concrt", "vcomp")):
        return "msvc-runtime"
    if "libwinpthread" in base:
        return "mingw-winpthreads"
    if any(token in base for token in ("libgcc", "libstdc++", "libgomp")):
        return "gnu-mingw-runtime"
    if base.startswith(("qt6", "libqt6")) or "/plugins/" in "/" + lower:
        return "qt"
    return "package-runtimes"


def stage_inventory(root, stage, data, review=None, kind=None):
    candidates = [path.parent for path in stage.rglob("distribution-build.json")
                  if path.parent.name == "qualpf" and path.parent.parent.name == "share"]
    if len(candidates) != 1:
        raise ValueError("stage must contain exactly one share/qualpf/distribution-build.json")
    materials = candidates[0]
    prefix = stage
    info = read_json(materials / "distribution-build.json")
    errors = []
    for name in (*LICENSE_FILES, MANIFEST):
        installed = relative(materials, name)
        if not installed.is_file() or installed.read_bytes() != relative(root, name).read_bytes():
            errors.append("missing/stale installed notice: " + name)
    for source in (root / "LICENSES").glob("*.txt"):
        installed = materials / "LICENSES" / source.name
        if not installed.is_file() or installed.read_bytes() != source.read_bytes():
            errors.append("missing/stale installed supplemental license: " + source.name)
    pdf = materials / "docs/qualpf_manual.pdf"
    if pdf.is_file():
        errors.extend("missing/stale installed PDF provenance: " + error
                      for error in manual_errors(root, pdf, materials / "docs/qualpf_manual.provenance.json"))
    if (materials / "DEPENDENCIES").is_dir():
        from prepare_dependency_bundle import verify
        try:
            verify(root, materials / "DEPENDENCIES")
        except (OSError, ValueError, KeyError, TypeError) as error:
            errors.append("missing/stale dependency source/notice bundle: " + str(error))
    files = []
    installed_destinations = {}
    review = review or {}
    excluded_names = {Path(name).name for name in data.get("excluded_legacy_assets", [])}
    for file in sorted(path for path in prefix.rglob("*") if path.is_file()):
        name = file.relative_to(prefix).as_posix()
        relative(prefix, name)  # reject links escaping the stage
        if file.name in excluded_names:
            errors.append("excluded legacy asset in runtime delivery: " + name)
        for excluded in data.get("excluded_distribution_materials", []):
            installed_name = excluded.replace("share/qualx/", "share/qualpf/", 1)
            if name == installed_name or name.endswith("/" + installed_name):
                errors.append("excluded material in runtime delivery: " + name)
        if file.name == INVENTORY:
            continue  # never hash the inventory into itself
        with file.open("rb") as stream:
            magic = stream.read(4)
        executable = file.suffix.lower() in {".exe", ".dll"} or re.search(r"\.so(?:\.[0-9]+)*$", file.name) or magic == b"\x7fELF"
        component = runtime_component(name) if executable else "material"
        value = dict(path=name, bytes=file.stat().st_size, sha256=digest(file), component=component)
        if kind == "IFW":
            destination = re.search(r"(?:^|/)packages/[^/]+/data/(.+)$", name)
            if destination:
                key = destination[1].lower()  # Windows destinations are case-insensitive.
                prior = installed_destinations.get(key)
                if prior and prior["sha256"] != value["sha256"]:
                    errors.append("conflicting installed payload: " + prior["path"] + " and " + name)
                installed_destinations[key] = value
        if component in data.get("release_policy", {}).get("excluded_runtime_components", []):
            errors.append("runtime excluded from formal distribution by owner policy: " + name)
        if file.name.lower() in data.get("release_policy", {}).get("excluded_windows_system_payloads", []):
            errors.append("operating-system DLL must not be redistributed in this package: " + name)
        if executable and component != "qualpf-additions":
            value["review_status"] = "pending"
            source_terms_verified = False
            accepted = review.get("files", {}).get(name, {})
            if accepted.get("sha256") == value["sha256"] and accepted.get("component") == component:
                notices = accepted.get("notices", [])
                source_terms = accepted.get("source_artifacts", [])
                if accepted.get("copyright") and notices and all(relative(prefix, notice).is_file() for notice in notices):
                    source_terms_verified = bool(source_terms) and all(
                        relative(prefix, item["path"]).is_file() and digest(relative(prefix, item["path"])) == item["sha256"]
                        for item in source_terms)
                    if source_terms_verified:
                        value["review_status"] = "reviewed"
                    elif accepted.get("source_exception"):
                        value["review_status"] = "reviewed"
            if value["review_status"] != "reviewed":
                errors.append("unreviewed shipped runtime: " + name)
            else:
                value["copyright"] = accepted["copyright"]
                value["notices"] = notices
                if source_terms_verified:
                    value["source_artifacts"] = source_terms
                else:
                    value["source_exception"] = accepted["source_exception"]
        files.append(value)
    if review and review.get("source_revision") != info["source_revision"]:
        errors.append("runtime review belongs to a different source revision")
    required_reviews = {"qt", "eigen", "boost"}
    if kind in {"IFW", "AppImage"}:
        required_reviews.add("package-runtimes")
    for component in data["components"]:
        if component["id"] in required_reviews:
            entry = review.get("components", {}).get(component["id"], {})
            if not entry.get("version") or not entry.get("copyright") or not entry.get("notices"):
                errors.append("missing package-specific header/SDK review: " + component["id"])
            elif not all(relative(prefix, name).is_file() for name in entry["notices"]):
                errors.append("missing package-specific SDK notice: " + component["id"])
            elif info.get(component["id"]) and entry["version"] != info[component["id"]]:
                errors.append("package-specific SDK version differs from the build: " + component["id"])
            elif component["id"] in {"qt", "eigen"}:
                sources = entry.get("source_artifacts", [])
                if not sources or not all(relative(prefix, item["path"]).is_file() and
                        digest(relative(prefix, item["path"])) == item["sha256"] for item in sources):
                    errors.append("missing/changed package-specific corresponding source: " + component["id"])
    report = dict(schema="qualpf.distribution_inventory.v1", build=info, files=files,
                  component_reviews=review.get("components", {}),
                  review_blockers=errors,
                  rights_blockers=[entry["id"] for entry in data["components"] if entry["status"] == "blocked"])
    return report, materials


def source_archive_errors(path, info, root, data):
    errors = []
    with tarfile.open(path, "r:gz") as archive:
        names = [member for member in archive.getmembers() if member.name.endswith("/" + STAMP) and member.isfile()]
        if len(names) != 1 or names[0].size > 8 * 1024 * 1024:
            return ["corresponding-source archive identity is missing/ambiguous"]
        source = json.load(archive.extractfile(names[0]))
        if source.get("schema") != "qualpf.distribution_source.v1":
            errors.append("unsupported corresponding-source manifest")
        if source.get("source_revision") != info["source_revision"] or source.get("version") != info["version"]:
            errors.append("binary and corresponding-source revision/version differ")
        if source.get("source_dirty") or source.get("audit_only"):
            errors.append("audit/modified source archive cannot clear a release binary")
        prefix = names[0].name[:-len(STAMP)]
        expected = source["files"]
        required, _ = source_names(root, data)
        if set(expected) != required:
            errors.append("corresponding-source manifest does not cover the selected source tree")
        actual = set()
        for member in archive.getmembers():
            if member.isdir():
                continue
            if not member.isfile() or not member.name.startswith(prefix):
                errors.append("unexpected corresponding-source archive entry: " + member.name)
                continue
            name = member.name[len(prefix):]
            relative(root, name)  # validate without extracting archive-controlled paths
            if name == STAMP:
                continue
            if name in actual or name not in expected or excluded_source(name, data):
                errors.append("unexpected/duplicate corresponding-source input: " + name)
                continue
            actual.add(name)
            with archive.extractfile(member) as stream:
                value = hashlib.sha256()
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    value.update(block)
            current = relative(root, name)
            if value.hexdigest() != expected[name] or not current.is_file() or digest(current) != expected[name]:
                errors.append("corresponding-source contents differ from the build input: " + name)
        for name in set(expected) - actual:
            errors.append("corresponding-source archive omitted: " + name)
        for name in data["source_inputs"]["generator_archives"] + data["source_inputs"]["tracked_generator_inputs"]:
            if name not in actual:
                errors.append("corresponding-source archive omitted preferred input: " + name)
    return errors


def ifw_metadata_errors(root, stage, info, data):
    errors = []
    config = ET.parse(stage / "config/config.xml").getroot()
    package_dir = stage / "packages/QualPF/meta"
    package = ET.parse(package_dir / "package.xml").getroot()
    for field, expected in (("Publisher", data["product"]["publisher"]), ("Version", info["version"])):
        if config.findtext(field) != expected:
            errors.append("IFW installer metadata mismatch: " + field)
    for field, expected in (("Version", info["version"]), ("Description", data["product"]["description"]),
                            ("ForcedInstallation", "true")):
        if package.findtext(field) != expected:
            errors.append("IFW component metadata mismatch: " + field)
    script = relative(package_dir, package.findtext("Script") or "").read_text(encoding="utf-8")
    if re.search(r"if\s*\(\s*true\s*\)", script) and "vc_redist.x64.exe" in script:
        runtimes = list(stage.glob("packages/*/data/bin/vc_redist.x64.exe"))
        if not any(path.is_file() for path in runtimes):
            errors.append("MSVC IFW runtime bootstrap is missing; package from the matching Visual Studio developer shell")
    licenses = package.findall("Licenses/License")
    if len(licenses) != 1 or licenses[0].get("name") != data["product"]["license"]:
        errors.append("IFW installer does not display the application license")
    else:
        license_file = relative(package_dir, licenses[0].get("file", ""))
        if not license_file.is_file() or license_file.read_bytes() != (root / "LICENSE").read_bytes():
            errors.append("IFW installer license text differs from the canonical GPL")
    for field, filename, original in (("InstallerApplicationIcon", "cm_appicon.ico", "qualpf.ico"),
                                       ("InstallerWindowIcon", "cm_winicon.png", "qualpf.png")):
        value = config.findtext(field)
        expected = Path(filename).stem if field == "InstallerApplicationIcon" else filename
        icon = stage / "config" / filename
        if value != expected or not icon.is_file() or digest(icon) != digest(root / "src/gui/resources/branding" / original):
            errors.append("IFW installer uses an unexpected product icon: " + field)
    return errors


def self_test():
    class Controls(unittest.TestCase):
        def test_email_and_paths(self):
            self.assertIsNotNone(email_error(""))
            self.assertIsNotNone(email_error("https://github.com/adnxuic/QualPF/issues"))
            self.assertIsNotNone(email_error("name@example.invalid"))
            self.assertIsNone(email_error("maintainer@project.org"))
            with tempfile.TemporaryDirectory() as directory:
                for name in ("../secret", "a/../../secret", "C:/outside", "a\\outside"):
                    with self.assertRaises(ValueError):
                        relative(Path(directory), name)

        def test_rights_and_dirty_revision(self):
            data = dict(components=[dict(id="table", status="blocked", obligations="grant missing")])
            info = dict(audit_only=False, source_dirty=False, source_revision="a" * 40)
            self.assertTrue(any("table" in error for error in release_errors(data, info)))
            data["components"][0]["status"] = "reviewed"
            self.assertEqual(release_errors(data, info), [])
            info["source_dirty"] = True
            self.assertTrue(release_errors(data, info))
            info["source_dirty"], info["audit_only"] = False, True
            self.assertTrue(release_errors(data, info))
            info["version"] = "1.0.0"
            self.assertEqual(build_identity_errors(info, "a" * 40, "1.0.0"), [])
            self.assertTrue(build_identity_errors(info, "b" * 40, "1.0.0"))
            self.assertTrue(build_identity_errors(info, "a" * 40, "1.0.1"))

        def test_owner_release_policy(self):
            data = dict(components=[], release_policy=dict(windows_binary_compilers=["GNU"]))
            info = dict(audit_only=False, source_dirty=False, source_revision="a" * 40,
                        compiler="MSVC 19.40 (Windows/AMD64)")
            self.assertTrue(any("development-only" in error for error in release_errors(data, info, "IFW")))
            # A portable source export is not an MSVC binary distribution.
            self.assertEqual(release_errors(data, info), [])
            info["compiler"] = "GNU 13.1 (Windows/AMD64)"
            self.assertEqual(release_errors(data, info, "IFW"), [])
            self.assertEqual(runtime_component("bin/libwinpthread-1.dll"), "mingw-winpthreads")

        def test_removed_image_replacement(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                resources = root / "src/gui/resources"
                icons = resources / "icons"
                icons.mkdir(parents=True)
                old = resources / "images/old.png"
                old.parent.mkdir()
                qrc = resources / "resource.qrc"
                qrc.write_text('<RCC><qresource prefix="/icons"><file alias="new.png">icons/new.png</file>'
                               '<file alias="new@2x.png">icons/new@2x.png</file></qresource></RCC>')
                for name in ("new.svg", "new.png", "new@2x.png"):
                    (icons / name).write_bytes(b"independent replacement fixture")
                original_path = "src/gui/resources/images/old.png"
                row = dict(path=original_path, sha256=hashlib.sha256(b"old artwork").hexdigest(),
                           status="blocked", delivery="excluded", removed_from_source=True,
                           retained_revision="a" * 40, replacement="src/gui/resources/icons/new.svg")
                review = dict(schema="qualpf.distribution_rights_review.v1", scientific_inputs=[], inherited_images=[row])
                data = dict(rights_review="review.json", source_inputs=dict(tracked_generator_inputs=[], generator_archives=[]),
                            components=[dict(id="inherited-artwork-docs", status="blocked")],
                            excluded_distribution_materials=[original_path])
                write_json(root / "review.json", review)
                self.assertEqual(rights_review_errors(root, data), [])
                data["excluded_distribution_materials"] = []
                self.assertTrue(any("without a distribution rule" in x for x in rights_review_errors(root, data)))
                data["excluded_distribution_materials"] = [original_path]
                old.write_bytes(b"old artwork")
                self.assertTrue(any("reintroduced" in x for x in rights_review_errors(root, data)))
                old.unlink()
                valid_qrc = qrc.read_text()
                # A renamed resource prefix must not bypass the exclusion.
                qrc.write_text(valid_qrc.replace('</RCC>', '<qresource prefix="/renamed"><file>images/old.png</file></qresource></RCC>'))
                self.assertTrue(any("excluded inherited image registered" in x for x in rights_review_errors(root, data)))
                qrc.write_text(valid_qrc)
                for name in ("new.svg", "new.png", "new@2x.png"):
                    png = icons / name
                    original = png.read_bytes()
                    png.unlink()
                    self.assertTrue(rights_review_errors(root, data), name)
                    png.write_bytes(original)
                qrc.write_text(valid_qrc.replace('alias="new@2x.png"', 'alias="wrong@2x.png"'))
                self.assertTrue(any("unregistered" in x for x in rights_review_errors(root, data)))
                qrc.write_text(valid_qrc)
                row["retained_revision"] = ""
                write_json(root / "review.json", review)
                self.assertTrue(any("history identity" in x for x in rights_review_errors(root, data)))

        def test_material_rights_binding(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                qrc = root / "src/gui/resources/resource.qrc"
                qrc.parent.mkdir(parents=True)
                qrc.write_text('<RCC><qresource prefix="/images"><file>image.png</file></qresource></RCC>')
                image = qrc.parent / "image.png"
                image.write_bytes(b"inherited image")
                row = dict(path="src/gui/resources/image.png", sha256=digest(image), status="blocked")
                review = dict(schema="qualpf.distribution_rights_review.v1", scientific_inputs=[], inherited_images=[row])
                data = dict(rights_review="review.json", source_inputs=dict(tracked_generator_inputs=[], generator_archives=[]),
                            components=[dict(id="inherited-artwork-docs", status="blocked")])
                write_json(root / "review.json", review)
                self.assertEqual(rights_review_errors(root, data), [])
                data["components"][0]["status"] = "reviewed"
                self.assertTrue(any("cannot clear" in error for error in rights_review_errors(root, data)))
                data["components"][0]["status"] = "blocked"
                row["status"] = "reviewed"
                write_json(root / "review.json", review)
                self.assertTrue(any("grant" in error for error in rights_review_errors(root, data)))
                image.write_bytes(b"changed inherited image")
                self.assertTrue(any("changed" in error for error in rights_review_errors(root, data)))
                review["inherited_images"] = []
                write_json(root / "review.json", review)
                self.assertTrue(any("active inherited" in error for error in rights_review_errors(root, data)))

        def test_stage_notices_and_runtime_hash_binding(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / "source"
                stage = Path(directory) / "stage"
                materials = stage / "share/qualpf"
                for name in (*LICENSE_FILES, MANIFEST, "LICENSES/library.txt"):
                    for base in (root, materials):
                        target = base / name
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_text("original notice\n", encoding="utf-8")
                write_json(materials / "distribution-build.json", dict(source_revision="a" * 40))
                dll = stage / "bin/Qt6Core.dll"
                dll.parent.mkdir(parents=True)
                dll.write_bytes(b"runtime")
                data = dict(components=[])
                report, _ = stage_inventory(root, stage, data)
                self.assertTrue(any("runtime" in error for error in report["review_blockers"]))
                review = dict(source_revision="a" * 40, files={"bin/Qt6Core.dll": dict(
                    sha256=digest(dll), component="qt", copyright="actual authors",
                    notices=["share/qualpf/LICENSE"], source_exception="fixture permissive exception")})
                report, _ = stage_inventory(root, stage, data, review)
                self.assertEqual(report["review_blockers"], [])
                dll.write_bytes(b"changed runtime")
                report, _ = stage_inventory(root, stage, data, review)
                self.assertTrue(report["review_blockers"])
                (materials / "NOTICE.md").unlink()
                report, _ = stage_inventory(root, stage, data)
                self.assertTrue(any("installed notice" in error for error in report["review_blockers"]))
                data["excluded_legacy_assets"] = ["original/qualx.png"]
                (materials / "qualx.png").write_bytes(b"old brand")
                report, _ = stage_inventory(root, stage, data)
                self.assertTrue(any("legacy asset" in error for error in report["review_blockers"]))
                data["release_policy"] = dict(excluded_runtime_components=["msvc-runtime"],
                                               excluded_windows_system_payloads=["d3dcompiler_47.dll"])
                (stage / "bin/vc_redist.x64.exe").write_bytes(b"VC runtime")
                (stage / "bin/D3Dcompiler_47.dll").write_bytes(b"system DLL copy")
                report, _ = stage_inventory(root, stage, data)
                self.assertTrue(any("owner policy" in error for error in report["review_blockers"]))
                self.assertTrue(any("operating-system DLL" in error for error in report["review_blockers"]))

        def test_ifw_component_destination_conflict(self):
            with tempfile.TemporaryDirectory() as directory:
                root, stage = Path(directory) / "source", Path(directory) / "stage"
                materials = stage / "packages/QualPF/data/share/qualpf"
                for name in (*LICENSE_FILES, MANIFEST):
                    for base in (root, materials):
                        target = base / name
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(b"original notice")
                write_json(materials / "distribution-build.json", dict(source_revision="a" * 40))
                first = stage / "packages/QualPF/data/bin/libgcc_s_seh-1.dll"
                second = stage / "packages/Unspecified/data/bin/LIBGCC_S_SEH-1.DLL"
                for path in (first, second):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"matching compiler runtime")
                report, _ = stage_inventory(root, stage, dict(components=[]), kind="IFW")
                self.assertFalse(any("conflicting installed" in item for item in report["review_blockers"]))
                second.write_bytes(b"different PATH runtime")
                report, _ = stage_inventory(root, stage, dict(components=[]), kind="IFW")
                self.assertTrue(any("conflicting installed" in item for item in report["review_blockers"]))

        def test_preferred_source_and_legacy_exclusion(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "table.inc").write_bytes(b"accepted table")
                data = dict(source_inputs=dict(generator_archives=["preferred.zip"], tracked_generator_inputs=[]),
                            excluded_legacy_assets=["old.png"])
                write_json(root / STAMP, dict(files={"table.inc": digest(root / "table.inc")}))
                self.assertTrue(any("preferred" in error for error in check_source_tree(root, data)))
                (root / "preferred.zip").write_bytes(b"independent source")
                write_json(root / STAMP, dict(files={name: digest(root / name) for name in ("table.inc", "preferred.zip")}))
                self.assertEqual(check_source_tree(root, data), [])
                (root / "old.png").write_bytes(b"old brand")
                self.assertTrue(any("legacy" in error for error in check_source_tree(root, data)))
                (root / "table.inc").write_bytes(b"modified table")
                self.assertTrue(any("changed/missing" in error for error in check_source_tree(root, data)))
                data["excluded_distribution_materials"] = ["restricted.cif"]
                (root / "restricted.cif").write_bytes(b"reserved independent test input")
                self.assertTrue(excluded_source("restricted.cif", data))
                self.assertTrue(excluded_source("tests/real_example_data/03_reference_structures/CoO.cif", data))
                self.assertTrue(any("uncleared material" in error for error in check_source_tree(root, data)))

        def test_sdk_source_and_version_binding(self):
            with tempfile.TemporaryDirectory() as directory:
                root, stage = Path(directory) / "root", Path(directory) / "stage"
                materials = stage / "share/qualpf"
                for name in (*LICENSE_FILES, MANIFEST):
                    for base in (root, materials):
                        file = base / name
                        file.parent.mkdir(parents=True, exist_ok=True)
                        file.write_bytes(b"original legal fixture")
                write_json(materials / "distribution-build.json", dict(source_revision="a" * 40, qt="6.8.3"))
                runtime = stage / "bin/Qt6Core.dll"
                runtime.parent.mkdir()
                runtime.write_bytes(b"runtime fixture")
                source = materials / "source.tar.gz"
                source.write_bytes(b"independent corresponding-source fixture")
                entry = dict(version="6.8.3", copyright="fixture authors", notices=["share/qualpf/LICENSE"],
                             source_artifacts=[dict(path="share/qualpf/source.tar.gz", sha256=digest(source))])
                review = dict(source_revision="a" * 40, components=dict(qt=entry),
                              files={"bin/Qt6Core.dll": dict(entry, sha256=digest(runtime), component="qt")})
                data = dict(components=[dict(id="qt", status="package-review")])
                report, _ = stage_inventory(root, stage, data, review)
                self.assertEqual(report["review_blockers"], [])
                entry["version"] = "6.9.0"
                report, _ = stage_inventory(root, stage, data, review)
                self.assertTrue(any("SDK version differs" in error for error in report["review_blockers"]))
                entry["version"] = "6.8.3"
                source.write_bytes(b"changed source")
                report, _ = stage_inventory(root, stage, data, review)
                self.assertTrue(any("corresponding source" in error for error in report["review_blockers"]))

        def test_added_source_marks_delivered_revision_dirty(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                write_json(root / STAMP, dict(source_revision="a" * 40, source_dirty=False, files={}))
                self.assertEqual(source_identity(root), ("a" * 40, False))
                (root / "src").mkdir()
                (root / "src/added.cpp").write_bytes(b"new product source")
                self.assertEqual(source_identity(root), ("a" * 40, True))

        def test_stale_generated_manual(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "docs/user").mkdir(parents=True)
                for name in ("docs/make_pdf.sh", "docs/mkdocs.yml", "docs/user/index.md"):
                    (root / name).write_text("current source", encoding="utf-8")
                pdf, proof = root / "manual.pdf", root / "manual.json"
                pdf.write_bytes(b"%PDF-1.7 identity fixture")
                self.assertTrue(manual_errors(root, pdf, proof))
                write_json(proof, dict(schema="qualpf.manual_provenance.v1", pdf_sha256=digest(pdf), sources=manual_sources(root)))
                self.assertEqual(manual_errors(root, pdf, proof), [])
                (root / "docs/user/index.md").write_text("updated publisher", encoding="utf-8")
                self.assertTrue(manual_errors(root, pdf, proof))

        def test_source_archive_contents(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "input.cpp").write_bytes(b"independent source")
                info = dict(source_revision="a" * 40, version="1.0.0")
                write_json(root / STAMP, dict(schema="qualpf.distribution_source.v1", **info,
                    source_dirty=False, audit_only=False, files={"input.cpp": digest(root / "input.cpp")}))
                write_json(root / "archives.json", dict(files=[]))
                data = dict(source_inputs=dict(generator_archives=[], tracked_generator_inputs=[], archive_manifest="archives.json"), excluded_legacy_assets=[])
                archive = root / "source.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    for name in (STAMP, "input.cpp"):
                        stream.add(root / name, arcname="source/" + name)
                self.assertEqual(source_archive_errors(archive, info, root, data), [])
                (root / "input.cpp").write_bytes(b"wrong source")
                self.assertTrue(any("contents differ" in error for error in source_archive_errors(archive, info, root, data)))

        def test_ifw_displayed_license_and_publisher(self):
            with tempfile.TemporaryDirectory() as directory:
                root, stage = Path(directory) / "source", Path(directory) / "installer"
                branding, config, meta = root / "src/gui/resources/branding", stage / "config", stage / "packages/QualPF/meta"
                for folder in (branding, config, meta):
                    folder.mkdir(parents=True)
                (root / "LICENSE").write_bytes(b"canonical GPL fixture")
                (meta / "LICENSE").write_bytes((root / "LICENSE").read_bytes())
                for original, staged in (("qualpf.ico", "cm_appicon.ico"), ("qualpf.png", "cm_winicon.png")):
                    (branding / original).write_bytes(original.encode())
                    (config / staged).write_bytes(original.encode())
                config_text = '<Installer><Publisher>adnxuic</Publisher><Version>1.0.0</Version><InstallerApplicationIcon>cm_appicon</InstallerApplicationIcon><InstallerWindowIcon>cm_winicon.png</InstallerWindowIcon></Installer>'
                (config / "config.xml").write_text(config_text, encoding="utf-8")
                package = '<Package><Version>1.0.0</Version><Description>description</Description><ForcedInstallation>true</ForcedInstallation><Script>installscript.qs</Script><Licenses><License name="GPL-3.0-only" file="LICENSE"/></Licenses></Package>'
                (meta / "installscript.qs").write_text('if (false) { execute("vc_redist.x64.exe"); }', encoding="utf-8")
                (meta / "package.xml").write_text(package, encoding="utf-8")
                data, info = dict(product=dict(publisher="adnxuic", description="description", license="GPL-3.0-only")), dict(version="1.0.0")
                self.assertEqual(ifw_metadata_errors(root, stage, info, data), [])
                (config / "config.xml").write_text(config_text.replace("adnxuic", "Institute of Crystallography - CNR"), encoding="utf-8")
                self.assertTrue(any("Publisher" in error for error in ifw_metadata_errors(root, stage, info, data)))
                (meta / "LICENSE").write_bytes(b"old restricted agreement")
                self.assertTrue(any("canonical GPL" in error for error in ifw_metadata_errors(root, stage, info, data)))
                (meta / "package.xml").write_text(package.replace('<Licenses><License name="GPL-3.0-only" file="LICENSE"/></Licenses>', ''), encoding="utf-8")
                self.assertTrue(any("display" in error for error in ifw_metadata_errors(root, stage, info, data)))
                (meta / "installscript.qs").write_text('if (true) { execute("vc_redist.x64.exe"); }', encoding="utf-8")
                self.assertTrue(any("runtime bootstrap" in error for error in ifw_metadata_errors(root, stage, info, data)))
                runtime = stage / "packages/Unspecified/data/bin/vc_redist.x64.exe"
                runtime.parent.mkdir(parents=True)
                runtime.write_bytes(b"MZ runtime fixture")
                self.assertFalse(any("runtime bootstrap" in error for error in ifw_metadata_errors(root, stage, info, data)))
    return unittest.TextTestRunner(verbosity=1).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--build", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--kind", choices=("IFW", "DEB", "AppImage", "portable", "source"))
    parser.add_argument("--contact-email", default="")
    parser.add_argument("--release", action="store_true")
    parser.add_argument("--audit-only", action="store_true")
    parser.add_argument("--review", type=Path)
    parser.add_argument("--source-artifact", type=Path)
    parser.add_argument("--source-tree", action="store_true")
    parser.add_argument("--ifw-metadata", type=Path)
    parser.add_argument("--prepare-source", type=Path)
    parser.add_argument("--write-build-info", type=Path)
    parser.add_argument("--compiler", default="unknown")
    parser.add_argument("--qt-version", default="unknown")
    parser.add_argument("--eigen-version", default="unknown")
    parser.add_argument("--boost-version", default="unknown")
    parser.add_argument("--cmake-version", default="unknown")
    parser.add_argument("--generator", default="unknown")
    parser.add_argument("--build-tool", default="unknown")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--record-artifacts", type=Path)
    parser.add_argument("--artifact", action="append", type=Path, default=[])
    parser.add_argument("--tool", action="append", type=Path, default=[])
    parser.add_argument("--manual-file", type=Path)
    parser.add_argument("--manual-proof", type=Path)
    parser.add_argument("--write-manual-proof", action="store_true")
    parser.add_argument("--check-manual", action="store_true")
    args = parser.parse_args()
    if args.self_test and not self_test():
        return 1
    root = args.root.resolve()
    errors, data = check_inputs(root)
    if args.prepare_source and not (args.release or args.audit_only):
        errors.append("source export requires explicit --release or --audit-only")
    info = build_info(root, args.compiler, args.qt_version, args.audit_only, args.eigen_version,
                      args.boost_version, args.cmake_version, args.generator, args.build_tool)
    if args.build:
        info = read_json(args.build / "configured_files/distribution-build.json")
        revision, dirty = source_identity(root)
        errors.extend(build_identity_errors(info, revision, version(root)))
        info["source_dirty"] = info["source_dirty"] or dirty
    if args.kind == "DEB" and email_error(args.contact_email):
        errors.append(email_error(args.contact_email))
    if args.release:
        if args.audit_only:
            errors.append("--release and --audit-only are mutually exclusive")
        errors.extend(release_errors(data, info, args.kind))
    if args.source_tree:
        errors.extend(check_source_tree(root, data))
    if args.ifw_metadata:
        errors.extend(ifw_metadata_errors(root, args.ifw_metadata, info, data))
    if args.check_manual:
        if not args.manual_file or not args.manual_proof:
            raise ValueError("manual check requires --manual-file and --manual-proof")
        errors.extend(manual_errors(root, args.manual_file, args.manual_proof))
    if args.stage:
        report, materials = stage_inventory(root, args.stage.resolve(), data,
                                            read_json(args.review) if args.review else None, args.kind)
        if args.build and any(report["build"].get(key) != value for key, value in info.items() if key != "source_dirty"):
            errors.append("staged build metadata differs from the selected build")
        info = report["build"]
        if args.release:
            errors.extend(release_errors(data, info, args.kind))
            errors.extend(report["review_blockers"])
            if not args.source_artifact:
                errors.append("release binary requires --source-artifact, not a moving branch URL")
            else:
                errors.extend(source_archive_errors(args.source_artifact, info, root, data))
        # Missing/stale legal texts are technical failures even in an audit package.
        errors.extend(error for error in report["review_blockers"] if error.startswith((
            "missing/stale", "excluded legacy", "excluded material", "conflicting installed")))
        if args.output:
            write_json(args.output, report)
        print(f"Inventory: {len(report['files'])} files; {len(report['review_blockers'])} package review blockers")
    if errors:
        for error in dict.fromkeys(errors):
            print("FAIL:", error)
        return 1
    if args.write_build_info:
        write_json(args.write_build_info, info)
    if args.write_manual_proof:
        if not args.manual_file or not args.manual_proof:
            raise ValueError("manual generation requires --manual-file and --manual-proof")
        write_json(args.manual_proof, dict(schema="qualpf.manual_provenance.v1",
            pdf_sha256=digest(args.manual_file), sources=manual_sources(root)))
    if args.prepare_source:
        prepare_source(root, args.prepare_source, info, data)
        print("Source export:", args.prepare_source)
    if args.record_artifacts:
        write_json(args.record_artifacts, dict(schema="qualpf.distribution_artifacts.v1", build=info,
            artifacts=[dict(file=path.name, bytes=path.stat().st_size, sha256=digest(path)) for path in args.artifact],
            build_tools=[dict(file=path.name, bytes=path.stat().st_size, sha256=digest(path)) for path in args.tool]))
    pending = [entry["id"] for entry in data["components"] if entry["status"] == "blocked"]
    print("PASS: distribution input consistency" + ("; rights still unresolved: " + ", ".join(pending) if pending else ""))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, tarfile.TarError, ET.ParseError) as error:
        print("FAIL:", error)
        raise SystemExit(1)
