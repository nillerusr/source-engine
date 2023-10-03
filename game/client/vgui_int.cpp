//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "itextmessage.h"
#include "vguicenterprint.h"
#include "iloadingdisc.h"
#include "ifpspanel.h"
#include "imessagechars.h"
#include "inetgraphpanel.h"
#include "idebugoverlaypanel.h"
#include <vgui/isurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "tier0/vprof.h"
#include "iclientmode.h"
#include <vgui_controls/Panel.h>
#include <KeyValues.h>
#include "FileSystem.h"
#include "matsys_controls/matsyscontrols.h"

using namespace vgui;

void MP3Player_Create( vgui::VPANEL parent );
void MP3Player_Destroy();

#include <vgui/IInputInternal.h>
vgui::IInputInternal *g_InputInternal = NULL;

#include <vgui_controls/Controls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


bool IsWidescreen( void );


void ss_verticalsplit_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.GetBool() != !!(int)flOldValue )
	{
		VGui_OnSplitScreenStateChanged();

		if ( GetFullscreenClientMode() )
		{
			// we have to force re-layout, because the screen dimensions haven't changed,
			// but our layout is going to be different.
			GetFullscreenClientMode()->Layout( true );
		}

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( i );
			GetClientMode()->Layout();
			GetHud().OnSplitScreenStateChanged();
		}
	}
}
static ConVar ss_verticalsplit( "ss_verticalsplit", "0", 0, "Two player split screen uses vertical split (do not set this directly, use ss_splitmode instead).", ss_verticalsplit_changed );

void ss_splitmode_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( !IsWidescreen() )
	{
		// Non-widescreen is alway horizontal
		ss_verticalsplit.SetValue( 0 );
	}
	else
	{
		if ( var.GetInt() == 1 )
		{
			// Horizontal
			ss_verticalsplit.SetValue( 0 );
		}
		else if ( var.GetInt() == 2 )
		{
			// Vertical
			ss_verticalsplit.SetValue( 1 );
		}
		else
		{
			// Vertical is default for widescreen
			ss_verticalsplit.SetValue( 1 );
		}
	}
}
static ConVar ss_splitmode( "ss_splitmode", "0", FCVAR_ARCHIVE, "Two player split screen mode (0 - recommended settings base on the width, 1 - horizontal, 2 - vertical (only allowed in widescreen)", ss_splitmode_changed );
static ConVar ss_enable( "ss_enable", "0", FCVAR_RELEASE, "Enables Split Screen support. Play Single Player now launches into split screen mode. NO ONLINE SUPPORT" );


void GetVGUICursorPos( int& x, int& y )
{
	vgui::input()->GetCursorPos(x, y);
}

void SetVGUICursorPos( int x, int y )
{
	if ( !g_bTextMode )
	{
		vgui::input()->SetCursorPos(x, y);
	}
}

class CHudTextureHandleProperty : public vgui::IPanelAnimationPropertyConverter
{
public:
	virtual void GetData( Panel *panel, KeyValues *kv, PanelAnimationMapEntry *entry )
	{
		void *data = ( void * )( (*entry->m_pfnLookup)( panel ) );
		CHudTextureHandle *pHandle = ( CHudTextureHandle * )data;

		// lookup texture name for id
		if ( pHandle->Get() )
		{
			kv->SetString( entry->name(), pHandle->Get()->szShortName );
		}
		else
		{
			kv->SetString( entry->name(), "" );
		}
	}
	
	virtual void SetData( Panel *panel, KeyValues *kv, PanelAnimationMapEntry *entry )
	{
		void *data = ( void * )( (*entry->m_pfnLookup)( panel ) );
		
		CHudTextureHandle *pHandle = ( CHudTextureHandle * )data;

		const char *texturename = kv->GetString( entry->name() );
		if ( texturename && texturename[ 0 ] )
		{
			CHudTexture *currentTexture = HudIcons().GetIcon( texturename );
			pHandle->Set( currentTexture );
		}
		else
		{
			pHandle->Set( NULL );
		}
	}

