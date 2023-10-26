#include "cbase.h"
#include "c_asw_sentry_top.h"
#include "c_asw_sentry_base.h"
#include "c_asw_sentry_top_flamer.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_te_legacytempents.h"
#include "c_asw_fx.h"
#include "c_asw_generic_emitter.h"
#include "c_user_message_register.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundenvelope.h"
#include "ai_debug_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Sentry_Top_Flamer, DT_ASW_Sentry_Top_Flamer, CASW_Sentry_Top_Flamer)
RecvPropInt( RECVINFO( m_bFiring ) ),	
RecvPropFloat( RECVINFO( m_flPitchHack ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Sentry_Top_Flamer )
END_PREDICTION_DATA()


ConVar asw_sentry_emitter_test("asw_sentry_emitter_test", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Set to 1 to test turret emitters being on");


C_ASW_Sentry_Top_Flamer::C_ASW_Sentry_Top_Flamer() : 
	//m_OldStyleEmitters( 0, 0 ),
	m_pFlamerLoopSound( NULL ), 
	m_szBeginFireSoundScriptName( NULL ),
	m_szDuringFireSoundScriptName( NULL ),
	m_szEndFireSoundScriptName( NULL ),
	m_bFiringShadow( false )
{

	m_szParticleEffectFireName = "asw_flamethrower";

	m_szDuringFireSoundScriptName = "ASW_Sentry.FlameLoop" ;
	m_szEndFireSoundScriptName = "ASW_Sentry.FlameStop" ;
}

void C_ASW_Sentry_Top_Flamer::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		//CreateObsoleteEmitters();

		SetNextClientThink(gpGlobals->curtime);
		// We might want to think every frame.
		// SetNextClientThink( CLIENT_THINK_ALWAYS );

		m_bFiringShadow = m_bFiring;

		if ( HasPilotLight() && !m_hPilotLight )
		{
			m_hPilotLight = ParticleProp()->Create( "asw_flamethrower_pilot_sml", PATTACH_POINT_FOLLOW, "pilot_light" );
			
			if ( m_hPilotLight )
				m_hPilotLight->SetControlPoint( 1, Vector( 1, 0, 0 ) );
		}
	}	
	else
	{
		// what was changed?
		if ( m_bFiring != m_bFiringShadow )
		{
			if ( m_bFiring )
			{
				OnStartFiring();
			}
			else
			{
				OnStopFiring();
			}

			m_bFiringShadow = m_bFiring;
		}
	}
}

void C_ASW_Sentry_Top_Flamer::ClientThink( void )
{
	BaseClass::ClientThink();
/*
	// walk emitters and turn them on or off as appropriate, then call their thinks.
	if ( m_OldStyleEmitters.Count() > 0 )
	{
		// a turret with old school emitters must think every frame. 
		// we need to forcibly reset this because the base class sets a longer
		// think interval.
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		bool bWantEmitters = ShouldEmittersBeOn();

		Vector vecMuzzle = GetAbsOrigin()+Vector(0,0,30);
		QAngle angMuzzle;

		GetAttachment( GetMuzzleAttachment(), vecMuzzle, angMuzzle );

		Vector vForward; AngleVectors(angMuzzle, &vForward);
		angMuzzle.x = m_flPitchHack;
		vecMuzzle += vForward * 36;

		for ( int i = 0 ; i < m_OldStyleEmitters.Count() ; ++i )
		{
			CSmartPtr<CASWGenericEmitter> &hEmitter = m_OldStyleEmitters[i];
			Assert( hEmitter.IsValid() );
			if ( hEmitter.IsValid() )
			{
				if ( hEmitter->GetActive() != bWantEmitters )
				{
					hEmitter->SetActive( bWantEmitters );
				}

				// now think them so they have the correct orientation
				hEmitter->Think(gpGlobals->frametime, vecMuzzle, angMuzzle);
			}
		}
	}
	*/
}

