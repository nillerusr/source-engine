//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: loads and unloads main matsystem dll and interface
//
//===========================================================================//

#include "render_pch.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ivballoctracker.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "sys_dll.h"
#include "host.h"
#include "cmodel_engine.h"
#include "gl_model_private.h"
#include "view.h"
#include "gl_matsysiface.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "lightcache.h"
#include "vstdlib/random.h"
#include "tier0/icommandline.h"
#include "draw.h"
#include "decal_private.h"
#include "l_studio.h"
#include "KeyValues.h"
#include "materialsystem/imaterial.h"
#include "gl_shader.h"
#include "ivideomode.h"
#include "cdll_engine_int.h"
#include "utldict.h"
#include "filesystem.h"
#include "host_saverestore.h"
#include "server.h"
#include "game/client/iclientrendertargets.h"
#include "tier2/tier2.h"
#include "LoadScreenUpdate.h"
#include "client.h"
#include "sourcevr/isourcevirtualreality.h"
#if defined( _X360 )
#include "xbox/xbox_launch.h"
#endif

#if defined( USE_SDL )
#include "SDL.h"
#endif

//X360TEMP
#include "materialsystem/itexture.h"

extern IFileSystem *g_pFileSystem;
#ifndef SWDS
#include "iregistry.h"
#endif

#include "igame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Start the frame count at one so stuff gets updated the first frame
int	r_framecount = 1;               // used for dlight + lightstyle push checking
int	d_lightstylevalue[256];
int	d_lightstylenumframes[256];

const MaterialSystem_Config_t *g_pMaterialSystemConfig;

static CSysModule	*g_MaterialsDLL = NULL;
bool g_LostVideoMemory = false;

IMaterial*	g_materialEmpty;	// purple checkerboard for missing textures

void ReleaseMaterialSystemObjects();
void RestoreMaterialSystemObjects( int nChangeFlags );

extern ConVar mat_colorcorrection;
extern ConVar sv_allow_color_correction;

ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );

// Static pointers to renderable textures
static CTextureReference g_PowerOfTwoFBTexture;
static CTextureReference g_WaterReflectionTexture;
static CTextureReference g_WaterRefractionTexture;
static CTextureReference g_CameraTexture;
static CTextureReference g_BuildCubemaps16BitTexture;
static CTextureReference g_QuarterSizedFBTexture0;
static CTextureReference g_QuarterSizedFBTexture1;
static CTextureReference g_TeenyFBTexture0;
static CTextureReference g_TeenyFBTexture1;
static CTextureReference g_TeenyFBTexture2;
static CTextureReference g_FullFrameFBTexture0;
static CTextureReference g_FullFrameFBTexture1;
static CTextureReference g_FullFrameFBTexture2;
static CTextureReference g_FullFrameDepth;
static CTextureReference g_ResolvedFullFrameDepth;

void WorldStaticMeshCreate( void );
void WorldStaticMeshDestroy( void );