	virtual void InitFromDefault( Panel *panel, PanelAnimationMapEntry *entry )
	{
		void *data = ( void * )( (*entry->m_pfnLookup)( panel ) );

		CHudTextureHandle *pHandle = ( CHudTextureHandle * )data;

		const char *texturename = entry->defaultvalue();
		if ( texturename && texturename[ 0 ] )
		{
			CHudTexture *currentTexture = HudIcons().GetIcon( texturename );
			pHandle->Set( currentTexture );
		}
		else
		{
			pHandle->Set( NULL );
		}
	}
};

class CSplitScreenLetterBox
{
public:

	enum
	{
		SPLITSCREEN_NONWIDESCREEN_HORIZONTAL_SPLIT = 0,
		SPLITSCREEN_WIDESCREEN_HORIZONTAL_SPLIT,
		SPLITSCREEN_WIDESCREEN_VERTICAL_SPLIT,

		NUM_SPLITSCREEN_TYPES,
	};

	void Init();

	void SetNumSplitScreenPlayers( int nPlayers );

	bool GetSettings( bool *pbInsetHud, float *pflAspect, float *pFOV, float *pViewmodelFOV );

private:

	struct LetterBox_t
	{
		LetterBox_t() : m_flAspectRatio( 4.0f / 3.0f ), m_bInsetHud( false ) {}
		float	m_flAspectRatio;
		bool	m_bInsetHud;
		float	m_flFOV;
		float 	m_flViewModelFOV;
	};

	bool		m_bValid;
	LetterBox_t	m_Settings[ NUM_SPLITSCREEN_TYPES ];
	int			m_nSplitScreenPlayers;
};

void CSplitScreenLetterBox::Init()
{
	m_nSplitScreenPlayers = 1;
	char const *pchSlotNames[] = { "nonwidescreen", "widescreen_horizontal_split", "widescreen_vertical_split" };
	char const *pchConfigFile = "splitscreen_config.txt";

	m_bValid = true;
	KeyValues *kv = new KeyValues( "splitscreen" );
	if ( kv->LoadFromFile( g_pFullFileSystem, pchConfigFile, "MOD" ) )
	{
		for ( int i = 0; i < NUM_SPLITSCREEN_TYPES && m_bValid; ++i )
		{
			KeyValues *settings = kv->FindKey( pchSlotNames[ i ], false );
			if ( settings )
			{
				// Get settings
				char const *pchAspect = settings->GetString( "aspect", "4 by 3" );
				if ( pchAspect )
				{
					// Allowable syntax is "16 by 9" or "16 x 9" or "1.77"
					if ( Q_stristr( pchAspect, " by " ) )
					{
						float f1, f2;
						if ( 2 == sscanf( pchAspect, "%f by %f", &f1, &f2 ) && f2 > 0.001f )
						{
							m_Settings[ i ].m_flAspectRatio = f1 / f2;
						}
						else
						{
							Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
							m_bValid = false;
						}
					}
					else if ( Q_stristr( pchAspect, " x " ) )
					{
						float f1, f2;
						if ( 2 == sscanf( pchAspect, "%f x %f", &f1, &f2 ) && f2 > 0.001f )
						{
							m_Settings[ i ].m_flAspectRatio = f1 / f2;
						}
						else
						{
							Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
							m_bValid = false;
						}
					}
					else if ( Q_atof( pchAspect ) > 0.1f )
					{
						m_Settings[ i ].m_flAspectRatio = Q_atof( pchAspect );
					}
					else
					{
						Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
						m_bValid = false;
					}
				}

				// Get inset for hud
				m_Settings[ i ].m_bInsetHud = settings->GetBool( "insethud", false );

				// Get FOV
				m_Settings[ i ].m_flFOV = settings->GetFloat( "fov", 90.0f );

				// Get viewmodel FOVs
				m_Settings[ i ].m_flViewModelFOV = settings->GetFloat( "viewmodelfov", 50.0f );
			}
			else
			{
				Error( "%s:  Missing settings block for split screen mode '%s'\n", pchConfigFile, pchSlotNames[ i ] );
				m_bValid = false;
				break;
			}
		}
	}
	else
	{
		Msg( "No split screen config file '%s', using defaults\n", pchConfigFile );
		m_bValid = false;
	}
	kv->deleteThis();
}

