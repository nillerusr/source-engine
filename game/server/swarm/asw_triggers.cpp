#include "cbase.h"
#include "asw_triggers.h"
#include "asw_marine.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "asw_gamerules.h"
#include "asw_weapon.h"
#include "asw_util_shared.h"
#include "asw_melee_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_synup_chatter_chance("asw_synup_chatter_chance", "0.2", FCVAR_CHEAT, "Chance of the synup chatter happening");

// Chance trigger - has a random chance of actually firing

LINK_ENTITY_TO_CLASS( trigger_asw_chance, CASW_Chance_Trigger_Multiple );

BEGIN_DATADESC( CASW_Chance_Trigger_Multiple )
	DEFINE_KEYFIELD( m_fTriggerChance, FIELD_FLOAT, "TriggerChance" ),		
	
	// Function Pointers
	DEFINE_FUNCTION(ChanceMultiTouch),
	DEFINE_FUNCTION(MultiWaitOver ),

	// Outputs
	DEFINE_OUTPUT(m_OnTrigger, "OnTrigger")
END_DATADESC()

void CASW_Chance_Trigger_Multiple::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	if (m_flWait == 0)
	{
		m_flWait = 0.2;
	}

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_Chance_Trigger_Multiple::ChanceMultiTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Chance_Trigger_Multiple::ChanceMultiTouch(CBaseEntity *pOther)
{
	if (PassesTriggerFilters(pOther))
	{
		ActivateChanceMultiTrigger( pOther );
	}
}

void CASW_Chance_Trigger_Multiple::ActivateChanceMultiTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	m_hActivator = pActivator;

	if (RandomFloat() < m_fTriggerChance)
		m_OnTrigger.FireOutput(m_hActivator, this);

	if (m_flWait > 0)
	{
		SetThink( &CASW_Chance_Trigger_Multiple::MultiWaitOver );
		SetNextThink( gpGlobals->curtime + m_flWait );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.1f );
		SetThink(  &CASW_Chance_Trigger_Multiple::SUB_Remove );
	}
}

void CASW_Chance_Trigger_Multiple::MultiWaitOver( void )
{
	SetThink( NULL );
}



LINK_ENTITY_TO_CLASS( trigger_asw_random_target, CASW_Random_Target_Trigger_Multiple );

BEGIN_DATADESC( CASW_Random_Target_Trigger_Multiple )
	DEFINE_KEYFIELD( m_iNumOutputs, FIELD_INTEGER, "NumOutputs" ),	
	// Function Pointers
	DEFINE_FUNCTION(RandomMultiTouch),
	DEFINE_FUNCTION(MultiWaitOver),

	// Outputs
	DEFINE_OUTPUT(m_OnTrigger1, "OnTrigger1"),
	DEFINE_OUTPUT(m_OnTrigger2, "OnTrigger2"),
	DEFINE_OUTPUT(m_OnTrigger3, "OnTrigger3"),
	DEFINE_OUTPUT(m_OnTrigger4, "OnTrigger4"),
	DEFINE_OUTPUT(m_OnTrigger5, "OnTrigger5"),
	DEFINE_OUTPUT(m_OnTrigger6, "OnTrigger6"),
END_DATADESC()

void CASW_Random_Target_Trigger_Multiple::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	if (m_flWait == 0)
	{
		m_flWait = 0.2;
	}

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_Random_Target_Trigger_Multiple::RandomMultiTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Random_Target_Trigger_Multiple::RandomMultiTouch(CBaseEntity *pOther)
{
	if (PassesTriggerFilters(pOther))
	{
		ActivateRandomMultiTrigger( pOther );
	}
}

void CASW_Random_Target_Trigger_Multiple::ActivateRandomMultiTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	m_hActivator = pActivator;

	int i = RandomInt(1, m_iNumOutputs);
	switch (i)
	{
		case 2: m_OnTrigger2.FireOutput(m_hActivator, this);  break;
		case 3: m_OnTrigger3.FireOutput(m_hActivator, this);  break;
		case 4: m_OnTrigger4.FireOutput(m_hActivator, this);  break;
		case 5: m_OnTrigger5.FireOutput(m_hActivator, this);  break;
		case 6: m_OnTrigger6.FireOutput(m_hActivator, this);  break;
		default: m_OnTrigger1.FireOutput(m_hActivator, this);  break;
	}

	if (m_flWait > 0)
	{
		SetThink( &CASW_Random_Target_Trigger_Multiple::MultiWaitOver );
		SetNextThink( gpGlobals->curtime + m_flWait );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.1f );
		SetThink(  &CASW_Random_Target_Trigger_Multiple::SUB_Remove );
	}
}