ConVar	r_norefresh( "r_norefresh","0");
ConVar	r_decals( "r_decals", "2048" );
ConVar	mp_decals( "mp_decals","200", FCVAR_ARCHIVE);
ConVar	r_lightmap( "r_lightmap", "-1", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar	r_lightstyle( "r_lightstyle","-1", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar	r_dynamic( "r_dynamic","1");

ConVar  mat_norendering( "mat_norendering", "0", FCVAR_CHEAT );
ConVar	mat_wireframe(  "mat_wireframe", "0", FCVAR_CHEAT );
ConVar	mat_luxels(  "mat_luxels", "0", FCVAR_CHEAT );
ConVar	mat_normals(  "mat_normals", "0", FCVAR_CHEAT );
ConVar	mat_bumpbasis(  "mat_bumpbasis", "0", FCVAR_CHEAT );
ConVar	mat_envmapsize(  "mat_envmapsize", "128" );
ConVar  mat_envmaptgasize( "mat_envmaptgasize", "32.0" );
ConVar  mat_levelflush( "mat_levelflush", "1" );
ConVar  mat_fastspecular( "mat_fastspecular", "1", 0, "Enable/Disable specularity for visual testing.  Will not reload materials and will not affect perf." );
ConVar  mat_fullbright( "mat_fullbright","0", FCVAR_CHEAT );

static ConVar mat_monitorgamma( "mat_monitorgamma", "2.2", FCVAR_ARCHIVE, "monitor gamma (typically 2.2 for CRT and 1.7 for LCD)", true, 1.6f, true, 2.6f  );
static ConVar mat_monitorgamma_tv_range_min( "mat_monitorgamma_tv_range_min", "16" );
static ConVar mat_monitorgamma_tv_range_max( "mat_monitorgamma_tv_range_max", "255" );
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer into a 2.5 frame buffer for display on a TV
static ConVar mat_monitorgamma_tv_exp( "mat_monitorgamma_tv_exp", "2.5", 0, "", true, 1.0f, true, 4.0f );
#ifdef _X360
static ConVar mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled", "1", FCVAR_ARCHIVE, "" );
#else
static ConVar mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled", "0", FCVAR_ARCHIVE, "" );
#endif
				  
ConVar r_drawbrushmodels( "r_drawbrushmodels", "1", FCVAR_CHEAT, "Render brush models. 0=Off, 1=Normal, 2=Wireframe" );

ConVar r_shadowrendertotexture( "r_shadowrendertotexture", "0" );
ConVar r_flashlightdepthtexture( "r_flashlightdepthtexture", "1" );
#ifndef _X360
ConVar r_waterforceexpensive( "r_waterforceexpensive", "0", FCVAR_ARCHIVE );
#endif
ConVar r_waterforcereflectentities( "r_waterforcereflectentities", "0" );

// Note: this is only here so we can ship an update without changing materialsystem.dll.
// Once we ship materialsystem.dll again, we can get rid of this and make the only one exist in materialsystem.dll.
ConVar mat_depthbias_normal( "mat_depthbias_normal", "0.0f", FCVAR_CHEAT );

// This is here so that the client and the materialsystem can both see this.
ConVar mat_show_ab_hdr( "mat_show_ab_hdr", "0" );

static void NukeModeSwitchSaveGames( void )
{
	if( g_pFileSystem->FileExists( "save\\modeswitchsave.sav" ) )
	{
		g_pFileSystem->RemoveFile( "save\\modeswitchsave.sav" );
	}
	if( g_pFileSystem->FileExists( "save\\modeswitchsave.tga" ) )
	{
		g_pFileSystem->RemoveFile( "save\\modeswitchsave.tga" );
	}
}

void mat_hdr_level_Callback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( IsX360() )
	{
		// can't support, expected to be static
		return;
	}

#ifdef CSS_PERF_TEST
	ConVarRef hdr( var );
	if ( hdr.GetInt() > 0 )
		hdr.SetValue( 0 );
	return;
#endif
	// Can do any reloading that is necessary here upon change.
	// FIXME: should check if there is actually going to be a change here (ie. are we able to run in HDR
	// given the current map and hardware.
#ifndef SWDS
	if ( g_pMaterialSystemHardwareConfig->GetHardwareHDRType() != HDR_TYPE_NONE &&
         saverestore->IsValidSave() &&
		 modelloader->LastLoadedMapHasHDRLighting() &&
		 sv.GetMaxClients() == 1 &&
		 !sv.IsLevelMainMenuBackground()
		 )
	{
		NukeModeSwitchSaveGames();
		Cbuf_AddText( "save modeswitchsave;wait;load modeswitchsave\n" );
	}
#endif
}

#ifdef CSS_PERF_TEST
ConVar mat_hdr_level( "mat_hdr_level", "0", 0, 
					 "Set to 0 for no HDR, 1 for LDR+bloom on HDR maps, and 2 for full HDR on HDR maps.",
					 mat_hdr_level_Callback );
#else
ConVar mat_hdr_level( "mat_hdr_level", "2", FCVAR_ARCHIVE, 
					  "Set to 0 for no HDR, 1 for LDR+bloom on HDR maps, and 2 for full HDR on HDR maps.",
					  mat_hdr_level_Callback );
#endif

MaterialSystem_SortInfo_t *materialSortInfoArray = 0;
static bool s_bConfigLightingChanged = false;

extern unsigned long GetRam();


//-----------------------------------------------------------------------------
// return true if lightmaps need to be redownloaded
//-----------------------------------------------------------------------------
bool MaterialConfigLightingChanged()
{
	return s_bConfigLightingChanged;
}

void ClearMaterialConfigLightingChanged()
{
	s_bConfigLightingChanged = false;
}


//-----------------------------------------------------------------------------
// List of all convars to store into the registry
//-----------------------------------------------------------------------------
static const char *s_pRegistryConVars[] = 
{
	"mat_forceaniso",
	"mat_picmip",
//	"mat_dxlevel",
	"mat_trilinear",
	"mat_vsync",
	"mat_forcehardwaresync",
	"mat_parallaxmap",
	"mat_reducefillrate",
	"r_shadowrendertotexture",
	"r_rootlod",
#ifndef _X360
	"r_waterforceexpensive",
#endif
	"r_waterforcereflectentities",
	"mat_antialias",
	"mat_aaquality",
	"mat_specular",
	"mat_bumpmap",
	"mat_hdr_level",
	"mat_colorcorrection",

	// NOTE: Empty string must always be last!
	""
};

#if defined( OSX )
	#define MOD_VIDEO_CONFIG_SETTINGS "videoconfig_mac.cfg"
	#define USE_VIDEOCONFIG_FILE 1
#elif defined( POSIX )
	#define MOD_VIDEO_CONFIG_SETTINGS "videoconfig_linux.cfg"
	#define USE_VIDEOCONFIG_FILE 1
#elif defined( DX_TO_GL_ABSTRACTION )
	#define MOD_VIDEO_CONFIG_SETTINGS "videoconfig_gl.cfg"
	#define USE_VIDEOCONFIG_FILE 1
#else
	#define MOD_VIDEO_CONFIG_SETTINGS "videoconfig.cfg"
	#define USE_VIDEOCONFIG_FILE 0
#endif

#if USE_VIDEOCONFIG_FILE
static CThreadMutex g_VideoConfigMutex;
#endif

static int ReadVideoConfigInt( const char *pName, int nDefault )
{
#if USE_VIDEOCONFIG_FILE
	AUTO_LOCK( g_VideoConfigMutex );
	
	// Try to make a keyvalues from the cfg file
	KeyValues *pVideoConfig = new KeyValues( "videoconfig" );
	bool bFileExists = pVideoConfig->LoadFromFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD" );
	
	// We probably didn't have one on disk yet, just bail.  It'll get created soon.
	if ( !bFileExists )
		return nDefault;
	
	int nInt = pVideoConfig->GetInt( pName, nDefault );
	pVideoConfig->deleteThis();
	return nInt;
#else
	return registry->ReadInt( pName, nDefault );
#endif
}

static void ReadVideoConfigInt( const char *pName, int *pEntry )
{
#if USE_VIDEOCONFIG_FILE
	AUTO_LOCK( g_VideoConfigMutex );
	
	// Try to make a keyvalues from the cfg file
	KeyValues *pVideoConfig = new KeyValues( "videoconfig" );
	bool bFileExists = pVideoConfig->LoadFromFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD" );
	
	// We probably didn't have one on disk yet, just bail.  It'll get created soon.
	if ( !bFileExists )
		return;

	if ( pVideoConfig->GetInt( pName, -1 ) != -1 )
	{
		*pEntry = pVideoConfig->GetInt( pName, 0 );
	}

	pVideoConfig->deleteThis();
#else
	if ( registry->ReadInt( pName, -1 ) != -1 )
	{
		*pEntry = registry->ReadInt( pName, 0 );
	}
#endif
}

static const char *ReadVideoConfigString( const char *pName, const char *pDefault )
{
#if USE_VIDEOCONFIG_FILE
	AUTO_LOCK( g_VideoConfigMutex );
	static char szRetString[ 255 ];
	
	// Try to make a keyvalues from the cfg file
	KeyValues *pVideoConfig = new KeyValues( "videoconfig" );
	bool bFileExists = pVideoConfig->LoadFromFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD" );
	
	// We probably didn't have one on disk yet, just bail.  It'll get created soon.
	if ( !bFileExists )
		return pDefault;
	
	const char *pString = pVideoConfig->GetString( pName, pDefault );
	Q_strncpy( szRetString, pString, sizeof(szRetString) );
	pVideoConfig->deleteThis();
	return szRetString;
#else
	return registry->ReadString( pName, pDefault );
#endif
}



static void WriteVideoConfigInt( const char *pName, int nEntry )
{
#if USE_VIDEOCONFIG_FILE
	AUTO_LOCK( g_VideoConfigMutex );
	
	// Try to make a keyvalues from the cfg file
	KeyValues *pVideoConfig = new KeyValues( "videoconfig" );
	pVideoConfig->LoadFromFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD" );
	
	pVideoConfig->SetInt( pName, nEntry );
	
	pVideoConfig->SaveToFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD", false, false, true );
	pVideoConfig->deleteThis();
#else
	registry->WriteInt( pName, nEntry );
#endif
}


static void WriteVideoConfigString( const char *pName, const char *pString )
{
#if USE_VIDEOCONFIG_FILE
	AUTO_LOCK( g_VideoConfigMutex );

	// Try to make a keyvalues from the cfg file
	KeyValues *pVideoConfig = new KeyValues( "videoconfig" );
	pVideoConfig->LoadFromFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD" );
	
	pVideoConfig->SetString( pName, pString );
	
	pVideoConfig->SaveToFile( g_pFullFileSystem, MOD_VIDEO_CONFIG_SETTINGS, "MOD", false, false, true );
	pVideoConfig->deleteThis();
#else
	registry->WriteString( pName, pString );
#endif
}

//-----------------------------------------------------------------------------
// Scan for video config convars which are overridden on the cmd line, used
// for development and automated timedemo regression testing.
// (Unfortunately, convars aren't set early enough during init from the cmd line 
// for the usual machinery to work here.)
//-----------------------------------------------------------------------------
static bool s_bVideoConfigOverriddenFromCmdLine;

template<typename T>
static T OverrideVideoConfigFromCommandLine( const char *pCVarName, T curVal )
{
	char szOption[256];
	V_snprintf( szOption, sizeof( szOption ), "+%s", pCVarName );
	if ( CommandLine()->CheckParm( szOption ) )
	{
		T newVal = CommandLine()->ParmValue( szOption, curVal );
		Warning( "Video configuration ignoring %s due to command line override\n", pCVarName );
		s_bVideoConfigOverriddenFromCmdLine = true;
		return newVal;
	}
	return curVal;
}

//-----------------------------------------------------------------------------
// Reads convars from the registry
//-----------------------------------------------------------------------------
static void ReadMaterialSystemConfigFromRegistry( MaterialSystem_Config_t &config )
{
#ifndef SWDS
	if ( IsX360() )
		return;

	ReadVideoConfigInt( "ScreenWidth", &config.m_VideoMode.m_Width );
	ReadVideoConfigInt( "ScreenHeight", &config.m_VideoMode.m_Height );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, ReadVideoConfigInt( "ScreenWindowed", 0 ) != 0 );
#if defined( USE_SDL ) && !defined( SWDS )
	// Read the ScreenDisplayIndex and set sdl_displayindex if it's there.
	ConVarRef conVar( "sdl_displayindex" );
	if ( conVar.IsValid() )
	{
		int displayIndex = 0;

		ReadVideoConfigInt( "ScreenDisplayIndex", &displayIndex );
		conVar.SetValue( displayIndex );
		displayIndex = conVar.GetInt();

		// Make sure the width / height isn't too large for this display.
		SDL_Rect rect;
		if ( !SDL_GetDisplayBounds( displayIndex, &rect ) )
		{
			if ( ( config.m_VideoMode.m_Width > rect.w ) || ( config.m_VideoMode.m_Height > rect.h ) )
			{
				config.m_VideoMode.m_Width = rect.w;
				config.m_VideoMode.m_Height = rect.h;
			}
		}
	}
#endif // USE_SDL && !SWDS

	// Special case...
	const char *szMonitorGamma = ReadVideoConfigString( "ScreenMonitorGamma", "2.2" );
	if ( szMonitorGamma && strlen(szMonitorGamma) > 0 )
	{
		float flMonitorGamma = atof( szMonitorGamma );
		// temp, to make sure people with gamma values saved in the old format don't max out
		if (flMonitorGamma > 3.0f)
		{
			flMonitorGamma = 2.2f;
		}
		
		flMonitorGamma = OverrideVideoConfigFromCommandLine( "mat_monitorgamma", flMonitorGamma );
		
		mat_monitorgamma.SetValue( flMonitorGamma );
		config.m_fMonitorGamma = mat_monitorgamma.GetFloat();
	}

	for ( int i = 0; s_pRegistryConVars[i][0]; ++i )
	{
		int nValue = ReadVideoConfigInt( s_pRegistryConVars[i], 0x80000000 );
		if ( nValue == 0x80000000 )
			continue;
		
		nValue = OverrideVideoConfigFromCommandLine( s_pRegistryConVars[i], nValue );

		ConVarRef conVar( s_pRegistryConVars[i] );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( nValue );
		}
	}

	int nValue = ReadVideoConfigInt( "DXLevel_V1", -1 );
	if ( nValue != -1 )
	{
		nValue = OverrideVideoConfigFromCommandLine( "mat_dxlevel", nValue );

		ConVarRef conVar( "mat_dxlevel" );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( nValue );
		}
	}

	nValue = ReadVideoConfigInt( "MotionBlur", -1 );
	if ( nValue != -1 )
	{
		nValue = OverrideVideoConfigFromCommandLine( "mat_motion_blur_enabled", nValue );

		ConVarRef conVar( "mat_motion_blur_enabled" );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( nValue );
			config.m_bMotionBlur = ReadVideoConfigInt( "MotionBlur", 0 ) != 0;
		}
	}

	nValue = ReadVideoConfigInt( "ShadowDepthTexture", -1 );
	if ( nValue != -1 )
	{
		nValue = OverrideVideoConfigFromCommandLine( "r_flashlightdepthtexture", nValue );

		ConVarRef conVar( "r_flashlightdepthtexture" );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( nValue );
			config.m_bShadowDepthTexture = ReadVideoConfigInt( "ShadowDepthTexture", 0 ) != 0;
		}
	}

	nValue = ReadVideoConfigInt( "VRModeAdapter", -1 );
	if ( nValue != -1 )
	{
		nValue = OverrideVideoConfigFromCommandLine( "mat_vrmode_adapter", nValue );

		ConVarRef conVar( "mat_vrmode_adapter" );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( nValue );
			config.m_nVRModeAdapter = ReadVideoConfigInt( "VRModeAdapter", -1 );
		}
	}

#endif	
}

