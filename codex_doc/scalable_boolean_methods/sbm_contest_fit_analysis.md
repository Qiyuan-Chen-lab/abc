# SBM Paper Fit Analysis for the Multi-Output Sharing Contest

## Objective

Evaluate whether the methods in `papers/MinerU_markdown_Scalable_Boolean_Methods_2053670839602790400.md` are suitable for this ABC-based contest and identify a practical AIG optimization direction.

## Context

The contest objective is BLIF optimization with functional equivalence preserved by ABC `cec`. The score prioritizes AIG node count, then AIG level count, runtime, and peak memory. The current repository already contains ABC AIG optimization infrastructure, including rewriting, refactoring, resubstitution, MFS/MSPF-style optimization, SBD/SAT-based optimization, CUDD/BDD support, and an experimental `orchestrate` command for trying rewrite/refactor/resub choices.

The current ABC standard reference flow is:

```text
b; rewrite; refactor; b; rewrite -lz; b; refactor -z; b
```

It improves many public cases but leaves several cases below 5% AND-node reduction relative to the strash-only reference. These low-improvement cases are the best targets for a new multi-output sharing method.

## Paper Methods

The paper proposes four scalable Boolean method engines:

1. Boolean-difference-based resubstitution.
2. Gradient-based AIG minimization.
3. Heterogeneous elimination for kernel extraction.
4. BDD-based MSPF computation.

## Suitability Ranking

### 1. Boolean-Difference Resubstitution

Fit: high, but implementation risk is medium-high.

This is the best paper method to adapt for the contest because it can explicitly exploit existing logic `g` to rewrite another node `f` as:

```text
f = (f xor g) xor g
```

If `f xor g` has a compact implementation or already exists structurally/functionally in the network, replacing `f` can reduce the MFFC of `f` while reusing `g`. This naturally targets cross-cone sharing when `f` and `g` are drawn from different CO cones or from a multi-output partition.

Contest relevance:

- Targets AIG node count directly.
- Can exploit multi-output similarity rather than only local single-output windows.
- Can be bounded by partition size, support size, BDD size, candidate pair count, and runtime budget.
- Has a natural no-op fallback when no profitable candidate is found.

Main risk:

- The paper uses BDDs and BDD-to-AIG construction. BDD blowup and incorrect local replacement are the main hazards.
- It needs careful level accounting because XOR reconstruction may increase depth.

Recommended adaptation:

- Implement a conservative GIA-level or AIG-wrapper command under the planned `moshare` integration layer.
- Start with small partitions and strict BDD limits.
- Only accept strictly positive node gain at first.
- Prefer candidates from different PO cones or candidates with strong multi-output support overlap.

### 2. Gradient-Based AIG Minimization

Fit: medium for short-term contest improvement, low as a true multi-output sharing algorithm.

The repo already has an experimental `orchestrate` command that tries rewrite/refactor/resub variants and records gains. This resembles the paper's adaptive AIG optimization direction. However, the paper method mainly schedules existing moves more intelligently; it does not itself introduce a new sharing transform.

Contest relevance:

- Can improve runtime/QoR tradeoff and may help cases where the ABC standard reference script is weak.
- Easier to experiment with than a new Boolean engine.
- Less aligned with the stated contest focus on true multi-output sharing.

Main risk:

- It may overfit public cases by tuning script order.
- It can become a wrapper around existing single-output/local transformations, which is weaker for the contest's intended opportunity.
- Existing `abcOrchestration.c` writes CSV files in the repository root in some paths, which conflicts with project artifact-location rules and should not be reused blindly.

Recommended adaptation:

- Use it as a secondary flow controller inside `moshare`, not as the primary contest algorithm.
- Keep all generated logs under `claude_run/` or `claude_doc/`.
- Use deterministic waterfall selection with a small move budget.

### 3. BDD-Based MSPF

Fit: medium-high theoretically, high implementation risk.

MSPF is powerful for don't-care-based local rewiring and can capture multi-output flexibility. It is relevant to this contest because many-output cases may contain permissible changes that preserve all POs while enabling sharing.

Contest relevance:

- Strong correctness model if implemented properly.
- Can exploit multi-output flexibility.
- Existing ABC has MFS and BDD infrastructure that may be reusable.

Main risk:

- Full BDD-based MSPF across many POs is memory-sensitive.
- Public cases include many-output designs, such as cases with 96, 532, or more POs. Naive MSPF computation over all outputs is unlikely to be safe.
- Implementation complexity is higher than Boolean-difference resubstitution.

Recommended adaptation:

- Do not start here.
- Consider a later restricted variant for small PO groups or local TFO windows.
- Use strict BDD node and memory limits.

### 4. Heterogeneous Elimination and Kernel Extraction

Fit: medium for logic sharing, low-medium for AIG-only implementation.

Kernel extraction can find broad algebraic sharing, but it naturally works on SOP/network representations rather than directly on `Gia_Man_t`. ABC already has `eliminate`, `fx`, and related network-level extraction commands.

Contest relevance:

- Can find wide common factors that AIG rewriting misses.
- May help decoder/control-like public cases.

Main risk:

- Network representation conversion may disturb AIG structure.
- It is less directly controllable by AIG node/level profitability.
- It may increase runtime or levels if used aggressively.

Recommended adaptation:

- Use as an optional benchmarked flow variant, not the first new implementation.
- Try existing ABC commands before implementing a new kerneling engine.

## Recommended First Algorithm