void CASW_Random_Target_Trigger_Multiple::MultiWaitOver( void )
{
	SetThink( NULL );
}


LINK_ENTITY_TO_CLASS( trigger_asw_supplies_chatter, CASW_Supplies_Chatter_Trigger );

BEGIN_DATADESC( CASW_Supplies_Chatter_Trigger )	
	// Function Pointers
	DEFINE_FUNCTION(SuppliesChatterTouch),
	DEFINE_FUNCTION(MultiWaitOver ),

	DEFINE_KEYFIELD(m_bNoAmmo, FIELD_BOOLEAN, "NoAmmo"),

	// Outputs
	DEFINE_OUTPUT(m_OnTrigger, "OnTrigger")
END_DATADESC()

void CASW_Supplies_Chatter_Trigger::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	if (m_flWait == 0)
	{
		m_flWait = 0.2;
	}

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_Supplies_Chatter_Trigger::SuppliesChatterTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Supplies_Chatter_Trigger::SuppliesChatterTouch(CBaseEntity *pOther)
{
	if (PassesTriggerFilters(pOther) && pOther->Classify() == CLASS_ASW_MARINE)
	{
		ActivateSuppliesChatterTrigger( pOther );
	}
}

void CASW_Supplies_Chatter_Trigger::ActivateSuppliesChatterTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	m_hActivator = pActivator;

	m_OnTrigger.FireOutput(m_hActivator, this);

	// pick a nearby marine to say the chatter
	if ( ASWGameResource() )
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		// count how many are nearby
		int iNearby = 0;						
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if (pMarine && (pActivator->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
						&& pMarine->GetHealth() > 0)
						iNearby++;
		}
		if (iNearby >= 2)		// only do the speech if there are at least 2 marines nearby
		{
			int iChatter = random->RandomInt(0, iNearby-1);
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
				if (pMarine && (pActivator->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
							&& pMarine->GetHealth() > 0)
				{
					if (iChatter <= 0)
					{
						if (m_bNoAmmo)
							pMarine->GetMarineSpeech()->Chatter(CHATTER_SUPPLIES);
						else
							pMarine->GetMarineSpeech()->Chatter(CHATTER_SUPPLIES_AMMO);
						break;					
					}
					iChatter--;
				}
			}
		}
	}

	if (m_flWait > 0)
	{
		SetThink( &CASW_Supplies_Chatter_Trigger::MultiWaitOver );
		SetNextThink( gpGlobals->curtime + m_flWait );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.1f );
		SetThink(  &CASW_Supplies_Chatter_Trigger::SUB_Remove );
	}
}

void CASW_Supplies_Chatter_Trigger::MultiWaitOver( void )
{
	SetThink( NULL );
}


LINK_ENTITY_TO_CLASS( trigger_asw_synup_chatter, CASW_SynUp_Chatter_Trigger );

BEGIN_DATADESC( CASW_SynUp_Chatter_Trigger )	
	// Function Pointers
	DEFINE_FUNCTION(ChatterTouch),
	DEFINE_FUNCTION(MultiWaitOver ),

	// Outputs
	DEFINE_OUTPUT(m_OnTrigger, "OnTrigger")
END_DATADESC()

void CASW_SynUp_Chatter_Trigger::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	m_flWait = 1.0f;	// always retry (trigger deletes itself when the conversation takes place or could take place)

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_SynUp_Chatter_Trigger::ChatterTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_SynUp_Chatter_Trigger::ChatterTouch(CBaseEntity *pOther)
{
	if (PassesTriggerFilters(pOther) && pOther->Classify() == CLASS_ASW_MARINE)
	{
		ActivateChatterTrigger( pOther );
	}
}