//-----------------------------------------------------------------------------
// Writes convars into the registry
//-----------------------------------------------------------------------------
static void WriteMaterialSystemConfigToRegistry( const MaterialSystem_Config_t &config )
{
#ifndef SWDS
	if ( IsX360() )
		return;

#if defined( USE_SDL ) && !defined( SWDS )
	// Save sdl_displayindex out to ScreenDisplayIndex.
	ConVarRef conVar( "sdl_displayindex" );
	if ( conVar.IsValid() && !UseVR() )
	{
		WriteVideoConfigInt( "ScreenDisplayIndex", conVar.GetInt() );
	}
#endif // USE_SDL && !SWDS
	WriteVideoConfigInt( "ScreenWidth", config.m_VideoMode.m_Width );
	WriteVideoConfigInt( "ScreenHeight", config.m_VideoMode.m_Height );
	WriteVideoConfigInt( "ScreenWindowed", config.Windowed() );
	WriteVideoConfigInt( "ScreenMSAA", config.m_nAASamples );
	WriteVideoConfigInt( "ScreenMSAAQuality", config.m_nAAQuality );
	WriteVideoConfigInt( "MotionBlur", config.m_bMotionBlur ? 1 : 0 );
	WriteVideoConfigInt( "ShadowDepthTexture", config.m_bShadowDepthTexture ? 1 : 0 );
	WriteVideoConfigInt( "VRModeAdapter", config.m_nVRModeAdapter );

	// Registry only stores ints, so divide/multiply by 100 when reading/writing.
	WriteVideoConfigString( "ScreenMonitorGamma", mat_monitorgamma.GetString() );

	for ( int i = 0; s_pRegistryConVars[i][0]; ++i )
	{
		ConVarRef conVar( s_pRegistryConVars[i] );
		if ( !conVar.IsValid() )
			continue;

		WriteVideoConfigInt( s_pRegistryConVars[i], conVar.GetInt() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Override config with command line params
//-----------------------------------------------------------------------------
static void OverrideMaterialSystemConfigFromCommandLine( MaterialSystem_Config_t &config )
{
	if ( IsX360() )
	{
		// these overrides cannot be supported
		// the console configuration is explicit
		return;
	}

	if ( CommandLine()->FindParm( "-dxlevel" ) )
	{
		config.dxSupportLevel = CommandLine()->ParmValue( "-dxlevel", config.dxSupportLevel );

		// hack, mat_dxlevel is a special case, since it's saved in the registry
		// but has a special different command-line override
		// we need to re-apply the cvar
		ConVarRef conVar( "mat_dxlevel" );
		if ( conVar.IsValid() )
		{
			conVar.SetValue( config.dxSupportLevel );
		}
	}

	// Check for windowed mode command line override
	if ( CommandLine()->FindParm( "-sw" ) || 
		CommandLine()->FindParm( "-startwindowed" ) ||
		CommandLine()->FindParm( "-windowed" ) ||
		CommandLine()->FindParm( "-window" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}
	// Check for fullscreen override
	else if ( CommandLine()->FindParm( "-full" ) ||	CommandLine()->FindParm( "-fullscreen" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, false );
	}

	// Get width and height
	if ( CommandLine()->FindParm( "-width" ) || CommandLine()->FindParm( "-w" ) )
	{
		config.m_VideoMode.m_Width = CommandLine()->ParmValue( "-width", config.m_VideoMode.m_Width );
		config.m_VideoMode.m_Width = CommandLine()->ParmValue( "-w", config.m_VideoMode.m_Width );
		if( !( CommandLine()->FindParm( "-height" ) || CommandLine()->FindParm( "-h" ) ) )
		{
			config.m_VideoMode.m_Height = ( config.m_VideoMode.m_Width * 3 ) / 4;
		}
	}
	if ( CommandLine()->FindParm( "-height" ) || CommandLine()->FindParm( "-h" ) )
	{
		config.m_VideoMode.m_Height = CommandLine()->ParmValue( "-height", config.m_VideoMode.m_Height );
		config.m_VideoMode.m_Height = CommandLine()->ParmValue( "-h", config.m_VideoMode.m_Height );
	}

#if defined( USE_SDL ) && !defined( SWDS )
	// If -displayindex was specified on the command line, then set sdl_displayindex.
	if ( CommandLine()->FindParm( "-displayindex" ) )
	{
		ConVarRef conVar( "sdl_displayindex" );

		if ( conVar.IsValid() )
		{
			int displayIndex = CommandLine()->ParmValue( "-displayindex", conVar.GetInt() );

			conVar.SetValue( displayIndex );
			displayIndex = conVar.GetInt();

			// Make sure the width / height isn't too large for this display.
			SDL_Rect rect;
			if ( !SDL_GetDisplayBounds( displayIndex, &rect ) )
			{
				if ( ( config.m_VideoMode.m_Width > rect.w ) || ( config.m_VideoMode.m_Height > rect.h ) )
				{
					config.m_VideoMode.m_Width = rect.w;
					config.m_VideoMode.m_Height = rect.h;
				}
			}
		}
	}
#endif // USE_SDL && !SWDS

	if ( CommandLine()->FindParm( "-resizing" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, CommandLine()->CheckParm( "-resizing" ) ? true : false );
	}
#ifndef CSS_PERF_TEST
	if ( CommandLine()->FindParm( "-mat_vsync" ) )
	{
		int vsync = CommandLine()->ParmValue( "-mat_vsync", 1 );
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, vsync == 0 );
	}
#endif
	config.m_nAASamples = CommandLine()->ParmValue( "-mat_antialias", config.m_nAASamples );
	config.m_nAAQuality = CommandLine()->ParmValue( "-mat_aaquality", config.m_nAAQuality );

	// Clamp the requested dimensions to the display resolution
	MaterialVideoMode_t videoMode;
	materials->GetDisplayMode( videoMode );
	config.m_VideoMode.m_Width = MIN( videoMode.m_Width, config.m_VideoMode.m_Width );
	config.m_VideoMode.m_Height = MIN( videoMode.m_Height, config.m_VideoMode.m_Height );

	// safe mode
	if ( CommandLine()->FindParm( "-safe" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
		config.m_VideoMode.m_Width = 640;
		config.m_VideoMode.m_Height = 480;
		config.m_VideoMode.m_RefreshRate = 0;
		config.m_nAASamples = 0;
		config.m_nAAQuality = 0;
	}
}


//-----------------------------------------------------------------------------
// Updates the material system config
//-----------------------------------------------------------------------------
void OverrideMaterialSystemConfig( MaterialSystem_Config_t &config )
{
	// enable/disable flashlight support based on mod (user can also set this explicitly)
	// FIXME: this is only here because dxsupport_override.cfg is currently broken
	ConVarRef mat_supportflashlight( "mat_supportflashlight" );
	if ( mat_supportflashlight.GetInt() == -1 )
	{
		const char * gameName = COM_GetModDirectory();
		if ( !V_stricmp( gameName, "portal" ) ||
			 !V_stricmp( gameName, "tf" ) ||
			 !V_stricmp( gameName, "tf_beta" ) )
		{
			mat_supportflashlight.SetValue( false );
		}
		else
		{
			mat_supportflashlight.SetValue( true );
		}
	}
	config.m_bSupportFlashlight = mat_supportflashlight.GetBool();

	// apply the settings in the material system
	bool bLightmapsNeedReloading = materials->OverrideConfig( config, false );
	if ( bLightmapsNeedReloading )
	{
		s_bConfigLightingChanged = true;
	}

	// if VRModeAdapter is set, don't let things come up full screen
	// They will be on the HMD display and that's BAD.
	if( config.m_nVRModeAdapter != -1 )
	{
		WriteVideoConfigInt( "ScreenWindowed", 1 );
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}
}	


void HandleServerAllowColorCorrection()
{
#ifndef SWDS
	if ( !sv_allow_color_correction.GetBool() && mat_colorcorrection.GetInt() )
	{
		Warning( "mat_colorcorrection being forced to 0 due to sv_allow_color_correction=0.\n" );
		mat_colorcorrection.SetValue( 0 );
	}
#endif
}

// auto config version to store in the registry so we can force reconfigs if needed
#define AUTOCONFIG_VERSION 1 

//-----------------------------------------------------------------------------
// Purpose: Initializes configuration
//-----------------------------------------------------------------------------
void InitMaterialSystemConfig( bool bInEditMode )
{
	// get the default config for the current card as a starting point.
	g_pMaterialSystemConfig = &materials->GetCurrentConfigForVideoCard();
	if ( !g_pMaterialSystemConfig )
	{
		Sys_Error( "Could not get the material system config record!" );
	}
	if ( bInEditMode )
		return;

	MaterialSystem_Config_t config = *g_pMaterialSystemConfig;

#if !defined(SWDS)
	// see if they've changed video card, or have no settings present
	MaterialAdapterInfo_t driverInfo;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), driverInfo );

	// see if the user has changed video cards or dx levels
	uint currentVendorID = ReadVideoConfigInt( "VendorID", -1 );
	uint currentDeviceID = ReadVideoConfigInt( "DeviceID", -1 );

	uint autoConfigVersion = ReadVideoConfigInt( "AutoConfigVersion", -1 );

	if ( autoConfigVersion != AUTOCONFIG_VERSION )
	{
		uint max_dxlevel, recommended_dxlevel;
		materials->GetDXLevelDefaults( max_dxlevel, recommended_dxlevel );
		uint currentDXLevel = ReadVideoConfigInt( "DXLevel_V1", -1 );
		if ((max_dxlevel != recommended_dxlevel) && (currentDXLevel != recommended_dxlevel))
		{
			ConVarRef conVar( "mat_dxlevel" );
			if ( conVar.IsValid() )
			{
				conVar.SetValue( (int)recommended_dxlevel );
			}
		}
		// Update the autoconfig version.
		WriteVideoConfigInt( "AutoConfigVersion", AUTOCONFIG_VERSION );
	}
	
	if ( driverInfo.m_VendorID == currentVendorID && 
		 driverInfo.m_DeviceID == currentDeviceID &&
		 !CommandLine()->FindParm( "-autoconfig" ) &&
		 !CommandLine()->FindParm( "-dxlevel" ))
	{
		// the stored configuration looks like it will be valid, load it in
		ReadMaterialSystemConfigFromRegistry( config );
	}
#endif

	OverrideMaterialSystemConfigFromCommandLine( config );
	OverrideMaterialSystemConfig( config );

	// Force the convars to update -- need this due to threading
	g_pCVar->ProcessQueuedMaterialThreadConVarSets();

	// Don't smack registry if dxlevel is overridden, or if the video config was overridden from the command line.
	if ( !CommandLine()->FindParm( "-dxlevel" ) && !s_bVideoConfigOverriddenFromCmdLine )
	{
		WriteMaterialSystemConfigToRegistry( *g_pMaterialSystemConfig );
	}

	UpdateMaterialSystemConfig();

#if !defined(SWDS)
	// write out the current vendor has been seen & registered
	WriteVideoConfigInt( "VendorID", driverInfo.m_VendorID );
	WriteVideoConfigInt( "DeviceID", driverInfo.m_DeviceID );
	WriteVideoConfigInt( "DXLevel_V1", g_pMaterialSystemConfig->dxSupportLevel );
#endif
}


//-----------------------------------------------------------------------------
// Updates the material system config
//-----------------------------------------------------------------------------
void UpdateMaterialSystemConfig( void )
{
	if ( host_state.worldbrush && !host_state.worldbrush->lightdata )
	{
		mat_fullbright.SetValue( 1 );
	}
	
	// apply the settings in the material system
	bool bLightmapsNeedReloading = materials->UpdateConfig( false );
	if ( bLightmapsNeedReloading )
	{
		s_bConfigLightingChanged = true;
	}

}


//-----------------------------------------------------------------------------
// Purpose: Sets all the relevant keyvalue data to be uploaded as part of the benchmark data
// Input  : *dataToUpload - keyvalue set that will be uploaded
//-----------------------------------------------------------------------------
void GetMaterialSystemConfigForBenchmarkUpload(KeyValues *dataToUpload)
{
#if !defined(SWDS)
	// hardware info
	MaterialAdapterInfo_t driverInfo;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), driverInfo );

	dataToUpload->SetInt( "vendorID", driverInfo.m_VendorID );
	dataToUpload->SetInt( "deviceID", driverInfo.m_DeviceID );
	dataToUpload->SetInt( "ram", GetRam() );

	const CPUInformation& pi = *GetCPUInformation();
	double fFrequency = pi.m_Speed / 1000000.0;
	dataToUpload->SetInt( "cpu_speed", (int)fFrequency );
	dataToUpload->SetString( "cpu", pi.m_szProcessorID );

	// material system settings
	dataToUpload->SetInt( "width", g_pMaterialSystemConfig->m_VideoMode.m_Width );
	dataToUpload->SetInt( "height", g_pMaterialSystemConfig->m_VideoMode.m_Height );
	dataToUpload->SetInt( "AASamples", g_pMaterialSystemConfig->m_nAASamples );
	dataToUpload->SetInt( "AAQuality", g_pMaterialSystemConfig->m_nAAQuality );
	dataToUpload->SetInt( "AnisoLevel", g_pMaterialSystemConfig->m_nForceAnisotropicLevel );
	dataToUpload->SetInt( "SkipMipLevels", g_pMaterialSystemConfig->skipMipLevels );
	dataToUpload->SetInt( "DXLevel", g_pMaterialSystemConfig->dxSupportLevel );
	dataToUpload->SetInt( "ShadowDepthTexture", g_pMaterialSystemConfig->ShadowDepthTexture() );
	dataToUpload->SetInt( "MotionBlur", g_pMaterialSystemConfig->MotionBlur() );
	dataToUpload->SetInt( "Windowed", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_WINDOWED) ? 1 : 0 );
	dataToUpload->SetInt( "Trilinear", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_FORCE_TRILINEAR) ? 1 : 0 );
	dataToUpload->SetInt( "ForceHWSync", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_FORCE_HWSYNC) ? 1 : 0 );
	dataToUpload->SetInt( "NoWaitForVSync", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC) ? 1 : 0 );
	dataToUpload->SetInt( "DisableSpecular", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR) ? 1 : 0 );
	dataToUpload->SetInt( "DisableBumpmapping", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP) ? 1 : 0 );
	dataToUpload->SetInt( "EnableParallaxMapping", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING) ? 1 : 0 );
	dataToUpload->SetInt( "ZPrefill", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL) ? 1 : 0 );
	dataToUpload->SetInt( "ReduceFillRate", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_REDUCE_FILLRATE) ? 1 : 0 );
	dataToUpload->SetInt( "RenderToTextureShadows", r_shadowrendertotexture.GetInt() ? 1 : 0 );
	dataToUpload->SetInt( "FlashlightDepthTexture", r_flashlightdepthtexture.GetInt() ? 1 : 0 );
