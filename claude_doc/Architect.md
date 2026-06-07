# ABC Architecture Reference

## Repository Map

Important locations:

| Purpose | Path |
| --- | --- |
| Main build file | `Makefile` |
| CMake build file | `CMakeLists.txt` |
| Contest statement | `contest.pdf` |
| Codex planning documents | `codex_doc/` |
| Claude reference documents and durable reports | `claude_doc/` |
| Claude scripts | `claude_scripts/` |
| Claude temporary run directory | `claude_run/` |
| Public BLIF benchmarks | `tc_public/` |
| Command registration | `src/base/abci/abc.c` |
| ABC command wrappers | `src/base/abci/` |
| Legacy network layer | `src/base/abc/` |
| Modern AIG manager | `src/aig/gia/` |
| Logic optimization algorithms | `src/opt/` |
| Boolean utilities | `src/bool/` |
| Vector/hash utilities | `src/misc/` |
| Equivalence checking | `src/proof/cec/` |

## Source Tree Overview

The `src/` tree is the main ABC implementation area. The directories below are the most relevant when adding or modifying logic synthesis algorithms.

```text
src/
├── base/
│   ├── abc/              Legacy network data structures and utilities (`Abc_Ntk_t`, `Abc_Obj_t`).
│   ├── abci/             ABC command implementations and command registration.
│   ├── cmd/              Command framework (`Cmd_CommandAdd`, command dispatch).
│   ├── io/               BLIF, AIGER, and other design I/O.
│   ├── main/             ABC executable entry and framework startup/shutdown.
│   ├── ver/              Verilog handling.
│   ├── wlc/              Word-level circuit representation.
│   └── test/             Base-level tests and utilities.
├── aig/
│   ├── gia/              Modern AIG manager (`Gia_Man_t`); preferred for new AIG algorithms.
│   ├── aig/              Legacy AIG manager (`Aig_Man_t`).
│   ├── saig/             Sequential AIG algorithms and utilities.
│   ├── hop/              HOP AIG package for fast Boolean operations.
│   ├── ioa/              AIGER read/write utilities.
│   └── ivy/              Ivy AIG package and related optimization utilities.
├── opt/
│   ├── rwr/              DAG-aware rewriting.
│   ├── res/              Resubstitution.
│   ├── nwk/              Network manipulation and merging utilities.
│   ├── mfs/              MFS optimization.
│   ├── dar/              DAG-aware rewriting/refactoring flow.
│   ├── rwt/              Rewriting with truth tables.
│   ├── ret/              Retiming.
│   ├── fret/             Forward retiming.
│   ├── sim/              Simulation-based optimization support.
│   ├── cut/              Cut enumeration.
│   ├── fxu/              Fast extraction, mostly single-output oriented.
│   ├── sfm/              SFM optimization.
│   ├── sbd/              SBD optimization.
│   ├── dau/              Decomposition and AIG utility algorithms.
│   ├── dsc/              DSC optimization.
│   ├── cgt/              CGT optimization.
│   ├── lpk/              LPK optimization.
│   ├── rar/              RAR optimization.
│   ├── eslim/            eSLIM optimization.
│   ├── ufar/             UFAR optimization.
│   ├── moshare/           Contest multi-output sharing framework (command: `moshare`).
│   └── util/             Shared optimization utilities.
├── map/
│   ├── if/               Integrated FPGA LUT mapper.
│   ├── fpga/             FPGA mapping.
│   ├── scl/              Standard-cell mapping.
│   ├── amap/             AIG mapping.
│   ├── cov/              Covering.
│   ├── mapper/           Generic mapper.
│   ├── super/            Supergate support.
│   └── mpm/              MPM mapper.
├── proof/
│   ├── cec/              Combinational equivalence checking.
│   ├── acec/             ABC CEC utilities.
│   ├── fraig/            FRAIG package.
│   ├── pdr/              Property directed reachability.
│   ├── int/              Interpolation.
│   ├── abs/              Abstraction.
│   └── ssw/              Sequential signal correspondence.
├── sat/
│   ├── glucose/          Glucose SAT solver integration.
│   ├── glucose2/         Glucose 2 SAT solver integration.
│   ├── kissat/           Kissat SAT solver integration.
│   ├── cadical/          CaDiCaL SAT solver integration.
│   ├── bmc/              Bounded model checking support.
│   └── cnf/              CNF construction and manipulation.
├── bool/
│   ├── kit/              Boolean function and truth-table utilities.
│   ├── dec/              Boolean decomposition.
│   ├── bdc/              BDD-based decomposition.
│   ├── rpo/              RPO utilities.
│   └── lucky/            Lucky decomposition/optimization utilities.
├── bdd/                  BDD packages and related utilities.
└── misc/
    ├── vec/              Core vector containers (`Vec_Int_t`, `Vec_Ptr_t`, `Vec_Wec_t`).
    ├── hash/             Hash table utilities.
    ├── util/             General utilities.
    ├── tim/              Timing utilities.
    ├── extra/            Extra helper algorithms.
    └── parse/            Parsing helpers.
```

