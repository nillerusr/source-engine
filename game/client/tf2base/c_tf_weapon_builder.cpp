//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CWeaponBuilder class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "clientmode_tf.h"
#include "engine/IEngineSound.h"
#include "c_tf_weapon_builder.h"
#include "c_weapon__stubs.h"
#include "iinput.h"
#include <vgui/IVGui.h>
#include "c_tf_player.h"
#include "c_vguiscreen.h"
#include "ienginevgui.h"

STUB_WEAPON_CLASS_IMPLEMENT( tf_weapon_builder, C_TFWeaponBuilder );

// Recalc object sprite when we receive a new object type to build
void RecvProxy_ObjectType( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Pass to normal Int recvproxy
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	// Reset the object sprite
	C_TFWeaponBuilder *pBuilder = ( C_TFWeaponBuilder * )pStruct;
	pBuilder->SetupObjectSelectionSprite();
}

BEGIN_NETWORK_TABLE_NOBASE( C_TFWeaponBuilder, DT_BuilderLocalData )
	RecvPropInt( RECVINFO(m_iObjectType), 0, RecvProxy_ObjectType ),
	RecvPropEHandle( RECVINFO(m_hObjectBeingBuilt) ),
END_NETWORK_TABLE()


IMPLEMENT_CLIENTCLASS_DT(C_TFWeaponBuilder, DT_TFWeaponBuilder, CTFWeaponBuilder)
	RecvPropInt( RECVINFO(m_iBuildState) ),
	RecvPropDataTable( "BuilderLocalData", 0, 0, &REFERENCE_RECV_TABLE( DT_BuilderLocalData ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFWeaponBuilder::C_TFWeaponBuilder()
{
	m_iBuildState = 0;
	m_iObjectType = BUILDER_INVALID_OBJECT;
	m_pSelectionTextureActive = NULL;
	m_pSelectionTextureInactive = NULL;
	m_iValidBuildPoseParam = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFWeaponBuilder::~C_TFWeaponBuilder()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_TFWeaponBuilder::GetCurrentSelectionObjectName( void )
{
	if ( m_iObjectType == -1 || (m_iBuildState == BS_SELECTING) )
		return "";

	return GetObjectInfo( m_iObjectType )->m_pBuilderWeaponName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFWeaponBuilder::Deploy( void )
{
	m_iViewModelIndex = modelinfo->GetModelIndex( GetViewModel( 0 ) );
	m_iWorldModelIndex = modelinfo->GetModelIndex( GetWorldModel() );

	bool bDeploy = BaseClass::Deploy();

	if ( bDeploy )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.35f;
		m_flNextSecondaryAttack = gpGlobals->curtime;		// asap

		CTFPlayer *pPlayer = ToTFPlayer( GetOwner() );
		if (!pPlayer)
			return false;

		pPlayer->SetNextAttack( gpGlobals->curtime );
	}

	return bDeploy;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFWeaponBuilder::SecondaryAttack( void )
{
	if ( m_bInAttack2 )
		return;

	// require a re-press
	m_bInAttack2 = true;

	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	pOwner->DoClassSpecialSkill();

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.2f;
}

//-----------------------------------------------------------------------------
// Purpose: cache the build pos pose param
//-----------------------------------------------------------------------------
CStudioHdr *C_TFWeaponBuilder::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	m_iValidBuildPoseParam = LookupPoseParameter( "valid_build_pos" );

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: only called for local player
//-----------------------------------------------------------------------------
void C_TFWeaponBuilder::Redraw()
{
	if ( m_iValidBuildPoseParam >= 0 )
	{
		CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
		if ( !pOwner )
			return;

		// Assuming here that our model is the same as our viewmodel's model!
		CBaseViewModel *pViewModel = pOwner->GetViewModel(0);

		if ( pViewModel )
		{
			float flPoseParamValue = pViewModel->GetPoseParameter( m_iValidBuildPoseParam );

			C_BaseObject *pObj = m_hObjectBeingBuilt.Get();

			if ( pObj && pObj->WasLastPlacementPosValid() )
			{
				// pose param approach 1.0
				flPoseParamValue = Approach( 1.0, flPoseParamValue, 3.0 * gpGlobals->frametime );
			}
			else
			{
				// pose param approach 0.0
				flPoseParamValue = Approach( 0.0, flPoseParamValue, 1.5 * gpGlobals->frametime );
			}

			pViewModel->SetPoseParameter( m_iValidBuildPoseParam, flPoseParamValue );
		}
	}

	BaseClass::Redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFWeaponBuilder::IsPlacingObject( void )
{
	if ( m_iBuildState == BS_PLACING || m_iBuildState == BS_PLACING_INVALID )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFWeaponBuilder::GetSlot( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_SelectionSlot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFWeaponBuilder::GetPosition( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_SelectionPosition;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFWeaponBuilder::SetupObjectSelectionSprite( void )
{
#ifdef CLIENT_DLL
	// Use the sprite details from the text file, with a custom sprite
	char *iconTexture = GetObjectInfo( m_iObjectType )->m_pIconActive;
	if ( iconTexture && iconTexture[ 0 ] )
	{
		m_pSelectionTextureActive = gHUD.GetIcon( iconTexture );
	}
	else
	{
		m_pSelectionTextureActive = NULL;
	}

	iconTexture = GetObjectInfo( m_iObjectType )->m_pIconInactive;
	if ( iconTexture && iconTexture[ 0 ] )
	{
		m_pSelectionTextureInactive = gHUD.GetIcon( iconTexture );
	}
	else
	{
		m_pSelectionTextureInactive = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture const *C_TFWeaponBuilder::GetSpriteActive( void ) const
{
	return m_pSelectionTextureActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture const *C_TFWeaponBuilder::GetSpriteInactive( void ) const
{
	return m_pSelectionTextureInactive;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_TFWeaponBuilder::GetPrintName( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_pStatusName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_TFWeaponBuilder::GetSubType( void )
{
	return m_iObjectType;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon can be selected via the weapon selection
//-----------------------------------------------------------------------------
bool C_TFWeaponBuilder::CanBeSelected( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
		return false;

	if ( pOwner->CanBuild( m_iObjectType ) != CB_CAN_BUILD )
		return false;

	return HasAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon should be visible in the weapon selection
//-----------------------------------------------------------------------------
bool C_TFWeaponBuilder::VisibleInWeaponSelection( void )
{
	return GetObjectInfo( m_iObjectType )->m_bVisibleInWeaponSelection;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon has some ammo
//-----------------------------------------------------------------------------
bool C_TFWeaponBuilder::HasAmmo( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
		return false;

	int iCost = CalculateObjectCost( m_iObjectType );
	return ( pOwner->GetBuildResources() >= iCost );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
const char *C_TFWeaponBuilder::GetViewModel( int iViewModel ) const
{
	if ( GetPlayerOwner() == NULL )
	{
		return BaseClass::GetViewModel();
	}

	if ( m_iObjectType != BUILDER_INVALID_OBJECT )
	{
		return GetObjectInfo( m_iObjectType )->m_pViewModel;
	}

	return BaseClass::GetViewModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_TFWeaponBuilder::GetWorldModel( void ) const
{
	if ( GetPlayerOwner() == NULL )
	{
		return BaseClass::GetWorldModel();
	}

	if ( m_iObjectType != BUILDER_INVALID_OBJECT )
	{
		return GetObjectInfo( m_iObjectType )->m_pPlayerModel;
	}

	return BaseClass::GetWorldModel();
}

Activity C_TFWeaponBuilder::GetDrawActivity( void )
{
	// sapper used to call different draw animations , one when invis and one when not.
	// now you can go invis *while* deploying, so let's always use the one-handed deploy.
	if ( GetSubType() == OBJ_ATTACHMENT_SAPPER )
	{
		return ACT_VM_DRAW_DEPLOYED;
	}
	else
	{
		return BaseClass::GetDrawActivity();
	}
}
