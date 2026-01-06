//=============================================================================//
//
// Purpose:		Base for husk classes. Originally based on stalker AI.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_BASE_H
#define NPC_HUSK_BASE_H
#ifdef _WIN32
#pragma once
#endif

#include "ez2/npc_husk_base_shared.h"
#include "ai_squad.h"
#include "ai_memory.h"
#include "ai_senses.h"
#include "ai_interactions.h"
#include "saverestore_utlvector.h"

//-----------------------------------------------------------------------------

// See npc_husk_base.cpp for cvar declarations
extern ConVar sk_husk_common_reaction_time_multiplier;

extern ConVar sk_husk_common_escalation_time;
extern ConVar sk_husk_common_escalation_sprint_speed;
extern ConVar sk_husk_common_escalation_sprint_delay;

extern ConVar sk_husk_common_infight_time;

//-----------------------------------------------------------------------------

#define	DEFINE_BASE_HUSK_DATADESC() \
	DEFINE_FIELD( m_nHuskAggressionLevel, FIELD_INTEGER ),	\
	DEFINE_KEYFIELD( m_nHuskCognitionFlags, FIELD_INTEGER, "HuskCognition" ),	\
	DEFINE_KEYFIELD( m_bAlwaysHostile, FIELD_BOOLEAN, "AlwaysHostile" ),	\
	DEFINE_KEYFIELD( m_flSuspiciousRadius, FIELD_FLOAT, "SuspiciousRadius" ),	\
	DEFINE_KEYFIELD( m_flHostileRadius, FIELD_FLOAT, "HostileRadius" ),	\
	DEFINE_EMBEDDED( m_SuspicionEscalateDelay ),	\
	DEFINE_FIELD( m_bDelayedEscalation, FIELD_BOOLEAN ),	\
	DEFINE_UTLVECTOR( m_PassiveTargets, FIELD_EHANDLE ),	\
	DEFINE_UTLVECTOR( m_HostileOverrideTargets, FIELD_EHANDLE ),	\
	DEFINE_UTLVECTOR( m_HostileOverrideTargetTimes, FIELD_FLOAT ),	\
	DEFINE_KEYFIELD( m_bNoInfighting, FIELD_BOOLEAN, "NoInfighting" ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"MakeCalm",	InputMakeCalm ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"MakeSuspicious",	InputMakeSuspicious ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"MakeAngry",		InputMakeAngry ),	\
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"SetHuskAggression",	InputSetHuskAggression ),	\
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"AddHuskAggression",	InputAddHuskAggression ),	\
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"SetHuskCognitionFlags",		InputSetHuskCognitionFlags ),	\
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"AddHuskCognitionFlags",		InputAddHuskCognitionFlags ),	\
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"RemoveHuskCognitionFlags",	InputRemoveHuskCognitionFlags ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"AddPassiveTarget",	InputAddPassiveTarget ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"RemovePassiveTarget",	InputRemovePassiveTarget ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"AddHostileOverrideTarget",	InputAddHostileOverrideTarget ),	\
	DEFINE_INPUTFUNC( FIELD_EHANDLE,	"RemoveHostileOverrideTarget",	InputRemoveHostileOverrideTarget ),	\
	DEFINE_OUTPUT(m_OnAggressionCalm, "OnAggressionCalm"),	\
	DEFINE_OUTPUT(m_OnAggressionSuspicious, "OnAggressionSuspicious"),	\
	DEFINE_OUTPUT(m_OnAggressionAngry, "OnAggressionAngry"),	\

#define	DEFINE_BASE_HUSK_SCRIPTDESC() \
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetHuskAggressionLevel, "GetHuskAggressionLevel", "Get the husk's aggression level." )	\
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetHuskCognitionFlags, "GetHuskCognitionFlags", "Get the husk's aggression flags." )	\

#define DEFINE_BASE_HUSK_SENDPROPS() \
	SendPropInt( SENDINFO( m_nHuskAggressionLevel ) ),	\
	SendPropInt( SENDINFO( m_nHuskCognitionFlags ) ),	\

//-----------------------------------------------------------------------------

#define TLK_HUSK_SUSPICIOUS "TLK_SUSPICIOUS"
#define TLK_HUSK_SUSPICIOUS_TO_ANGRY "TLK_STARTLED"

//-----------------------------------------------------------------------------
// Classes can cast to this and access some basic husk functions.
//-----------------------------------------------------------------------------
abstract_class CAI_HuskSink
{
public:

	// The NPC this husk is based off of
	virtual const char *GetBaseNPCClassname() = 0;

	virtual int GetHuskAggressionLevel() = 0;
	virtual int GetHuskCognitionFlags() = 0;

	virtual void MakeCalm( CBaseEntity *pActivator ) = 0;
	virtual void MakeSuspicious( CBaseEntity *pActivator ) = 0;
	virtual void MakeAngry( CBaseEntity *pActivator ) = 0;

	virtual bool IsSuspicious() = 0;
	virtual bool IsSuspiciousOrHigher() = 0;
	virtual bool IsAngry() = 0;

	virtual bool IsPassiveTarget( CBaseEntity *pTarget ) = 0;
	virtual bool IsHostileOverrideTarget( CBaseEntity *pTarget ) = 0;
};

