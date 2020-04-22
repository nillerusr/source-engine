//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#include "render_pch.h"
#include "gl_shader.h"
#include "const.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "decal_private.h"
#include "gl_model_private.h"
#include "r_local.h"
#include "materialproxyfactory.h"
#include "enginestats.h"
#include "editor_sendcommand.h"
#include "gl_matsysiface.h"
#include "vmodes.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "traceinit.h"
#include "sys_dll.h"
#include "tier0/icommandline.h"
#include "materialsystem/idebugtextureinfo.h"
#include "common.h"
#include "iregistry.h"
#include "materialsystem/materialsystem_config.h"
#include "ivideomode.h"
#include "modes.h"
#include "ivideomode.h"
#include "tier2/tier2.h"

#include "igame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CMaterialProxyFactory s_MaterialProxyFactory;


//-----------------------------------------------------------------------------
// Connects to other
//-----------------------------------------------------------------------------
static void Shader_ConnectOtherMatSysInterfaces( )
{
	// NOTE: These two interfaces have been connected in the tier2 library
	if ( !g_pMaterialSystemHardwareConfig )
	{
		Shader_Disconnect();
		Sys_Error( "Could not get the material system hardware config interface! (2)" );
	}
	if ( !g_pMaterialSystemDebugTextureInfo )
	{
		Shader_Disconnect();
		Sys_Error( "Could not get the debug texture info interface!" );
	}
}


//-----------------------------------------------------------------------------
// Connect to interfaces we need
//-----------------------------------------------------------------------------
bool Shader_Connect( bool bSetProxyFactory )
{
	if ( !materials )
		return false;

	int nAdapter = CommandLine()->ParmValue( "-adapter", 0 );
	int nModeFlags = MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	if ( CommandLine()->FindParm( "-ref" ) )
	{
		nModeFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}

	materials->SetAdapter( nAdapter, nModeFlags );
	if ( bSetProxyFactory )
		materials->SetMaterialProxyFactory( &s_MaterialProxyFactory );

	Shader_ConnectOtherMatSysInterfaces( );
	return g_pMaterialSystemHardwareConfig && g_pMaterialSystemDebugTextureInfo;
}


//-----------------------------------------------------------------------------
// Connect to interfaces we need
//-----------------------------------------------------------------------------
void Shader_Disconnect( void )
{
}



#ifndef SWDS

void Shader_SwapBuffers( void )
{
	assert( materials );
	materials->SwapBuffers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Shader_BeginRendering ( void )
{
	// FIXME:
//	vid.width  = window_rect.width;
//	vid.height = window_rect.height;
}
#endif
