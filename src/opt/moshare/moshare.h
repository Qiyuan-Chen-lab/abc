/**CFile****************************************************************

  FileName    [moshare.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework.]

  Synopsis    [External declarations.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 10, 2026.]

***********************************************************************/

#ifndef ABC__opt__moshare__moshare_h
#define ABC__opt__moshare__moshare_h


////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "aig/gia/gia.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_HEADER_START

#define MOSH_ALGO_COUNT 2

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef enum Mosh_AlgoId_t_ {
    MOSH_ALGO_NONE  = 0,
    MOSH_ALGO_BDIFF = 1
} Mosh_AlgoId_t;

typedef struct Mosh_Par_t_ {
    int            AlgoId;
    int            nMaxWindowSize;
    int            nMaxCutSize;
    int            nMaxCandidates;
    int            nMaxBddNodes;
    int            nMaxLevelGrowth;
    int            RandomSeed;
    int            fStats;
    int            fVerbose;
    int            fDryRun;
} Mosh_Par_t;

typedef struct Mosh_Res_t_ {
    int            fChanged;
    int            nNodesBefore;
    int            nNodesAfter;
    int            nLevelsBefore;
    int            nLevelsAfter;
    int            nCandidates;
    int            nApplied;
    int            nPartitions;
    int            nRejectedSupport;
    int            nRejectedBdd;
    int            nRejectedProfit;
    int            nRejectedLevel;
} Mosh_Res_t;

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

void         Mosh_ParDefault( Mosh_Par_t * pPar );
void         Mosh_ResClear( Mosh_Res_t * pRes );
void         Mosh_PrintAlgos( FILE * pOut );
int          Mosh_AlgoNameToId( const char * pName );
const char * Mosh_AlgoIdToName( int AlgoId );
Gia_Man_t *  Mosh_ManPerform( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );

/*=== moshareAlgoNone.c =====================================================*/
Gia_Man_t *  Mosh_ManPerformAlgoNone( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );

/*=== moshareAlgoBdiff.c ====================================================*/
Gia_Man_t *  Mosh_ManPerformAlgoBdiff( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes );

ABC_NAMESPACE_HEADER_END

#endif
