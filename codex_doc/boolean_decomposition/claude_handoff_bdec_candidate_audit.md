# Claude Code Handoff: BDec Candidate Audit in Moshare

## Goal

Add a new `moshare` algorithm variant:

```text
moshare -algo bdec
```

This first version is a **candidate-audit implementation only**. It should not implement the full paper method and should not replace logic. Its job is to measure whether public contest cases, especially stubborn cases, contain small multi-output windows with promising two-literal Boolean decomposition divisors.

The implementation should follow:

- `CODEX.md`
- `CLAUDE.md`
- `claude_doc/Contest.md`
- `claude_doc/Algorithm.md`
- `claude_doc/Architect.md`
- `claude_doc/Run.md`
- `claude_doc/moshare_integration_guide.md`
- `codex_doc/boolean_decomposition/boolean_decomposition_contest_fit_analysis.md`

## Contest Constraint Summary

- Preserve correctness first: no crash, no unintended network mutation, CEC-clean written outputs.
- Keep this version no-op: return the original `Gia_Man_t *` with `fChanged = 0`.
- Target true multi-output sharing in candidate discovery: useful candidates should be associated with at least two local outputs or cross-CO structure.
- Keep the implementation bounded by leaves, window size, candidate count, and printed-candidate count.
- Do not introduce external dependencies.
- Do not modify existing ABC commands or existing `moshare` algorithms except for registry integration.
- Do not modify files under `tc_public/`.
- Do not write temporary or durable artifacts to the repository root.
- Use `claude_run/` for temporary generated BLIF/log artifacts.
- Use `claude_doc/` for durable benchmark reports and CSV summaries.
- Write all code comments, reports, scripts, and generated documents in English.

## Files to Inspect

- `CODEX.md`
- `CLAUDE.md`
- `claude_doc/Contest.md`
- `claude_doc/Algorithm.md`
- `claude_doc/Architect.md`
- `claude_doc/Run.md`
- `claude_doc/moshare_integration_guide.md`
- `codex_doc/boolean_decomposition/boolean_decomposition_contest_fit_analysis.md`
- `src/opt/moshare/moshare.h`
- `src/opt/moshare/moshare.c`
- `src/opt/moshare/moshareUtil.c`
- `src/opt/moshare/moshareAlgoNone.c`
- `src/opt/moshare/moshareAlgoBdiff.c`
- `src/opt/moshare/module.make`
- `src/base/abci/abcMoshare.c`
- `src/aig/gia/gia.h`
- Existing small truth-table helper examples under `src/misc/util/`, `src/bool/kit/`, and nearby optimization code.

## Files to Modify

Required:

- `src/opt/moshare/moshare.h`
- `src/opt/moshare/moshare.c`
- `src/opt/moshare/moshareUtil.c`
- `src/opt/moshare/module.make`
- `src/opt/moshare/moshareAlgoBdec.c`

Optional only if needed:

- `src/base/abci/abcMoshare.c`

Avoid changing `abcMoshare.c` in the first pass unless a small new flag is genuinely needed. Prefer reusing existing parameters:

- `nMaxCutSize`: maximum leaves, default for `bdec` should be interpreted as 6 when the user passes `-maxcut 6`.
- `nMaxWindowSize`: maximum internal window nodes.
- `nMaxCandidates`: maximum candidate divisors processed or reported.
- `fStats`: aggregate stats.
- `fVerbose`: top candidate details.

Do not modify:

- `src/base/abci/abc.c`
- `src/base/abci/module.make`
- `Makefile`
- Existing rewrite/refactor/resub implementation files.
- `tc_public/`.

## Required Behavior

The command:

```text
moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats [-v]
```

must:

- Run from ABC batch mode after `strash`.
- Build bounded multi-output windows.
- Skip windows with more than 6 leaves for Version 0.
- Compute exact 64-bit truth tables for local window outputs.
- Enumerate two-literal divisors over local leaves.
- Track AND and OR divisor forms with literal polarities.
- Deduplicate equivalent divisor truth tables within each window.
- Report aggregate candidate statistics under `-stats`.
- Report capped top candidate/window details under `-v`.
- Leave the network unchanged.
- Preserve existing `moshare -algo none` and `moshare -algo bdiff` behavior.

Version 0 must not:

- Implement recursive Boolean decomposition.
- Implement Boolean division.
- Call a two-level minimizer.
- Synthesize divisor-based replacement logic.
- Update the current ABC frame with a new GIA.
- Claim actual AIG node gain.

