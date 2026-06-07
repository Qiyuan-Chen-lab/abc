# Moshare Integration Guide

## Overview

`moshare` is the contest multi-output sharing integration layer. It provides a stable ABC batch command, a pluggable algorithm registry, and shared parameter/result structs. New contest algorithms are integrated as isolated modules under `src/opt/moshare/` and selected via `-algo <name>`.

## Why Use Moshare

Before moshare, every new algorithm required:

- Adding a new command name to `abc.c`
- Writing a full command wrapper in `src/base/abci/`
- Modifying `abc.c`, `module.make`, and `Makefile`

With moshare, adding a new algorithm only requires:

- One new `.c` file under `src/opt/moshare/`
- One new enum entry in `moshare.h`
- One line in `moshareUtil.c` (name mapping)
- One case in `moshare.c` (dispatch)
- One line in `module.make` (build)

No changes to `abc.c`, `src/base/abci/module.make`, or `Makefile` are needed.

## Architecture

```
src/opt/moshare/
├── moshare.h             Public types and entry points
├── moshare.c             Dispatcher (Mosh_ManPerform)
├── moshareUtil.c         Shared helpers, param defaults, name registry
├── moshareAlgoNone.c     No-op reference implementation
├── moshareAlgoXxx.c      <-- future algorithms go here
└── module.make           Build entries

src/base/abci/
└── abcMoshare.c          ABC command wrapper (one-time integration)
```

The command wrapper `abcMoshare.c` handles:

- Manual argument parsing (`-algo <name>`, `-maxwin`, `-maxcut`, `-maxcand`, `-seed`, `-stats`, `-v`, `-h`)
- Ntk→Gia conversion fallback when `pAbc->pGia` is not set
- Result handling (`pRes->fChanged` controls whether `Abc_FrameUpdateGia` is called)
- `-algo list` special case (no network needed)

## Public Interface

