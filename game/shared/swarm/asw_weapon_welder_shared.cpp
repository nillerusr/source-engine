#include "cbase.h"
#include "in_buttons.h"
#include "soundenvelope.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_door_area.h"
#include "c_asw_door.h"
#include "c_asw_marine_resource.h"
#include "FX.h"
#define CASW_Marine C_ASW_Marine
#define CASW_Door_Area C_ASW_Door_Area
#define CASW_Door C_ASW_Door
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_door_area.h"
#include "asw_door.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_gamerules.h"
#include "asw_marine_resource.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_welder_shared.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Welder, DT_ASW_Weapon_Welder )

BEGIN_NETWORK_TABLE( CASW_Weapon_Welder, DT_ASW_Weapon_Welder )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropBool		(RECVINFO(m_bWeldSeal)),	
#else
	// sendprops
	SendPropBool		(SENDINFO(m_bWeldSeal)),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Welder )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_welder, CASW_Weapon_Welder );
PRECACHE_WEAPON_REGISTER(asw_weapon_welder);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Welder )
	DEFINE_FIELD( m_iAutomaticWeldDirection, FIELD_INTEGER ),
END_DATADESC()

#endif /* not client */

CASW_Weapon_Welder::CASW_Weapon_Welder()
{
	m_fWeldTime = 0;
	m_bWeldSeal = false;
	m_pWeldDoor = NULL;
	m_pWeldingSound = NULL;
	m_bPlayingWelderSound = false;

#ifdef CLIENT_DLL
	m_bWeldSealLast = false;
#endif
}


CASW_Weapon_Welder::~CASW_Weapon_Welder()
{
	if ( m_pWeldingSound )
	{
		CSoundEnvelopeController::GetController().Shutdown(m_pWeldingSound);
	}

#ifdef CLIENT_DLL
	if ( m_hWeldEffects )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hWeldEffects );
		m_hWeldEffects = NULL;
	}
#endif
}

void CASW_Weapon_Welder::Precache()
{
	// precache the weapon model here?
	PrecacheScriptSound("ASW_Welder.WeldLoop");
	PrecacheParticleSystem( "welding_door_seal" );
	PrecacheParticleSystem( "welding_door_cut" );
	BaseClass::Precache();
}

void CASW_Weapon_Welder::PrimaryAttack( void )
{
	WeldDoor( true );		// seal
}

void CASW_Weapon_Welder::SecondaryAttack( void )
{
	WeldDoor( false );	// cut
}