## Candidate Bounds

Initial expected command bounds:

```text
moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats
```

Interpretation:

- `nMaxCutSize <= 6`: compute truth tables; skip larger windows.
- `nMaxWindowSize`: maximum internal nodes per window.
- `nMaxCandidates`: maximum divisor candidates processed per window or per run. Pick one interpretation and print it clearly.
- `fVerbose`: print only a capped number of best windows/divisors.

If a printed-top-candidate cap is needed, add a local compile-time constant inside `moshareAlgoBdec.c` first, for example:

```c
#define MOSH_BDEC_TOP_PRINT 20
```

Do not add public flags for `nMaxOutputs` or `nTopDivisors` in Version 0 unless the implementation becomes unusable without them.

## Profitability Model

Version 0 is no-op and has no transform profitability decision.

Use audit-only ranking proxies:

```text
score = 10 * multi_output_hits
      +  3 * existing_internal_match
      +  2 * support_overlap_hits
      -      divisor_cost
```

Where:

- `multi_output_hits`: number of local outputs where the divisor appears relevant.
- `existing_internal_match`: 1 if the divisor truth table already appears as an internal window node, otherwise 0.
- `support_overlap_hits`: number of local outputs whose support contains both divisor variables.
- `divisor_cost`: normally 1 for a two-input AND/OR implemented as one AIG AND with possible complemented edges.

This score is for sorting and reporting only. Do not use it to modify the network.

Future replacement versions must verify actual global AND-node reduction after rebuild and cleanup. The previous `bdiff` experiment showed that estimate-only profitability can be misleading.

## Technical Implementation Blueprint

This section is intentionally more concrete than the high-level fit analysis. Follow it unless local ABC APIs make a small adjustment clearly better.

The subsections are aligned with the implementation steps below. Claude Code should implement and validate one step at a time instead of trying to build the whole audit path in one patch.

### Technical Step 0: Scope Reduction from the Paper

The paper's full flow is:

```text
for each node:
    enumerate K-cuts
    derive one KL-cut from each K-cut
    treat KL-cut outputs as multi-output functions over the same K leaves
    recursively choose two-literal divisors
    perform Boolean division with SDC/don't-cares
    replace the KL-cut if the decomposed AIG is smaller
```

Version 0 should preserve only the parts needed to test whether the method has signal on contest cases:

```text
bounded multi-output region
same leaf set for several local outputs
exact truth tables for leaves <= 6
two-literal divisor enumeration
multi-output usefulness statistics
no replacement
```

Do not implement SDCs, DC projection, factored-form literal counting, recursive decomposition, or local AIG replacement in Version 0.

### Technical Step 3A: CO-Root Window Approximation

Full KL-cut enumeration is not required for Version 0. Use a deterministic shared-leaf TFI-window approximation that is easier to implement in GIA.

The preferred first implementation is CO-root grouping:

1. Iterate COs in their original order.
2. For each CO driver, collect a bounded TFI cone with at most `nMaxCutSize` leaves and `nMaxWindowSize` AND nodes.
3. Compute the leaf set of this cone.
4. Search subsequent CO drivers for other bounded cones with the same leaf set, or with a leaf set that can be merged without exceeding `nMaxCutSize`.
5. Build one multi-output window from the union of internal nodes and the selected CO drivers.
6. Evaluate only groups with at least two local outputs.

This is closer to the paper's multi-output block idea than a single-root window because it forces the audit to look for common support across several outputs.

Recommended local structs:

```c
typedef struct Mosh_BdecWin_t_ {
    Vec_Int_t * vLeaves;    // object IDs of local leaves, deterministic order
    Vec_Int_t * vNodes;     // internal AND object IDs, topological order
    Vec_Int_t * vOutLits;   // GIA literals driving local outputs
    Vec_Int_t * vOutCos;    // optional CO indices; use -1 for non-CO local outputs
} Mosh_BdecWin_t;
```

For Version 0, `vOutLits` can be only CO driver literals. Adding internal boundary outputs can wait until the CO-root implementation works.

### Technical Step 3B: TFI Cone Collection Detail

Use a bounded recursive collector. The collector should return failure rather than partially collecting an ambiguous window.

Pseudo-code:

