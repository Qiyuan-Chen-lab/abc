# Boolean Decomposition Paper Fit Analysis for the Multi-Output Sharing Contest

## Objective

Evaluate whether the method in `papers/MinerU_markdown_Boolean_Decomposition_for_AIG_Optimization_2063585507255525376.md` is suitable for this ABC-based multi-output sharing contest, and define a conservative first experiment that can be implemented through the existing `moshare` framework.

The recommended first experiment is not a full reproduction of the paper. It is a statistics-only candidate audit:

```text
moshare -algo bdec
```

Version 0 should build bounded multi-output windows, compute small truth tables, enumerate two-literal divisors, and report whether stubborn public cases contain promising multi-output divisor candidates. It should not replace logic.

## Contest Metric Targeted

The full paper method targets AIG node count reduction by introducing shared two-literal divisors inside multi-output local regions. This is aligned with the contest scoring priority:

- Primary: reduce AIG AND node count.
- Secondary: control AIG level growth.
- Tertiary: keep runtime and memory bounded.

Version 0 does not directly optimize the score because it performs no replacement. Its metric contribution is diagnostic: it should tell us whether the public cases have enough small-support, multi-output divisor structure to justify a real `bdec` transform.

## Paper Method Summary

The paper starts from the observation that AIG rewriting and refactoring are powerful but biased by the current AIG structure and often limited by single-output local optimization. It proposes local Boolean decomposition over multi-output AIG regions.

Main ideas:

- Extract multi-output local regions using KL-cuts.
- Treat the outputs of each region as a multi-output Boolean function over the same leaf set.
- Enumerate two-literal divisors such as a two-input AND/OR-style expression over two literals.
- Use Boolean division with satisfiability don't-cares to decide whether introducing a divisor reduces the factored-form literal cost of the region outputs.
- Replace the local region only when the decomposed implementation is smaller.
- Restart cut enumeration after a successful replacement.

The paper reports additional AIG node reduction over strong ABC results, but also shows nontrivial runtime and level-growth risk.

## Contest Fit

Fit: high for the contest objective, medium-high implementation risk.

Why it is a better fit than the current `bdiff` Version A:

- It explicitly optimizes multi-output windows instead of one target node at a time.
- The shared object is a small two-literal divisor, so the implementation cost can be bounded and predictable.
- Candidate discovery can be done with truth tables for very small windows before implementing any risky replacement.
- The method directly addresses cases where single-output local rewriting misses cross-output sharing.

Why the full paper should not be reproduced first:

- Full KL-cut enumeration and recursive Boolean decomposition are larger than needed for a first contest experiment.
- Boolean division with two-level minimization and don't-care projection is implementation-heavy.
- The paper does not primarily optimize delay; reported results can increase AIG levels.
- A replacement implementation would need careful GIA rebuild, level checks, cleanup, and CEC validation.

## Recommended First Experiment: `moshare -algo bdec`

Implement a stats-only `bdec` algorithm variant inside `src/opt/moshare/`.

Version 0 should do only:

1. Build bounded multi-output windows.
2. Compute truth tables with `leaves <= 6`.
3. Enumerate two-literal divisors.
4. Collect candidate statistics.
5. Leave the network unchanged.

This answers the most important question before implementing Boolean division or replacement:

```text
Do the stubborn public cases contain many small multi-output windows where the same two-literal divisor is relevant to multiple outputs?
```

## True Multi-Output Sharing Target

This task targets true multi-output sharing if the window construction and candidate scoring require more than one local output.

Version 0 should reject or separately classify single-output windows. The most useful candidates are divisors that appear useful for at least two window outputs or for outputs that feed different CO cones.

Required reporting should distinguish:

- Total windows.
- Multi-output windows.
- Windows with `leaves <= 6`.
- Candidate divisors seen in at least two window outputs.
- Candidate divisors whose support variables appear in at least two output truth functions.
- Candidate divisors passing simple polarity/support filters for multiple outputs.

## Simplified Window Strategy

