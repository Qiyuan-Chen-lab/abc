# TODO: Implement SBM Boolean-Difference Sharing in `moshare`

## Goal

Implement a conservative contest-oriented variant of the paper method from **"Scalable Boolean Methods in a Modern Synthesis Flow"** inside the existing `moshare` framework.

The first real algorithm should be:

```text
moshare -algo bdiff
```

It should attempt multi-output AIG sharing using Boolean-difference resubstitution:

```text
f = (f xor g) xor g
```

where `g` is existing logic and `(f xor g)` is either already represented in the network or can be implemented cheaply. The first implementation should prioritize correctness, bounded runtime, and CEC-clean output over aggressive QoR.

## Existing Integration Status

Claude Code has already implemented the `moshare` integration layer from:

```text
codex_doc/todo_moshare_integration_layer.md
```

Current files:

```text
src/opt/moshare/moshare.h
src/opt/moshare/moshare.c
src/opt/moshare/moshareUtil.c
src/opt/moshare/moshareAlgoNone.c
src/opt/moshare/module.make
src/base/abci/abcMoshare.c
```

Current interface assumptions:

- `MOSH_ALGO_COUNT` is currently `1`.
- `MOSH_ALGO_NONE` is currently the only algorithm.
- `moshare -algo list` prints available algorithms.
- `moshare -algo none -stats` runs a no-op and reports GIA AND/level stats.
- The command wrapper already parses:
  - `-algo <name>`
  - `-maxwin <n>`
  - `-maxcut <n>`
  - `-maxcand <n>`
  - `-seed <n>`
  - `-stats`
  - `-v`
  - `-h`
- The command wrapper obtains a `Gia_Man_t` from the ABC frame or converts a strashed `Abc_Ntk_t` to GIA.

Do not reimplement the integration layer. Extend it.

## Contest Constraint Summary

- Preserve functional equivalence.
- Preserve CI/CO ordering exactly.
- Leave the network unchanged when unsupported, unprofitable, or resource-limited.
- Prioritize AIG AND-node reduction.
- Do not accept severe level growth.
- Keep runtime bounded on all public cases.
- Do not use benchmark-specific names or public-case special handling.
- Do not modify `tc_public/`.
- Store temporary outputs under `claude_run/`.
- Store durable benchmark summaries under `claude_doc/`.
- Keep planning/handoff notes under `codex_doc/`.
- Avoid root-level generated CSV/log/temp files.

## Why This Targets True Multi-Output Sharing

The target transformation should select candidate pairs from a partition that may cover multiple CO cones. If `f` and `g` are in different output cones but have related support or similar functions, then rewriting `f` through `g` can force shared reuse across outputs.

The implementation should therefore prefer candidate pairs with at least one of:

- `f` and `g` feed different CO reachability sets.
- `f` and `g` have overlapping support but are not in the same immediate local MFFC.
- `g` is already reused by multiple outputs.
- `(f xor g)` is already represented by another node shared outside the MFFC of `f`.

## Recommended Implementation Shape

Add the first real algorithm as isolated files:

```text
src/opt/moshare/moshareAlgoBdiff.c
src/opt/moshare/moshareBdd.c
src/opt/moshare/mosharePart.c
```

If the first implementation can stay smaller, it is acceptable to start with:

```text
src/opt/moshare/moshareAlgoBdiff.c
```

and split BDD/partition helpers later. Do not bury the algorithm inside `moshare.c` or `moshareUtil.c`.

## Phase 0: Pre-Implementation Checks

- [ ] Read `CLAUDE.md`.
- [ ] Read `CODEX.md`.
- [ ] Read `claude_doc/Contest.md`.
- [ ] Read `claude_doc/Algorithm.md`.
- [ ] Read `claude_doc/Architect.md`.
- [ ] Read `claude_doc/Run.md`.
- [ ] Read `codex_doc/scalable_boolean_methods/sbm_contest_fit_analysis.md`.
- [ ] Inspect current `moshare` files listed above.
- [ ] Run `git status --short` and note existing user/Claude changes.
- [ ] Confirm `moshare -algo none` still builds and runs before adding `bdiff`.

Suggested smoke commands before editing:

```bash
make
./abc -c "moshare -h"
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo none -stats; print_stats"
./abc -c "read_blif tc_public/tc_public_25/input.blif; strash; print_stats; moshare -algo none -stats; print_stats"
```

If `make` fails due to readline, try:

