#include "cbase.h"
#include "props.h"
#include "asw_sentry_base.h"
#include "asw_sentry_top.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_skills.h"
#include "asw_marine_speech.h"
#include "asw_marine_resource.h"
#include "world.h"
#include "asw_util_shared.h"
#include "asw_fx_shared.h"
#include "asw_gamerules.h"
#include "asw_weapon_sentry_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SENTRY_BASE_MODEL "models/sentry_gun/sentry_base.mdl"
//#define SENTRY_BASE_MODEL "models/swarm/droneprops/DronePropIdle.mdl"

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud


ConVar asw_sentry_gun_type("asw_sentry_gun_type", "-1", FCVAR_CHEAT, "Force the type of sentry guns built to this. -1, the default, reads from the marine attributes.");
ConVar asw_sentry_infinite_ammo( "asw_sentry_infinite_ammo", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

LINK_ENTITY_TO_CLASS( asw_sentry_base, CASW_Sentry_Base );
PRECACHE_REGISTER( asw_sentry_base );

IMPLEMENT_SERVERCLASS_ST(CASW_Sentry_Base, DT_ASW_Sentry_Base)
	SendPropBool(SENDINFO(m_bAssembled)),
	SendPropBool(SENDINFO(m_bIsInUse)),
	SendPropFloat(SENDINFO(m_fAssembleProgress)),
	SendPropFloat(SENDINFO(m_fAssembleCompleteTime)),
	SendPropInt(SENDINFO(m_iAmmo)),	
	SendPropInt(SENDINFO(m_iMaxAmmo)),	
	SendPropBool(SENDINFO(m_bSkillMarineHelping)),
	SendPropInt(SENDINFO(m_nGunType)),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Sentry_Base )
	DEFINE_THINKFUNC( AnimThink ),
	DEFINE_FIELD( m_hSentryTop, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bAssembled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsInUse, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fAssembleProgress, FIELD_FLOAT ),
	DEFINE_FIELD( m_fAssembleCompleteTime, FIELD_TIME ),
	DEFINE_FIELD( m_hDeployer, FIELD_EHANDLE ),
END_DATADESC()


CASW_Sentry_Base::CASW_Sentry_Base()
{
	m_iAmmo = -1;
	m_fSkillMarineHelping = 0;
	m_bSkillMarineHelping = false;
	m_fDamageScale = 1.0f;
	m_nGunType = kAUTOGUN;
	m_bAlreadyTaken = false;
}


CASW_Sentry_Base::~CASW_Sentry_Base()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Sentry_Base::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );

	SetSolid( SOLID_BBOX );
	SetCollisionGroup( ASW_COLLISION_GROUP_SENTRY );

	Precache();
	SetModel(SENTRY_BASE_MODEL);

	BaseClass::Spawn();

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );	

	SetCollisionBounds( Vector(-26,-26,0), Vector(26,26,60));

	SetMaxHealth(300);
	SetHealth(300);
	m_takedamage = DAMAGE_YES;

	SetThink( &CASW_Sentry_Base::AnimThink );	
	SetNextThink( gpGlobals->curtime + 0.1f );

	// check for attaching to elevators
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector(0, 0, 2),
					GetAbsOrigin() - Vector(0, 0, 32), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f && tr.m_pEnt && !tr.m_pEnt->IsWorld() && !tr.m_pEnt->IsNPC() )
	{
		SetParent( tr.m_pEnt );
	}

	if ( m_iAmmo == -1 )
	{
		m_iAmmo = GetBaseAmmoForGunType( GetGunType() );
	}
	m_iMaxAmmo = m_iAmmo;
}

void CASW_Sentry_Base::PlayDeploySound()
{
	EmitSound("ASW_Sentry.Deploy");
}

void CASW_Sentry_Base::Precache()
{
	PrecacheModel( SENTRY_BASE_MODEL );
	PrecacheScriptSound( "ASW_Sentry.SetupLoop" );
	PrecacheScriptSound( "ASW_Sentry.SetupInterrupt" );
	PrecacheScriptSound( "ASW_Sentry.SetupComplete" );
	PrecacheScriptSound( "ASW_Sentry.Dismantled" );

	BaseClass::Precache();
}