void CASW_SynUp_Chatter_Trigger::ActivateChatterTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	m_hActivator = pActivator;


	m_OnTrigger.FireOutput(m_hActivator, this);
	
	float f = random->RandomFloat();


	// pick a nearby marine to say the chatter
	if ( ASWGameResource() )
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		// count how many are nearby
		int iNearby = 0;						
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if (pMarine && (pActivator->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
						&& pMarine->GetHealth() > 0 && pMarine->GetMarineProfile()
						&& (pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_VEGAS || pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH))
						iNearby++;
		}
		int iChatter = random->RandomInt(0, iNearby-1);
		if (iNearby > 0)
		{
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
				if (pMarine && (pActivator->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
							&& pMarine->GetHealth() > 0 && pMarine->GetMarineProfile()
							&& (pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_VEGAS || pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH))
				{
					if (iChatter <= 0)
					{
						if (f <= asw_synup_chatter_chance.GetFloat())
						{
							CASW_MarineSpeech::StartConversation(CONV_SYNUP, pMarine);
						}
						else
						{
						}
						m_flWait = 0;							
						break;						
					}
					iChatter--;
				}
			}
		}
	}

	if (m_flWait > 0)
	{
		SetThink( &CASW_SynUp_Chatter_Trigger::MultiWaitOver );
		SetNextThink( gpGlobals->curtime + m_flWait );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		SetNextThink( gpGlobals->curtime + 0.1f );
		SetThink(  &CASW_SynUp_Chatter_Trigger::SUB_Remove );
	}
}

void CASW_SynUp_Chatter_Trigger::MultiWaitOver( void )
{
	SetThink( NULL );
}


LINK_ENTITY_TO_CLASS( trigger_asw_marine_position, CASW_Marine_Position_Trigger );

BEGIN_DATADESC( CASW_Marine_Position_Trigger )
	DEFINE_KEYFIELD( m_fDesiredFacing,			FIELD_FLOAT,	"DesiredFacing" ),
	DEFINE_KEYFIELD( m_fTolerance, FIELD_FLOAT, "Tolerance" ),	
	// Function Pointers
	DEFINE_FUNCTION(PositionTouch),
	DEFINE_FUNCTION(MultiWaitOver ),

	// Outputs
	DEFINE_OUTPUT(m_OnTrigger, "OnTrigger"),
	DEFINE_OUTPUT(m_OnMarineInPosition, "MarineInPosition"),
	DEFINE_OUTPUT(m_OnMarineOutOfPosition, "MarineOutOfPosition"),	
	
END_DATADESC()

void CASW_Marine_Position_Trigger::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	if (m_flWait == 0)
	{
		m_flWait = 0.2;
	}

	m_hMarine = NULL;

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_Marine_Position_Trigger::PositionTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Marine_Position_Trigger::PositionTouch(CBaseEntity *pOther)
{	
	if (PassesTriggerFilters(pOther) && pOther->Classify() == CLASS_ASW_MARINE)
	{
		ActivatePositionTrigger( pOther );
	}
}

void CASW_Marine_Position_Trigger::ActivatePositionTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	m_hActivator = pActivator;
	m_OnTrigger.FireOutput(m_hActivator, this);
			
	SetThink( &CASW_Marine_Position_Trigger::MultiWaitOver );
	SetNextThink( gpGlobals->curtime + 0.1f );	
}

void CASW_Marine_Position_Trigger::MultiWaitOver( void )
{
	int iMarines = 0;
	for (int i=0;i<m_hTouchingEntities.Count();i++)
	{
		CBaseEntity *pOther = m_hTouchingEntities[i];
		if (pOther && pOther->Classify() == CLASS_ASW_MARINE)
		{
			iMarines++;
			if (!m_hMarine.Get() || m_hMarine.Get() == pOther)
			{
				float fYawDiff = UTIL_AngleDiff(m_fDesiredFacing, pOther->EyeAngles()[YAW]);
				bool bInPosition = fabs(fYawDiff) < m_fTolerance;
				if (bInPosition && !m_hMarine.Get())
				{
					m_OnMarineInPosition.FireOutput(pOther, this);
					m_hMarine = pOther;
				}
				else if (m_hMarine.Get() && !bInPosition)
				{
					m_OnMarineOutOfPosition.FireOutput(pOther, this);
					m_hMarine = NULL;
				}
			}
		}
	}

	if (iMarines <= 0)
	{
		SetThink( NULL );		// no more marines inside us, so clear think and allow retrigger
		if (m_hMarine.Get())
		{
			m_OnMarineOutOfPosition.FireOutput(m_hMarine.Get(), this);
			m_hMarine = NULL;
		}
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );		// constantly check up on the marines inside us, NOTE: no more OnTriggers will fire while doing this
	}
}

