# TODO: Multi-Algorithm Integration Layer for Contest Logic Sharing

## Objective

Build a thin, stable, low-intrusion integration layer for future multi-output logic sharing algorithms.

The goal of this task is not to implement a real optimization algorithm yet. The goal is to create the infrastructure that lets multiple future algorithms coexist, be selected from ABC batch mode, share common parameters/results, and be benchmarked independently.

## Contest Metric Targeted

This task does not directly optimize contest metrics. It enables later algorithms to target:

- AIG node count reduction.
- AIG level control.
- Runtime and memory bounded experimentation.

## Expected Algorithmic Benefit

Future algorithms can be integrated as isolated implementations under one contest command instead of repeatedly modifying ABC core command registration and existing optimization code.

Expected benefits:

- Multiple algorithm variants can coexist.
- Benchmark scripts can use one stable command interface.
- Old benchmarked variants can remain available while new variants are developed.
- ABC core code is touched only through a small integration surface.

## Contest Constraint Summary

- Preserve existing ABC command behavior.
- Do not modify existing rewrite, refactor, resubstitution, cleanup, or mapping behavior.
- Add a new opt-in command for contest experiments.
- Keep CI/CO ordering unchanged.
- Use deterministic defaults.
- Provide a no-op reference algorithm path that leaves the network unchanged.
- Make it possible to add future algorithms without touching `src/base/abci/abc.c` again, if practical.

## Files to Inspect

Read these first:

- `CLAUDE.md`
- `claude_doc/Architect.md`
- `claude_doc/Run.md`
- `src/base/abci/abc.c`
- `src/base/abci/module.make`
- `Makefile`
- Existing nearby command wrappers under `src/base/abci/`
- Existing `src/opt/*/module.make` examples

Useful examples to inspect:

- `src/base/abci/abcRewrite.c`
- `src/base/abci/abcResub.c`
- `src/base/abci/abcRefactor.c`
- `src/aig/gia/gia.h`

## Files to Modify or Add

Prefer this structure:

```text
src/opt/moshare/
├── moshare.h
├── moshare.c
├── moshareUtil.c
├── moshareAlgoNone.c
└── module.make
```

Add one command wrapper:

```text
src/base/abci/abcMoshare.c
```

Modify only required integration files:

```text
src/base/abci/abc.c
src/base/abci/module.make
Makefile
```

If the actual ABC build system uses a different module inclusion pattern, follow the existing local pattern and keep the diff minimal.

## Required Behavior

Add a new ABC command:

```text
moshare
```

The command should support at least:

```text
moshare -h
moshare -algo none
moshare -algo list
moshare -v
```

Recommended optional flags for future compatibility:

```text
moshare -algo <name>
moshare -maxwin <int>
moshare -maxcut <int>
moshare -maxcand <int>
moshare -seed <int>
moshare -stats
moshare -v
```

Initial supported algorithms:

- `none`: no-op algorithm. It should leave the network functionally unchanged and report before/after stats.
- `list`: print available algorithms and return without modifying the network.

Do not implement real multi-output sharing in this task.

## Proposed Public Interface

Create a small public interface in `src/opt/moshare/moshare.h`.

Suggested shape:

