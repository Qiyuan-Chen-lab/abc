# ABC Standard Reference Flow Report

This report is not an official contest baseline. The contest scores final
outputs directly by AIG node count, AIG level count, runtime, and peak
memory. This internal reference records what the standard ABC optimization
flow achieves, against which contest-specific multi-output sharing algorithms
are compared.

**Date**: 2026-05-27 13:39:08

**ABC binary**: `./abc`

## Standard Reference Flow

```
b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b
```

This flow was selected via competition-weighted evaluation (AND 50%, Level 20%,
Time 20%, Memory 10%) across 6 candidate flows. It adds one `resub`+cleanup
post-pass to the standard ABC optimization sequence, giving significant additional
AND reduction without level degradation.

---

## Per-Case Results: No-opt vs Standard Reference

| Case  | I/O     | Init AND | Init Lev | Opt AND | Opt Lev | Δ AND | Δ%     | T(s)  | CEC  |
| ---   | ---     | ---      | ---      | ---     | ---     | ---   | ---    | ---   | ---  |
| tc_01 | 10/8    | 207      | 13       | 148     | 10      | +59   | +28.5% | 0.056 | PASS |
| tc_02 | 6/5     | 34       | 6        | 19      | 4       | +15   | +44.1% | 0.048 | PASS |
| tc_03 | 10/8    | 208      | 13       | 134     | 10      | +74   | +35.6% | 0.056 | PASS |
| tc_04 | 10/8    | 145      | 12       | 108     | 10      | +37   | +25.5% | 0.058 | PASS |
| tc_05 | 66/64   | 128      | 2        | 65      | 2       | +63   | +49.2% | 0.053 | PASS |
| tc_06 | 67/32   | 256      | 5        | 162     | 4       | +94   | +36.7% | 0.057 | PASS |
| tc_07 | 130/63  | 252      | 3        | 252     | 3       | +0    | +0.0%  | 0.051 | PASS |
| tc_08 | 67/96   | 193      | 3        | 192     | 4       | +1    | +0.5%  | 0.054 | PASS |
| tc_09 | 67/32   | 224      | 5        | 224     | 5       | +0    | +0.0%  | 0.060 | PASS |
| tc_10 | 36/32   | 192      | 5        | 161     | 4       | +31   | +16.1% | 0.054 | PASS |
| tc_11 | 134/96  | 1064     | 6        | 328     | 7       | +736  | +69.2% | 0.068 | PASS |
| tc_12 | 406/532 | 1482     | 15       | 1366    | 14      | +116  | +7.8%  | 0.091 | PASS |
| tc_13 | 335/20  | 1284     | 17       | 1045    | 13      | +239  | +18.6% | 0.079 | PASS |
| tc_14 | 928/77  | 6388     | 29       | 4097    | 26      | +2291 | +35.9% | 0.251 | PASS |
| tc_15 | 31/32   | 931      | 12       | 925     | 10      | +6    | +0.6%  | 0.080 | PASS |
| tc_16 | 8/5     | 45       | 6        | 45      | 6       | +0    | +0.0%  | 0.047 | PASS |
| tc_17 | 16/6    | 93       | 6        | 93      | 6       | +0    | +0.0%  | 0.053 | PASS |
| tc_18 | 64/8    | 525      | 12       | 507     | 14      | +18   | +3.4%  | 0.078 | PASS |
| tc_19 | 512/11  | 5478     | 18       | 5364    | 26      | +114  | +2.1%  | 0.478 | PASS |
| tc_20 | 532/11  | 5742     | 20       | 5625    | 30      | +117  | +2.0%  | 0.522 | PASS |
| tc_21 | 534/11  | 5763     | 20       | 5646    | 34      | +117  | +2.0%  | 0.545 | PASS |
| tc_22 | 1024/12 | 11367    | 20       | 11178   | 30      | +189  | +1.7%  | 0.997 | PASS |
| tc_23 | 520/2   | 1530     | 263      | 1530    | 16      | +0    | +0.0%  | 0.102 | PASS |
| tc_24 | 104/9   | 223      | 15       | 215     | 8       | +8    | +3.6%  | 0.054 | PASS |
| tc_25 | 15/9    | 101      | 14       | 27      | 4       | +74   | +73.3% | 0.052 | PASS |
| tc_26 | 33/32   | 1064     | 32       | 1064    | 10      | +0    | +0.0%  | 0.089 | PASS |
| tc_27 | 189/16  | 576      | 20       | 376     | 7       | +200  | +34.7% | 0.056 | PASS |
| tc_28 | 51/16   | 576      | 20       | 520     | 7       | +56   | +9.7%  | 0.061 | PASS |
| tc_29 | 30/16   | 121      | 8        | 108     | 5       | +13   | +10.7% | 0.049 | PASS |
| tc_30 | 12/12   | 14139    | 1036     | 7460    | 21      | +6679 | +47.2% | 0.603 | PASS |