```bash
make ABC_USE_NO_READLINE=1
```

## Phase 1: Register the `bdiff` Algorithm

Update `src/opt/moshare/moshare.h`:

- [ ] Add a new enum value:

```c
MOSH_ALGO_BDIFF = 1
```

- [ ] Update `MOSH_ALGO_COUNT` from `1` to `2`.
- [ ] Add a prototype:

```c
Gia_Man_t * Mosh_ManPerformAlgoBdiff( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

Update `src/opt/moshare/moshareUtil.c`:

- [ ] Map `"bdiff"` to `MOSH_ALGO_BDIFF`.
- [ ] Map `MOSH_ALGO_BDIFF` back to `"bdiff"`.
- [ ] Ensure `moshare -algo list` prints both `none` and `bdiff`.

Update `src/opt/moshare/moshare.c`:

- [ ] Dispatch `MOSH_ALGO_BDIFF` to `Mosh_ManPerformAlgoBdiff`.
- [ ] Keep the unknown algorithm fallback unchanged.

Update `src/opt/moshare/module.make`:

- [ ] Add `src/opt/moshare/moshareAlgoBdiff.c`.
- [ ] Add helper files only when they exist.

Create `src/opt/moshare/moshareAlgoBdiff.c`:

- [ ] Start with a stats-only implementation that returns no change.
- [ ] Match ABC file header/comment style used by existing `moshare` files.
- [ ] Use `Mosh_ResClear`.
- [ ] Fill `nNodesBefore`, `nLevelsBefore`, `nNodesAfter`, `nLevelsAfter`.
- [ ] Set `fChanged = 0`.
- [ ] Print a clear stats line under `pPar->fStats`.

Validation after Phase 1:

```bash
make
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -stats"
./abc -c "read_blif tc_public/tc_public_25/input.blif; strash; moshare -algo bdiff -stats"
```

Pass criteria:

- Build succeeds.
- `moshare -algo list` contains `bdiff`.
- `moshare -algo bdiff -stats` runs without modifying the network.

## Phase 2: Extend Parameters Conservatively

The existing command already has useful bounds:

- `nMaxWindowSize`: use as maximum partition/window node count.
- `nMaxCutSize`: use as maximum local support size.
- `nMaxCandidates`: use as maximum candidate pairs per run or per partition.
- `RandomSeed`: use for deterministic simulation ordering if needed.

Add only the parameters needed for safe Boolean-difference search:

Update `Mosh_Par_t`:

- [ ] Add `int nMaxBddNodes;`
- [ ] Add `int nMaxLevelGrowth;`
- [ ] Optional: add `int fDryRun;`

Update `Mosh_ParDefault`:

- [ ] Set `nMaxBddNodes = 16`.
- [ ] Set `nMaxLevelGrowth = 0`.
- [ ] Set `fDryRun = 0` if added.

Update `abcMoshare.c` parser:

- [ ] Add `-maxbdd <n>`.
- [ ] Add `-maxgrow <n>`.
- [ ] Optional: add `-dryrun`.
- [ ] Reject negative values.
- [ ] Update usage text.

Do not add many knobs at once. Keep the first user-facing command stable:

```text
moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 1000 -maxbdd 16 -maxgrow 0 -stats
```

## Concrete Implementation Blueprint

This section is intentionally prescriptive. Follow this implementation order before attempting more ambitious versions of the paper algorithm.

### Version A: Existing-Diff Boolean-Difference Only

The first real version should only implement this pattern:

```text
given existing nodes f, g, h
if func(h) == func(f) xor func(g)
then replace f by h xor g
```

Do not synthesize arbitrary `f xor g` BDDs in Version A. This keeps the first patch small and reduces the chance of creating an incorrect BDD-to-AIG converter.

Required properties:

- `f`, `g`, and `h` are existing AND nodes or supported literals in the original GIA.
- `g` is topologically before `f`.
- `h` is topologically before `f`.
- `h` is not in the MFFC that will be removed with `f`.
- Replacement is built as one hashed XOR:

```c
iLitNewF = Gia_ManHashXor( pNew, iLitHCopy, iLitGCopy );
```

This is a conservative subset of the paper method, but it is enough to test whether public designs contain reusable Boolean differences.

### Recommended Local Types

Keep these types private in `moshareAlgoBdiff.c` at first:

```c
typedef struct Mosh_BdiffCand_t_ {
    int iF;
    int iG;
    int iH;
    int nMffcF;
    int nDiffBddSize;
    int nGainEst;
    int nGainActual;
    int nLevelGrowth;
} Mosh_BdiffCand_t;