```c
typedef enum Mosh_AlgoId_t_ {
    MOSH_ALGO_NONE = 0
} Mosh_AlgoId_t;

typedef struct Mosh_Par_t_ {
    int AlgoId;
    int nMaxWindowSize;
    int nMaxCutSize;
    int nMaxCandidates;
    int RandomSeed;
    int fStats;
    int fVerbose;
} Mosh_Par_t;

typedef struct Mosh_Res_t_ {
    int fChanged;
    int nNodesBefore;
    int nNodesAfter;
    int nLevelsBefore;
    int nLevelsAfter;
    int nCandidates;
    int nApplied;
} Mosh_Res_t;

void Mosh_ParDefault( Mosh_Par_t * pPar );
void Mosh_ResClear( Mosh_Res_t * pRes );
void Mosh_PrintAlgos( FILE * pOut );
int  Mosh_AlgoNameToId( const char * pName );
const char * Mosh_AlgoIdToName( int AlgoId );
Gia_Man_t * Mosh_ManPerform( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

Adjust names and signatures to match ABC style if nearby code strongly suggests a better pattern.

## Implementation Steps

- [ ] Inspect existing ABC command wrappers and command registration style.
- [ ] Inspect existing module/build inclusion style.
- [ ] Create `src/opt/moshare/` as the isolated contest integration module.
- [ ] Add `moshare.h` with parameter, result, algorithm ID, and public entry points.
- [ ] Add `moshare.c` as the dispatcher and algorithm registry.
- [ ] Add `moshareAlgoNone.c` implementing the no-op algorithm.
- [ ] Add `moshareUtil.c` only for genuinely shared helpers, such as stats collection or algorithm-name mapping.
- [ ] Add `src/base/abci/abcMoshare.c` as the only ABC command wrapper.
- [ ] Register the `moshare` command in `src/base/abci/abc.c`.
- [ ] Add the command wrapper to `src/base/abci/module.make`.
- [ ] Add `src/opt/moshare/` to the build using the existing ABC module pattern.
- [ ] Implement command-line parsing for `-algo`, `-maxwin`, `-maxcut`, `-maxcand`, `-seed`, `-stats`, `-v`, and `-h` if feasible.
- [ ] Ensure `moshare -algo none` does not change logic.
- [ ] Ensure `moshare -algo list` prints available algorithms and does not change logic.
- [ ] Ensure unsupported algorithm names return a clear error without modifying the network.
- [ ] Ensure parameter defaults are deterministic and conservative.
- [ ] Ensure all temporary managers/vectors are freed.

## Candidate Bounds

No real candidate search is implemented in this task.

However, the interface must include explicit bounds for future algorithms:

- `nMaxWindowSize`
- `nMaxCutSize`
- `nMaxCandidates`
- `RandomSeed`

Default values should be conservative and documented in the command help text.

## Profitability Model

No real profitability model is implemented in this task.

The result struct should already support future reporting:

- nodes before/after
- levels before/after
- number of candidates
- number of applied transformations
- changed/no-change status

The no-op algorithm should report `fChanged = 0`.

## No-Op Fallback Behavior

The command must never force a risky transformation in this infrastructure task.

Required fallback cases:

- Unsupported algorithm name: print an error and leave the network unchanged.
- Missing network: print a clear error and return according to ABC command conventions.
- `-algo none`: leave the network unchanged.
- Internal no-op result: keep the original network.

## Validation Goals and Acceptance Criteria

Use `claude_doc/Run.md` for exact command patterns.

Minimum acceptance criteria:

- ABC builds successfully.
- `moshare -h` is available from ABC batch mode.
- `moshare -algo list` prints available algorithms.
- `moshare -algo none` runs on at least one BLIF without crash.
- Running `moshare -algo none` preserves equivalence on at least one public case.
- Before/after metrics for `moshare -algo none` show no unexpected change.
- Existing common ABC flows still run after command registration.

## Required Post-Implementation Smoke Test

After writing the code, Claude Code must run a real smoke test to prove the interface is callable and the no-op path is functionally safe.

Pick one small BLIF from `tc_public/`. If there is no obvious small case, use the first readable BLIF found under `tc_public/`.

Required checks:

- [ ] Build ABC successfully.
- [ ] Run the command help from batch mode:

```bash
./abc -c "moshare -h"
```

- [ ] Run the algorithm list mode from batch mode:

```bash
./abc -c "moshare -algo list"
```

- [ ] Run the no-op algorithm on one public BLIF, print before/after metrics, and write the output under `claude_run/`:

```bash
./abc -c "read_blif tc_public/<case>.blif; strash; print_stats; moshare -algo none -stats; print_stats; write_blif claude_run/moshare_none_<case>.blif"
```

- [ ] Check equivalence between the original public BLIF and the no-op output:

```bash
./abc -c "cec tc_public/<case>.blif claude_run/moshare_none_<case>.blif"
```

- [ ] Run one unsupported algorithm-name test and confirm it fails cleanly without crash:

```bash
./abc -c "read_blif tc_public/<case>.blif; strash; moshare -algo __invalid_algo__"
```

Smoke test pass criteria:

- Help text is printed.
- Algorithm list contains `none`.
- `moshare -algo none` does not crash.
- Before/after stats are unchanged or only differ in a clearly explained no-op-compatible way.
- CEC reports equivalence.
- Invalid algorithm handling prints a clear error and does not crash.

## Suggested Smoke Cases

Use at least one small public case from `tc_public/`.

Suggested minimum:

- One small case.
- One case with many outputs if easy to locate.

Do not modify benchmark input files.

## Correctness Risks

- Incorrect conversion between `Abc_Ntk_t` and `Gia_Man_t`.
- Accidentally updating the ABC frame on a no-op path with a broken manager.
- Losing CI/CO ordering during conversion.
- Treating GIA object IDs as literals or literals as object IDs.
- Leaking temporary managers when no-op or error paths return early.
- Command parser accepting invalid parameters silently.

## Runtime and Memory Risks

This task should have negligible runtime and memory overhead.

Avoid:

- Full graph duplication unless required by the chosen no-op implementation.
- Global caches.
- Persistent global state.
- Unbounded per-node work in the command wrapper.

## Claude Code Handoff Instructions

Implement only the integration layer. Do not implement real optimization logic in this task.

Keep the patch reviewable:

- Isolated new module under `src/opt/moshare/`.
- One command wrapper under `src/base/abci/`.
- Minimal build and registration edits.
- No changes to existing optimization algorithms.
- No changes to public benchmarks.

If local ABC patterns conflict with the proposed file names or function names, follow the local pattern and document the deviation in the final response.

## Done Criteria

- [ ] New `moshare` command exists.
- [ ] `moshare -h` explains command usage and parameters.
- [ ] `moshare -algo list` lists at least `none`.
- [ ] `moshare -algo none` runs without changing the network.
- [ ] ABC builds successfully.
- [ ] At least one public BLIF smoke test passes.
- [ ] Equivalence is checked for the no-op path on at least one case.
- [ ] Implementation follows module isolation rules in `claude_doc/Architect.md`.
- [ ] Final response lists changed files, validation commands, and any deviations from this TODO.

## Open Questions

- Should the stable command be exactly `moshare`, or should it use a more contest-specific prefix?
- Should future algorithms be selected only by `-algo`, or should some mature algorithms receive direct command aliases later?
- Should result/stat reporting be printed by default or only under `-stats`/`-v`?
- Should the no-op algorithm update the ABC frame with a duplicated manager, or simply return without updating when unchanged?
