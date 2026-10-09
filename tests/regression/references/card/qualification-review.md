# Phase 18 qualification review

Pending explicit user approval of the exact 109 case/field records below. Raw comparisons remain failed; no earlier approval applies.

## Meaning and consequences

* 60 fields (cases 33, 57–60): the frozen Phase 16 exact-image convention retains 24/192 distinct images; Legacy 0.3 A proximity merging counts 12/96. Cell mass, density, linear absorption double and RIR halves (within ordinary floating error). Z doubles; normalized formula stays the same. This is a semantic difference, not a rounding waiver. These are synthetic fixtures.
* 5 fields (case 93): deterministic mixed-site ordering chooses Z=2 for both permutations. Legacy chooses Z=1 for the reversed input. Formula mass and numeric formula amounts halve and the display becomes `Ge0.2 Zn0.8`; cell mass, density, absorption and RIR are unchanged.
* 44 fields: binary32 logarithmic interpolation weights differ from the 80-digit source-decimal calculation. The final MAC, absorption and RIR already pass the frozen tolerances. These records cover neutral/ionic labels of the affected elements individually.
* All 12 actual full-export observations (six real structures, anomalous off/on), line threshold/cap/identity/ranking comparisons and remaining fields pass without qualification. Equal-intensity native order remains recorded; Core uses the frozen deterministic angular-slot tie break.

## Exact scope

