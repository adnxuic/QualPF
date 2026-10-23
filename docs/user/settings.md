# Settings and workflow history

QualPF separates defaults for future projects from the parameters of the open
project. Check which editor you are using before changing an option.

## Defaults and project options

| Entry | What it changes |
| --- | --- |
| **Setting** on the menu bar | Saved defaults for new projects |
| A workflow's **Options** command | That workflow in the current project |
| A preprocessing editor | Its private preview, followed by the project on successful acceptance |

The Setting window has independent Preprocessing, CSM, PFSM and Peak Search
profiles. You can edit them without a loaded pattern. **Restore Page Defaults**
resets the current module's draft; **Save Defaults** validates the complete draft.
Changing pages retains unsaved input, and Cancel discards it. Profiles you did
not open retain their previous values.

A new project or replacement main-pattern import copies saved defaults. Opening
a QPF project restores that project's parameters instead. Importing a comparison
pattern does not replace the main project's settings.

Options dialogs retain their workflow-specific controls. Saving CSM or PFSM
options updates the project; running the menu command starts the calculation.
Preprocessing editors can preview changes before acceptance. Cancelling a draft
does not change the saved defaults or the committed scientific result.

## Preparation belongs to a workflow

CSM, PFSM and Peak Search resolve missing preparation using their own profiles.
Compatible manually prepared evidence may be shared, but an automatic result
from another workflow is not automatically considered manual preparation.

The PFSM defaults include an **Initial Peaks** page. Its saved SG controls are
independent of other profiles; automatic PFSM width preparation currently uses
geometric detection rather than that SG module. Changing those controls therefore
does not change automatic geometric FWHM.

Under PFSM **Profile Expansion**, template caching is enabled by default. It
uses extra preparation time and memory to reduce repeated profile evaluation;
final fits are checked by direct evaluation. Disabling it retains direct
evaluation throughout fitting. Existing saved choices remain effective.

## Inspect and undo

Open **View > Workflow**, select a step and inspect **Details** for its inputs,
parameters and dependencies. Failed or cancelled attempts are reported without
being presented as successful outputs.

**Undo Selected** rolls back the selected step and the results that depend on it.
Read the affected-step confirmation; cancelling it leaves the project unchanged.
Unrelated display state and current configuration are retained. Undo is confined
to the open session and has no redo. Reopening a project starts a new undo history.

Changes that would invalidate dependent scientific results require confirmation.
Wait for running work to finish or cancel it before editing analysis settings,
opening or saving a project, or attempting a rollback.
