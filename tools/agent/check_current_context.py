#!/usr/bin/env python3
"""Check Agent rules, navigation and single-task handoff without replaying acceptance."""
import argparse
from copy import deepcopy
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
STATE = ".agent/TASK_STATE.json"
BASELINE = "docs/development/project-baseline.json"
AGENT_FILES = {"README.md", "POLICY.md", "WORKFLOW.md", "CONTEXT_MAP.md", "TASK_STATE.json", ".gitattributes"}
ENTRY_DOCUMENTS = ("AGENTS.md", ".agent/README.md", ".agent/POLICY.md", ".agent/WORKFLOW.md", ".agent/CONTEXT_MAP.md")
REQUIRED_INPUTS = ("AGENTS.md", "docs/development/building.md", "tests/reference/manifest.json",
                   "tools/architecture/check_boundaries.py", "tools/validation/check_references.py", BASELINE)
RETIRED_NAMES = ("CURRENT_STATE.md", "ARCHITECTURE.md", "NUMERICAL_POLICY.md", "VALIDATION_POLICY.md",
                 "EXECUTION_POLICY.md", "ACTIVE_PHASE.md", "PROJECT_STATE.json", "OWNERSHIP.md", "COMPATIBILITY.md")
TASK_FIELDS = {"id", "goal", "in_scope", "out_of_scope", "status", "base_commit", "checkpoint_commit",
               "completed", "next_action", "blockers", "checks"}
LINK = re.compile(r"\[[^\]\n]*\]\(([^)\n]+)\)")


def nonempty(value):
    return isinstance(value, str) and bool(value.strip())


def relative(value, *, glob=False):
    return (nonempty(value) and value == value.strip() and not PureWindowsPath(value).drive
            and not PurePosixPath(value).is_absolute() and ".." not in PurePosixPath(value).parts
            and not any(character in value for character in "\\:\x00\r\n<>|\"")
            and (glob or not any(character in value for character in "*?[]{}")))