```text
CollectConeLit(lit):
    obj = regular object of lit
    if obj is const0:
        return ok
    if obj is CI:
        add obj ID to leaves if not present
        fail if leaf count > maxcut
        return ok
    if obj is not AND:
        fail
    collect fanin0
    collect fanin1
    add obj ID to nodes if not present
    fail if node count > maxwin
```

Important details:

- Use object IDs for `vLeaves` and `vNodes`.
- Use GIA literals for `vOutLits` so CO-driver complements are preserved.
- Deduplicate leaves and nodes with a temporary object-ID map, not repeated linear scans on large windows.
- Keep `vNodes` topological by pushing an AND node after collecting both fanins. Since fanins have smaller IDs in GIA, sorting `vNodes` ascending is also acceptable.
- Deterministic leaf order matters. Use first-seen postorder or ascending object ID, but use the same rule everywhere.
- If the collector exceeds a bound, discard the whole window and update skip stats.

Suggested helper signatures:

```c
static int  Mosh_BdecCollectConeLit( Gia_Man_t * pGia, int iLit, Mosh_Par_t * pPar,
                                     Mosh_BdecWin_t * pWin, Vec_Int_t * vObjSeen,
                                     Vec_Int_t * vLeafSeen );
static void Mosh_BdecWinSortNormalize( Mosh_BdecWin_t * pWin );
static int  Mosh_BdecWinCanMergeLeaves( Mosh_BdecWin_t * pBase, Mosh_BdecWin_t * pOther,
                                        int nMaxLeaves );
static int  Mosh_BdecWinMerge( Mosh_BdecWin_t * pBase, Mosh_BdecWin_t * pOther,
                               Mosh_Par_t * pPar );
```

If implementing robust merge logic takes too long, start with the stricter rule `same leaf set only`. The report should then say `leaf_merge_mode = exact`.

### Technical Step 3C: Local Output Selection

Version 0 should start with CO outputs only.

For each CO group:

- Store the CO driver literal with `Gia_ObjFaninLit0p( pGia, Gia_ManCo(pGia, iCo) )`.
- The local output truth is the truth of that literal over the window leaves.
- Skip duplicated local output truth tables only for reporting; do not drop them before counting output hits because duplicated outputs are still evidence of multi-output sharing.

Later versions can add internal boundary nodes:

- A node inside the window is a boundary output if it has a fanout outside the window.
- This requires fanout support and is not necessary for Version 0.

### Technical Step 4A: Truth Table Encoding

Use a 64-bit `word` because `word` is already used in ABC truth-table code and represents up to 6 variables.

Local variable truth patterns for `nVars <= 6`:

```text
var 0: 0xAAAAAAAAAAAAAAAA
var 1: 0xCCCCCCCCCCCCCCCC
var 2: 0xF0F0F0F0F0F0F0F0
var 3: 0xFF00FF00FF00FF00
var 4: 0xFFFF0000FFFF0000
var 5: 0xFFFFFFFF00000000
```

Use a mask to ignore unused upper bits when `nVars < 6`:

```text
nBits = 1 << nVars
mask  = nBits == 64 ? ~(word)0 : (((word)1 << nBits) - 1)
```

Truth operations:

```text
not(t)       = (~t) & mask
and(t0, t1)  = (t0 & t1) & mask
or(t0, t1)   = (t0 | t1) & mask
```

Compute node truth:

```text
t0 = truth(fanin0)
if fanin0 is complemented: t0 = not(t0)
t1 = truth(fanin1)
if fanin1 is complemented: t1 = not(t1)
truth(node) = and(t0, t1)
```

Compute output truth:

```text
t = truth(Abc_Lit2Var(outLit))
if Abc_LitIsCompl(outLit): t = not(t)
```

Suggested helper signatures:

```c
static word Mosh_BdecTruthMask( int nVars );
static word Mosh_BdecTruthVar( int iVar, int nVars );
static word Mosh_BdecTruthNot( word t, int nVars );
static int  Mosh_BdecObjToLeafIndex( Vec_Int_t * vLeaves, int iObj );
static int  Mosh_BdecComputeTruths( Gia_Man_t * pGia, Mosh_BdecWin_t * pWin,
                                    Vec_Wrd_t * vObjTruths, Vec_Wrd_t * vOutTruths );
```

`vObjTruths` can be a `Vec_Wrd_t` sized to `Gia_ManObjNum(pGia)` for simplicity, or a compact per-window vector plus an ID-to-index map. Simplicity is better for Version 0 if memory remains reasonable.

