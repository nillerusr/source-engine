#include "cbase.h"
#include "particle_parse.h"
#include "asw_prop_laserable_shared.h"
#include "takedamageinfo.h"

#ifndef CLIENT_DLL
	#include "asw_marine.h"
	#include "asw_player.h"
	#include "cvisibilitymonitor.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
	extern ConVar asw_visrange_generic;
#endif

// This is a physics prop that can only be damaged effectively with the mining laser
// (used for the rocks in Timor Station)

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Prop_Laserable, DT_ASW_Prop_Laserable )

BEGIN_NETWORK_TABLE( CASW_Prop_Laserable, DT_ASW_Prop_Laserable )
#ifdef CLIENT_DLL
// recvprops
	RecvPropString( RECVINFO( m_iszBreakEffect ) ),
	RecvPropString( RECVINFO( m_iszBreakSound ) ),
#else
// sendprops
	SendPropString( SENDINFO( m_iszBreakEffect ) ),
	SendPropString( SENDINFO( m_iszBreakSound ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_prop_laserable, CASW_Prop_Laserable );	

BEGIN_DATADESC( CASW_Prop_Laserable )
#ifndef CLIENT_DLL
	DEFINE_KEYFIELD( m_Key_BreakEffect, FIELD_STRING, "BreakParticleEffect" ),
	DEFINE_KEYFIELD( m_Key_BreakSound, FIELD_SOUNDNAME, "BreakSound" ),
	DEFINE_FIELD( m_fNextLaseredEventTime, FIELD_TIME ),
#endif
END_DATADESC()


CASW_Prop_Laserable::CASW_Prop_Laserable()
{
}

#ifndef CLIENT_DLL
void CASW_Prop_Laserable::Spawn()
{
	BaseClass::Spawn();
	memset( m_iszBreakEffect.GetForModify(), 0, sizeof( m_iszBreakEffect ) );
	memset( m_iszBreakSound.GetForModify(), 0, sizeof( m_iszBreakSound ) );
	Precache();

	VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, NULL, NULL );
}

void CASW_Prop_Laserable::Precache( void )
{
	char *szParticle = (char *)STRING( m_Key_BreakEffect );
	if ( m_Key_BreakEffect != NULL_STRING && Q_strlen( szParticle ) > 1 )
	{
		PrecacheParticleSystem(szParticle);			
	}

	char *szSoundFile = (char *)STRING( m_Key_BreakSound );
	if ( m_Key_BreakSound != NULL_STRING && Q_strlen( szSoundFile ) > 1 )
	{
		if (*szSoundFile != '!')
		{
			PrecacheScriptSound(szSoundFile);			
		}
	}

	BaseClass::Precache();
}

void CASW_Prop_Laserable::Activate( void )
{
	BaseClass::Activate();
	// copy message details from the keyfields over to the networked strings
	Q_strncpy( m_iszBreakEffect.GetForModify(), STRING( m_Key_BreakEffect ), 255 );
	Q_strncpy( m_iszBreakSound.GetForModify(), STRING( m_Key_BreakSound ), 255 );
}

void CASW_Prop_Laserable::OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker )
{
	BaseClass::OnBreak( vecVelocity, angVel, pBreaker );
}

int CASW_Prop_Laserable::OnTakeDamage(const CTakeDamageInfo &info)
{
	CTakeDamageInfo newInfo(info);

	// reduce damage of everything except energy beam (mining laser) damage
	if (newInfo.GetDamageType() & DMG_CLUB)
	{
		return 0;
	}
	else if (newInfo.GetDamageType() & DMG_BUCKSHOT)
	{
		newInfo.ScaleDamage(0.1f);
	}
	else if (!(newInfo.GetDamageType() & DMG_ENERGYBEAM))
	{
		newInfo.ScaleDamage(0.5f);
	}

	if ( gpGlobals->curtime > m_fNextLaseredEventTime && ( newInfo.GetDamageType() & DMG_ENERGYBEAM ) != 0 )
	{
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
		CASW_Player *pPlayerAttacer = NULL;
		if ( pMarine )
		{
			pPlayerAttacer = pMarine->GetCommander();
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "boulder_lasered" );
		if ( event )
		{
			event->SetInt( "userid", ( pPlayerAttacer ? pPlayerAttacer->GetUserID() : 0 ) );
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEventClientSide( event );
		}

		m_fNextLaseredEventTime = gpGlobals->curtime + 3.0f;
	}

	return BaseClass::OnTakeDamage(newInfo);
}
#else
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CASW_Prop_Laserable::UpdateOnRemove( void )
{
	if ( Q_strcmp( m_iszBreakEffect, "" ) != 0 )
	{
		DispatchParticleEffect( m_iszBreakEffect, GetNetworkOrigin(), GetNetworkAngles() );
	}

	if ( Q_strcmp( m_iszBreakSound, "" ) != 0 )
	{
		EmitSound( m_iszBreakSound );
	}

	BaseClass::UpdateOnRemove();
}
#endif



