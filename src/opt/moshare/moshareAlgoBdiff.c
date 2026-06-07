/**CFile****************************************************************

  FileName    [moshareAlgoBdiff.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework — Boolean-difference algorithm.]

  Synopsis    [Conservative Boolean-difference resubstitution for multi-output sharing.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 11, 2026.]

***********************************************************************/

#include "moshare.h"
#include "bdd/cudd/cuddInt.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                         LOCAL TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Mosh_BdiffCand_t_ {
    int iF;
    int iG;
    int iH;           // existing node ID, or -1 if synthesis mode
    int fComplH;
    int fSynth;       // 1 = synthesize bDiff instead of reusing existing h
    int nMffcF;
    int nDiffBddSize;
    int nGainEst;
    int nGainActual;
    int nLevelGrowth;
} Mosh_BdiffCand_t;

#define MOSH_BDIFF_MAX_MEMO 128

typedef struct Mosh_BdiffMemoEntry_t_ {
    DdNode * pBdd;
    int      iLit;
} Mosh_BdiffMemoEntry_t;

typedef struct Mosh_BdiffMemo_t_ {
    Mosh_BdiffMemoEntry_t entries[MOSH_BDIFF_MAX_MEMO];
    int nEntries;
} Mosh_BdiffMemo_t;

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

////////////////////////////////////////////////////////////////////////
///                     LOCAL FUNCTIONS                              ///
////////////////////////////////////////////////////////////////////////

static void Mosh_BdiffStatsClear( Mosh_BdiffStats_t * p )
{
    p->nPartitions         = 0;
    p->nWindowsSkipped     = 0;
    p->nCandidatesTried    = 0;
    p->nCandidatesWithDiff = 0;
    p->nRejectedSupport    = 0;
    p->nRejectedTopo       = 0;
    p->nRejectedBdd        = 0;
    p->nRejectedProfit     = 0;
    p->nRejectedLevel      = 0;
    p->nAccepted           = 0;
}

static void Mosh_BdiffStatsPrint( Mosh_BdiffStats_t * p, Mosh_Res_t * pRes )
{
    fprintf( stdout, "moshare (algo bdiff): partitions      = %6d\n", p->nPartitions );
    fprintf( stdout, "moshare (algo bdiff): windows skipped = %6d\n", p->nWindowsSkipped );
    fprintf( stdout, "moshare (algo bdiff): candidates tried= %6d\n", p->nCandidatesTried );
    fprintf( stdout, "moshare (algo bdiff): candidates w/diff= %6d\n", p->nCandidatesWithDiff );
    fprintf( stdout, "moshare (algo bdiff): reject_support  = %6d\n", p->nRejectedSupport );
    fprintf( stdout, "moshare (algo bdiff): reject_topo     = %6d\n", p->nRejectedTopo );
    fprintf( stdout, "moshare (algo bdiff): reject_bdd      = %6d\n", p->nRejectedBdd );
    fprintf( stdout, "moshare (algo bdiff): reject_profit   = %6d\n", p->nRejectedProfit );
    fprintf( stdout, "moshare (algo bdiff): reject_level    = %6d\n", p->nRejectedLevel );
    fprintf( stdout, "moshare (algo bdiff): accepted        = %6d\n", p->nAccepted );
    fprintf( stdout, "moshare (algo bdiff): nodes before    = %6d\n", pRes->nNodesBefore );
    fprintf( stdout, "moshare (algo bdiff): levels before   = %6d\n", pRes->nLevelsBefore );
    fprintf( stdout, "moshare (algo bdiff): nodes after     = %6d\n", pRes->nNodesAfter );
    fprintf( stdout, "moshare (algo bdiff): levels after    = %6d\n", pRes->nLevelsAfter );
}

static void Mosh_BdiffResFill( Mosh_Res_t * pRes, Mosh_BdiffStats_t * pStats,
    int nNodesBefore, int nLevelsBefore, int nNodesAfter, int nLevelsAfter,
    int fChanged, int nApplied )
{
    pRes->fChanged         = fChanged;
    pRes->nNodesBefore     = nNodesBefore;
    pRes->nNodesAfter      = nNodesAfter;
    pRes->nLevelsBefore    = nLevelsBefore;
    pRes->nLevelsAfter     = nLevelsAfter;
    pRes->nCandidates      = pStats->nCandidatesTried;
    pRes->nApplied         = nApplied;
    pRes->nPartitions      = pStats->nPartitions;
    pRes->nRejectedSupport = pStats->nRejectedSupport;
    pRes->nRejectedBdd     = pStats->nRejectedBdd;
    pRes->nRejectedProfit  = pStats->nRejectedProfit;
    pRes->nRejectedLevel   = pStats->nRejectedLevel;
}

static void Mosh_BdiffCandClear( Mosh_BdiffCand_t * p )
{
    p->iF          = -1;
    p->iG          = -1;
    p->iH          = -1;
    p->fComplH     = 0;
    p->fSynth      = 0;
    p->nMffcF      = 0;
    p->nDiffBddSize = 0;
    p->nGainEst    = 0;
    p->nGainActual = 0;
    p->nLevelGrowth = 0;
}