### Technical Step 4B: Support Mask Computation

Compute support from the truth table by checking whether each variable changes the function.

For each variable `v`:

```text
cof0 = truth restricted to v = 0
cof1 = truth restricted to v = 1
if cof0 != cof1, variable v is in support
```

For Version 0, a simple bit loop is acceptable because `nVars <= 6`:

```text
for v in 0..nVars-1:
    differs = 0
    for minterm in 0..(1<<nVars)-1:
        if bit v of minterm is 0:
            mate = minterm | (1 << v)
            if bit(truth, minterm) != bit(truth, mate):
                differs = 1
                break
    if differs:
        support |= 1 << v
```

Do not infer support only from the structural cone; the truth-table support is more useful for Boolean decomposition and can remove irrelevant leaves.

### Technical Step 5A: Two-Literal Divisor Encoding

Represent a divisor by:

```text
leaf0 index
leaf1 index
compl0
compl1
op: AND or OR
truth
```

For each unordered pair `i < j`:

```text
for compl0 in {0,1}:
  for compl1 in {0,1}:
    lit0 = compl0 ? not(varTruth[i]) : varTruth[i]
    lit1 = compl1 ? not(varTruth[j]) : varTruth[j]
    div_and = lit0 & lit1
    div_or  = lit0 | lit1
```

Deduplication:

- Deduplicate by exact `truth` first.
- Keep the first deterministic encoding for a truth value.
- Count raw divisors and unique divisors separately.
- Do not deduplicate across windows in Version 0.

Existing internal match:

- A divisor has `existing_internal_match = 1` if its truth equals any internal node truth or its complement equals any internal node truth.
- Track complement match separately if cheap, for example `fExistingCompl`.
- This is only a signal; it is not a guarantee that a future replacement will save nodes.

### Technical Step 5B: Candidate Relevance Tests

Version 0 should start with simple tests that are easy to implement correctly.

Required support-hit test:

```text
support_hits = number of output truth tables whose support contains both leaf variables
```

Required multi-output divisor test:

```text
multi_output_hits = support_hits
candidate is multi-output if support_hits >= 2
```

Optional truth-correlation test, still cheap for `nVars <= 6`:

```text
cofactor_difference_hit(output, divisor):
    out_when_div0 has at least one 1/0 variation
    out_when_div1 has at least one 1/0 variation
```

If this optional test is ambiguous, skip it in Version 0. Do not spend time making a complex proxy look precise.

The top candidate score should be stable:

```text
score = 10 * support_hits
      +  3 * existing_internal_match
      +  2 * truth_correlation_hits
      -      1
```

If `truth_correlation_hits` is not implemented, set it to 0 and print that the score is support-only.

### Technical Step 5C: Candidate Cap Semantics

Use one clear interpretation and print it.

Recommended:

```text
nMaxCandidates = maximum raw divisors evaluated per window
```

Because `leaves <= 6`, the worst-case raw divisor count is:

```text
C(6,2) * 4 * 2 = 120
```

So the default `1000` will not cap ordinary Version 0 windows. It still gives a future-proof bound if the implementation later evaluates more divisor classes.

### Technical Steps 2-6: Suggested Function Layout in `moshareAlgoBdec.c`

Keep the implementation in one file for Version 0. Use private helpers.

Suggested order:

