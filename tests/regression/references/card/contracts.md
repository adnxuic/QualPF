# Phase 18 contracts — frozen before production implementation

## Audited sequence

`exportqualx.F90::intensity_file_for_database` generates X-ray Cu K-alpha1
reflections at 1–150 degrees, assigns special-position fractions and default
ADPs, calculates Fc, LP and transitive overlap groups. Each group's intensity
is stored in its first reflection slot; every other member slot is zero.
Then it computes density, linear absorption, volume, RIR, Z and formula.
`import_data_structure.f90` copies the **first 500 reflection slots**, obtains
their maximum, sorts them by intensity, filters and normalizes the surviving
rows. RIR uses the maximum over **all** slots, before truncation.

This phase accepts already calculated physical lines; it does not perform the
CIF/default-ADP steps or change their Phase 13–17 contracts. Zero product
consumers switch. CIF/DB and continuous Profile remain Legacy-owned.

## Composition and formula

Legacy keeps XEN table-pointer species identity: Fe2+ and Fe3+ are separate
entries with separate masses. Formula formatting hides charge but **does not
merge** the entries (so repeated visible `Fe` tokens are possible). Numerical
composition must retain species, element Z, charge, occupancy and site
multiplicity. Mass is never obtained by reparsing that lossy string.

Core uses the already checked Phase 16 expansion: duplicate images within a
site are removed modulo one at 1e-10 per component; separate ASU entries,
including mixed occupancy, are not merged. Cell amount = sum(unique image
count * chemical occupancy). Native `ocry` instead stores count/nsym in REAL
after a 0.3 Angstrom proximity test. Both observations stay in the oracle.

Legacy Z is a display normalization heuristic, not independently supplied
chemical formula units. `get_atom_site` greedily groups sites less than 0.01
apart in raw fractional Euclidean coordinates when the first occupancy is
less than 1. It assigns the group's summed occupancy to that first species,
multiplies by `ocry*nsym`, rounds positive amounts within 0.1 of an integer,
then takes their GCD (default 1). Formula amounts are the true per-species cell
amounts divided by that Z. Mass/density do not depend on this heuristic.

The Core deterministic extension canonically orders sites by their least
expanded fractional position then species identity and occupancy before this
grouping. Thus translated/symmetry-equivalent ASU descriptions and atom input
permutations give identical Z. Any observed Legacy order-dependent Z/formula
discrepancy must remain a raw failed field, with a case-specific review.

Formatting follows the actual `hill_order`: C first, H second (even without
C), then full species labels in lexical order. Zero amounts are omitted.
Amounts within 0.05 of a positive integer use that integer; 1 is hidden.
Others use two decimal places with trailing zeros removed. Numeric amounts
are retained without formatting-rounding. Exact ties have a specified species
identity order; native heap-sort observations remain recorded.

## Registry, density and absorption

Only XEN species label/Z/atomic mass and NIST energy/mass-attenuation columns
are added. XEN and NIST source decimals are interpreted as binary64, matching
Phase 16's resource convention. A deterministic generator pins original bytes
and checks generated output. No table initialization, Qt or Fortran in Core.

Cell mass is in daltons (numerically g/mol of cells), volume in Angstrom^3.
Formula mass = cell mass/Z. Density in g/cm^3 = 1.660543780*cell_mass/volume,
using the retained decimal conversion constant; no replacement by modern NA.

NIST provides mass attenuation in cm^2/g for Z=1..92. Energy in keV is
`((1e7)*6.62607015e-34*2.998e8/1.602176634e-19)/wavelength_A`.
Interpolate linearly in base-10 log energy and log attenuation. Closed table
endpoints return their value; outside range is an error. Preserve Legacy's
zero-endpoint rule if present. Mixture MAC = sum(cell species mass * MAC) /
cell mass. Linear absorption in cm^-1 = density * mixture MAC. Ion entries
use their element Z's attenuation table and their own XEN mass.