void CASW_Weapon_Welder::WeldDoor(bool bSeal)
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->GetCommander() )
		return;

	bool bWelding = false;

	CASW_Door* pDoor = FindDoor();
	if ( pDoor )
	{
		bWelding = true;

		Vector vecFacingPoint = pDoor->GetWeldFacingPoint(pMarine);
		Vector diff = vecFacingPoint - pMarine->GetAbsOrigin();
		diff.z = 0;
		VectorNormalize(diff);
		QAngle angCurrentFacing = pMarine->ASWEyeAngles();
		Vector vecCurrentFacing = vec3_origin;
		AngleVectors(angCurrentFacing, &vecCurrentFacing);
		vecCurrentFacing.z = 0;
		VectorNormalize(vecCurrentFacing);
		bool bFacing = DotProduct(diff, vecCurrentFacing) > 0.6f;
		if ( !pMarine->IsInhabited() )
		{
			bFacing = true;	// AI marines don't know how to face the door yet
		}
		if ( bFacing && !pDoor->IsOpen() )
		{
			// do our muzzle flash, if we're going to alter the weld some
			if ( bSeal && pDoor->GetSealAmount() < 1.0f )
			{
				pMarine->DoMuzzleFlash();
				m_bIsFiring = true;
			}
			else if ( !bSeal && pDoor->GetSealAmount() > 0 )
			{
				pMarine->DoMuzzleFlash();
				m_bIsFiring = true;
			}					
		}
#ifdef CLIENT_DLL
		pMarine->SetFacingPoint(vecFacingPoint, GetFireRate()*2.0f);
#else
		if ( gpGlobals->maxClients <= 1 )
		{
			pMarine->SetFacingPoint(vecFacingPoint, GetFireRate()*2.0f);
		}
#endif
		if ( bFacing )
		{
			if ( pDoor->IsOpen() )
			{
#ifndef CLIENT_DLL
				if ( bSeal )
				{
					pDoor->CloseForWeld( pMarine );	// shut the door first, so we can start welding it
				}
#endif
			}
			else
			{
				// tell the weapon to weld over the next fire rate interval
				m_fWeldTime = GetFireRate() + 0.004f;
				m_bWeldSeal = bSeal;
#ifndef CLIENT_DLL
				m_pWeldDoor = pDoor;
				//Msg( "Setting weld door to %d\n", pDoor->entindex() );
				// network door which door we're working on so all players can see progress
				if ( pMarine->GetMarineResource() )
					pMarine->GetMarineResource()->m_hWeldingDoor = pDoor;
#endif
			}
		}
	}
	else
	{
		//Msg( "Couldn't find door to weld\n" );
	}

	if ( pMarine->GetActiveWeapon() != this )
	{
		bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
		GetButtons( bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

		if ( bAttack1 || bAttack2 || bReload || pMarine->GetCurrentMeleeAttack() )
		{
			bWelding = false;
		}
	}

	if ( !bWelding )
	{
		m_iAutomaticWeldDirection = 0;
		m_bShotDelayed = false;
#ifdef GAME_DLL
		if ( pMarine->GetMarineResource() )
		{
			pMarine->GetMarineResource()->m_hWeldingDoor = NULL;
		}

		m_pWeldDoor = NULL;

		pMarine->OnWeldFinished();
#endif
		m_bIsFiring = false;
	}
	else
	{
		if ( bSeal )
		{
			m_flNextPrimaryAttack = m_flNextPrimaryAttack + GetFireRate();
		}
		else
		{
			m_flNextSecondaryAttack = m_flNextSecondaryAttack + GetFireRate();
		}
	}
}

// make the weapon weld if needed

void CASW_Weapon_Welder::ItemPostFrame()
{
#ifndef CLIENT_DLL
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->GetCommander() )
	{
		if ( m_bPlayingWelderSound )
		{
			m_bIsFiring = false;

			if ( pMarine->GetMarineResource() )
			{
				pMarine->GetMarineResource()->m_hWeldingDoor = NULL;
			}

			m_pWeldDoor = NULL;

			pMarine->OnWeldFinished();
			//Msg( "Clearing weld door as no marine\n" );
		}
		return BaseItemPostFrame();
	}

	if ( m_fWeldTime > 0 && m_pWeldDoor )
	{
		m_fWeldTime -= gpGlobals->frametime;

		if ( m_fWeldTime <= 0 )
		{
			m_bIsFiring = false;
			if ( pMarine->GetMarineResource() )
			{
				pMarine->GetMarineResource()->m_hWeldingDoor = NULL;
			}
		}

		float fSkillScale = MarineSkills()->GetHighestSkillValueNearby(pMarine->GetAbsOrigin(), ENGINEERING_AURA_RADIUS,
				ASW_MARINE_SKILL_ENGINEERING, ASW_MARINE_SUBSKILL_ENGINEERING_WELDING);

		CASW_Marine *pSkillMarine = MarineSkills()->GetLastSkillMarine();		
		if ( fSkillScale > 0.0f && pSkillMarine && pSkillMarine->GetMarineResource() )
		{
			pSkillMarine->m_fUsingEngineeringAura = gpGlobals->curtime;
			m_pWeldDoor->m_fSkillMarineHelping = gpGlobals->curtime;
		}
		else
		{
			m_pWeldDoor->m_fSkillMarineHelping = 0;
		}

		if ( fSkillScale < 1.0 )
		{
			fSkillScale = 0.6f;
		}

		float fSealAmount = gpGlobals->frametime * fSkillScale;
		m_pWeldDoor->WeldDoor( m_bWeldSeal, fSealAmount, pMarine );		// if the door is shut, we can weld
	}
#else
	if ( m_fWeldTime > 0 )
	{
		m_fWeldTime -= gpGlobals->frametime;
		if ( m_fWeldTime <= 0 )
		{
			m_bIsFiring = false;
		}
	}
