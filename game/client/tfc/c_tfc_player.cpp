//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_tfc_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "playerandobjectenumerator.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "weapon_tfcbase.h"

#if defined( CTFCPlayer )
	#undef CTFCPlayer
#endif

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_TFCPlayer *pPlayer = dynamic_cast< C_TFCPlayer* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );


// ------------------------------------------------------------------------------------------ //
// Data tables and prediction tables.
// ------------------------------------------------------------------------------------------ //

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_TFCPlayer, DT_TFCPlayer, CTFCPlayer )
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropDataTable( RECVINFO_DT( m_Shared ), 0, &REFERENCE_RECV_TABLE( DT_TFCPlayerShared ) )
END_RECV_TABLE()


BEGIN_PREDICTION_DATA( C_TFCPlayer )
	DEFINE_PRED_TYPEDESCRIPTION( m_Shared, CTFCPlayerShared ),
END_PREDICTION_DATA()



// ------------------------------------------------------------------------------------------ //
// C_TFCPlayer implementation.
// ------------------------------------------------------------------------------------------ //

C_TFCPlayer::C_TFCPlayer() : 
	m_iv_angEyeAngles( "C_TFCPlayer::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreatePlayerAnimState( this );
	m_Shared.Init( this );

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );
}


C_TFCPlayer::~C_TFCPlayer()
{
	m_PlayerAnimState->Release();
}


C_TFCPlayer* C_TFCPlayer::GetLocalTFCPlayer()
{
	return ToTFCPlayer( C_BasePlayer::GetLocalPlayer() );
}

const QAngle& C_TFCPlayer::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return m_PlayerAnimState->GetRenderAngles();
	}
}


void C_TFCPlayer::UpdateClientSideAnimation()
{
	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_TFCPlayer::GetLocalTFCPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	BaseClass::UpdateClientSideAnimation();
}


void C_TFCPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );
	
	BaseClass::PostDataUpdate( updateType );
}


void C_TFCPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}


void C_TFCPlayer::ProcessMuzzleFlashEvent()
{
	// Reenable when the weapons have muzzle flash attachments in the right spot.
	if ( this != C_BasePlayer::GetLocalPlayer() )
	{
		Vector vAttachment;
		QAngle dummyAngles;
		
		C_WeaponTFCBase *pWeapon = m_Shared.GetActiveTFCWeapon();
				
		if ( pWeapon )
		{
			int iAttachment = pWeapon->LookupAttachment( "muzzle_flash" );
				
			if ( iAttachment > 0 )
			{
				float flScale = 1;
				pWeapon->GetAttachment( iAttachment, vAttachment, dummyAngles );
				
				// The way the models are setup, the up vector points along the barrel.
				Vector vForward, vRight, vUp;
				AngleVectors( dummyAngles, &vForward, &vRight, &vUp );
				VectorAngles( vUp, dummyAngles );

				FX_MuzzleEffect( vAttachment, dummyAngles, flScale, INVALID_EHANDLE_INDEX, NULL, true );
			}
		}
	}

	Vector vAttachment;
	QAngle dummyAngles;
	
	bool bFoundAttachment = GetAttachment( 1, vAttachment, dummyAngles );
	// If we have an attachment, then stick a light on it.
	if ( bFoundAttachment )
	{
		dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH + index );
		el->origin = vAttachment;
		el->radius = 24; 
		el->decay = el->radius / 0.05f;
		el->die = gpGlobals->curtime + 0.05f;
		el->color.r = 255;
		el->color.g = 192;
		el->color.b = 64;
		el->color.exponent = 5;
	}
}
