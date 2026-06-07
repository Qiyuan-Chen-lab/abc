# Contest Algorithm Reference

## Core Idea

Single-output local-window optimization can miss sharing opportunities across output cones. Contest-oriented algorithms should explicitly exploit multi-output structure rather than only chaining existing local passes.

## Promising Directions

- Multi-output divisor extraction across several CO cones.
- Sharing-aware resubstitution.
- Reconvergent fanout analysis.
- Cut-based common subfunction discovery.
- Simulation-assisted candidate filtering.
- Common subgraph reuse across output groups.
- Selective cleanup, balancing, rewriting, or refactoring after sharing transforms.

## Planning Constraints

Every algorithm plan should state:

- Why the change targets true multi-output sharing.
- The sharing signal to exploit, such as common divisors, compatible cuts, repeated subfunctions, or equivalent simulation signatures.
- Transformation granularity: whole AIG, CO group, MFFC, window, cut set, or divisor set.
- Candidate bounds: cut size, window size, output group size, fanout cone size, candidate count, runtime budget, or memory budget.
- Profitability model: node-count reduction first, then level impact, runtime cost, and memory cost.
- No-op fallback behavior when no profitable safe transform is found.
- Determinism requirements, including fixed seeds for randomized filtering.

## Implementation Constraints

- Preserve CI/CO ordering.
- Preserve combinational equivalence and compatibility with ABC `cec`.
- Prefer AIG/GIA-level implementation for sharing transforms.
- Avoid benchmark-specific heuristics and hardcoded case names.
- Avoid exact global Boolean optimization on large cones unless strict limits are implemented.
- Do not accept severe CO-level growth unless benchmark data justifies the tradeoff.
- Leave the network unchanged on unsupported or unprofitable inputs.

## Integration

New contest algorithms should integrate through the **moshare** framework (`src/opt/moshare/`) using the `moshare -algo <name>` command. This avoids modifying `abc.c`, `src/base/abci/module.make`, or `Makefile` for each new algorithm. See `claude_doc/moshare_integration_guide.md` for the plug-in procedure.

## Done Criteria

An algorithm implementation is not done until:

- ABC builds successfully.
- The new command or flow runs from ABC batch mode.
- At least one public BLIF smoke test runs without crash.
- Generated optimized BLIF passes `cec` against the original.
- Before/after `print_stats` results are recorded.
- Runtime is measured for at least one representative public case when search behavior changes.
- Regressions and remaining risks are documented.

