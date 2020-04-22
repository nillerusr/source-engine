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
//
//=============================================================================

#include "dmserializers.h"
#include "dmserializers/idmserializers.h"
#include "appframework/iappsystem.h"
#include "filesystem.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// format updater macros
//-----------------------------------------------------------------------------

#define DECLARE_FORMAT_UPDATER( _name, _description, _extension, _version, _encoding ) \
	class CDmFormatUpdater_ ## _name : public IDmFormatUpdater \
	{ \
	public: \
		CDmFormatUpdater_ ## _name() {} \
		virtual const char *GetName() const { return #_name; } \
		virtual const char *GetDescription() const { return _description; } \
		virtual const char *GetExtension() const { return _extension; } \
		virtual const char *GetDefaultEncoding() const { return _encoding; } \
		virtual int GetCurrentVersion() const { return _version; } \
		virtual bool Update( CDmElement **pRoot, int nSourceVersion ) { return true; } \
	}; \
	static CDmFormatUpdater_ ## _name s_FormatUpdater ## _name; \
	void InstallFormatUpdater_ ## _name( IDataModel *pFactory ) \
	{ \
		pFactory->AddFormatUpdater( &s_FormatUpdater ## _name ); \
	}

#define INSTALL_FORMAT_UPDATER( _name ) InstallFormatUpdater_ ## _name( g_pDataModel )


//-----------------------------------------------------------------------------
// format updaters
//-----------------------------------------------------------------------------

DECLARE_FORMAT_UPDATER( dmx,				"Generic DMX",					"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( movieobjects,		"Generic MovieObjects",			"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( sfm,				"Generic SFM",					"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( sfm_session,		"SFM Session",					"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( sfm_trackgroup,		"SFM TrackGroup",				"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( pcf,				"Particle Configuration File",	"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( preset,				"Preset File",					"dmx", 1, "keyvalues2" )
DECLARE_FORMAT_UPDATER( facial_animation,	"Facial Animation File",		"dmx", 1, "binary" )
DECLARE_FORMAT_UPDATER( model,				"DMX Model",					"dmx", 1, "binary" )
//DECLARE_FORMAT_UPDATER( animation,			"DMX Animation",				"dmx", 1, "binary" )
//DECLARE_FORMAT_UPDATER( dcc_makefile,			"DMX Makefile",					"dmx", 1, "keyvalues2" )


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CDmSerializers : public CBaseAppSystem< IDmSerializers >
{
	typedef CBaseAppSystem< IDmSerializers > BaseClass;

public:
	// Inherited from IAppSystem 
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
};


//-----------------------------------------------------------------------------
// Singleton interface 
//-----------------------------------------------------------------------------
static CDmSerializers g_DmSerializers;
IDmSerializers *g_pDmSerializers = &g_DmSerializers;


//-----------------------------------------------------------------------------
// Here's where the app systems get to learn about each other 
//-----------------------------------------------------------------------------
bool CDmSerializers::Connect( CreateInterfaceFn factory ) 
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !factory( FILESYSTEM_INTERFACE_VERSION, NULL ) )
	{
		Warning( "DmSerializers needs the file system to function" );
		return false;
	}

	// Here's the main point where all DM element classes get installed
	// Necessary to do it here so all type symbols for all DME classes are set 
	// up prior to install
	InstallDmElementFactories( );

	return true;
}


//-----------------------------------------------------------------------------
// Here's where systems can access other interfaces implemented by this object
//-----------------------------------------------------------------------------
void *CDmSerializers::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, DMSERIALIZERS_INTERFACE_VERSION ) )
		return (IDmSerializers*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CDmSerializers::Init() 
{ 
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	// Install non-dmx importers
	InstallActBusyImporter( g_pDataModel );
	InstallVMTImporter( g_pDataModel );
	InstallVMFImporter( g_pDataModel );

	// Install legacy dmx importers
	InstallSFMV1Importer( g_pDataModel );
	InstallSFMV2Importer( g_pDataModel );
	InstallSFMV3Importer( g_pDataModel );
	InstallSFMV4Importer( g_pDataModel );
	InstallSFMV5Importer( g_pDataModel );
	InstallSFMV6Importer( g_pDataModel );
	InstallSFMV7Importer( g_pDataModel );
	InstallSFMV8Importer( g_pDataModel );
	InstallSFMV9Importer( g_pDataModel );

	// install dmx format updaters
	INSTALL_FORMAT_UPDATER( dmx );
	INSTALL_FORMAT_UPDATER( movieobjects );
	INSTALL_FORMAT_UPDATER( sfm );
	INSTALL_FORMAT_UPDATER( sfm_session );
	INSTALL_FORMAT_UPDATER( sfm_trackgroup );
	INSTALL_FORMAT_UPDATER( pcf );
	INSTALL_FORMAT_UPDATER( preset );
	INSTALL_FORMAT_UPDATER( facial_animation );
	INSTALL_FORMAT_UPDATER( model );
//	INSTALL_FORMAT_UPDATER( animation );
//	INSTALL_FORMAT_UPDATER( dcc_makefile );

	return INIT_OK; 
}