```c
// constants and local structs
typedef struct Mosh_BdecWin_t_      Mosh_BdecWin_t;
typedef struct Mosh_BdecCand_t_     Mosh_BdecCand_t;
typedef struct Mosh_BdecStats_t_    Mosh_BdecStats_t;

// stats/window lifecycle
static void Mosh_BdecStatsClear( Mosh_BdecStats_t * p );
static void Mosh_BdecStatsPrint( Mosh_BdecStats_t * p, Mosh_Res_t * pRes );
static void Mosh_BdecWinInit( Mosh_BdecWin_t * p );
static void Mosh_BdecWinClear( Mosh_BdecWin_t * p );
static void Mosh_BdecWinFree( Mosh_BdecWin_t * p );

// collection
static int  Mosh_BdecCollectConeLit( Gia_Man_t * pGia, int iLit, Mosh_Par_t * pPar,
                                     Mosh_BdecWin_t * pWin, Vec_Int_t * vObjSeen,
                                     Vec_Int_t * vLeafSeen );
static int  Mosh_BdecCollectCoWindow( Gia_Man_t * pGia, int iCo, Mosh_Par_t * pPar,
                                      Mosh_BdecWin_t * pWin, Mosh_BdecStats_t * pStats );
static int  Mosh_BdecTryMergeCoWindow( Gia_Man_t * pGia, int iCo, Mosh_Par_t * pPar,
                                       Mosh_BdecWin_t * pBase, Mosh_BdecStats_t * pStats );

// truth tables
static word Mosh_BdecTruthMask( int nVars );
static word Mosh_BdecTruthVar( int iVar, int nVars );
static word Mosh_BdecTruthNot( word t, int nVars );
static int  Mosh_BdecTruthSupportMask( word t, int nVars );
static int  Mosh_BdecComputeTruths( Gia_Man_t * pGia, Mosh_BdecWin_t * pWin,
                                    Vec_Wrd_t * vObjTruths, Vec_Wrd_t * vOutTruths );

// divisors
static int  Mosh_BdecEvalDivisors( Mosh_BdecWin_t * pWin, Vec_Wrd_t * vObjTruths,
                                   Vec_Wrd_t * vOutTruths, Mosh_Par_t * pPar,
                                   Mosh_BdecStats_t * pStats );
static int  Mosh_BdecDivisorSeen( Vec_Wrd_t * vDivTruths, word Truth );
static void Mosh_BdecCandUpdateBest( Mosh_BdecCand_t * pBest, Mosh_BdecCand_t * pCand );

// entry point
Gia_Man_t * Mosh_ManPerformAlgoBdec( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

This function list is a guide, not a requirement. The important point is to keep collection, truth computation, divisor evaluation, and stats separated.

### Technical Step 6: Entry Point Pseudo-Code

```text
Mosh_ManPerformAlgoBdec(pGia, pPar, pRes):
    clear result and local stats
    fill before stats
    if pPar->nMaxCutSize > 6:
        use 6 as bdec truth-table limit, but report skipped larger windows
    allocate reusable window structs and truth vectors
    for each CO index i:
        clear base window
        if collect CO window fails:
            continue
        for each later CO j, bounded by simple deterministic limit:
            try to merge if same/compatible leaf set
        if local output count < 2:
            count single-output and continue
        if leaf count == 0 or leaf count > 6:
            skip
        compute output truth tables
        evaluate two-literal divisors
    fill pRes:
        changed = 0
        nodes/levels unchanged
        candidates = stats unique or raw count
        applied = 0
        partitions = multi-output windows
    print stats if requested
    free temporaries
    return pGia
```

Do not call `Abc_FrameUpdateGia`; the command wrapper will not update if `fChanged = 0`.

## Implementation Steps

### Step 0: Pre-Implementation Baseline

- [ ] Run `git status --short` and note existing untracked/modified files.
- [ ] Read all documents listed in "Files to Inspect".
- [ ] Confirm current `moshare` commands still work before editing.
- [ ] Identify the current `MOSH_ALGO_COUNT` and existing enum values.
- [ ] Confirm `moshareAlgoBdiff.c` is left unchanged except where shared registry changes require no direct edit.

Suggested baseline commands:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "moshare -h"
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo none -stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdiff -maxcut 6 -maxwin 64 -maxcand 100 -stats"
```

### Step 1: Register `bdec` as a No-Op Algorithm

- [ ] In `moshare.h`, update `MOSH_ALGO_COUNT` from `2` to `3`.
- [ ] Add `MOSH_ALGO_BDEC = 2`.
- [ ] Add prototype:

```c
Gia_Man_t * Mosh_ManPerformAlgoBdec( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );
```

- [ ] In `moshareUtil.c`, map `"bdec"` to `MOSH_ALGO_BDEC`.
- [ ] In `moshareUtil.c`, map `MOSH_ALGO_BDEC` back to `"bdec"`.
- [ ] In `moshare.c`, dispatch `MOSH_ALGO_BDEC` to `Mosh_ManPerformAlgoBdec`.
- [ ] In `module.make`, add `src/opt/moshare/moshareAlgoBdec.c`.
- [ ] Create `moshareAlgoBdec.c` with an ABC-style file header.
- [ ] Implement only stats skeleton:
  - clear result
  - fill before/after node and level stats
  - set `fChanged = 0`
  - return `pGia`
  - print a simple `bdec` no-op line under `-stats`