#ifndef _X360
	dataToUpload->SetInt( "RealtimeWaterReflection", r_waterforceexpensive.GetInt() ? 1 : 0 );
#endif
	dataToUpload->SetInt( "WaterReflectEntities", r_waterforcereflectentities.GetInt() ? 1 : 0 );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Dumps the specified config info to the console
//-----------------------------------------------------------------------------
void PrintMaterialSystemConfig( const MaterialSystem_Config_t &config )
{
	Warning( "width: %d\n", config.m_VideoMode.m_Width );
	Warning( "height: %d\n", config.m_VideoMode.m_Height );
	Warning( "m_nForceAnisotropicLevel: %d\n", config.m_nForceAnisotropicLevel );
	Warning( "aasamples: %d\n", config.m_nAASamples );
	Warning( "aaquality: %d\n", config.m_nAAQuality );

	Warning( "skipMipLevels: %d\n", config.skipMipLevels );
	Warning( "dxSupportLevel: %d\n", config.dxSupportLevel );
	Warning( "monitorGamma: %f\n", config.m_fMonitorGamma );
	Warning( "MATSYS_VIDCFG_FLAGS_WINDOWED: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_WINDOWED ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_FORCE_TRILINEAR: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_FORCE_TRILINEAR ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_FORCE_HWSYNC: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_FORCE_HWSYNC ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_REDUCE_FILLRATE: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_REDUCE_FILLRATE ) ? "true" : "false" );
	Warning( "r_shadowrendertotexture: %s\n", r_shadowrendertotexture.GetInt() ? "true" : "false" );
	Warning( "motionblur: %s\n", config.m_bMotionBlur ? "true" : "false" );
	Warning( "shadowdepthtexture: %s\n", config.m_bShadowDepthTexture ? "true" : "false" );
#ifndef _X360
	Warning( "r_waterforceexpensive: %s\n", r_waterforceexpensive.GetInt() ? "true" : "false" );
#endif
	Warning( "r_waterforcereflectentities: %s\n", r_waterforcereflectentities.GetInt() ? "true" : "false" );
}

CON_COMMAND( mat_configcurrent, "show the current video control panel config for the material system" )
{
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	PrintMaterialSystemConfig( config );
}

#if !defined(SWDS) && !defined( _X360 )
CON_COMMAND( mat_setvideomode, "sets the width, height, windowed state of the material system" )
{
	if ( args.ArgC() != 4 )
		return;

	int nWidth = Q_atoi( args[1] );
	int nHeight = Q_atoi( args[2] );
	bool bWindowed = Q_atoi( args[3] ) > 0 ? true : false;

	videomode->SetMode( nWidth, nHeight, bWindowed );
}
#endif

CON_COMMAND( mat_enable_vrmode, "Switches the material system to VR mode (after restart)" )
{
	if( args.ArgC() != 2 )
		return;

	if( !g_pSourceVR )
		return;

	ConVarRef mat_vrmode_adapter( "mat_vrmode_adapter" );
	bool bVRMode = Q_atoi( args[1] ) != 0;
	if( bVRMode )
	{
#if defined( _WIN32 )
		int32 nVRModeAdapter = g_pSourceVR->GetVRModeAdapter();
		if( nVRModeAdapter == -1 )
		{
			Warning( "Unable to get VRModeAdapter from OpenVR. VR mode will not be enabled. Try restarting and then enabling VR again.\n" );
		}
		mat_vrmode_adapter.SetValue( nVRModeAdapter );
#else
		mat_vrmode_adapter.SetValue( 0 ); // This convar isn't actually used on other platforms so just use 0 to indicate that it's set
#endif
	}
	else
	{
		mat_vrmode_adapter.SetValue( -1 );
	}
}


CON_COMMAND( mat_savechanges, "saves current video configuration to the registry" )
{
	// if the user has got to the point where they can adjust and apply video changes, then we can clear safe mode
	CommandLine()->RemoveParm( "-safe" );

	// write out config
	UpdateMaterialSystemConfig();
	if ( !CommandLine()->FindParm( "-dxlevel" ) )
	{
		WriteMaterialSystemConfigToRegistry( *g_pMaterialSystemConfig );
	}
}

#ifdef _DEBUG
//-----------------------------------------------------------------------------
// A console command to debug materials
//-----------------------------------------------------------------------------
CON_COMMAND_F( mat_debug, "Activates debugging spew for a specific material.", FCVAR_CHEAT )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg ("usage: mat_debug [ <material name> ]\n");
		return;
	}

	materials->ToggleDebugMaterial( args[1] );
}

//-----------------------------------------------------------------------------
// A console command to suppress materials
//-----------------------------------------------------------------------------
CON_COMMAND_F( mat_suppress, "Supress a material from drawing", FCVAR_CHEAT )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg ("usage: mat_suppress [ <material name> ]\n");
		return;
	}

	materials->ToggleSuppressMaterial( args[1] );
}
#endif // _DEBUG