void CSplitScreenLetterBox::SetNumSplitScreenPlayers( int nPlayers )
{
	m_nSplitScreenPlayers = nPlayers;
}

bool IsWidescreen( void )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int screenWidth, screenHeight;
	g_pMaterialSystem->GetBackBufferDimensions( screenWidth, screenHeight );
	float aspectRatio = ( float )screenWidth / ( float )screenHeight;
	// Has to be nearly 16:10 or higher to be considered widescreen.  We do this elsewhere in the code.
	bool bIsWidescreen = ( aspectRatio >= 1.59f ); 
	return bIsWidescreen;
}

bool CSplitScreenLetterBox::GetSettings( bool *pbInsetHud, float *pflAspect, float *pFOV, float *pViewModelFOV )
{
	Assert( pbInsetHud );
	Assert( pflAspect );
	Assert( pFOV );
	Assert( pViewModelFOV );
	static bool bUsedDefaultsLastTime = false;
	if ( !m_bValid || m_nSplitScreenPlayers == 1 )
	{
		if ( !bUsedDefaultsLastTime )
		{
			bUsedDefaultsLastTime = true;
		}
		*pbInsetHud = false;
		*pflAspect = 4.0f / 3.0f;
		// FIXME: These are the non-splitscreen defaults for L4D.  This code needs to be sanitized for other games.
		*pFOV = 90.0f;
		*pViewModelFOV = 50.0f;
		return false;
	}

	// Figure out which splitscreen mode to use based on current configuration.
	int slot;
	if ( IsWidescreen() )
	{
		if ( ss_verticalsplit.GetBool() )
		{
			slot = SPLITSCREEN_WIDESCREEN_VERTICAL_SPLIT;
		}
		else
		{
			slot = SPLITSCREEN_WIDESCREEN_HORIZONTAL_SPLIT;
		}
	}
	else
	{
		slot = SPLITSCREEN_NONWIDESCREEN_HORIZONTAL_SPLIT;
	}

	bUsedDefaultsLastTime = false;

	const LetterBox_t &lb = m_Settings[ slot ];

	*pbInsetHud = lb.m_bInsetHud;
	*pflAspect = lb.m_flAspectRatio;
	*pFOV = lb.m_flFOV;
	*pViewModelFOV = lb.m_flViewModelFOV;
	return true;
}

static CSplitScreenLetterBox g_LetterBox;

#ifdef _X360
CON_COMMAND( ss_reloadletterbox, "ss_reloadletterbox" )
{
	g_LetterBox.Init();
	VGui_OnSplitScreenStateChanged();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( i );
		GetClientMode()->Layout();
		GetHud().OnSplitScreenStateChanged();
	}
}
#endif

static CHudTextureHandleProperty textureHandleConverter;

static void VGui_OneTimeInit()
{
	static bool initialized = false;
	if ( initialized )
		return;
	initialized = true;

	vgui::Panel::AddPropertyConverter( "CHudTextureHandle", &textureHandleConverter );

	g_LetterBox.Init();
}

bool VGui_Startup( CreateInterfaceFn appSystemFactory )
{
	if ( !vgui::VGui_InitInterfacesList( "CLIENT", &appSystemFactory, 1 ) )
		return false;

	if ( !vgui::VGui_InitMatSysInterfacesList( "CLIENT", &appSystemFactory, 1 ) )
		return false;

	g_InputInternal = (IInputInternal *)appSystemFactory( VGUI_INPUTINTERNAL_INTERFACE_VERSION,  NULL );
	if ( !g_InputInternal )
	{
		return false; // c_vguiscreen.cpp needs this!
	}

	VGui_OneTimeInit();

	// Create any root panels for .dll
	VGUI_CreateClientDLLRootPanel();

	// Make sure we have a panel
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		VPANEL root = VGui_GetClientDLLRootPanel();
		if ( !root )
		{
			return false;
		}
	}

	CUtlVector< Panel * > list;
	VGui_GetPanelList( list );

	for ( int i = 0; i < list.Count(); ++i )
	{
		list[ i ]->SetMessageContextId_R( (uint32)i );
	}

	VGui_GetFullscreenRootPanel()->SetMessageContextId_R( (uint32)0 );

	VGui_OnSplitScreenStateChanged();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_CreateGlobalPanels( void )
{
	VPANEL gameToolParent = enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS );
	VPANEL toolParent = enginevgui->GetPanel( PANEL_TOOLS );