```c
// Algorithm IDs — add new entries here
#define MOSH_ALGO_COUNT 2  // <-- increment when adding
typedef enum Mosh_AlgoId_t_ {
    MOSH_ALGO_NONE = 0,
    MOSH_ALGO_XXX  = 1   // <-- new algorithm
} Mosh_AlgoId_t;

// Parameters — all tunable limits
typedef struct Mosh_Par_t_ {
    int AlgoId;
    int nMaxWindowSize;    // default 64
    int nMaxCutSize;       // default 8
    int nMaxCandidates;    // default 100
    int RandomSeed;        // default 0
    int fStats;
    int fVerbose;
} Mosh_Par_t;

// Results — populated by each algorithm
typedef struct Mosh_Res_t_ {
    int fChanged;          // 1 = network modified
    int nNodesBefore;
    int nNodesAfter;
    int nLevelsBefore;
    int nLevelsAfter;
    int nCandidates;
    int nApplied;
} Mosh_Res_t;

// Entry point — each algorithm implements this signature
Gia_Man_t * Mosh_ManPerformAlgoXxx( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

## Step-by-Step: Adding a New Algorithm

### 1. Add algorithm ID in `src/opt/moshare/moshare.h`

```c
#define MOSH_ALGO_COUNT 2  // was 1
typedef enum Mosh_AlgoId_t_ {
    MOSH_ALGO_NONE = 0,
    MOSH_ALGO_XXX  = 1   // add here
} Mosh_AlgoId_t;
```

Also add the function declaration:

```c
Gia_Man_t * Mosh_ManPerformAlgoXxx( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

### 2. Register the name in `src/opt/moshare/moshareUtil.c`

In `Mosh_AlgoNameToId`:

```c
if ( strcmp( pName, "xxx" ) == 0 )
    return MOSH_ALGO_XXX;
```

In `Mosh_AlgoIdToName`:

```c
case MOSH_ALGO_XXX: return "xxx";
```

### 3. Add dispatch case in `src/opt/moshare/moshare.c`

```c
case MOSH_ALGO_XXX:
    return Mosh_ManPerformAlgoXxx( pGia, pPar, pRes );
```

### 4. Create `src/opt/moshare/moshareAlgoXxx.c`

Follow the template:

```c
#include "moshare.h"

ABC_NAMESPACE_IMPL_START

Gia_Man_t * Mosh_ManPerformAlgoXxx( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes )
{
    Mosh_ResClear( pRes );

    // collect before stats
    pRes->nNodesBefore  = Gia_ManAndNum( pGia );
    pRes->nLevelsBefore = Gia_ManLevelNum( pGia );

    // TODO: implement algorithm logic here
    // ...

    // collect after stats
    pRes->nNodesAfter   = Gia_ManAndNum( pResult );
    pRes->nLevelsAfter  = Gia_ManLevelNum( pResult );
    pRes->fChanged      = (pRes->nNodesBefore != pRes->nNodesAfter);

    if ( pPar->fStats )
    {
        fprintf( stdout, "moshare (algo xxx): nodes %d -> %d  levels %d -> %d\n",
            pRes->nNodesBefore, pRes->nNodesAfter,
            pRes->nLevelsBefore, pRes->nLevelsAfter );
    }

    return pResult;
}

ABC_NAMESPACE_IMPL_END
```

### 5. Add to `src/opt/moshare/module.make`

```
SRC +=  src/opt/moshare/moshare.c \
    src/opt/moshare/moshareUtil.c \
    src/opt/moshare/moshareAlgoNone.c \
    src/opt/moshare/moshareAlgoXxx.c
```

### 6. Build and test

```bash
make ABC_USE_NO_READLINE=1 -j$(nproc)
./abc -c "moshare -algo list"                    # verify it appears
./abc -c "read_blif tc_public/tc_public_1/input.blif; strash; moshare -algo xxx -stats"
```

## Algorithm Contract

Every algorithm must follow these rules:

### Correctness

- **Preserve CI/CO ordering**: Do not reorder primary inputs or outputs.
- **Handle complemented literals correctly**: Use `Abc_LitNot`, `Abc_LitRegular`, `Abc_LitIsCompl`; never confuse object IDs with literals.
- **Pass CEC**: The optimized network must be combinationally equivalent to the input.
- **No-op fallback**: If no profitable transformation is found, return `pGia` unchanged with `fChanged = 0`.

### Memory

- Free all temporary `Vec_*`, hash tables, simulation buffers, and temporary `Gia_Man_t` pointers.
- If the algorithm returns a new `Gia_Man_t` (fChanged=1), the caller takes ownership.
- If unchanged (fChanged=0), the caller expects the same pointer back (or a new one that will be freed).
- Reset GIA marks, traversal IDs, and `Value` fields when reused.

### Profitability

- Primary metric: AIG node count reduction.
- Secondary: AIG level impact.
- Do not accept severe level growth without benchmark justification.
- `pRes->fChanged` must accurately reflect whether optimization was applied.

### Bounds

All search must be bounded by parameters in `Mosh_Par_t`:
- `nMaxWindowSize`: max nodes in a local window
- `nMaxCutSize`: max leaves in a cut
- `nMaxCandidates`: max candidate count

Add additional bounds via new fields in `Mosh_Par_t` if needed.

### Determinism

- Randomization is allowed only with `RandomSeed` and deterministic fallback.
- No benchmark-specific or case-name-specific heuristics.

## Command Reference

```
moshare -algo <name> [-maxwin <n>] [-maxcut <n>] [-maxcand <n>] [-seed <n>] [-stats] [-v] [-h]
```

| Flag | Type | Default | Description |
|---|---|---|---|
| `-algo <name>` | string | required | Algorithm selection (`-algo list` to see choices) |
| `-maxwin <n>` | int | 64 | Max window size |
| `-maxcut <n>` | int | 8 | Max cut size |
| `-maxcand <n>` | int | 100 | Max candidates |
| `-seed <n>` | int | 0 | Random seed |
| `-stats` | flag | off | Print detailed statistics |
| `-v` | flag | off | Verbose output |
| `-h` | flag | — | Print help |

## Batch Mode Examples

```bash
# List available algorithms
./abc -c "moshare -algo list"

# No-op (safety net)
./abc -c "read_blif input.blif; strash; moshare -algo none -stats"

# Run a specific algorithm
./abc -c "read_blif input.blif; strash; moshare -algo xxx -maxwin 128 -maxcut 10 -stats"

# Full optimization flow with CEC
./abc -c "read_blif input.blif; strash; print_stats; moshare -algo xxx -stats; print_stats; write_blif output.blif"
./abc -c "cec input.blif output.blif"
```

## File History

- 2026-05-10: Initial integration layer created (`moshare` command, `none` algorithm, `list` pseudo-algorithm).