static ITexture *CreatePowerOfTwoFBTexture( void )
{
	if ( IsX360() )
		return NULL;

	return materials->CreateNamedRenderTargetTextureEx2( 
		"_rt_PowerOfTwoFB",
		1024, 1024, RT_SIZE_DEFAULT,
		// Has dest alpha for vort warp effect
		IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateWaterReflectionTexture( void )
{
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterReflection",
		1024, 1024, RT_SIZE_PICMIP,
		materials->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateWaterRefractionTexture( void )
{
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterRefraction",
		1024, 1024, RT_SIZE_PICMIP,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888, 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateCameraTexture( void )
{
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_Camera",
		256, 256, RT_SIZE_DEFAULT,
		materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateBuildCubemaps16BitTexture( void )
{
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_BuildCubemaps16bit",
		0, 0, 
		RT_SIZE_FULL_FRAME_BUFFER,
		IMAGE_FORMAT_RGBA16161616, 
		MATERIAL_RT_DEPTH_SHARED );
}

static ITexture *CreateQuarterSizedFBTexture( int n, unsigned int iRenderTargetFlags )
{
	char nbuf[20];
	sprintf(nbuf,"_rt_SmallFB%d",n);

	ImageFormat fmt=materials->GetBackBufferFormat();
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
		fmt = IMAGE_FORMAT_RGBA16161616F;

	return materials->CreateNamedRenderTargetTextureEx2(
		nbuf, 0, 0, RT_SIZE_HDR,
		fmt, MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 
		iRenderTargetFlags );
}

static ITexture *CreateTeenyFBTexture( int n )
{
	char nbuf[20];
	sprintf(nbuf,"_rt_TeenyFB%d",n);

	ImageFormat fmt = materials->GetBackBufferFormat();
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
		fmt = IMAGE_FORMAT_RGBA16161616F;

	return materials->CreateNamedRenderTargetTextureEx2(
		nbuf, 32, 32, RT_SIZE_DEFAULT,
		fmt, MATERIAL_RT_DEPTH_SHARED );
}

static ITexture *CreateFullFrameFBTexture( int textureIndex, int iExtraFlags = 0 )
{
	char textureName[256];
	if ( textureIndex > 0 )
	{
		sprintf( textureName, "_rt_FullFrameFB%d", textureIndex );
	}
	else
	{
		V_strcpy_safe( textureName, "_rt_FullFrameFB" );
	}

	int rtFlags = iExtraFlags | CREATERENDERTARGETFLAGS_HDR;
	if ( IsX360() )
	{
		// just make the system memory texture only
		rtFlags |= CREATERENDERTARGETFLAGS_NOEDRAM;
	}
	return materials->CreateNamedRenderTargetTextureEx2(
		textureName,
		1, 1, RT_SIZE_FULL_FRAME_BUFFER, materials->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		rtFlags );
}

static ITexture *CreateFullFrameDepthTexture( void )
{
	if ( IsX360() )
	{
		return materials->CreateNamedRenderTargetTextureEx2( "_rt_FullFrameDepth", 1, 1, 
			RT_SIZE_FULL_FRAME_BUFFER, materials->GetShadowDepthTextureFormat(), MATERIAL_RT_DEPTH_NONE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
			CREATERENDERTARGETFLAGS_NOEDRAM );

	}
	else
	{
		materials->AddTextureAlias( "_rt_FullFrameDepth", "_rt_PowerOfTwoFB" );
	}
	return NULL;
}


static ITexture *CreateResolvedFullFrameDepthTexture( void )
{
	if ( IsPC() )
	{
		return materials->CreateNamedRenderTargetTextureEx2( "_rt_ResolvedFullFrameDepth", 1, 1, 
			RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
			CREATERENDERTARGETFLAGS_NOEDRAM );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Create render targets which mods rely upon to render correctly
//-----------------------------------------------------------------------------
void InitWellKnownRenderTargets( void )
{
#if !defined( SWDS )
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: InitWellKnownRenderTargets\n" );
	}

	// Begin block in which all render targets should be allocated
	materials->BeginRenderTargetAllocation();

	// JasonM - 
	// Do we put logic in here to determine which of these to create, based upon DX level, HDR enable etc?
	// YES! DX Level should gate these

	// before we create anything, see if VR mode wants to override the "framebuffer" size
	if( UseVR() )
	{
		int nWidth, nHeight;
		g_pSourceVR->GetRenderTargetFrameBufferDimensions( nWidth, nHeight );
		g_pMaterialSystem->SetRenderTargetFrameBufferSizeOverrides( nWidth, nHeight );
	}
	else
	{
		g_pMaterialSystem->SetRenderTargetFrameBufferSizeOverrides( 0, 0 );
	}

	// Create the render targets upon which mods may rely

	if ( IsPC() )
	{
		// Create for all mods as vgui2 uses it for 3D painting
		g_PowerOfTwoFBTexture.Init( CreatePowerOfTwoFBTexture() );
	}

	// Create these for all mods because the engine references them
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
	{
		if ( IsPC() && g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 && 
			g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
		{
			// Used for building HDR Cubemaps
			g_BuildCubemaps16BitTexture.Init( CreateBuildCubemaps16BitTexture() );
		}

		// Used in Bloom effects
		g_QuarterSizedFBTexture0.Init( CreateQuarterSizedFBTexture( 0, 0 ) );
		if( IsX360() )
			materials->AddTextureAlias( "_rt_SmallFB1", "_rt_SmallFB0" ); //an alias is good enough on the 360 since we don't have a texture lock problem during post processing
		else
			g_QuarterSizedFBTexture1.Init( CreateQuarterSizedFBTexture( 1, 0 ) );			
	}

	if ( IsPC() )
	{
		g_TeenyFBTexture0.Init( CreateTeenyFBTexture( 0 ) );
		g_TeenyFBTexture1.Init( CreateTeenyFBTexture( 1 ) );
		g_TeenyFBTexture2.Init( CreateTeenyFBTexture( 2 ) );
	}

	g_FullFrameFBTexture0.Init( CreateFullFrameFBTexture( 0 ) );
	g_FullFrameFBTexture1.Init( CreateFullFrameFBTexture( 1 ) );

	if ( IsX360() )
	{
		g_FullFrameFBTexture2.Init( CreateFullFrameFBTexture( 2, CREATERENDERTARGETFLAGS_TEMP ) );
	}

	g_FullFrameDepth.Init( CreateFullFrameDepthTexture() );
	g_ResolvedFullFrameDepth.Init( CreateResolvedFullFrameDepthTexture() );

	// if we're in stereo mode init a render target for VGUI
	if( UseVR() )
	{
		g_pSourceVR->CreateRenderTargets( materials );
	}

	// Allow the client to init their own mod-specific render targets
	if ( g_pClientRenderTargets )
	{
		g_pClientRenderTargets->InitClientRenderTargets( materials, g_pMaterialSystemHardwareConfig );
	}
	else
	{
		// If this mod doesn't define the interface, fallback to initializing the standard render textures 
		// NOTE: these should match up with the 'Get' functions in cl_dll/rendertexture.h/cpp
		g_WaterReflectionTexture.Init( CreateWaterReflectionTexture() );
		g_WaterRefractionTexture.Init( CreateWaterRefractionTexture() );
		g_CameraTexture.Init( CreateCameraTexture() );
	}

	// End block in which all render targets should be allocated (kicking off an Alt-Tab type behavior)
	materials->EndRenderTargetAllocation();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( g_FullFrameFBTexture0, MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Shut down the render targets which mods rely upon to render correctly
//-----------------------------------------------------------------------------
void ShutdownWellKnownRenderTargets( void )
{
#if !defined( SWDS )
	if ( IsX360() )
	{
		// cannot allowing RT's to reconstruct, causes other fatal problems
		// many other 360 systems have been coded with this expected constraint
		Assert( 0 );
		return;
	}

	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: ShutdownWellKnownRenderTargets\n" );
	}

	g_PowerOfTwoFBTexture.Shutdown();
	g_BuildCubemaps16BitTexture.Shutdown();
		
	g_QuarterSizedFBTexture0.Shutdown();
	
	if( IsX360() )
		materials->RemoveTextureAlias( "_rt_SmallFB1" );
	else
		g_QuarterSizedFBTexture1.Shutdown();
	
	g_TeenyFBTexture0.Shutdown();
	g_TeenyFBTexture1.Shutdown();
	g_TeenyFBTexture2.Shutdown();
	g_FullFrameFBTexture0.Shutdown();
	g_FullFrameFBTexture1.Shutdown();
	if ( IsX360() )
	{
		g_FullFrameFBTexture2.Shutdown();
	}
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( NULL, MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );

	g_FullFrameDepth.Shutdown();
	if( IsPC() )
	{
		materials->RemoveTextureAlias( "_rt_FullFrameDepth" );
	}

	if( g_pSourceVR )
		g_pSourceVR->ShutdownRenderTargets();


	// Shutdown client render targets
	if ( g_pClientRenderTargets )
	{
		g_pClientRenderTargets->ShutdownClientRenderTargets();
	}
	else
	{
		g_WaterReflectionTexture.Shutdown();
		g_WaterRefractionTexture.Shutdown();
		g_CameraTexture.Shutdown();
	}
#endif
}


//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
CON_COMMAND( mat_reset_rendertargets, "Resets all the render targets" )
{
	ShutdownWellKnownRenderTargets();
	InitWellKnownRenderTargets();
}


//-----------------------------------------------------------------------------
// Purpose: Make the debug system materials
//-----------------------------------------------------------------------------
static void InitDebugMaterials( void )
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: InitDebugMaterials\n" );
	}

	g_materialEmpty = GL_LoadMaterial( "debug/debugempty", TEXTURE_GROUP_OTHER );
#ifndef SWDS
	g_materialWireframe = GL_LoadMaterial( "debug/debugwireframe", TEXTURE_GROUP_OTHER );
	g_materialTranslucentSingleColor = GL_LoadMaterial( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
	g_materialTranslucentVertexColor = GL_LoadMaterial( "debug/debugtranslucentvertexcolor", TEXTURE_GROUP_OTHER );
	g_materialWorldWireframe = GL_LoadMaterial( "debug/debugworldwireframe", TEXTURE_GROUP_OTHER );
	g_materialWorldWireframeZBuffer = GL_LoadMaterial( "debug/debugworldwireframezbuffer", TEXTURE_GROUP_OTHER );

	g_materialBrushWireframe = GL_LoadMaterial( "debug/debugbrushwireframe", TEXTURE_GROUP_OTHER );
	g_materialDecalWireframe = GL_LoadMaterial( "debug/debugdecalwireframe", TEXTURE_GROUP_OTHER );
	g_materialDebugLightmap = GL_LoadMaterial( "debug/debuglightmap", TEXTURE_GROUP_OTHER );
	g_materialDebugLightmapZBuffer = GL_LoadMaterial( "debug/debuglightmapzbuffer", TEXTURE_GROUP_OTHER );
	g_materialDebugLuxels = GL_LoadMaterial( "debug/debugluxels", TEXTURE_GROUP_OTHER );

	g_materialLeafVisWireframe = GL_LoadMaterial( "debug/debugleafviswireframe", TEXTURE_GROUP_OTHER );
	g_pMaterialWireframeVertexColor = GL_LoadMaterial( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );
	g_pMaterialWireframeVertexColorIgnoreZ = GL_LoadMaterial( "debug/debugwireframevertexcolorignorez", TEXTURE_GROUP_OTHER );
	g_pMaterialLightSprite = GL_LoadMaterial( "engine/lightsprite", TEXTURE_GROUP_OTHER );
	g_pMaterialShadowBuild = GL_LoadMaterial( "engine/shadowbuild", TEXTURE_GROUP_OTHER);
	g_pMaterialMRMWireframe = GL_LoadMaterial( "debug/debugmrmwireframe", TEXTURE_GROUP_OTHER );
	g_pMaterialDebugFlat = GL_LoadMaterial( "debug/debugdrawflattriangles", TEXTURE_GROUP_OTHER );

	g_pMaterialAmbientCube = GL_LoadMaterial( "debug/debugambientcube", TEXTURE_GROUP_OTHER );

	g_pMaterialWriteZ = GL_LoadMaterial( "engine/writez", TEXTURE_GROUP_OTHER );

	if( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 )
	{
		// Materials for writing to shadow depth buffer
		KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 0 );
		pVMTKeyValues->SetInt( "$nocull", 0 );
		g_pMaterialDepthWrite[0][0] = g_pMaterialSystem->FindProceduralMaterial( "__DepthWrite00", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialDepthWrite[0][0]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 0 );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		g_pMaterialDepthWrite[0][1] = g_pMaterialSystem->FindProceduralMaterial( "__DepthWrite01", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialDepthWrite[0][1]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 1 );
		pVMTKeyValues->SetInt( "$nocull", 0 );
		g_pMaterialDepthWrite[1][0] = g_pMaterialSystem->FindProceduralMaterial( "__DepthWrite10", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialDepthWrite[1][0]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 1 );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		g_pMaterialDepthWrite[1][1] = g_pMaterialSystem->FindProceduralMaterial( "__DepthWrite11", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialDepthWrite[1][1]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 0 );
		pVMTKeyValues->SetInt( "$nocull", 0 );
		pVMTKeyValues->SetInt( "$color_depth", 1 );
		g_pMaterialSSAODepthWrite[0][0] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite00", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialSSAODepthWrite[0][0]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 0 );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$color_depth", 1 );
		g_pMaterialSSAODepthWrite[0][1] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite01", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialSSAODepthWrite[0][1]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 1 );
		pVMTKeyValues->SetInt( "$nocull", 0 );
		pVMTKeyValues->SetInt( "$color_depth", 1 );
		g_pMaterialSSAODepthWrite[1][0] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite10", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialSSAODepthWrite[1][0]->IncrementReferenceCount();

		pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 1 );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$color_depth", 1 );
		g_pMaterialSSAODepthWrite[1][1] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite11", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		g_pMaterialSSAODepthWrite[1][1]->IncrementReferenceCount();
	}
	else
	{
		g_pMaterialDepthWrite[0][0] = g_pMaterialDepthWrite[0][1] = g_pMaterialDepthWrite[1][0] = g_pMaterialDepthWrite[1][1] = NULL;
		g_pMaterialSSAODepthWrite[0][0] = g_pMaterialSSAODepthWrite[0][1] = g_pMaterialSSAODepthWrite[1][0] = g_pMaterialSSAODepthWrite[1][1] = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ShutdownDebugMaterials( void )
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: ShutdownDebugMaterials\n" );
	}

	GL_UnloadMaterial( g_materialEmpty );
#ifndef SWDS
	GL_UnloadMaterial( g_pMaterialLightSprite );
	GL_UnloadMaterial( g_pMaterialWireframeVertexColor );
	GL_UnloadMaterial( g_pMaterialWireframeVertexColorIgnoreZ );
	GL_UnloadMaterial( g_materialLeafVisWireframe );

	GL_UnloadMaterial( g_materialDebugLuxels );
	GL_UnloadMaterial( g_materialDebugLightmapZBuffer );
	GL_UnloadMaterial( g_materialDebugLightmap );
	GL_UnloadMaterial( g_materialDecalWireframe );
	GL_UnloadMaterial( g_materialBrushWireframe );

	GL_UnloadMaterial( g_materialWorldWireframeZBuffer );
	GL_UnloadMaterial( g_materialWorldWireframe );
	GL_UnloadMaterial( g_materialTranslucentSingleColor );
	GL_UnloadMaterial( g_materialTranslucentVertexColor );
	GL_UnloadMaterial( g_materialWireframe );
	GL_UnloadMaterial( g_pMaterialShadowBuild );
	GL_UnloadMaterial( g_pMaterialMRMWireframe );
	GL_UnloadMaterial( g_pMaterialWriteZ );

	GL_UnloadMaterial( g_pMaterialAmbientCube );
	GL_UnloadMaterial( g_pMaterialDebugFlat );

	// Materials for writing to shadow depth buffer
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 )
	{
		for (int i = 0; i<2; i++)
		{
			for (int j = 0; j<2; j++)
			{
				if( g_pMaterialDepthWrite[i][j] )
				{
					g_pMaterialDepthWrite[i][j]->DecrementReferenceCount();
				}
				g_pMaterialDepthWrite[i][j] = NULL;

				if( g_pMaterialSSAODepthWrite[i][j] )
				{
					g_pMaterialSSAODepthWrite[i][j]->DecrementReferenceCount();
				}
				g_pMaterialSSAODepthWrite[i][j] = NULL;
			}
		}
	}

#endif
}


//-----------------------------------------------------------------------------
// Used to deal with making sure Present is called often enough 
//-----------------------------------------------------------------------------
void InitStartupScreen()
{
	if ( !IsX360() )
		return;

#ifdef _X360
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );
	bool bIsWidescreen = videoMode.fIsWideScreen != FALSE;
#else
	int width, height;
	materials->GetBackBufferDimensions( width, height );
	float aspectRatio = (float)width/(float)height;
	bool bIsWidescreen = aspectRatio >= 1.5999f;
#endif

	// NOTE: Brutal hackery, this code is duplicated in gameui.dll
	// but I have to do this prior to gameui being loaded.
	// 360 uses hi-res game specific backgrounds
	char gameName[MAX_PATH];
	char filename[MAX_PATH];
	V_FileBase( com_gamedir, gameName, sizeof( gameName ) );
	V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_%s%s", gameName, ( bIsWidescreen ? "_widescreen" : "" ) );

	ITexture *pTexture = materials->FindTexture( filename, TEXTURE_GROUP_OTHER );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( pTexture, MATERIAL_NON_INTERACTIVE_MODE_STARTUP );

	pTexture = materials->FindTexture( "//platform/materials/engine/box", TEXTURE_GROUP_OTHER );

	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		if ( V_stricmp( modinfo->GetString("type", "singleplayer_only" ), "multiplayer_only" ) == 0 )
		{
			pRenderContext->SetNonInteractivePacifierTexture( pTexture, 0.5f, 0.9f, 0.1f );
		}
		else
		{
			pRenderContext->SetNonInteractivePacifierTexture( pTexture, 0.5f, 0.86f, 0.1f );
		}
	}
	modinfo->deleteThis();

	BeginLoadingUpdates( MATERIAL_NON_INTERACTIVE_MODE_STARTUP );
}


