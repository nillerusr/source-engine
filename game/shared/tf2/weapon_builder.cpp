//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The "weapon" used to build objects
//					
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "EntityList.h"
#include "in_buttons.h"
#include "weapon_builder.h"
#include "tf_obj.h"
#include "sendproxy.h"
#include "weapon_objectselection.h"
#include "info_act.h"
#include "vguiscreen.h"

extern ConVar tf2_object_hard_limits;
extern ConVar tf_fastbuild;

EXTERN_SEND_TABLE(DT_BaseCombatWeapon)

IMPLEMENT_SERVERCLASS_ST(CWeaponBuilder, DT_WeaponBuilder)
	SendPropInt( SENDINFO( m_iBuildState ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iCurrentObject ), BUILDER_OBJECT_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iCurrentObjectState ), 4, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hObjectBeingBuilt ) ),
	SendPropTime( SENDINFO( m_flStartTime ) ),
	SendPropTime( SENDINFO( m_flTotalTime ) ),
	SendPropArray
	( 
		SendPropInt( SENDINFO_ARRAY(m_bObjectValidity), 1, SPROP_UNSIGNED), m_bObjectValidity
	),
	SendPropArray
	( 
		SendPropInt( SENDINFO_ARRAY(m_bObjectBuildability), 1, SPROP_UNSIGNED), m_bObjectBuildability
	),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_builder, CWeaponBuilder );