#endif

	// check for ending automatic welding
	if ( m_bShotDelayed )
	{
		if ( m_pWeldDoor )
		{
			if ( ( m_iAutomaticWeldDirection > 0 && m_pWeldDoor->GetSealAmount() >= 1.0f ) ||
				 ( m_iAutomaticWeldDirection < 0 && m_pWeldDoor->GetSealAmount() <= 0.0f ) )
			{
				m_bShotDelayed = false;
#ifdef GAME_DLL
				if ( pMarine->GetMarineResource() )
				{
					pMarine->GetMarineResource()->m_hWeldingDoor = NULL;
				}
				m_pWeldDoor = NULL;
				pMarine->OnWeldFinished();
#endif
				m_bIsFiring = false;
			}
		}
		else
		{
			if ( !FindDoor() )
			{
				m_bShotDelayed = false;
#ifdef GAME_DLL
				if ( pMarine->GetMarineResource() )
				{
					pMarine->GetMarineResource()->m_hWeldingDoor = NULL;
				}
				m_pWeldDoor = NULL;
				pMarine->OnWeldFinished();
#endif
				m_bIsFiring = false;
			}
		}
	}

	BaseItemPostFrame();
}

// simplified version of the one in CASW_Weapon, don't worry about ammo or clips for the welder
void CASW_Weapon_Welder::BaseItemPostFrame()
{
	//CBasePlayer *pOwner = GetCommander();
	CASW_Marine* pOwner = GetMarine();
	
	if (!pOwner)
		return;

	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	// check for automatic welding
	if ( m_bShotDelayed )
	{
		if ( m_iAutomaticWeldDirection > 0 )
		{
			bAttack1 = true;
			//Msg( "doing automatic attack1 since bShotDelayed\n" );
		}
		else if ( m_iAutomaticWeldDirection < 0 )
		{
			bAttack2 = true;
			//Msg( "doing automatic attack2 since bShotDelayed\n" );
		}
	}

	//Track the duration of the fire
	//FIXME: Check for IN_ATTACK2 as well?
	//FIXME: What if we're calling ItemBusyFrame?
	m_fFireDuration = bAttack1 ? ( m_fFireDuration + gpGlobals->frametime ) : 0.0f;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	bool bFired = false;

	if ( bAttack2 && m_flNextSecondaryAttack <= gpGlobals->curtime )
	{
		bFired = true;
		SecondaryAttack();
	}
	
	if ( !bFired && bAttack1 && m_flNextPrimaryAttack <= gpGlobals->curtime )
	{				
		// If the firing button was just pressed, reset the firing time
		if ( pOwner && bAttack1 )
		{
			m_flNextPrimaryAttack = gpGlobals->curtime;
		}

		PrimaryAttack();
	}

	if ( !bAttack1 && !bAttack2 )
		m_bIsFiring = false;

	// -----------------------
	//  No buttons down
	// -----------------------
	if (!(bAttack1 || bAttack2 || bReload))
	{
		// no fire buttons down or reloading
		if ( !ReloadOrSwitchWeapons() && !m_bInReload )
		{
			WeaponIdle();
		}
	}
}

#ifdef CLIENT_DLL
void CASW_Weapon_Welder::ProcessMuzzleFlashEvent()
{
	// attach muzzle flash particle system effect
	int iAttachment = GetMuzzleAttachment();		
	if ( iAttachment > 0 )
	{		
		Vector sparkOrigin, sparkNormal;
		QAngle sparkAngles;

		if ( GetAttachment( iAttachment, sparkOrigin, sparkAngles ) )
		{
			AngleVectors(sparkAngles, &sparkNormal);
			FX_Sparks( sparkOrigin, 1, 1, sparkNormal, 3, 32, 128 );	// disabled for now
		}
	}
	C_BaseCombatWeapon::ProcessMuzzleFlashEvent();
	OnMuzzleFlashed();
}

void CASW_Weapon_Welder::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateDoorWeldingEffects();
}

void CASW_Weapon_Welder::ClientThink()
{
	BaseClass::ClientThink();

	UpdateDoorWeldingEffects();
}