For this contest, the most likely implementation areas are `src/aig/gia/`, `src/opt/`, `src/base/abci/`, `src/bool/`, and `src/misc/`.

## Algorithm Module Isolation Rules

Future contest work may test several competing algorithms in the same repository. New implementations must be isolated enough that they can be enabled, disabled, benchmarked, compared, and removed independently.

### Minimal Intrusion Rule

Prefer additive changes over invasive edits:

- Add new implementation files instead of rewriting existing optimization files.
- Add thin command wrappers instead of changing existing command semantics.
- Touch central ABC files only for required integration points, such as command registration and build file inclusion.
- Keep edits to `src/base/abci/abc.c`, `Makefile`, and `module.make` files as small and mechanical as possible.
- Do not modify shared core algorithms such as existing rewrite, refactor, resubstitution, or cleanup behavior unless the task explicitly requires it.
- Do not change public APIs used by unrelated ABC subsystems unless there is no narrower alternative.
- Do not alter benchmark inputs, default startup scripts, or existing command names for experiment convenience.

When an invasive edit looks necessary, first consider whether a wrapper, adapter, new command, or duplicated experiment-local path can avoid changing shared behavior.

### Recommended Module Layout

For a substantial new algorithm, prefer a dedicated module under `src/opt/`:

```text
src/opt/<algo_name>/
├── <algo_name>.h        Public entry points and parameter/result structs.
├── <algo_name>.c        Main algorithm implementation.
├── <algo_name>Util.c    Optional private helpers.
├── <algo_name>Sim.c     Optional simulation/candidate code.
└── module.make          Build entries for this module.
```

For a small experiment, a single clearly named file under a related existing directory is acceptable, but it must not bury unrelated behavior inside an existing algorithm file.

### Moshare Integration Pattern (Preferred for Contest Algorithms)

The `moshare` command under `src/opt/moshare/` is the standard integration point for new contest algorithms. Instead of adding a new ABC command per algorithm, add the algorithm as a plug-in under `moshare`:

```text
src/opt/moshare/
├── moshare.h             Public types, enum, param/result structs
├── moshare.c             Dispatcher (Mosh_ManPerform)
├── moshareUtil.c         Shared helpers, name registry, defaults
├── moshareAlgoNone.c     No-op reference implementation
├── moshareAlgoXxx.c      <-- add new algorithms as new files
└── module.make
```

Benefits:
- No need to modify `abc.c`, `src/base/abci/module.make`, or `Makefile` for new algorithms.
- All algorithms share the same command `moshare -algo <name>`.
- Algorithms coexist as isolated files in the same directory.
- Benchmark scripts use a single stable command interface.

See `claude_doc/moshare_integration_guide.md` for the step-by-step procedure.

Use stable prefixes for symbols:

- Types: `<Algo>_Par_t`, `<Algo>_Man_t`, `<Algo>_Res_t`.
- Functions: `<Algo>_ManAlloc`, `<Algo>_ManFree`, `<Algo>_ManPerform`.
- Command wrapper: `Abc_Command<Algo>` or another existing local naming pattern.
- ABC command name: a unique, explicit name such as `<algo_name>` or `<contest_algo_name>`.

Avoid generic names such as `NewOpt`, `MyOpt`, `BetterRewrite`, or `contest` once the algorithm has a real purpose.

