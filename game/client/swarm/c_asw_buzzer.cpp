#include "cbase.h"
#include "c_AI_BaseNPC.h"
#include "soundenvelope.h"
#include "iasw_client_aim_target.h"
#include "c_asw_alien.h"
#include "c_asw_buzzer.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_fx.h"
#include "c_asw_player.h"
#include "baseparticleentity.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Buzzer is our flying poisoning alien (based on the hl2 manhack code)

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Buzzer, DT_ASW_Buzzer, CASW_Buzzer)
	RecvPropIntWithMinusOneFlag(RECVINFO(m_nEnginePitch1)),
	RecvPropFloat(RECVINFO(m_flEnginePitch1Time)),
	RecvPropBool(RECVINFO(m_bOnFire)),
	RecvPropBool(RECVINFO(m_bElectroStunned)),
END_RECV_TABLE()


C_ASW_Buzzer::C_ASW_Buzzer()
	: m_GlowObject( this )
{
	//m_fAmbientLight = 0.02f;
	m_bClientOnFire = false;
	m_fNextElectroStunEffect = 0;
	m_pBurningEffect = NULL;

	m_GlowObject.SetColor( Vector( 0.3f, 0.6f, 0.1f ) );
	m_GlowObject.SetAlpha( 0.55f );
	m_GlowObject.SetRenderFlags( false, false );
	m_GlowObject.SetFullBloomRender( true );
}

C_ASW_Buzzer::~C_ASW_Buzzer()
{
	m_bOnFire = false;
	UpdateFireEmitters();

	if ( m_pTrailEffect )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_pTrailEffect );
		m_pTrailEffect = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Start the buzzer's engine sound.
//-----------------------------------------------------------------------------
void C_ASW_Buzzer::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if (( m_nEnginePitch1 <= 0 ) )
	{
		SoundShutdown();
	}
	else
	{
		SoundInit();		
	}
	UpdateFireEmitters();

	if ( type == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		if ( !m_pTrailEffect )
		{
			m_pTrailEffect = this->ParticleProp()->Create( "buzzer_trail", PATTACH_ABSORIGIN_FOLLOW );
		}
	}
}

void C_ASW_Buzzer::OnRestore()
{
	BaseClass::OnRestore();
	SoundInit();	
}


//-----------------------------------------------------------------------------
// Purpose: Start the buzzer's engine sound.
//-----------------------------------------------------------------------------
void C_ASW_Buzzer::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
	SoundShutdown();
	m_bOnFire = false;
	UpdateFireEmitters();

	if ( m_pTrailEffect )
	{
		ParticleProp()->StopEmission( m_pTrailEffect, false, true, false );
		m_pTrailEffect = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Start the buzzer's engine sound.
//-----------------------------------------------------------------------------
void C_ASW_Buzzer::SoundInit( void )
{
	if (( m_nEnginePitch1 <= 0 ) )
		return;

	// play an engine start sound!!
	CPASAttenuationFilter filter( this );

	// Bring up the engine looping sound.
	if( !m_pEngineSound1 )
	{
		m_pEngineSound1 = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Buzzer.Idle" );
		CSoundEnvelopeController::GetController().Play( m_pEngineSound1, 0.0, m_nEnginePitch1 );
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pEngineSound1, 0.7, 2.0 );
	}
}

void C_ASW_Buzzer::SoundShutdown(void)
{	
	if ( m_pEngineSound1 )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pEngineSound1 );
		m_pEngineSound1 = NULL;
	}
}


void C_ASW_Buzzer::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
}

void C_ASW_Buzzer::ClientThink()
{
	BaseClass::ClientThink();

	if (m_bElectroStunned && m_fNextElectroStunEffect <= gpGlobals->curtime)
	{
		// apply electro stun effect
		FX_ElectroStun(this);
		m_fNextElectroStunEffect = gpGlobals->curtime + RandomFloat( 0.2, 0.7 );
	}

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && pPlayer->IsSniperScopeActive() )
	{
		m_GlowObject.SetRenderFlags( true, true );
	}
	else
	{
		m_GlowObject.SetRenderFlags( false, false );
	}
}

int C_ASW_Buzzer::DrawModel( int flags, const RenderableInstance_t &instance )
{
	m_vecLastRenderedPos = WorldSpaceCenter();

	return BaseClass::DrawModel( flags, instance );
}