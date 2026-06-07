/**CFile****************************************************************

  FileName    [moshareAlgoNone.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework — no-op algorithm.]

  Synopsis    [No-op algorithm that reports stats without modifying the network.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 10, 2026.]

***********************************************************************/

#include "moshare.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [No-op algorithm: collects stats without modifying the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Mosh_ManPerformAlgoNone( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes )
{
    // clear result
    Mosh_ResClear( pRes );

    // collect before stats
    pRes->nNodesBefore  = Gia_ManAndNum( pGia );
    pRes->nLevelsBefore = Gia_ManLevelNum( pGia );

    // no transformation
    pRes->nNodesAfter   = pRes->nNodesBefore;
    pRes->nLevelsAfter  = pRes->nLevelsBefore;
    pRes->nCandidates   = 0;
    pRes->nApplied      = 0;
    pRes->fChanged      = 0;

    if ( pPar->fStats )
    {
        fprintf( stdout, "moshare (algo none): nodes = %6d  levels = %4d (unchanged)\n",
            pRes->nNodesBefore, pRes->nLevelsBefore );
    }

    return pGia;
}

ABC_NAMESPACE_IMPL_END