#if defined( TRACK_BLOCKING_IO )
	VPANEL gameDLLPanel = enginevgui->GetPanel( PANEL_GAMEDLL );
#endif
	// Part of game
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		VPANEL root = VGui_GetClientDLLRootPanel();
		GetCenterPrint()->Create( root );
	}
	loadingdisc->Create( gameToolParent );
	messagechars->Create( gameToolParent );

	// Debugging or related tool
	fps->Create( toolParent );
#if defined( TRACK_BLOCKING_IO )
	iopanel->Create( gameDLLPanel );
#endif
	netgraphpanel->Create( toolParent );
	debugoverlaypanel->Create( gameToolParent );

#ifndef _X360
	// Create mp3 player off of tool parent panel
	MP3Player_Create( toolParent );
#endif
}

void VGui_Shutdown()
{
#ifndef _X360
	MP3Player_Destroy();
#endif

	netgraphpanel->Destroy();
	debugoverlaypanel->Destroy();
#if defined( TRACK_BLOCKING_IO )
	iopanel->Destroy();
#endif
	fps->Destroy();

	messagechars->Destroy();
	loadingdisc->Destroy();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetCenterPrint()->Destroy();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		if ( GetClientMode() )
		{
			GetClientMode()->VGui_Shutdown();

			if ( hh == 0 )
			{
				GetFullscreenClientMode()->VGui_Shutdown();
			}
		}
	}

	VGUI_DestroyClientDLLRootPanel();

	// Make sure anything "marked for deletion"
	//  actually gets deleted before this dll goes away
	vgui::ivgui()->RunFrame();
}

static ConVar cl_showpausedimage( "cl_showpausedimage", "1", 0, "Show the 'Paused' image when game is paused." );

//-----------------------------------------------------------------------------
// Things to do before rendering vgui stuff...
//-----------------------------------------------------------------------------
void VGui_PreRender()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	VPROF( "VGui_PreRender" );

	// 360 does not use these plaques
	if ( IsPC() )
	{
		loadingdisc->SetLoadingVisible( engine->IsDrawingLoadingImage() && !engine->IsPlayingDemo() );
		loadingdisc->SetPausedVisible( !enginevgui->IsGameUIVisible() && cl_showpausedimage.GetBool() && engine->IsPaused() && !engine->IsTakingScreenshot() && !engine->IsPlayingDemo() );
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CUtlVector< Panel * > list;
	VGui_GetPanelList( list );
	for ( int i = 0; i < list.Count() ; ++i )
	{
		list[ i ]->SetVisible( i == nSlot );
	}

	VGui_GetFullscreenRootPanel()->SetVisible( true );
}