| Case / name | Field | Legacy | GNU | MSVC | Independent value (rounded here) | Type |
|---|---|---|---|---|---|---|
| 33 / phase17-hexagonal-r0-s0 | absorption.14.weighted_mac | 3035.230712890625 | 6070.462950261661 | 6070.462950261661 | 6070.46295026165908 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | absorption.mu | 33.256263732910156 | 66.51252920841581 | 66.51252920841581 | 66.5125292084157927 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | composition.cell_mass | 337.0320129394531 | 674.064 | 674.064 | 674.064 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | composition.density | 3.692775249481201 | 7.385548986244795 | 7.385548986244795 | 7.38554898624479489 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | composition.z | 12 | 24 | 24 | 24 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | rir.denominator | 84818.3984375 | 169636.8282778101 | 169636.8282778101 | 169636.828277810133 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | rir.rho_volume_ratio | 3.054515838623047 | 1.527257631819857 | 1.527257631819857 | 1.52725763181985668 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | rir.rir | 69.5367660522461 | 34.768378492293095 | 34.768378492293095 | 34.7683784922930899 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | site.1.amount | 12.0 | 24.0 | 24.0 | 24.0 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | site.1.multiplicity | 12 | 24.0 | 24.0 | 24 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | species.14.cell_amount | 12.0 | 24.0 | 24.0 | 24.0 | exact_symmetry_vs_legacy_proximity |
| 33 / phase17-hexagonal-r0-s0 | species.14.cell_mass | 337.0320129394531 | 674.064 | 674.064 | 674.064 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | absorption.14.weighted_mac | 20982.89453125 | 41965.79727821752 | 41965.79727821752 | 41965.7972782175522 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | absorption.mu | 278.74407958984375 | 557.4883491446803 | 557.4883491446802 | 557.488349144680687 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | composition.cell_mass | 337.0320129394531 | 674.064 | 674.064 | 674.064 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | composition.density | 4.477251052856445 | 8.95450226017536 | 8.954502260175358 | 8.95450226017536 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | composition.z | 12 | 24 | 24 | 24 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | rir.denominator | 69957.046875 | 139914.09781523998 | 139914.09781523998 | 139914.09781524 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | rir.rho_volume_ratio | 3.7034029960632324 | 1.85170147019152 | 1.85170147019152 | 1.85170147019151968 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | rir.rir | 0.00010864811338251457 | 5.4324055523295424e-05 | 5.4324055523295424e-05 | 0.0000543240555232954137 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | site.1.amount | 12.0 | 24.0 | 24.0 | 24.0 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | site.1.multiplicity | 96 | 192.0 | 192.0 | 192 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | species.14.cell_amount | 12.0 | 24.0 | 24.0 | 24.0 | exact_symmetry_vs_legacy_proximity |
| 57 / g225-occ0.125 | species.14.cell_mass | 337.0320129394531 | 674.064 | 674.064 | 674.064 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | absorption.14.weighted_mac | 62948.6796875 | 125897.39183465258 | 125897.39183465258 | 125897.391834652657 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | absorption.mu | 836.2323608398438 | 1672.4650474340413 | 1672.465047434041 | 1672.46504743404206 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | composition.cell_mass | 1011.0960693359375 | 2022.192 | 2022.192 | 2022.192 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | composition.density | 13.431754112243652 | 26.863506780526084 | 26.86350678052608 | 26.86350678052608 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | composition.z | 36 | 72 | 72 | 72 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | rir.denominator | 209871.15625 | 419742.29344572 | 419742.29344572 | 419742.29344572 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | rir.rho_volume_ratio | 1.234467625617981 | 0.6172338233971733 | 0.6172338233971733 | 0.617233823397173228 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | rir.rir | 3.621603536885232e-05 | 1.8108018507765138e-05 | 1.8108018507765138e-05 | 0.0000181080185077651379 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | site.1.amount | 36.0 | 72.0 | 72.0 | 72.0 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | site.1.multiplicity | 96 | 192.0 | 192.0 | 192 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | species.14.cell_amount | 36.0 | 72.0 | 72.0 | 72.0 | exact_symmetry_vs_legacy_proximity |
| 58 / g225-occ0.375 | species.14.cell_mass | 1011.0960083007812 | 2022.192 | 2022.192 | 2022.192 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | absorption.14.weighted_mac | 111964.7109375 | 223929.49427656873 | 223929.49427656873 | 223929.494276568859 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | absorption.mu | 1487.378662109375 | 2974.7578310360145 | 2974.7578310360145 | 2974.75783103601614 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | composition.cell_mass | 1798.40283203125 | 3596.8055040000004 | 3596.8055040000004 | 3596.805504 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | composition.density | 23.89061164855957 | 47.78122406029573 | 47.78122406029573 | 47.781224060295721 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | composition.z | 64 | 128 | 128 | 128 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | rir.denominator | 373290.8125 | 746581.6259421206 | 746581.6259421208 | 746581.62594212064 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | rir.rho_volume_ratio | 0.6940410137176514 | 0.3470205154032084 | 0.3470205154032083 | 0.347020515403208336 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | rir.rir | 2.0361339920782484e-05 | 1.0180670075580101e-05 | 1.0180670075580098e-05 | 0.0000101806700755801 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | site.1.amount | 64.03199768066406 | 128.06400000000002 | 128.06400000000002 | 128.064 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | site.1.multiplicity | 96 | 192.0 | 192.0 | 192 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | species.14.cell_amount | 64.03199768066406 | 128.06400000000002 | 128.06400000000002 | 128.064 | exact_symmetry_vs_legacy_proximity |
| 59 / g225-occ0.667 | species.14.cell_mass | 1798.4027099609375 | 3596.8055040000004 | 3596.8055040000004 | 3596.805504 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | absorption.14.weighted_mac | 167863.15625 | 335726.37822574016 | 335726.37822574016 | 335726.378225740418 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | absorption.mu | 2229.95263671875 | 4459.906793157443 | 4459.906793157442 | 4459.90679315744549 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | composition.cell_mass | 2696.256103515625 | 5392.512 | 5392.512 | 5392.512 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | composition.density | 35.81800842285156 | 71.63601808140288 | 71.63601808140287 | 71.63601808140288 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | composition.z | 96 | 192 | 192 | 192 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | rir.denominator | 559656.375 | 1119312.7825219198 | 1119312.7825219198 | 1119312.78252192 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | rir.rho_volume_ratio | 0.46292537450790405 | 0.23146268377394 | 0.23146268377394 | 0.23146268377393996 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | rir.rir | 1.358101417281432e-05 | 6.790506940411928e-06 | 6.790506940411928e-06 | 6.79050694041192671e-6 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | site.1.amount | 96.0 | 192.0 | 192.0 | 192.0 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | site.1.multiplicity | 96 | 192.0 | 192.0 | 192 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | species.14.cell_amount | 96.0 | 192.0 | 192.0 | 192.0 | exact_symmetry_vs_legacy_proximity |
| 60 / g225-occ1 | species.14.cell_mass | 2696.256103515625 | 5392.512 | 5392.512 | 5392.512 | exact_symmetry_vs_legacy_proximity |
| 93 / z-order-reversed | composition.formula_mass | 133.66799926757812 | 66.834 | 66.834 | 66.834 | canonical_formula_unit_order |
| 93 / z-order-reversed | composition.z | 1 | 2 | 2 | 2 | canonical_formula_unit_order |
| 93 / z-order-reversed | formula.display | Ge0.4 Zn1.6 | Ge0.2 Zn0.8 | Ge0.2 Zn0.8 | Ge0.2 Zn0.8 | canonical_formula_unit_order |
| 93 / z-order-reversed | species.30.formula_amount | 1.600000023841858 | 0.8 | 0.8 | 0.8 | canonical_formula_unit_order |
| 93 / z-order-reversed | species.32.formula_amount | 0.4000000059604645 | 0.2 | 0.2 | 0.2 | canonical_formula_unit_order |
| 181 / registry-Eu | absorption.63.log_fraction | 0.08464165031909943 | 0.08462356779636648 | 0.08462356779636648 | 0.0846235677963437055 | binary32_log_interpolation |
| 185 / registry-Ho | absorption.67.log_fraction | 0.5357643961906433 | 0.5357529041210363 | 0.5357529041210363 | 0.535752904120996152 | binary32_log_interpolation |
| 193 / registry-Re | absorption.75.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 194 / registry-Os | absorption.76.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 195 / registry-Ir | absorption.77.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 196 / registry-Pt | absorption.78.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 197 / registry-Au | absorption.79.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 198 / registry-Hg | absorption.80.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 199 / registry-Tl | absorption.81.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 200 / registry-Pb | absorption.82.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 201 / registry-Bi | absorption.83.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 202 / registry-Po | absorption.84.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 203 / registry-At | absorption.85.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 204 / registry-Rn | absorption.86.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 205 / registry-Fr | absorption.87.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 206 / registry-Ra | absorption.88.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 207 / registry-Ac | absorption.89.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 360 / registry-Eu2+ | absorption.242.log_fraction | 0.08464165031909943 | 0.08462356779636648 | 0.08462356779636648 | 0.0846235677963437055 | binary32_log_interpolation |
| 361 / registry-Eu3+ | absorption.243.log_fraction | 0.08464165031909943 | 0.08462356779636648 | 0.08462356779636648 | 0.0846235677963437055 | binary32_log_interpolation |
| 365 / registry-Ho3+ | absorption.247.log_fraction | 0.5357643961906433 | 0.5357529041210363 | 0.5357529041210363 | 0.535752904120996152 | binary32_log_interpolation |
| 375 / registry-Re4+ | absorption.257.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 376 / registry-Os4+ | absorption.258.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 377 / registry-Ir3+ | absorption.259.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 378 / registry-Ir4+ | absorption.260.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 379 / registry-Pt2+ | absorption.261.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 380 / registry-Pt4+ | absorption.262.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 381 / registry-Au1+ | absorption.263.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 382 / registry-Au3+ | absorption.264.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 383 / registry-Hg1+ | absorption.265.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 384 / registry-Hg2+ | absorption.266.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 385 / registry-Tl1+ | absorption.267.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 386 / registry-Tl3+ | absorption.268.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 387 / registry-Pb2+ | absorption.269.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 388 / registry-Pb4+ | absorption.270.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 389 / registry-Bi3+ | absorption.271.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 390 / registry-Bi5+ | absorption.272.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 391 / registry-Po2+ | absorption.273.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 392 / registry-Po4+ | absorption.274.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 393 / registry-At1- | absorption.275.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 394 / registry-At1+ | absorption.276.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 395 / registry-Rn2+ | absorption.277.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 396 / registry-Fr1+ | absorption.278.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 397 / registry-Ra2+ | absorption.279.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |
| 398 / registry-Ac3+ | absorption.280.log_fraction | 0.10294158011674881 | 0.10291505762307204 | 0.10291505762307204 | 0.102915057623060046 | binary32_log_interpolation |

The JSON proof retains 80-digit values, exact orbit coordinates, close-pair witnesses and per-compiler errors. This table is a display only.
Approval must bind the exact records, proof, source/reference bytes, comparison code and compiler executables. Missing approval, stale evidence and new fields fail closed.
Independent proof: 218 checks, 0 failures. Zero production consumers switch.
