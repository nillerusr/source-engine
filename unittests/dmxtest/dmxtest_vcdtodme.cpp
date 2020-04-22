//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "unitlib/unitlib.h"
#include "datamodel/dmelement.h"
#include "movieobjects/movieobjects.h"
#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "movieobjects/dmelog.h"
#include "choreoscene.h"
#include "choreoevent.h"
#include "iscenetokenprocessor.h"
#include "tier1/tokenreader.h"
#include "characterset.h"
#include "movieobjects/dmx_to_vcd.h"
#include "tier3/scenetokenprocessor.h"
#include "tier2/tier2.h"

char const *vcdtestfile = "dmxtest.vcd";

void RunSceneToDmxTests( CChoreoScene *scene )
{
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( scene->GetFilename() );
	CDmeFilmClip *dmx = CreateElement< CDmeFilmClip >( scene->GetFilename(), fileid );
	Assert( dmx );

	bool success = ConvertSceneToDmx( scene, dmx );
	Assert( success );

	CChoreoScene *scene2 = new CChoreoScene( NULL );
	scene2->SetFileName( scene->GetFilename() );
	
	success = ConvertDmxToScene( dmx, scene2 );
	Assert( success );

	char sz[ 512 ];
	Q_StripExtension( scene->GetFilename(), sz, sizeof( sz ) );
	Q_strncat( sz, "_2.vcd", sizeof( sz ), COPY_ALL_CHARACTERS );
	scene2->SaveToFile( sz );
	
	delete scene2;

	g_pDataModel->RemoveFileId( fileid );
}

DEFINE_TESTCASE_NOSUITE( DmxTestVcdToDme )
{
	Msg( "Running .vcd (faceposer) to dmx tests\n" );
	
#ifdef _DEBUG
	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
#endif

	CDisableUndoScopeGuard guard;

	g_pDmElementFramework->BeginEdit();

	const char *pFileName = vcdtestfile;
	char pFullPathName[ MAX_PATH ];
	char pDir[ MAX_PATH ];
	if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof( pDir ) ) )
	{
		V_ComposeFileName( pDir, vcdtestfile, pFullPathName, sizeof( pFullPathName ) );
		V_RemoveDotSlashes( pFullPathName );
		pFileName = pFullPathName;
	}

	CUtlBuffer buf;
	if ( g_pFullFileSystem->ReadFile( pFileName, NULL, buf ) )
	{
		SetTokenProcessorBuffer( (char *)buf.Base() );
		CChoreoScene *scene = ChoreoLoadScene( pFileName, NULL, GetTokenProcessor(), NULL );
		if ( scene )
		{
			RunSceneToDmxTests( scene );
			delete scene;
		}
	}
	else
	{
		Msg( "Unable to load test file '%s'\n", pFileName );
	}

	g_pDataModel->ClearUndo();

#ifdef _DEBUG
	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEquals( nEndingCount, nStartingCount );
	if ( nEndingCount != nStartingCount )
	{
		for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement() ;
			hElement != DMELEMENT_HANDLE_INVALID;
			hElement = g_pDataModel->NextAllocatedElement( hElement ) )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			Assert( pElement );
			if ( !pElement )
				return;

			Msg( "[%s : %s] in memory\n", pElement->GetName(), pElement->GetTypeString() );
		}
	}
#endif
}