void VGui_PostRender()
{
	int w, h;
	CUtlVector< Panel * > list;
	VGui_GetPanelList( list );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		int x, y;
		VGui_GetHudBounds( i, x, y, w, h);
		list[ i ]->SetVisible( true );
		list[ i ]->SetBounds( x, y, w, h );

		surface()->SetAbsPosForContext( i, x, y );
	}

	VGui_GetTrueScreenSize( w, h );
	VGui_GetFullscreenRootPanel()->SetVisible( true );
	VGui_GetFullscreenRootPanel()->SetBounds( 0, 0, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cl_panelanimation - 
//-----------------------------------------------------------------------------
CON_COMMAND( cl_panelanimation, "Shows panel animation variables: <panelname | blank for all panels>." )
{
	if ( args.ArgC() == 2 )
	{
		PanelAnimationDumpVars( args[1] );
	}
	else
	{
		PanelAnimationDumpVars( NULL );
	}
}

void GetHudSize( int& w, int &h )
{
	vgui::surface()->GetScreenSize( w, h );
}

static vrect_t g_TrueScreenSize;
static vrect_t g_ScreenSpaceBounds[ MAX_SPLITSCREEN_CLIENTS ];

void VGui_GetTrueScreenSize( int &w, int &h )
{
	w = g_TrueScreenSize.width;
	h = g_TrueScreenSize.height;
}

void VGUI_SetScreenSpaceBounds( int slot, int x, int y, int w, int h )
{
	vrect_t &r = g_ScreenSpaceBounds[ slot ];
	r.x = x;
	r.y = y;
	r.width = w;
	r.height = h;
}

void VGUI_UpdateScreenSpaceBounds( int nNumSplits, int sx, int sy, int sw, int sh )
{
	g_TrueScreenSize.x = sx;
	g_TrueScreenSize.y = sy;
	g_TrueScreenSize.width = sw;
	g_TrueScreenSize.height = sh;

	CUtlVector< int > validSlots;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		validSlots.AddToTail( i );
	}

	Assert( validSlots.Count() == nNumSplits );

	switch ( nNumSplits )
	{
	default:
	case 1:
		// Make it screen sized
		{
			VGUI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
		}
		break;
	case 2:
		{
			if ( ss_verticalsplit.GetBool() )
			{
				sw /= 2;
				// Stack two horiz, side by side
				VGUI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
				VGUI_SetScreenSpaceBounds( validSlots[ 1 ], sx + sw, sy, sw, sh );
			}
			else
			{
				sh /= 2;
				// Stack two wide on top of one another
				VGUI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
				VGUI_SetScreenSpaceBounds( validSlots[ 1 ], sx, sy + sh, sw, sh );
			}
		}
		break;
	case 3:
		{
			int fullw = sw;

			sw /= 2;
			sh /= 2;

			VGUI_SetScreenSpaceBounds( validSlots[ 0 ], sx + ( fullw - sw ) / 2, sy, sw, sh );
			VGUI_SetScreenSpaceBounds( validSlots[ 1 ], sx, sy + sh, sw, sh );
			VGUI_SetScreenSpaceBounds( validSlots[ 2 ], sx + sw, sy + sh, sw, sh );
		}
		break;
	case 4:
		{
			sw /= 2;
			sh /= 2;

			// Stack two wide on top of one another
			VGUI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
			VGUI_SetScreenSpaceBounds( validSlots[ 1 ], sx + sw, sy, sw, sh );
			VGUI_SetScreenSpaceBounds( validSlots[ 2 ], sx, sy + sh, sw, sh );
			VGUI_SetScreenSpaceBounds( validSlots[ 3 ], sx + sw, sy + sh, sw, sh );
		}
		break;
	}
}

CBitVec< MAX_SPLITSCREEN_PLAYERS > g_SplitScreenPlayers;

bool g_bIterateRemoteSplitScreenPlayers = false;
C_BasePlayer *g_RemoteSplitScreenPlayers[MAX_SPLITSCREEN_PLAYERS];

void AddRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( g_RemoteSplitScreenPlayers[i] == pPlayer )
			return; //don't add it twice
	}

	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( !g_SplitScreenPlayers.IsBitSet( i ) && (g_RemoteSplitScreenPlayers[i] == NULL) )
		{
			g_RemoteSplitScreenPlayers[i] = pPlayer;
			VGui_OnSplitScreenStateChanged();
			return;
		}
	}
}

void RemoveRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( g_RemoteSplitScreenPlayers[i] == pPlayer )
		{
			g_RemoteSplitScreenPlayers[i] = NULL;
			VGui_OnSplitScreenStateChanged();
			return;
		}
	}
}

C_BasePlayer *GetSplitScreenViewPlayer( int nSlot )
{
	return g_SplitScreenPlayers.IsBitSet( nSlot ) ? C_BasePlayer::GetLocalPlayer( nSlot ) : g_RemoteSplitScreenPlayers[nSlot];
}

void cl_enable_remote_splitscreen_callback_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	VGui_OnSplitScreenStateChanged();
}