int CASW_Sentry_Base::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Sentry_Base::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CASW_Sentry_Base::AnimThink( void )
{		
	if (!m_bAssembled)
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	
		StudioFrameAdvance();

		m_bSkillMarineHelping = (m_fSkillMarineHelping >= gpGlobals->curtime - 0.2f);
	}
	else
	{
		m_bSkillMarineHelping = false;
	}
}

// player has used this item
void CASW_Sentry_Base::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if (!m_bIsInUse && !m_bAssembled && nHoldType != ASW_USE_HOLD_RELEASE_FULL)
	{
		pMarine->StartUsing(this);
		pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);
	}
	else if (m_bAssembled && GetSentryTop())
	{
		if ( nHoldType == ASW_USE_HOLD_START )
		{
			pMarine->StartUsing(this);
			pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);
		}
		else if ( nHoldType == ASW_USE_HOLD_RELEASE_FULL )
		{
			pMarine->StopUsing();

			if ( !m_bAlreadyTaken )
			{			
				//Msg( "Disassembling sentry gun!\n" );
				IGameEvent * event = gameeventmanager->CreateEvent( "sentry_dismantled" );
				if ( event )
				{
					CBasePlayer *pPlayer = pMarine->GetCommander();
					event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
					event->SetInt( "entindex", entindex() );

					gameeventmanager->FireEvent( event );
				}

				CASW_Weapon_Sentry *pWeapon = dynamic_cast<CASW_Weapon_Sentry*>( Create( GetWeaponNameForGunType( GetGunType() ), WorldSpaceCenter(), GetAbsAngles(), NULL ) );
				pWeapon->SetSentryAmmo( m_iAmmo );
				pMarine->TakeWeaponPickup( pWeapon );
				EmitSound( "ASW_Sentry.Dismantled" );
				UTIL_Remove( this );
				m_bAlreadyTaken = true;
			}

			// TODO: just have the marine pick it up now and let that logic deal with the slot?

			// TODO: Find an empty inv slot. Or default to 2nd.
			//       Drop whatever's in that slot currently
			//		 Create a new sentry gun weapon with our ammo amount and give it to the marine
			//		 Destroy ourselves
		}
		else if ( nHoldType == ASW_USE_RELEASE_QUICK )
		{
			pMarine->StopUsing();

			pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);

			IGameEvent * event = gameeventmanager->CreateEvent( "sentry_rotated" );
			if ( event )
			{
				CBasePlayer *pPlayer = pMarine->GetCommander();
				event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
				event->SetInt( "entindex", entindex() );

				gameeventmanager->FireEvent( event );
			}

			// tell the top piece to turn to face the same way as pMarine is facing
			GetSentryTop()->SetDeployYaw(pMarine->ASWEyeAngles().y);
			GetSentryTop()->PlayTurnSound();
		}
	}
}

#define SENTRY_ASSEMBLE_TIME 7.0f			// was 14.0
void CASW_Sentry_Base::MarineUsing(CASW_Marine* pMarine, float deltatime)
{
	if (m_bIsInUse && !m_bAssembled && pMarine)
	{
		// check if any techs nearby have engineering skill to speed this up
		float fSkillScale = MarineSkills()->GetHighestSkillValueNearby(pMarine->GetAbsOrigin(), ENGINEERING_AURA_RADIUS,
				ASW_MARINE_SKILL_ENGINEERING, ASW_MARINE_SUBSKILL_ENGINEERING_SENTRY);
		CASW_Marine *pSkillMarine = MarineSkills()->GetLastSkillMarine();		
		if ( fSkillScale> 0.0f && pSkillMarine && pSkillMarine->GetMarineResource())
		{
			pSkillMarine->m_fUsingEngineeringAura = gpGlobals->curtime;
			m_fSkillMarineHelping = gpGlobals->curtime;
		}
		else
		{
			m_fSkillMarineHelping = 0;
		}
		if (fSkillScale < 1.0)
			fSkillScale = 1.0f;
		float fSetupAmount = (deltatime * (1.0f/SENTRY_ASSEMBLE_TIME)) * fSkillScale;
		m_fAssembleProgress += fSetupAmount;
		if (m_fAssembleProgress >= 1.0f)
		{
			m_fAssembleProgress = 1.0f;
			
			pMarine->StopUsing();
			m_bAssembled = true;
			m_fAssembleCompleteTime = gpGlobals->curtime;
			pMarine->GetMarineSpeech()->Chatter(CHATTER_SENTRY);
			// spawn top half and activate it
			CASW_Sentry_Top * RESTRICT  pSentryTop = dynamic_cast<CASW_Sentry_Top*>( CreateEntityByName( GetEntityNameForGunType( GetGunType() ) ) );	
			m_hSentryTop = pSentryTop;
			if ( pSentryTop )
			{
				pSentryTop->SetSentryBase( this );

				const QAngle &angles = GetAbsAngles();
				pSentryTop->SetAbsAngles( angles );
				DispatchSpawn( pSentryTop );				
			}
		}
	}
}