//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
CON_COMMAND( mat_info, "Shows material system info" )
{
	materials->SpewDriverInfo();
}

void InitMaterialSystem( void )
{
	materials->AddReleaseFunc( ReleaseMaterialSystemObjects );
	materials->AddRestoreFunc( RestoreMaterialSystemObjects );

	UpdateMaterialSystemConfig();

	InitWellKnownRenderTargets();

	InitDebugMaterials();

#ifndef SWDS
	DispInfo_InitMaterialSystem();
#endif

#ifdef BENCHMARK
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 80 )
	{
		Error( "dx6 and dx7 hardware not supported by this benchmark!" );
	}
#endif
}

void ShutdownMaterialSystem( void )
{
	ShutdownDebugMaterials();

	ShutdownWellKnownRenderTargets();

#ifndef SWDS
	DispInfo_ShutdownMaterialSystem();
#endif
}

//-----------------------------------------------------------------------------
// Methods to restore, release material system objects
//-----------------------------------------------------------------------------
void ReleaseMaterialSystemObjects()
{
#ifndef SWDS
	DispInfo_ReleaseMaterialSystemObjects( host_state.worldmodel );

	modelrender->ReleaseAllStaticPropColorData();
#endif

#ifndef SWDS
	WorldStaticMeshDestroy();
#endif
	g_LostVideoMemory = true;
}

void RestoreMaterialSystemObjects( int nChangeFlags )
{
	if ( IsX360() )
		return;

	bool bThreadingAllowed = Host_AllowQueuedMaterialSystem( false );
	g_LostVideoMemory = false;

	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
	{
		// ensure decals have no stale references to invalid lods
		modelrender->RemoveAllDecalsFromAllModels();
	}

	if (host_state.worldmodel)
	{
		if ( (nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED) || materials->GetNumSortIDs() == 0 )
		{
#ifndef SWDS
			// Reload lightmaps, world meshes, etc. because we may have switched from bumped to unbumped
			R_LoadWorldGeometry( true );
#endif
		}
		else
		{
			modelloader->Map_LoadDisplacements( host_state.worldmodel, true );
#ifndef SWDS
			WorldStaticMeshCreate();
			// Gotta recreate the lightmaps
			R_RedownloadAllLightmaps();
#endif
		}

#ifndef SWDS
		// Need to re-figure out the env_cubemaps, so blow away the lightcache.
		R_StudioInitLightingCache();
		modelrender->RestoreAllStaticPropColorData();
#endif
	}

#ifndef DEDICATED
	cl.ForceFullUpdate();
#endif

	Host_AllowQueuedMaterialSystem( bThreadingAllowed );
}

bool TangentSpaceSurfaceSetup( SurfaceHandle_t surfID, Vector &tVect )
{
	Vector sVect;
	VectorCopy( MSurf_TexInfo( surfID )->textureVecsTexelsPerWorldUnits[0].AsVector3D(), sVect );
	VectorCopy( MSurf_TexInfo( surfID )->textureVecsTexelsPerWorldUnits[1].AsVector3D(), tVect );
	VectorNormalize( sVect );
	VectorNormalize( tVect );
	Vector tmpVect;
	CrossProduct( sVect, tVect, tmpVect );
	// Make sure that the tangent space works if textures are mapped "backwards".
	if( DotProduct( MSurf_Plane( surfID ).normal, tmpVect ) > 0.0f )
	{
		return true;
	}
	return false;
}

