//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl1mp_basecombatweapon_shared.h"

#include "effect_dispatch_data.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"
#endif

#include "hl1_player_shared.h"

LINK_ENTITY_TO_CLASS( basehl1mpcombatweapon, CBaseHL1MPCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHL1MPCombatWeapon , DT_BaseHL1MPCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHL1MPCombatWeapon , DT_BaseHL1MPCombatWeapon )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBaseHL1MPCombatWeapon )
END_PREDICTION_DATA()


CBaseHL1MPCombatWeapon::CBaseHL1MPCombatWeapon()
{
	SetPredictionEligible( true );
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}


void CBaseHL1MPCombatWeapon::EjectShell( CBaseEntity *pPlayer, int iType )
{
	QAngle angShellAngles = pPlayer->GetAbsAngles();

	Vector vecForward, vecRight, vecUp;
	AngleVectors( angShellAngles, &vecForward, &vecRight, &vecUp );

	Vector vecShellPosition = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
	switch ( iType )
	{
	case 0:
	default:
		vecShellPosition += vecRight * 4;
		vecShellPosition += vecUp * -12;
		vecShellPosition += vecForward * 20;
		break;
	case 1:
		vecShellPosition += vecRight * 6;
		vecShellPosition += vecUp * -12;
		vecShellPosition += vecForward * 32;
		break;
	}

	Vector vecShellVelocity	= vec3_origin; // pPlayer->GetAbsVelocity();
	vecShellVelocity += vecRight * random->RandomFloat( 50, 70 );
	vecShellVelocity += vecUp * random->RandomFloat( 100, 150 );
	vecShellVelocity += vecForward * 25;

	angShellAngles.x = 0;
	angShellAngles.z = 0;

	CEffectData	data;
	data.m_vStart	= vecShellVelocity;
	data.m_vOrigin	= vecShellPosition;
	data.m_vAngles	= angShellAngles;
	data.m_fFlags	= iType;

	DispatchEffect( "HL1ShellEject", data );
}


#ifdef CLIENT_DLL

void CBaseHL1MPCombatWeapon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}


bool CBaseHL1MPCombatWeapon::ShouldPredict()
{
	if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}


void CBaseHL1MPCombatWeapon::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
	BaseClass::ApplyBoneMatrixTransform( transform );
}

#endif


bool CBaseHL1MPCombatWeapon::IsPredicted() const
{ 
	return true;
}


CBasePlayer* CBaseHL1MPCombatWeapon::GetPlayerOwner() const
{
	return dynamic_cast< CBasePlayer* >( GetOwner() );
}


void CBaseHL1MPCombatWeapon::WeaponSound( WeaponSound_t sound_type, float soundtime /* = 0.0f */ )
{
#ifdef CLIENT_DLL
	// If we have some sounds from the weapon classname.txt file, play a random one of them
	const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
	if ( !shootsound || !shootsound[0] )
		return;

	CBroadcastRecipientFilter filter; // this is client side only
	if ( !te->CanPredict() )
		return;
				
	CBaseEntity::EmitSound( filter, GetPlayerOwner()->entindex(), shootsound, &GetPlayerOwner()->GetAbsOrigin() ); 
#else
	BaseClass::WeaponSound( sound_type, soundtime );
#endif
}
