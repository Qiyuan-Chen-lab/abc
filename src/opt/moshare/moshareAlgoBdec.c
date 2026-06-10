/**CFile****************************************************************

  FileName    [moshareAlgoBdec.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework — Boolean decomposition algorithm.]

  Synopsis    [Boolean decomposition candidate audit and optimization for multi-output sharing.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - June 8, 2026.]

***********************************************************************/

#include "moshare.h"
#include "bool/kit/kit.h"

#include <limits.h>

ABC_NAMESPACE_IMPL_START

extern int Kit_TruthToGia( Gia_Man_t * pMan, unsigned * pTruth, int nVars,
                           Vec_Int_t * vMemory, Vec_Int_t * vLeaves, int fHash );

////////////////////////////////////////////////////////////////////////
///                         LOCAL CONSTANTS                           ///
////////////////////////////////////////////////////////////////////////

#define MOSH_BDEC_TOP_PRINT 20

////////////////////////////////////////////////////////////////////////
///                         LOCAL TYPES                               ///
////////////////////////////////////////////////////////////////////////

typedef struct Mosh_BdecWin_t_ {
    Vec_Int_t * vLeaves;    // object IDs of local leaves, sorted ascending
    Vec_Int_t * vNodes;     // internal AND object IDs, sorted ascending (topological)
    Vec_Int_t * vOutLits;   // GIA literals driving local outputs (CO driver literals)
    Vec_Int_t * vOutCos;    // CO indices; -1 for non-CO local outputs
} Mosh_BdecWin_t;

typedef struct Mosh_BdecStats_t_ {
    int nWindowsTried;
    int nWindowsMultiOutput;
    int nWindowsSmallEnough;
    int nWindowsSkippedLeaves;
    int nWindowsSkippedNodes;
    int nWindowsSkippedSingleOutput;
    int nWindowsTruthFailed;
    int nTruthComputed;
    int nDivisorsRaw;
    int nDivisorsUnique;
    int nDivisorsMultiOutput;
    int nDivisorsExisting;
    int nDivisorsDecomposable;
    int nCandidatesCapped;
    int nReplacementTried;
    int nReplacementAccepted;
    int nRejectedProfit;
    int nRejectedLevel;
    int nBestScore;
} Mosh_BdecStats_t;

typedef struct Mosh_BdecCand_t_ {
    int   iLeaf0;
    int   iLeaf1;
    int   fCompl0;
    int   fCompl1;
    int   fOr;
    int   nOutputHits;
    int   nSupportHits;
    int   nDecOutputs;
    int   fExisting;
    int   iExistingObj;
    int   fExistingCompl;
    int   nScore;
    word  Truth;
} Mosh_BdecCand_t;

////////////////////////////////////////////////////////////////////////
///                     LOCAL FUNCTIONS                              ///
////////////////////////////////////////////////////////////////////////

static void Mosh_BdecStatsClear( Mosh_BdecStats_t * p )
{
    p->nWindowsTried            = 0;
    p->nWindowsMultiOutput      = 0;
    p->nWindowsSmallEnough      = 0;
    p->nWindowsSkippedLeaves    = 0;
    p->nWindowsSkippedNodes     = 0;
    p->nWindowsSkippedSingleOutput = 0;
    p->nWindowsTruthFailed      = 0;
    p->nTruthComputed           = 0;
    p->nDivisorsRaw             = 0;
    p->nDivisorsUnique          = 0;
    p->nDivisorsMultiOutput     = 0;
    p->nDivisorsExisting        = 0;
    p->nDivisorsDecomposable    = 0;
    p->nCandidatesCapped        = 0;
    p->nReplacementTried        = 0;
    p->nReplacementAccepted     = 0;
    p->nRejectedProfit          = 0;
    p->nRejectedLevel           = 0;
    p->nBestScore               = 0;
}

static void Mosh_BdecStatsPrint( Mosh_BdecStats_t * p )
{
    fprintf( stdout, "moshare (algo bdec): windows tried          = %d\n", p->nWindowsTried );
    fprintf( stdout, "moshare (algo bdec): multi-output windows    = %d\n", p->nWindowsMultiOutput );
    fprintf( stdout, "moshare (algo bdec): single-output skipped    = %d\n", p->nWindowsSkippedSingleOutput );
    fprintf( stdout, "moshare (algo bdec): small windows            = %d\n", p->nWindowsSmallEnough );
    fprintf( stdout, "moshare (algo bdec): skipped (leaves)         = %d\n", p->nWindowsSkippedLeaves );
    fprintf( stdout, "moshare (algo bdec): skipped (nodes)          = %d\n", p->nWindowsSkippedNodes );
    fprintf( stdout, "moshare (algo bdec): truth failed windows     = %d\n", p->nWindowsTruthFailed );
    fprintf( stdout, "moshare (algo bdec): truth outputs            = %d\n", p->nTruthComputed );
    fprintf( stdout, "moshare (algo bdec): divisors raw/unique/multi = %d / %d / %d\n",
        p->nDivisorsRaw, p->nDivisorsUnique, p->nDivisorsMultiOutput );
    fprintf( stdout, "moshare (algo bdec): existing divisor matches  = %d\n", p->nDivisorsExisting );
    fprintf( stdout, "moshare (algo bdec): decomposable divisors     = %d\n", p->nDivisorsDecomposable );
    fprintf( stdout, "moshare (algo bdec): replacements tried/accepted = %d / %d\n",
        p->nReplacementTried, p->nReplacementAccepted );
    fprintf( stdout, "moshare (algo bdec): reject_profit             = %d\n", p->nRejectedProfit );
    fprintf( stdout, "moshare (algo bdec): reject_level              = %d\n", p->nRejectedLevel );
    fprintf( stdout, "moshare (algo bdec): best audit score          = %d\n", p->nBestScore );
    if ( p->nCandidatesCapped > 0 )
        fprintf( stdout, "moshare (algo bdec): candidate cap hits        = %d\n", p->nCandidatesCapped );
}

static void Mosh_BdecCandClear( Mosh_BdecCand_t * p )
{
    memset( p, 0, sizeof(*p) );
    p->iLeaf0 = -1;
    p->iLeaf1 = -1;
    p->iExistingObj = -1;
}