//-----------------------------------------------------------------------------
//
// Template class for husks.
//
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_BaseHusk : public BASE_NPC, public CAI_HuskSink
{
	DECLARE_CLASS_NOFRIEND( CAI_BaseHusk, BASE_NPC );

public:

	void		Spawn();
	void		Activate();
	bool		KeyValue( const char *szKeyName, const char *szValue );
	bool		GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );

	Class_T			Classify( void ) { return CLASS_COMBINE_HUSK; }
	bool			IsNeutralTarget( CBaseEntity *pTarget );
	Disposition_t	IRelationType( CBaseEntity *pTarget );

	bool		IsValidEnemy( CBaseEntity *pEnemy );
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy );
	bool		QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC );
	bool		QueryHearSound( CSound *pSound );
	void		OnLooked( int iDistance );
	void		OnListened();
	void		OnStateChange( NPC_STATE OldState, NPC_STATE NewState );
	bool		UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );
	float		GetReactionDelay( CBaseEntity *pEnemy );

	void		ModifyOrAppendCriteria( AI_CriteriaSet &set );
	void		ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet &set );

	bool		FCanCheckAttacks( void );
	Vector		GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy = true );
	Vector		GetActualShootPosition( const Vector &shootOrigin );
	void		AimGun();

	void		PrescheduleThink();
	int			TranslateSchedule( int scheduleType );

	void		Event_Killed( const CTakeDamageInfo &info );
	int			OnTakeDamage_Alive( const CTakeDamageInfo &info );

	//-----------------------------------------------------------------------------
	
	// Should be overridden by child classes
	virtual void	SuspiciousSound() {}
	virtual void	StartledSound() {}

	virtual float	GetHostileRadius() { return this->m_flHostileRadius; }
	virtual float	GetSuspiciousRadius() { return this->m_flSuspiciousRadius; }

	virtual float	ShouldDelayEscalation() { return false; }

	//-----------------------------------------------------------------------------

	// From CAI_HuskSink
	int GetHuskAggressionLevel() override { return this->m_nHuskAggressionLevel; }
	int ScriptGetHuskAggressionLevel() { return this->m_nHuskAggressionLevel; }

	void MakeCalm( CBaseEntity *pActivator ) override;
	void MakeSuspicious( CBaseEntity *pActivator ) override;
	void MakeAngry( CBaseEntity *pActivator ) override;

	bool IsSuspicious() override { return this->m_nHuskAggressionLevel == HUSK_AGGRESSION_LEVEL_SUSPICIOUS; }
	bool IsSuspiciousOrHigher() override { return this->m_nHuskAggressionLevel >= HUSK_AGGRESSION_LEVEL_SUSPICIOUS; }
	bool IsAngry() override { return this->m_nHuskAggressionLevel == HUSK_AGGRESSION_LEVEL_ANGRY; }

	void SetHuskAggression( int nAmt, CBaseEntity *pActivator );

	//-----------------------------------------------------------------------------

	// From CAI_HuskSink
	int GetHuskCognitionFlags() override { return this->m_nHuskCognitionFlags; }
	int ScriptGetHuskCognitionFlags() { return this->m_nHuskCognitionFlags; }

	void RefreshCognitionFlags();

	inline void	SetCognitionFlags( int nFlags ) { this->m_nHuskCognitionFlags = nFlags; this->RefreshCognitionFlags(); }
	inline void	AddCognitionFlags( int nFlags ) { this->m_nHuskCognitionFlags |= nFlags; this->RefreshCognitionFlags(); }
	inline void	RemoveCognitionFlags( int nFlags ) { this->m_nHuskCognitionFlags &= ~nFlags; this->RefreshCognitionFlags(); }

	inline bool	HasCognitionFlags( int nFlag ) { return this->m_nHuskCognitionFlags & nFlag; }

	//-----------------------------------------------------------------------------

	// From CAI_HuskSink
	bool IsPassiveTarget( CBaseEntity *pTarget ) override { return this->m_PassiveTargets.HasElement( pTarget ); }

	inline bool HasPassiveTargets() { return this->m_PassiveTargets.Count() > 0; }
	virtual void AddPassiveTarget( CBaseEntity *pTarget ) { this->m_PassiveTargets.AddToTail( pTarget ); }
	virtual bool RemovePassiveTarget( CBaseEntity *pTarget ) { return this->m_PassiveTargets.FindAndRemove( pTarget ); }

	//-----------------------------------------------------------------------------

	// From CAI_HuskSink
	bool IsHostileOverrideTarget( CBaseEntity *pTarget ) override { return this->m_HostileOverrideTargets.HasElement( pTarget ); }

	inline bool HasHostileOverrideTargets() { return this->m_HostileOverrideTargets.Count() > 0; }
	virtual void AddHostileOverrideTarget( CBaseEntity *pTarget, float flTime );
	virtual bool RemoveHostileOverrideTarget( CBaseEntity *pTarget );

	//-----------------------------------------------------------------------------

	void InputMakeCalm( inputdata_t &inputdata ) { this->MakeCalm( inputdata.value.Entity() ); }
	void InputMakeSuspicious( inputdata_t &inputdata ) { this->MakeSuspicious( inputdata.value.Entity() ); }
	void InputMakeAngry( inputdata_t &inputdata ) { this->MakeAngry( inputdata.value.Entity() ); }

	void InputSetHuskAggression( inputdata_t &inputdata ) { this->SetHuskAggression( clamp( inputdata.value.Int(), HUSK_AGGRESSION_LEVEL_CALM, LAST_HUSK_AGGRESSION_LEVEL ), inputdata.pActivator ); }
	void InputAddHuskAggression( inputdata_t &inputdata ) { this->SetHuskAggression( clamp( this->m_nHuskAggressionLevel+inputdata.value.Int(), HUSK_AGGRESSION_LEVEL_CALM, LAST_HUSK_AGGRESSION_LEVEL ), inputdata.pActivator ); }

	void InputSetHuskCognitionFlags( inputdata_t &inputdata ) { this->SetCognitionFlags( inputdata.value.Int() ); }
	void InputAddHuskCognitionFlags( inputdata_t &inputdata ) { this->AddCognitionFlags( inputdata.value.Int() ); }
	void InputRemoveHuskCognitionFlags( inputdata_t &inputdata ) { this->RemoveCognitionFlags( inputdata.value.Int() ); }

	void InputAddPassiveTarget( inputdata_t &inputdata ) { this->AddPassiveTarget( inputdata.value.Entity() ); }
	void InputRemovePassiveTarget( inputdata_t &inputdata ) { this->RemovePassiveTarget( inputdata.value.Entity() ); }

	void InputAddHostileOverrideTarget( inputdata_t &inputdata ) { this->AddHostileOverrideTarget( inputdata.value.Entity(), FLT_MAX ); }
	void InputRemoveHostileOverrideTarget( inputdata_t &inputdata ) { this->RemoveHostileOverrideTarget( inputdata.value.Entity() ); }

	// We can't have any private saved variables or protected networked variables because only derived classes use the relevant interfaces
