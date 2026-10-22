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
`.agent/`. Historical reports belong in `history/`; never rewrite frozen evidence
to describe today's implementation.

Current prose uses English, UTF-8 and LF. Historical evidence retains its original
language and bytes. Record notable completed changes in [CHANGELOG.md](../CHANGELOG.md).

## User manual publication

From the repository root:

```sh
python -m pip install -r docs/requirements.txt
python -m mkdocs build --strict -f docs/mkdocs.yml
bash docs/make_pdf.sh
```

The website publishes only `user/`, with the existing page URLs. Developer guides
are read in the repository. The optional PDF requires Pandoc, XeLaTeX and DejaVu
fonts. Generated `site/` and `qualpf_manual.pdf` stay outside version control.
