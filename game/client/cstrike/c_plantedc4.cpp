//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_plantedc4.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "engine/IEngineSound.h"
#include "dlight.h"
#include "iefx.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include <bitbuf.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLANTEDC4_MSG_JUSTBLEW 1

ConVar cl_c4dynamiclight( "cl_c4dynamiclight", "0", 0, "Draw dynamic light when planted c4 flashes" );

IMPLEMENT_CLIENTCLASS_DT(C_PlantedC4, DT_PlantedC4, CPlantedC4)
	RecvPropBool( RECVINFO(m_bBombTicking) ),
	RecvPropFloat( RECVINFO(m_flC4Blow) ),
	RecvPropFloat( RECVINFO(m_flTimerLength) ),
	RecvPropFloat( RECVINFO(m_flDefuseLength) ),
	RecvPropFloat( RECVINFO(m_flDefuseCountDown) ),
END_RECV_TABLE()

CUtlVector< C_PlantedC4* > g_PlantedC4s;

C_PlantedC4::C_PlantedC4()
{
	g_PlantedC4s.AddToTail( this );

	m_flNextRadarFlashTime = gpGlobals->curtime;
	m_bRadarFlash = true;
	m_pC4Explosion = NULL;

	// Don't beep right away, leave time for the planting sound
	m_flNextGlow = gpGlobals->curtime + 1.0;
	m_flNextBeep = gpGlobals->curtime + 1.0;
}


C_PlantedC4::~C_PlantedC4()
{
	g_PlantedC4s.FindAndRemove( this );
	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Upon the new round remove the remaining bomb explosion particle effect
	//=============================================================================
	
	if (m_pC4Explosion)
	{
		m_pC4Explosion->SetRemoveFlag();
	}
	 
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void C_PlantedC4::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	
	// Remove us from the list of planted C4s.
	if ( bDormant )
	{
		g_PlantedC4s.FindAndRemove( this );
	}
	else
	{
		if ( g_PlantedC4s.Find( this ) == -1 )
			g_PlantedC4s.AddToTail( this );
	}
}

void C_PlantedC4::Spawn( void )
{
	BaseClass::Spawn();

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_PlantedC4::ClientThink( void )
{
	BaseClass::ClientThink();

	// If it's dormant, don't beep or anything..
	if ( IsDormant() )
		return;

	if ( !m_bBombTicking )
	{
		// disable C4 thinking if not armed
		SetNextClientThink( CLIENT_THINK_NEVER );
		return;
	}

	if( gpGlobals->curtime > m_flNextBeep )
	{
		// as it gets closer to going off, increase the radius

		CLocalPlayerFilter filter;
		float attenuation;
		float freq;

		//the percent complete of the bomb timer
		float fComplete = ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );
		
		fComplete = clamp( fComplete, 0.0f, 1.0f );

		attenuation = MIN( 0.3 + 0.6 * fComplete, 1.0 );
		
		CSoundParameters params;

		if ( GetParametersForSound( "C4.PlantSound", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_SoundLevel = ATTN_TO_SNDLVL( attenuation );
			ep.m_pOrigin = &GetAbsOrigin();

			EmitSound( filter, SOUND_FROM_WORLD, ep );
		}

		freq = MAX( 0.1 + 0.9 * fComplete, 0.15 );

		m_flNextBeep = gpGlobals->curtime + freq;
	}

	if( gpGlobals->curtime > m_flNextGlow )
	{
		int modelindex = modelinfo->GetModelIndex( "sprites/ledglow.vmt" );

		float scale = 0.8f;
		Vector vPos = GetAbsOrigin();
		const Vector offset( 0, 0, 4 );

		// See if the c4 ended up underwater - we need to pull the flash up, or it won't get seen
		if ( enginetrace->GetPointContents( vPos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
		{
			C_CSPlayer *player = GetLocalOrInEyeCSPlayer();
			if ( player )
			{
				const Vector& eyes = player->EyePosition();

				if ( ( enginetrace->GetPointContents( eyes ) & (CONTENTS_WATER|CONTENTS_SLIME) ) == 0 )
				{
					// trace from the player to the water
					trace_t waterTrace;
					UTIL_TraceLine( eyes, vPos, (CONTENTS_WATER|CONTENTS_SLIME), player, COLLISION_GROUP_NONE, &waterTrace );

					if( waterTrace.allsolid != 1 )
					{
						// now trace from the C4 to the edge of the water (in case there was something solid in the water)
						trace_t solidTrace;
						UTIL_TraceLine( vPos, waterTrace.endpos, MASK_SOLID, this, COLLISION_GROUP_NONE, &solidTrace );

						if( solidTrace.allsolid != 1 )
						{
							float waterDist = (solidTrace.endpos - vPos).Length();
							float remainingDist = (solidTrace.endpos - eyes).Length();

							scale = scale * remainingDist / ( remainingDist + waterDist );
							vPos = solidTrace.endpos;
						}
					}
				}
			}
		}

		vPos += offset;

		tempents->TempSprite( vPos, vec3_origin, scale, modelindex, kRenderTransAdd, 0, 1.0, 0.05, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP );

		if( cl_c4dynamiclight.GetBool() )
		{
			dlight_t *dl;

			dl = effects->CL_AllocDlight( entindex() );

			if( dl ) 
			{
				dl->origin = GetAbsOrigin() + offset; // can't use vPos because it might have been moved
				dl->color.r = 255;
				dl->color.g = 0;
				dl->color.b = 0;
				dl->radius = 64;
				dl->die = gpGlobals->curtime + 0.01;
			}
		}

		float freq = 0.1 + 0.9 * ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );

		if( freq < 0.15 ) freq = 0.15;

		m_flNextGlow = gpGlobals->curtime + freq;
	}
}


//=============================================================================
// HPE_BEGIN:
// [menglish] Create the client side explosion particle effect for when the bomb explodes and hide the bomb
//=============================================================================
void C_PlantedC4::Explode( void )
{
	m_pC4Explosion = ParticleProp()->Create( "bomb_explosion_huge", PATTACH_ABSORIGIN );
	AddEffects( EF_NODRAW );
	SetDormant( true );
}
//=============================================================================
// HPE_END
//=============================================================================