typedef struct Mosh_BdiffStats_t_ {
    int nPartitions;
    int nWindowsSkipped;
    int nCandidatesTried;
    int nCandidatesWithDiff;
    int nRejectedSupport;
    int nRejectedTopo;
    int nRejectedBdd;
    int nRejectedProfit;
    int nRejectedLevel;
    int nAccepted;
} Mosh_BdiffStats_t;
```

Use local `Vec_Int_t *` containers for windows, support, and candidate node IDs. Do not add public structs until another file genuinely needs them.

### Recommended Function Order

Implement functions in this order. Compile and smoke test after each group.

1. Entry and stats:

```c
Gia_Man_t * Mosh_ManPerformAlgoBdiff( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
static void Mosh_BdiffStatsClear( Mosh_BdiffStats_t * p );
static void Mosh_BdiffStatsPrint( Mosh_BdiffStats_t * p, Mosh_Res_t * pRes );
```

2. Window collection:

```c
static int Mosh_BdiffCollectWindow( Gia_Man_t * pGia, int iCo, Mosh_Par_t * pPar, Vec_Int_t * vNodes, Vec_Int_t * vSupp );
static void Mosh_BdiffCollectTfi_rec( Gia_Man_t * pGia, int iObj, Mosh_Par_t * pPar, Vec_Int_t * vNodes, Vec_Int_t * vSupp, int * pAbort );
```

3. Candidate search:

```c
static int Mosh_BdiffFindBestInWindow( Gia_Man_t * pGia, Mosh_Par_t * pPar, Vec_Int_t * vNodes, Vec_Int_t * vSupp, Mosh_BdiffCand_t * pBest, Mosh_BdiffStats_t * pStats );
static int Mosh_BdiffEvalPairExistingDiff( Gia_Man_t * pGia, Mosh_Par_t * pPar, Vec_Int_t * vNodes, Vec_Int_t * vSupp, int iF, int iG, Mosh_BdiffCand_t * pCand, Mosh_BdiffStats_t * pStats );
```

4. BDD construction:

```c
static int Mosh_BdiffBuildWindowBdds( Gia_Man_t * pGia, Vec_Int_t * vNodes, Vec_Int_t * vSupp, DdManager ** pdd, Vec_Ptr_t ** pvBdds );
static void Mosh_BdiffFreeWindowBdds( DdManager * dd, Vec_Ptr_t * vBdds );
static DdNode * Mosh_BdiffLookupBdd( Gia_Man_t * pGia, Vec_Int_t * vNodes, Vec_Ptr_t * vBdds, int iObj );
static int Mosh_BdiffFindExistingBddNode( Gia_Man_t * pGia, Vec_Int_t * vNodes, Vec_Ptr_t * vBdds, DdNode * bDiff, int iF );
```

5. Rebuild and exact acceptance:

```c
static Gia_Man_t * Mosh_BdiffTryApplyCandidate( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_BdiffCand_t * pCand, Mosh_BdiffStats_t * pStats );
static Gia_Man_t * Mosh_BdiffRebuildWithOneReplacement( Gia_Man_t * pGia, Mosh_BdiffCand_t * pCand );
```

It is acceptable to merge small helpers, but keep this dependency direction: collect windows first, evaluate candidates second, rebuild last.

### Entry Function Pseudocode

`Mosh_ManPerformAlgoBdiff` should follow this structure:

```text
clear result and local stats
record nodes/levels before
if pGia is NULL: return NULL
if any bound is zero: print stats and return pGia unchanged

call Gia_ManCreateRefs(pGia) before using Gia_NodeMffcSize

best = empty candidate
for each CO index in original order:
    clear vNodes and vSupp
    if CollectWindow fails or aborts:
        stats.nWindowsSkipped++
        continue
    if FindBestInWindow finds a better candidate:
        update best
    if stats.nCandidatesTried >= pPar->nMaxCandidates:
        break

if no candidate:
    record after = before
    print stats
    free temp vectors
    return pGia

pNew = TryApplyCandidate(pGia, best)
if pNew == NULL:
    record after = before
    print stats
    free temp vectors
    return pGia

pClean = Gia_ManCleanup(pNew)
free pNew if cleanup returns a different manager
measure actual nodes/levels
if actual nodes improve and level growth <= bound:
    pRes->fChanged = 1
    pRes->nApplied = 1
    return pClean
else:
    free pClean
    return pGia unchanged
```

Do not update the ABC frame in this function. The existing command wrapper owns that responsibility based on `pRes->fChanged`.

### Window Collection Rules

Version A should collect a small TFI window rooted at one CO driver.

Implementation guidance:

- Use traversal IDs or a local mark vector to avoid duplicate nodes.
- Include only AND nodes in `vNodes`.
- Include CIs and boundary nodes in `vSupp`.
- If support exceeds `nMaxCutSize`, abort this window and return 0.
- If `vNodes` exceeds `nMaxWindowSize`, abort this window and return 0.
- Sort `vNodes` by object ID before candidate enumeration so BDD construction and pair order are deterministic.

Practical recursion behavior:

```text
CollectTfi(obj):
    if abort: return
    if obj is const: return
    if obj is CI:
        add CI id to support if not present
        abort if support too large
        return
    if obj is AND:
        recursively collect fanin0
        recursively collect fanin1
        add obj id to vNodes after fanins
        abort if window too large
```

If a fanin is outside the current window because limits would be exceeded, treat it as a boundary support leaf rather than forcing recursion. If this becomes hard to implement correctly, choose the simpler safe behavior: abort the whole window.

### BDD Construction Rules

Build one BDD manager for one window.

Detailed steps:

1. Create a support index mapping:

```text
support object id -> BDD variable index
```

For the first implementation, a simple linear lookup over `vSupp` is acceptable because `nMaxCutSize` is small.

2. Allocate BDD storage sized by `Gia_ManObjNum(pGia)` or by `Vec_IntSize(vNodes)`:

- Simpler but larger: `Vec_PtrStart( Gia_ManObjNum(pGia) )`.
- More compact: parallel `vNodes`/`vBdds` plus lookup helper.

Prefer simple and correct first.

3. Initialize constants and support leaves:

- Const0 maps to `Cudd_ReadLogicZero(dd)`.
- Each support leaf maps to `Cudd_bddIthVar(dd, index)`.
- Remember to `Cudd_Ref` every BDD stored in a vector.

4. For each node in `vNodes` topological order:

```text
b0 = BDD of fanin0, complemented if fanin0 edge is complemented
b1 = BDD of fanin1, complemented if fanin1 edge is complemented
bNode = Cudd_bddAnd(dd, b0, b1)
Cudd_Ref(bNode)
store bNode
```

5. Bail out safely when:

- Any BDD operation returns `NULL`.
- `Cudd_ReadNodeCount(dd)` exceeds a conservative internal cap.
- `Cudd_DagSize(bNode)` exceeds a practical local cap for too many nodes.

6. Freeing:

- For every stored BDD, call `Cudd_RecursiveDeref(dd, bNode)`.
- Then call `Cudd_Quit(dd)`.

Do not reuse BDD pointers after `Cudd_Quit`.

### Existing-Diff Candidate Evaluation

For each `(f, g)`:

```text
if iG >= iF: reject topo
if MFFC(f) <= estimated xor cost: reject profit
if f or g BDD is missing: reject BDD
bDiff = Cudd_bddXor(dd, bF, bG)
if bDiff is NULL: reject BDD
Cudd_Ref(bDiff)
if Cudd_DagSize(bDiff) > nMaxBddNodes: reject BDD
iH = find existing node whose BDD pointer equals bDiff or Cudd_Not(bDiff)
if no iH: reject BDD
if iH >= iF: reject topo
if iH == iF or iH == iG: reject topo
estimate gain
record best candidate
Cudd_RecursiveDeref(dd, bDiff)
```

Complement handling for `h`:

- If the matching BDD is `bDiff`, replacement uses `h xor g`.
- If the matching BDD is `not bDiff`, replacement uses `not(h) xor g`.
- Add a candidate field later if needed:

```c
int fComplH;
```

If complement handling feels risky in the first patch, reject complemented `h` matches and add support later. This is less powerful but safer.

### Rebuild Rules for One Replacement

Rebuild exactly once for the chosen best candidate.

Pseudocode:

```text
pNew = Gia_ManStart( Gia_ManObjNum(pGia) + 10 )
copy pName/pSpec if local style does this
append all CIs in original order; set CI Value fields
Gia_ManHashStart(pNew)

for each object in pGia in topological order:
    if object is const or CI: continue
    if object is AND:
        if object id == iF:
            litG = copied literal of original g
            litH = copied literal of original h, complemented if needed
            obj->Value = Gia_ManHashXor(pNew, litH, litG)
        else:
            obj->Value = Gia_ManHashAnd(pNew, copied fanin0 lit, copied fanin1 lit)

for each CO in original order:
    append copied CO fanin literal

Gia_ManHashStop(pNew)
return pNew
```

Safety checks before applying:

- `iG < iF`.
- `iH < iF`.
- `Gia_ObjIsAnd(Gia_ManObj(pGia, iF))`.
- `Gia_ObjIsAnd(Gia_ManObj(pGia, iG))` unless support-literal support is explicitly added.
- `Gia_ObjIsAnd(Gia_ManObj(pGia, iH))` unless support-literal support is explicitly added.
- Replacement literal is valid before any CO uses it.

### What Not To Implement in the First Patch

Avoid these until Version A is CEC-clean:

- Arbitrary BDD-to-AIG synthesis.
- Multiple replacements per run.
- Randomized candidate order.
- Cross-window global BDD managers.
- Global mutable caches.
- Zero-gain reshaping.
- Direct in-place mutation of `pGia`.
- Root-level CSV/log generation.

## Phase 3: Candidate and Partition Infrastructure

Implement bounded candidate discovery before implementing transformations.

### 3.1 Required Helper Data

- [ ] Compute current AIG stats:
  - `Gia_ManAndNum(pGia)`
  - `Gia_ManLevelNum(pGia)`
- [ ] Ensure levels are available before level-based filtering:
  - Use existing GIA level APIs.
  - Recompute levels if needed.
- [ ] Use `Gia_NodeMffcSize(pGia, pObj)` for the target `f` MFFC estimate.
- [ ] Use `Gia_ObjCheckMffc` only if actual MFFC nodes/leaves are needed.
- [ ] Avoid storing state in globals.
- [ ] Free all vectors on every return path.

### 3.2 Initial Partition Strategy

Start simple and deterministic.

- [ ] Iterate COs in CO order.
- [ ] For each CO, collect a bounded TFI window in reverse topological order.
- [ ] Stop collecting when either:
  - window nodes exceed `nMaxWindowSize`, or
  - support leaves exceed `nMaxCutSize`.
- [ ] Keep only AND nodes as internal candidate nodes.
- [ ] Deduplicate nodes if partitions overlap.
- [ ] Optionally merge nearby CO partitions only after single-CO partitions are stable.

The first implementation can use overlapping windows. This is acceptable because candidate count and runtime are bounded.

### 3.3 Multi-Output Preference

Add a cheap PO-reachability signal when feasible.

Suggested implementation:

- [ ] Compute a bitset of reachable COs for each node, or compute a cheaper approximate output reference count.
- [ ] If bitsets are implemented, use ABC vector/word containers. Avoid custom heap-heavy structures.
- [ ] Prefer pairs where `f` and `g` have different but overlapping or related CO reachability.
- [ ] Do not require the multi-output filter initially if it blocks all candidates. Instead, rank candidates with it.

### 3.4 Candidate Pair Filtering

For each partition:

- [ ] Consider ordered pairs `(f, g)` where both are AND nodes.
- [ ] Require `f != g`.
- [ ] Require `Gia_ObjId(g) < Gia_ObjId(f)` for the first version, so `g` is topologically available when replacing `f`.
- [ ] Skip candidates where `Gia_NodeMffcSize(pGia, f) <= 1`.
- [ ] Skip candidates if estimated `xor_cost >= MFFC_size(f)`.
- [ ] Skip candidates whose support union exceeds `nMaxCutSize`.
- [ ] Skip candidates with no support overlap.
- [ ] Bound total tried pairs by `nMaxCandidates`.
- [ ] Keep deterministic ordering:
  - partition order by CO index,
  - target `f` by topological order,
  - helper `g` by topological order or score.

Under `-stats`, report:

```text
moshare (algo bdiff): partitions = ...
moshare (algo bdiff): candidates tried = ...
moshare (algo bdiff): candidates accepted = ...
moshare (algo bdiff): reject_support = ...
moshare (algo bdiff): reject_bdd = ...
moshare (algo bdiff): reject_profit = ...
moshare (algo bdiff): reject_level = ...
```

Validation after Phase 3:

```bash
make
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 1000 -stats -v"
./abc -c "read_blif tc_public/tc_public_12/input.blif; strash; moshare -algo bdiff -maxwin 300 -maxcut 12 -maxcand 2000 -stats"
```

Pass criteria:

- No crash.
- Candidate stats are printed.
- Network remains unchanged until replacement is implemented.

## Phase 4: BDD-Based Boolean-Difference Evaluation

This phase should evaluate candidate pairs exactly within a small support/window. Do not update the network yet.

### 4.1 BDD Scope

Use BDDs only inside bounded partitions:

- [ ] Create one CUDD manager per partition or per small candidate batch.
- [ ] Use at most `nMaxCutSize` BDD variables.
- [ ] Map partition support leaves to BDD variables.
- [ ] Compute BDDs for all partition nodes in topological order.
- [ ] Use `Cudd_Ref` and `Cudd_RecursiveDeref` consistently.
- [ ] Ensure every referenced BDD is dereferenced before `Cudd_Quit`.
- [ ] Bail out if BDD node count exceeds `nMaxBddNodes` or a conservative manager limit.

Useful existing includes/patterns:

- `bdd/extrab/extraBdd.h`
- `src/aig/gia/giaClp.c`
- `src/base/abci/abcMulti.c`
- `src/base/abci/abcCollapse.c`

### 4.2 Difference Computation

For each candidate `(f, g)`:

- [ ] Retrieve `bdd_f`.
- [ ] Retrieve `bdd_g`.
- [ ] Compute:

```text
bdd_diff = bdd_f xor bdd_g
```

- [ ] Reject if `bdd_diff` is constant unless it gives an obvious safe simplification.
- [ ] Reject if `Cudd_DagSize(bdd_diff) > nMaxBddNodes`.
- [ ] Check whether `bdd_diff` matches an existing partition node BDD.

### 4.3 First Version: Existing-Diff Only

For the first transforming version, do not synthesize arbitrary BDDs into AIG yet.

Only accept a candidate if:

- [ ] `bdd_diff` is functionally equal to an existing node `h` in the same partition.
- [ ] `h` is topologically before `f`.
- [ ] `h` is not the same object as `f`.
- [ ] The replacement can be implemented as:

```text
f_new = h xor g
```

This makes implementation simpler:

- No BDD-to-AIG synthesis is needed.
- The only new logic is `Gia_ManHashXor(pNew, h_lit, g_lit)`.
- Cleanup can remove the old MFFC of `f` when profitable.

### 4.4 Later Version: Small BDD-to-AIG Synthesis

Only after existing-diff mode is correct and benchmarked, add small BDD synthesis:

- [ ] Convert `bdd_diff` to an AIG using Shannon recursion.
- [ ] Use `Gia_ManHashMux(pNew, var_lit, then_lit, else_lit)` when implementing BDD nodes.
- [ ] Memoize BDD node to GIA literal mappings inside one manager.
- [ ] Abort if synthesized AND count exceeds `nMaxBddNodes` or profitability budget.
- [ ] Reuse existing nodes through hashing whenever possible.

Do not implement Phase 4.4 before Phase 4.3 passes CEC on public cases.

## Phase 5: Profitability and Level Checks

Before accepting a replacement, estimate profitability:

```text
old_cost = Gia_NodeMffcSize(pGia, f)
new_cost = xor_cost + optional_diff_cost
gain_est = old_cost - new_cost
```

For existing-diff mode:

- `optional_diff_cost = 0`.
- `xor_cost` can be estimated as 1 to 3 ANDs, but the final check must use actual rebuilt/cleaned AIG size.

Acceptance rules for the first implementation:

- [ ] Require `gain_est > 0`.
- [ ] Rebuild a candidate output `Gia_Man_t`.
- [ ] Run GIA cleanup on the candidate result.
- [ ] Measure actual:
  - `Gia_ManAndNum(pNewClean)`
  - `Gia_ManLevelNum(pNewClean)`
- [ ] Accept only if actual AND count is lower than before.
- [ ] Accept only if level growth is `<= nMaxLevelGrowth`.
- [ ] If rejected, free the candidate manager and leave the original unchanged.

Do not allow zero-gain rewrites in the first implementation. The paper allows zero-cost reshaping, but it increases validation complexity.

## Phase 6: Safe Network Rebuild

Implement accepted replacements by rebuilding the whole `Gia_Man_t` with structural hashing.

Recommended approach:

- [ ] Create `pNew = Gia_ManStart(...)` or local equivalent used in existing GIA duplication code.
- [ ] Append CIs in the original order.
- [ ] Call `Gia_ManHashStart(pNew)`.
- [ ] Iterate objects in original topological order.
- [ ] For each AND object:
  - if this object is the selected target `f`, set `pObj->Value` to the replacement literal.
  - otherwise set `pObj->Value = Gia_ManHashAnd(pNew, Gia_ObjFanin0Copy(pObj), Gia_ObjFanin1Copy(pObj))`.
- [ ] Use `Gia_ManHashXor(pNew, h_lit, g_lit)` for `f_new`.
- [ ] Append COs in original order using copied fanin literals.
- [ ] Call `Gia_ManHashStop(pNew)`.
- [ ] Cleanup the result with `Gia_ManCleanup` or the local cleanup pattern used elsewhere.
- [ ] Preserve the original manager if no accepted replacement exists.

Important safety rules:

- [ ] Do not confuse GIA object IDs with literals.
- [ ] Use `Gia_ObjFanin*C` or `Gia_ObjFanin*Copy` helpers to preserve complemented fanins.
- [ ] Ensure replacement literals refer to objects already copied into `pNew`.
- [ ] Require `g` and `h` to be topologically before `f` in the first implementation.
- [ ] Preserve CI and CO ordering exactly.
- [ ] Do not mutate the input `pGia` in place.

## Phase 7: Iteration Strategy

Start with at most one accepted replacement per `moshare -algo bdiff` command invocation.

- [ ] Search candidates.
- [ ] Accept the best candidate found by estimated or actual gain.
- [ ] Rebuild once.
- [ ] Return the changed manager.

After this is stable:

- [ ] Add `nMaxApplied` only if needed.
- [ ] Consider repeating search on the cleaned result.
- [ ] Stop when no profitable replacement exists or when `nApplied` reaches the limit.

Do not add unbounded loops. Every repeated search must have an explicit cap.

## Phase 8: Stats and Debug Output

Under `-stats`, print concise summary lines:

```text
moshare (algo bdiff): nodes before = ...
moshare (algo bdiff): levels before = ...
moshare (algo bdiff): partitions = ...
moshare (algo bdiff): candidates tried = ...
moshare (algo bdiff): accepted = ...
moshare (algo bdiff): nodes after = ...
moshare (algo bdiff): levels after = ...
moshare (algo bdiff): gain = ...
```

Under `-v`, print candidate-level debug only in bounded amounts:

- [ ] Print the first few accepted/rejected candidate details.
- [ ] Do not print thousands of lines on large cases.
- [ ] Do not create root-level CSV files.

Update `Mosh_Res_t` only if needed:

- [ ] Add `nPartitions`.
- [ ] Add `nRejectedSupport`.
- [ ] Add `nRejectedBdd`.
- [ ] Add `nRejectedProfit`.
- [ ] Add `nRejectedLevel`.

Keep result extensions simple.

## Phase 9: Correctness Validation

Run these after the first transforming implementation.

### 9.1 Basic Command Tests

```bash
make
./abc -c "moshare -h"
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo bdiff -stats; print_stats"
./abc -c "read_blif tc_public/tc_public_25/input.blif; strash; print_stats; moshare -algo bdiff -stats; print_stats"
```

### 9.2 Write BLIF and CEC

Use outputs under `claude_run/`:

```bash
mkdir -p claude_run
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -stats; write_blif claude_run/moshare_bdiff_tc02.blif"
./abc -c "cec tc_public/tc_public_2/input.blif claude_run/moshare_bdiff_tc02.blif"
```

Also test a case where no change is likely:

```bash
./abc -c "read_blif tc_public/tc_public_5/input.blif; strash; moshare -algo bdiff -stats; write_blif claude_run/moshare_bdiff_tc05.blif"
./abc -c "cec tc_public/tc_public_5/input.blif claude_run/moshare_bdiff_tc05.blif"
```

Pass criteria:

- No crash.
- If no profitable candidate exists, output remains equivalent.
- If a candidate is accepted, `cec` reports equivalence.
- CI/CO counts are unchanged.
- CO ordering is preserved by construction and confirmed by CEC.

### 9.3 Parameter Stress Tests

```bash
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -maxwin 0 -maxcut 0 -maxcand 0 -stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -maxwin 50 -maxcut 8 -maxcand 10 -maxbdd 8 -stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo __invalid_algo__"
```

Pass criteria:

- Zero limits should no-op cleanly.
- Small limits should not crash.
- Invalid algorithm handling remains clean.

## Phase 10: Benchmark Plan

Start with public cases where the current ABC standard reference flow has low improvement:

```text
tc_public_5
tc_public_7
tc_public_8
tc_public_9
tc_public_10
tc_public_12
tc_public_15
tc_public_16
tc_public_17
tc_public_18
tc_public_19
tc_public_20
tc_public_21
tc_public_22
tc_public_23
tc_public_24
tc_public_26
```

Always include at least one small easy case:

```text
tc_public_2
tc_public_25
```

Recommended first benchmark flow:

```text
read_blif <input>; strash; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; b; moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 1000 -maxbdd 16 -maxgrow 0 -stats; b; print_stats; write_blif <output>
```

Also test `moshare` before the standard ABC cleanup flow:

```text
read_blif <input>; strash; moshare -algo bdiff -maxwin 200 -maxcut 12 -maxcand 1000 -maxbdd 16 -maxgrow 0 -stats; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; b; print_stats; write_blif <output>
```

For each case, record in `claude_doc/`:

- Exact command line.
- Initial `strash` AND count and level.
- ABC standard reference-flow AND count and level.
- `bdiff`-flow AND count and level.
- Runtime from `/usr/bin/time -v`.
- Peak memory from `/usr/bin/time -v`.
- CEC result.
- Number of candidates tried.
- Number of accepted replacements.

Suggested script location:

```text
claude_scripts/run_moshare_bdiff.py
```

Suggested report location:

```text
claude_doc/moshare_bdiff_report.md
claude_doc/moshare_bdiff_results.csv
```

Do not write reports or CSV files to the repository root.

## Phase 11: Acceptance Criteria

The implementation is acceptable for first contest experimentation when:

- [ ] ABC builds successfully.
- [ ] `moshare -algo list` includes `bdiff`.
- [ ] `moshare -algo bdiff` runs from ABC batch mode.
- [ ] No-op fallback works when bounds are zero or no profitable candidate exists.
- [ ] At least one small public case runs without crash.
- [ ] Every written optimized BLIF passes `cec`.
- [ ] The command never modifies `tc_public/`.
- [ ] Runtime remains bounded by `nMaxCandidates`, `nMaxWindowSize`, `nMaxCutSize`, and `nMaxBddNodes`.
- [ ] `-stats` reports useful candidate and result counts.
- [ ] Existing `moshare -algo none` behavior is unchanged.
- [ ] Existing common ABC flows still run.

QoR acceptance for keeping the algorithm:

- [ ] It improves AND count on at least one public case without increasing level.
- [ ] Or it opens a downstream improvement when followed by the ABC standard reference flow.
- [ ] If it does not improve any case, keep the isolated code only if it is useful for the next BDD-synthesis phase; otherwise disable it by default or keep it as an experimental option.

## Phase 12: Follow-Up Enhancements Only After CEC-Clean Prototype

Do not start these before the existing-diff version passes CEC:

- [ ] Add small BDD-to-AIG synthesis for `bdd_diff`.
- [ ] Add repeated application with `nMaxApplied`.
- [ ] Add output-group partitioning instead of single-CO windows.
- [ ] Add simulation-assisted candidate ranking.
- [ ] Add a level-aware score:

```text
score = 1000 * and_gain - 100 * level_growth - runtime_penalty
```

- [ ] Add a flow controller that tries `bdiff` before/after rewrite/refactor and keeps the best CEC-clean output.
- [ ] Consider a later BDD-MSPF algorithm as a separate `moshare` variant, not inside `bdiff`.

## Known Risks

- BDD memory blowup if support bounds are not enforced.
- Incorrect replacement if `g` or `h` is not topologically available before `f`.
- Literal/object-ID confusion in GIA.
- Complement handling mistakes in copied fanins.
- XOR reconstruction may increase levels.
- Candidate enumeration can become quadratic without `nMaxCandidates`.
- Existing `abcOrchestration.c` writes root-level CSV files in some paths; do not copy that behavior.
- Some public cases may not contain existing `(f xor g)` nodes, so Phase 4.3 may have limited QoR until BDD-to-AIG synthesis is added.

## Final Response Requirements for Claude Code

When implementation is complete, report in Chinese:

- Files changed.
- Build command and result.
- Smoke commands and results.
- CEC commands and results.
- Before/after node and level metrics.
- Runtime and memory if benchmarked.
- Public cases that improved, regressed, or no-oped.
- Remaining risks and next recommended phase.