Validation after Step 1:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo bdec -stats; print_stats"
```

Pass criteria:

- Build succeeds.
- Algorithm list contains `none`, `bdiff`, and `bdec`.
- `bdec` runs without crash.
- Before/after `print_stats` is unchanged.

### Step 2: Add BDec Stats Structures

Keep all new BDec-specific structs private in `moshareAlgoBdec.c`.

- [ ] Add a stats struct, for example:

```c
typedef struct Mosh_BdecStats_t_ {
    int nWindowsTried;
    int nWindowsMultiOutput;
    int nWindowsSmallEnough;
    int nWindowsSkippedLeaves;
    int nWindowsSkippedNodes;
    int nTruthComputed;
    int nDivisorsRaw;
    int nDivisorsUnique;
    int nDivisorsMultiOutput;
    int nDivisorsExisting;
    int nCandidatesCapped;
    int nBestScore;
} Mosh_BdecStats_t;
```

- [ ] Add a candidate summary struct, for example:

```c
typedef struct Mosh_BdecCand_t_ {
    int iLeaf0;
    int iLeaf1;
    int fCompl0;
    int fCompl1;
    int fOr;
    int nOutputHits;
    int nSupportHits;
    int fExisting;
    int nScore;
    word Truth;
} Mosh_BdecCand_t;
```

- [ ] Add local clear/print helpers.
- [ ] Keep result fields in `Mosh_Res_t` consistent:
  - `nCandidates`: total unique or processed divisor candidates, whichever is printed clearly.
  - `nApplied = 0`
  - `nPartitions` can be used as number of multi-output windows if useful.

Validation after Step 2:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdec -stats"
```

Pass criteria:

- Build succeeds.
- Stats print fields exist and are all zero or placeholder values.
- No network change.

### Step 3: Implement Bounded Window Collection

Keep this first window implementation simple. It does not need to be a full KL-cut implementation.
Use Technical Steps 3A, 3B, and 3C above as the coding blueprint.

- [ ] Iterate COs in original CO order.
- [ ] Build a small TFI window for each CO driver:
  - recursively traverse fanins
  - stop at CIs
  - stop when adding more leaves would exceed `nMaxCutSize`
  - stop when internal nodes would exceed `nMaxWindowSize`
  - collect internal nodes in topological order
- [ ] Try to merge later CO-driver windows into the base window when the merged leaf set stays within `nMaxCutSize`.
- [ ] Start with exact same-leaf-set merging if union merging is too risky for the first patch.
- [ ] Use CO driver literals as local outputs.
- [ ] Count and separately classify single-output windows.
- [ ] Evaluate only merged windows with at least two local CO outputs.
- [ ] Ensure no GIA marks or `Value` fields leak after collection.

Implementation notes:

- Do not require internal boundary-output detection in Version 0.
- Document the leaf merge mode in stats output: `exact` or `union`.
- Determinism is more important than maximizing candidate count in Version 0.
- If a window cannot prove all local outputs are functions of the selected leaves, skip it.

Validation after Step 3:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "read_blif tc_public/tc_public_7/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats"
./abc -c "read_blif tc_public/tc_public_25/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats"
```

Pass criteria:

- Runs without crash.
- Reports non-negative window counters.
- Skipped-window counters explain why no windows were evaluated, if none are evaluated.
- No network change.

### Step 4: Implement 64-Bit Truth Table Computation

Use Technical Steps 4A and 4B above as the coding blueprint.

- [ ] Limit exact computation to `nLeaves <= 6`.
- [ ] Use one `word` or `unsigned long long` consistently with ABC style.
- [ ] Assign local truth tables to leaves using deterministic variable order.
- [ ] Compute each internal AND node truth table in topological order:
  - apply fanin complement before AND
  - mask truth bits with `Mosh_BdecTruthMask(nLeaves)` after NOT/AND/OR operations
- [ ] Compute local output truth tables with complement handling.
- [ ] Compute support masks for each output truth table.
- [ ] Skip constant-only and literal-only output sets unless `-v` asks to print them.

Recommended local helpers:

```c
static word Mosh_BdecTruthVar( int iVar, int nVars );
static word Mosh_BdecTruthNot( word t, int nVars );
static int  Mosh_BdecTruthSupportMask( word t, int nVars );
static int  Mosh_BdecTruthIsConstOrLit( word t, int nVars );
```

Validation after Step 4:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats -v"
./abc -c "read_blif tc_public/tc_public_7/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats"
```