// -------------------------------------------------------------
// entity that fires outputs when a marine is healed
// -------------------------------------------------------------

LINK_ENTITY_TO_CLASS( asw_info_heal, CASW_Info_Heal );

BEGIN_DATADESC( CASW_Info_Heal )
	DEFINE_OUTPUT( m_MarineHealed,	"MarineHealed" ),
END_DATADESC()

void CASW_Info_Heal::Spawn()
{
	BaseClass::Spawn();
	
	if ( ASWGameRules() )
	{
		ASWGameRules()->SetInfoHeal( this );
	}
}

void CASW_Info_Heal::OnMarineHealed( CASW_Marine *pHealer, CASW_Marine *pHealed, CASW_Weapon *pHealEquip )
{
	m_MarineHealed.FireOutput(pHealed, pHealer);
}

// -------------------------------------------------------------
// entity that hurts the nearest marine, but doesn't kill him
// -------------------------------------------------------------

LINK_ENTITY_TO_CLASS( asw_hurt_nearest_marine, CASW_Hurt_Nearest_Marine );

BEGIN_DATADESC( CASW_Hurt_Nearest_Marine )
	DEFINE_INPUTFUNC( FIELD_VOID,	"HurtMarine",	InputHurtMarine ),
END_DATADESC()

void CASW_Hurt_Nearest_Marine::InputHurtMarine( inputdata_t &inputdata )
{
	// find the nearest marine
	float distance = 0;
	CBaseEntity *pMarine = UTIL_ASW_NearestMarine( GetAbsOrigin(), distance );
	if ( pMarine )
	{
		if ( pMarine->GetHealth() > 35 )
		{
			pMarine->SetHealth( 35 );
		}

		CTakeDamageInfo info( this, this, Vector(0,0,0), GetAbsOrigin(), MIN( 5, pMarine->GetHealth() - 1 ), DMG_INFEST );
		pMarine->TakeDamage( info );
	}
	else
	{
		Msg("asw_hurt_nearest_marine failed to find a nearest marine\n");
	}
}

// -------------------------------------------------------------
// entity that sends an output when X marines are past an area
//   Requires two triggers, one that fires when a marine is behind us and one when the marine is past us.
// -------------------------------------------------------------

LINK_ENTITY_TO_CLASS( asw_marines_past_area, CASW_Marines_Past_Area );

BEGIN_DATADESC( CASW_Marines_Past_Area )
	DEFINE_KEYFIELD( m_iNumMarines, FIELD_INTEGER, "NumMarines" ),		

	DEFINE_OUTPUT( m_OutputMarinesPast,	"MarinesPast" ),

	DEFINE_INPUTFUNC( FIELD_VOID,	"MarineInFront",	InputMarineInFront ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"MarineBehind",		InputMarineBehind ),
END_DATADESC()

IMPLEMENT_AUTO_LIST( IASW_Marines_Past_Area_List );

CASW_Marines_Past_Area::CASW_Marines_Past_Area()
{
	m_MarinesPast.Purge();
}

void CASW_Marines_Past_Area::OnMarineKilled( CASW_Marine *pKilledMarine )
{
	int nMarineCount = 0;

	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( pGameResource )
	{
		for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource* pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;

			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( !pMarine )
				continue;

			if ( pMarine != pKilledMarine && pMarine->IsAlive() )
			{
				nMarineCount++;
			}
		}
	}

	if ( pKilledMarine && m_MarinesPast.Find( pKilledMarine ) != m_MarinesPast.InvalidIndex() )
	{
		m_MarinesPast.FindAndRemove( pKilledMarine );
	}

	if ( ( m_iNumMarines != -1 && m_MarinesPast.Count() >= m_iNumMarines ) || 
		 ( m_iNumMarines == -1 && nMarineCount > 0 && m_MarinesPast.Count() >= nMarineCount ) )
	{
		m_OutputMarinesPast.FireOutput( this, this );
	}
}