// --- window lifecycle ---

static Mosh_BdecWin_t * Mosh_BdecWinAlloc()
{
    Mosh_BdecWin_t * p = ABC_ALLOC( Mosh_BdecWin_t, 1 );
    p->vLeaves  = Vec_IntAlloc( 16 );
    p->vNodes   = Vec_IntAlloc( 64 );
    p->vOutLits = Vec_IntAlloc( 8 );
    p->vOutCos  = Vec_IntAlloc( 8 );
    return p;
}

static void Mosh_BdecWinClear( Mosh_BdecWin_t * p )
{
    Vec_IntClear( p->vLeaves );
    Vec_IntClear( p->vNodes );
    Vec_IntClear( p->vOutLits );
    Vec_IntClear( p->vOutCos );
}

static void Mosh_BdecWinFree( Mosh_BdecWin_t * p )
{
    if ( !p ) return;
    Vec_IntFree( p->vLeaves );
    Vec_IntFree( p->vNodes );
    Vec_IntFree( p->vOutLits );
    Vec_IntFree( p->vOutCos );
    ABC_FREE( p );
}

static void Mosh_BdecWinSortNormalize( Mosh_BdecWin_t * p )
{
    Vec_IntSort( p->vLeaves,  0 );
    Vec_IntSort( p->vNodes,   0 );
}

// --- cone collection ---