void TangentSpaceComputeBasis( Vector& tangentS, Vector& tangentT, const Vector& normal, const Vector& tVect, bool negateTangent )
{
	// tangent x binormal = normal
	// tangent = sVect
	// binormal = tVect
	CrossProduct( normal, tVect, tangentS );
	VectorNormalize( tangentS );
	CrossProduct( tangentS, normal, tangentT );
	VectorNormalize( tangentT );

	if ( negateTangent )
	{
		VectorScale( tangentS, -1.0f, tangentS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MaterialSystem_DestroySortinfo( void )
{
	if ( materialSortInfoArray )
	{
#ifndef SWDS
		WorldStaticMeshDestroy();
#endif
		delete[] materialSortInfoArray;
		materialSortInfoArray = NULL;
	}
}


#ifndef SWDS

// The amount to blend between basetexture and basetexture2 used to sit in lightmap
// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
// using it, we have to make sure the vertex has the color and alpha specified correctly
// or it will look weird.
static inline bool CheckMSurfaceBaseTexture2( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID )
{
	if ( !SurfaceHasDispInfo( surfID ) && 
			(MSurf_TexInfo( surfID )->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
	{
		const char *pMaterialName = MSurf_TexInfo( surfID )->material->GetName();
		if ( pMaterialName )
		{
			bool bShowIt = false;
			if ( developer.GetInt() <= 1 )
			{
				static CUtlDict<int,int> nameDict;
				if ( nameDict.Find( pMaterialName ) == -1 )
				{
					nameDict.Insert( pMaterialName, 0 );
					bShowIt = true;
				}
			}
			else
			{
				bShowIt = true;
			}

			if ( bShowIt )
			{
				// Calculate the surface's centerpoint.
				Vector vCenter( 0, 0, 0 );
				for ( int i = 0; i < MSurf_VertCount( surfID ); i++ )
				{
					int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];
					vCenter += pBrushData->vertexes[vertIndex].position;
				}
				vCenter /= (float)MSurf_VertCount( surfID );
				
				// Spit out the warning.				
				Warning( "Warning: using WorldTwoTextureBlend on a non-displacement surface.\n"
						 "Support for this will go away soon.\n"
						 "   - Material       : %s\n"
						 "   - Surface center : %d %d %d\n"
						 , pMaterialName, (int)vCenter.x, (int)vCenter.y, (int)vCenter.z );
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Build a vertex buffer for this face
// Input  : *pWorld - world model base
//			*surf - surf to add to the mesh
//			overbright - overbright factor (for colors)
//			&builder - mesh that holds the vertex buffer
//-----------------------------------------------------------------------------
#ifdef NEWMESH
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, float overbright, 
							   CVertexBufferBuilder &builder )
{
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );

	byte flatColor[4] = { 255, 255, 255, 255 };

	Vector tVect;
	bool negate = false;
	if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
	{
		negate = TangentSpaceSurfaceSetup( surfID, tVect );
	}

	CheckMSurfaceBaseTexture2( pBrushData, surfID );

	for ( int i = 0; i < MSurf_VertCount( surfID ); i++ )
	{
		int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];

		// world-space vertex
		Vector& vec = pBrushData->vertexes[vertIndex].position;

		// output to mesh
		builder.Position3fv( vec.Base() );

		Vector2D uv;
		SurfComputeTextureCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 0, uv.Base() );

		// garymct: normalized (within space of surface) lightmap texture coordinates
		SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 1, uv.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			// bump maps appear left to right in lightmap page memory, calculate 
			// the offset for the width of a single map. The pixel shader will use 
			// this to compute the actual texture coordinates
			builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
		}

		Vector& normal = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID ) + i] ];
		builder.Normal3fv( normal.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
		{
			Vector tangentS, tangentT;
			TangentSpaceComputeBasis( tangentS, tangentT, normal, tVect, negate );
			builder.TangentS3fv( tangentS.Base() );
			builder.TangentT3fv( tangentT.Base() );
		}

		// The amount to blend between basetexture and basetexture2 used to sit in lightmap
		// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
		// using it, we have to make sure the vertex has the color and alpha specified correctly
		// or it will look weird.
		if ( !SurfaceHasDispInfo( surfID ) && 
			 (MSurf_TexInfo( surfID )->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
		{
			static bool bWarned = false;
			if ( !bWarned )
			{
				const char *pMaterialName = MSurf_TexInfo( surfID )->material->GetName();
				bWarned = true;
				Warning( "Warning: WorldTwoTextureBlend found on a non-displacement surface (material: %s). This wastes perf for no benefit.\n", pMaterialName );
			}

			builder.Color4ub( 255, 255, 255, 0 );
		}
		else
		{
			builder.Color3ubv( flatColor );
		}
		
		builder.AdvanceVertex();
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose: Build a vertex buffer for this face
// Input  : *pWorld - world model base
//			*surf - surf to add to the mesh
//			overbright - overbright factor (for colors)
//			&builder - mesh that holds the vertex buffer
//-----------------------------------------------------------------------------
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, float overbright, 
							   CMeshBuilder &builder )
{
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );

	byte flatColor[4] = { 255, 255, 255, 255 };

	Vector tVect;
	bool negate = false;
	if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
	{
		negate = TangentSpaceSurfaceSetup( surfID, tVect );
	}

	CheckMSurfaceBaseTexture2( pBrushData, surfID );

	for ( int i = 0; i < MSurf_VertCount( surfID ); i++ )
	{
		int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];

		// world-space vertex
		Vector& vec = pBrushData->vertexes[vertIndex].position;

		// output to mesh
		builder.Position3fv( vec.Base() );

		Vector2D uv;
		SurfComputeTextureCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 0, uv.Base() );

		// garymct: normalized (within space of surface) lightmap texture coordinates
		SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 1, uv.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			// bump maps appear left to right in lightmap page memory, calculate 
			// the offset for the width of a single map. The pixel shader will use 
			// this to compute the actual texture coordinates

			if ( uv.x + ctx.m_BumpSTexCoordOffset*3 > 1.00001f )
			{
				Assert(0);

				SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
			}
			builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
		}

		Vector& normal = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID ) + i] ];
		builder.Normal3fv( normal.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
		{
			Vector tangentS, tangentT;
			TangentSpaceComputeBasis( tangentS, tangentT, normal, tVect, negate );
			builder.TangentS3fv( tangentS.Base() );
			builder.TangentT3fv( tangentT.Base() );
		}

		// The amount to blend between basetexture and basetexture2 used to sit in lightmap
		// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
		// using it, we have to make sure the vertex has the color and alpha specified correctly
		// or it will look weird.
		if ( !SurfaceHasDispInfo( surfID ) && 
			 (MSurf_TexInfo( surfID )->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
		{
			static bool bWarned = false;
			if ( !bWarned )
			{
				const char *pMaterialName = MSurf_TexInfo( surfID )->material->GetName();
				bWarned = true;
				Warning( "Warning: WorldTwoTextureBlend found on a non-displacement surface (material: %s). This wastes perf for no benefit.\n", pMaterialName );
			}

			builder.Color4ub( 255, 255, 255, 0 );
		}
		else
		{
			builder.Color3ubv( flatColor );
		}
		
		builder.AdvanceVertex();
	}
}
#endif // NEWMESH

static int VertexCountForSurfaceList( const CMSurfaceSortList &list, const surfacesortgroup_t &group )
{
	int vertexCount = 0;
	MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(list, group, surfID)
		vertexCount += MSurf_VertCount(surfID);
	MSL_FOREACH_SURFACE_IN_GROUP_END();
	return vertexCount;
}

//-----------------------------------------------------------------------------
// Builds a static mesh from a list of all surfaces with the same material
//-----------------------------------------------------------------------------

struct meshlist_t
{
#ifdef NEWMESH
	IVertexBuffer *pVertexBuffer;
#else
	IMesh *pMesh;
#endif
	IMaterial *pMaterial;
	int vertCount;
	VertexFormat_t vertexFormat;
};

static CUtlVector<meshlist_t> g_Meshes;

ConVar mat_max_worldmesh_vertices("mat_max_worldmesh_vertices", "65536");

static VertexFormat_t GetUncompressedFormat( const IMaterial * pMaterial )
{
	// FIXME: IMaterial::GetVertexFormat() should do this stripping (add a separate 'SupportsCompression' accessor)
	return ( pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED );
}

int FindOrAddMesh( IMaterial *pMaterial, int vertexCount )
{
	VertexFormat_t format = GetUncompressedFormat( pMaterial );

	CMatRenderContextPtr pRenderContext( materials );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int worldLimit = mat_max_worldmesh_vertices.GetInt();
	worldLimit = max(worldLimit,1024);
	if ( nMaxVertices > worldLimit )
	{
		nMaxVertices = mat_max_worldmesh_vertices.GetInt();
	}

	for ( int i = 0; i < g_Meshes.Count(); i++ )
	{
		if ( g_Meshes[i].vertexFormat != format )
			continue;

		if ( g_Meshes[i].vertCount + vertexCount > nMaxVertices )
			continue;

		g_Meshes[i].vertCount += vertexCount;
		return i;
	}

	int index = g_Meshes.AddToTail();
	g_Meshes[index].vertCount = vertexCount;
	g_Meshes[index].vertexFormat = format;
	g_Meshes[index].pMaterial = pMaterial;

	return index;
}

void SetTexInfoBaseTexture2Flags()
{
	for ( int i=0; i < host_state.worldbrush->numtexinfo; i++ )
	{
		host_state.worldbrush->texinfo[i].texinfoFlags &= ~TEXINFO_USING_BASETEXTURE2;
	}
	
	for ( int i=0; i < host_state.worldbrush->numtexinfo; i++ )
	{
		mtexinfo_t *pTexInfo = &host_state.worldbrush->texinfo[i];
		IMaterial *pMaterial = pTexInfo->material;
		if ( !pMaterial )
			continue;

		IMaterialVar **pParms = pMaterial->GetShaderParams();
		int nParms = pMaterial->ShaderParamCount();
		for ( int j=0; j < nParms; j++ )
		{
			if ( !pParms[j]->IsDefined() )
				continue;

			if ( Q_stricmp( pParms[j]->GetName(), "$basetexture2" ) == 0 )
			{
				pTexInfo->texinfoFlags |= TEXINFO_USING_BASETEXTURE2;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Determines vertex formats for all the world geometry
//-----------------------------------------------------------------------------
VertexFormat_t ComputeWorldStaticMeshVertexFormat( const IMaterial * pMaterial )
{
	VertexFormat_t vertexFormat = GetUncompressedFormat( pMaterial );

	// FIXME: set VERTEX_FORMAT_COMPRESSED if there are no artifacts and if it saves enough memory (use 'mem_dumpvballocs')
	// vertexFormat |= VERTEX_FORMAT_COMPRESSED;
	// FIXME: check for and strip unused vertex elements (TANGENT_S/T?)

	return vertexFormat;
}

//-----------------------------------------------------------------------------
// Builds static meshes for all the world geometry
//-----------------------------------------------------------------------------
void WorldStaticMeshCreate( void )
{
	r_framecount = 1;
	WorldStaticMeshDestroy();
	g_Meshes.RemoveAll();

	SetTexInfoBaseTexture2Flags();

	int nSortIDs = materials->GetNumSortIDs();
	if ( nSortIDs == 0 )
	{
		// this is probably a bug in alt-tab.  It's calling this as a restore function
		// but the lightmaps haven't been allocated yet
		Assert(0);
		return;
	}

	// Setup sortbins for flashlight rendering
	// FIXME!!!!  Could have less bins since we don't care about the lightmap
	// for projective light rendering purposes.
	// Not entirely true since we need the correct lightmap page for WorldVertexTransition materials.
	g_pShadowMgr->SetNumWorldMaterialBuckets( nSortIDs );

	Assert( !g_WorldStaticMeshes.Count() );
	g_WorldStaticMeshes.SetCount( nSortIDs );
	memset( g_WorldStaticMeshes.Base(), 0, sizeof(g_WorldStaticMeshes[0]) * g_WorldStaticMeshes.Count() );

	CMSurfaceSortList matSortArray;
	matSortArray.Init( nSortIDs, 512 );
	int *sortIndex = (int *)_alloca( sizeof(int) * g_WorldStaticMeshes.Count() );

	bool bTools = CommandLine()->CheckParm( "-tools" ) != NULL;

	int i;
	// sort the surfaces into the sort arrays
	for( int surfaceIndex = 0; surfaceIndex < host_state.worldbrush->numsurfaces; surfaceIndex++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfaceIndex );
		// set these flags here as they are determined by material data
		MSurf_Flags( surfID ) &= ~(SURFDRAW_TANGENTSPACE);

		// do we need to compute tangent space here?
		if ( bTools || ( MSurf_TexInfo( surfID )->material->GetVertexFormat() & VERTEX_TANGENT_SPACE ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_TANGENTSPACE;
		}
		
		// don't create vertex buffers for nodraw faces, water faces, or faces with dynamic data
//		if ( (MSurf_Flags( surfID ) & (SURFDRAW_NODRAW|SURFDRAW_WATERSURFACE|SURFDRAW_DYNAMIC)) 
//			|| SurfaceHasDispInfo( surfID ) )
		if( SurfaceHasDispInfo( surfID ) )
		{
			MSurf_VertBufferIndex( surfID ) = 0xFFFF;
			continue;
		}

		// attach to head of list
		matSortArray.AddSurfaceToTail( surfID, 0, MSurf_MaterialSortID( surfID ) );
	}

	// iterate the arrays and create buffers
	for ( i = 0; i < g_WorldStaticMeshes.Count(); i++ )
	{
		const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,i);
		int vertexCount = VertexCountForSurfaceList( matSortArray, group );

		SurfaceHandle_t surfID = matSortArray.GetSurfaceAtHead( group );
		g_WorldStaticMeshes[i] = NULL;
		sortIndex[i] = surfID ? FindOrAddMesh( MSurf_TexInfo( surfID )->material, vertexCount ) : -1;
	}

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "WorldStaticMeshCreate" );
#ifdef NEWMESH
	for ( i = 0; i < g_Meshes.Count(); i++ )
	{
		Assert( g_Meshes[i].vertCount > 0 );
		Assert( g_Meshes[i].pMaterial );
		g_Meshes[i].pVertexBuffer = pRenderContext->CreateStaticVertexBuffer( GetUncompressedFormat( g_Meshes[i].pMaterial ), g_Meshes[i].vertCount, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_WORLD );
		int vertBufferIndex = 0;
		// NOTE: Index count is zero because this will be a static vertex buffer!!!
		CVertexBufferBuilder vertexBufferBuilder;
		vertexBufferBuilder.Begin( g_Meshes[i].pVertexBuffer, g_Meshes[i].vertCount );
		for ( int j = 0; j < g_WorldStaticMeshes.Count(); j++ )
		{
			int meshId = sortIndex[j];
			if ( meshId == i )
			{
				g_WorldStaticMeshes[j] = g_Meshes[i].pVertexBuffer;
				const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,j);
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(matSortArray, group, surfID);
					MSurf_VertBufferIndex( surfID ) = vertBufferIndex;
					BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, OVERBRIGHT, vertexBufferBuilder );
		
					vertBufferIndex += MSurf_VertCount( surfID );

				MSL_FOREACH_SURFACE_IN_GROUP_END();
			}
		}
		vertexBufferBuilder.End();
		Assert(vertBufferIndex == g_Meshes[i].vertCount);
	}
#else
	for ( i = 0; i < g_Meshes.Count(); i++ )
	{
		Assert( g_Meshes[i].vertCount > 0 );
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "WorldStaticMeshCreate" );
		VertexFormat_t vertexFormat = ComputeWorldStaticMeshVertexFormat( g_Meshes[i].pMaterial );
		g_Meshes[i].pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_WORLD, g_Meshes[i].pMaterial );
		int vertBufferIndex = 0;
		// NOTE: Index count is zero because this will be a static vertex buffer!!!
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( g_Meshes[i].pMesh, MATERIAL_TRIANGLES, g_Meshes[i].vertCount, 0 );

		for ( int j = 0; j < g_WorldStaticMeshes.Count(); j++ )
		{
			int meshId = sortIndex[j];
			if ( meshId == i )
			{
				g_WorldStaticMeshes[j] = g_Meshes[i].pMesh;
				const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,j);
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(matSortArray, group, surfID);

					MSurf_VertBufferIndex( surfID ) = vertBufferIndex;
					BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, OVERBRIGHT, meshBuilder );
					vertBufferIndex += MSurf_VertCount( surfID );

				MSL_FOREACH_SURFACE_IN_GROUP_END();
			}
		}
		meshBuilder.End();
		Assert(vertBufferIndex == g_Meshes[i].vertCount);
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( NULL );
	}