void CASW_Marines_Past_Area::InputMarineInFront( inputdata_t &inputdata )
{
	int nMarineCount = 0;

	bool bAddedMarine = false;
	CBaseTrigger *pTrigger = dynamic_cast<CBaseTrigger*>( inputdata.pCaller );
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( pTrigger && pGameResource)
	{
		// go through all touching marines from the caller
		for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource* pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;

			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( !pMarine )
				continue;

			if ( pMarine->IsAlive() )
			{
				nMarineCount++;
			}

			if ( pTrigger->IsTouching( pMarine ) )
			{
				if ( pMarine && m_MarinesPast.Find( pMarine ) == m_MarinesPast.InvalidIndex() )
				{
					m_MarinesPast.AddToTail( pMarine );
					bAddedMarine = true;
				}
			}
		}
	}
	else
	{
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( inputdata.pActivator );
		if ( pMarine && m_MarinesPast.Find( pMarine ) == m_MarinesPast.InvalidIndex() )
		{
			m_MarinesPast.AddToTail( pMarine );
			bAddedMarine = true;

			if ( ( m_iNumMarines != -1 && m_MarinesPast.Count() >= m_iNumMarines ) || 
				 ( m_iNumMarines == -1 && nMarineCount > 0 && m_MarinesPast.Count() >= nMarineCount ) )
			{
				m_OutputMarinesPast.FireOutput( pMarine, this );
			}
		}
	}

	if ( bAddedMarine && 
		 ( m_iNumMarines != -1 && m_MarinesPast.Count() >= m_iNumMarines ) || 
		 ( m_iNumMarines == -1 && nMarineCount > 0 && m_MarinesPast.Count() >= nMarineCount ) )
	{
		m_OutputMarinesPast.FireOutput( inputdata.pActivator, this );
	}
}

void CASW_Marines_Past_Area::InputMarineBehind( inputdata_t &inputdata )
{
	CBaseTrigger *pTrigger = dynamic_cast<CBaseTrigger*>( inputdata.pCaller );
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( pTrigger && pGameResource)
	{
		// go through all touching marines from the caller
		for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource* pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;

			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( !pMarine )
				continue;

			if ( pTrigger->IsTouching( pMarine ) )
			{
				if ( pMarine && m_MarinesPast.Find( pMarine ) != m_MarinesPast.InvalidIndex() )
				{
					m_MarinesPast.FindAndRemove( pMarine );
				}
			}
		}
	}
	else
	{
		m_MarinesPast.FindAndRemove( inputdata.pActivator );
	}
}

// ===========================

LINK_ENTITY_TO_CLASS( trigger_asw_marine_knockback, CASW_Marine_Knockback_Trigger );

BEGIN_DATADESC( CASW_Marine_Knockback_Trigger )	
	DEFINE_KEYFIELD( m_vecKnockbackDir, FIELD_VECTOR, "knockdir" ),

	// Function Pointers
	DEFINE_FUNCTION( KnockbackTriggerTouch ),

	// Outputs
	DEFINE_OUTPUT(m_OnKnockedBack, "OnKnockedBack")
END_DATADESC()

void CASW_Marine_Knockback_Trigger::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
	SetTouch( &CASW_Marine_Knockback_Trigger::KnockbackTriggerTouch );
}


//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Marine_Knockback_Trigger::KnockbackTriggerTouch(CBaseEntity *pOther)
{
	if ( PassesTriggerFilters(pOther) && pOther->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine *pMarine = assert_cast<CASW_Marine*>( pOther );
		if ( pMarine )
		{
			CASW_Melee_Attack *pAttack = pMarine->GetCurrentMeleeAttack();
			if ( !pAttack || !( pAttack->m_nAttackID == CASW_Melee_System::s_nKnockdownBackwardAttackID || pAttack->m_nAttackID == CASW_Melee_System::s_nKnockdownForwardAttackID ) )
			{
				if ( pMarine->GetForcedActionRequest() == 0 )
				{
					pMarine->Knockdown( this, m_vecKnockbackDir, true );
					m_hActivator = pOther;
					m_OnKnockedBack.FireOutput(pOther, this);
				}
			}
		}
	}
}