ConVar cl_enable_remote_splitscreen( "cl_enable_remote_splitscreen", "0", 0, "Allows viewing of nonlocal players in a split screen fashion", cl_enable_remote_splitscreen_callback_f );
static CUtlVector<bool> s_IterateNetworkedSplitScreenSlotsPushedValues;
void IterateRemoteSplitScreenViewSlots_Push( bool bSet )
{
	if( !cl_enable_remote_splitscreen.GetBool() )
	{
		bSet = false;
	}

	s_IterateNetworkedSplitScreenSlotsPushedValues.AddToTail( g_bIterateRemoteSplitScreenPlayers );
	g_bIterateRemoteSplitScreenPlayers = bSet;
}

void IterateRemoteSplitScreenViewSlots_Pop( void )
{
	Assert( s_IterateNetworkedSplitScreenSlotsPushedValues.Count() > 0 );
	g_bIterateRemoteSplitScreenPlayers = s_IterateNetworkedSplitScreenSlotsPushedValues.Tail();
	s_IterateNetworkedSplitScreenSlotsPushedValues.RemoveMultipleFromTail( 1 );
}

bool IsLocalSplitScreenPlayer( int nSlot )
{
	return g_SplitScreenPlayers.IsBitSet( nSlot );
}

int FirstValidSplitScreenSlot()
{
	return 0;
}

int NextValidSplitScreenSlot( int i )
{
	++i;
	while ( i<  MAX_SPLITSCREEN_PLAYERS )
	{
		if ( g_SplitScreenPlayers.IsBitSet( i ) )
			return i;

		if( g_bIterateRemoteSplitScreenPlayers && cl_enable_remote_splitscreen.GetBool() && (g_RemoteSplitScreenPlayers[i] != NULL) )
			return i;

		++i;
	}
	return -1;
}

bool IsValidSplitScreenSlot( int i )
{
	return g_SplitScreenPlayers.IsBitSet( i ) || (g_bIterateRemoteSplitScreenPlayers && (g_RemoteSplitScreenPlayers[i] != NULL));
}

static int g_nCachedScreenSize[ 2 ] = { -1, -1 };

void VGui_OnScreenSizeChanged()
{
	vgui::surface()->GetScreenSize( g_nCachedScreenSize[ 0 ], g_nCachedScreenSize[ 1 ] );

	VGui_OnSplitScreenStateChanged();
}

static int g_nNumSplits = 1;


bool VGui_IsSplitScreen()
{
	return g_nNumSplits >= 2;
}

void VGui_OnSplitScreenStateChanged()
{
	CUtlVector< Panel * > list;
	VGui_GetPanelList( list );

	g_SplitScreenPlayers.ClearAll();
	g_nNumSplits = 0;
	for ( int i = engine->FirstValidSplitScreenSlot();				
		i != -1;												
		i = engine->NextValidSplitScreenSlot( i ) )	
	{
		g_SplitScreenPlayers.Set( i );
		g_RemoteSplitScreenPlayers[i] = NULL; //actual splitscreen players nuke networked splitscreen players
		++g_nNumSplits;
	}

	if( cl_enable_remote_splitscreen.GetBool() )
	{
		for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			if( g_RemoteSplitScreenPlayers[i] != NULL )
			{
				++g_nNumSplits;
			}
		}
	}

	IterateRemoteSplitScreenViewSlots_Push( true );
	g_LetterBox.SetNumSplitScreenPlayers( g_nNumSplits );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		list[ i ]->SetVisible( IsValidSplitScreenSlot( i ) );
	}

	// Now tile, etc. the rest of them
	int sw, sh;
	if ( g_nCachedScreenSize[ 0 ] == -1 )
	{
		vgui::surface()->GetScreenSize( g_nCachedScreenSize[ 0 ], g_nCachedScreenSize[ 1 ] );
	}

	sw = g_nCachedScreenSize[ 0 ];
	sh = g_nCachedScreenSize[ 1 ];

	VGUI_UpdateScreenSpaceBounds( g_nNumSplits, 0, 0, sw, sh );

	// get the current splitscreen/letterbox settings.  We only care about fov and viewmodelfov.
	bool bDummy;
	float flDummy, flFOV, flViewModelFOV;
	g_LetterBox.GetSettings( &bDummy, &flDummy, &flFOV, &flViewModelFOV );


	static SplitScreenConVarRef fov_desired( "fov_desired" );

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( fov_desired.IsValid() )
		{
			fov_desired.SetValue( i, flFOV );
		}

		// The actual viewport panels are all at the top left of the screen, but sized appropriately
		int x, y, w, h;
		VGui_GetHudBounds( i, x, y, w, h);
		list[ i ]->SetBounds( x, y, w, h );

		surface()->SetAbsPosForContext( i, x, y );
	}

	ConfigureCurrentSystemLevel( );
	IterateRemoteSplitScreenViewSlots_Pop();
}

