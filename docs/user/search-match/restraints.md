# Search restraints

Use **S&M > Restraints** to restrict a search using information about the sample.
Choose criteria you can justify independently of the desired result: an overly
narrow restriction can remove the correct reference before it is evaluated.

## Available criteria

| Tab | Information used to narrow cards |
| --- | --- |
| Properties | Chemical name, color and calculated or measured density |
| Composition | Included or excluded elements, composition mode and species count |
| Symmetry | Crystal system, space group and cell parameters with tolerances |
| Entries | Explicit card identifiers |
| Subfiles | Applicable database subfile codes |

On Composition, the **SM: CSM / PFSM** selector chooses the search method.
Minimum and maximum species are inclusive bounds on distinct elements;
`any` disables the corresponding bound. Choosing a method is not itself a
restraint. The method selection survives tab changes within the dialog.

## Load cards or run a search

**Load cards** replaces the current card list with the query result;
**Load & Merge** adds queried cards. These independent card-loading operations
are available for CSM. PFSM uses a frozen candidate universe and does not merge
unrelated cards into an existing ranking.

**Search with Restraints** runs the selected method and requires an effective
restriction plus a continuous measurement. CSM can prepare missing peaks;
PFSM works from continuous source evidence without a classical peak prefilter.
Changing the method of an existing analysis asks to end it first. Declining
preserves that analysis.

**Cancel All Restraints** clears the criteria but retains the dialog's method
choice. **Close** abandons a method choice that has not been executed. Search
submission and method switching are disabled while work is running or the
window is closing.
