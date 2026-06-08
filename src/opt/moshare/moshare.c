/**CFile****************************************************************

  FileName    [moshare.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework — dispatcher.]

  Synopsis    [Algorithm dispatcher and public entry point.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 10, 2026.]

***********************************************************************/

#include "moshare.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Dispatches to the appropriate algorithm based on AlgoId.]

  Description [Returns the resulting Gia_Man_t. If unchanged, the input
  pGia may be returned directly. The caller is responsible for freeing
  or updating the frame as appropriate based on pRes->fChanged.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Mosh_ManPerform( Gia_Man_t * pGia, Mosh_Par_t * pPar, Mosh_Res_t * pRes )
{
    if ( pGia == NULL )
        return NULL;

    switch ( pPar->AlgoId )
    {
    case MOSH_ALGO_NONE:
        return Mosh_ManPerformAlgoNone( pGia, pPar, pRes );
    case MOSH_ALGO_BDIFF:
        return Mosh_ManPerformAlgoBdiff( pGia, pPar, pRes );
    case MOSH_ALGO_BDEC:
        return Mosh_ManPerformAlgoBdec( pGia, pPar, pRes );
    default:
        Mosh_ResClear( pRes );
        if ( pPar->fVerbose )
            fprintf( stdout, "moshare: unknown algorithm ID %d, falling back to no-op\n", pPar->AlgoId );
        return pGia;
    }
}

ABC_NAMESPACE_IMPL_END
