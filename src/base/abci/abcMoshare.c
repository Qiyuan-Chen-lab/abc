/**CFile****************************************************************

  FileName    [abcMoshare.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Command wrapper for multi-output sharing optimization framework.]

  Author      [Contest]

  Date        [Ver. 1.0. Started - May 10, 2026.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "opt/moshare/moshare.h"
#include "aig/gia/giaAig.h"

ABC_NAMESPACE_IMPL_START

extern Abc_Ntk_t * Abc_NtkFromAigPhase( Aig_Man_t * pMan );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [ABC command for multi-output sharing optimization.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandMoshare( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
    Mosh_Par_t Par, * pPar = &Par;
    Mosh_Res_t Res, * pRes = &Res;
    Gia_Man_t * pGia = NULL, * pTemp;
    Abc_Ntk_t * pNtk = NULL;
    Aig_Man_t * pAig;
    char * pAlgoName = NULL;
    int c, fOwnGia = 0;

    Mosh_ParDefault( pPar );
    Mosh_ResClear( pRes );

    // manual argument parsing (Extra_UtilGetopt does not handle -algo well)
    for ( c = 1; c < argc; c++ )
    {
        if ( strcmp( argv[c], "-h" ) == 0 )
        {
            goto usage;
        }
        else if ( strcmp( argv[c], "-algo" ) == 0 && c + 1 < argc )
        {
            pAlgoName = argv[++c];
        }
        else if ( strcmp( argv[c], "-maxwin" ) == 0 && c + 1 < argc )
        {
            pPar->nMaxWindowSize = atoi( argv[++c] );
            if ( pPar->nMaxWindowSize < 0 )
                goto usage;
        }
        else if ( strcmp( argv[c], "-maxcut" ) == 0 && c + 1 < argc )
        {
            pPar->nMaxCutSize = atoi( argv[++c] );
            if ( pPar->nMaxCutSize < 0 )
                goto usage;
        }
        else if ( strcmp( argv[c], "-maxcand" ) == 0 && c + 1 < argc )
        {
            pPar->nMaxCandidates = atoi( argv[++c] );
            if ( pPar->nMaxCandidates < 0 )
                goto usage;
        }
        else if ( strcmp( argv[c], "-maxbdd" ) == 0 && c + 1 < argc )
        {
            pPar->nMaxBddNodes = atoi( argv[++c] );
            if ( pPar->nMaxBddNodes < 0 )
                goto usage;
        }
        else if ( strcmp( argv[c], "-maxgrow" ) == 0 && c + 1 < argc )
        {
            pPar->nMaxLevelGrowth = atoi( argv[++c] );
            if ( pPar->nMaxLevelGrowth < 0 )
                goto usage;
        }
        else if ( strcmp( argv[c], "-seed" ) == 0 && c + 1 < argc )
        {
            pPar->RandomSeed = atoi( argv[++c] );
        }
        else if ( strcmp( argv[c], "-dryrun" ) == 0 )
        {
            pPar->fDryRun ^= 1;
        }
        else if ( strcmp( argv[c], "-stats" ) == 0 )
        {
            pPar->fStats ^= 1;
        }
        else if ( strcmp( argv[c], "-v" ) == 0 )
        {
            pPar->fVerbose ^= 1;
        }
        else
        {
            Abc_Print( -1, "Unknown option \"%s\".\n", argv[c] );
            goto usage;
        }
    }

    // -algo is required
    if ( pAlgoName == NULL )
    {
        Abc_Print( -1, "Algorithm name is required. Use \"moshare -algo <name>\".\n" );
        Abc_Print( -1, "Use \"moshare -algo list\" to see available algorithms.\n" );
        goto usage;
    }

    // handle -algo list
    if ( strcmp( pAlgoName, "list" ) == 0 )
    {
        Mosh_PrintAlgos( pAbc->Out );
        return 0;
    }

    // resolve algorithm name
    pPar->AlgoId = Mosh_AlgoNameToId( pAlgoName );
    if ( pPar->AlgoId < 0 )
    {
        Abc_Print( -1, "Unknown algorithm \"%s\".\n", pAlgoName );
        Abc_Print( -1, "Use \"moshare -algo list\" to see available algorithms.\n" );
        return 1;
    }

    // obtain Gia_Man_t
    pGia = Abc_FrameReadGia( pAbc );
    if ( pGia == NULL )
    {
        // try to convert from the legacy network
        pNtk = Abc_FrameReadNtk( pAbc );
        if ( pNtk == NULL )
        {
            Abc_Print( -1, "There is no network available.\n" );
            return 1;
        }
        if ( !Abc_NtkIsStrash( pNtk ) )
        {
            Abc_Print( -1, "The current network is not structurally hashed.\n" );
            Abc_Print( -1, "Please run \"strash\" first.\n" );
            return 1;
        }
        // convert strashed Abc_Ntk_t to Gia_Man_t
        pAig  = Abc_NtkToDar( pNtk, 0, 1 );
        pGia  = Gia_ManFromAig( pAig );
        Aig_ManStop( pAig );
        fOwnGia = 1;
    }

    // perform the algorithm
    pTemp = Mosh_ManPerform( pGia, pPar, pRes );

    // handle result
    if ( pRes->fChanged )
    {
        Aig_Man_t * pAig;
        Abc_Ntk_t * pNtkNew, * pNtkOld;

        Abc_FrameUpdateGia( pAbc, pTemp );

        // convert Gia back to Ntk so commands that read pNtk see the changes
        pAig = Gia_ManToAig( pTemp, 0 );
        if ( pAig != NULL )
        {
            pNtkNew = Abc_NtkFromAigPhase( pAig );
            Aig_ManStop( pAig );
            if ( pNtkNew != NULL )
            {
                // transfer PI/PO names from the old network
                pNtkOld = Abc_FrameReadNtk( pAbc );
                if ( pNtkOld != NULL )
                {
                    int i;
                    Abc_Obj_t * pObjOld, * pObjNew;
                    char * pName;
                    Abc_NtkForEachCi( pNtkOld, pObjOld, i )
                    {
                        if ( i < Abc_NtkCiNum( pNtkNew ) )
                        {
                            pObjNew = Abc_NtkCi( pNtkNew, i );
                            pName = Abc_ObjName( pObjOld );
                            if ( pName && pName[0] )
                            {
                                Nm_ManDeleteIdName( pNtkNew->pManName, pObjNew->Id );
                                Abc_ObjAssignName( pObjNew, pName, NULL );
                            }
                        }
                    }
                    Abc_NtkForEachCo( pNtkOld, pObjOld, i )
                    {
                        if ( i < Abc_NtkCoNum( pNtkNew ) )
                        {
                            pObjNew = Abc_NtkCo( pNtkNew, i );
                            pName = Abc_ObjName( pObjOld );
                            if ( pName && pName[0] )
                            {
                                Nm_ManDeleteIdName( pNtkNew->pManName, pObjNew->Id );
                                Abc_ObjAssignName( pObjNew, pName, NULL );
                            }
                        }
                    }
                }
                Abc_FrameReplaceCurrentNetwork( pAbc, pNtkNew );
            }
        }

        // if we owned the original, transfer ownership to the frame
        if ( fOwnGia )
            Gia_ManStop( pGia );
    }
    else
    {
        // algorithm did not modify the network
        if ( pTemp != pGia )
            Gia_ManStop( pTemp );
        if ( fOwnGia )
            Gia_ManStop( pGia );
    }

    return 0;

usage:
    Abc_Print( -2, "usage: moshare -algo <name> [-maxwin <n>] [-maxcut <n>] [-maxcand <n>] [-maxbdd <n>] [-maxgrow <n>] [-seed <n>] [-dryrun] [-stats] [-v] [-h]\n" );
    Abc_Print( -2, "\t        multi-output sharing optimization framework\n" );
    Abc_Print( -2, "\t-algo <name> : algorithm selection (\"-algo list\" to see choices)\n" );
    Abc_Print( -2, "\t-maxwin <n>  : max window size [default = %d]\n",         pPar->nMaxWindowSize );
    Abc_Print( -2, "\t-maxcut <n>  : max cut size [default = %d]\n",            pPar->nMaxCutSize );
    Abc_Print( -2, "\t-maxcand <n> : max candidates [default = %d]\n",          pPar->nMaxCandidates );
    Abc_Print( -2, "\t-maxbdd <n>  : max BDD nodes [default = %d]\n",           pPar->nMaxBddNodes );
    Abc_Print( -2, "\t-maxgrow <n> : max level growth [default = %d]\n",        pPar->nMaxLevelGrowth );
    Abc_Print( -2, "\t-seed <n>    : random seed [default = %d]\n",              pPar->RandomSeed );
    Abc_Print( -2, "\t-dryrun      : evaluate but do not apply changes\n" );
    Abc_Print( -2, "\t-stats       : print detailed statistics\n" );
    Abc_Print( -2, "\t-v           : enable verbose output\n" );
    Abc_Print( -2, "\t-h           : print the command usage\n" );
    return 1;
}

ABC_NAMESPACE_IMPL_END