def identity(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{40}", value) is not None


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def validate(state):
    errors = []
    if not isinstance(state, dict):
        return [f"{STATE}: expected an object"]
    if set(state) != {"schema", "baseline_ref", "active_task"}:
        errors.append(f"{STATE}: expected only schema, baseline_ref and active_task")
    if state.get("schema") != "qualpf.agent_task_state.v1":
        errors.append(f"{STATE}.schema: unsupported task state schema")
    if state.get("baseline_ref") != BASELINE:
        errors.append(f"{STATE}.baseline_ref: expected {BASELINE}")
    task = state.get("active_task")
    if task is None:
        return errors
    if not isinstance(task, dict):
        return errors + [f"{STATE}.active_task: expected an object or null"]
    if set(task) != TASK_FIELDS:
        errors.append(f"{STATE}.active_task: missing/unknown fields: {sorted(set(task) ^ TASK_FIELDS)}")
    for field in ("id", "goal", "next_action"):
        if not nonempty(task.get(field)):
            errors.append(f"active_task.{field}: expected a nonempty string")
    if task.get("status") not in ("in_progress", "blocked", "paused"):
        errors.append("active_task.status: expected in_progress, blocked or paused")
    if not identity(task.get("base_commit")):
        errors.append("active_task.base_commit: expected a complete Git identity")
    if task.get("checkpoint_commit") is not None and not identity(task["checkpoint_commit"]):
        errors.append("active_task.checkpoint_commit: expected a complete Git identity or null")
    for field in ("in_scope", "out_of_scope", "completed", "blockers"):
        values = task.get(field)
        if not isinstance(values, list) or not all(nonempty(value) for value in values):
            errors.append(f"active_task.{field}: expected an array of nonempty strings")
        elif field == "in_scope" and (not values or not all(relative(value, glob=True) for value in values)):
            errors.append("active_task.in_scope: expected nonempty repository-relative paths/globs")
    if task.get("status") == "blocked" and not task.get("blockers"):
        errors.append("active_task.blockers: blocked tasks need a concrete blocker")
    checks = task.get("checks")
    if not isinstance(checks, list):
        errors.append("active_task.checks: expected an array")
        return errors
    for index, row in enumerate(checks):
        label = f"active_task.checks[{index}]"
        if not isinstance(row, dict) or set(row) != {"command", "result", "kind", "evidence_ref"}:
            errors.append(f"{label}: expected command, result, kind and evidence_ref")
            continue
        if not nonempty(row["command"]):
            errors.append(f"{label}.command: expected a nonempty string")
        if row["result"] not in ("PASS", "FAIL", "NOT_VERIFIED"):
            errors.append(f"{label}.result: expected PASS, FAIL or NOT_VERIFIED")
        if row["kind"] not in ("FRESH_VALIDATION", "REUSED_UNCHANGED_DEPENDENCIES", "NOT_RUN"):
            errors.append(f"{label}.kind: unsupported validation provenance")
        if row["evidence_ref"] is not None and not relative(row["evidence_ref"]):
            errors.append(f"{label}.evidence_ref: expected a repository-relative file or null")
        if row["kind"] == "NOT_RUN" and row["result"] != "NOT_VERIFIED":
            errors.append(f"{label}: an unexecuted check cannot claim PASS or FAIL")
        if row["kind"] == "REUSED_UNCHANGED_DEPENDENCIES" and row["evidence_ref"] is None:
            errors.append(f"{label}: reuse requires an evidence reference")
    return errors


def read_text(root, path, errors):
    if not path.resolve().is_relative_to(root.resolve()):
        errors.append(f"{path.relative_to(root)}: reference escapes repository")
        return None
    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as error:
        errors.append(f"{path.relative_to(root)}: {error}")
        return None


def prose_lines(text):
    fence = None
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        marker = re.match(r"(`{3,}|~{3,})", stripped)
        if marker:
            if fence is None:
                fence = marker.group(1)
            elif stripped.startswith(fence[0] * len(fence)):
                fence = None
            continue
        if fence is None:
            yield number, line


def anchors(text):
    result, counts = set(re.findall(r'<a\s+(?:id|name)=["\']([^"\']+)["\']', text)), {}
    for _, line in prose_lines(text):
        heading = re.match(r"^ {0,3}#{1,6}\s+(.+?)\s*#*\s*$", line)
        if not heading:
            continue
        title = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", heading.group(1))
        title = re.sub(r"<[^>]+>", "", title).replace("`", "").replace("*", "")
        slug = re.sub(r"[^\w\- ]", "", title.lower()).replace(" ", "-")
        count = counts.get(slug, 0)
        result.add(slug + (f"-{count}" if count else ""))
        counts[slug] = count + 1
    return result


def check_links(root, path, content, errors):
    for number, line in prose_lines(content):
        for match in LINK.finditer(line):
            target = match.group(1).strip()
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1]
            try:
                parsed = urlsplit(target)
            except ValueError as error:
                errors.append(f"{path.relative_to(root)}:{number}: invalid link: {error}")
                continue
            if parsed.scheme in ("https", "http", "mailto") or parsed.netloc:
                continue
            label = f"{path.relative_to(root)}:{number}"
            if parsed.scheme or "\\" in parsed.path:
                errors.append(f"{label}: expected a repository-relative link: {target}")
                continue
            try:
                name = unquote(parsed.path)
                if any(character in name for character in "\x00\r\n"):
                    raise ValueError("control character in local path")
                destination = (path.parent / name).resolve() if name else path.resolve()
            except (OSError, ValueError) as error:
                errors.append(f"{label}: invalid local reference: {error}")
                continue
            if not destination.is_relative_to(root) or not destination.exists():
                errors.append(f"{label}: missing/unsafe local reference: {target}")
            elif parsed.fragment and destination.suffix.lower() == ".md":
                text = read_text(root, destination, errors)
                if text is not None and unquote(parsed.fragment) not in anchors(text):
                    errors.append(f"{label}: missing local anchor: {target}")