The paper uses KL-cuts. Version 0 can use a simpler approximation.

Recommended first implementation:

- Iterate over candidate root nodes or CO drivers in topological order.
- Build a small TFI window from a root or root group.
- Choose leaves by stopping at CIs, fanout boundaries, or when `nMaxCutSize` leaves is reached.
- Add multiple outputs to the same window when internal nodes in the window also drive logic outside the window or when nearby CO roots share enough leaves.
- Require all selected outputs to be expressible over the same leaf set.
- Reject windows with more than `nMaxCutSize` leaves.
- Reject windows with more than `nMaxWindowSize` internal nodes.

This is not a full KL-cut implementation, but it preserves the key property needed for the audit: a small shared leaf set and multiple local outputs.

Later versions can replace this with real K-cut/KL-cut enumeration if the candidate audit is promising.

## Truth Table Plan

Use exact truth tables only for small windows.

Initial bound:

```text
nMaxCutSize = 6
```

With six leaves, every output truth table fits in one 64-bit word. This keeps Version 0 simple, fast, and deterministic.

For each accepted window:

- Assign each leaf a local variable index from 0 to `nLeaves - 1`.
- Compute truth tables for all internal nodes in topological order.
- Preserve complemented edge semantics when composing AND truth tables.
- Store each selected output truth table.
- Compute actual support masks for each output truth table.

If a window has constant or single-literal outputs only, count it but skip divisor evaluation unless useful for debugging.

## Two-Literal Divisor Enumeration

Initial divisor set:

```text
z = lit_i AND lit_j
z = lit_i OR  lit_j
```

where each `lit_i` may be positive or negative. OR can be represented as an inverted AND in AIG cost accounting, but should still be reported separately because it may match different output polarity patterns.

For `n <= 6` leaves, the maximum raw divisor count is small:

```text
C(n, 2) * 4 polarities * 2 operators
```

Recommended filters for Version 0:

- Skip divisors whose two variables do not both appear in any output support.
- Track whether both variables appear together in at least two output supports.
- Deduplicate divisors with identical truth tables.
- Prefer divisors relevant to two or more outputs.
- Keep an optional per-window `nMaxCandidates` cap.

Do not implement recursive divisor selection in Version 0.

## Candidate Statistics

Because Version 0 does not run Boolean division, the statistics should avoid claiming real node gain. It should report proxies that help decide whether Version 1 is worth implementing.

Per window:

- Number of leaves.
- Number of internal nodes.
- Number of local outputs.
- Number of output truth tables with nontrivial support.
- Number of raw divisors.
- Number of unique divisor truth tables.
- Number of divisors relevant to at least two outputs.
- Best divisor score.

Per divisor:

- Leaf variable pair.
- Literal polarities.
- Operator kind.
- Number of local outputs whose support contains both variables.
- Number of local outputs for which the divisor truth table is support-correlated with the output truth table.
- Whether the divisor truth table already exists as an internal node in the window.
- Approximate AIG cost of materializing the divisor, normally one AND node.

Suggested score for audit ranking:

```text
score = 10 * multi_output_hits
      +  3 * existing_internal_match
      +  2 * support_overlap_hits
      -      divisor_cost
```

This score is only for ranking and reporting. It must not be used to apply a transform in Version 0.

## Profitability Model for Later Versions

Version 0 has no profitability acceptance because it is a no-op.

For a later replacement version, a candidate should be accepted only if:

- The replacement reduces global AIG AND count after cleanup.
- Level growth is within `nMaxLevelGrowth`.
- CI/CO ordering is unchanged.
- The modified network passes `cec`.
- Runtime stays bounded.

A future real model should include:

```text
gain = removed_window_nodes
     + reused_existing_divisor_nodes
     - new_divisor_cost
     - rebuilt_output_logic_cost
```

The model must be verified by actually rebuilding and cleaning the GIA before accepting a change. The `bdiff` experiment showed that estimated gains can be misleading when sharing overlaps are ignored.