Optional synthetic tests:

- If useful, create small temporary BLIFs only under `claude_run/`.
- Include simple shared expressions such as two outputs both depending on `a & b`.
- Do not put synthetic test files in the repository root.

Pass criteria:

- Truth table counters are non-negative.
- Verbose output is deterministic across repeated runs.
- No crash on complemented fanins.
- No network change.

### Step 5: Enumerate Two-Literal Divisors

Use Technical Steps 5A, 5B, and 5C above as the coding blueprint.

For each evaluated window:

- [ ] Enumerate unordered leaf pairs `(i, j)` with `i < j`.
- [ ] Enumerate four literal polarities.
- [ ] Enumerate AND and OR forms:

```text
lit_i AND lit_j
lit_i OR  lit_j
```

- [ ] Deduplicate equivalent divisor truth tables within the window.
- [ ] Compute support hit counts:
  - how many local outputs contain both variables in their support
  - how many local outputs show a simple truth correlation with the divisor, if implemented
- [ ] Check whether the divisor truth table already exists as an internal node truth table.
- [ ] Score the candidate using the audit-only score.
- [ ] Enforce `nMaxCandidates` and count capped candidates.
- [ ] Track the best candidate per window and best candidates globally.

Version 0 may start with support-hit scoring only. A small truth-correlation test can be added after support-hit scoring is stable.