void VGui_GetPanelBounds( int slot, int &x, int &y, int &w, int &h )
{
	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		vgui::surface()->GetScreenSize( w, h );
		return;
	}

	vrect_t &r = g_ScreenSpaceBounds[ slot ];
	x = r.x;
	y = r.y;
	w = r.width;
	h = r.height;
}

void VGui_GetEngineRenderBounds( int slot, int &x, int &y, int &w, int &h, int &insetX, int &insetY )
{
	insetX = insetY = 0;

	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		vgui::surface()->GetScreenSize( w, h );
		return;
	}

	VGui_GetPanelBounds( slot, x, y, w, h );

	bool bDummy;
	float flDummy;
	float flAspect = 1.0f;
	if ( !g_LetterBox.GetSettings( &bDummy, &flAspect, &flDummy, &flDummy )  )
	{
		return;
	}

	// Figure out current aspect ratio
	float flCurrentAspect = (float)w / (float)h;
	float ratio = flAspect / flCurrentAspect;

	if ( ratio > 1.0f )
	{
		// Screen is wider, need bars at top and bottom
		int usetall = (float)w / flAspect;
		if ( IsPC() )
		{
			insetY = ( h - usetall ) / 2;
			y += insetY;
			h = usetall;
		}
		else
		{
			// hopefully it centers, but it might not
			usetall = AlignValue( usetall, 2 * GPU_RESOLVE_ALIGNMENT );
			insetY = ( h - usetall ) / 2;
			y += insetY;
			y = AlignValue( y, GPU_RESOLVE_ALIGNMENT );
			insetY = AlignValue( insetY, GPU_RESOLVE_ALIGNMENT );
			h = usetall;
		}
	}
	else
	{
		// Screen is narrower, need bars at left/right
		int usewide = (float)h * flAspect;
		if ( IsPC() )
		{
			insetX = ( w - usewide  ) / 2;
			x += insetX;
			w = usewide;
		}
		else
		{
			// hopefully it centers, but it might not
			usewide = AlignValue( usewide, 2 * GPU_RESOLVE_ALIGNMENT );
			insetX = ( w - usewide  ) / 2;
			x += insetX;
			x = AlignValue( x, GPU_RESOLVE_ALIGNMENT );
			insetX = AlignValue( insetX, GPU_RESOLVE_ALIGNMENT );
			w = usewide;
		}
	}
}

void VGui_GetHudBounds( int slot, int &x, int &y, int &w, int &h )
{
	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		vgui::surface()->GetScreenSize( w, h );
		return;
	}

	bool bInset = false;
	float dummy = 1.0f;

	if ( !g_LetterBox.GetSettings( &bInset, &dummy, &dummy, &dummy ) || 
		 !bInset )
	{
		// Use entire bounds for HUD
		VGui_GetPanelBounds( slot, x, y, w, h );
		return;
	}

	int insetX, insetY;
	VGui_GetEngineRenderBounds( slot, x, y, w, h, insetX, insetY );
}

int VGUI_FindSlotForRootPanel( vgui::Panel *pRoot )
{
	CUtlVector< Panel * > list;
	VGui_GetPanelList( list );
	int slot =  list.Find( pRoot ) ;
	if ( slot == list.InvalidIndex() )
		return 0;
	return slot;
}