Implement a conservative Boolean-difference sharing algorithm as a `moshare` algorithm variant, for example:

```text
moshare -algo bdiff
```

The first implementation should be intentionally narrow:

- Combinational AIG only.
- Operate after `strash`.
- Preserve CI/CO ordering exactly.
- Partition by CO cones or support-similar node groups.
- Candidate nodes should be internal AIG nodes, not CIs or COs.
- Prefer candidate pairs where `f` and `g` are in different CO cones but share support.
- Use BDDs or truth tables only for small-support windows.
- Accept only transformations with positive estimated and measured AIG node gain.
- Reject candidates that exceed a level-growth bound.
- Leave the network unchanged when unsupported, unprofitable, or resource-limited.

## Candidate Bounds

Initial conservative defaults:

- Maximum partition support: 12 to 16 inputs.
- Maximum partition nodes: 200 to 500 nodes.
- Maximum partition levels: 8 to 16.
- Maximum candidate pairs per partition: 1000 to 5000.
- Maximum BDD nodes per candidate difference: 10 to 32.
- Maximum total BDD nodes per partition: configurable hard cap.
- Maximum accepted level growth per changed CO: 0 initially, then maybe 1 after benchmarking.
- Runtime budget per public case: start under 2x the ABC standard reference runtime for smoke experiments.

These should become explicit `moshare` parameters rather than hidden constants.

## Profitability Model

Primary score:

```text
gain = MFFC_size(f) + estimated_existing_sharing(diff) - cost(diff) - xor_cost
```

Acceptance checks:

- `gain > 0` for the first version.
- New global AIG AND count after cleanup is lower than before.
- Maximum CO level does not increase, or stays within an explicit bound.
- Transformation passes local sanity checks before updating the manager.
- Final generated BLIF passes `cec`.

Zero-gain rewrites should be disabled initially. They can open later opportunities, but they complicate contest validation.

## Implementation Placement

Recommended module:

```text
src/opt/moshare/
├── moshare.h
├── moshare.c
├── moshareAlgoNone.c
├── moshareAlgoBdiff.c
├── moshareBdd.c
├── mosharePartition.c
└── module.make
```

Recommended command wrapper:

```text
src/base/abci/abcMoshare.c
```

This follows the existing TODO in `codex_doc/todo_moshare_integration_layer.md` and keeps new contest algorithms isolated from upstream ABC behavior.

## Relevant Existing APIs and Modules

Likely source areas:

- `src/aig/gia/`: preferred AIG representation for new logic sharing.
- `src/base/abci/`: command wrapper and ABC frame integration.
- `src/bdd/cudd/`, `src/bdd/extrab/`: CUDD and BDD helper APIs.
- `src/opt/res/`: existing resubstitution ideas.
- `src/opt/mfs/`: don't-care/MSPF-style optimization.
- `src/opt/sbd/`: SAT-based optimization with internal don't-cares.
- `src/misc/vec/`: ABC vector containers.

Key concerns:

- Use literals and object IDs consistently.
- Preserve complements carefully.
- Recompute levels before level-based filtering.
- Reset temporary marks and values.
- Free all BDD references and temporary vectors.

## Validation Plan

Minimum smoke validation:

- Build ABC with `make`.
- Run `moshare -h`.
- Run `moshare -algo list`.
- Run `moshare -algo bdiff` on one small public case.
- Write optimized BLIF under `claude_run/`.
- Run `cec` against the original BLIF.
- Record before/after `print_stats`.

Benchmark targets:

- Start with public cases where the ABC standard reference flow has low improvement:
  - `tc_05`, `tc_07`, `tc_08`, `tc_09`, `tc_10`, `tc_15`, `tc_16`, `tc_17`, `tc_18`, `tc_19`, `tc_20`, `tc_21`, `tc_22`, `tc_23`, `tc_24`, `tc_26`.
- Include at least one larger many-input/many-output case:
  - `tc_12`, `tc_19`, `tc_20`, `tc_21`, or `tc_22`.
- Always record AND count, level count, runtime, memory when measured, exact command, and CEC result.

## Main Risks

- BDD memory blowup on large partitions.
- Incorrect replacement due to window support mismatch.
- XOR reconstruction increasing levels.
- Candidate-pair quadratic runtime.
- Weak benefit if public cases lack compact `f xor g` differences.
- Reusing `abcOrchestration.c` as-is may create root-level CSV artifacts and non-isolated behavior.

## Conclusion

The paper is relevant to this contest, but the full SBM framework is too large to implement directly. The best contest-aligned path is a narrow Boolean-difference sharing transform inside the planned `moshare` integration layer. This offers a real multi-output sharing mechanism and can be bounded conservatively enough for public-case benchmarking.

Gradient-based orchestration should be treated as a secondary flow-tuning layer. BDD-MSPF is promising but should come after a simpler Boolean-difference prototype. Heterogeneous elimination/kerneling is worth benchmarking with existing ABC commands before writing a new engine.

## Claude Code Handoff Instructions

Do not implement the full SBM framework first. Implement the `moshare` integration layer from `codex_doc/todo_moshare_integration_layer.md`, then add a conservative `bdiff` algorithm variant only after the no-op command is buildable and tested.

Done criteria for the first real algorithm:

- `moshare -algo bdiff` runs from ABC batch mode.
- Unsupported or unprofitable cases leave the network unchanged.
- At least one public case runs without crash.
- Every generated BLIF passes `cec`.
- Before/after AIG node and level metrics are recorded.
- Runtime and memory are measured on at least one representative public case.
