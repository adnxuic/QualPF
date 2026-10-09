# Phase 17 qualification review

Pending explicit user approval. These are semantic tiny-pattern normalization differences, not binary32 residual waivers.
The original comparisons remain failed. Phase 13–16 decisions do not apply here.
All normal LP/raw/group/position/order/rank fields pass the frozen tolerances. Only case 67 (`sub-epsilon-maximum`) differs.

| Case | Field | Legacy | GNU | MSVC | Independent 80-digit value |
|---|---|---|---|---|---|
| 67 | line.1.relative | 5.5425610829752493e-17 | 341.46507817532273 | 341.46507817532273 | 341.4650781753226244410740806684837947055031997162392823759160572882786835393895736 |
| 67 | line.2.relative | 1.6231709573590899e-16 | 1000 | 1000 | 1000.0 |

Legacy substitutes max=1 when its positive maximum is below binary32 epsilon; Core divides by the actual positive maximum.
This preserves global positive Fc² scale invariance, including tiny values, and gives a maximum of 1000.
Membership, representative hkl, d/2theta, raw intensity and normalized rank are unchanged. No production consumer is switched.
Acceptance must bind the exact case/field/Legacy value/C++ values/compiler hashes/proof/reference/source hashes; new or stale fields fail.

Independent proof: 91942 checks across both compilers, 0 failures.