#endif
	//Msg("Total %d meshes, %d before\n", g_Meshes.Count(), g_WorldStaticMeshes.Count() );
}

void WorldStaticMeshDestroy( void )
{
	CMatRenderContextPtr pRenderContext( materials );

	// Blat out the static meshes associated with each material
	for ( int i = 0; i < g_Meshes.Count(); i++ )
	{
#ifdef NEWMESH
		pRenderContext->DestroyVertexBuffer( g_Meshes[i].pVertexBuffer );
#else
		pRenderContext->DestroyStaticMesh( g_Meshes[i].pMesh );
#endif
	}
	g_WorldStaticMeshes.Purge();
	g_Meshes.RemoveAll();
}


//-----------------------------------------------------------------------------
// Compute texture and lightmap coordinates
//-----------------------------------------------------------------------------

void SurfComputeTextureCoordinate( SurfaceCtx_t const& ctx, SurfaceHandle_t surfID, 
									    Vector const& vec, Vector2D& uv )
{
	mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

	// base texture coordinate
	uv.x = DotProduct (vec, pTexInfo->textureVecsTexelsPerWorldUnits[0].AsVector3D()) + 
		pTexInfo->textureVecsTexelsPerWorldUnits[0][3];
	uv.x /= pTexInfo->material->GetMappingWidth();

	uv.y = DotProduct (vec, pTexInfo->textureVecsTexelsPerWorldUnits[1].AsVector3D()) + 
		pTexInfo->textureVecsTexelsPerWorldUnits[1][3];
	uv.y /= pTexInfo->material->GetMappingHeight();
}

#if _DEBUG
void CheckTexCoord( float coord )
{
	Assert(coord <= 1.0f );
}
#endif

void SurfComputeLightmapCoordinate( SurfaceCtx_t const& ctx, SurfaceHandle_t surfID, 
										 Vector const& vec, Vector2D& uv )
{
	if ( (MSurf_Flags( surfID ) & SURFDRAW_NOLIGHT) )
	{
		uv.x = uv.y = 0.5f;
	}
	else if ( MSurf_LightmapExtents( surfID )[0] == 0 )
	{
		uv = (0.5f * ctx.m_Scale + ctx.m_Offset);
	}
	else
	{
		mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

		uv.x = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
		uv.x -= MSurf_LightmapMins( surfID )[0];
		uv.x += 0.5f;

		uv.y = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];
		uv.y -= MSurf_LightmapMins( surfID )[1];
		uv.y += 0.5f;

		uv *= ctx.m_Scale;
		uv += ctx.m_Offset;

		assert( uv.IsValid() );
	}
#if _DEBUG
	// This was here for check against displacements and they actually get calculated later correctly.
//	CheckTexCoord( uv.x );
//	CheckTexCoord( uv.y );
#endif
	uv.x = clamp(uv.x, 0.0f, 1.0f);
	uv.y = clamp(uv.y, 0.0f, 1.0f);
}


//-----------------------------------------------------------------------------
// Compute a context necessary for creating vertex data
//-----------------------------------------------------------------------------

void SurfSetupSurfaceContext( SurfaceCtx_t& ctx, SurfaceHandle_t surfID )
{
	materials->GetLightmapPageSize( 
		SortInfoToLightmapPage( MSurf_MaterialSortID( surfID ) ), 
		&ctx.m_LightmapPageSize[0], &ctx.m_LightmapPageSize[1] );
	ctx.m_LightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
	ctx.m_LightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;

	ctx.m_Scale.x = 1.0f / ( float )ctx.m_LightmapPageSize[0];
	ctx.m_Scale.y = 1.0f / ( float )ctx.m_LightmapPageSize[1];

	ctx.m_Offset.x = ( float )MSurf_OffsetIntoLightmapPage( surfID )[0] * ctx.m_Scale.x;
	ctx.m_Offset.y = ( float )MSurf_OffsetIntoLightmapPage( surfID )[1] * ctx.m_Scale.y;

	if ( ctx.m_LightmapPageSize[0] != 0.0f )
	{
		ctx.m_BumpSTexCoordOffset = ( float )ctx.m_LightmapSize[0] / ( float )ctx.m_LightmapPageSize[0];
	}
	else
	{
		ctx.m_BumpSTexCoordOffset = 0.0f;
	}
}

#endif // SWDS
