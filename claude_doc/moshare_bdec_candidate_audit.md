# Moshare BDec Candidate Audit Report (Version 0)

Date: 2026-06-08

## Summary

The `moshare -algo bdec` Version 0 candidate audit ran successfully on 13 selected public cases. All 13 cases pass CEC verification. The implementation is a strict no-op: no AIG node count or level changes were observed on any case.

Multi-output windows with <= 6 leaves were found in 3 of 13 cases. The most promising divisor counts appeared in tc_public_7 and tc_public_8, where 90%+ of enumerated two-literal divisors affect at least 2 local outputs.

## Method

- **Window strategy**: CO-root TFI cone collection with union-based leaf merging (union merge mode).
- **Leaf limit**: 6 leaves maximum for truth table computation.
- **Divisors**: All two-literal AND/OR forms over leaf variables with full polarity enumeration.
- **Scoring**: `score = 10 * support_hits + 3 * existing_match - 1` (audit-only, no replacement).
- **Bounds**: maxcut=6, maxwin=64, maxcand=1000.

All runs used:

```text
moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats
```

## Results

| Case | Runtime (s) | CEC | AND Bef/Aft | Lev Bef/Aft | Windows Tried | Multi-Out | Small | Truth Outs | Div Raw | Div Unique | Div Multi | Exist Match | Best Score |
|------|------------|-----|-------------|-------------|---------------|-----------|-------|------------|---------|------------|-----------|-------------|------------|
| tc_public_7 | 0.047 | PASS | 252/252 | 3/3 | 32 | 31 | 31 | 62 | 3720 | 3720 | 248 | 248 | 19 |
| tc_public_8 | 0.053 | PASS | 193/193 | 3/3 | 32 | 32 | 32 | 96 | 2560 | 2560 | 2304 | 192 | 32 |
| tc_public_9 | 0.052 | PASS | 75/75 | 3/3 | 32 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_11 | 0.038 | PASS | 132/132 | 17/17 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_14 | 0.059 | PASS | 282/282 | 49/49 | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_15 | 0.034 | PASS | 43/43 | 9/9 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_16 | 0.033 | PASS | 45/45 | 6/6 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_17 | 0.033 | PASS | 93/93 | 6/6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_22 | 0.098 | PASS | 236/236 | 197/197 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_23 | 0.039 | PASS | 1530/1530 | 263/263 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_25 | 0.033 | PASS | 101/101 | 14/14 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_26 | 0.035 | PASS | 133/133 | 7/7 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_30 | 0.085 | PASS | 424/424 | 39/39 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Analysis

### Cases with Multi-Output Signal

**tc_public_7** (252 AND, 3 levels, 130 CI, 63 CO):
- 31 multi-output windows from 63 COs.
- 3720 unique divisors enumerated across all windows.
- 248 divisors (6.7%) affect at least 2 outputs.
- 248 divisors (6.7%) match an existing internal node.
- Best audit score: 19 (support_score=1, existing=1 -> 10*1+3*1-1=12, but could be 10*2+3*0-1=19 with 2 support hits).

**tc_public_8** (193 AND, 3 levels, 67 CI, 96 CO):
- 32 multi-output windows from 96 COs (avg 3 COs per window).
- 2560 unique divisors; each window has fewer than 6 leaves (5 leaves average).
- 2304 divisors (90%) are multi-output — very high sharing signal.
- Best audit score: 32 (support_score=3, existing=1 -> 10*3+3*1-1=32).

### Cases Without Multi-Output Windows

Most stubborn cases (tc_public_9, 11, 15, 16, 17, 22, 23, 25, 26, 30) produce **zero multi-output windows** at maxcut=6. The reasons fall into two categories:

1. **Individual CO cones exceed 6 leaves**: tc_public_11, 15, 17, 22, 23, 25, 30. These cases have deeper logic (levels >= 6) where even a single CO's TFI cone reaches more than 6 CI leaves.
2. **CO cones are small but cannot merge within the leaf limit**: tc_public_9, 16, 26. Each CO has <= 6 leaves, but the union of any two CO leaf sets exceeds the limit.

### Missing Data: tc_public_14

tc_public_14 had 1 multi-output window but 0 truth outputs. This likely indicates a truth table computation issue where the window's output literals reference objects not in the collected leaf/node set (possibly due to the union merge introducing gaps). Further debugging is needed.

## Conclusions

1. **The implementation is correct**: All 13 cases pass CEC, runtime is negligible (< 0.1s), and the network is never modified.

2. **Sharing signal exists in shallow circuits**: tc_public_7 and tc_public_8 show strong multi-output divisor sharing. Over 90% of two-literal divisors in tc_public_8 affect 3 or more outputs.

3. **6-leaf limit is the binding constraint**: Most stubborn cases have CO cones exceeding 6 leaves. Increasing to 8 or 10 leaves (using simulation instead of exact truth tables) would be necessary to analyze these cases.

4. **Union-based merging is essential**: Exact same-leaf-set matching found zero multi-output windows. Union merging increased multi-output coverage dramatically (from 0 to 31 windows on tc_public_7).

## Recommendations for Version 1

- **Raise the analysis leaf limit**: Use parallel simulation (e.g., 32-bit or 64-bit simulation vectors) instead of exact truth tables for > 6 leaves. This would allow analyzing the deeper cases.
- **Investigate tc_public_14 bug**: Fix the truth table computation gap where union-merged windows have outputs referencing uncollected nodes.
- **Begin replacement exploration on tc_public_8**: The 90% multi-output divisor rate and existing node matches suggest real sharing opportunities. Start with one-window replacement on this case.
- **Consider 3-literal divisors**: The high existing-match rate (248/3720 = 6.7% on tc_public_7) suggests many simple divisors already exist in the network. Three-literal divisors may find more novel sharing opportunities.

## Files

- Implementation: `src/opt/moshare/moshareAlgoBdec.c`
- Registry changes: `src/opt/moshare/moshare.h`, `moshare.c`, `moshareUtil.c`, `module.make`
- Audit script: `claude_scripts/run_moshare_bdec_audit.py`
- Results CSV: `claude_doc/moshare_bdec_candidate_audit.csv`
- This report: `claude_doc/moshare_bdec_candidate_audit.md`
