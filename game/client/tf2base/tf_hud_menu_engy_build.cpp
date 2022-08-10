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

#include "tf_hud_menu_engy_build.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Set to 1 to simulate xbox-style menu interaction
ConVar tf_build_menu_controller_mode( "tf_build_menu_controller_mode", "0", FCVAR_ARCHIVE, "Use console controller build menus. 1 = ON, 0 = OFF." );

//======================================

DECLARE_HUDELEMENT_DEPTH( CHudMenuEngyBuild, 40 );	// in front of engy building status

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMenuEngyBuild::CHudMenuEngyBuild( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudMenuEngyBuild" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	for ( int i=0; i<4; i++ )
	{
		char buf[32];

		Q_snprintf( buf, sizeof(buf), "active_item_%d", i+1 );
		m_pAvailableObjects[i] = new EditablePanel( this, buf );

		Q_snprintf( buf, sizeof(buf), "already_built_item_%d", i+1 );
		m_pAlreadyBuiltObjects[i] = new EditablePanel( this, buf );

		Q_snprintf( buf, sizeof(buf), "cant_afford_item_%d", i+1 );
		m_pCantAffordObjects[i] = new EditablePanel( this, buf );
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_pActiveSelection = NULL;

	m_iSelectedItem = -1;

	m_pBuildLabelBright = NULL;
	m_pBuildLabelDim = NULL;

	m_pDestroyLabelBright = NULL;
	m_pDestroyLabelDim = NULL;

	m_bInConsoleMode = false;

	RegisterForRenderGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenuEngyBuild::ApplySchemeSettings( IScheme *pScheme )
{
	bool b360Style = ( IsConsole() || tf_build_menu_controller_mode.GetBool() );

	// load control settings...

	if ( b360Style )
	{
		LoadControlSettings( "resource/UI/build_menu_360/HudMenuEngyBuild.res" );

		// Load the already built images, destroyable
		m_pAlreadyBuiltObjects[0]->LoadControlSettings( "resource/UI/build_menu_360/sentry_already_built.res" );
		m_pAlreadyBuiltObjects[1]->LoadControlSettings( "resource/UI/build_menu_360/dispenser_already_built.res" );
		m_pAlreadyBuiltObjects[2]->LoadControlSettings( "resource/UI/build_menu_360/tele_entrance_already_built.res" );
		m_pAlreadyBuiltObjects[3]->LoadControlSettings( "resource/UI/build_menu_360/tele_exit_already_built.res" );

		m_pAvailableObjects[0]->LoadControlSettings( "resource/UI/build_menu_360/sentry_active.res" );
		m_pAvailableObjects[1]->LoadControlSettings( "resource/UI/build_menu_360/dispenser_active.res" );
		m_pAvailableObjects[2]->LoadControlSettings( "resource/UI/build_menu_360/tele_entrance_active.res" );
		m_pAvailableObjects[3]->LoadControlSettings( "resource/UI/build_menu_360/tele_exit_active.res" );

		m_pCantAffordObjects[0]->LoadControlSettings( "resource/UI/build_menu_360/sentry_cant_afford.res" );
		m_pCantAffordObjects[1]->LoadControlSettings( "resource/UI/build_menu_360/dispenser_cant_afford.res" );
		m_pCantAffordObjects[2]->LoadControlSettings( "resource/UI/build_menu_360/tele_entrance_cant_afford.res" );
		m_pCantAffordObjects[3]->LoadControlSettings( "resource/UI/build_menu_360/tele_exit_cant_afford.res" );

		m_pActiveSelection = dynamic_cast< CIconPanel * >( FindChildByName( "active_selection_bg" ) );

		m_pBuildLabelBright = dynamic_cast< CTFLabel * >( FindChildByName( "BuildHintLabel_Bright" ) );
		m_pBuildLabelDim = dynamic_cast< CTFLabel * >( FindChildByName( "BuildHintLabel_Dim" ) );
	
		m_pDestroyLabelBright = dynamic_cast< CTFLabel * >( FindChildByName( "DestroyHintLabel_Bright" ) );
		m_pDestroyLabelDim = dynamic_cast< CTFLabel * >( FindChildByName( "DestroyHintLabel_Dim" ) );

		// Reposition the activeselection to the default position
		m_iSelectedItem = -1;	// force reposition
		SetSelectedItem( 1 );
	}
	else
	{
		LoadControlSettings( "resource/UI/build_menu/HudMenuEngyBuild.res" );

		// Load the already built images, not destroyable
		m_pAlreadyBuiltObjects[0]->LoadControlSettings( "resource/UI/build_menu/sentry_already_built.res" );
		m_pAlreadyBuiltObjects[1]->LoadControlSettings( "resource/UI/build_menu/dispenser_already_built.res" );
		m_pAlreadyBuiltObjects[2]->LoadControlSettings( "resource/UI/build_menu/tele_entrance_already_built.res" );
		m_pAlreadyBuiltObjects[3]->LoadControlSettings( "resource/UI/build_menu/tele_exit_already_built.res" );

		m_pAvailableObjects[0]->LoadControlSettings( "resource/UI/build_menu/sentry_active.res" );
		m_pAvailableObjects[1]->LoadControlSettings( "resource/UI/build_menu/dispenser_active.res" );
		m_pAvailableObjects[2]->LoadControlSettings( "resource/UI/build_menu/tele_entrance_active.res" );
		m_pAvailableObjects[3]->LoadControlSettings( "resource/UI/build_menu/tele_exit_active.res" );

		m_pCantAffordObjects[0]->LoadControlSettings( "resource/UI/build_menu/sentry_cant_afford.res" );
		m_pCantAffordObjects[1]->LoadControlSettings( "resource/UI/build_menu/dispenser_cant_afford.res" );
		m_pCantAffordObjects[2]->LoadControlSettings( "resource/UI/build_menu/tele_entrance_cant_afford.res" );
		m_pCantAffordObjects[3]->LoadControlSettings( "resource/UI/build_menu/tele_exit_cant_afford.res" );

		m_pActiveSelection = NULL;

		m_pBuildLabelBright = NULL;
		m_pBuildLabelDim = NULL;

		m_pDestroyLabelBright = NULL;
		m_pDestroyLabelDim = NULL;
	}

	// Set the cost label
	for ( int i=0; i<4; i++ )
	{
		int iCost = GetObjectInfo( GetBuildingIDFromSlot( i+1 ) )->m_Cost;

		m_pAvailableObjects[i]->SetDialogVariable( "metal", iCost );
		m_pAlreadyBuiltObjects[i]->SetDialogVariable( "metal", iCost );
		m_pCantAffordObjects[i]->SetDialogVariable( "metal", iCost );
	}

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMenuEngyBuild::ShouldDraw( void )
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

	return ( pWpn->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_BUILD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudMenuEngyBuild::GetBuildingIDFromSlot( int iSlot )
{
	int iBuilding = OBJ_LAST;
	switch( iSlot )
	{
	case 1:
		iBuilding = OBJ_SENTRYGUN;
		break;
	case 2:
		iBuilding = OBJ_DISPENSER;
		break;
	case 3:
		iBuilding = OBJ_TELEPORTER_ENTRANCE;
		break;
	case 4:
		iBuilding = OBJ_TELEPORTER_EXIT;
		break;

	default:
		Assert( !"What slot are we asking for and why?" );
		break;
	}

	return iBuilding;
}

//-----------------------------------------------------------------------------
// Purpose: Keyboard input hook. Return 0 if handled
//-----------------------------------------------------------------------------
int	CHudMenuEngyBuild::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( !ShouldDraw() )
	{
		return 1;
	}

	if ( !down )
	{
		return 1;
	}

	bool bController = ( IsConsole() || ( keynum >= JOYSTICK_FIRST ) );

	if ( bController )
	{
		int iNewSelection = m_iSelectedItem;

		switch( keynum )
		{
		case KEY_XBUTTON_UP:
			// jump to last
			iNewSelection = 4;
			break;

		case KEY_XBUTTON_DOWN:
			// jump to first
			iNewSelection = 1;
			break;

		case KEY_XBUTTON_RIGHT:
			// move selection to the right
			iNewSelection++;
			if ( iNewSelection > 4 )
				iNewSelection = 1;
			break;

		case KEY_XBUTTON_LEFT:
			// move selection to the left
			iNewSelection--;
			if ( iNewSelection < 1 )
				iNewSelection = 4;
			break;

		case KEY_XBUTTON_A:
		case KEY_XBUTTON_RTRIGGER:
			// build selected item
			SendBuildMessage( m_iSelectedItem );
			return 0;

		case KEY_XBUTTON_Y:
		case KEY_XBUTTON_LTRIGGER:
			{
				// destroy selected item
				bool bSuccess = SendDestroyMessage( m_iSelectedItem );

				if ( bSuccess )
				{
					engine->ExecuteClientCmd( "lastinv" );
				}
			}
			return 0;

		case KEY_XBUTTON_B:
			// cancel, close the menu
			engine->ExecuteClientCmd( "lastinv" );
			return 0;

		default:
			return 1;	// key not handled
		}

		SetSelectedItem( iNewSelection );

		return 0;
	}
	else
	{
		int iSlot = 0;

		switch( keynum )
		{
		case KEY_1:
			iSlot = 1;
			break;
		case KEY_2:
			iSlot = 2;
			break;
		case KEY_3:
			iSlot = 3;
			break;
		case KEY_4:
			iSlot = 4;
			break;

		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			// Eat these keys
			return 0;

		case KEY_0:
		case KEY_XBUTTON_B:
			// cancel, close the menu
			engine->ExecuteClientCmd( "lastinv" );
			return 0;

		default:
			return 1;	// key not handled
		}

		if ( iSlot > 0 )
		{
			SendBuildMessage( iSlot );
			return 0;
		}
	}

	return 1;	// key not handled
}

void CHudMenuEngyBuild::SendBuildMessage( int iSlot )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return;

	int iBuilding = GetBuildingIDFromSlot( iSlot );

	C_BaseObject *pObj = pLocalPlayer->GetObjectOfType( iBuilding );
	int iCost = GetObjectInfo( iBuilding )->m_Cost;

	if ( pObj == NULL && pLocalPlayer->GetAmmoCount( TF_AMMO_METAL ) >= iCost )
	{
		char szCmd[128];
		Q_snprintf( szCmd, sizeof(szCmd), "build %d", iBuilding );
		engine->ClientCmd( szCmd );
	}
	else
	{
		pLocalPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

bool CHudMenuEngyBuild::SendDestroyMessage( int iSlot )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return false;

	bool bSuccess = false;

	int iBuilding = GetBuildingIDFromSlot( iSlot );

	C_BaseObject *pObj = pLocalPlayer->GetObjectOfType( iBuilding );

	if ( pObj != NULL )
	{
		char szCmd[128];
		Q_snprintf( szCmd, sizeof(szCmd), "destroy %d", iBuilding );
		engine->ClientCmd( szCmd );
		bSuccess = true; 
	}
	else
	{
		pLocalPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}

	return bSuccess;
}

void CHudMenuEngyBuild::OnTick( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return;

	int iAccount = pLocalPlayer->GetAmmoCount( TF_AMMO_METAL );

	for ( int i=0;i<4; i++ )
	{
		int iRemappedObjectID = GetBuildingIDFromSlot( i + 1 );

		// update this slot
		C_BaseObject *pObj = NULL;

		if ( pLocalPlayer )
		{
			pObj = pLocalPlayer->GetObjectOfType( iRemappedObjectID );
		}			

		m_pAvailableObjects[i]->SetVisible( false );
		m_pAlreadyBuiltObjects[i]->SetVisible( false );
		m_pCantAffordObjects[i]->SetVisible( false );

		// If the building is already built
		if ( pObj != NULL && !pObj->IsPlacing() )
		{
			m_pAlreadyBuiltObjects[i]->SetVisible( true );
		}
		// See if we can afford it
		else if ( iAccount < GetObjectInfo( iRemappedObjectID )->m_Cost )
		{
			m_pCantAffordObjects[i]->SetVisible( true );
		}
		else
		{
			// we can buy it
			m_pAvailableObjects[i]->SetVisible( true );
		}
	}
}

void CHudMenuEngyBuild::SetVisible( bool state )
{
	if ( state == true )
	{
		// close the weapon selection menu
		engine->ClientCmd( "cancelselect" );

		bool bConsoleMode = ( IsConsole() || tf_build_menu_controller_mode.GetBool() );

		if ( bConsoleMode != m_bInConsoleMode )
		{
			InvalidateLayout( true, true );
			m_bInConsoleMode = bConsoleMode;
		}

		// set the %lastinv% dialog var to our binding
		const char *key = engine->Key_LookupBinding( "lastinv" );
		if ( !key )
		{
			key = "< not bound >";
		}

		SetDialogVariable( "lastinv", key );

		// Set selection to the first available building that we can build

		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( !pLocalPlayer )
			return;

		int iDefaultSlot = 1;

		// Find the first slot that represents a building that we haven't built
		int iSlot;
		for ( iSlot = 1; iSlot <= 4; iSlot++ )
		{
			int iBuilding = GetBuildingIDFromSlot( iSlot );
			C_BaseObject *pObj = pLocalPlayer->GetObjectOfType( iBuilding );

			if ( pObj == NULL )
			{
				iDefaultSlot = iSlot;
				break;
			}
		}

		m_iSelectedItem = -1;	//force redo
		SetSelectedItem( iDefaultSlot );

		HideLowerPriorityHudElementsInGroup( "mid" );
	}
	else
	{
		UnhideLowerPriorityHudElementsInGroup( "mid" );
	}

	BaseClass::SetVisible( state );
}

void CHudMenuEngyBuild::SetSelectedItem( int iSlot )
{
	if ( m_iSelectedItem != iSlot )
	{
		m_iSelectedItem = iSlot;

		// move the selection item to the new position
		if ( m_pActiveSelection )
		{
			// move the selection background
			int x, y;
			m_pAlreadyBuiltObjects[m_iSelectedItem-1]->GetPos( x, y );

			x -= XRES(4);
			y -= XRES(4);

			m_pActiveSelection->SetPos( x, y );

			UpdateHintLabels();			
		}
	}
}

void CHudMenuEngyBuild::UpdateHintLabels( void )
{
	// hilight the action we can perform ( build or destroy or neither )
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer )
	{
		int iBuilding = GetBuildingIDFromSlot( m_iSelectedItem );
		C_BaseObject *pObj = pLocalPlayer->GetObjectOfType( iBuilding );

		bool bDestroyLabelBright = false;
		bool bBuildLabelBright = false;

		if ( pObj )
		{
			// hilight destroy, we have a building
			bDestroyLabelBright = true;
		}
		else if ( pLocalPlayer->GetAmmoCount( TF_AMMO_METAL ) >= GetObjectInfo( iBuilding )->m_Cost )	// I can afford it
		{
			// hilight build, we can build this
			bBuildLabelBright = true;
		}
		else
		{
			// dim both, do nothing
		}

		if ( m_pDestroyLabelBright && m_pDestroyLabelDim && m_pBuildLabelBright && m_pBuildLabelDim )
		{
			m_pDestroyLabelBright->SetVisible( bDestroyLabelBright );
			m_pDestroyLabelDim->SetVisible( !bDestroyLabelBright );

			m_pBuildLabelBright->SetVisible( bBuildLabelBright );
			m_pBuildLabelDim->SetVisible( !bBuildLabelBright );
		}
	}
}