static int Mosh_BdiffCandIsValid( Mosh_BdiffCand_t * p )
{
    if ( p->iF < 0 || p->iG < 0 )
        return 0;
    return p->fSynth || p->iH >= 0;
}

////////////////////////////////////////////////////////////////////////
///                 BDD HELPER STRUCTURES                            ///
////////////////////////////////////////////////////////////////////////

typedef struct Mosh_BdiffBddMan_t_ {
    DdManager *   dd;
    Vec_Ptr_t *   vBdds;       // indexed by GIA object ID
    int           nBddsAlloc;
} Mosh_BdiffBddMan_t;

////////////////////////////////////////////////////////////////////////
///                 WINDOW COLLECTION                                ///
////////////////////////////////////////////////////////////////////////

// Collect TFI of a single CO into the provided vectors.
// Uses local dedup (vSeen); limits are checked against per-CO deltas.
// Returns 0 if this CO's TFI alone exceeds limits, 1 on success.
static int Mosh_BdiffCollectOneCoTfi( Gia_Man_t * pGia, int iCo,
    Mosh_Par_t * pPar, Vec_Int_t * vLocalNodes, Vec_Int_t * vLocalSupp )
{
    Gia_Obj_t * pCo, * pRoot;
    Vec_Int_t * vStack;
    Vec_Int_t * vSeen;
    int iObj, iFan0, iFan1, nObjId, fAbort = 0;

    pCo   = Gia_ManCo( pGia, iCo );
    nObjId = Gia_ObjFaninId0p( pGia, pCo );
    if ( nObjId <= 0 )
        return 0;

    vStack = Vec_IntAlloc( 64 );
    vSeen  = Vec_IntAlloc( Gia_ManObjNum( pGia ) );
    Vec_IntFill( vSeen, Gia_ManObjNum( pGia ), 0 );

    Vec_IntPush( vStack, nObjId );

    while ( Vec_IntSize( vStack ) > 0 )
    {
        iObj = Vec_IntPop( vStack );
        if ( iObj <= 0 )
            continue;

        if ( Vec_IntEntry( vSeen, iObj ) )
            continue;
        Vec_IntWriteEntry( vSeen, iObj, 1 );

        pRoot = Gia_ManObj( pGia, iObj );

        if ( Gia_ObjIsCi( pRoot ) )
        {
            Vec_IntPush( vLocalSupp, iObj );
            if ( Vec_IntSize( vLocalSupp ) > pPar->nMaxCutSize )
            {
                fAbort = 1;
                break;
            }
            continue;
        }

        if ( !Gia_ObjIsAnd( pRoot ) )
            continue;

        iFan0 = Gia_ObjFaninId0( pRoot, iObj );
        iFan1 = Gia_ObjFaninId1( pRoot, iObj );
        Vec_IntPush( vStack, iFan0 );
        Vec_IntPush( vStack, iFan1 );

        Vec_IntPush( vLocalNodes, iObj );
        if ( Vec_IntSize( vLocalNodes ) > pPar->nMaxWindowSize )
        {
            fAbort = 1;
            break;
        }
    }

    Vec_IntFree( vStack );
    Vec_IntFree( vSeen );

    if ( fAbort )
    {
        Vec_IntClear( vLocalNodes );
        Vec_IntClear( vLocalSupp );
        return 0;
    }

    return Vec_IntSize( vLocalNodes ) > 0;
}

// Merge a single CO's local nodes/supp into the global merged window.
// Nodes already in vGlobalSeen are skipped; new nodes get coOwner = iCo.
// Returns 1 if the merge succeeded without exceeding limits, 0 if limits hit.
static int Mosh_BdiffTryMergeOneCo( Gia_Man_t * pGia, Vec_Int_t * vLocalNodes,
    Vec_Int_t * vLocalSupp, Vec_Int_t * vNodes, Vec_Int_t * vSupp,
    Vec_Int_t * vCoOwner, Vec_Int_t * vGlobalSeen,
    Mosh_Par_t * pPar, int iCo )
{
    int k, iObj, nNewNodes, nNewSupp;

    // count genuinely new nodes
    nNewNodes = 0;
    Vec_IntForEachEntry( vLocalNodes, iObj, k )
        if ( !Vec_IntEntry( vGlobalSeen, iObj ) )
            nNewNodes++;
    nNewSupp = 0;
    Vec_IntForEachEntry( vLocalSupp, iObj, k )
        if ( !Vec_IntEntry( vGlobalSeen, iObj ) )
            nNewSupp++;

    if ( Vec_IntSize( vNodes ) + nNewNodes > pPar->nMaxWindowSize ||
         Vec_IntSize( vSupp )  + nNewSupp  > pPar->nMaxCutSize )
        return 0;

    // merge local nodes
    Vec_IntForEachEntry( vLocalNodes, iObj, k )
    {
        if ( !Vec_IntEntry( vGlobalSeen, iObj ) )
        {
            Vec_IntWriteEntry( vGlobalSeen, iObj, 1 );
            Vec_IntWriteEntry( vCoOwner, iObj, iCo );
            Vec_IntPush( vNodes, iObj );
        }
    }
    // merge local supp
    Vec_IntForEachEntry( vLocalSupp, iObj, k )
    {
        if ( !Vec_IntEntry( vGlobalSeen, iObj ) )
        {
            Vec_IntWriteEntry( vGlobalSeen, iObj, 1 );
            Vec_IntPush( vSupp, iObj );
        }
    }

    return 1;
}