void CASW_Weapon_Welder::UpdateDoorWeldingEffects( void )
{
	if ( !m_bIsFiring || !GetMarine() || !GetMarine()->GetMarineResource() )
	{
		RemoveWeldingEffects();
		StopWelderSound();
		return;
	}

	C_ASW_Door* pDoor = GetMarine()->GetMarineResource()->m_hWeldingDoor.Get();
	if ( !pDoor || pDoor->IsOpen() )
	{
		RemoveWeldingEffects();
		StopWelderSound();
		return;
	}

	StartWelderSound();

	if ( !m_hWeldEffects || m_bWeldSealLast != m_bWeldSeal )
	{
		CreateWeldingEffects( pDoor );
	}

	if ( m_hWeldEffects )
	{
		m_hWeldEffects->SetControlPoint( 0, pDoor->GetWeldFacingPoint( GetMarine() ) );
		m_hWeldEffects->SetControlPointForwardVector( 0, pDoor->GetSparkNormal( GetMarine() ) );
	}

	m_bWeldSealLast = m_bWeldSeal;
}

void CASW_Weapon_Welder::CreateWeldingEffects( C_ASW_Door* pDoor )
{
	if ( !m_bIsFiring || !GetOwner() || !pDoor )
		return;

	if ( m_hWeldEffects )
	{
		RemoveWeldingEffects();
	}

	if ( !m_hWeldEffects )
	{
		if ( m_bWeldSeal )
			m_hWeldEffects = ParticleProp()->Create( "welding_door_seal", PATTACH_CUSTOMORIGIN );
		else
			m_hWeldEffects = ParticleProp()->Create( "welding_door_cut", PATTACH_CUSTOMORIGIN );

		if ( m_hWeldEffects )
		{
			m_hWeldEffects->SetControlPoint( 0, pDoor->GetSparkNormal( GetMarine() ) );
			m_hWeldEffects->SetControlPointForwardVector( 0, pDoor->GetWeldFacingPoint( GetMarine() ) );
		}

		/*
		m_pLaserPointerEffect->SetControlPoint( 1, vecOrigin );
		m_pLaserPointerEffect->SetControlPoint( 2, tr.endpos );
		m_pLaserPointerEffect->SetControlPointForwardVector ( 1, vecDirShooting );
		Vector vecImpactY, vecImpactZ;
		VectorVectors( tr.plane.normal, vecImpactY, vecImpactZ ); 
		vecImpactY *= -1.0f;
		m_pLaserPointerEffect->SetControlPointOrientation( 2, vecImpactY, vecImpactZ, tr.plane.normal );
		m_pLaserPointerEffect->SetControlPoint( 3, Vector( alpha, 0, 0 ) );

		if ( m_hWeldEffects )
		{
		ParticleProp()->AddControlPoint( m_pLaserPointerEffect, 1, this, PATTACH_CUSTOMORIGIN );
		ParticleProp()->AddControlPoint( m_pLaserPointerEffect, 2, this, PATTACH_CUSTOMORIGIN );
		}
		*/
	}
}

void CASW_Weapon_Welder::RemoveWeldingEffects( void )
{
	if ( m_hWeldEffects )
	{
		m_hWeldEffects->StopEmission( false, false, true );
		m_hWeldEffects = NULL;
	}
}
#endif	// CLIENT_DLL

// get the continuous welding sound for this weapon
//   in multiplayer, server doesn't send the sound to the owning player as he'll make the sound himself clientside
CSoundPatch *CASW_Weapon_Welder::GetWelderSound( void )
{
	if ( m_pWeldingSound == NULL )
	{
#ifdef CLIENT_DLL
		CPASAttenuationFilter filter( this );
#else
		CPASAttenuationFilter filter( this );
		if ( gpGlobals->maxClients > 1 )
		{
			CASW_Marine *pMarine = GetMarine();
			if (pMarine)
			{
				CASW_Player *pPlayer = pMarine->GetCommander();
				if ( pPlayer )
					filter.RemoveRecipient(pPlayer);
			}			
		}
#endif
		m_pWeldingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_AUTO, "ASW_Welder.WeldLoop", ATTN_NORM );
	}

	return m_pWeldingSound;
}