Legacy can silently use zero MAC for unsupported Z, and ignores the vector
lookup's error in `linear_abs_coeff`; these are historical observations only.
Core rejects missing positive-contribution masses/MAC, unsupported energy and
invalid interpolation. Occupancy-zero entries contribute neither mass nor MAC.

## RIR

The fixed compatibility calibration is Cu K-alpha1 X-ray only. A complete card
rejects other radiation/wavelength; standalone attenuation permits any positive
in-range X-ray wavelength. No calibration is invented for neutron/electron or
custom wavelength. Both anomalous flag values are allowed: the input raw
intensities already contain that choice, as in the original export helper.

`rho_volume_ratio = 259079.140625/(volume*volume*density)`;
`intensity_ratio = max_all_merged_raw/340862.156`;
`RIR = rho_volume_ratio*intensity_ratio`.
Both are original unsuffixed REAL literal constants; the Core uses their exact
binary32 literal values widened to binary64, as Phase 16 does for source-code
literal tables. Absorption is an output alongside RIR, **not a RIR factor**.
No integer/decimal RIR rounding occurs in this Core (the `.int` file printer's
three decimals and DB representation are presentation outside the boundary).
All intermediates are retained. Zero, nonfinite or overflowing RIR is an error.

## Line selection

Reconstruct the angular reflection slots from the complete line membership,
ordered by (2theta, stable source identity). The representative receives the
whole group's raw intensity and the other slots receive zero. Keep the first
500 slots, including zero slots, then compute their maximum. If maximum is
not greater than binary32 epsilon (2^-23), the card is empty and fails.
Threshold is `(2^-23 * maximum)/10`, inclusive. Sort eligible representatives
by descending raw intensity, breaking exact ties by ascending angular slot
identity. This explicit deterministic tie convention replaces native indexx
quicksort/reversal permutations; differential compares native tie-block
membership and records actual native rank separately.

Card intensity is `1000*(raw/maximum_within_first_500_slots)`. Preserve physical
normalized intensity separately. A stronger line beyond slot 500 changes RIR
and physical normalization but cannot change card normalization/retention.
Members of a retained overlap line remain complete, including those beyond
the slot cap. No maximum of 500 *merged* lines and no global top-500 ranking.

## Ownership and errors

The card is an immutable independent value with copied identity metadata,
cell/group/radiation description, composition/formula, mass, density,
attenuation/RIR intermediates, physical lines and selection provenance. The
card validates member geometry against the structure and declared radiation.
PowderLineSet has no structure owner token; geometric compatibility is checked,
and no claim of proven original Fc2 provenance is made. No DB/Qt object exists.

Structured errors cover empty/invalid composition, delegated invalid occupancy,
unsupported chemistry, invalid formula/cell mass/volume, nonfinite density,
unsupported radiation/wavelength/attenuation, invalid RIR denominator or zero
RIR, malformed line members/geometry/intensities, empty selection and nonfinite
card fields. No Legacy NaN, sentinel or stale value becomes a successful value.

## Oracle and frozen comparison policy

Capture uses original Fortran helpers and source-extracted unmodified RIR and
ABI-selection statements. Real structures additionally call the whole original
export routine. Capture stores complete inputs/raw output plus actual REAL
observations (including inputs after native conversion). Replay can isolate
Phase 18 from previously qualified upstream Fc2/geometry by consuming these
observations; full C++ pipelines are separately required in properties.

Preimplementation tolerances: mass/amount/density absolute 1e-7, relative 3e-6;
attenuation and RIR absolute 1e-7, relative 1e-5; normalized intensities absolute
1e-4, relative 8e-6; integers/species/membership/threshold decisions exact.
Native REAL representation versus binary64 nextafter boundaries is observed
explicitly. Core binary64 boundary properties use independent exact arithmetic.
No golden or tolerance may be changed after the corpus is sealed. Any further
numerical or Z/formula failures need independent proof and explicit per-field
acceptance; all previous approvals remain strictly scoped to their old fields.
