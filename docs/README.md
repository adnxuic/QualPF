# QualPF documentation

Choose the guide for your work:

- [User manual](user/index.md): installation, databases, analysis workflows,
  settings, project files and interpretation of results.
- [Developer guide](developer/README.md): building and testing, architecture,
  numerical contracts, persistence and contributing changes.

These guides describe the current program. The [history index](../history/README.md)
locates earlier acceptance reports, qualifications and original evidence.
[AGENTS.md](../AGENTS.md) and [.agent](../.agent/README.md) define how an AI agent
works on the repository; they are not product manuals.

## Maintaining documentation

Keep user actions and observable behavior in `user/`, and implementation contracts
and reproducible developer instructions in `developer/`. Link to the owning guide
instead of copying its rules. Keep AI startup, task navigation and handoff in
`.agent/`. Keep necessary acceptance records and recovery indexes in `history/`; store full
logs and raw historical evidence in the verified independent archive. Never rewrite
frozen evidence to describe today's implementation.

Current prose uses English, UTF-8 and LF. Historical evidence retains its original
language and bytes. Record notable completed changes in [CHANGELOG.md](../CHANGELOG.md).

## User manual publication

From the repository root:

```sh
python -m pip install -r docs/requirements.txt
python -m mkdocs build --strict -f docs/mkdocs.yml
python -B tools/documentation/build_manual.py
```

The website publishes only `user/`, with the existing page URLs. Developer guides
are read in the repository. The optional PDF requires Pandoc, XeLaTeX and DejaVu
fonts. Generated `site/` and `qualpf_manual.pdf` stay outside version control.
The shell wrapper `bash docs/make_pdf.sh` calls the same portable PDF builder.
Both preserve navigation order, resolve page/section links to internal PDF
destinations and produce `build/documentation/qualpf_manual-provenance.json`.
That proof binds all user sources, the builder, publication configuration and
distribution metadata. A change to any bound input requires regeneration.

## Manual source and artwork

The 23 October 2026 manual replaces the earlier prose with independent guidance
derived from current implementation and technical contracts. Keep existing page
URLs, necessary attribution and scientific qualifications when editing it.
The tutorial uses the reader's own measurement, not delivered historical data.
The rights inventory records the old manual's recoverable Git identities;
do not copy an old manual or its assets into the current publication tree.
The FWHM/PFSM bibliography incorporates the numbered references from the local
`references/IFwhmEstimator/` and `references/PFSM/` notes. Shared entries are
listed once, with chapter links and explicit limits on cross-domain evidence.
The notes themselves stay outside publication and source delivery; current
product contracts remain the authority for implemented behavior.

`user/images/analysis-route.svg` is the editable original workflow illustration.
Its PNG is used in both HTML and PDF. With Node.js and sharp available, regenerate
or verify it from the repository root:

```sh
node tools/documentation/render_diagram.cjs
node tools/documentation/render_diagram.cjs --check
```

Current exports use sharp 0.35.5. The logo and favicon are derived from the
existing [QualPF branding master](../src/gui/resources/branding/README.md).
Publication has no inherited analytics account or remote mathematics script.
