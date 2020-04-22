//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Imports .fac files exported from the SFM into VCD files
//
//=============================================================================

#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "appframework/tier3app.h"
#include "tier1/utlbuffer.h"
#include "dmserializers/idmserializers.h"
#include "tier1/utlstring.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmelement.h"
#include "tier3/tier3.h"
#include "movieobjects/importintovcd.h"
#include "p4lib/ip4.h"
#include "tier2/p4helpers.h"
#include "interpolatortypes.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"

#ifdef _DEBUG
#include <windows.h>
#endif

class CDmElement;


//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t VcdImportOutputFunc( SpewType_t spewType, const char *pMsg )
{
#ifdef _DEBUG
	OutputDebugString( pMsg );
#endif

	printf( pMsg );
	fflush( stdout );

	if (spewType == SPEW_ERROR)
		return SPEW_ABORT;
	return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVcdImportApp : public CTier3DmSteamApp
{
	typedef CTier3DmSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy() {}
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CVcdImportApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CVcdImportApp::Create()
{
	SpewOutputFunc( VcdImportOutputFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystems( appSystems );

	IMaterialSystem *pMaterialSystem = reinterpret_cast< IMaterialSystem * >( FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION ) );
	if ( !pMaterialSystem )
	{
		Error( "// ERROR: Unable to connect to material system interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );
	return true;
}

bool CVcdImportApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pDataModel || !g_pMDLCache )
	{
		Warning( "VCDImport is missing a required interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	return true;
}



//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CVcdImportApp::Main()
{
	g_pDataModel->OnlyCreateUntypedElements( true );
	g_pDataModel->SetDefaultElementFactory( NULL );
	g_pDataModel->SetUndoEnabled( false );

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "Automatically Generated VCD files" );

	ImportVCDInfo_t info;

	const char *pImportFileName = CommandLine()->ParmValue("-f" );
	const char *pInFileName = CommandLine()->ParmValue("-i" );
	const char *pOutFileName = CommandLine()->ParmValue("-o" );
	info.m_flSimplificationThreshhold = CommandLine()->ParmValue( "-s", 0.05f );
	info.m_nInterpolationType = Interpolator_InterpolatorForName( CommandLine()->ParmValue( "-c", "linear_interp" ) );
	info.m_bIgnorePhonemes = CommandLine()->FindParm( "-p" ) != 0;
	if ( !pImportFileName || !pInFileName )
	{
		Msg( "Usage: vcdimport -f <imported .fac file> -i <in .vcd file> [-o <out .vcd file>]\n" );
		Msg( "\t\t[-nop4] [-p] [-s <simplification_amount>] [-c <curve type>]\n" );
		Msg( "\t-f : .FAC file containing SFM-authored facial animation exported from the animation set editor in the SFM.\n" );
		Msg( "\t-i : Source .VCD file to import the facial animation into.\n" );
		Msg( "\t-o : If no output file is specified, vcdimport will overwrite the input .vcd file.\n" );
		Msg( "\t-p : Causes the generated .VCD file to not use phonemes in associated wav files to autogenerate animation.\n" );
		Msg( "\t-nop4 : Disables auto perforce checkout.\n" );
		Msg( "\t-s : Requires a second argument, the amount of error we're willing to accept in simplification. Ranges from 0.0 - 1.0 [default = 0.05]\n" );
		Msg( "\t-c : Specify interpolation type. Options are: [linear_interp is used if -c isn't specified]\n" );
		for ( int i = 0; i < NUM_INTERPOLATE_TYPES; ++i )
		{
			Msg( "\t\t%22s :\t%s\n", Interpolator_NameForInterpolator( i, false ), Interpolator_NameForInterpolator( i, true ) );
		}
		return -1;
	}

	if ( !pOutFileName )
	{
		pOutFileName = pInFileName;
	}

	bool bOk = ImportLogsIntoVCD( pImportFileName, pInFileName, pOutFileName, info );
	if ( bOk )	
	{
		Msg( "Wrote file %s.\n", pOutFileName );
	}
	return bOk;
}



