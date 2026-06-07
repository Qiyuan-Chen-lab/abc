# Moshare Bdiff Algorithm Benchmark Report

**Date**: 2026-05-27 16:51:13
**ABC binary**: `/home/24qyc/Desktop/abc/abc`

## Tested Flows

### noopt — Strash-only baseline

```
read_blif <case>/input.blif; strash; ps
```

### v1z — ABC standard reference flow

```
read_blif <case>/input.blif; strash; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b; ps
```

### bdiff — Moshare bdiff algorithm only (no standard optimization)

```
read_blif <case>/input.blif; strash; moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 2000 -maxbdd 32 -maxgrow 3 -stats; ps
```

### v1z+bdiff — ABC standard reference flow followed by moshare bdiff

```
read_blif <case>/input.blif; strash; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b; moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 2000 -maxbdd 32 -maxgrow 3 -stats; ps
```

---

## Per-Case Results: noopt vs v1z vs bdiff

| Case  | I/O     | noopt AND | v1z AND | bdiff AND | bdiff Acc | v1z+bdiff AND | v1z+bdiff Acc | CEC  |
| ---   | ---     | ---       | ---     | ---       | ---       | ---           | ---           | ---  |
| tc_01 | 10/8    | 207       | 148     | 207       | 0         | 148           | 0             | PASS |
| tc_02 | 6/5     | 34        | 19      | 34        | 0         | 19            | 0             | PASS |
| tc_03 | 10/8    | 208       | 134     | 208       | 0         | 134           | 0             | PASS |
| tc_04 | 10/8    | 145       | 108     | 145       | 0         | 108           | 0             | PASS |
| tc_05 | 66/64   | 128       | 65      | 128       | 0         | 65            | 0             | PASS |
| tc_06 | 67/32   | 256       | 162     | 256       | 0         | 162           | 0             | PASS |
| tc_07 | 130/63  | 252       | 252     | 252       | 0         | 252           | 0             | PASS |
| tc_08 | 67/96   | 193       | 192     | 193       | 0         | 192           | 0             | PASS |
| tc_09 | 67/32   | 224       | 224     | 224       | 0         | 224           | 0             | PASS |
| tc_10 | 36/32   | 192       | 161     | 192       | 0         | 161           | 0             | PASS |
| tc_11 | 134/96  | 1064      | 328     | 1064      | 0         | 328           | 0             | PASS |
| tc_12 | 406/532 | 1482      | 1366    | 1482      | 0         | 1366          | 0             | PASS |
| tc_13 | 335/20  | 1284      | 1045    | 1284      | 0         | 1045          | 0             | PASS |
| tc_14 | 928/77  | 6388      | 4097    | 6388      | 0         | 4097          | 0             | PASS |
| tc_15 | 31/32   | 931       | 925     | 931       | 0         | 925           | 0             | PASS |
| tc_16 | 8/5     | 45        | 45      | 45        | 0         | 45            | 0             | PASS |
| tc_17 | 16/6    | 93        | 93      | 93        | 0         | 93            | 0             | PASS |
| tc_18 | 64/8    | 525       | 507     | 525       | 0         | 507           | 0             | PASS |
| tc_19 | 512/11  | 5478      | 5364    | 5478      | 0         | 5364          | 0             | PASS |
| tc_20 | 532/11  | 5742      | 5625    | 5742      | 0         | 5625          | 0             | PASS |
| tc_21 | 534/11  | 5763      | 5646    | 5763      | 0         | 5646          | 0             | PASS |
| tc_22 | 1024/12 | 11367     | 11178   | 11367     | 0         | 11178         | 0             | PASS |
| tc_23 | 520/2   | 1530      | 1530    | 1530      | 0         | 1530          | 0             | PASS |
| tc_24 | 104/9   | 223       | 215     | 223       | 0         | 215           | 0             | PASS |
| tc_25 | 15/9    | 101       | 27      | 101       | 0         | 27            | 0             | PASS |
| tc_26 | 33/32   | 1064      | 1064    | 1064      | 0         | 1064          | 0             | PASS |
| tc_27 | 189/16  | 576       | 376     | 576       | 0         | 376           | 0             | PASS |
| tc_28 | 51/16   | 576       | 520     | 576       | 0         | 520           | 0             | PASS |
| tc_29 | 30/16   | 121       | 108     | 121       | 0         | 108           | 0             | PASS |
| tc_30 | 12/12   | 14139     | 7460    | 14139     | 0         | 7460          | 0             | PASS |