void CASW_Sentry_Base::MarineStartedUsing(CASW_Marine* pMarine)
{
	EmitSound( "ASW_Sentry.SetupLoop" );

	if ( !m_bIsInUse && m_fAssembleProgress < 1.0f )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "sentry_start_building" );
		if ( event )
		{
			CBasePlayer *pPlayer = pMarine->GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", entindex() );

			gameeventmanager->FireEvent( event );
		}
	}

	m_bIsInUse = true;
}

void CASW_Sentry_Base::MarineStoppedUsing(CASW_Marine* pMarine)
{
	if ( m_fAssembleProgress >= 1.0f )
	{
		EmitSound( "ASW_Sentry.SetupComplete" );

		IGameEvent * event = gameeventmanager->CreateEvent( "sentry_complete" );
		if ( event )
		{
			CBasePlayer *pPlayer = pMarine->GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", entindex() );
			event->SetInt( "marine", pMarine->entindex() );

			gameeventmanager->FireEvent( event );
		}
	}
	else
	{
		EmitSound( "ASW_Sentry.SetupInterrupt" );

		IGameEvent * event = gameeventmanager->CreateEvent( "sentry_stop_building" );
		if ( event )
		{
			CBasePlayer *pPlayer = pMarine->GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", entindex() );

			gameeventmanager->FireEvent( event );
		}
	}
	m_bIsInUse = false;
}

CASW_Sentry_Top* CASW_Sentry_Base::GetSentryTop()
{	
	return assert_cast<CASW_Sentry_Top*>(m_hSentryTop.Get());
}

bool CASW_Sentry_Base::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

void CASW_Sentry_Base::OnFiredShots( int nNumShots )
{
	if ( GetSentryTop() )
	{
		int nThreeQuarterAmmo = GetBaseAmmoForGunType( GetGunType() ) * 0.75f;
		int nLowAmmo = GetBaseAmmoForGunType( GetGunType() ) / 5;

		if ( m_iAmmo <= 0 )
		{
			GetSentryTop()->OnOutOfAmmo();
		}
		else if ( m_iAmmo <= nLowAmmo )
		{
			GetSentryTop()->OnLowAmmo();
		}
		else if ( m_iAmmo <= nThreeQuarterAmmo )
		{
			GetSentryTop()->OnUsedQuarterAmmo();
		}

		if( m_hDeployer )
			m_hDeployer->OnWeaponFired( GetSentryTop(), nNumShots );
	}

	if ( !asw_sentry_infinite_ammo.GetBool() )
		m_iAmmo -= nNumShots;
}

int CASW_Sentry_Base::OnTakeDamage( const CTakeDamageInfo &info )
{
	// no friendly fire damage 
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( pAttacker && pAttacker->Classify() == CLASS_ASW_MARINE )
	{
		return 0;
	}
	else
	{
		return BaseClass::OnTakeDamage(info);
	}
}