### Public Interface Rules

Each algorithm module should expose a small public interface:

- A parameter struct with all tunable limits and mode switches.
- A result/statistics struct when the algorithm reports changes, candidates, timing, or failure reasons.
- One main entry point that accepts the relevant ABC data structure and parameters.
- Optional initialization and cleanup helpers only when needed.

Do not expose internal candidate data structures, temporary vectors, simulation buffers, or hash tables unless another module truly needs them.

Prefer this shape:

```c
typedef struct <Algo>_Par_t_ {
    int nMaxCutSize;
    int nMaxWindowSize;
    int nMaxCandidates;
    int fVerbose;
} <Algo>_Par_t;

typedef struct <Algo>_Res_t_ {
    int nNodesBefore;
    int nNodesAfter;
    int nChanges;
    int fChanged;
} <Algo>_Res_t;

Gia_Man_t * <Algo>_ManPerform( Gia_Man_t * pGia, <Algo>_Par_t * pPar, <Algo>_Res_t * pRes );
```

If the algorithm can fail or decide not to transform, make that explicit in the result rather than encoding it through ambiguous `NULL` returns.

### Command Coexistence Rules

Each algorithm should be callable independently from ABC batch mode:

- Add a unique command for each substantial algorithm.
- Do not change existing command defaults to run a new experimental algorithm.
- If adding flags to a shared contest command, flags must select algorithms explicitly.
- Keep command help text accurate and include key limits.
- Command names and flags must remain stable once benchmark scripts depend on them.
- Default command behavior should be deterministic and conservative.

Good patterns:

- `moshare`: one specific multi-output sharing algorithm.
- `moshare2`: a second-generation isolated implementation when behavior differs substantially.
- `contest_flow -algo moshare -maxwin 64`: a wrapper command that selects an isolated implementation.

Poor patterns:

- Changing `rewrite` or `resub` to silently run contest logic.
- Using the same command name for incompatible algorithm versions.
- Adding global flags that affect unrelated ABC commands.

### Shared Utility Rules

Shared helpers are useful, but they must not become a hidden coupling point between experiments.

Create shared utility code only when:

- At least two algorithm modules need the same helper.
- The helper is algorithm-neutral.
- The helper has a small API and clear ownership rules.

Shared utility code should avoid:

- Algorithm-specific global state.
- Hardcoded benchmark assumptions.
- Hidden mutations of `Gia_Man_t`, `Abc_Ntk_t`, or global ABC frame state.
- Side effects outside its documented return values.

When in doubt, keep helper code private inside the algorithm module until reuse is real.

### Parameter and State Rules

All tunable behavior should live in explicit parameter structs:

- Search bounds.
- Cut/window/output-group limits.
- Runtime or candidate caps.
- Verbosity.
- Random seed if randomized filtering is used.
- Feature switches.

Do not store algorithm state in global variables unless ABC infrastructure requires it. If global state is unavoidable, document it, reset it after use, and keep it namespaced to the algorithm.

Temporary per-object state must be cleaned up:

- Reset GIA marks, traversal IDs, and `Value` fields when they are reused.
- Free all `Vec_*`, hash tables, simulation buffers, and temporary managers.
- Keep ownership rules explicit for returned `Gia_Man_t *` or `Abc_Ntk_t *`.

### Versioning and Experiment Coexistence

When developing a new variant:

- Prefer a new module or command if behavior is not backward-compatible with an existing algorithm.
- Keep old benchmarked variants available until the user decides to remove them.
- Avoid overwriting an existing algorithm implementation just to try a new idea.
- Use command flags for small compatible variations.
- Use separate files/modules for materially different candidate generation, scoring, or rewriting strategies.

Document meaningful variants in `codex_doc/` or a durable report under `claude_doc/` when benchmarked.

### Build Integration Rules

Build integration should be minimal and easy to review:

- Add only the new source files to the relevant `module.make`.
- If a new `src/opt/<algo_name>/` directory is added, include that module through the existing ABC module pattern.
- Do not reorder unrelated build entries.
- Do not remove existing source files from the build.
- Do not change compiler flags globally for one algorithm.
- Keep optional experiment code compiled by default only when it is stable and isolated.