public:

	CNetworkVarForDerived( int, m_nHuskAggressionLevel );
	CNetworkVarForDerived( int, m_nHuskCognitionFlags );

protected:

	bool	m_bAlwaysHostile = false;

	float	m_flSuspiciousRadius = 256.0f;
	float	m_flHostileRadius = 48.0f;

	CSimpleStopwatch m_SuspicionEscalateDelay;
	bool	m_bDelayedEscalation;

	// Husks won't be suspicious of passive targets
	CUtlVector<EHANDLE>	m_PassiveTargets;

	// Husks attack neutral/friendly targets which provoked them
	CUtlVector<EHANDLE>		m_HostileOverrideTargets;
	CUtlVector<float>		m_HostileOverrideTargetTimes;
	bool	m_bNoInfighting; // Ignores attacks from squadmates

	COutputEvent	m_OnAggressionCalm;
	COutputEvent	m_OnAggressionSuspicious;
	COutputEvent	m_OnAggressionAngry;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::Spawn()
{
	BaseClass::Spawn();

	this->RefreshCognitionFlags();
	
	// Interactions
	if (g_interactionHuskSuspicious == 0)
	{
		g_interactionHuskSuspicious = CBaseCombatCharacter::GetInteractionID();
		g_interactionHuskAngry = CBaseCombatCharacter::GetInteractionID();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::Activate()
{
	BaseClass::Activate();

	this->RefreshCognitionFlags();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "blind" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_BLIND ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_BLIND );
		return true;
	}
	else if ( FStrEq( szKeyName, "deaf" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_DEAF ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_DEAF );
		return true;
	}
	else if ( FStrEq( szKeyName, "hyperfocused" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_HYPERFOCUSED ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_HYPERFOCUSED );
		return true;
	}
	else if ( FStrEq( szKeyName, "short_memory" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_SHORT_MEMORY ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_SHORT_MEMORY );
		return true;
	}
	else if ( FStrEq( szKeyName, "miscount_ammo" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_MISCOUNT_AMMO ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_MISCOUNT_AMMO );
		return true;
	}
	else if ( FStrEq( szKeyName, "broken_radio" ) )
	{
		atoi( szValue ) > 0 ?
			this->AddCognitionFlags( bits_HUSK_COGNITION_BROKEN_RADIO ) :
			this->RemoveCognitionFlags( bits_HUSK_COGNITION_BROKEN_RADIO );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "blind" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) ? "1" : "0", iMaxLen );
		return true;
	}
	else if ( FStrEq( szKeyName, "deaf" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_DEAF ) ? "1" : "0", iMaxLen );
		return true;
	}
	else if ( FStrEq( szKeyName, "hyperfocused" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_HYPERFOCUSED ) ? "1" : "0", iMaxLen );
		return true;
	}
	else if ( FStrEq( szKeyName, "short_memory" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_SHORT_MEMORY ) ? "1" : "0", iMaxLen );
		return true;
	}
	else if ( FStrEq( szKeyName, "miscount_ammo" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_MISCOUNT_AMMO ) ? "1" : "0", iMaxLen );
		return true;
	}
	else if ( FStrEq( szKeyName, "broken_radio" ) )
	{
		Q_strncpy( szValue, this->HasCognitionFlags( bits_HUSK_COGNITION_BROKEN_RADIO ) ? "1" : "0", iMaxLen );
		return true;
	}

	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Disposition_t CAI_BaseHusk<BASE_NPC>::IRelationType( CBaseEntity *pTarget )
{
	Disposition_t disposition = BaseClass::IRelationType( pTarget );

	if (pTarget == NULL)
		return disposition;

	if ( this->IsPassiveTarget( pTarget ) )
	{
		// We like people who give us what we want
		return D_LI;
	}

	if ( this->IsHostileOverrideTarget( pTarget ) )
	{
		// We hate people who give us what we hate (e.g. gunshot wounds)
		return D_HT;
	}

	if ( this->HasPassiveTargets() )
	{
		// Commandable husks will defend their passive targets
		if (pTarget->GetEnemy() && this->IsCommandable() && this->IsPassiveTarget( pTarget->GetEnemy() ))
		{
			Class_T targetClassify = pTarget->Classify();
			if ( targetClassify == CLASS_COMBINE_HUSK )
			{
				// Don't attack ourselves, silly
				if ( pTarget == this )
					return disposition;

				// If this is another husk, only attack it if it's angry
				CAI_HuskSink *pHusk = dynamic_cast <CAI_HuskSink*>(pTarget);
				if (pHusk && pHusk->IsAngry())
					return D_HT;
			}
			else if ( targetClassify != CLASS_ALIEN_FLORA )
				return D_HT;
		}
		else if (pTarget->IsCombatCharacter())
		{
			// If it likes one of our passive targets, we like it too
			for (int i = 0; i < this->m_PassiveTargets.Count(); i++)
			{
				if (pTarget->MyCombatCharacterPointer()->IRelationType( this->m_PassiveTargets[i] ) == D_LI)
					return D_LI;
			}
		}
	}

	return disposition;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::IsValidEnemy( CBaseEntity *pEnemy )
{
	if ( this->IsNeutralTarget( pEnemy ) )
	{
		// Don't get angry at these folks unless provoked.
		if ( !this->IsSuspiciousOrHigher() )
		{
			return false;
		}
	}

	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_HYPERFOCUSED ) && this->GetEnemy() != pEnemy && this->HasCondition(COND_SEE_ENEMY) && this->EnemyDistance( pEnemy ) > this->m_flHostileRadius )
	{
		// Short attention span. If I have an enemy, stick with it.
		return false;
	}

	return BaseClass::IsValidEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::CanBeAnEnemyOf( CBaseEntity *pEnemy )	
{ 
	// Neutral targets which fear me respect my neutrality, but enemies which hate me do not
	// (i.e. vorts might not mind me, but citizens and Combine soldiers will)
	if ( !this->IsAngry() && pEnemy->IsNPC() && this->IsNeutralTarget(pEnemy) && pEnemy->MyNPCPointer()->IRelationType(this) == D_FR )
		return false;

	// If this enemy is allied with one of my passive targets, they shouldn't need to attack me
	if ( this->HasPassiveTargets() && pEnemy->IsCombatCharacter() && !this->IsHostileOverrideTarget( pEnemy ) )
	{
		// If it likes one of our passive targets, we like it too
		for (int i = 0; i < this->m_PassiveTargets.Count(); i++)
		{
			if (pEnemy->MyCombatCharacterPointer()->IRelationType( this->m_PassiveTargets[i] ) == D_LI)
				return false;
		}
	}

	return BaseClass::CanBeAnEnemyOf( pEnemy );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	// Only see really close enemies (i.e. close enough to feel their vibrations)
	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) && (this->GetAbsOrigin() - pEntity->GetAbsOrigin()).LengthSqr() > Square( this->m_flHostileRadius ) )
		return false;

	return BaseClass::QuerySeeEntity( pEntity, bOnlyHateOrFearIfNPC );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::QueryHearSound( CSound *pSound )
{
	// Only hear really close sounds (i.e. close enough to feel their vibrations)
	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_DEAF ) && (this->GetAbsOrigin() - pSound->GetSoundOrigin()).LengthSqr() > Square( this->m_flHostileRadius ) )
		return false;

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::OnLooked( int iDistance )
{
	BaseClass::OnLooked( iDistance );

	if ( !this->IsAngry() )
	{
		AISightIter_t iter;
		CBaseEntity *pSightEnt = this->GetSenses()->GetFirstSeenEntity( &iter );

		CBaseEntity *pAngryTarget = NULL;
		CBaseEntity *pSuspiciousTarget = NULL;

		while( pSightEnt )
		{
			Disposition_t relation = this->IRelationType( pSightEnt );

			if ( relation <= D_FR )
			{
				if (this->IsNeutralTarget( pSightEnt ))
				{
					// If this entity is close enough, become suspicious
					float flDist = this->EnemyDistance( pSightEnt );
					if (flDist <= this->GetHostileRadius())
					{
						pAngryTarget = pSightEnt;
						break;
					}
					else if (flDist <= this->GetSuspiciousRadius())
					{
						pSuspiciousTarget = pSightEnt;
					}
				}
				else
				{
					// Always get angry if we see someone we actually don't like
					pAngryTarget = pSightEnt;
					break;
				}
			}

			pSightEnt = this->GetSenses()->GetNextSeenEntity( &iter );
		}

		if ( pAngryTarget )
		{
			this->MakeAngry( pAngryTarget );
		}
		else if ( pSuspiciousTarget )
		{
			if (!this->IsSuspicious())
			{
				this->MakeSuspicious( pSuspiciousTarget );
			}
			else if (this->m_SuspicionEscalateDelay.Expired())
			{
				// Get angry after a while
				this->MakeAngry( pSuspiciousTarget );
				this->m_SuspicionEscalateDelay.Stop();
			}
			else
			{
				// Reduce the timer every think we look at a fast-moving player
				if ( pSuspiciousTarget->IsPlayer() && pSuspiciousTarget->GetSmoothedVelocity().Length() >= sk_husk_common_escalation_sprint_speed.GetFloat() )
				{
					this->m_SuspicionEscalateDelay.Delay( sk_husk_common_escalation_sprint_delay.GetFloat() );
				}
			}
		}
	}

	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) )
	{
		// HACKHACK: Blind husks should still set COND_SEE_PLAYER due to its requirement in cases like weapon-giving
		// (...even though blind husks have no logical reason to see them anyway...)
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		if ( pPlayer && this->FInViewCone( pPlayer ) && this->FVisible( pPlayer ) )
		{
			this->SetCondition( COND_SEE_PLAYER );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::OnListened()
{
	BaseClass::OnListened();

	// Only query sounds if we hear something we care about
	if (!IsAngry() &&
		(this->HasCondition( COND_HEAR_COMBAT ) || this->HasCondition( COND_HEAR_BULLET_IMPACT ) || this->HasCondition( COND_HEAR_DANGER )))
	{
		AISoundIter_t iter;
		CSound *pCurrentSound = this->GetSenses()->GetFirstHeardSound( &iter );
		while ( pCurrentSound )
		{
			if ( pCurrentSound->FIsSound() )
			{
				int nSoundType = pCurrentSound->SoundTypeNoContext();
				if (nSoundType == SOUND_COMBAT || nSoundType == SOUND_BULLET_IMPACT || nSoundType == SOUND_DANGER)
				{
					// UNDONE: Visibility checking
					if (this->MyNPCPointer()->FVisible(pCurrentSound->GetSoundOrigin()))
					{
						// Seven Hour War flashbacks
						this->MakeAngry( pCurrentSound->m_hOwner ? pCurrentSound->m_hOwner : this );
					}
					/*else
					{
						this->MakeSuspicious();
					}*/

					if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND )
						&& (this->WorldSpaceCenter() - pCurrentSound->GetSoundOrigin()).LengthSqr() > Square( 48.0f )
						&& pCurrentSound->m_hOwner && this->IRelationType(pCurrentSound->m_hOwner) <= D_FR)
					{
						// Update memory for combat sounds from enemies
						this->UpdateEnemyMemory( pCurrentSound->m_hOwner, pCurrentSound->GetSoundOrigin(), pCurrentSound->m_hOwner );
					}
					break;
				}
			}

			pCurrentSound = this->GetSenses()->GetNextHeardSound( &iter );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );

	if (OldState == NPC_STATE_COMBAT && (NewState == NPC_STATE_IDLE || NewState == NPC_STATE_ALERT))
	{
		// Reset suspicion
		//if (this->IsSuspicious())
			this->MakeCalm( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update information on my enemy
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if (pInformer && pInformer != this && pInformer != pEnemy && this->HasCognitionFlags(bits_HUSK_COGNITION_BROKEN_RADIO))
	{
		// Only listen to informers we can see and hear
		if ((this->GetAbsOrigin() - pInformer->GetAbsOrigin()).LengthSqr() > 4096.0f || !this->FVisible( pInformer ))
			return false;
	}

	return BaseClass::UpdateEnemyMemory( pEnemy, position, pInformer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_BaseHusk<BASE_NPC>::GetReactionDelay( CBaseEntity *pEnemy )
{
	if (this->IsInPlayerSquad())
	{
		// Having the player as a leader gives clarity
		return BaseClass::GetReactionDelay( pEnemy );
	}
	else
	{
		return BaseClass::GetReactionDelay( pEnemy ) * sk_husk_common_reaction_time_multiplier.GetFloat();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline bool CAI_BaseHusk<BASE_NPC>::IsNeutralTarget( CBaseEntity *pTarget )
{
	// Nobody's neutral if we're always hostile
	if (this->m_bAlwaysHostile)
		return false;

	Class_T targetClass = pTarget->Classify();
	switch (targetClass)
	{
		case CLASS_PLAYER:
		case CLASS_PLAYER_ALLY:
		case CLASS_PLAYER_ALLY_VITAL:
		case CLASS_VORTIGAUNT:
		case CLASS_CITIZEN_PASSIVE:
		case CLASS_CITIZEN_REBEL:
		case CLASS_COMBINE:
		case CLASS_METROPOLICE:
		case CLASS_COMBINE_HUNTER:
		case CLASS_MANHACK:
		case CLASS_COMBINE_NEMESIS:
			return true;
	}

	// Simple way of adding new neutral targets
	// TODO: Is this too unintuitive? The use cases of this might be too limited to justify a more complex implementation
	if (pTarget->HasContext( "husk_neutral", "1" ))
		return true;

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "husk_cognizance", UTIL_VarArgs("%i", (int)this->m_nHuskCognitionFlags) );
	set.AppendCriteria( "husk_aggression", UTIL_VarArgs("%i", (int)this->m_nHuskAggressionLevel) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteriaForPlayer( pPlayer, set );
	
	set.AppendCriteria( "husk_cognizance", UTIL_VarArgs("%i", (int)this->m_nHuskCognitionFlags) );
	set.AppendCriteria( "husk_aggression", UTIL_VarArgs("%i", (int)this->m_nHuskAggressionLevel) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::FCanCheckAttacks( void )
{
	if ( this->GetEnemy() )
	{
		if ( this->IsSuspicious() )
		{
			// Don't attack enemies you're just suspicious of
			return false;
		}
		else if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) && ( this->GetNavType() != NAV_CLIMB && this->GetNavType() != NAV_JUMP ) )
		{
			// Blind husks should just shoot forwards if they noticed the enemy in the past 5 seconds
			//if (gpGlobals->curtime - this->GetEnemies()->LastTimeSeen( this->GetEnemy() ) <= 5.0f)
			CSound *pSound = this->GetBestSound( SOUND_PLAYER | SOUND_COMBAT | SOUND_BULLET_IMPACT );
			if (pSound != NULL /*&& pSound->m_hOwner && IRelationType(pSound->m_hOwner) <= D_FR*/)
				return true;
		}
	}

	return BaseClass::FCanCheckAttacks();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Vector CAI_BaseHusk<BASE_NPC>::GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy )
{
	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) )
	{
		// Can't see our enemy, just shoot forwards
		matrix3x4_t attachmentToWorld;
		if ( this->GetActiveWeapon() )
		{
			this->GetActiveWeapon()->GetAttachment( this->GetActiveWeapon()->LookupAttachment( "muzzle" ), attachmentToWorld );
		}
		else if ( this->CapabilitiesGet() & bits_CAP_INNATE_RANGE_ATTACK1 )
		{
			this->GetAttachment( this->LookupAttachment( "muzzle" ), attachmentToWorld );
		}
		else
			return BaseClass::GetShootEnemyDir( shootOrigin, bNoisy );

		Vector vecShootDir;
		MatrixGetColumn( attachmentToWorld, 0, vecShootDir );
		return vecShootDir;
	}

	return BaseClass::GetShootEnemyDir( shootOrigin, bNoisy );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Vector CAI_BaseHusk<BASE_NPC>::GetActualShootPosition( const Vector &shootOrigin )
{
	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) )
	{
		// Can't see our enemy, just shoot forwards
		Vector vecShootOrigin, vecShootDir;
		if ( this->GetActiveWeapon() )
		{
			this->GetActiveWeapon()->GetAttachment( this->GetActiveWeapon()->LookupAttachment( "muzzle" ), vecShootOrigin, &vecShootDir );
		}
		else if ( this->CapabilitiesGet() & bits_CAP_INNATE_RANGE_ATTACK1 )
		{
			this->GetAttachment( this->LookupAttachment( "muzzle" ), vecShootOrigin, &vecShootDir );
		}
		else
			return BaseClass::GetActualShootPosition( shootOrigin );

		return vecShootOrigin + (vecShootDir * 128.0f);
	}

	return BaseClass::GetActualShootPosition( shootOrigin );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::AimGun()
{
	if ( this->HasCognitionFlags( bits_HUSK_COGNITION_BLIND ) && this->GetEnemy() )
	{
		// Get our best sound location relative to my weapon shoot position
		CSound *pSound = this->GetBestSound();
		if (pSound && pSound->m_hOwner && this->IRelationType( pSound->m_hOwner ) <= D_FR && (this->WorldSpaceCenter() - pSound->GetSoundOrigin()).LengthSqr() > Square( 48.0f ))
		{
			Vector vecSoundOrigin = pSound->GetSoundOrigin() + (this->Weapon_ShootPosition() - this->GetAbsOrigin());
			Vector vecShootDir = vecSoundOrigin - this->Weapon_ShootPosition();
			this->MyNPCPointer()->SetAim(vecShootDir);
		}
		else
		{
			this->MyNPCPointer()->RelaxAim();
		}
		return;
	}

	BaseClass::AimGun();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::PrescheduleThink( void )
{
	if (this->IsSuspicious() && !this->m_bDelayedEscalation)
	{
		// Check if we should delay escalation
		float flDelay = this->ShouldDelayEscalation();
		if (flDelay != 0.0f)
		{
			this->m_bDelayedEscalation = true;
			this->m_SuspicionEscalateDelay.Delay( flDelay );
		}
	}
	else
	{
		if ( this->HasHostileOverrideTargets() )
		{
			// Check if we should stop attacking this target
			for (int i = this->m_HostileOverrideTargets.Count()-1; i >= 0; i--)
			{
				CBaseEntity *pTarget = this->m_HostileOverrideTargets[i];
				if (!pTarget || !pTarget->IsAlive() || gpGlobals->curtime > this->m_HostileOverrideTargetTimes[i])
				{
					// Should no longer attack
					this->m_HostileOverrideTargets.Remove( i );
					this->m_HostileOverrideTargetTimes.Remove( i );
					continue;
				}
			}
		}
	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_BaseHusk<BASE_NPC>::TranslateSchedule( int scheduleType )
{
	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::Event_Killed( const CTakeDamageInfo &info )
{
	if ( this->IsInSquad() && info.GetAttacker() /*&& this->IsNeutralTarget( info.GetAttacker() )*/ )
	{
		AISquadIter_t iter;
		for ( CAI_BaseNPC *pSquadMember = this->GetSquad()->GetFirstMember( &iter ); pSquadMember; pSquadMember = this->GetSquad()->GetNextMember( &iter ) )
		{
			if ( pSquadMember->IsAlive() && pSquadMember != this )
			{
				CAI_HuskSink *pHusk = dynamic_cast <CAI_HuskSink *>(pSquadMember);

				if ( pHusk )
				{
					if (pSquadMember->FVisible( this ) || pSquadMember->FVisible( info.GetAttacker() ))
					{
						pHusk->MakeAngry( info.GetAttacker() );
					}
					else
					{
						pHusk->MakeSuspicious( info.GetAttacker() );
					}
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_BaseHusk<BASE_NPC>::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int nRet = BaseClass::OnTakeDamage_Alive( info );

	if (nRet > 0)
	{
		if (/*!this->IsAngry() &&*/ info.GetAttacker() /*&& this->IsNeutralTarget( info.GetAttacker() )*/)
		{
			// Friendship ended with activator
			this->RemovePassiveTarget( info.GetAttacker() );

			// If we liked them anyway, start hating them
			Disposition_t disposition = this->IRelationType( info.GetAttacker() );
			if ( (disposition >= D_LI || this->IsHostileOverrideTarget(info.GetAttacker())) && info.GetAttacker() != this )
			{
				if (info.GetAttacker()->IsNPC() && this->GetSquad() && this->GetSquad() == info.GetAttacker()->MyNPCPointer()->GetSquad())
				{
					// They're in the same squad as me. If infighting is allowed, exile them from the squad
					if (!m_bNoInfighting)
					{
						info.GetAttacker()->MyNPCPointer()->RemoveFromSquad();
						this->AddHostileOverrideTarget( info.GetAttacker(), sk_husk_common_infight_time.GetFloat() );
					}
				}
				else
					this->AddHostileOverrideTarget( info.GetAttacker(), sk_husk_common_infight_time.GetFloat() );
			}

			this->MakeAngry( info.GetAttacker() );

			if (this->IsInSquad() )
			{
				AISquadIter_t iter;
				for ( CAI_BaseNPC *pSquadMember = this->GetSquad()->GetFirstMember( &iter ); pSquadMember; pSquadMember = this->GetSquad()->GetNextMember( &iter ) )
				{
					if ( pSquadMember->IsAlive() && pSquadMember != this )
					{
						CAI_HuskSink *pHusk = dynamic_cast <CAI_HuskSink *>(pSquadMember);

						if ( pHusk )
						{
							// For regular damage, other husks must also have the attacker in their viewcone
							if (pSquadMember->FVisible( info.GetAttacker() ) && pSquadMember->FInViewCone( info.GetAttacker() ))
							{
								pHusk->MakeAngry( info.GetAttacker() );
							}
							else
							{
								pHusk->MakeSuspicious( info.GetAttacker() );
							}
						}
					}
				}
			}
		}

		if ( this->m_nHuskCognitionFlags & bits_HUSK_COGNITION_BLIND && info.GetAttacker() && IRelationType( info.GetAttacker() ) <= D_FR )
		{
			// Shoot in the direction we were shot in
			this->UpdateEnemyMemory( info.GetAttacker(), info.GetAttacker()->GetAbsOrigin(), info.GetAttacker() );
		}
	}
	
	return nRet;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::SetHuskAggression( int nAmt, CBaseEntity *pActivator )
{
	if (this->m_nHuskAggressionLevel == nAmt)
		return;

	if (pActivator == NULL)
		pActivator = this;

	this->m_nHuskAggressionLevel = nAmt;

	switch (this->m_nHuskAggressionLevel)
	{
		case HUSK_AGGRESSION_LEVEL_CALM:
			this->m_OnAggressionCalm.FireOutput( pActivator, this );
			break;
		case HUSK_AGGRESSION_LEVEL_SUSPICIOUS:
			this->m_SuspicionEscalateDelay.Start( sk_husk_common_escalation_time.GetFloat() );
			this->m_OnAggressionSuspicious.FireOutput( pActivator, this );
			break;
		case HUSK_AGGRESSION_LEVEL_ANGRY:
			this->m_OnAggressionAngry.FireOutput( pActivator, this );
			break;
	}

	this->RefreshCognitionFlags();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::MakeCalm( CBaseEntity *pActivator )
{
	if (this->IsInAScript())
		return;

	if (this->m_nHuskAggressionLevel > HUSK_AGGRESSION_LEVEL_CALM)
	{
		this->SetHuskAggression( HUSK_AGGRESSION_LEVEL_CALM, pActivator );
		this->m_bDelayedEscalation = false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::MakeSuspicious( CBaseEntity *pActivator )
{
	if (this->IsInAScript())
		return;

	if (this->m_nHuskAggressionLevel < HUSK_AGGRESSION_LEVEL_SUSPICIOUS)
	{
		this->SetCondition( COND_PROVOKED );
		this->SetEnemy( pActivator );

		if (this->m_nHuskAggressionLevel == HUSK_AGGRESSION_LEVEL_CALM)
		{
			this->SuspiciousSound();
		}

		this->SetHuskAggression( HUSK_AGGRESSION_LEVEL_SUSPICIOUS, pActivator );

		this->UpdateEnemyMemory( pActivator, pActivator->GetAbsOrigin(), pActivator );

		pActivator->DispatchInteraction( g_interactionHuskSuspicious, NULL, this );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::MakeAngry( CBaseEntity *pActivator )
{
	if (this->IsInAScript())
		return;

	if (this->IsPassiveTarget( pActivator ))
		return;

	if (this->m_nHuskAggressionLevel < HUSK_AGGRESSION_LEVEL_ANGRY)
	{
		this->SetCondition( COND_PROVOKED );
		this->SetEnemy( pActivator );

		if ((this->m_NPCState == NPC_STATE_IDLE || this->m_nHuskAggressionLevel == HUSK_AGGRESSION_LEVEL_SUSPICIOUS) && this->IsAlive())
		{
			// Only play startled sound if the activator wasn't already our enemy
			// (i.e. don't play over the regular see enemy sound)
			this->StartledSound();
		}

		this->SetHuskAggression( HUSK_AGGRESSION_LEVEL_ANGRY, pActivator );

		pActivator->DispatchInteraction( g_interactionHuskAngry, NULL, this );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::RefreshCognitionFlags()
{
	// Always use short memory when suspicious
	if ( this->m_nHuskAggressionLevel < HUSK_AGGRESSION_LEVEL_ANGRY || this->m_nHuskCognitionFlags & bits_HUSK_COGNITION_SHORT_MEMORY )
	{
		this->GetEnemies()->SetEnemyDiscardTime( 5.0f );
	}
	else
	{
		this->GetEnemies()->SetEnemyDiscardTime( AI_DEF_ENEMY_DISCARD_TIME );
	}

	// Hide eye glow when blind
	if ( this->m_nHuskCognitionFlags & bits_HUSK_COGNITION_BLIND )
	{
		this->KillSprites( 0.0f );
	}
	else
	{
		this->StartEye();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BaseHusk<BASE_NPC>::AddHostileOverrideTarget( CBaseEntity *pTarget, float flTime )
{
	int idx = this->m_HostileOverrideTargets.Find( pTarget );
	if (idx != this->m_HostileOverrideTargets.InvalidIndex())
	{
		// Refresh the timer
		float flNewTime = (gpGlobals->curtime + flTime);
		if (this->m_HostileOverrideTargetTimes[idx] < flNewTime)
			this->m_HostileOverrideTargetTimes[idx] = flNewTime;
		return;
	}

	this->m_HostileOverrideTargets.AddToTail( pTarget );

	idx = this->m_HostileOverrideTargetTimes.AddToTail();
	this->m_HostileOverrideTargetTimes[idx] = ( gpGlobals->curtime + flTime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BaseHusk<BASE_NPC>::RemoveHostileOverrideTarget( CBaseEntity *pTarget )
{
	int idx = this->m_HostileOverrideTargets.Find( pTarget );
	if (idx == this->m_HostileOverrideTargets.InvalidIndex())
		return false;

	this->m_HostileOverrideTargets.Remove( idx );
	this->m_HostileOverrideTargetTimes.Remove( idx );
	return true;
}

#endif // NPC_HUSK_BASE_H
