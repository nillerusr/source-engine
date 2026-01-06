//=============================================================================//
//
// Purpose:		Combine husks.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_husk_soldier.h"
#include "ammodef.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Husks generally use 90% of the original NPC's health

ConVar	sk_husk_soldier_health( "sk_husk_soldier_health", "90" ); // From 100
ConVar	sk_husk_soldier_kick( "sk_husk_soldier_kick", "10" );
ConVar	sk_husk_soldier_num_grenades( "sk_husk_soldier_num_grenades", "3" );

ConVar	sk_husk_elite_health( "sk_husk_elite_health", "125" ); // From 140
ConVar	sk_husk_elite_kick( "sk_husk_elite_kick", "15" );
ConVar	sk_husk_elite_num_grenades( "sk_husk_elite_num_grenades", "3" );

ConVar	sk_husk_grunt_health( "sk_husk_grunt_health", "70" ); // From 75
ConVar	sk_husk_grunt_kick( "sk_husk_grunt_kick", "10" );
ConVar	sk_husk_grunt_num_grenades( "sk_husk_grunt_num_grenades", "3" );

ConVar	sk_husk_ordinal_health( "sk_husk_ordinal_health", "80" ); // From 90
ConVar	sk_husk_ordinal_kick( "sk_husk_ordinal_kick", "10" );
ConVar	sk_husk_ordinal_num_grenades( "sk_husk_ordinal_num_grenades", "2" );

ConVar	sk_husk_charger_health( "sk_husk_charger_health", "110" ); // From 125
ConVar	sk_husk_charger_kick( "sk_husk_charger_kick", "20" );
ConVar	sk_husk_charger_num_grenades( "sk_husk_charger_num_grenades", "0" );
ConVar	sk_husk_charger_armor_hitgroup_scale( "sk_husk_charger_armor_hitgroup_scale", "0.75", FCVAR_NONE, "General hitgroup scale for husk chargers against default NPC hitgroup scales" );

ConVar	sk_husk_suppressor_health( "sk_husk_suppressor_health", "120" ); // From 140
ConVar	sk_husk_suppressor_kick( "sk_husk_suppressor_kick", "15" );
ConVar	sk_husk_suppressor_num_grenades( "sk_husk_suppressor_num_grenades", "0" );

//-----------------------------------------------------------------------------

static ConVar *g_HuskValues_Health[] =
{
	NULL, // Default, should never be used

	&sk_husk_soldier_health,
	&sk_husk_elite_health,

	&sk_husk_grunt_health,
	&sk_husk_ordinal_health,
	&sk_husk_charger_health,
	&sk_husk_suppressor_health,
};

static ConVar *g_HuskValues_Kick[] =
{
	NULL, // Default, should never be used

	&sk_husk_soldier_kick,
	&sk_husk_elite_kick,

	&sk_husk_grunt_kick,
	&sk_husk_ordinal_kick,
	&sk_husk_charger_kick,
	&sk_husk_suppressor_kick,
};

static ConVar *g_HuskValues_NumGrenades[] =
{
	NULL, // Default, should never be used

	&sk_husk_soldier_num_grenades,
	&sk_husk_elite_num_grenades,

	&sk_husk_grunt_num_grenades,
	&sk_husk_ordinal_num_grenades,
	&sk_husk_charger_num_grenades,
	&sk_husk_suppressor_num_grenades,
};

static const char *g_HuskValues_Labels[] =
{
	NULL, // Default, should never be used

	"soldier",
	"elite",

	"grunt",
	"ordinal",
	"charger",
	"suppressor",
};

static const char *g_HuskValues_DefaultModels[] =
{
	NULL, // Default, should never be used

	"models/husks/husk_soldier.mdl",
	"models/husks/husk_super_soldier.mdl",

	"models/husks/husk_grunt.mdl",
	"models/husks/husk_ordinal.mdl",
	"models/husks/husk_charger.mdl",
	"models/husks/husk_suppressor.mdl",
};