### Review Checklist for New Algorithms

Before finishing a new algorithm implementation, verify:

- The algorithm has a unique module/file ownership boundary.
- Existing ABC commands behave as before unless explicitly changed.
- The command can be run independently from batch mode.
- CI/CO order is preserved.
- Object IDs and literals are handled consistently.
- All temporary marks and object fields are reset or locally scoped.
- No global state leaks into other algorithms.
- Search is bounded by explicit parameters.
- No benchmark names or circuit-specific heuristics are hardcoded.
- Failure or no-profit cases leave the input behavior unchanged.
- The implementation can coexist with previous and future contest algorithms.

## Network Layers

### Legacy Network Layer

`Abc_Ntk_t` and `Abc_Obj_t` live under `src/base/abc/`.

Many existing ABC commands still use this layer. Use it when integrating with an existing command path that already operates on `Abc_Ntk_t`.

### Modern AIG Layer

`Gia_Man_t` and `Gia_Obj_t` live under `src/aig/gia/`.

Prefer `Gia_Man_t` for new AIG-level optimization work unless an existing command path clearly requires `Abc_Ntk_t`.

Common concerns:

- Preserve CI and CO ordering.
- Handle complemented literals correctly.
- Do not confuse object IDs with literals.
- Reset temporary marks, traversal IDs, and `Value` fields.
- Recompute levels before using level-based profitability.
- Be explicit about manager ownership and lifetime.

## Common GIA APIs

- `Gia_ManCreate(int nObjsAlloc)`
- `Gia_ManAppendCi(pGia)`
- `Gia_ManAppendCo(pGia, iLit)`
- `Gia_ManAppendAnd(pGia, iLit0, iLit1)`
- `Gia_ManAppendBuf(pGia, iLit)`
- `Gia_ManCleanup(pGia)`
- `Gia_ManComputeLevels(pGia)`
- `Gia_ManPiNum(pGia)`
- `Gia_ManPoNum(pGia)`
- `Gia_ManObjNum(pGia)`
- `Gia_ManAndNum(pGia)`
- `Gia_ObjIsAnd(pObj)`
- `Gia_ObjIsCi(pObj)`
- `Gia_ObjIsCo(pObj)`
- `Gia_ManForEachCi(pGia, pObj, i)`
- `Gia_ManForEachCo(pGia, pObj, i)`
- `Gia_ManForEachAnd(pGia, pObj, i)`

## Adding a New ABC Command

Typical integration path:

1. Implement algorithm code under an appropriate `src/opt/<name>/` directory or an existing related directory.
2. Add a public header only if another module needs to call the implementation.
3. Add a command wrapper under `src/base/abci/`.
4. Register the command in `src/base/abci/abc.c` with `Cmd_CommandAdd`.
5. Update the relevant `module.make` file.
6. Build and run smoke tests.

### Shortcut: Adding a Contest Algorithm via Moshare

For contest multi-output sharing algorithms, use the moshare plug-in system instead of the full integration path above:

1. Add algorithm ID to `Mosh_AlgoId_t` in `src/opt/moshare/moshare.h`.
2. Register the name in `src/opt/moshare/moshareUtil.c`.
3. Add a dispatch case in `src/opt/moshare/moshare.c`.
4. Create the algorithm file `src/opt/moshare/moshareAlgoXxx.c`.
5. Add the file to `src/opt/moshare/module.make`.
6. Build and run smoke tests via `moshare -algo xxx`.

No changes to `abc.c`, `src/base/abci/module.make`, or `Makefile` are needed.
See `claude_doc/moshare_integration_guide.md` for details.

Follow nearby ABC style before introducing new abstractions.

## Key Source References

| Purpose | Path |
| --- | --- |
| GIA manager header | `src/aig/gia/gia.h` |
| GIA manager implementation | `src/aig/gia/giaAig.c`, `src/aig/gia/giaMan.c`, `src/aig/gia/giaUtil.c` |
| Command registration center | `src/base/abci/abc.c` |
| Command framework header | `src/base/cmd/cmd.h` |
| ABC command build config | `src/base/abci/module.make` |
| Logic optimization root | `src/opt/` |
