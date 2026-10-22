# Local 1.0.5 release refresh

On 2026-10-09, the existing ignored `releases/1.0.5` directory was replaced
with current QualPF Release builds from source commit
`fa6a22400879aac9f0d278ee0afda641fb30d9d6`. Production source, scientific
references and assertions were unchanged. Existing untracked `docs/maintenance`
work was preserved.

| Artifact under `releases/1.0.5` | Build environment |
| --- | --- |
| `windows-x64-gcc/bin/qualpf.exe` | Windows x64 GCC 13.1, Qt 6.8.3 |
| `installers/windows-gnu/QualPF-1.0.5-win64.exe` | Same GCC build, QtIFW 4.8.1 |
| `installers/windows-msvc/QualPF-1.0.5-win64.exe` | Windows x64 MSVC 19.40, toolset 14.40.33807, Qt 6.8.3, QtIFW 4.8.1 |
| `installers/linux/qualpf-1.0.5-resolute_amd64.deb` | Ubuntu 26.04.1 x64, GCC 15.2.0, Qt 6.10.2; existing isolated WSL SDK |

Each configuration started with an empty build directory. Windows used the
current worktree; Linux used a copy of current tracked source bytes in a private
mount namespace. All 1,817 indexed build inputs were unchanged at assembly.
The product identity is now QualPF, with `qualpf` executable/resource names.

Fresh verification passed for all three builds: mandatory configured architecture
boundaries and their negative controls, eight installed CLI cases per platform,
and GUI version initialization plus actual example-pattern loading and exit.
Windows runtime checks removed Qt SDK/compiler directories from PATH and
confirmed the deployed Windows platform plugin loaded. Linux used xcb under
Xvfb and installed the DEB into a private root with dependency checks enabled.
All 169 frozen reference files passed length/hash verification. The relocated
GCC portable release also passed its eight CLI cases and both GUI checks.

Both Windows installers were generated successfully, and every deployed file
was compared by SHA-256 with the selected/forced IFW component payloads. An
initial verifier assumed all files belonged to the QualPF component; its failed
comparison is retained in
`build/release-refresh-20261009/logs/payload-comparison-attempt-01.json`.
Inspection showed that CPack places dependencies in the forced Unspecified
component. The corrected verifier checks both components without changing
packaging inputs. Interactive installer wizard execution and the full normal
regression suite were not run for this source-unchanged local rebuild.

`releases/1.0.5/build-manifest.json` records source identity, toolchains, checks
and per-file SHA-256 values. The portable subtree has its own manifest.
Build scripts, original logs, source-input hashes and package intermediates remain
under `build/release-refresh-20261009`. The entire previous release remains at
`build/release-refresh-20261009/previous-release-1.0.5`; every backup file and
every replacement artifact passed SHA-256 verification after the directory moves.
No remote publication occurred.