//-----------------------------------------------------------------------------

// This takes the model_t pointer directly because the husk entity most likely hasn't set its own model yet
void HuskSoldier_AscertainModelType( CBaseAnimating *pAnimating, const model_t *pModel, HuskSoldierType_t &tType )
{
	KeyValues *modelKeyValues = new KeyValues( "" );
	if (modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( pModel ), modelinfo->GetModelKeyValueText( pModel ) ))
	{
		KeyValues *pkvHuskData = modelKeyValues->FindKey( "husk_data" );
		if (pkvHuskData)
		{
			// Find soldier type
			const char *pszType = pkvHuskData->GetString( "type", "soldier" );

			for (int i = HUSK_SOLDIER; i < NUM_HUSK_SOLDIER_TYPES; i++)
			{
				if ( FStrEq( pszType, g_HuskValues_Labels[i] ) )
				{
					tType = (HuskSoldierType_t)i;
					break;
				}
			}
		}

		modelKeyValues->deleteThis();
	}

	// Fall back to HUSK_SOLDIER if needed
	if (tType == HUSK_DEFAULT)
	{
		Msg( "%s: Has default husk type but unrecognized model\n", pAnimating->GetDebugName() );
		tType = HUSK_SOLDIER;
	}
}

//-----------------------------------------------------------------------------

extern ConVar sk_plr_dmg_buckshot;
extern ConVar sk_plr_num_shotgun_pellets;

#define HUSK_HELMET_BODYGROUP_NORMAL 0
#define HUSK_HELMET_BODYGROUP_DAMAGED_RIGHT 1
#define HUSK_HELMET_BODYGROUP_DAMAGED_LEFT 2
#define HUSK_HELMET_BODYGROUP_DAMAGED_FULL 3

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_husk_soldier, CNPC_HuskSoldier );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HuskSoldier )

	DEFINE_KEYFIELD( m_tHuskSoldierType, FIELD_INTEGER, "HuskSoldierType" ),
	DEFINE_KEYFIELD( m_bNoDynamicDamage, FIELD_BOOLEAN, "NoDynamicDamage" ),

	DEFINE_OUTPUT( m_OnGiftAccept, "OnGiftAccept" ),
	DEFINE_OUTPUT( m_OnGiftReject, "OnGiftReject" ),

	DEFINE_BASE_HUSK_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// VScript
//---------------------------------------------------------
BEGIN_ENT_SCRIPTDESC( CNPC_HuskSoldier, CAI_BaseActor, "Husk Combine soldier." )

	DEFINE_BASE_HUSK_SCRIPTDESC()