def check(root=ROOT):
    root = root.resolve()
    errors = []
    for name in (*REQUIRED_INPUTS, *(".agent/" + name for name in sorted(AGENT_FILES))):
        path = root / name
        if not path.resolve().is_relative_to(root) or not path.is_file():
            errors.append("missing/unsafe current development input: " + name)
    agent = root / ".agent"
    if agent.is_dir() and agent.resolve().is_relative_to(root):
        for path in sorted(agent.rglob("*")):
            if path.is_file() and path.relative_to(agent).as_posix() not in AGENT_FILES:
                errors.append("unexpected Agent file: " + path.relative_to(root).as_posix())
    content = read_text(root, root / STATE, errors)
    state = None
    if content is not None:
        try:
            state = json.loads(content, object_pairs_hook=unique_object)
            errors.extend(validate(state))
        except ValueError as error:
            errors.append(f"{STATE}: {error}")
    if isinstance(state, dict) and isinstance(state.get("active_task"), dict):
        task = state["active_task"]
        scopes = task.get("in_scope", [])
        checks = task.get("checks", [])
        references = [value for value in scopes if relative(value, glob=True)] if isinstance(scopes, list) else []
        if isinstance(checks, list):
            references += [row["evidence_ref"] for row in checks if isinstance(row, dict) and relative(row.get("evidence_ref"))]
        for name in references:
            if not (root / name).resolve().is_relative_to(root):
                errors.append("active_task: reference escapes repository: " + name)
    for name in ENTRY_DOCUMENTS:
        path = root / name
        content = read_text(root, path, errors)
        if content is not None:
            check_links(root, path, content, errors)
            for retired in RETIRED_NAMES:
                if re.search(r"(?<![\w-])" + re.escape(retired) + r"(?![\w-])", content):
                    errors.append(f"{name}: retired startup input: {retired}")
    consumers = [root / name for name in ("AGENTS.md", "README.md", ".gitattributes", "docs/development/building.md",
                                         "docs/development/repository-layout.md", "tests/README.md")]
    for area in ("tools", ".github"):
        consumers.extend(path for path in (root / area).rglob("*") if path.is_file()
                         and path.suffix in (".py", ".ps1", ".sh", ".yml", ".yaml", ".json", ".cmake"))
    retired_paths = [".agent/" + name for name in RETIRED_NAMES]
    retired_paths += [".agent/" + "decisions/", ".agent/" + "product-identity.json", "tools/agent/" + "check_product_identity.py"]
    for path in sorted(set(consumers)):
        if not path.exists():
            continue
        content = read_text(root, path, errors)
        if content is not None:
            for target in retired_paths:
                if target in content.replace("\\", "/"):
                    errors.append(f"{path.relative_to(root)}: retired current reference: {target}")
    return errors


def empty_state():
    return dict(schema="qualpf.agent_task_state.v1", baseline_ref=BASELINE, active_task=None)


def example_task():
    return dict(id="documentation-maintenance", goal="Update the current engineering guide",
                in_scope=["docs/**"], out_of_scope=["Product behavior and PFSM"], status="in_progress",
                base_commit="a" * 40, checkpoint_commit=None, completed=[], next_action="Inspect affected references",
                blockers=[], checks=[])