/**Function*************************************************************

  Synopsis    [Recursively collects the TFI cone of a literal into the window.]

  Description [Adds CIs to vLeaves and AND nodes to vNodes. vLeafSeen and
  vObjSeen are used as bit-flag vectors indexed by object ID.]

  Returns 1 on success, 0 if bounds are exceeded (window is left in an
  incomplete state and should be discarded).

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecCollectConeLit( Gia_Man_t * pGia, int iLit, Mosh_Par_t * pPar,
                                     Mosh_BdecWin_t * pWin, Vec_Int_t * vObjSeen,
                                     Vec_Int_t * vLeafSeen )
{
    int iObj = Abc_Lit2Var( iLit );
    Gia_Obj_t * pObj;

    // const0 node: nothing to collect
    if ( iObj == 0 )
        return 1;

    // CI: add as leaf if not already seen
    pObj = Gia_ManObj( pGia, iObj );
    if ( Gia_ObjIsCi( pObj ) )
    {
        if ( !Vec_IntEntry( vLeafSeen, iObj ) )
        {
            if ( Vec_IntSize( pWin->vLeaves ) >= pPar->nMaxCutSize )
                return 0;
            Vec_IntPush( pWin->vLeaves, iObj );
            Vec_IntWriteEntry( vLeafSeen, iObj, 1 );
        }
        return 1;
    }

    // not an AND node: unsupported structure
    if ( !Gia_ObjIsAnd( pObj ) )
        return 0;

    // already collected this node
    if ( Vec_IntEntry( vObjSeen, iObj ) )
        return 1;

    // check node budget
    if ( Vec_IntSize( pWin->vNodes ) >= pPar->nMaxWindowSize )
        return 0;

    // recurse into fanins
    if ( !Mosh_BdecCollectConeLit( pGia, Gia_ObjFaninLit0p( pGia, pObj ), pPar, pWin, vObjSeen, vLeafSeen ) )
        return 0;
    if ( !Mosh_BdecCollectConeLit( pGia, Gia_ObjFaninLit1p( pGia, pObj ), pPar, pWin, vObjSeen, vLeafSeen ) )
        return 0;

    // add this node
    Vec_IntWriteEntry( vObjSeen, iObj, 1 );
    Vec_IntPush( pWin->vNodes, iObj );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Collects the TFI cone of a single CO driver into a window.]

  Description [Allocates temporary seen vectors, collects the cone, and
  normalizes the window. Returns 1 on success, 0 on bounds overflow.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecCollectCoWindow( Gia_Man_t * pGia, int iCo, Mosh_Par_t * pPar,
                                      Mosh_BdecWin_t * pWin, Vec_Int_t * vObjSeen,
                                      Vec_Int_t * vLeafSeen, Mosh_BdecStats_t * pStats )
{
    Gia_Obj_t * pCo = Gia_ManCo( pGia, iCo );
    int iLit = Gia_ObjFaninLit0p( pGia, pCo );
    int nNodesBefore, nLeavesBefore;

    nNodesBefore  = Vec_IntSize( pWin->vNodes );
    nLeavesBefore = Vec_IntSize( pWin->vLeaves );

    if ( !Mosh_BdecCollectConeLit( pGia, iLit, pPar, pWin, vObjSeen, vLeafSeen ) )
    {
        // roll back partial collection
        while ( Vec_IntSize( pWin->vNodes ) > nNodesBefore )
        {
            int iObj = Vec_IntPop( pWin->vNodes );
            Vec_IntWriteEntry( vObjSeen, iObj, 0 );
        }
        while ( Vec_IntSize( pWin->vLeaves ) > nLeavesBefore )
        {
            int iObj = Vec_IntPop( pWin->vLeaves );
            Vec_IntWriteEntry( vLeafSeen, iObj, 0 );
        }

        if ( Vec_IntSize( pWin->vNodes ) >= pPar->nMaxWindowSize )
            pStats->nWindowsSkippedNodes++;
        else
            pStats->nWindowsSkippedLeaves++;
        return 0;
    }

    // record the CO driver as a local output
    Vec_IntPush( pWin->vOutLits, iLit );
    Vec_IntPush( pWin->vOutCos,  iCo );

    return 1;
}

/**Function*************************************************************

  Synopsis    [Checks whether two windows can be merged without exceeding the leaf limit.]

  Description [Computes the union of two sorted leaf sets and returns the
  union size. Returns 0 if the union would exceed nMaxLeaves.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecWinCanMergeLeaves( Mosh_BdecWin_t * pA, Mosh_BdecWin_t * pB, int nMaxLeaves )
{
    int iA, iB, nA, nB, nUnion;
    nA = Vec_IntSize( pA->vLeaves );
    nB = Vec_IntSize( pB->vLeaves );
    iA = iB = nUnion = 0;

    while ( iA < nA || iB < nB )
    {
        int leafA = (iA < nA) ? Vec_IntEntry( pA->vLeaves, iA ) : INT_MAX;
        int leafB = (iB < nB) ? Vec_IntEntry( pB->vLeaves, iB ) : INT_MAX;

        if ( leafA < leafB )
        {
            nUnion++;
            iA++;
        }
        else if ( leafB < leafA )
        {
            nUnion++;
            iB++;
        }
        else
        {
            nUnion++;
            iA++;
            iB++;
        }

        if ( nUnion > nMaxLeaves )
            return 0;
    }

    return nUnion;
}

/**Function*************************************************************

  Synopsis    [Merges pOther's outputs, nodes, and leaves into pBase.]

  Description [Computes the union leaf set. Deduplicates nodes and leaves.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Mosh_BdecWinMergeUnion( Mosh_BdecWin_t * pBase, Mosh_BdecWin_t * pOther,
                                     Gia_Man_t * pGia, Vec_Int_t * vObjSeen, Vec_Int_t * vLeafSeen )
{
    int i, iObj;
    Vec_Int_t * vMerged;

    // merge nodes
    Vec_IntForEachEntry( pOther->vNodes, iObj, i )
    {
        if ( !Vec_IntEntry( vObjSeen, iObj ) )
        {
            Vec_IntWriteEntry( vObjSeen, iObj, 1 );
            Vec_IntPush( pBase->vNodes, iObj );
        }
    }

    // merge leaves (union of sorted sets)
    vMerged = Vec_IntAlloc( Vec_IntSize(pBase->vLeaves) + Vec_IntSize(pOther->vLeaves) );
    {
        int iA = 0, iB = 0;
        int nA = Vec_IntSize( pBase->vLeaves );
        int nB = Vec_IntSize( pOther->vLeaves );
        while ( iA < nA || iB < nB )
        {
            int leafA = (iA < nA) ? Vec_IntEntry( pBase->vLeaves, iA ) : INT_MAX;
            int leafB = (iB < nB) ? Vec_IntEntry( pOther->vLeaves, iB ) : INT_MAX;
            if ( leafA < leafB )
            {
                Vec_IntPush( vMerged, leafA );
                iA++;
            }
            else if ( leafB < leafA )
            {
                Vec_IntPush( vMerged, leafB );
                Vec_IntWriteEntry( vLeafSeen, leafB, 1 );
                iB++;
            }
            else
            {
                Vec_IntPush( vMerged, leafA );
                iA++;
                iB++;
            }
        }
    }
    Vec_IntClear( pBase->vLeaves );
    Vec_IntForEachEntry( vMerged, iObj, i )
        Vec_IntPush( pBase->vLeaves, iObj );
    Vec_IntFree( vMerged );

    // merge outputs
    Vec_IntForEachEntry( pOther->vOutLits, iObj, i )
        Vec_IntPush( pBase->vOutLits, iObj );
    Vec_IntForEachEntry( pOther->vOutCos, iObj, i )
        Vec_IntPush( pBase->vOutCos, iObj );
}

// --- truth table helpers ---

static word Mosh_BdecTruthMask( int nVars )
{
    int nBits = 1 << nVars;
    if ( nBits >= 64 )
        return ~(word)0;
    return ((word)1 << nBits) - 1;
}

static word Mosh_BdecTruthVar( int iVar, int nVars )
{
    // Precomputed truth-table columns for up to 6 variables
    static const word s_VarTruths[6] = {
        0xAAAAAAAAAAAAAAAAULL,
        0xCCCCCCCCCCCCCCCCULL,
        0xF0F0F0F0F0F0F0F0ULL,
        0xFF00FF00FF00FF00ULL,
        0xFFFF0000FFFF0000ULL,
        0xFFFFFFFF00000000ULL
    };
    assert( iVar >= 0 && iVar < 6 );
    return s_VarTruths[iVar];
}

static word Mosh_BdecTruthNot( word t, int nVars )
{
    return (~t) & Mosh_BdecTruthMask( nVars );
}

static int Mosh_BdecObjToLeafIndex( Vec_Int_t * vLeaves, int iObj )
{
    int i, iEntry;
    Vec_IntForEachEntry( vLeaves, iEntry, i )
        if ( iEntry == iObj )
            return i;
    return -1;
}

/**Function*************************************************************

  Synopsis    [Computes support mask from a truth table.]

  Description [Returns a bitmask where bit v is set if variable v is in
  the support of the function.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecTruthSupportMask( word t, int nVars )
{
    int v, support = 0;
    int nBits = 1 << nVars;

    for ( v = 0; v < nVars; v++ )
    {
        int m;
        for ( m = 0; m < nBits; m++ )
        {
            int mate = m ^ (1 << v);
            if ( ((t >> m) & 1) != ((t >> mate) & 1) )
            {
                support |= (1 << v);
                break;
            }
        }
    }

    return support;
}

/**Function*************************************************************

  Synopsis    [Computes truth tables for all nodes and outputs in a window.]

  Description [Fills vObjTruths (sized to nObjs) with truth-table words for
  leaves and internal nodes in pWin. Fills vOutTruths with truth-table words
  for each output literal in pWin.]

  Returns 1 on success, 0 on failure.

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecComputeTruths( Gia_Man_t * pGia, Mosh_BdecWin_t * pWin,
                                    Vec_Wrd_t * vObjTruths, Vec_Wrd_t * vOutTruths )
{
    int nVars = Vec_IntSize( pWin->vLeaves );
    word Mask = Mosh_BdecTruthMask( nVars );
    int i, iObj, iLit;
    word t0, t1;

    if ( nVars < 1 || nVars > 6 )
        return 0;

    // assign leaf truth tables
    Vec_IntForEachEntry( pWin->vLeaves, iObj, i )
    {
        Vec_WrdWriteEntry( vObjTruths, iObj, Mosh_BdecTruthVar( i, nVars ) );
    }

    // compute node truth tables in topological order (ascending ID)
    Vec_IntForEachEntry( pWin->vNodes, iObj, i )
    {
        Gia_Obj_t * pObj = Gia_ManObj( pGia, iObj );
        int iFan0 = Gia_ObjFaninId0p( pGia, pObj );
        int iFan1 = Gia_ObjFaninId1p( pGia, pObj );

        t0 = Vec_WrdEntry( vObjTruths, iFan0 );
        if ( Gia_ObjFaninC0( pObj ) )
            t0 = (~t0) & Mask;

        t1 = Vec_WrdEntry( vObjTruths, iFan1 );
        if ( Gia_ObjFaninC1( pObj ) )
            t1 = (~t1) & Mask;

        Vec_WrdWriteEntry( vObjTruths, iObj, (t0 & t1) & Mask );
    }

    // compute output truth tables
    Vec_IntForEachEntry( pWin->vOutLits, iLit, i )
    {
        int iVar = Abc_Lit2Var( iLit );
        word t;

        if ( iVar == 0 )
        {
            // const0 node
            t = Abc_LitIsCompl( iLit ) ? Mask : 0;
        }
        else if ( Gia_ObjIsCi( Gia_ManObj( pGia, iVar ) ) )
        {
            // CI (leaf): find its index
            int iLeaf = Mosh_BdecObjToLeafIndex( pWin->vLeaves, iVar );
            if ( iLeaf < 0 )
                return 0; // output depends on a non-leaf CI — should not happen
            t = Mosh_BdecTruthVar( iLeaf, nVars );
            if ( Abc_LitIsCompl( iLit ) )
                t = (~t) & Mask;
        }
        else
        {
            // internal node: use computed truth
            t = Vec_WrdEntry( vObjTruths, iVar );
            if ( Abc_LitIsCompl( iLit ) )
                t = (~t) & Mask;
        }

        Vec_WrdPush( vOutTruths, t );
    }

    return 1;
}

// --- divisor enumeration ---

/**Function*************************************************************

  Synopsis    [Checks whether a divisor truth is already in the seen list.]

  Returns 1 if seen, 0 otherwise.

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecDivisorSeen( Vec_Wrd_t * vDivTruths, word Truth )
{
    int i;
    word t;
    Vec_WrdForEachEntry( vDivTruths, t, i )
        if ( t == Truth )
            return 1;
    return 0;
}

/**Function*************************************************************

  Synopsis    [Checks whether a divisor truth matches any internal node truth or its complement.]

  Returns 1 if a match is found.

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Mosh_BdecFindExistingNode( Vec_Int_t * vNodes, Vec_Wrd_t * vObjTruths,
                                      word Mask, word Truth, int * piObj, int * pfCompl )
{
    int i, iObj;
    word tNode;
    if ( piObj )
        *piObj = -1;
    if ( pfCompl )
        *pfCompl = 0;
    Vec_IntForEachEntry( vNodes, iObj, i )
    {
        tNode = Vec_WrdEntry( vObjTruths, iObj );
        if ( tNode == Truth )
        {
            if ( piObj )
                *piObj = iObj;
            if ( pfCompl )
                *pfCompl = 0;
            return 1;
        }
        if ( tNode == ((~Truth) & Mask) )
        {
            if ( piObj )
                *piObj = iObj;
            if ( pfCompl )
                *pfCompl = 1;
            return 1;
        }
    }
    return 0;
}

static int Mosh_BdecCompressOutputTruth( word tOut, word tDiv, int nVars,
                                         int iLeaf0, int iLeaf1, word * pTruthNew )
{
    int nVarsNew = nVars - 1;
    int nMints = 1 << nVars;
    word tNew = 0;
    word tSeen = 0;
    int m;

    assert( nVarsNew >= 1 && nVarsNew <= 5 );

    for ( m = 0; m < nMints; m++ )
    {
        int fBit = (int)((tOut >> m) & 1);
        int dBit = (int)((tDiv >> m) & 1);
        int mNew = dBit;
        int iVar, iNewVar = 1;

        for ( iVar = 0; iVar < nVars; iVar++ )
        {
            if ( iVar == iLeaf0 || iVar == iLeaf1 )
                continue;
            if ( (m >> iVar) & 1 )
                mNew |= (1 << iNewVar);
            iNewVar++;
        }

        if ( (tSeen >> mNew) & 1 )
        {
            if ( (int)((tNew >> mNew) & 1) != fBit )
                return 0;
        }
        else
        {
            tSeen |= ((word)1 << mNew);
            if ( fBit )
                tNew |= ((word)1 << mNew);
        }
    }

    *pTruthNew = tNew & Mosh_BdecTruthMask( nVarsNew );
    return 1;
}

static int Mosh_BdecCollectDecomposableOutputs( Mosh_BdecWin_t * pWin,
                                                Vec_Wrd_t * vOutTruths,
                                                word tDiv,
                                                Mosh_BdecCand_t * pCand,
                                                Vec_Int_t * vOutIdxs,
                                                Vec_Wrd_t * vDecTruths )
{
    int nVars = Vec_IntSize( pWin->vLeaves );
    int nOuts = Vec_WrdSize( vOutTruths );
    int iOut, nDecOuts = 0;

    assert( nVars >= 2 && nVars <= 6 );

    if ( vOutIdxs )
        Vec_IntClear( vOutIdxs );
    if ( vDecTruths )
        Vec_WrdClear( vDecTruths );

    for ( iOut = 0; iOut < nOuts; iOut++ )
    {
        word tDec;
        if ( !Mosh_BdecCompressOutputTruth( Vec_WrdEntry( vOutTruths, iOut ), tDiv,
                 nVars, pCand->iLeaf0, pCand->iLeaf1, &tDec ) )
            continue;

        nDecOuts++;
        if ( vOutIdxs )
            Vec_IntPush( vOutIdxs, iOut );
        if ( vDecTruths )
            Vec_WrdPush( vDecTruths, tDec );
    }

    return nDecOuts;
}

static int Mosh_BdecBuildDivisorLit( Gia_Man_t * pGia, Gia_Man_t * pNew,
                                     Mosh_BdecWin_t * pWin, Mosh_BdecCand_t * pCand )
{
    int iLit0, iLit1;

    if ( pCand->iExistingObj >= 0 )
    {
        int iLit = Gia_ManObj( pGia, pCand->iExistingObj )->Value;
        return Abc_LitNotCond( iLit, pCand->fExistingCompl );
    }

    iLit0 = Gia_ManObj( pGia, Vec_IntEntry( pWin->vLeaves, pCand->iLeaf0 ) )->Value;
    iLit1 = Gia_ManObj( pGia, Vec_IntEntry( pWin->vLeaves, pCand->iLeaf1 ) )->Value;

    if ( pCand->fCompl0 )
        iLit0 = Abc_LitNot( iLit0 );
    if ( pCand->fCompl1 )
        iLit1 = Abc_LitNot( iLit1 );

    if ( !pCand->fOr )
        return Gia_ManHashAnd( pNew, iLit0, iLit1 );

    return Abc_LitNot( Gia_ManHashAnd( pNew, Abc_LitNot(iLit0), Abc_LitNot(iLit1) ) );
}

static void Mosh_BdecBuildSynthLeaves( Gia_Man_t * pGia, Mosh_BdecWin_t * pWin,
                                       Mosh_BdecCand_t * pCand, int iDivLit,
                                       Vec_Int_t * vSynthLeaves )
{
    int i, iObj;
    Vec_IntClear( vSynthLeaves );
    Vec_IntPush( vSynthLeaves, iDivLit );
    Vec_IntForEachEntry( pWin->vLeaves, iObj, i )
    {
        if ( i == pCand->iLeaf0 || i == pCand->iLeaf1 )
            continue;
        Vec_IntPush( vSynthLeaves, Gia_ManObj( pGia, iObj )->Value );
    }
}

static int Mosh_BdecBuildDecTruth( Gia_Man_t * pNew, word Truth, int nVars,
                                   Vec_Int_t * vLeaves, Vec_Int_t * vMemory )
{
    unsigned uTruth;
    int m;
    assert( nVars >= 1 && nVars <= 5 );
    assert( Vec_IntSize( vLeaves ) == nVars );
    uTruth = 0;
    for ( m = 0; m < 32; m++ )
        if ( (Truth >> (m & ((1 << nVars) - 1))) & 1 )
            uTruth |= (1u << m);
    return Kit_TruthToGia( pNew, &uTruth, nVars, vMemory, vLeaves, 1 );
}

static Gia_Man_t * Mosh_BdecRebuildWithCandidate( Gia_Man_t * pGia,
                                                  Mosh_BdecWin_t * pWin,
                                                  Vec_Wrd_t * vOutTruths,
                                                  Mosh_BdecCand_t * pCand )
{
    Gia_Man_t * pNew;
    Gia_Obj_t * pObj;
    Vec_Int_t * vOutIdxs, * vSynthLeaves, * vMemory;
    Vec_Wrd_t * vDecTruths;
    int * pCoToDec;
    int nCos, nVarsNew, nDecOuts;
    int i, iOut, iCo, iObj, iDivLitNew = -1;

    nVarsNew = Vec_IntSize( pWin->vLeaves ) - 1;
    if ( nVarsNew < 1 || nVarsNew > 5 )
        return NULL;

    vOutIdxs = Vec_IntAlloc( 8 );
    vDecTruths = Vec_WrdAlloc( 8 );
    nDecOuts = Mosh_BdecCollectDecomposableOutputs( pWin, vOutTruths, pCand->Truth,
        pCand, vOutIdxs, vDecTruths );
    if ( nDecOuts < 2 )
    {
        Vec_IntFree( vOutIdxs );
        Vec_WrdFree( vDecTruths );
        return NULL;
    }

    nCos = Gia_ManCoNum( pGia );
    pCoToDec = ABC_ALLOC( int, nCos );
    for ( i = 0; i < nCos; i++ )
        pCoToDec[i] = -1;
    Vec_IntForEachEntry( vOutIdxs, iOut, i )
    {
        iCo = Vec_IntEntry( pWin->vOutCos, iOut );
        if ( iCo >= 0 && iCo < nCos )
            pCoToDec[iCo] = i;
    }

    pNew = Gia_ManStart( Gia_ManObjNum( pGia ) + 128 );
    if ( pNew == NULL )
    {
        ABC_FREE( pCoToDec );
        Vec_IntFree( vOutIdxs );
        Vec_WrdFree( vDecTruths );
        return NULL;
    }

    pNew->pName = Abc_UtilStrsav( pGia->pName );
    pNew->pSpec = Abc_UtilStrsav( pGia->pSpec );

    vSynthLeaves = Vec_IntAlloc( nVarsNew );
    vMemory = Vec_IntAlloc( 0 );

    Gia_ManConst0( pGia )->Value = 0;
    Gia_ManHashStart( pNew );

    iCo = 0;
    Gia_ManForEachObj1( pGia, pObj, iObj )
    {
        if ( Gia_ObjIsCi( pObj ) )
        {
            pObj->Value = Gia_ManAppendCi( pNew );
        }
        else if ( Gia_ObjIsAnd( pObj ) )
        {
            pObj->Value = Gia_ManHashAnd( pNew,
                Gia_ObjFanin0Copy( pObj ), Gia_ObjFanin1Copy( pObj ) );
        }
        else if ( Gia_ObjIsCo( pObj ) )
        {
            int iDec = iCo < nCos ? pCoToDec[iCo] : -1;
            if ( iDec >= 0 )
            {
                int iLitNew;
                if ( iDivLitNew < 0 )
                    iDivLitNew = Mosh_BdecBuildDivisorLit( pGia, pNew, pWin, pCand );
                Mosh_BdecBuildSynthLeaves( pGia, pWin, pCand, iDivLitNew, vSynthLeaves );
                iLitNew = Mosh_BdecBuildDecTruth( pNew, Vec_WrdEntry( vDecTruths, iDec ),
                    nVarsNew, vSynthLeaves, vMemory );
                pObj->Value = Gia_ManAppendCo( pNew, iLitNew );
            }
            else
            {
                pObj->Value = Gia_ManAppendCo( pNew, Gia_ObjFanin0Copy( pObj ) );
            }
            iCo++;
        }
    }

    Gia_ManHashStop( pNew );
    Gia_ManSetRegNum( pNew, Gia_ManRegNum( pGia ) );

    ABC_FREE( pCoToDec );
    Vec_IntFree( vOutIdxs );
    Vec_WrdFree( vDecTruths );
    Vec_IntFree( vSynthLeaves );
    Vec_IntFree( vMemory );

    return pNew;
}

static Gia_Man_t * Mosh_BdecTryApplyCandidate( Gia_Man_t * pGia, Mosh_Par_t * pPar,
                                               Mosh_BdecWin_t * pWin, Vec_Wrd_t * vOutTruths,
                                               Mosh_BdecCand_t * pCand,
                                               Mosh_BdecStats_t * pStats )
{
    Gia_Man_t * pNew, * pClean;
    int nOldAnd, nNewAnd, nOldLevel, nNewLevel, nLevelGrowth;

    pStats->nReplacementTried++;

    if ( pPar->fDryRun )
        return NULL;

    pNew = Mosh_BdecRebuildWithCandidate( pGia, pWin, vOutTruths, pCand );
    if ( pNew == NULL )
        return NULL;

    pClean = Gia_ManCleanup( pNew );
    if ( pClean != pNew )
    {
        Gia_ManStop( pNew );
        pNew = pClean;
    }

    nOldAnd = Gia_ManAndNum( pGia );
    nNewAnd = Gia_ManAndNum( pNew );
    nOldLevel = Gia_ManLevelNum( pGia );
    nNewLevel = Gia_ManLevelNum( pNew );
    nLevelGrowth = nNewLevel - nOldLevel;

    if ( nOldAnd <= nNewAnd )
    {
        if ( pPar->fVerbose )
            fprintf( stdout, "moshare (algo bdec): replacement rejected: old=%d new=%d gain=%d dec_outputs=%d\n",
                nOldAnd, nNewAnd, nOldAnd - nNewAnd, pCand->nDecOutputs );
        pStats->nRejectedProfit++;
        Gia_ManStop( pNew );
        return NULL;
    }

    if ( nLevelGrowth > pPar->nMaxLevelGrowth )
    {
        if ( pPar->fVerbose )
            fprintf( stdout, "moshare (algo bdec): replacement rejected: old_level=%d new_level=%d growth=%d\n",
                nOldLevel, nNewLevel, nLevelGrowth );
        pStats->nRejectedLevel++;
        Gia_ManStop( pNew );
        return NULL;
    }

    pStats->nReplacementAccepted++;
    if ( pPar->fVerbose )
        fprintf( stdout, "moshare (algo bdec): replacement accepted: nodes %d->%d levels %d->%d dec_outputs=%d\n",
            nOldAnd, nNewAnd, nOldLevel, nNewLevel, pCand->nDecOutputs );
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Enumerate two-literal divisors for one window and update stats.]

  Description [Enumerates all AND/OR divisor forms for every unordered leaf
  pair with all polarity combinations. Deduplicates by truth table and scores
  by multi-output support overlap.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static Gia_Man_t * Mosh_BdecEvalDivisors( Gia_Man_t * pGia,
                                           Mosh_BdecWin_t * pWin,
                                           Vec_Wrd_t * vObjTruths,
                                           Vec_Wrd_t * vOutTruths,
                                           Mosh_Par_t * pPar,
                                           Mosh_BdecStats_t * pStats,
                                           Mosh_BdecCand_t * pBestGlobal,
                                           Mosh_BdecCand_t * pBestDecomp )
{
    int nVars   = Vec_IntSize( pWin->vLeaves );
    int nOuts   = Vec_WrdSize( vOutTruths );
    word Mask   = Mosh_BdecTruthMask( nVars );
    Vec_Wrd_t * vDivTruths = Vec_WrdAlloc( 128 );
    int i, j, ci, cj, fOr;
    int nProcessed = 0;
    int * pOutSupports;
    Gia_Man_t * pNew = NULL;

    // precompute output support masks
    pOutSupports = ABC_ALLOC( int, nOuts );
    for ( i = 0; i < nOuts; i++ )
        pOutSupports[i] = Mosh_BdecTruthSupportMask( Vec_WrdEntry( vOutTruths, i ), nVars );

    // enumerate all unordered leaf pairs
    for ( i = 0; i < nVars; i++ )
    {
        word tVarI = Mosh_BdecTruthVar( i, nVars );

        for ( j = i + 1; j < nVars; j++ )
        {
            word tVarJ = Mosh_BdecTruthVar( j, nVars );

            for ( ci = 0; ci <= 1; ci++ )
            {
                word tLitI = ci ? (~tVarI) & Mask : tVarI;

                for ( cj = 0; cj <= 1; cj++ )
                {
                    word tLitJ = cj ? (~tVarJ) & Mask : tVarJ;

                    for ( fOr = 0; fOr <= 1; fOr++ )
                    {
                        word tDiv;
                        int nSupportHits, nOutputHits, nDecOutputs, fExisting;
                        int iExistingObj, fExistingCompl;
                        Mosh_BdecCand_t cand;

                        // cap check
                        if ( nProcessed >= pPar->nMaxCandidates )
                        {
                            if ( nProcessed == pPar->nMaxCandidates )
                                pStats->nCandidatesCapped++;
                            nProcessed++;
                            continue;
                        }
                        nProcessed++;

                        tDiv = fOr ? ((tLitI | tLitJ) & Mask) : ((tLitI & tLitJ) & Mask);
                        pStats->nDivisorsRaw++;

                        // deduplicate within this window
                        if ( Mosh_BdecDivisorSeen( vDivTruths, tDiv ) )
                            continue;

                        Vec_WrdPush( vDivTruths, tDiv );

                        // count unique divisors
                        pStats->nDivisorsUnique++;

                        // compute support hits: how many outputs have BOTH leaf vars in support
                        {
                            int bothMask = (1 << i) | (1 << j);
                            nSupportHits = 0;
                            for ( nOutputHits = 0; nOutputHits < nOuts; nOutputHits++ )
                                if ( (pOutSupports[nOutputHits] & bothMask) == bothMask )
                                    nSupportHits++;
                        }
                        nOutputHits = nSupportHits; // Version 0: use support overlap as proxy

                        // check existing node match
                        fExisting = Mosh_BdecFindExistingNode( pWin->vNodes, vObjTruths, Mask, tDiv,
                            &iExistingObj, &fExistingCompl );
                        if ( fExisting )
                            pStats->nDivisorsExisting++;

                        if ( nSupportHits >= 2 )
                            pStats->nDivisorsMultiOutput++;

                        // exact bound-set decomposability: f = h(divisor, remaining leaves)
                        Mosh_BdecCandClear( &cand );
                        cand.iLeaf0      = i;
                        cand.iLeaf1      = j;
                        cand.fCompl0     = ci;
                        cand.fCompl1     = cj;
                        cand.fOr         = fOr;
                        cand.Truth       = tDiv;
                        nDecOutputs = Mosh_BdecCollectDecomposableOutputs( pWin, vOutTruths,
                            tDiv, &cand, NULL, NULL );
                        if ( nDecOutputs >= 2 )
                            pStats->nDivisorsDecomposable++;

                        // score
                        cand.nOutputHits = nOutputHits;
                        cand.nSupportHits = nSupportHits;
                        cand.nDecOutputs = nDecOutputs;
                        cand.fExisting   = fExisting;
                        cand.iExistingObj = iExistingObj;
                        cand.fExistingCompl = fExistingCompl;
                        cand.nScore      = 10 * nSupportHits + 8 * nDecOutputs + 3 * fExisting - 1;

                        // track best
                        if ( cand.nScore > pBestGlobal->nScore )
                            *pBestGlobal = cand;
                        if ( nDecOutputs >= 2 && cand.nScore > pBestDecomp->nScore )
                            *pBestDecomp = cand;
                        if ( cand.nScore > pStats->nBestScore )
                            pStats->nBestScore = cand.nScore;

                        if ( nDecOutputs >= 2 && pNew == NULL )
                        {
                            pNew = Mosh_BdecTryApplyCandidate( pGia, pPar, pWin,
                                vOutTruths, &cand, pStats );
                        }
                    }
                }
            }
        }
    }
    ABC_FREE( pOutSupports );
    Vec_WrdFree( vDivTruths );
    return pNew;
}

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [BDec algorithm entry point: candidate audit plus conservative replacement.]

  Description [Builds bounded multi-output windows from CO drivers,
  computes truth tables for windows with <= 6 leaves, enumerates two-literal
  divisors, and tries one exact decomposable CO-driver replacement when it
  strictly reduces AND count without exceeding the level-growth bound.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Mosh_ManPerformAlgoBdec( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes )
{
    Mosh_BdecStats_t stats;
    Mosh_BdecWin_t * pBaseWin;    // accumulating multi-output window
    Mosh_BdecWin_t * pCurWin;     // current CO's single-output window
    Vec_Int_t * vObjSeen;         // object-ID -> seen flag for nodes (base window only)
    Vec_Int_t * vLeafSeen;        // object-ID -> seen flag for leaves (base window only)
    Vec_Int_t * vProcessedCos;    // boolean: CO already in a group
    Vec_Wrd_t * vObjTruths;       // object-ID -> truth table word
    Vec_Wrd_t * vOutTruths;       // per-window output truth tables
    Mosh_BdecCand_t bestGlobal;
    Gia_Man_t * pNew;
    int nVerbosePrinted;          // counter for capped verbose output
    int nObjs, nCos, iCo, jCo;
    int iLeafLimit;

    // clear result and local stats
    Mosh_ResClear( pRes );
    Mosh_BdecStatsClear( &stats );
    Mosh_BdecCandClear( &bestGlobal );
    pNew = NULL;

    // collect before stats
    pRes->nNodesBefore  = Gia_ManAndNum( pGia );
    pRes->nLevelsBefore = Gia_ManLevelNum( pGia );

    // allocate temporaries
    nObjs         = Gia_ManObjNum( pGia );
    nCos          = Gia_ManCoNum( pGia );
    vObjSeen      = Vec_IntStart( nObjs );
    vLeafSeen     = Vec_IntStart( nObjs );
    vProcessedCos = Vec_IntStart( nCos );
    pBaseWin      = Mosh_BdecWinAlloc();
    pCurWin       = Mosh_BdecWinAlloc();
    vObjTruths    = Vec_WrdStart( nObjs );
    vOutTruths    = Vec_WrdAlloc( 32 );

    // limit leaves to 6 for truth table even if user passed a larger value
    iLeafLimit = pPar->nMaxCutSize > 6 ? 6 : pPar->nMaxCutSize;
    nVerbosePrinted = 0;

    // outer loop: each unprocessed CO starts a new group
    for ( iCo = 0; iCo < nCos; iCo++ )
    {
        int fBaseOk;

        if ( Vec_IntEntry( vProcessedCos, iCo ) )
            continue;

        // clear base window for a new group
        Mosh_BdecWinClear( pBaseWin );
        Vec_IntFill( vObjSeen,   nObjs, 0 );
        Vec_IntFill( vLeafSeen,  nObjs, 0 );

        // collect the first CO's cone as the base window
        fBaseOk = Mosh_BdecCollectCoWindow( pGia, iCo, pPar, pBaseWin, vObjSeen, vLeafSeen, &stats );
        if ( !fBaseOk )
        {
            Vec_IntWriteEntry( vProcessedCos, iCo, 1 );
            continue;
        }

        Vec_IntWriteEntry( vProcessedCos, iCo, 1 );

        // sort base window's leaves and nodes for deterministic comparison
        Mosh_BdecWinSortNormalize( pBaseWin );

        // scan later COs to find ones with the same leaf set
        for ( jCo = iCo + 1; jCo < nCos; jCo++ )
        {
            int fOk;

            if ( Vec_IntEntry( vProcessedCos, jCo ) )
                continue;

            // clear current window for this CO
            Mosh_BdecWinClear( pCurWin );

            // use fresh seen maps for the candidate CO to avoid shared-node
            // contamination from the base window
            {
                Vec_Int_t * vTmpObj  = Vec_IntStart( nObjs );
                Vec_Int_t * vTmpLeaf = Vec_IntStart( nObjs );
                fOk = Mosh_BdecCollectCoWindow( pGia, jCo, pPar, pCurWin, vTmpObj, vTmpLeaf, &stats );
                Vec_IntFree( vTmpObj );
                Vec_IntFree( vTmpLeaf );
            }

            if ( !fOk )
                continue;

            Mosh_BdecWinSortNormalize( pCurWin );

            // union-based merge: check if merged leaves stay within limit
            if ( !Mosh_BdecWinCanMergeLeaves( pBaseWin, pCurWin, iLeafLimit ) )
                continue;

            // merge into base window (union leaves, dedup nodes, append outputs)
            Mosh_BdecWinMergeUnion( pBaseWin, pCurWin, pGia, vObjSeen, vLeafSeen );
            Vec_IntWriteEntry( vProcessedCos, jCo, 1 );
        }

        stats.nWindowsTried++;

        // require at least 2 local outputs for multi-output
        if ( Vec_IntSize( pBaseWin->vOutLits ) < 2 )
        {
            stats.nWindowsSkippedSingleOutput++;
            continue;
        }

        stats.nWindowsMultiOutput++;

        // check leaf count for truth table feasibility
        if ( Vec_IntSize( pBaseWin->vLeaves ) == 0 ||
             Vec_IntSize( pBaseWin->vLeaves ) > iLeafLimit )
        {
            if ( Vec_IntSize( pBaseWin->vLeaves ) > iLeafLimit )
                stats.nWindowsSkippedLeaves++;
            continue;
        }

        stats.nWindowsSmallEnough++;

        // Step 4: compute truth tables
        {
            int nLeaves = Vec_IntSize( pBaseWin->vLeaves );
            int fOk;
            Mosh_BdecCand_t bestDecomp;

            Vec_WrdFill( vObjTruths, nObjs, 0 );
            Vec_WrdClear( vOutTruths );
            Mosh_BdecCandClear( &bestDecomp );

            fOk = Mosh_BdecComputeTruths( pGia, pBaseWin, vObjTruths, vOutTruths );
            if ( !fOk )
            {
                stats.nWindowsTruthFailed++;
                if ( pPar->fVerbose && nVerbosePrinted < MOSH_BDEC_TOP_PRINT )
                {
                    fprintf( stdout, "moshare (algo bdec): window truth failed  leaves=%d  nodes=%d  outputs=%d\n",
                        nLeaves,
                        Vec_IntSize( pBaseWin->vNodes ),
                        Vec_IntSize( pBaseWin->vOutLits ) );
                    nVerbosePrinted++;
                }
                continue;
            }

            stats.nTruthComputed += Vec_IntSize( pBaseWin->vOutLits );

            // Step 5: enumerate two-literal divisors
            {
                pNew = Mosh_BdecEvalDivisors( pGia, pBaseWin, vObjTruths,
                    vOutTruths, pPar, &stats, &bestGlobal, &bestDecomp );

                // verbose per-window summary (capped)
                if ( pPar->fVerbose && nVerbosePrinted < MOSH_BDEC_TOP_PRINT )
                {
                    fprintf( stdout, "moshare (algo bdec): window #%d  leaves=%d  nodes=%d  outputs=%d  global_best=%d  best_dec_outs=%d\n",
                        nVerbosePrinted + 1,
                        nLeaves,
                        Vec_IntSize( pBaseWin->vNodes ),
                        Vec_IntSize( pBaseWin->vOutLits ),
                        bestGlobal.nScore,
                        bestDecomp.nDecOutputs );
                    nVerbosePrinted++;
                }
                if ( pNew != NULL )
                    break;
            }
        }
    }

    // collect after stats
    pRes->nNodesAfter   = pNew ? Gia_ManAndNum( pNew ) : pRes->nNodesBefore;
    pRes->nLevelsAfter  = pNew ? Gia_ManLevelNum( pNew ) : pRes->nLevelsBefore;
    pRes->nCandidates   = stats.nDivisorsUnique;
    pRes->nApplied      = stats.nReplacementAccepted;
    pRes->nPartitions   = stats.nWindowsMultiOutput;
    pRes->nRejectedProfit = stats.nRejectedProfit;
    pRes->nRejectedLevel  = stats.nRejectedLevel;
    pRes->fChanged      = pNew != NULL;

    if ( pPar->fStats )
    {
        Mosh_BdecStatsPrint( &stats );
        fprintf( stdout, "moshare (algo bdec): leaf_merge_mode = union\n" );
        fprintf( stdout, "moshare (algo bdec): nodes %d -> %d  levels %d -> %d  changed = %d\n",
            pRes->nNodesBefore, pRes->nNodesAfter,
            pRes->nLevelsBefore, pRes->nLevelsAfter, pRes->fChanged );
    }

    if ( pPar->fVerbose && bestGlobal.nScore > 0 )
    {
        fprintf( stdout, "moshare (algo bdec): best global candidate:\n" );
        fprintf( stdout, "  leaf pair     = %d, %d\n", bestGlobal.iLeaf0, bestGlobal.iLeaf1 );
        fprintf( stdout, "  polarities    = %s%d, %s%d\n",
            bestGlobal.fCompl0 ? "~" : "", bestGlobal.iLeaf0,
            bestGlobal.fCompl1 ? "~" : "", bestGlobal.iLeaf1 );
        fprintf( stdout, "  operator      = %s\n", bestGlobal.fOr ? "OR" : "AND" );
        fprintf( stdout, "  output hits   = %d\n", bestGlobal.nOutputHits );
        fprintf( stdout, "  support hits  = %d\n", bestGlobal.nSupportHits );
        fprintf( stdout, "  dec outputs   = %d\n", bestGlobal.nDecOutputs );
        fprintf( stdout, "  existing node = %d\n", bestGlobal.fExisting );
        if ( bestGlobal.iExistingObj >= 0 )
            fprintf( stdout, "  existing obj  = %d%s\n", bestGlobal.iExistingObj,
                bestGlobal.fExistingCompl ? " (compl)" : "" );
        fprintf( stdout, "  score         = %d\n", bestGlobal.nScore );
        fprintf( stdout, "  truth         = 0x%016llx\n", (unsigned long long)bestGlobal.Truth );
    }

    // free temporaries
    Mosh_BdecWinFree( pBaseWin );
    Mosh_BdecWinFree( pCurWin );
    Vec_IntFree( vObjSeen );
    Vec_IntFree( vLeafSeen );
    Vec_IntFree( vProcessedCos );
    Vec_WrdFree( vObjTruths );
    Vec_WrdFree( vOutTruths );

    return pNew != NULL ? pNew : pGia;
}

ABC_NAMESPACE_IMPL_END
