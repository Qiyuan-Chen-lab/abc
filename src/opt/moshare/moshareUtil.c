/**CFile****************************************************************

  FileName    [moshareUtil.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Multi-output sharing optimization framework — shared utilities.]

  Synopsis    [Parameter defaults, result clearing, and algorithm name registry.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 10, 2026.]

***********************************************************************/

#include "moshare.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Sets default parameter values.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mosh_ParDefault( Mosh_Par_t * pPar )
{
    pPar->AlgoId         = MOSH_ALGO_NONE;
    pPar->nMaxWindowSize  = 64;
    pPar->nMaxCutSize     = 8;
    pPar->nMaxCandidates  = 100;
    pPar->nMaxBddNodes    = 16;
    pPar->nMaxLevelGrowth = 0;
    pPar->RandomSeed      = 0;
    pPar->fStats          = 0;
    pPar->fVerbose        = 0;
    pPar->fDryRun         = 0;
}

/**Function*************************************************************

  Synopsis    [Clears the result struct.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mosh_ResClear( Mosh_Res_t * pRes )
{
    pRes->fChanged      = 0;
    pRes->nNodesBefore  = 0;
    pRes->nNodesAfter   = 0;
    pRes->nLevelsBefore = 0;
    pRes->nLevelsAfter  = 0;
    pRes->nCandidates   = 0;
    pRes->nApplied      = 0;
}

/**Function*************************************************************

  Synopsis    [Maps algorithm name to ID.]

  Description [Returns -1 if the name is unknown.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mosh_AlgoNameToId( const char * pName )
{
    if ( pName == NULL )
        return -1;
    if ( strcmp( pName, "none" ) == 0 )
        return MOSH_ALGO_NONE;
    if ( strcmp( pName, "bdiff" ) == 0 )
        return MOSH_ALGO_BDIFF;
    return -1;
}

/**Function*************************************************************

  Synopsis    [Maps algorithm ID to name.]

  Description [Returns NULL if the ID is unknown.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
const char * Mosh_AlgoIdToName( int AlgoId )
{
    switch ( AlgoId )
    {
    case MOSH_ALGO_NONE:  return "none";
    case MOSH_ALGO_BDIFF: return "bdiff";
    default:              return NULL;
    }
}

/**Function*************************************************************

  Synopsis    [Prints available algorithms to the given output.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mosh_PrintAlgos( FILE * pOut )
{
    int i;
    fprintf( pOut, "Available moshare algorithms:\n" );
    for ( i = 0; i < MOSH_ALGO_COUNT; i++ )
        fprintf( pOut, "  %s\n", Mosh_AlgoIdToName( i ) );
}

ABC_NAMESPACE_IMPL_END