void CASW_Weapon_Welder::StartWelderSound()
{
	if ( GetWelderSound() && !m_bPlayingWelderSound )
	{
		m_bPlayingWelderSound = true;
		CSoundEnvelopeController::GetController().Play( GetWelderSound(), 1.0f, 100 );		
	}
}

void CASW_Weapon_Welder::StopWelderSound()
{
	if ( m_pWeldingSound && m_bPlayingWelderSound )
	{
		m_bPlayingWelderSound = false;
		
		CSoundEnvelopeController::GetController().SoundFadeOut( m_pWeldingSound, 0.1f, true );
		m_pWeldingSound = NULL;
		//Msg( "Stopping welder sound\n" );
	}
}

bool CASW_Weapon_Welder::OffhandActivate()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->GetCommander() )
		return false;

	bool bRecommendedSeal = false;

	float fSealAmount = -1.0f;

	CASW_Door *pDoor = FindDoor();
	if ( pDoor )
	{
		fSealAmount = pDoor->GetSealAmount();
		bRecommendedSeal = pDoor->IsRecommendedSeal();
	}

	if ( fSealAmount < 0.0f )
	{
		return true;
	}

	// start automatic welding
	if ( !m_bShotDelayed )
	{
		m_bShotDelayed = true;
#ifdef GAME_DLL
		pMarine->OnWeldStarted();
#endif
		if ( fSealAmount == 0.0f || ( bRecommendedSeal && fSealAmount < 1.0f ) )
		{
			m_iAutomaticWeldDirection = 1;
		}
		else
		{
			// default to unsealing the door
			m_iAutomaticWeldDirection = -1;
		}
	}	
	else
	{
		// already welding, just flip direction
		m_iAutomaticWeldDirection = -m_iAutomaticWeldDirection;
	}

	return true;
}


float CASW_Weapon_Welder::GetFireRate()
{
	//float flRate = 0.07f;
	float flRate = GetWeaponInfo()->m_flFireRate;
		
	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}

CASW_Door* CASW_Weapon_Welder::FindDoor()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return NULL;

	if ( pMarine->IsInhabited() )
	{
		CASW_Player *pPlayer = pMarine->GetCommander();
		if ( !pPlayer )
			return NULL;

		// find our door area
		for ( int i = 0; i < pPlayer->GetNumUseEntities(); i++ )
		{
			CBaseEntity* pEnt = pPlayer->GetUseEntity( i );
			CASW_Door_Area* pDoorArea = dynamic_cast< CASW_Door_Area* >( pEnt );
			if ( pDoorArea )
			{
				CASW_Door* pDoor = pDoorArea->GetASWDoor();
				if ( pDoor && pDoor->GetHealth() > 0 )
				{
					return pDoor;
				}
			}
		}
	}
	else
	{
#ifdef GAME_DLL
		CASW_Door_Area *pClosestArea = NULL;
		float flClosestDist = FLT_MAX;

		for ( int i = 0; i < IASW_Use_Area_List::AutoList().Count(); i++ )
		{
			CASW_Use_Area *pArea = static_cast< CASW_Use_Area* >( IASW_Use_Area_List::AutoList()[ i ] );
			if ( pArea->Classify() == CLASS_ASW_DOOR_AREA )
			{
				CASW_Door_Area *pDoorArea = assert_cast<CASW_Door_Area*>( pArea );
				CASW_Door* pDoor = pDoorArea->GetASWDoor();
				if ( !pDoor || pDoor->GetHealth() <= 0 )
				{
					continue;
				}

				float flDist = GetAbsOrigin().DistTo( pArea->WorldSpaceCenter() );
				if ( flDist < flClosestDist && pArea->CollisionProp()->IsPointInBounds( pMarine->WorldSpaceCenter() ) )
				{
					flClosestDist = flDist;
					pClosestArea = pDoorArea;
				}
			}
		}

		if ( pClosestArea )
		{
			CASW_Door* pDoor = pClosestArea->GetASWDoor();
			if ( pDoor )
			{
				return pDoor;
			}
		}
#endif
	}
	return NULL;
}