---

## Summary

- **Total cases**: 30
- **Total noopt AND**: 60331
- **Total v1z AND**: 48984
- **Total bdiff AND**: 60331
- **Total v1z+bdiff AND**: 48984
- **v1z reduction vs noopt**: 11347 (+18.8%)
- **bdiff reduction vs noopt**: 0 (+0.0%)
- **v1z+bdiff reduction vs noopt**: 11347 (+18.8%)

## Bdiff Detailed Statistics (medium params)

| Case | Init AND | Candidates | w/Diff | Accepted | Rej_Profit | Rej_Level | Rej_BDD | Result AND |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| tc_01 | 207 | 2000 | 1136 | 0 | 1963 | 0 | 14 | 207 |
| tc_02 | 34 | 561 | 388 | 0 | 547 | 0 | 1 | 34 |
| tc_03 | 208 | 2000 | 1041 | 0 | 1981 | 0 | 4 | 208 |
| tc_04 | 145 | 2000 | 1044 | 0 | 1957 | 0 | 28 | 145 |
| tc_05 | 128 | 190 | 100 | 0 | 190 | 0 | 0 | 128 |
| tc_06 | 256 | 496 | 268 | 0 | 481 | 0 | 0 | 256 |
| tc_07 | 252 | 190 | 105 | 0 | 181 | 0 | 0 | 252 |
| tc_08 | 193 | 171 | 41 | 0 | 164 | 0 | 0 | 193 |
| tc_09 | 224 | 378 | 178 | 0 | 363 | 0 | 0 | 224 |
| tc_10 | 192 | 1128 | 784 | 0 | 1113 | 0 | 0 | 192 |
| tc_11 | 1064 | 435 | 282 | 0 | 422 | 0 | 0 | 1064 |
| tc_12 | 1482 | 45 | 13 | 0 | 45 | 0 | 0 | 1482 |
| tc_13 | 1284 | 66 | 26 | 0 | 59 | 0 | 0 | 1284 |
| tc_14 | 6388 | 1 | 1 | 0 | 1 | 0 | 0 | 6388 |
| tc_15 | 931 | 0 | 0 | 0 | 0 | 0 | 0 | 931 |
| tc_16 | 45 | 990 | 345 | 0 | 951 | 0 | 0 | 45 |
| tc_17 | 93 | 210 | 77 | 0 | 191 | 0 | 0 | 93 |
| tc_18 | 525 | 0 | 0 | 0 | 0 | 0 | 0 | 525 |
| tc_19 | 5478 | 0 | 0 | 0 | 0 | 0 | 0 | 5478 |
| tc_20 | 5742 | 0 | 0 | 0 | 0 | 0 | 0 | 5742 |
| tc_21 | 5763 | 0 | 0 | 0 | 0 | 0 | 0 | 5763 |
| tc_22 | 11367 | 0 | 0 | 0 | 0 | 0 | 0 | 11367 |
| tc_23 | 1530 | 0 | 0 | 0 | 0 | 0 | 0 | 1530 |
| tc_24 | 223 | 0 | 0 | 0 | 0 | 0 | 0 | 223 |
| tc_25 | 101 | 1176 | 440 | 0 | 1147 | 0 | 0 | 101 |
| tc_26 | 1064 | 1953 | 757 | 0 | 1912 | 0 | 0 | 1064 |
| tc_27 | 576 | 2000 | 532 | 0 | 1951 | 0 | 0 | 576 |
| tc_28 | 576 | 0 | 0 | 0 | 0 | 0 | 0 | 576 |
| tc_29 | 121 | 325 | 165 | 0 | 314 | 0 | 0 | 121 |
| tc_30 | 14139 | 0 | 0 | 0 | 0 | 0 | 0 | 14139 |

## Parameter Sweep (cases 1-5)