// Collect a merged window spanning as many COs as possible within limits.
// Each CO's TFI must be complete (no partial-CO abort).
// Nodes are tagged with their originating CO index in vCoOwner.
static int Mosh_BdiffCollectMergedWindow( Gia_Man_t * pGia, Mosh_Par_t * pPar,
    Vec_Int_t * vNodes, Vec_Int_t * vSupp,
    Vec_Int_t * vCoOwner, Vec_Int_t * vGlobalSeen )
{
    Vec_Int_t * vLocalNodes, * vLocalSupp;
    int iCo, nCoCount, nMerged = 0;

    nCoCount = Gia_ManCoNum( pGia );

    Vec_IntClear( vNodes );
    Vec_IntClear( vSupp );
    Vec_IntFill( vGlobalSeen, Gia_ManObjNum( pGia ), 0 );
    Vec_IntFill( vCoOwner, Gia_ManObjNum( pGia ), -1 );

    vLocalNodes = Vec_IntAlloc( pPar->nMaxWindowSize + 64 );
    vLocalSupp  = Vec_IntAlloc( pPar->nMaxCutSize + 16 );

    for ( iCo = 0; iCo < nCoCount; iCo++ )
    {
        Vec_IntClear( vLocalNodes );
        Vec_IntClear( vLocalSupp );

        if ( !Mosh_BdiffCollectOneCoTfi( pGia, iCo, pPar, vLocalNodes, vLocalSupp ) )
        {
            // this CO alone exceeds limits — skip it
            continue;
        }

        if ( !Mosh_BdiffTryMergeOneCo( pGia, vLocalNodes, vLocalSupp,
                 vNodes, vSupp, vCoOwner, vGlobalSeen, pPar, iCo ) )
        {
            // global limits would be exceeded — stop merging
            break;
        }

        nMerged++;
    }

    Vec_IntFree( vLocalNodes );
    Vec_IntFree( vLocalSupp );

    if ( nMerged == 0 )
        return 0;

    Vec_IntSort( vNodes, 0 );
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                 BDD CONSTRUCTION                                 ///
////////////////////////////////////////////////////////////////////////

static int Mosh_BdiffFindSuppIndex( Vec_Int_t * vSupp, int iObj )
{
    int k;
    for ( k = 0; k < Vec_IntSize( vSupp ); k++ )
        if ( Vec_IntEntry( vSupp, k ) == iObj )
            return k;
    return -1;
}

static Mosh_BdiffBddMan_t * Mosh_BdiffBddManAlloc( int nObjs )
{
    Mosh_BdiffBddMan_t * p;
    p = (Mosh_BdiffBddMan_t *)malloc( sizeof( Mosh_BdiffBddMan_t ) );
    p->dd         = NULL;
    p->vBdds      = Vec_PtrStart( nObjs );
    p->nBddsAlloc = nObjs;
    return p;
}

static void Mosh_BdiffBddManFree( Mosh_BdiffBddMan_t * p )
{
    if ( p == NULL )
        return;
    if ( p->vBdds != NULL )
    {
        int i;
        for ( i = 0; i < Vec_PtrSize( p->vBdds ); i++ )
        {
            DdNode * b = (DdNode *)Vec_PtrEntry( p->vBdds, i );
            if ( b != NULL )
            {
                Cudd_RecursiveDeref( p->dd, b );
                Vec_PtrWriteEntry( p->vBdds, i, NULL );
            }
        }
        Vec_PtrFree( p->vBdds );
    }
    if ( p->dd != NULL )
        Cudd_Quit( p->dd );
    free( p );
}

static DdNode * Mosh_BdiffBddManLookup( Mosh_BdiffBddMan_t * p, int iObj )
{
    if ( iObj < 0 || iObj >= Vec_PtrSize( p->vBdds ) )
        return NULL;
    return (DdNode *)Vec_PtrEntry( p->vBdds, iObj );
}

static void Mosh_BdiffBddManStore( Mosh_BdiffBddMan_t * p, int iObj, DdNode * b )
{
    assert( iObj >= 0 && iObj < Vec_PtrSize( p->vBdds ) );
    assert( Vec_PtrEntry( p->vBdds, iObj ) == NULL );
    Vec_PtrWriteEntry( p->vBdds, iObj, b );
}

// Build BDD for a literal using BDD of the driver node
static DdNode * Mosh_BdiffBuildLitBdd( Mosh_BdiffBddMan_t * pMan, int iLit )
{
    int iObj   = Abc_Lit2Var( iLit );
    int fCompl = Abc_LitIsCompl( iLit );
    DdNode * b = Mosh_BdiffBddManLookup( pMan, iObj );
    if ( b == NULL )
        return NULL;
    return fCompl ? Cudd_Not( b ) : b;
}

static int Mosh_BdiffBuildWindowBdds( Gia_Man_t * pGia, Vec_Int_t * vNodes,
    Vec_Int_t * vSupp, Mosh_Par_t * pPar, Mosh_BdiffBddMan_t ** ppMan )
{
    Mosh_BdiffBddMan_t * pMan = NULL;
    DdManager * dd;
    DdNode * bConst0, * bVar, * b0, * b1, * bAnd;
    int nVars, nObjs, i, iObj, iLit0, iLit1;

    *ppMan = NULL;
    nVars  = Vec_IntSize( vSupp );
    nObjs  = Gia_ManObjNum( pGia );

    if ( nVars == 0 )
        return 0;

    dd = Cudd_Init( nVars, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0 );
    if ( dd == NULL )
        return 0;
    Cudd_AutodynDisable( dd );

    pMan = Mosh_BdiffBddManAlloc( nObjs );
    pMan->dd = dd;

    bConst0 = Cudd_ReadLogicZero( dd );
    Cudd_Ref( bConst0 );
    Mosh_BdiffBddManStore( pMan, 0, bConst0 );

    // store BDD for each support variable
    for ( i = 0; i < nVars; i++ )
    {
        iObj = Vec_IntEntry( vSupp, i );
        bVar = Cudd_bddIthVar( dd, i );
        Cudd_Ref( bVar );
        Mosh_BdiffBddManStore( pMan, iObj, bVar );
    }

    // build BDDs for each node in vNodes order (sorted by ID = topological)
    for ( i = 0; i < Vec_IntSize( vNodes ); i++ )
    {
        iObj = Vec_IntEntry( vNodes, i );
        Gia_Obj_t * pObj = Gia_ManObj( pGia, iObj );

        if ( !Gia_ObjIsAnd( pObj ) )
            continue;

        iLit0 = Gia_ObjFaninLit0p( pGia, pObj );
        iLit1 = Gia_ObjFaninLit1p( pGia, pObj );

        b0 = Mosh_BdiffBuildLitBdd( pMan, iLit0 );
        b1 = Mosh_BdiffBuildLitBdd( pMan, iLit1 );

        if ( b0 == NULL || b1 == NULL )
            goto bail;

        bAnd = Cudd_bddAnd( dd, b0, b1 );
        if ( bAnd == NULL )
            goto bail;
        Cudd_Ref( bAnd );

        // cap on BDD manager size
        if ( Cudd_ReadNodeCount( dd ) > 100000 )
        {
            Cudd_RecursiveDeref( dd, bAnd );
            goto bail;
        }

        Mosh_BdiffBddManStore( pMan, iObj, bAnd );
    }

    *ppMan = pMan;
    return 1;

bail:
    Mosh_BdiffBddManFree( pMan );
    return 0;
}

////////////////////////////////////////////////////////////////////////
///                 BDD-TO-AIG SYNTHESIS                              ///
////////////////////////////////////////////////////////////////////////

static int Mosh_BdiffSynthBddToAigRec( Gia_Man_t * pNew, DdManager * dd,
    DdNode * bNode, int * pSuppLits, Mosh_BdiffMemo_t * pMemo )
{
    DdNode * bReg, * bThen, * bElse;
    int fCompl, nVarId, iVarLit, iLitThen, iLitElse, iLit;
    int i;

    // check memo table
    for ( i = 0; i < pMemo->nEntries; i++ )
        if ( pMemo->entries[i].pBdd == bNode )
            return pMemo->entries[i].iLit;

    fCompl = Cudd_IsComplement( bNode );
    bReg   = Cudd_Regular( bNode );

    // constant node
    if ( bReg == Cudd_ReadLogicZero( dd ) )
        return fCompl ? 1 : 0;

    nVarId  = Cudd_NodeReadIndex( bReg );
    iVarLit = pSuppLits[ nVarId ];

    bThen = Cudd_T( bReg );
    bElse = Cudd_E( bReg );

    if ( fCompl )
    {
        bThen = Cudd_Not( bThen );
        bElse = Cudd_Not( bElse );
    }

    iLitThen = Mosh_BdiffSynthBddToAigRec( pNew, dd, bThen, pSuppLits, pMemo );
    iLitElse = Mosh_BdiffSynthBddToAigRec( pNew, dd, bElse, pSuppLits, pMemo );

    iLit = Gia_ManHashMux( pNew, iVarLit, iLitThen, iLitElse );

    // memoize
    assert( pMemo->nEntries < MOSH_BDIFF_MAX_MEMO );
    pMemo->entries[ pMemo->nEntries ].pBdd = bNode;
    pMemo->entries[ pMemo->nEntries ].iLit = iLit;
    pMemo->nEntries++;

    return iLit;
}

static int Mosh_BdiffSynthBddToAig( Gia_Man_t * pNew, DdManager * dd,
    DdNode * bNode, int * pSuppLits )
{
    Mosh_BdiffMemo_t Memo;
    Memo.nEntries = 0;
    return Mosh_BdiffSynthBddToAigRec( pNew, dd, bNode, pSuppLits, &Memo );
}

////////////////////////////////////////////////////////////////////////
///                 CANDIDATE EVALUATION                             ///
////////////////////////////////////////////////////////////////////////

static int Mosh_BdiffEvalPairExistingDiff( Gia_Man_t * pGia, Mosh_Par_t * pPar,
    Mosh_BdiffBddMan_t * pMan, Vec_Int_t * vNodes, Vec_Int_t * vCoOwner,
    int iF, int iG, Mosh_BdiffCand_t * pCand, Mosh_BdiffStats_t * pStats )
{
    DdManager * dd = pMan->dd;
    DdNode * bF, * bG, * bDiff, * bH;
    int iH, k, fComplH, nDiffSize, nMffcF;

    pStats->nCandidatesTried++;

    // topological constraint: g must be before f
    if ( iG >= iF )
    {
        pStats->nRejectedTopo++;
        return 0;
    }

    // require f has non-trivial MFFC
    nMffcF = Gia_NodeMffcSize( pGia, Gia_ManObj( pGia, iF ) );
    if ( nMffcF <= 1 )
    {
        pStats->nRejectedProfit++;
        return 0;
    }

    bF = Mosh_BdiffBddManLookup( pMan, iF );
    bG = Mosh_BdiffBddManLookup( pMan, iG );

    if ( bF == NULL || bG == NULL )
    {
        pStats->nRejectedBdd++;
        return 0;
    }

    // compute bdd_diff = bF xor bG
    bDiff = Cudd_bddXor( dd, bF, bG );
    if ( bDiff == NULL )
    {
        pStats->nRejectedBdd++;
        return 0;
    }
    Cudd_Ref( bDiff );

    // reject if diff is constant (trivial)
    if ( Cudd_IsConstant( bDiff ) )
    {
        Cudd_RecursiveDeref( dd, bDiff );
        pStats->nRejectedBdd++;
        return 0;
    }

    nDiffSize = Cudd_DagSize( bDiff );
    if ( nDiffSize > pPar->nMaxBddNodes )
    {
        Cudd_RecursiveDeref( dd, bDiff );
        pStats->nRejectedBdd++;
        return 0;
    }

    pStats->nCandidatesWithDiff++;

    // look for existing node h whose BDD equals bDiff or ~bDiff
    iH = -1;
    fComplH = 0;
    for ( k = 0; k < Vec_IntSize( vNodes ); k++ )
    {
        int iCand = Vec_IntEntry( vNodes, k );
        if ( iCand == iF || iCand == iG )
            continue;
        if ( iCand >= iF )
            break;

        bH = Mosh_BdiffBddManLookup( pMan, iCand );
        if ( bH == NULL )
            continue;

        if ( bDiff == bH )
        {
            iH = iCand;
            fComplH = 0;
            break;
        }
        // check complement: ~bDiff == bH  <->  bDiff == ~bH
        if ( Cudd_Not( bDiff ) == bH )
        {
            iH = iCand;
            fComplH = 1;
            break;
        }
    }

    Cudd_RecursiveDeref( dd, bDiff );

    // fill common candidate fields
    pCand->iF           = iF;
    pCand->iG           = iG;
    pCand->nMffcF       = nMffcF;
    pCand->nDiffBddSize = nDiffSize;
    pCand->nGainActual  = 0;
    pCand->nLevelGrowth = 0;

    if ( iH >= 0 )
    {
        // existing-diff: reuse node h
        assert( iH < iF );
        pCand->iH       = iH;
        pCand->fComplH  = fComplH;
        pCand->fSynth   = 0;
        pCand->nGainEst = nMffcF - 1; // one XOR replaces MFFC(f)
        // cross-CO bonus: pairs from different cones are more likely profitable
        if ( vCoOwner != NULL && Vec_IntEntry( vCoOwner, iF ) != Vec_IntEntry( vCoOwner, iG ) )
            pCand->nGainEst += 5;
        return 1;
    }

    // synthesis fallback: BDD is small enough, try to synthesize (f xor g) as AIG
    if ( nDiffSize <= pPar->nMaxBddNodes )
    {
        int nSynthCostEst = 3 * nDiffSize + 2; // each BDD node → MUX(~3 ANDs) + XOR
        if ( nMffcF > nSynthCostEst )
        {
            pCand->iH       = -1;
            pCand->fComplH  = 0;
            pCand->fSynth   = 1;
            pCand->nGainEst = nMffcF - nSynthCostEst;
            if ( vCoOwner != NULL && Vec_IntEntry( vCoOwner, iF ) != Vec_IntEntry( vCoOwner, iG ) )
                pCand->nGainEst += 3;
            return 1;
        }
        pStats->nRejectedProfit++;
        return 0;
    }

    pStats->nRejectedBdd++;
    return 0;
}

static int Mosh_BdiffFindBestInWindow( Gia_Man_t * pGia, Mosh_Par_t * pPar,
    Mosh_BdiffBddMan_t * pMan, Vec_Int_t * vNodes, Vec_Int_t * vSupp,
    Vec_Int_t * vCoOwner, Mosh_BdiffCand_t * pBest, Mosh_BdiffStats_t * pStats )
{
    int i, j, iF, iG, nSize;
    Mosh_BdiffCand_t Cand;

    nSize = Vec_IntSize( vNodes );

    for ( i = 0; i < nSize; i++ )
    {
        iF = Vec_IntEntry( vNodes, i );

        // early bound check
        if ( pStats->nCandidatesTried >= pPar->nMaxCandidates )
            return Mosh_BdiffCandIsValid( pBest );

        for ( j = 0; j < i; j++ )
        {
            iG = Vec_IntEntry( vNodes, j );

            if ( pStats->nCandidatesTried >= pPar->nMaxCandidates )
                return Mosh_BdiffCandIsValid( pBest );

            Mosh_BdiffCandClear( &Cand );

            if ( !Mosh_BdiffEvalPairExistingDiff( pGia, pPar, pMan, vNodes,
                     vCoOwner, iF, iG, &Cand, pStats ) )
                continue;

            if ( Cand.nGainEst > pBest->nGainEst )
            {
                *pBest = Cand;
            }
        }
    }

    return Mosh_BdiffCandIsValid( pBest );
}

////////////////////////////////////////////////////////////////////////
///                 NETWORK REBUILD                                  ///
////////////////////////////////////////////////////////////////////////


static Gia_Man_t * Mosh_BdiffRebuildWithOneReplacement( Gia_Man_t * pGia,
    Mosh_BdiffCand_t * pCand, Mosh_BdiffBddMan_t * pMan, Vec_Int_t * vSupp )
{
    Gia_Man_t * pNew;
    Gia_Obj_t * pObj;
    int iObj, nLitG, nLitH;

    pNew = Gia_ManStart( Gia_ManObjNum( pGia ) + 20 );
    if ( pNew == NULL )
        return NULL;

    pNew->pName = Abc_UtilStrsav( pGia->pName );
    pNew->pSpec = Abc_UtilStrsav( pGia->pSpec );

    Gia_ManConst0( pGia )->Value = 0;
    Gia_ManHashStart( pNew );

    Gia_ManForEachObj1( pGia, pObj, iObj )
    {
        if ( Gia_ObjIsCi( pObj ) )
        {
            pObj->Value = Gia_ManAppendCi( pNew );
        }
        else if ( Gia_ObjIsAnd( pObj ) )
        {
            if ( iObj == pCand->iF )
            {
                nLitG = Gia_ManObj( pGia, pCand->iG )->Value;

                if ( pCand->fSynth )
                {
                    DdManager * dd = pMan->dd;
                    DdNode * bF, * bG, * bDiff;
                    int nSupp, k, iLitDiff;
                    int * pSuppLits;

                    bF = Mosh_BdiffBddManLookup( pMan, pCand->iF );
                    bG = Mosh_BdiffBddManLookup( pMan, pCand->iG );
                    bDiff = Cudd_bddXor( dd, bF, bG );
                    if ( bDiff == NULL )
                    {
                        Gia_ManHashStop( pNew );
                        Gia_ManStop( pNew );
                        return NULL;
                    }
                    Cudd_Ref( bDiff );

                    nSupp = Vec_IntSize( vSupp );
                    pSuppLits = (int *)malloc( nSupp * sizeof( int ) );
                    for ( k = 0; k < nSupp; k++ )
                    {
                        int iSuppId = Vec_IntEntry( vSupp, k );
                        pSuppLits[ k ] = Gia_ManObj( pGia, iSuppId )->Value;
                    }

                    iLitDiff = Mosh_BdiffSynthBddToAig( pNew, dd, bDiff, pSuppLits );
                    free( pSuppLits );
                    Cudd_RecursiveDeref( dd, bDiff );

                    pObj->Value = Gia_ManHashXor( pNew, iLitDiff, nLitG );
                }
                else
                {
                    nLitH = Gia_ManObj( pGia, pCand->iH )->Value;
                    if ( pCand->fComplH )
                        nLitH = Abc_LitNot( nLitH );
                    pObj->Value = Gia_ManHashXor( pNew, nLitH, nLitG );
                }
            }
            else
            {
                pObj->Value = Gia_ManHashAnd( pNew,
                    Gia_ObjFanin0Copy( pObj ), Gia_ObjFanin1Copy( pObj ) );
            }
        }
        else if ( Gia_ObjIsCo( pObj ) )
        {
            pObj->Value = Gia_ManAppendCo( pNew, Gia_ObjFanin0Copy( pObj ) );
        }
    }

    Gia_ManHashStop( pNew );
    Gia_ManSetRegNum( pNew, Gia_ManRegNum( pGia ) );

    return pNew;
}

static Gia_Man_t * Mosh_BdiffTryApplyCandidate( Gia_Man_t * pGia,
    Mosh_Par_t * pPar, Mosh_BdiffCand_t * pCand, Mosh_BdiffStats_t * pStats,
    Mosh_BdiffBddMan_t * pMan, Vec_Int_t * vSupp )
{
    Gia_Man_t * pNew, * pClean;
    int nOldAnd, nNewAnd, nOldLevel, nNewLevel, nLevelGrowth;

    // safety checks
    if ( !Gia_ObjIsAnd( Gia_ManObj( pGia, pCand->iF ) ) )
        return NULL;
    if ( !Gia_ObjIsAnd( Gia_ManObj( pGia, pCand->iG ) ) )
        return NULL;
    if ( !pCand->fSynth )
    {
        if ( !Gia_ObjIsAnd( Gia_ManObj( pGia, pCand->iH ) ) )
            return NULL;
        if ( pCand->iH >= pCand->iF )
            return NULL;
    }
    if ( pCand->iG >= pCand->iF )
        return NULL;

    // dry-run: skip actual rebuild
    if ( pPar->fDryRun )
    {
        pStats->nAccepted++;
        return NULL;
    }

    pNew = Mosh_BdiffRebuildWithOneReplacement( pGia, pCand, pMan, vSupp );
    if ( pNew == NULL )
        return NULL;

    // cleanup
    pClean = Gia_ManCleanup( pNew );
    if ( pClean != pNew )
    {
        Gia_ManStop( pNew );
        pNew = pClean;
    }

    // measure actual change
    nOldAnd   = Gia_ManAndNum( pGia );
    nNewAnd   = Gia_ManAndNum( pNew );

    // recompute levels for accurate measurement (Gia_ManLevelNum triggers computation)
    nOldLevel  = Gia_ManLevelNum( pGia );
    nNewLevel  = Gia_ManLevelNum( pNew );

    nLevelGrowth = nNewLevel - nOldLevel;
    pCand->nLevelGrowth = nLevelGrowth;
    pCand->nGainActual  = nOldAnd - nNewAnd;

    // profitability: must strictly reduce AND count
    if ( pCand->nGainActual <= 0 )
    {
        if ( pPar->fVerbose )
            fprintf( stdout, "moshare (algo bdiff): rebuild f=%d g=%d h=%d old=%d new=%d gain=%d mffc_est=%d level_grow=%d (rejected: no gain)\n",
                pCand->iF, pCand->iG, pCand->iH, nOldAnd, nNewAnd, pCand->nGainActual,
                pCand->nMffcF, nLevelGrowth );
        pStats->nRejectedProfit++;
        Gia_ManStop( pNew );
        return NULL;
    }

    // level check
    if ( nLevelGrowth > pPar->nMaxLevelGrowth )
    {
        if ( pPar->fVerbose )
            fprintf( stdout, "moshare (algo bdiff): rebuild f=%d g=%d h=%d old=%d new=%d gain=%d level_grow=%d (rejected: level)\n",
                pCand->iF, pCand->iG, pCand->iH, nOldAnd, nNewAnd, pCand->nGainActual, nLevelGrowth );
        pStats->nRejectedLevel++;
        Gia_ManStop( pNew );
        return NULL;
    }

    pStats->nAccepted++;
    return pNew;
}

////////////////////////////////////////////////////////////////////////
///                 MAIN ENTRY POINT                                 ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Boolean-difference sharing algorithm.]

  Description [Finds AIG node pairs (f, g) where func(h) == func(f) xor func(g)
  for some existing node h, then replaces f by h xor g. This reuses existing
  logic for multi-output sharing. Operates on a per-CO window basis.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Mosh_ManPerformAlgoBdiff( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes )
{
    Mosh_BdiffStats_t Stats, * pStats = &Stats;
    Mosh_BdiffCand_t Best, * pBest = &Best;
    Mosh_BdiffBddMan_t * pMan = NULL;
    Vec_Int_t * vNodes = NULL, * vSupp = NULL, * vGlobalSeen = NULL, * vCoOwner = NULL;
    Gia_Man_t * pNew = NULL;
    int nNodesBefore, nLevelsBefore, nNodesAfter, nLevelsAfter;
    int nCoCount;

    Mosh_ResClear( pRes );
    Mosh_BdiffStatsClear( pStats );
    Mosh_BdiffCandClear( pBest );

    if ( pGia == NULL )
        return NULL;

    nNodesBefore  = Gia_ManAndNum( pGia );
    nLevelsBefore = Gia_ManLevelNum( pGia );

    // zero bounds = no-op
    if ( pPar->nMaxWindowSize <= 0 || pPar->nMaxCutSize <= 0 ||
         pPar->nMaxCandidates <= 0 || pPar->nMaxBddNodes <= 0 )
    {
        if ( pPar->fStats )
        {
            Mosh_BdiffResFill( pRes, pStats, nNodesBefore, nLevelsBefore,
                nNodesBefore, nLevelsBefore, 0, 0 );
            Mosh_BdiffStatsPrint( pStats, pRes );
        }
        return pGia;
    }

    // ensure levels and refs are available
    Gia_ManLevelNum( pGia );
    Gia_ManCreateRefs( pGia );

    vNodes      = Vec_IntAlloc( pPar->nMaxWindowSize + 64 );
    vSupp       = Vec_IntAlloc( pPar->nMaxCutSize + 16 );
    vGlobalSeen = Vec_IntStart( Gia_ManObjNum( pGia ) );
    vCoOwner    = Vec_IntStart( Gia_ManObjNum( pGia ) );

    // Collect a merged window from as many COs as fits within limits.
    // Each node is tagged with its originating CO index for cross-CO pairing.
    if ( !Mosh_BdiffCollectMergedWindow( pGia, pPar, vNodes, vSupp, vCoOwner, vGlobalSeen ) )
    {
        // no window collected — every CO's TFI exceeds limits individually
        Vec_IntFree( vNodes );
        Vec_IntFree( vSupp );
        Vec_IntFree( vGlobalSeen );
        Vec_IntFree( vCoOwner );

        Mosh_BdiffResFill( pRes, pStats, nNodesBefore, nLevelsBefore,
            nNodesBefore, nLevelsBefore, 0, 0 );
        if ( pPar->fStats )
            Mosh_BdiffStatsPrint( pStats, pRes );
        return pGia;
    }

    pStats->nPartitions = 1;

    if ( pPar->fVerbose )
    {
        nCoCount = Gia_ManCoNum( pGia );
        fprintf( stdout, "moshare (algo bdiff): merged window nodes=%d supp=%d (from %d COs)\n",
            Vec_IntSize( vNodes ), Vec_IntSize( vSupp ), nCoCount );
    }

    // Build BDDs for the merged window
    if ( !Mosh_BdiffBuildWindowBdds( pGia, vNodes, vSupp, pPar, &pMan ) )
    {
        Mosh_BdiffBddManFree( pMan );
        Vec_IntFree( vNodes );
        Vec_IntFree( vSupp );
        Vec_IntFree( vGlobalSeen );
        Vec_IntFree( vCoOwner );

        Mosh_BdiffResFill( pRes, pStats, nNodesBefore, nLevelsBefore,
            nNodesBefore, nLevelsBefore, 0, 0 );
        if ( pPar->fStats )
            Mosh_BdiffStatsPrint( pStats, pRes );
        return pGia;
    }

    // Search for the best candidate within the merged window
    Mosh_BdiffCandClear( pBest );
    Mosh_BdiffFindBestInWindow( pGia, pPar, pMan, vNodes, vSupp, vCoOwner, pBest, pStats );

    if ( Mosh_BdiffCandIsValid( pBest ) )
    {
        if ( pPar->fVerbose )
        {
            int nCrossCo = ( Vec_IntEntry( vCoOwner, pBest->iF ) != Vec_IntEntry( vCoOwner, pBest->iG ) );
            fprintf( stdout, "moshare (algo bdiff): found candidate f=%d(co=%d) g=%d(co=%d) h=%d synth=%d cross_co=%d gain_est=%d\n",
                pBest->iF, Vec_IntEntry( vCoOwner, pBest->iF ),
                pBest->iG, Vec_IntEntry( vCoOwner, pBest->iG ),
                pBest->iH, pBest->fSynth, nCrossCo, pBest->nGainEst );
        }

        pNew = Mosh_BdiffTryApplyCandidate( pGia, pPar, pBest, pStats, pMan, vSupp );
    }

    Mosh_BdiffBddManFree( pMan );
    pMan = NULL;

    // handle result
    if ( pNew != NULL )
    {
        // success
        nNodesAfter  = Gia_ManAndNum( pNew );
        nLevelsAfter = Gia_ManLevelNum( pNew );

        Mosh_BdiffResFill( pRes, pStats, nNodesBefore, nLevelsBefore,
            nNodesAfter, nLevelsAfter, 1, 1 );

        if ( pPar->fStats )
            Mosh_BdiffStatsPrint( pStats, pRes );

        if ( pPar->fVerbose )
        {
            fprintf( stdout, "moshare (algo bdiff): applied f=%d g=%d h=%d synth=%d gain=%d nodes %d->%d\n",
                pBest->iF, pBest->iG, pBest->iH, pBest->fSynth, pBest->nGainActual,
                nNodesBefore, nNodesAfter );
        }
    }
    else
    {
        // unchanged
        Mosh_BdiffResFill( pRes, pStats, nNodesBefore, nLevelsBefore,
            nNodesBefore, nLevelsBefore, 0, 0 );

        if ( pPar->fStats )
            Mosh_BdiffStatsPrint( pStats, pRes );

        if ( pPar->fDryRun && Mosh_BdiffCandIsValid( pBest ) )
        {
            fprintf( stdout, "moshare (algo bdiff): dry-run candidate f=%d g=%d h=%d synth=%d gain_est=%d\n",
                pBest->iF, pBest->iG, pBest->iH, pBest->fSynth, pBest->nGainEst );
        }
    }

    Vec_IntFree( vNodes );
    Vec_IntFree( vSupp );
    Vec_IntFree( vGlobalSeen );
    Vec_IntFree( vCoOwner );

    return pNew != NULL ? pNew : pGia;
}

ABC_NAMESPACE_IMPL_END
