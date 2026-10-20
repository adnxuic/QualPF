# Restraints

Restraints narrow the database search by pre-filtering cards that do not match
specified chemical or crystallographic criteria.

Open **S&M → Restraints**. This shared entry is separated from the CSM and PFSM groups.

## Properties tab

| Field | Description |
|-------|-------------|
| **Chemical name** | Filter by compound name (partial match) |
| **Color** | Filter by crystal color |
| **Density (calc.)** | Filter by calculated density ± tolerance |
| **Density (meas.)** | Filter by measured density ± tolerance |

## Composition tab

| Field | Description |
|-------|-------------|
| **Elements** | Include/exclude chemical elements |
| **Mode** | `Contains` (AND/OR), `Only` (exact composition), `Just` (at least these) |
| **SM: CSM / PFSM** | Mutually exclusive search method, immediately left of Min species |
| **Min / Max species** | Inclusive number of distinct chemical elements; `any` disables that bound |

The method buttons keep a private selection while switching tabs. Opening the
dialog selects the current analysis method, or CSM before an analysis exists.
**Close** discards an unexecuted method choice. **Cancel All Restraints** clears
criteria and retains the method choice; choosing a method is not a restraint.

## Symmetry tab

| Field | Description |
|-------|-------------|
| **Crystal system** | Filter by crystal system (cubic, tetragonal, …) |
| **Space group** | Filter by space group symbol |
| **Cell parameters** | Filter by unit cell constants a, b, c, α, β, γ ± tolerance |

## Entries tab

Paste a list of card IDs (one per line) to restrict the search to those
specific entries.

## Subfiles tab

Filter by PDF-2 subfile codes (e.g., `I` = inorganic, `O` = organic).

## Buttons

| Button | Action |
|--------|--------|
| **Load cards** | Query the database with the current restraints (replaces results) |
| **Load & Merge** | Query and add results to the current list |
| **Search with Restraints** | Run the selected CSM or PFSM method within the restrained subset |

Search requires an effective restraint and a continuous spectrum. CSM prepares
missing experimental peaks automatically; PFSM uses the continuous imported
original without a peak-table prefilter. Switching an existing analysis asks to
end it; declining preserves the current analysis. Method selection and repeated
submission are disabled while a worker runs or the window closes.

Loading or merging independent cards is available for CSM. PFSM retains one
frozen candidate universe and refuses independent merges into its ranking.
