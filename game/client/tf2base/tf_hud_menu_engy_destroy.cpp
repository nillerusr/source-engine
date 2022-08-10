//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include "c_baseobject.h"

#include "tf_hud_menu_engy_destroy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//======================================

DECLARE_BUILD_FACTORY( CEngyDestroyMenuItem );


//======================================

DECLARE_HUDELEMENT_DEPTH( CHudMenuEngyDestroy, 40 );	// in front of engy building status

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMenuEngyDestroy::CHudMenuEngyDestroy( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudMenuEngyDestroy" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	for ( int i=0; i<4; i++ )
	{
		char buf[32];

		Q_snprintf( buf, sizeof(buf), "active_item_%d", i+1 );
		m_pActiveItems[i] = new CEngyDestroyMenuItem( this, buf );

		Q_snprintf( buf, sizeof(buf), "inactive_item_%d", i+1 );
		m_pInactiveItems[i] = new CEngyDestroyMenuItem( this, buf );
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	RegisterForRenderGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: called whenever a new level's starting
//-----------------------------------------------------------------------------
void CHudMenuEngyDestroy::LevelInit( void )
{
	//RecalculateBuildingItemState( ALL_BUILDINGS );

	CHudElement::LevelInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenuEngyDestroy::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/destroy_menu/HudMenuEngyDestroy.res" );

	m_pActiveItems[0]->LoadControlSettings( "resource/UI/destroy_menu/sentry_active.res" );
	m_pActiveItems[1]->LoadControlSettings( "resource/UI/destroy_menu/dispenser_active.res" );
	m_pActiveItems[2]->LoadControlSettings( "resource/UI/destroy_menu/tele_entrance_active.res" );
	m_pActiveItems[3]->LoadControlSettings( "resource/UI/destroy_menu/tele_exit_active.res" );

	m_pInactiveItems[0]->LoadControlSettings( "resource/UI/destroy_menu/sentry_inactive.res" );
	m_pInactiveItems[1]->LoadControlSettings( "resource/UI/destroy_menu/dispenser_inactive.res" );
	m_pInactiveItems[2]->LoadControlSettings( "resource/UI/destroy_menu/tele_entrance_inactive.res" );
	m_pInactiveItems[3]->LoadControlSettings( "resource/UI/destroy_menu/tele_exit_inactive.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMenuEngyDestroy::ShouldDraw( void )
{
	CTFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return false;

	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();

	if ( !pWpn )
		return false;

	// Don't show the menu for first person spectator
	if ( pPlayer != pWpn->GetOwner() )
		return false;

	if ( !CHudElement::ShouldDraw() )
		return false;

	return ( pWpn->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_DESTROY );
}

//-----------------------------------------------------------------------------
// Purpose: Keyboard input hook. Return 0 if handled
//-----------------------------------------------------------------------------
int	CHudMenuEngyDestroy::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( !ShouldDraw() )
	{
		return 1;
	}

	if ( !down )
	{
		return 1;
	}

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return 1;

	bool bHandled = false;

	int iSlot = -1;

	switch( keynum )
	{
	case KEY_1:
	case KEY_XBUTTON_UP:
		iSlot = 0;
		bHandled = true;
		break;
	case KEY_2:
	case KEY_XBUTTON_RIGHT:
		iSlot = 1;
		bHandled = true;
		break;
	case KEY_3:
	case KEY_XBUTTON_DOWN:
		iSlot = 2;
		bHandled = true;
		break;
	case KEY_4:
	case KEY_XBUTTON_LEFT:
		iSlot = 3;
		bHandled = true;
		break;

	case KEY_5:
	case KEY_6:
	case KEY_7:
	case KEY_8:
	case KEY_9:
		// Eat these keys
		bHandled = true;
		break;

	case KEY_0:
	case KEY_XBUTTON_B:
		engine->ExecuteClientCmd( "lastinv" );
		bHandled = true;
		break;

	default:
		break;
	}

	if ( iSlot >= 0 )
	{
		int iBuildingID = MapIndexToObjectID( iSlot );

		if ( pLocalPlayer->GetObjectOfType( iBuildingID ) != NULL )
		{
			char szCmd[128];
			Q_snprintf( szCmd, sizeof(szCmd), "destroy %d; lastinv", iBuildingID );
			engine->ExecuteClientCmd( szCmd );
		}
		else
		{
			ErrorSound();
		}
	}

	// return 0 if we ate the key
	return ( bHandled == false );
}

void CHudMenuEngyDestroy::ErrorSound( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer )
	{
		pLocalPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

int CHudMenuEngyDestroy::MapIndexToObjectID( int index )
{
	static int iRemapIndexToObjectID[4] = 
	{
		OBJ_SENTRYGUN,
		OBJ_DISPENSER,
		OBJ_TELEPORTER_ENTRANCE,
		OBJ_TELEPORTER_EXIT
	};

	Assert( index >= 0 && index <= 3 );

	if ( index >= 0 && index <= 3 )
	{
		return iRemapIndexToObjectID[index];
	}
	else
	{
		Assert( !"Bad param to CHudMenuEngyBuild::MMapIndexToObjectID" );
		return OBJ_LAST;
	}
}

void CHudMenuEngyDestroy::OnTick( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	
	int i;

	for ( i=0;i<4; i++ )
	{
		int iRemappedObjectID = MapIndexToObjectID( i );

		// update this slot
		C_BaseObject *pObj = NULL;

		if ( pLocalPlayer )
		{
			pObj = pLocalPlayer->GetObjectOfType( iRemappedObjectID );
		}			

		m_pActiveItems[i]->SetVisible( false );
		m_pInactiveItems[i]->SetVisible( false );

		// If the building is built, we can destroy it
		if ( pObj != NULL && !pObj->IsPlacing() )
		{
			m_pActiveItems[i]->SetVisible( true );
		}
		else
		{
			m_pInactiveItems[i]->SetVisible( true );
		}
	}
}

void CHudMenuEngyDestroy::SetVisible( bool state )
{
	if ( state == IsVisible() )
		return;

	if ( state == true )
	{
		// set the %lastinv% dialog var to our binding
		const char *key = engine->Key_LookupBinding( "lastinv" );
		if ( !key )
		{
			key = "< not bound >";
		}

		SetDialogVariable( "lastinv", key );

		//RecalculateBuildingState( ALL_BUILDINGS );

		HideLowerPriorityHudElementsInGroup( "mid" );
	}
	else
	{
		UnhideLowerPriorityHudElementsInGroup( "mid" );
	}

	BaseClass::SetVisible( state );
}