Validation after Step 5:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "read_blif tc_public/tc_public_7/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats -v"
./abc -c "read_blif tc_public/tc_public_16/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats -v"
./abc -c "read_blif tc_public/tc_public_23/input.blif; strash; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats"
```

Pass criteria:

- Reports raw/unique/multi-output divisor counts.
- Candidate counts obey `nMaxCandidates`.
- Verbose output shows top candidate summaries but remains capped.
- No network change.

### Step 6: Add Robust Stats and Result Reporting

- [ ] Print one compact aggregate stats block under `-stats`.
- [ ] Print top candidate/window details only under `-v`.
- [ ] Make labels stable for scripts.
- [ ] Populate `Mosh_Res_t` fields:
  - `fChanged = 0`
  - `nNodesBefore == nNodesAfter`
  - `nLevelsBefore == nLevelsAfter`
  - `nCandidates` set consistently
  - `nApplied = 0`
- [ ] Include enough skip counters to explain no-candidate cases.
- [ ] Avoid excessive per-window output by default.

Suggested output shape:

```text
moshare (algo bdec): windows tried = ...
moshare (algo bdec): multi-output windows = ...
moshare (algo bdec): small windows = ...
moshare (algo bdec): truth outputs = ...
moshare (algo bdec): divisors raw/unique/multi = ... / ... / ...
moshare (algo bdec): existing divisor matches = ...
moshare (algo bdec): best audit score = ...
moshare (algo bdec): nodes ... -> ...  levels ... -> ...  changed = 0
```

Validation after Step 6:

```bash
make ABC_USE_NO_READLINE=1
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats; print_stats"
```

Pass criteria:

- Labels are stable and parseable.
- Before/after ABC stats are unchanged.
- No network change.

### Step 7: Correctness Smoke Tests

Run these after implementation is complete.

```bash
make ABC_USE_NO_READLINE=1
./abc -c "moshare -h"
./abc -c "moshare -algo list"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo none -stats; print_stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo bdiff -maxcut 6 -maxwin 64 -maxcand 100 -stats; print_stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; print_stats; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats; print_stats; write_blif claude_run/moshare_bdec_tc02.blif"
./abc -c "cec tc_public/tc_public_2/input.blif claude_run/moshare_bdec_tc02.blif"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo bdec -maxcut 0 -maxwin 0 -maxcand 0 -stats"
./abc -c "read_blif tc_public/tc_public_2/input.blif; strash; moshare -algo __invalid_algo__"
```

Pass criteria:

- Build succeeds.
- `moshare -algo list` contains `bdec`.
- Existing `none` and `bdiff` still run.
- `bdec` runs and leaves stats unchanged.
- Written BLIF passes `cec`.
- Zero-bound run exits cleanly without crash.
- Invalid algorithm name still fails cleanly.

### Step 8: Candidate Audit Benchmark Plan

After smoke tests pass, run a bounded candidate audit.

Primary stubborn / low-improvement cases:

- `tc_public_7`
- `tc_public_8`
- `tc_public_9`
- `tc_public_15`
- `tc_public_16`
- `tc_public_17`
- `tc_public_22`
- `tc_public_23`
- `tc_public_26`

Sanity comparison cases with strong ABC-flow improvement:

- `tc_public_11`
- `tc_public_14`
- `tc_public_25`
- `tc_public_30`

Suggested per-case command:

```bash
./abc -c "read_blif tc_public/<case>/input.blif; strash; print_stats; moshare -algo bdec -maxcut 6 -maxwin 64 -maxcand 1000 -stats; print_stats; write_blif claude_run/moshare_bdec_<case>.blif"
./abc -c "cec tc_public/<case>/input.blif claude_run/moshare_bdec_<case>.blif"
```

Suggested script, if scripting is useful:

```text
claude_scripts/run_moshare_bdec_audit.py
```

Durable outputs:

```text
claude_doc/moshare_bdec_candidate_audit.md
claude_doc/moshare_bdec_candidate_audit.csv
```

Report fields:

- Case name.
- Initial AND/level after `strash`.
- Final AND/level after `bdec` (should match initial).
- CEC result.
- Runtime.
- Windows tried.
- Multi-output windows.
- Small windows.
- Truth outputs.
- Raw divisors.
- Unique divisors.
- Multi-output divisors.
- Existing divisor matches.
- Best audit score.
- Candidate cap hits.

## Validation Evidence

Claude Code should include the following in the final response:

- Files changed.
- Build command and result.
- `moshare -algo list` output summary proving `bdec` is registered.
- Smoke command outcomes.
- Before/after `print_stats` evidence showing no network change for at least one public case.
- CEC result for at least one written no-op `bdec` BLIF.
- Candidate audit report path under `claude_doc/`, if benchmark audit was run.
- Any tests not run and why.

## Expected Risks

Correctness risks:

- Complemented GIA literal handling in truth table computation.
- Treating object IDs as literals.
- Non-deterministic leaf ordering.
- Window outputs not fully represented by selected leaves.
- Leaking temporary marks or `Value` fields.
- Accidentally returning a new manager or setting `fChanged = 1`.

Runtime and memory risks:

- Repeated overlapping window collection may be expensive on large public cases.
- Fanout discovery can be costly or awkward if not already available.
- Verbose output can become too large.
- Candidate cap semantics can be unclear unless printed.

Method risks:

- `leaves <= 6` may be too strict and find few candidates.
- A simple TFI-window approximation may miss the KL-cut structures the paper relies on.
- Support-overlap candidate scoring may overestimate usefulness without Boolean division.
- Existing `bdiff` results show that promising Boolean candidates do not necessarily translate to AIG node gain.

## Done Criteria

Implementation done:

- [ ] ABC builds successfully.
- [ ] `moshare -algo list` includes `bdec`.
- [ ] `moshare -algo bdec -stats` runs from ABC batch mode.
- [ ] `bdec` leaves the GIA/network unchanged.
- [ ] `print_stats` before and after `bdec` is unchanged on at least one public case.
- [ ] Written `bdec` BLIF passes `cec` against the original.
- [ ] Zero-bound and invalid-algorithm smoke tests fail or no-op cleanly without crash.
- [ ] Existing `none` and `bdiff` behavior is not broken.
- [ ] Stats report window and divisor candidate counts.
- [ ] Runtime is bounded by explicit limits.
- [ ] No public benchmark files are modified.
- [ ] No root-level artifacts are created.

Benchmark/audit done:

- [ ] Candidate audit runs on the listed stubborn cases, or a documented subset if time is limited.
- [ ] Results are saved under `claude_doc/`.
- [ ] Temporary generated BLIFs/logs are under `claude_run/`.
- [ ] The report clearly states whether Version 1 replacement planning is justified.

## Open Questions for After Version 0

- Should Version 1 implement real KL-cut enumeration before Boolean division?
- Should Version 1 add a cofactor/divisibility test using truth tables instead of jumping directly to two-level minimization?
- Should XOR divisors be part of `bdec`, or remain separate under `bdiff`-style algorithms?
- Should `bdec` reuse ABC `Kit_Dsd` or `Bdc` utilities for later decomposition?
- Should the first replacement version rebuild only one best window per run, similar to conservative `bdiff`, or support iterative replacements immediately?