// explode if we die
void CASW_Sentry_Base::Event_Killed( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;

	// explosion effect
	Vector vecPos = GetAbsOrigin() + Vector(0, 0, 30);
				
	trace_t		tr;
	UTIL_TraceLine ( vecPos, vecPos - Vector(0,0, 60), MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	UTIL_ASW_ScreenShake( vecPos, 25.0, 150.0, 1.0, 750, SHAKE_START );

	UTIL_ASW_GrenadeExplosion( vecPos, 400.0f );

	EmitSound( "ASWGrenade.Explode" );

	// damage to nearby things
	ASWGameRules()->RadiusDamage ( CTakeDamageInfo( this, info.GetAttacker(), 150.0f, DMG_BLAST ), vecPos, 400.0f, CLASS_NONE, NULL );

	if (GetSentryTop())
	{
		UTIL_Remove(GetSentryTop());
	}

	BaseClass::Event_Killed(info);
}


const char *CASW_Sentry_Base::GetEntityNameForGunType( GunType_t guntype )
{
	AssertMsg1( static_cast<int>(guntype) >= 0, "Faulty guntype %d passed to CASW_Sentry_Base::GetEntityNameForGunType()\n", guntype );
	if ( guntype < kGUNTYPE_MAX )
	{
		return sm_gunTypeToInfo[guntype].m_entityName;
	}
	else
	{
		Warning( "GetEntityNameForGunType called with unsupported GunType_t %d .. defaulting to machine gun.\n", guntype );
		return sm_gunTypeToInfo[kAUTOGUN].m_entityName;
	}
}

const char *CASW_Sentry_Base::GetWeaponNameForGunType( GunType_t guntype )
{
	AssertMsg1( static_cast<int>(guntype) >= 0, "Faulty guntype %d passed to CASW_Sentry_Base::GetWeaponNameForGunType()\n", guntype );
	if ( guntype < kGUNTYPE_MAX )
	{
		return sm_gunTypeToInfo[guntype].m_weaponName;
	}
	else
	{
		Warning( "GetWeaponNameForGunType called with unsupported GunType_t %d .. defaulting to machine gun.\n", guntype );
		return sm_gunTypeToInfo[kAUTOGUN].m_weaponName;
	}
}

int CASW_Sentry_Base::GetBaseAmmoForGunType( GunType_t guntype )
{
	AssertMsg1( static_cast<int>(guntype) >= 0, "Faulty guntype %d passed to CASW_Sentry_Base::GetBaseAmmoForGunType()\n", guntype );
	if ( guntype < kGUNTYPE_MAX )
	{
		return sm_gunTypeToInfo[guntype].m_nBaseAmmo;
	}
	else
	{
		Warning( "GetBaseAmmoForGunType called with unsupported GunType_t %d .. defaulting to machine gun.\n", guntype );
		return sm_gunTypeToInfo[kAUTOGUN].m_nBaseAmmo;
	}
}

CASW_Sentry_Base::GunType_t CASW_Sentry_Base::GetGunType( void ) const
{
	// read the cvar
	int nCvarGunType = asw_sentry_gun_type.GetInt();
	if ( nCvarGunType >= 0 )
	{
		return static_cast<CASW_Sentry_Base::GunType_t>(nCvarGunType);
	}
	else
	{
		return (GunType_t) m_nGunType.Get();
	}
}


/// This must exactly match the enum CASW_Sentry_Base::GunType_t
const CASW_Sentry_Base::SentryGunTypeInfo_t CASW_Sentry_Base::sm_gunTypeToInfo[CASW_Sentry_Base::kGUNTYPE_MAX] =
{
	SentryGunTypeInfo_t("asw_sentry_top_machinegun", "asw_weapon_sentry", 450), // kAUTOGUN
	SentryGunTypeInfo_t("asw_sentry_top_cannon", "asw_weapon_sentry_cannon",	  25), // kCANNON
	SentryGunTypeInfo_t("asw_sentry_top_flamer", "asw_weapon_sentry_flamer",  800), // kFLAME (ammo is stored as milliseconds of fire)
	SentryGunTypeInfo_t("asw_sentry_top_icer", "asw_weapon_sentry_freeze",    800), // kICE
};