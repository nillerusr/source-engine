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
// Converts from any one DMX file format to another
// Can also output SMD or a QCI header from DMX input
//
//=============================================================================

#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "appframework/appframework.h"
#include "tier1/utlbuffer.h"
#include "dmserializers/idmserializers.h"
#include "tier1/utlstring.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmelement.h"
#include "tier2/tier2.h"


class CDmElement;


//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t DMXConvertOutputFunc( SpewType_t spewType, char const *pMsg )
{
	printf( pMsg );
	fflush( stdout );

	if (spewType == SPEW_ERROR)
		return SPEW_ABORT;
	return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CDmxConvertApp : public CDefaultAppSystemGroup<CSteamAppSystemGroup>
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown();
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CDmxConvertApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CDmxConvertApp::Create()
{
	SpewOutputFunc( DMXConvertOutputFunc );
	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );
	return true;
}

bool CDmxConvertApp::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pDataModel )
	{
		Warning( "DMXConvert is missing a required interface!\n" );
		return false;
	}
	return true;
}


void CDmxConvertApp::PostShutdown()
{
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CDmxConvertApp::Main()
{
	g_pDataModel->OnlyCreateUntypedElements( true );
	g_pDataModel->SetDefaultElementFactory( NULL );

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	const char *pInFileName = CommandLine()->ParmValue("-i" );
	const char *pOutFileName = CommandLine()->ParmValue("-o" );
	const char *pInFormat = CommandLine()->ParmValue("-if" );
	const char *pOutFormat = CommandLine()->ParmValue("-of" );
	const char *pOutEncoding = CommandLine()->ParmValue("-oe" );

	if ( !pInFileName )
	{
		Msg( "Usage: dmxconvert -i <in file> [-if <in format_hint>] [-o <out file>] [-oe <out encoding>] [-of <out format>]\n" );
		Msg( "If no output file is specified, dmx to dmx conversion will overwrite the input\n" );
		Msg( "Supported DMX file encodings:\n" );
		for ( int i = 0; i < g_pDataModel->GetEncodingCount(); ++i )
		{
			Msg( "   %s\n", g_pDataModel->GetEncodingName( i ) );
		}

		Msg( "Supported DMX file formats:\n" );
		for ( int i = 0; i < g_pDataModel->GetFormatCount(); ++i )
		{
			Msg( "   %s\n", g_pDataModel->GetFormatName( i ) );
		}

		return -1;
	}

	// When reading, keep the CRLF; this will make ReadFile read it in binary format
	// and also append a couple 0s to the end of the buffer.
	DmxHeader_t header;
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pInFileName, NULL, pInFormat, &pRoot, CR_DELETE_NEW, &header ) == DMFILEID_INVALID )
	{
		Error( "Encountered an error reading file \"%s\"!\n", pInFileName );
		return -1;
	}

	if ( !pOutFormat )
	{
		pOutFormat = header.formatName;
		if ( !g_pDataModel->FindFormatUpdater( pOutFormat ) )
		{
			pOutFormat = "dmx"; // default to generic dmx format for legacy files
		}
	}

	if ( !pOutEncoding )
	{
		pOutEncoding = g_pDataModel->GetDefaultEncoding( pOutFormat );
		if ( !pOutEncoding )
		{
			// no default encoding was found, try to convert the file to the generic dmx format
			pOutFormat = GENERIC_DMX_FORMAT;
			pOutEncoding = g_pDataModel->GetDefaultEncoding( pOutFormat );
		}
	}

	if ( !pOutFileName )
	{
		pOutFileName = pInFileName;
	}

	// TODO - in theory, at some point, we may have converters from pInFormat to pOutFormat
	//		  until then, treat it as a noop, and hope for the best

	if ( !g_pDataModel->SaveToFile( pOutFileName, NULL, pOutEncoding, pOutFormat, pRoot ) )
	{
		Error( "Encountered an error writing file \"%s\"!\n", pOutFileName );
		return -1;
	}

	g_pDataModel->RemoveFileId( pRoot->GetFileId() );

	return 0;
}