/*
void C_ASW_Sentry_Top_Flamer::CreateObsoleteEmitters( void )
{
	AssertMsg( m_OldStyleEmitters.Count() == 0, "Flame turret tried to create emitters when it already had some!" );

	// create two emitter handles (use default constructor to pull them to NULL to begin with)
	m_OldStyleEmitters.EnsureCount(2);

	CSmartPtr<CASWGenericEmitter> &m_hFlameEmitter = m_OldStyleEmitters[0];
	CSmartPtr<CASWGenericEmitter> &m_hFlameStreamEmitter = m_OldStyleEmitters[1];

	m_hFlameEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFlameEmitter.IsValid() )
	{			
		m_hFlameEmitter->UseTemplate("flamer5");
		m_hFlameEmitter->SetActive(false);
		m_hFlameEmitter->m_hCollisionIgnoreEntity = this;
		m_hFlameEmitter->SetCustomCollisionGroup(ASW_COLLISION_GROUP_IGNORE_NPCS);
		//m_hFlameEmitter->SetGlowMaterial("swarm/sprites/aswredglow2");
	}
	else
	{
		AssertMsg( false, "m_hFlameEmitter.IsValid()" );
		Warning("Failed to create a turret's flame emitter\n");
	}

	m_hFlameStreamEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFlameStreamEmitter.IsValid() )
	{			
		m_hFlameStreamEmitter->UseTemplate("flamerstream1");
		m_hFlameStreamEmitter->SetActive(false);
		m_hFlameStreamEmitter->m_hCollisionIgnoreEntity = this;
		m_hFlameStreamEmitter->SetCustomCollisionGroup(ASW_COLLISION_GROUP_IGNORE_NPCS);
		//m_hFlameEmitter->SetGlowMaterial("swarm/sprites/aswredglow2");
	}
	else
	{
		AssertMsg( false, "m_hFlameStreamEmitter.IsValid()" );
		Warning("Failed to create a turret's flame stream emitter\n");
	}
}
*/

void C_ASW_Sentry_Top_Flamer::OnStartFiring()
{
	if ( m_szBeginFireSoundScriptName )
	{
		EmitSound("ASW_Sentry.FlameStop");
	}

	if ( !IsPlayingFiringLoopSound() && m_szDuringFireSoundScriptName )
	{
		CPASAttenuationFilter filter( this );
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		m_pFlamerLoopSound = controller.SoundCreate( filter, entindex(), m_szDuringFireSoundScriptName );
		CSoundEnvelopeController::GetController().Play( m_pFlamerLoopSound, 1.0, 100 );
	}

	if ( HasPilotLight() && m_hPilotLight )
	{
		if ( m_hPilotLight )
			m_hPilotLight->SetControlPoint( 1, Vector( 0, 0, 0 ) );
	}

	if ( !m_hFiringEffect )
	{
		if ( GetSentryBase() )
		{	
			m_hFiringEffect = ParticleProp()->Create( m_szParticleEffectFireName, PATTACH_POINT_FOLLOW, "muzzle" );
			/*
			if ( m_hFiringEffect )
			{
				//GetSentryBase()->ParticleProp()->AddControlPoint( m_hFiringEffect, 0, this, PATTACH_POINT_FOLLOW, "muzzle" );
				m_hFiringEffect->SetControlPointEntity( 0, GetSentryBase() );
				m_hFiringEffect->SetOwner( GetSentryBase() );
			}
			//GetMuzzleAttachment()
			*/
		}

		/*
		m_hFiringEffect = ParticleProp()->Create( m_szParticleEffectFireName, PATTACH_POINT_FOLLOW ); 
		if ( GetSentryBase() && m_hFiringEffect )
		{
			m_hFiringEffect->SetControlPointEntity( 0, GetSentryBase() );
		}
		*/
	}
}

void C_ASW_Sentry_Top_Flamer::OnStopFiring()
{
	if ( IsPlayingFiringLoopSound() )
	{
		//Msg("Ending flamer loop!\n");
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pFlamerLoopSound );
		m_pFlamerLoopSound = NULL;
	}

	EmitSound( m_szEndFireSoundScriptName );

	if ( m_hFiringEffect )
	{
		m_hFiringEffect->StopEmission();
		m_hFiringEffect = NULL;
	}

	if ( m_hPilotLight )
		m_hPilotLight->SetControlPoint( 1, Vector( 1, 0, 0 ) );
}

void C_ASW_Sentry_Top_Flamer::UpdateOnRemove()
{
	OnStopFiring();

	BaseClass::UpdateOnRemove();
}