END_SCRIPTDESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_HuskSoldier, DT_NPC_HuskSoldier )

	DEFINE_BASE_HUSK_SENDPROPS()

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HuskSoldier::CNPC_HuskSoldier()
{
	// Undo CNPC_Combine's npc_create flags
	RemoveSpawnFlags( /*SF_COMBINE_COMMANDABLE |*/ SF_COMBINE_REGENERATE );

	m_iNumGrenades = -1;
	m_bDontPickupWeapons = false;
	m_bLookForItems = true;

	m_tHuskSoldierType = HUSK_DEFAULT;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	Assert( m_tHuskSoldierType != HUSK_DEFAULT );

	SetHealth( g_HuskValues_Health[m_tHuskSoldierType]->GetInt() );
	SetMaxHealth( GetHealth() );
	SetKickDamage( g_HuskValues_Kick[m_tHuskSoldierType]->GetInt() );

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	BaseClass::Spawn();

	m_flFieldOfView = 0.5f;

	if (m_iNumGrenades == -1)
	{
		// Based on type
		m_iNumGrenades = g_HuskValues_NumGrenades[m_tHuskSoldierType]->GetInt();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::Precache()
{
	if (m_tHuskSoldierType > NUM_HUSK_SOLDIER_TYPES || m_tHuskSoldierType < HUSK_DEFAULT)
	{
		Warning( "%s has invalid soldier type (%i)\n", m_tHuskSoldierType );
		m_tHuskSoldierType = HUSK_SOLDIER;
	}

	if( !GetModelName() )
	{
		// Default models
		if (m_tHuskSoldierType == HUSK_DEFAULT)
		{
			SetModelName( MAKE_STRING( g_HuskValues_DefaultModels[HUSK_SOLDIER] ) );
		}
		else
		{
			SetModelName( MAKE_STRING( g_HuskValues_DefaultModels[m_tHuskSoldierType] ) );
		}
	}

	if (V_strncmp( STRING( GetModelName() ), "models/husks/husk_", 18 ) == 0)
	{
		const char *pszVariant = GetEZVariantString();
		if (pszVariant != NULL)
		{
			// If this is a default model, reassign it to the relevant E:Z variant
			char szNewModelName[MAX_PATH];
			V_snprintf( szNewModelName, sizeof( szNewModelName ), "models/husks/%s/%s", pszVariant, STRING( GetModelName() ) + 13 );

			SetModelName( AllocPooledString( szNewModelName ) );
		}
	}

	int nModel = PrecacheModel( STRING( GetModelName() ) );

	if (m_tHuskSoldierType == HUSK_DEFAULT)
	{
		// Ascertain type from model
		HuskSoldier_AscertainModelType( this, modelinfo->GetModel( nModel ), m_tHuskSoldierType);
	}

	if (m_tHuskSoldierType == HUSK_ELITE)
	{
		m_fIsElite = true;
	}
	else
	{
		m_fIsElite = false;
	}

	switch (m_tHuskSoldierType)
	{
		case HUSK_CHARGER:
			PrecacheScriptSound( "NPC_HuskCharger.WeaponBash" );
			// Fall through

		case HUSK_GRUNT:
		case HUSK_ORDINAL:
			PrecacheScriptSound( "NPC_HuskSoldier.HelmetBreak" );
	}

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "weapon_frag" );
	UTIL_PrecacheOther( "item_ammo_ar2_altfire" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "husk_soldier_type", UTIL_VarArgs("%i", m_tHuskSoldierType) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::GetGameTextSpeechParams( hudtextparms_t &params )
{
	switch (m_tHuskSoldierType)
	{
		default:
		case HUSK_SOLDIER:
		case HUSK_ELITE:
			if (GetEZVariant() == EZ_VARIANT_ATHENAEUM)
			{
				params.r1 = 72; params.g1 = 64; params.b1 = 128;
			}
			else
			{
				params.r1 = 128; params.g1 = 96; params.b1 = 64;
			}
			break;

		case HUSK_GRUNT:
			params.r1 = 128; params.g1 = 112; params.b1 = 48;
			break;

		case HUSK_ORDINAL:
			params.r1 = 128; params.g1 = 80; params.b1 = 48;
			break;

		case HUSK_CHARGER:
			params.r1 = 48; params.g1 = 96; params.b1 = 128;
			break;

		case HUSK_SUPPRESSOR:
			params.r1 = 160; params.g1 = 112; params.b1 = 72;
			break;
	}
	
	return BaseClass::GetGameTextSpeechParams( params );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline const char *CNPC_HuskSoldier::GetEZVariantString()
{
	switch ( GetEZVariant() )
	{
		default:
		case EZ_VARIANT_DEFAULT:	return NULL;
		case EZ_VARIANT_XEN:		return "xen";
		case EZ_VARIANT_RAD:		return "rad";
		case EZ_VARIANT_TEMPORAL:	return "temporal";
		case EZ_VARIANT_ARBEIT:		return "arbeit";
		case EZ_VARIANT_BLOODLION:	return "blood";
		case EZ_VARIANT_ATHENAEUM:	return "athenaeum";
		case EZ_VARIANT_ASH:		return "ash";
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	BaseClass::Use( pActivator, pCaller, useType, value );

	Disposition_t relation = IRelationType( pActivator );
	if ( (relation == D_HT || relation == D_FR) && !FInViewCone( pActivator ) )
	{
		// Don't like being used withoutwarning
		MakeAngry( pActivator );
		UpdateEnemyMemory( pActivator, pActivator->GetAbsOrigin(), pActivator );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::DeathSound( const CTakeDamageInfo &info )
{
	AI_CriteriaSet set;
	ModifyOrAppendDamageCriteria(set, info);
	SpeakIfAllowed( TLK_CMB_DIE, set, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::BashSound()
{
	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		EmitSound( "NPC_HuskCharger.WeaponBash" );
		return;
	}

	BaseClass::BashSound();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_HuskSoldier::GetHostileRadius()
{
	if (HasCondition(COND_TALKER_PLAYER_STARING) && GetEnemy() && GetEnemy()->IsPlayer())
	{
		// Pause if the player is trying to give me something
		CBaseEntity *pHeld = GetPlayerHeldEntity( ToBasePlayer( GetEnemy() ) );
		if (pHeld && pHeld->IsBaseCombatWeapon() && pHeld->IsCombatItem())
			return 0.0f;
	}

	return BaseClass::GetHostileRadius();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::IsCommandable()
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (pPlayer && IRelationType( pPlayer ) == D_LI)
	{
		// Only commandable if we like the player (i.e. have been pacified)
		return BaseClass::IsCommandable();
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_HuskSoldier::ShouldDelayEscalation()
{
	if (HasCondition(COND_TALKER_PLAYER_STARING) && GetEnemy() && GetEnemy()->IsPlayer())
	{
		// Pause if the player is trying to give me something
		CBaseEntity *pHeld = GetPlayerHeldEntity( ToBasePlayer( GetEnemy() ) );
		if (pHeld && pHeld->IsBaseCombatWeapon() && pHeld->IsCombatItem())
			return 10.0f;
	}

	return 0.0f;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CNPC_HuskSoldier::PickupItem( CBaseEntity *pItem )
{
	BaseClass::PickupItem( pItem );

	// Pretend we're picking this up and reloading our weapon with it
	if (FClassnameIs( pItem, "item_ammo*" ))
	{
		UTIL_Remove( pItem );
		if (GetActiveWeapon())
		{
			GetActiveWeapon()->m_iClip1 = 0;
			SetCondition( COND_NO_PRIMARY_AMMO );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::IsGiveableWeapon( CBaseCombatWeapon *pWeapon )
{
	if (!IsCommandable() && GetActiveWeapon())
	{
		// Husks won't accept the same weapon
		if (GetActiveWeapon()->GetClassname() == pWeapon->GetClassname())
			return false;

		// Husks won't accept weapons worse than what they have
		if (GetActiveWeapon()->GetWeight() > pWeapon->GetWeight())
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::IsGiveableItem( CBaseEntity *pItem )
{
	if (!BaseClass::IsGiveableItem( pItem ))
	{
		if (pItem->IsCombatItem())
		{
			// Normally, soldiers refuse to take health kits when they don't need the health.
			// However, husks should always accept health kits. Even if they're not technically hurt,
			// their decrepit state should give off the impression that they are.
			// TODO: Is giving health kits to any husk OP?
			if (pItem->ClassMatches( "item_health*" ) || pItem->ClassMatches( "item_battery" ))
				return true;

			// Allow players to give us ammo corresponding to the weapon we're holding
			if (pItem->ClassMatches( "item_ammo*" ) && GetActiveWeapon() && GetActiveWeapon()->GetPrimaryAmmoType() != -1)
			{
				// Since ammo items have no header nor any way of qualifying their ammo type by default,
				// we have to guess
				const char *pszAmmoName = GetAmmoDef()->Name( GetActiveWeapon()->GetPrimaryAmmoType() );
				if (!pszAmmoName)
					return false;

				// Handle immediate cases where item type does not match up with item class
				if (FStrEq(pszAmmoName, "XbowBolt"))
					pszAmmoName = "crossbow";
				else if (FStrEq(pszAmmoName, "RPG_Round"))
					pszAmmoName = "rpg";
				else if (FStrEq( pszAmmoName, "Buckshot" ))
				{
					// This uses the old classname nomenclature
					// (If the other item_box_ classes are ever restored, expand this)
					return pItem->ClassMatches( "item_box_buckshot" );
				}

				char szItemClassname[128];
				Q_snprintf( szItemClassname, sizeof( szItemClassname ), "item_ammo_%s*", pszAmmoName );

				return pItem->ClassMatches( szItemClassname );
			}
		}

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::StartPlayerGive( CBasePlayer *pPlayer )
{
	if ((GetTarget()->ClassMatches("item_health*") || GetTarget()->ClassMatches("item_battery")) && GetHealth() >= GetMaxHealth())
	{
		// HACKHACK: This will make sure the medkit is properly removed
		SetHealth( GetMaxHealth() - 1 );
	}

	// We like this person now
	AddPassiveTarget( pPlayer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::OnCantBeGivenObject( CBaseEntity *pItem )
{
	if (!IsCurSchedule( SCHED_HUSK_SOLDIER_REJECT_GIFT ))
	{
		// Just punch it out of their hands if we don't want it
		SetTarget( pItem );
		SetSchedule( SCHED_HUSK_SOLDIER_REJECT_GIFT );

		m_OnGiftReject.FireOutput( pItem, this );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CNPC_HuskSoldier::GetBackupWeaponClass()
{
	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		return "weapon_stunstick";
	}

	// We only have what we have
	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::RemovePassiveTarget( CBaseEntity *pTarget )
{
	if (!BaseClass::RemovePassiveTarget( pTarget ))
		return false;

	if ( pTarget->IsPlayer() && IsInPlayerSquad() && IRelationType( pTarget ) <= D_FR )
	{
		RemoveFromPlayerSquad();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_HuskSoldier::SelectSchedule( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CNPC_HuskSoldier::SelectCombatSchedule( void )
{
	if ( IsSuspicious() )
	{
		// Only face the enemy
		return SCHED_COMBAT_FACE;
	}

	return BaseClass::SelectCombatSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_HuskSoldier::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	float flBase = BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );

	switch( iHitGroup )
	{
	case HITGROUP_HEAD:
		{
			// Soldiers take double headshot damage
			flBase = 2.0f;
		}
	}

	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		switch (iHitGroup)
		{
		case HITGROUP_HEAD:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			{
				flBase *= sk_husk_charger_armor_hitgroup_scale.GetFloat();
			}
		}
	}

	return flBase;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case 3: // COMBINE_AE_KICK
			{
				if (IsCurSchedule(SCHED_HUSK_SOLDIER_REJECT_GIFT) && GetEnemy() && GetEnemy()->IsPlayer())
				{
					// Punt away whatever weak thing the player is trying to give me
					CBaseEntity *pHeld = GetPlayerHeldEntity( ToBasePlayer( GetEnemy() ) );
					if (pHeld)
					{
						Pickup_ForcePlayerToDropThisObject( pHeld );

						Vector forward, right, up;
						AngleVectors( GetLocalAngles(), &forward, &right, &up );
						pHeld->ApplyAbsVelocityImpulse( forward * 50 + (right * -1.0f) * 100 + up * 100  );

						AngularImpulse angVelocity = RandomAngularImpulse( -150, 150 );
						pHeld->ApplyLocalAngularVelocityImpulse( angVelocity );

						BashSound();
					}
				}
				else
				{
					BaseClass::HandleAnimEvent( pEvent );
				}
				break;
			}

		default:
		BaseClass::HandleAnimEvent( pEvent );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskSoldier::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	// Husk soldiers can have their helmets damaged
	if (ptr->hitgroup == HITGROUP_HEAD && (IsHeavyDamage( info ) || RandomInt(1,3) == 1) && !m_bNoDynamicDamage)
	{
		int nHelmetBody = FindBodygroupByName( "helmet" );
		if ((m_tHuskSoldierType == HUSK_GRUNT || m_tHuskSoldierType == HUSK_ORDINAL || m_tHuskSoldierType == HUSK_CHARGER) && nHelmetBody != -1)
		{
			int nCurrentBody = GetBodygroup( nHelmetBody );
			int nNewBody = nCurrentBody;

			if ( nCurrentBody != HUSK_HELMET_BODYGROUP_DAMAGED_FULL )
			{
				int nEyeShot = TestEyeShot( ptr->endpos );

				if (nEyeShot == -1) // Left
				{
					if (nCurrentBody == HUSK_HELMET_BODYGROUP_NORMAL)
						nNewBody = HUSK_HELMET_BODYGROUP_DAMAGED_LEFT;
					else if (nCurrentBody == HUSK_HELMET_BODYGROUP_DAMAGED_RIGHT)
						nNewBody = HUSK_HELMET_BODYGROUP_DAMAGED_FULL;
				}
				else if (nEyeShot == 1) // Right
				{
					if (nCurrentBody == HUSK_HELMET_BODYGROUP_NORMAL)
						nNewBody = HUSK_HELMET_BODYGROUP_DAMAGED_RIGHT;
					else if (nCurrentBody == HUSK_HELMET_BODYGROUP_DAMAGED_LEFT)
						nNewBody = HUSK_HELMET_BODYGROUP_DAMAGED_FULL;
				}
				else if (info.GetDamage() >= GetMaxHealth())
				{
					// Helmet was obliterated anyway
					nNewBody = HUSK_HELMET_BODYGROUP_DAMAGED_FULL;
				}
			}

			if (nNewBody != nCurrentBody)
			{
				SetBodygroup( nHelmetBody, nNewBody );
				EmitSound( "NPC_HuskSoldier.HelmetBreak" );

				// TODO: Particle effect for this
				Vector vecSparkDir = ptr->startpos - ptr->endpos;
				g_pEffects->Sparks( ptr->endpos, 1, 5, &vecSparkDir );

				if (nNewBody == HUSK_HELMET_BODYGROUP_DAMAGED_FULL || RandomInt(1,4) == 1)
				{
					// Chance of going blind
					AddCognitionFlags( bits_HUSK_COGNITION_BLIND );
				}
			}
		}
		else
		{
			// Just go blind if an eye was hit
			//if (RandomInt(1,4) == 1)
			{
				int nEyeShot = TestEyeShot( ptr->endpos );
				if (nEyeShot != 0)
					AddCognitionFlags( bits_HUSK_COGNITION_BLIND );
			}
		}

		// Hitting the ears within 3 units can cause deafness
		if (RandomInt( 1, 4 ) == 1 && !HasCognitionFlags(bits_HUSK_COGNITION_DEAF))
		{
			Vector vecRightEarOrigin, vecLeftEarOrigin;
			GetAttachment( LookupAttachment( "ear_r" ), vecRightEarOrigin );
			GetAttachment( LookupAttachment( "ear_l" ), vecLeftEarOrigin );

			if ((ptr->endpos - vecLeftEarOrigin).LengthSqr() <= 3.0f || (ptr->endpos - vecRightEarOrigin).LengthSqr() <= 3.0f)
			{
				AddCognitionFlags( bits_HUSK_COGNITION_DEAF );
			}
		}
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
// Purpose: Tests which eye a bullet hit (-1 for left, 1 for right, 0 for none)
//-----------------------------------------------------------------------------
int CNPC_HuskSoldier::TestEyeShot( const Vector &vecShotPos )
{
	Vector vecRightEyeOrigin, vecLeftEyeOrigin;
	GetAttachment( LookupAttachment( "eye_r" ), vecRightEyeOrigin );
	GetAttachment( LookupAttachment( "eye_l" ), vecLeftEyeOrigin );

	float flRightEyeDistSqr = (vecShotPos - vecRightEyeOrigin).LengthSqr();
	float flLeftEyeDistSqr = (vecShotPos - vecLeftEyeOrigin).LengthSqr();

	if (flLeftEyeDistSqr < flRightEyeDistSqr && flLeftEyeDistSqr <= Square( 8.0f )) // Left
	{
		return -1;
	}
	else if (flRightEyeDistSqr < flLeftEyeDistSqr && flRightEyeDistSqr <= Square( 8.0f )) // Right
	{
		return 1;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	// Heavy damage when hit directly by shotgun flechettes
	if ( info.GetDamageType() & (DMG_DISSOLVE | DMG_NEVERGIB) && info.GetInflictor() && FClassnameIs( info.GetInflictor(), "shotgun_flechette" ) )
		return true;

	return BaseClass::IsHeavyDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HuskSoldier::Crouch( void )
{
	// Chargers don't crouch
	if (m_tHuskSoldierType == HUSK_CHARGER /*|| m_tHuskSoldierType == HUSK_SUPPRESSOR*/)
		return false;

	return BaseClass::Crouch();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_HuskSoldier::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if( pWeapon->ClassMatches( gm_isz_class_AR2 ) )
	{
		return WEAPON_PROFICIENCY_GOOD; // From WEAPON_PROFICIENCY_VERY_GOOD
	}
	else if( pWeapon->ClassMatches( gm_isz_class_Shotgun ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD; // From WEAPON_PROFICIENCY_PERFECT
	}
	else if( pWeapon->ClassMatches( gm_isz_class_SMG1 ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
	else if ( pWeapon->ClassMatches( gm_isz_class_Pistol ) )
	{
		return WEAPON_PROFICIENCY_GOOD; // From WEAPON_PROFICIENCY_VERY_GOOD
	}
	else if (FClassnameIs( pWeapon, "weapon_smg2" ))
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	return WEAPON_PROFICIENCY_GOOD; // Override base
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_HuskSoldier::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	return BaseClass::Weapon_TranslateActivity( baseAct, pRequired );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_HuskSoldier::NPC_TranslateActivity( Activity eNewActivity )
{
	// Chargers prefer to walk unless running from danger
	if (m_tHuskSoldierType == HUSK_CHARGER && (!HasCondition( COND_HEAR_DANGER ) && !HasCondition( COND_HEAVY_DAMAGE )))
	{
		if (eNewActivity == ACT_RUN_AIM /*&& GetActiveWeapon() && !GetActiveWeapon()->IsMeleeWeapon()*/)
		{
			eNewActivity = ACT_WALK_AIM;
		}
		else if (eNewActivity == ACT_RUN)
		{
			if (GetNavigator()->GetPathDistanceToGoal() < 192.0f)
				eNewActivity = ACT_WALK;
		}
	}

	eNewActivity = BaseClass::NPC_TranslateActivity( eNewActivity );

	if (eNewActivity == ACT_IDLE_ANGRY && !IsAngry() && m_NPCState == NPC_STATE_ALERT)
	{
		// Don't aim when alert but not angry
		eNewActivity = ACT_IDLE;
	}

	return eNewActivity;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_husk_soldier, CNPC_HuskSoldier )

 //=========================================================
 // Soldier attacks a prop
 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_HUSK_SOLDIER_REJECT_GIFT,
 
 	"	Tasks"
 	//"		TASK_GET_PATH_TO_TARGET		0"
 	//"		TASK_MOVE_TO_TARGET_RANGE	50"
 	"		TASK_STOP_MOVING			0"
 	"		TASK_FACE_TARGET			0"
 	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_MELEE_ATTACK1"
 	"	"
 	"	Interrupts"
 	"		COND_NEW_ENEMY"
 	"		COND_ENEMY_DEAD"
 )

 AI_END_CUSTOM_NPC()