---

## Summary

- **Total cases**: 30
- **CEC PASS**: 30 / FAIL: 0
- **Total init AND**: 60331
- **Total opt AND**: 48984
- **Total reduction**: 11347 (+18.8%)
- **Total runtime**: 4.90s

Optimized BLIF files saved to: `/home/24qyc/Desktop/abc/claude_run/` as `<case>_v1z_opt.blif`

## Competition Metrics Summary

Metrics: AND nodes (50%), Levels (20%), Time (20%), Memory (10%)

| Case | AND | Level | Time(s) | Mem(KB) |
|------|-----|-------|---------|---------|
| tc_01 | 148 | 10 | 0.056 | 15004 |
| tc_02 | 19 | 4 | 0.048 | 14972 |
| tc_03 | 134 | 10 | 0.056 | 15208 |
| tc_04 | 108 | 10 | 0.058 | 14936 |
| tc_05 | 65 | 2 | 0.053 | 15052 |
| tc_06 | 162 | 4 | 0.057 | 15184 |
| tc_07 | 252 | 3 | 0.051 | 15628 |
| tc_08 | 192 | 4 | 0.054 | 15248 |
| tc_09 | 224 | 5 | 0.06 | 15216 |
| tc_10 | 161 | 4 | 0.054 | 15360 |
| tc_11 | 328 | 7 | 0.068 | 15672 |
| tc_12 | 1366 | 14 | 0.091 | 16152 |
| tc_13 | 1045 | 13 | 0.079 | 15488 |
| tc_14 | 4097 | 26 | 0.251 | 18736 |
| tc_15 | 925 | 10 | 0.08 | 15336 |
| tc_16 | 45 | 6 | 0.047 | 14660 |
| tc_17 | 93 | 6 | 0.053 | 14668 |
| tc_18 | 507 | 14 | 0.078 | 14872 |
| tc_19 | 5364 | 26 | 0.478 | 18196 |
| tc_20 | 5625 | 30 | 0.522 | 18352 |
| tc_21 | 5646 | 34 | 0.545 | 18336 |
| tc_22 | 11178 | 30 | 0.997 | 23076 |
| tc_23 | 1530 | 16 | 0.102 | 16172 |
| tc_24 | 215 | 8 | 0.054 | 15028 |
| tc_25 | 27 | 4 | 0.052 | 14600 |
| tc_26 | 1064 | 10 | 0.089 | 15696 |
| tc_27 | 376 | 7 | 0.056 | 14880 |
| tc_28 | 520 | 7 | 0.061 | 15076 |
| tc_29 | 108 | 5 | 0.049 | 14784 |
| tc_30 | 7460 | 21 | 0.603 | 21032 |

## Cases With <2% Improvement Over No-opt

These cases see little benefit from standard ABC optimization and are the
primary targets for contest-specific multi-output sharing algorithms.

| Case | I/O | Init AND | Opt AND | Δ% |
|------|-----|----------|---------|-----|
| tc_07 | 130/63 | 252 | 252 | +0.0% |
| tc_08 | 67/96 | 193 | 192 | +0.5% |
| tc_09 | 67/32 | 224 | 224 | +0.0% |
| tc_15 | 31/32 | 931 | 925 | +0.6% |
| tc_16 | 8/5 | 45 | 45 | +0.0% |
| tc_17 | 16/6 | 93 | 93 | +0.0% |
| tc_22 | 1024/12 | 11367 | 11178 | +1.7% |
| tc_23 | 520/2 | 1530 | 1530 | +0.0% |
| tc_26 | 33/32 | 1064 | 1064 | +0.0% |

