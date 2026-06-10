# Moshare BDec Candidate Audit Report

Date: 2026-06-10

## Summary

The `moshare -algo bdec` implementation was reviewed, fixed, and retested after
the initial candidate-audit report. The original Version 0 implementation had
useful window/truth/divisor scaffolding, but it was effectively a no-op and had
measurement/correctness bugs that made the first report unreliable for judging
the Boolean-decomposition method.

After fixes, `bdec` now performs a conservative exact replacement attempt for
two-literal Boolean-decomposition candidates. On all 30 public cases, CEC passed.
AND-count improvement was observed on 3 cases.

Final public retest:

- CEC: 30/30 PASS
- Multi-output windows found in 8/30 cases
- AND-count improvements found in 3/30 cases

## Method

- **Window strategy**: CO-root TFI cone collection with union-based leaf merging.
- **Leaf limit**: exact truth-table computation is limited to `leaves <= 6`.
- **Divisors**: two-literal AND/OR forms over leaf variables with all input
  polarities.
- **Exact check**: a candidate divisor is replacement-eligible only if local
  outputs satisfy `f(x) = h(d(x_i, x_j), remaining leaves)`.
- **Replacement scope**: conservative CO-driver replacement only.
- **Acceptance rule**: rebuild, run `Gia_ManCleanup`, and accept only if AND
  count strictly decreases and level growth is within `-maxgrow`.
- **Bounds**: `maxcut=6`, `maxwin=64`, `maxcand=1000`.

Command:

```text
moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats
```

## Fixes Applied

- Fixed `Mosh_BdecObjToLeafIndex()` so ABC vector iteration uses separate entry
  and index variables.
- Added exact bound-set decomposability checking for two-literal divisors.
- Added conservative CO-driver replacement with actual profit/level checks.
- Fixed truth-to-Kit conversion for `nVars < 5` by expanding compact local truth
  tables into Kit's 32-bit truth format.
- Fixed divisor raw/unique accounting.
- Added stats for truth failures, decomposable divisors, replacement tries,
  accepted replacements, and profit/level rejects.
- Completed `Mosh_ResClear()` for the expanded result struct.
- Fixed the audit script to parse colorized ABC `print_stats` output with regex
  instead of fragile split indexes.

## Improved Cases

| Case | AND Before | AND After | Level Before | Level After | Notes |
| --- | ---: | ---: | ---: | ---: | --- |
| `tc_public_2` | 34 | 23 | 6 | 5 | Replacement accepted; strongest improvement. |
| `tc_public_5` | 128 | 125 | 2 | 2 | Replacement accepted with no level growth. |
| `tc_public_10` | 192 | 191 | 5 | 5 | Replacement accepted with no level growth. |

## Full Public Results

| Case | CEC | AND Bef/Aft | Lev Bef/Aft | Multi-Out | Truth Outs | Div Unique | Div Multi | Exist Match | Div Decomp | Repl Tried | Repl Accepted | Reject Profit | Best Score |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| tc_public_1 | PASS | 207/207 | 13/13 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_2 | PASS | 34/23 | 6/5 | 1 | 5 | 120 | 104 | 26 | 6 | 1 | 1 | 0 | 84 |
| tc_public_3 | PASS | 208/208 | 13/13 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_4 | PASS | 145/145 | 12/12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_5 | PASS | 128/125 | 2/2 | 1 | 4 | 120 | 8 | 8 | 50 | 1 | 1 | 0 | 71 |
| tc_public_6 | PASS | 256/256 | 5/5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_7 | PASS | 252/252 | 3/3 | 31 | 62 | 3720 | 248 | 248 | 0 | 0 | 0 | 0 | 19 |
| tc_public_8 | PASS | 193/193 | 3/3 | 32 | 96 | 2560 | 2304 | 192 | 0 | 0 | 0 | 0 | 32 |
| tc_public_9 | PASS | 224/224 | 5/5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_10 | PASS | 192/191 | 5/5 | 1 | 2 | 120 | 48 | 8 | 2 | 1 | 1 | 0 | 35 |
| tc_public_11 | PASS | 1064/1064 | 6/6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_12 | PASS | 1482/1482 | 15/15 | 40 | 373 | 4720 | 1720 | 312 | 1424 | 1424 | 0 | 1424 | 444 |
| tc_public_13 | PASS | 1284/1284 | 17/17 | 2 | 4 | 160 | 0 | 16 | 0 | 0 | 0 | 0 | 17 |
| tc_public_14 | PASS | 6388/6388 | 29/29 | 1 | 2 | 48 | 0 | 2 | 6 | 6 | 0 | 6 | 28 |
| tc_public_15 | PASS | 931/931 | 12/12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_16 | PASS | 45/45 | 6/6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_17 | PASS | 93/93 | 6/6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_18 | PASS | 525/525 | 12/12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_19 | PASS | 5478/5478 | 18/18 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_20 | PASS | 5742/5742 | 20/20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_21 | PASS | 5763/5763 | 20/20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_22 | PASS | 11367/11367 | 20/20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_23 | PASS | 1530/1530 | 263/263 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_24 | PASS | 223/223 | 15/15 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_25 | PASS | 101/101 | 14/14 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_26 | PASS | 1064/1064 | 32/32 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_27 | PASS | 576/576 | 20/20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_28 | PASS | 576/576 | 20/20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_29 | PASS | 121/121 | 8/8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tc_public_30 | PASS | 14139/14139 | 1036/1036 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Important Negative Cases

- `tc_public_7`: many support-level candidates, but `div_decomp = 0`. The
  current support-overlap signal does not translate into exact two-literal
  bound-set decomposition.
- `tc_public_8`: high support-level sharing (`div_multi = 2304`), but
  `div_decomp = 0`.
- `tc_public_12`: `div_decomp = 1424`, but all 1424 replacement attempts were
  rejected by actual profit.
- `tc_public_14`: the previous truth-output failure is fixed
  (`truth_outputs = 2`, `truth_failed = 0`), but all 6 exact candidates had no
  actual profit.

## Current Judgment

The first issue was partly code quality: the leaf-index bug, truth-format bug,
and fragile report parsing were real.

After fixing them and adding a conservative exact replacement path, `bdec` shows
real improvement on some small public cases. However, the current simplified
two-literal CO-driver replacement is weak on the stubborn cases. In particular,
`tc_public_7` and `tc_public_8` have support-sharing signals but no exact
two-literal bound-set decomposition under the current window/leaves formulation.

The remaining limitation is mostly method/search strength, not just a simple
implementation bug. Promising next steps are richer divisors, larger or different
windows, and replacement inside internal roots rather than only CO drivers.

## Files

- Implementation: `src/opt/moshare/moshareAlgoBdec.c`
- Registry changes: `src/opt/moshare/moshare.h`, `moshare.c`, `moshareUtil.c`,
  `module.make`
- Audit script: `claude_scripts/run_moshare_bdec_audit.py`
- Results CSV: `claude_doc/moshare_bdec_candidate_audit.csv`
- This report: `claude_doc/moshare_bdec_candidate_audit.md`
