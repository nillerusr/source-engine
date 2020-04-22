//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_team.h"
#include "baseentity.h"
#include "engine/IEngineSound.h"
#include "triggers.h"

// Spawnflags
#define SF_PLAYSOUND_USE_THIS_ORIGIN		0x0001

//-----------------------------------------------------------------------------
// Purpose: Map entity that plays sounds to players
//-----------------------------------------------------------------------------
class CInfoInputPlaySound : public CBaseEntity
{
	DECLARE_CLASS( CInfoInputPlaySound, CBaseEntity );
public:
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );

	// Inputs
	void InputPlaySoundToAll( inputdata_t &inputdata );
	void InputPlaySoundToTeam1( inputdata_t &inputdata );
	void InputPlaySoundToTeam2( inputdata_t &inputdata );
	void InputPlaySoundToPlayer( inputdata_t &inputdata );
	void InputSetSound( inputdata_t &inputdata );

	// Sound playing
	void PlaySoundToPlayer( CBaseTFPlayer *pPlayer );
	void PlaySoundToTeam( CTFTeam *pTeam );

private:
	string_t	m_iszSound;
	float		m_flVolume;
	float		m_flAttenuation;
	string_t	m_iszTestVolumeName;
	EHANDLE		m_hTestVolume;
};

BEGIN_DATADESC( CInfoInputPlaySound )

	// variables
	DEFINE_KEYFIELD( m_iszSound, FIELD_SOUNDNAME, "Sound" ),
	DEFINE_KEYFIELD( m_flVolume, FIELD_FLOAT, "Volume" ),
	DEFINE_KEYFIELD( m_flAttenuation, FIELD_FLOAT, "Attenuation" ),
	DEFINE_KEYFIELD( m_iszTestVolumeName, FIELD_STRING, "TestVolume" ),

	// inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "SetSound", InputSetSound ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlaySoundToAll", InputPlaySoundToAll ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlaySoundToTeam1", InputPlaySoundToTeam1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlaySoundToTeam2", InputPlaySoundToTeam2 ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "PlaySoundToPlayer", InputPlaySoundToPlayer ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_input_playsound, CInfoInputPlaySound );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::Spawn( void )
{
	m_hTestVolume = NULL;
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::Precache( void )
{
	if ( m_iszSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_iszSound) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::Activate( void )
{
	BaseClass::Activate();

	// Find our test volume, if we have one
	if ( m_iszTestVolumeName != NULL_STRING )
	{
		m_hTestVolume = gEntList.FindEntityByName( NULL, STRING(m_iszTestVolumeName) );
		if ( !m_hTestVolume )
		{
			Msg("ERROR: Could not find test volume %s for info_input_playsound.\n", STRING(m_iszTestVolumeName) );
		}
		else
		{
			// Make sure it's a trigger
			CBaseTrigger *pTrigger = dynamic_cast<CBaseTrigger*>((CBaseEntity*)m_hTestVolume);
			if ( !pTrigger )
			{
				Msg("ERROR: info_input_playsound specifies a volume %s, but it's not a trigger.\n", STRING(m_iszTestVolumeName) );
				m_hTestVolume = NULL;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to all players
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::InputPlaySoundToAll( inputdata_t &inputdata )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
		if ( pPlayer )
		{
			PlaySoundToPlayer( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to all players on team 1
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::InputPlaySoundToTeam1( inputdata_t &inputdata )
{
	PlaySoundToTeam( GetGlobalTFTeam(1) );
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to all players on team 2
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::InputPlaySoundToTeam2( inputdata_t &inputdata )
{
	PlaySoundToTeam( GetGlobalTFTeam(2) );
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to a specific player
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::InputPlaySoundToPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		PlaySoundToPlayer( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the sound to play
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::InputSetSound( inputdata_t &inputdata )
{
	m_iszSound = MAKE_STRING( inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to a team
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::PlaySoundToTeam( CTFTeam *pTeam )
{
	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		PlaySoundToPlayer( (CBaseTFPlayer*)pTeam->GetPlayer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play sound to a player
//-----------------------------------------------------------------------------
void CInfoInputPlaySound::PlaySoundToPlayer( CBaseTFPlayer *pPlayer )
{
	// First, if we have a test volume, make sure the player's within it
	if ( m_hTestVolume )
	{
		CBaseTrigger *pTrigger = (CBaseTrigger *)(CBaseEntity*)m_hTestVolume;
		if ( !pTrigger->IsTouching( pPlayer ) )
			return;
	}

	// Check to see if we're supposed to play it from this entity's location
	if ( HasSpawnFlags( SF_PLAYSOUND_USE_THIS_ORIGIN ) )
	{
		CPASAttenuationFilter filter;
		filter.AddRecipient( pPlayer );
		filter.Filter( GetAbsOrigin(), m_flAttenuation );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_VOICE;
		ep.m_pSoundName = STRING(m_iszSound);
		ep.m_flVolume = m_flVolume;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_flAttenuation );

		ep.m_pOrigin = &(GetAbsOrigin());

		EmitSound( filter, entindex(), ep );
	}
	else
	{
		CSingleUserRecipientFilter filter( pPlayer );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_VOICE;
		ep.m_pSoundName = STRING(m_iszSound);
		ep.m_flVolume = m_flVolume;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_flAttenuation );

		EmitSound( filter, pPlayer->entindex(), ep );
	}
}