## No-Op Fallback Behavior

Version 0 always returns the original `Gia_Man_t` unchanged with `fChanged = 0`.

Required fallback cases:

- No current network: report an ABC command error.
- Non-strashed network: ask the user to run `strash` first, following existing `moshare` behavior.
- No valid multi-output windows: print zero-window stats and return unchanged.
- All windows exceed bounds: print skipped-window stats and return unchanged.
- Truth table computation exceeds bounds or hits unsupported structure: skip that window and return unchanged.

## Candidate Bounds

Recommended Version 0 defaults:

- `nMaxCutSize = 6`
- `nMaxWindowSize = 64`
- `nMaxCandidates = 1000`
- `nMaxLevelGrowth = 0`, reserved for later replacement versions
- `RandomSeed = 0`, reserved; Version 0 should be deterministic without randomization
- `-stats` should print aggregate stats
- `-v` should print top candidate windows and divisors

Potential new parameters:

- `nMaxOutputs`: maximum local outputs per window, default 8.
- `nTopDivisors`: number of verbose divisors to print per run, default 20.
- `fAuditOnly`: optional, but Version 0 can simply be audit-only by definition.

Avoid adding many flags until the first implementation shows useful signal.

## Affected Source Directories and Likely Files

Primary integration files:

- `src/opt/moshare/moshare.h`
- `src/opt/moshare/moshare.c`
- `src/opt/moshare/moshareUtil.c`
- `src/opt/moshare/module.make`
- `src/opt/moshare/moshareAlgoBdec.c`

Possible later helper files:

- `src/opt/moshare/moshareTruth.c`
- `src/opt/moshare/moshareWindow.c`

Avoid modifying:

- Existing ABC rewrite/refactor/resub commands.
- Public benchmarks under `tc_public/`.
- Existing `bdiff` behavior, except for shared moshare registry changes.

## Relevant ABC Data Structures and APIs

Likely relevant data structures:

- `Gia_Man_t`, `Gia_Obj_t` for AIG traversal and object/literal handling.
- `Vec_Int_t`, `Vec_Ptr_t`, `Vec_Wec_t` for windows, leaves, outputs, and candidate storage.
- ABC truth-table utilities under `src/misc/util/utilTruth.h` and `src/bool/kit/`.

Implementation details to inspect before coding:

- Current `moshare` parameter/result structs.
- Current `moshareAlgoBdiff.c` for command integration, stats style, and no-op fallback.
- GIA fanin/literal access patterns.
- Existing small truth-table helpers for 6-input functions.
- Existing cut/window code only as reference; do not pull in a broad dependency until needed.

Key correctness concerns:

- Do not confuse GIA object IDs with literals.
- Preserve complemented fanins when computing truth tables.
- Use deterministic local leaf ordering.
- Reset any temporary object marks or values.
- Free all vectors and temporary arrays.

## Step-by-Step Implementation Plan

- [ ] Read this document and `claude_doc/moshare_integration_guide.md`.
- [ ] Inspect the current `moshare` files and `moshareAlgoBdiff.c`.
- [ ] Add `MOSH_ALGO_BDEC` and register the name `bdec`.
- [ ] Create `src/opt/moshare/moshareAlgoBdec.c`.
- [ ] Implement a stats-only entry point returning the input GIA unchanged.
- [ ] Implement bounded multi-output window collection.
- [ ] Implement deterministic local leaf ordering.
- [ ] Implement 64-bit truth table computation for `leaves <= 6`.
- [ ] Enumerate two-literal AND/OR divisors with literal polarities.
- [ ] Deduplicate equivalent divisor truth tables per window.
- [ ] Compute aggregate and verbose candidate statistics.
- [ ] Ensure `moshare -algo none` and `moshare -algo bdiff` still list and run.
- [ ] Build ABC.
- [ ] Run smoke tests on small public cases.
- [ ] Run the candidate audit on stubborn public cases and save a report under `claude_doc/`.

## Validation Goals and Acceptance Criteria