| Case | Params | Init AND | Candidates | Accepted | Result AND | Time(s) |
| --- | --- | --- | --- | --- | --- | --- |
| tc_01 | default | 207 | 0 | 0 | 207 | 0.047 |
| tc_02 | default | 34 | 100 | 0 | 34 | 0.053 |
| tc_03 | default | 208 | 0 | 0 | 208 | 0.045 |
| tc_04 | default | 145 | 0 | 0 | 145 | 0.045 |
| tc_05 | default | 128 | 66 | 0 | 128 | 0.032 |
| tc_01 | medium | 207 | 2000 | 0 | 207 | 0.056 |
| tc_02 | medium | 34 | 561 | 0 | 34 | 0.051 |
| tc_03 | medium | 208 | 2000 | 0 | 208 | 0.055 |
| tc_04 | medium | 145 | 2000 | 0 | 145 | 0.053 |
| tc_05 | medium | 128 | 190 | 0 | 128 | 0.033 |
| tc_01 | large | 207 | 5000 | 0 | 207 | 0.067 |
| tc_02 | large | 34 | 561 | 0 | 34 | 0.054 |
| tc_03 | large | 208 | 5000 | 0 | 208 | 0.061 |
| tc_04 | large | 145 | 5000 | 0 | 145 | 0.056 |
| tc_05 | large | 128 | 378 | 0 | 128 | 0.034 |

## Root Cause Analysis

The bdiff algorithm searches for AIG node pairs (f, g) where an existing node h satisfies
`func(h) == func(f) xor func(g)`, then replaces f by `h xor g`. The gain estimation uses:

```
gain_est = MFFC(f) - 1
```

This assumes replacing f with a single XOR gate removes the entire MFFC of f. However,
the actual gain is computed via `Gia_ManCleanup` after rebuild, which only removes nodes
that become **unreferenced**. When h shares logic with f's MFFC (common in multi-output
networks), those shared nodes are not removed.

### Concrete Example (tc_02)

Verbose output from the smoke test:

```
moshare (algo bdiff): found candidate f=23(co=2) g=17(co=2) h=22 synth=0 cross_co=0 gain_est=10
moshare (algo bdiff): rebuild f=23 g=17 h=22 old=34 new=36 gain=-2 mffc_est=11 level_grow=0 (rejected: no gain)
```

- MFFC(f=23) = 11 nodes → gain_est = 10 (replacing 11 nodes with 1 XOR)
- Actual rebuild: 34 → 36 nodes (**+2 worse**)
- Reason: node h=22 and its cone share internal nodes with MFFC(f=23), so cleanup
  cannot remove all 11 nodes. The XOR gate adds 1 node but fewer than 11 are removed.

### Rejection Breakdown (all 30 cases)

The dominant rejection reason is **profit**: across all tested parameter configurations,
the vast majority (98%+) of candidates are rejected because the actual rebuild shows
gain <= 0. Only a handful are rejected for BDD size (maxbdd limit).

## Key Findings

1. The bdiff algorithm finds **zero profitable replacements** across all 30 test cases
   with all three parameter configurations (default, medium, large).
2. The root cause is a **profit estimation mismatch**: the gain formula `MFFC(f) - 1`
   overestimates actual savings because MFFC nodes shared with h or g cannot be removed
   by cleanup.
3. The previous benchmark results (`moshare_bdiff_results.csv`, dated earlier) showed
   positive gains on 13 cases, indicating the code previously used a different acceptance
   criterion (likely trusting the estimate without actual rebuild verification).
4. The v1z standard reference flow alone achieves 18.8% AND reduction. Adding bdiff
   either before or after v1z produces **zero additional improvement**.
5. All generated BLIF files pass CEC verification (30/30).

## Recommendations

1. **Profit estimation fix**: The gain estimation should account for shared-node overlap
   between MFFC(f) and the cone of h. A quick sharing check before rebuild would reduce
   false positives.
2. **Iterative application**: The current algorithm applies only a single replacement.
   Even with a fixed profit model, multi-output sharing typically requires iterative
   application for meaningful cumulative gains.
3. **Alternative sharing strategies**: Consider methods that identify shared subgraphs
   directly (e.g., cut-based enumeration with simulation preprocessing) rather than
   relying on BDD-based Boolean difference, which is expensive and limited by BDD size.