def self_test():
    class Controls(unittest.TestCase):
        def fixture(self, directory):
            root = Path(directory)
            for name in (*REQUIRED_INPUTS, *(".agent/" + name for name in AGENT_FILES)):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("", encoding="utf-8")
            (root / STATE).write_text(json.dumps(empty_state()), encoding="utf-8")
            (root / BASELINE).write_text(json.dumps(dict(milestone_a=dict(next_action="STOP", outcome="CPP_ARCHITECTURE_REFACTOR_COMPLETE"))), encoding="utf-8")
            (root / ".agent/README.md").write_text("# Agent context\n\n[Rules](POLICY.md#engineering-policy)\n", encoding="utf-8")
            (root / ".agent/POLICY.md").write_text("# Engineering policy\n", encoding="utf-8")
            return root

        def test_task_is_independent_of_acceptance(self):
            self.assertEqual(validate(empty_state()), [])
            state = dict(empty_state(), active_task=example_task())
            self.assertEqual(validate(state), [])
            for status in ("in_progress", "paused", "blocked"):
                task = dict(example_task(), status=status, blockers=["Missing local SDK"] if status == "blocked" else [])
                self.assertEqual(validate(dict(empty_state(), active_task=task)), [])
            with tempfile.TemporaryDirectory() as directory:
                root = self.fixture(directory)
                (root / STATE).write_text(json.dumps(state), encoding="utf-8")
                self.assertEqual(check(root), [])

        def test_shape_and_scope(self):
            for value in (None, [], True, "task"):
                self.assertTrue(validate(value))
            for field, value in (("schema", "unknown"), ("baseline_ref", "../outside.json"), ("active_task", [])):
                self.assertTrue(validate(dict(empty_state(), **{field: value})))
            for field in TASK_FIELDS:
                task = example_task()
                del task[field]
                self.assertTrue(validate(dict(empty_state(), active_task=task)))
            for field, value in (("status", "complete"), ("base_commit", "short"), ("checkpoint_commit", False),
                                 ("goal", " "), ("completed", [1]), ("checks", {}), ("in_scope", [])):
                task = dict(example_task(), **{field: value})
                self.assertTrue(validate(dict(empty_state(), active_task=task)))
            for unsafe in ("../outside", "/outside", "C:/outside", "C:outside", "src\\gui", "src/../../outside"):
                task = dict(example_task(), in_scope=[unsafe])
                self.assertTrue(validate(dict(empty_state(), active_task=task)))

        def test_validation_provenance(self):
            row = dict(command="python -B check.py", result="PASS", kind="FRESH_VALIDATION", evidence_ref="build/check.log")
            for kind in ("FRESH_VALIDATION", "REUSED_UNCHANGED_DEPENDENCIES"):
                task = dict(example_task(), checks=[dict(row, kind=kind)])
                self.assertEqual(validate(dict(empty_state(), active_task=task)), [])
            for changed in (dict(row, kind="NOT_RUN"), dict(row, result="SKIP"),
                            dict(row, evidence_ref="../outside.log"), dict(row, evidence_ref=4),
                            dict(row, kind="REUSED_UNCHANGED_DEPENDENCIES", evidence_ref=None), None):
                task = dict(example_task(), checks=[changed])
                self.assertTrue(validate(dict(empty_state(), active_task=task)))

        def test_missing_inputs_and_invalid_json(self):
            with tempfile.TemporaryDirectory() as directory:
                root = self.fixture(directory)
                for name in (*REQUIRED_INPUTS, *(".agent/" + name for name in AGENT_FILES)):
                    path = root / name
                    original = path.read_bytes()
                    path.unlink()
                    with self.subTest(name=name):
                        self.assertTrue(check(root))
                    path.write_bytes(original)
                for invalid in ("{", "[]", '{"schema": 1, "schema": 2}'):
                    (root / STATE).write_text(invalid, encoding="utf-8")
                    self.assertTrue(check(root))

        def test_navigation_and_retired_consumers(self):
            with tempfile.TemporaryDirectory() as directory:
                root = self.fixture(directory)
                readme = root / ".agent/README.md"
                content = readme.read_text()
                for target in ("missing.md", "POLICY.md#missing", "../../outside.md", "file:///outside.md", "POLICY.md%00"):
                    readme.write_text(content + f"[Broken]({target})\n", encoding="utf-8")
                    self.assertTrue(check(root))
                readme.write_text(content + "```md\n[Example](missing.md)\n```\n", encoding="utf-8")
                self.assertEqual(check(root), [])
                readme.write_text(content + "Read " + RETIRED_NAMES[0] + "\n", encoding="utf-8")
                self.assertTrue(check(root))
                readme.write_text(content, encoding="utf-8")
                (root / "tools/old.py").write_text('command = "tools/agent/' + 'check_product_identity.py"\n', encoding="utf-8")
                self.assertTrue(check(root))
                (root / "tools/old.py").unlink()
                (root / ".agent/history.md").write_text("Unexpected archive", encoding="utf-8")
                self.assertTrue(check(root))

    result = unittest.TextTestRunner(verbosity=0).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    return result.wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    if args.self_test and not self_test():
        return 1
    errors = check(args.root)
    for error in errors:
        print("FAIL:", error)
    if not errors:
        print("PASS: Agent rules, navigation and single-task handoff; baseline acceptance is checked separately")
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