PRECACHE_WEAPON_REGISTER(weapon_builder);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponBuilder::CWeaponBuilder()
{
	for ( int i=0; i < m_bObjectValidity.Count(); i++ )
		m_bObjectValidity.Set( i, 0 );

	m_iCurrentObject = BUILDER_INVALID_OBJECT;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBuilder::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/weapons/v_slam.mdl" );
	PrecacheVGuiScreen( "screen_human_pda" );
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CWeaponBuilder::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_human_pda";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponBuilder::ShouldShowControlPanels( void )
{
	if ( GetActivity() == ACT_VM_IDLE )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBuilder::UpdateOnRemove( void )
{
	// Tell the player he's lost his build weapon
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( pOwner && pOwner->GetWeaponBuilder() == this )
	{
		pOwner->SetWeaponBuilder( NULL );
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Builder weapon has just been given to a player
//-----------------------------------------------------------------------------
void CWeaponBuilder::Equip( CBaseCombatCharacter *pOwner )
{
	BaseClass::Equip( pOwner );
	((CBaseTFPlayer*)pOwner)->SetWeaponBuilder( this );
}

//-----------------------------------------------------------------------------
// Purpose: Add a new object type to this build weapon. This will allow
//			the player carrying this builder to build the object.
//-----------------------------------------------------------------------------
void CWeaponBuilder::AddBuildableObject( int iObjectType )
{
	m_bObjectValidity.Set( iObjectType, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponBuilder::CanDeploy( void )
{
	if ( m_iCurrentObject != BUILDER_INVALID_OBJECT )
	{
		SetCurrentState( BS_PLACING );
		StartPlacement(); 
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.35f;
	}
	else
	{
		m_hObjectBeingBuilt = NULL;
		SetCurrentState( BS_IDLE );
		SetCurrentObject( m_iCurrentObject );
	}
	return BaseClass::CanDeploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponBuilder::Deploy( )
{
#if !defined( CLIENT_DLL )
	if ( m_hObjectBeingBuilt.Get() && m_hObjectBeingBuilt->IsAnUpgrade() )
		return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_SLAM_STICKWALL_ND_DRAW, (char*)GetAnimPrefix() );

	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_DRAW, (char*)GetAnimPrefix() );
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Prevent switching when working on something
//-----------------------------------------------------------------------------
bool CWeaponBuilder::CanHolster( void )
{
	if ( IsBuilding() )
		return false;

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CWeaponBuilder::GetLastWeapon( void )
{
	return BaseClass::GetLastWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: Stop placement when holstering
//-----------------------------------------------------------------------------
bool CWeaponBuilder::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_iBuildState == BS_PLACING || m_iBuildState == BS_PLACING_INVALID )
	{
		SetCurrentState( BS_IDLE );
	}
	StopPlacement();

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBuilder::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Ignore input while the player's building anything
	if ( pOwner->IsBuilding() )
		return;

	// Switch away if I'm not in placement mode
	if ( m_iBuildState != BS_PLACING && m_iBuildState != BS_PLACING_INVALID )
	{
		pOwner->SwitchToNextBestWeapon( NULL );
		return;
	}

	if (( pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		PrimaryAttack();
	}

	// Allow shield post frame 
	AllowShieldPostFrame( true );

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: Start placing or building the currently selected object
//-----------------------------------------------------------------------------
void CWeaponBuilder::PrimaryAttack( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// What state should we move to?
	switch( m_iBuildState )
	{
	case BS_IDLE:
		{
			// Idle state starts selection
			SetCurrentState( BS_SELECTING );
		}
		break;

	case BS_SELECTING:
		{
			// Do nothing, client handles selection
			return;
		}
		break;

	case BS_PLACING:
		{
			if ( m_hObjectBeingBuilt )
			{
				// Give the object a chance to veto the "start building" command. Objects like barbed wire
				// may want to change their properties instead of actually building yet.
				if ( m_hObjectBeingBuilt->PreStartBuilding() )
				{
					int iFlags = m_hObjectBeingBuilt->GetObjectFlags();

					// Can't build if the game hasn't started
					if ( !tf_fastbuild.GetInt() && CurrentActIsAWaitingAct() )
					{
						ClientPrint( pOwner, HUD_PRINTCENTER, "Can't build until the game's started.\n" );
						return;
					}

					StartBuilding();

					// Should we switch away?
					if ( iFlags & OF_ALLOW_REPEAT_PLACEMENT )
					{
						// Start placing another
						SetCurrentState( BS_PLACING );
						StartPlacement(); 
					}
					else
					{
						pOwner->SwitchToNextBestWeapon( NULL );
					}
				}
			}
		}
		break;

	case BS_PLACING_INVALID:
		{
			WeaponSound( SINGLE_NPC );

			// If there is any associated error text when placing the object, display it
			if( m_hObjectBeingBuilt != NULL )
			{
				if (m_hObjectBeingBuilt->MustBeBuiltInResourceZone())
				{
					ClientPrint( pOwner, HUD_PRINTCENTER, "Only placeable in an empty resource zone.\n" );
				}
				else if (m_hObjectBeingBuilt->MustBeBuiltInConstructionYard())
				{
					ClientPrint( pOwner, HUD_PRINTCENTER, "Only placeable in a construction yard.\n" );
				}
			}
		}
		break;
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
}

//-----------------------------------------------------------------------------
// Purpose: Set the builder to the specified state
//-----------------------------------------------------------------------------
void CWeaponBuilder::SetCurrentState( int iState )
{
	// Check the current build state... we may need to shut some stuff down...
	switch(m_iBuildState)
	{
	case BS_PLACING:
	case BS_PLACING_INVALID:
		{
			if ((iState != BS_PLACING) && (iState != BS_PLACING_INVALID) && (iState != BS_BUILDING))
			{
				StopPlacement();
				WeaponSound( SPECIAL1 );
			}
		}
		break;
	}

	m_iBuildState = iState;
}

//-----------------------------------------------------------------------------
// Purpose: Set the builder to the specified object
//-----------------------------------------------------------------------------
void CWeaponBuilder::SetCurrentObject( int iObject )
{
	// Fixup for invalid objects
	if (iObject < 0)
		iObject = BUILDER_INVALID_OBJECT;

	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	int i;

	// If -1 was passed in, set to our first available object
	if ( iObject == BUILDER_INVALID_OBJECT )
	{
		for ( i = 0; i < OBJ_LAST; i++ )
		{
			if ( m_bObjectValidity[i] )
			{
				iObject = i;
				break;
			}
		}
	}

	// Recalculate the buildability of each object (for propagation to the client)
	for ( i=0; i < m_bObjectBuildability.Count(); i++ )
		m_bObjectBuildability.Set( i, 0 );

	for ( i = 0; i < OBJ_LAST; i++ )
	{
		if ( m_bObjectValidity[i] && pOwner->CanBuild(i) == CB_CAN_BUILD )
		{
			m_bObjectBuildability.Set( i, true );
		}
	}

	m_iCurrentObject = iObject;
	m_iCurrentObjectState = pOwner->CanBuild( m_iCurrentObject );
	m_flStartTime = 0;
	m_flTotalTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Idle updates the position of the build placement model
//-----------------------------------------------------------------------------
void CWeaponBuilder::WeaponIdle( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// If we're in placement mode, update the placement model
	switch( m_iBuildState )
	{
	case BS_PLACING:
	case BS_PLACING_INVALID:
		{
			if ( UpdatePlacement() )
			{
				SetCurrentState( BS_PLACING );
			}
			else
			{
				SetCurrentState( BS_PLACING_INVALID );
			}
		}
		break;

	default:
		break;
	}

	if ( HasWeaponIdleTimeElapsed() )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player holding this weapon has just gained new technology.
//			Check to see if it affects the medikit
//-----------------------------------------------------------------------------
void CWeaponBuilder::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		// Force a recalculation of the state for this object
		SetCurrentObject( m_iCurrentObject );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Start placing the object
//-----------------------------------------------------------------------------
void CWeaponBuilder::StartPlacement( void )
{
	StopPlacement();

	// Create the slab
	m_hObjectBeingBuilt = (CBaseObject*)CreateEntityByName( GetObjectInfo( m_iCurrentObject )->m_pClassName );
	if ( m_hObjectBeingBuilt )
	{
		m_hObjectBeingBuilt->Spawn();
		m_hObjectBeingBuilt->StartPlacement( ToBaseTFPlayer( GetOwner() ) );
		UpdatePlacement();

		// Stomp this here in the same frame we make the object, so prevent clientside warnings that it's under attack
		m_hObjectBeingBuilt->m_iHealth = OBJECT_CONSTRUCTION_STARTINGHEALTH;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the viewmodel according to the type of object we're placing
//-----------------------------------------------------------------------------
const char *CWeaponBuilder::GetViewModel( int viewmodelindex /*=0*/ )  const
{
	if ( m_hObjectBeingBuilt.Get() && m_hObjectBeingBuilt->IsAnUpgrade() )
		return "models/weapons/v_slam.mdl";

	return BaseClass::GetViewModel( viewmodelindex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBuilder::StopPlacement( void )
{
	if ( m_hObjectBeingBuilt )
	{
		m_hObjectBeingBuilt->StopPlacement();
		m_hObjectBeingBuilt = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Move the placement model to the current position. Return false if it's an invalid position
//-----------------------------------------------------------------------------
bool CWeaponBuilder::UpdatePlacement( void )
{
	if ( !m_hObjectBeingBuilt )
		return false;

	return m_hObjectBeingBuilt->UpdatePlacement( ToBaseTFPlayer(GetOwner()) );
}

//-----------------------------------------------------------------------------
// Purpose: Player holding this weapon has started building something
//-----------------------------------------------------------------------------
void CWeaponBuilder::StartBuilding( void )
{
	if ( m_hObjectBeingBuilt.Get() && UpdatePlacement() )
	{
		SetCurrentState( BS_BUILDING );
		m_hObjectBeingBuilt->StartBuilding( GetOwner() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player holding this weapon has aborted the build of an object
//-----------------------------------------------------------------------------
void CWeaponBuilder::StoppedBuilding( int iObjectType )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		// Force a recalculation of the state for this object
		SetCurrentObject( m_iCurrentObject );
		SetCurrentState( BS_IDLE );

		WeaponSound( SPECIAL2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponBuilder::IsBuilding( void )
{
	return ( m_iBuildState == BS_BUILDING );
}

//-----------------------------------------------------------------------------
// Purpose: The player holding this weapon has just finished building an object
//-----------------------------------------------------------------------------
void CWeaponBuilder::FinishedObject( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		// We're no longer building anything...
		m_hObjectBeingBuilt = NULL;

		// Force a recalculation of the state for this object
		SetCurrentObject( m_iCurrentObject );
		SetCurrentState( BS_IDLE );
	}
}
