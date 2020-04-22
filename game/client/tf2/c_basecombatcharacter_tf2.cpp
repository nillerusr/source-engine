//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2 Specific C_BaseCombatCharacter code.
//
//=============================================================================//
#include "cbase.h"
#include "c_basecombatcharacter.h"
#include "tf_shareddefs.h"
#include "particles_simple.h"
#include "functionproxy.h"
#include "IEffects.h"
#include "weapon_combatshield.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::Release( void )
{
	RemoveAllPowerups();

	BaseClass::Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::SetDormant( bool bDormant )
{
	// If we're going dormant, stop all our powerup sounds
	if ( bDormant )
	{
		RemoveAllPowerups();
	}
	else
	{
		// Restart any powerups on him
		for ( int i = 0; i < MAX_POWERUPS; i++ )
		{
			if ( m_iPowerups & (1 << i) )
			{
				PowerupStart( i, false );
			}
		}
	}

	BaseClass::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_iPrevPowerups = m_iPowerups;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	// Power state changed?
	if ( m_iPowerups != m_iPrevPowerups )
	{
		for ( int i = 0; i < MAX_POWERUPS; i++ )
		{
			bool bPoweredNow = ( m_iPowerups & (1 << i) );
			bool bPoweredThen = ( m_iPrevPowerups & (1 << i) );

			if ( !bPoweredThen && bPoweredNow )
			{
				PowerupStart( i, true );
			}
			else if ( bPoweredThen && !bPoweredNow )
			{
				PowerupEnd( i );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//			If bInitial is set, the server's just told us this powerup has come on.
//			If it's false, the entity had it on when it left PVS, and now it's re-entered
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::PowerupStart( int iPowerup, bool bInitial )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	switch( iPowerup )
	{
	case POWERUP_BOOST:
		break;

	case POWERUP_EMP:
		{
			// Play the EMP sound
			if ( !bInitial )
			{
				EmitSound( "BaseCombatCharacter.EMPPulse" );
			}
		}
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just finished
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::PowerupEnd( int iPowerup )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::RemoveAllPowerups( void )
{
	// Stop any powerups we have
	if ( m_iPowerups )
	{
		for ( int i = 0; i < MAX_POWERUPS; i++ )
		{
			if ( m_iPowerups & (1 << i) )
			{
				PowerupEnd( i );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( !IsDormant() )
	{
		if ( HasPowerup(POWERUP_EMP) )
		{
			AddEMPEffect( WorldAlignSize().Length() * 0.15 );
		}

		if ( HasPowerup(POWERUP_BOOST) )
		{
			AddBuffEffect( WorldAlignSize().Length() * 0.15 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw an effect to show this entity has been EMPed
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::AddEMPEffect( float flSize )
{
	// Don't draw on the local player
	if ( this == C_BasePlayer::GetLocalPlayer() )
		return;

	CSmartPtr<CSimpleEmitter>	pEmitter;
	PMaterialHandle				hParticleMaterial;
	TimedEvent					pParticleEvent;

	pParticleEvent.Init( 300 );
	pEmitter = CSimpleEmitter::Create( "ObjectEMPEffect" );
	hParticleMaterial = pEmitter->GetPMaterial( "sprites/chargeball" );

	// Add particles
	float flCur = gpGlobals->frametime;
	Vector vCenter = WorldSpaceCenter( );

	while ( pParticleEvent.NextEvent( flCur ) )
	{
		Vector vPos;
		Vector vOffset = RandomVector( -1, 1 );
		VectorNormalize( vOffset );
		vPos = vCenter + (vOffset * RandomFloat( 0, flSize ));
		
		pEmitter->SetSortOrigin( vPos );
		SimpleParticle *pParticle = pEmitter->AddSimpleParticle( hParticleMaterial, vPos );
		if ( pParticle )
		{
			// Move the points along the path.
			pParticle->m_vecVelocity.Init();
			pParticle->m_flRoll = 0;
			pParticle->m_flRollDelta = 0;
			pParticle->m_flDieTime = 0.4f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[0] = 255; 
			pParticle->m_uchColor[1] = 255;
			pParticle->m_uchColor[2] = 255;
			pParticle->m_uchStartAlpha = 32;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 4;
			pParticle->m_uchEndSize = 2;
			pParticle->m_iFlags = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw an effect to show this entity is being buffed
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::AddBuffEffect( float flSize )
{
	// Don't draw on the local player
	if ( this == C_BasePlayer::GetLocalPlayer() )
		return;

	CSmartPtr<CSimpleEmitter>	pEmitter;
	PMaterialHandle				hParticleMaterial;
	TimedEvent					pParticleEvent;

	pParticleEvent.Init( 300 );
	pEmitter = CSimpleEmitter::Create( "ObjectBuffEffect" );
	hParticleMaterial = pEmitter->GetPMaterial( "sprites/chargeball" );

	// Add particles
	float flCur = gpGlobals->frametime;
	Vector vCenter = WorldSpaceCenter( );

	while ( pParticleEvent.NextEvent( flCur ) )
	{
		Vector vPos;
		Vector vOffset = RandomVector( -1, 1 );
		VectorNormalize( vOffset );
		vPos = vCenter + (vOffset * RandomFloat( 0, flSize ));
		
		SimpleParticle *pParticle = pEmitter->AddSimpleParticle( hParticleMaterial, vPos );
		if ( pParticle )
		{
			// Move the points along the path.
			pParticle->m_vecVelocity.Init();
			pParticle->m_flRoll = 0;
			pParticle->m_flRollDelta = 0;
			pParticle->m_flDieTime = 0.4f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[1] = 255; pParticle->m_uchColor[0] = pParticle->m_uchColor[2] = 0;
			pParticle->m_uchStartAlpha = 128;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 3;
			pParticle->m_uchEndSize = 1;
			pParticle->m_iFlags = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns shield if owned.
//-----------------------------------------------------------------------------
C_WeaponCombatShield *C_BaseCombatCharacter::GetShield( void )
{
	C_BaseCombatWeapon *pWeapon;
	if ( GetTeamNumber() == TEAM_ALIENS )
	{
		pWeapon = Weapon_OwnsThisType( "weapon_combat_shield_alien" );	
	}
	else
	{
		pWeapon = Weapon_OwnsThisType( "weapon_combat_shield" );	
	}

	if ( !pWeapon )
		return NULL;

	return ( CWeaponCombatShield* )pWeapon;
}