Version 0 acceptance:

- ABC builds successfully.
- `moshare -algo list` includes `bdec`.
- `moshare -algo bdec -stats` runs from ABC batch mode.
- The command leaves the network unchanged.
- `print_stats` before and after `bdec` is unchanged.
- Written BLIF output passes `cec`.
- Runtime is bounded on all public cases by window and candidate limits.
- `-stats` reports useful window and candidate counts.
- `-v` reports enough top candidate detail to decide whether Version 1 is worth planning.

QoR acceptance does not apply to Version 0 because it is no-op.

Decision criteria for continuing to Version 1:

- At least one stubborn public case has nontrivial multi-output windows with `leaves <= 6`.
- Several candidate divisors are relevant to two or more local outputs.
- Some candidate divisors already exist as internal window nodes, or have very low materialization cost.
- Candidate density is high enough that Boolean division or a lighter replacement heuristic is worth implementing.

## Benchmark Cases to Run

Start with stubborn or low-improvement public cases:

- `tc_public_7`
- `tc_public_8`
- `tc_public_9`
- `tc_public_15`
- `tc_public_16`
- `tc_public_17`
- `tc_public_22`
- `tc_public_23`
- `tc_public_26`

Also include at least one case where ABC standard flow improves strongly, as a sanity comparison:

- `tc_public_11`
- `tc_public_14`
- `tc_public_25`
- `tc_public_30`

Example commands:

```bash
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_7/input.blif; strash; print_stats; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats -v; print_stats"
./abc -c "read_blif tc_public/tc_public_7/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats; write_blif claude_run/moshare_bdec_tc07.blif"
./abc -c "cec tc_public/tc_public_7/input.blif claude_run/moshare_bdec_tc07.blif"
```

Durable reports should go under `claude_doc/`, for example:

```text
claude_doc/moshare_bdec_candidate_audit.md
claude_doc/moshare_bdec_candidate_audit.csv
```

## Correctness Risks

- Incorrect truth table computation due to complemented literal handling.
- Non-deterministic leaf ordering causing unstable reports.
- Incorrect window boundary causing outputs not to be functions of the selected leaves.
- Accidentally modifying the GIA even though Version 0 is intended to be no-op.
- Misreporting divisor usefulness as real profitability.

## Runtime and Memory Risks

- Window construction may become expensive if it scans too much fanout for every root.
- Candidate enumeration can grow with `O(leaves^2 * outputs)`, though `leaves <= 6` keeps Version 0 small.
- Recomputing truth tables from scratch for many overlapping windows may be redundant.
- Verbose output can become too large on big cases; cap printed candidates.

## Open Questions

- Should Version 0 approximate KL-cuts from TFI windows, or should it use existing cut enumeration immediately?
- Should two-literal divisors include XOR in addition to AND/OR, or should XOR stay with the `bdiff` family?
- Should divisor relevance be based only on support overlap, or should Version 0 add a small truth-table cofactor test?
- Should `bdec` eventually replace local windows directly in GIA, or generate a local network and rely on ABC cleanup?
- Should a later version use ABC's existing `Kit_Dsd` or `Bdc` decomposition utilities instead of implementing Boolean division from scratch?

## Claude Code Handoff Instructions

Implement only the Version 0 candidate audit unless the user explicitly requests replacement.

Required behavior:

- Add `moshare -algo bdec`.
- Keep the algorithm no-op.
- Limit truth tables to `leaves <= 6`.
- Enumerate two-literal AND/OR divisors with polarities.
- Report aggregate and top-candidate statistics.
- Do not write root-level artifacts.
- Do not modify `tc_public/`.
- Preserve existing `none` and `bdiff` behavior.

Done criteria:

- ABC builds.
- `moshare -algo list` shows `bdec`.
- `moshare -algo bdec -stats` runs on at least one public case.
- Before/after `print_stats` is unchanged.
- A written no-op BLIF passes `cec`.
- A candidate audit report is produced under `claude_doc/`.
