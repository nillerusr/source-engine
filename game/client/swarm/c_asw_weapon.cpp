
#include "cbase.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "in_buttons.h"
#include "fx.h"
#include "precache_register.h"
#include "c_breakableprop.h"
#include "asw_marine_skills.h"
#include "datacache/imdlcache.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_fx.h"
#include "prediction.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include "dlight.h"
#include "r_efx.h"
#include "asw_input.h"
#include "asw_weapon_parse.h"
#include "asw_gamerules.h"
#include "game_timescale_shared.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_NETWORK_TABLE_NOBASE( C_ASW_Weapon, DT_ASWLocalWeaponData )
	RecvPropIntWithMinusOneFlag( RECVINFO(m_iClip2 )),
	RecvPropInt( RECVINFO(m_iSecondaryAmmoType )),
	RecvPropFloat(RECVINFO(m_fReloadStart)),
	RecvPropFloat(RECVINFO(m_fFastReloadStart)),
	RecvPropFloat(RECVINFO(m_fFastReloadEnd)),
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE_NOBASE( C_ASW_Weapon, DT_ASWActiveLocalWeaponData )
	RecvPropTime( RECVINFO( m_flNextPrimaryAttack ) ),
	RecvPropTime( RECVINFO( m_flNextSecondaryAttack ) ),
	RecvPropInt( RECVINFO( m_nNextThinkTick ) ),
	RecvPropTime( RECVINFO( m_flTimeWeaponIdle ) ),
END_NETWORK_TABLE()

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon, DT_ASW_Weapon )

BEGIN_NETWORK_TABLE( CASW_Weapon, DT_ASW_Weapon )
	RecvPropBool		( RECVINFO( m_bIsFiring ) ),
	RecvPropBool		( RECVINFO( m_bInReload ) ),
	RecvPropBool		( RECVINFO( m_bSwitchingWeapons ) ),
	RecvPropDataTable("ASWLocalWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_ASWLocalWeaponData)),
	RecvPropDataTable("ASWActiveLocalWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_ASWActiveLocalWeaponData)),
	RecvPropBool		( RECVINFO( m_bFastReloadSuccess ) ),
	RecvPropBool		( RECVINFO( m_bFastReloadFailure ) ),
	RecvPropBool		( RECVINFO( m_bPoweredUp ) ),
	RecvPropIntWithMinusOneFlag( RECVINFO(m_iClip1 )),
	RecvPropInt( RECVINFO(m_iPrimaryAmmoType )),
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Weapon )
	DEFINE_PRED_FIELD( m_nNextThinkTick, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flNextPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
	DEFINE_PRED_FIELD_TOL( m_flNextSecondaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_iPrimaryAmmoType, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iSecondaryAmmoType, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iClip1, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),			
	DEFINE_PRED_FIELD( m_iClip2, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),			

	DEFINE_PRED_FIELD( m_bIsFiring, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bInReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_fReloadStart, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.1f ),
	DEFINE_FIELD( m_fReloadProgress, FIELD_FLOAT ),
	DEFINE_PRED_FIELD_TOL( m_fFastReloadStart, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.1f ),
	DEFINE_PRED_FIELD_TOL( m_fFastReloadEnd, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.1f ),
	DEFINE_FIELD( m_bFastReloadSuccess, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bFastReloadFailure, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_fReloadClearFiringTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDelayedFire, FIELD_FLOAT ),
	DEFINE_FIELD( m_bShotDelayed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hLaserSight, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bFastReloadSuccess, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bFastReloadFailure, FIELD_BOOLEAN ),

END_PREDICTION_DATA()
//DEFINE_PRED_FIELD_TOL( m_flNextCoolTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),

ConVar asw_red_muzzle_r("asw_red_muzzle_r", "255");
ConVar asw_red_muzzle_g("asw_red_muzzle_g", "128");
ConVar asw_red_muzzle_b("asw_red_muzzle_b", "128");
ConVar asw_muzzle_light_radius_min( "asw_muzzle_light_radius_min", "50", FCVAR_CHEAT );
ConVar asw_muzzle_light_radius_max( "asw_muzzle_light_radius_max", "100", FCVAR_CHEAT );
ConVar asw_muzzle_light( "asw_muzzle_light", "255 192 64 6", FCVAR_CHEAT );
ConVar asw_muzzle_flash_new_type( "asw_muzzle_flash_new_type", "0", FCVAR_CHEAT );
ConVar asw_laser_sight( "asw_laser_sight", "1", FCVAR_ARCHIVE );
ConVar asw_laser_sight_min_distance( "asw_laser_sight_min_distance", "9999", 0, "The min distance at which to accurately draw the laser sight from the muzzle rather than using the shoot direction" );

extern ConVar asw_use_particle_tracers;
extern ConVar muzzleflash_light;

C_ASW_Weapon::C_ASW_Weapon() :
m_GlowObject( this, Vector( 0.0f, 0.4f, 0.75f ), 1.0f, false, true )
{
	SetPredictionEligible( true );
	m_iEquipmentListIndex = -1;
	m_fLastMuzzleFlashTime = 0;
	m_fMuzzleFlashScale = -1;
	m_fTurnRateModifier = 1.0f;
	m_fReloadClearFiringTime = 0;
	m_fFiringTurnRateModifier = 0.7f;
	m_bSwitchingWeapons = false;
	
	m_bShotDelayed = 0;
	m_flDelayedFire = 0;
	bOldHidden = false;

	m_flReloadFailTime = 1.0;
	m_fReloadStart = 0;
	m_fReloadProgress = 0;
	m_fFastReloadStart = 0;
	m_fFastReloadEnd = 0;
	m_bFastReloadSuccess = false;
	m_bFastReloadFailure = false;

	m_nUseIconTextureID = -1;
	m_bWeaponCreated = false;
	m_nMuzzleAttachment = 0;
	m_nLastMuzzleAttachment = 0;
	m_bLocalPlayerControlling = false;

	m_fMuzzleFlashTime = 0.0f;

	m_bPoweredUp = false;
}


C_ASW_Weapon::~C_ASW_Weapon()
{
	// delete the bayonet, if there's any
	if (m_hBayonet.Get())
	{
		UTIL_Remove( m_hBayonet.Get() );
		m_hBayonet = NULL;
	}

	if (m_hLaserSight.Get())
	{
		UTIL_Remove( m_hLaserSight.Get() );
		m_hLaserSight = NULL;
	}
}

void C_ASW_Weapon::OnDataChanged( DataUpdateType_t type )
{	
	bool bPredict = ShouldPredict();
	if (bPredict)
	{
		SetPredictionEligible( true );
	}
	else
	{
		SetPredictionEligible( false );
	}

	BaseClass::OnDataChanged( type );

	if ( GetPredictable() && !bPredict )
		ShutdownPredictable();

	if ( type == DATA_UPDATE_CREATED )
	{
		if (SupportsBayonet())
		{
			// check if our owner has the bayonet skill
			// bayonet disabled at this time
			//C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(GetOwner());
			//if (pMarine && MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_EDGED) > 0)
			//{
				//CreateBayonet();
			//}
		}
		// don't create the laser sight anymore
		// we use the particle one now
		//CreateLaserSight();

		m_bWeaponCreated = true;

		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	if (IsEffectActive(EF_NODRAW) != bOldHidden)
	{
		if (m_hBayonet.Get())
		{
			if (IsEffectActive(EF_NODRAW))
				m_hBayonet->AddEffects( EF_NODRAW );
			else
				m_hBayonet->RemoveEffects( EF_NODRAW );
		}

		if (m_hLaserSight.Get())
		{
			if (IsEffectActive(EF_NODRAW))
				m_hLaserSight->AddEffects( EF_NODRAW );
			else
				m_hLaserSight->RemoveEffects( EF_NODRAW );
		}
	}
	bOldHidden = IsEffectActive(EF_NODRAW);
}

#define ASW_BAYONET_MODEL "models/swarm/Bayonet/bayonet.mdl"

void C_ASW_Weapon::CreateBayonet()
{
	if (m_hBayonet.Get())	// already have a bayonet
		return;
	int iAttach = LookupAttachment("muzzle");
	if (iAttach == -1)
	{
		Msg("no attachment for bayonet\n");
		return;
	}

	C_BreakableProp *pEnt = new C_BreakableProp;
	if (!pEnt)
	{
		Msg("No C_BreakableProp crated\n");
		return;
	}

	if (pEnt->InitializeAsClientEntity( ASW_BAYONET_MODEL, false ))
	{
		//pEnt->SetFadeMinMax( 1200, 1500 );
		pEnt->SetParent( this, iAttach );
		pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
		pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
		pEnt->SetSolid( SOLID_NONE );
		pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
		m_hBayonet = pEnt;
		Msg("Bayonet Entity all setup: %d\n", pEnt->entindex());
	}
	else
	{
		Msg("Bayonet  Failed to init as client ent\n");
	}
}

bool C_ASW_Weapon::ShouldPredict( void )
{
	C_BasePlayer *pOwner = NULL;
	C_ASW_Marine *pMarine = NULL;

	pMarine = dynamic_cast<C_ASW_Marine*>(GetOwner());
	if (pMarine)
	{
		if (pMarine->m_Commander && pMarine->IsInhabited())
			pOwner = pMarine->GetCommander();
	}

	if (!pMarine)
		pOwner = ToBasePlayer( GetOwner() );

	if ( C_BasePlayer::IsLocalPlayer( pOwner ) )
		return true;

	return BaseClass::ShouldPredict();
}

C_BasePlayer *C_ASW_Weapon::GetPredictionOwner( void )
{
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>( GetOwner() );
	if ( pMarine && pMarine->IsInhabited() )
	{
		return pMarine->GetCommander();
	}
	return NULL;
}

int C_ASW_Weapon::GetMuzzleAttachment( void )
{
	if ( m_nMuzzleAttachment == 0 )
	{
		// lazy initialization
		m_nMuzzleAttachment = LookupAttachment( "muzzle" );
	}
	return m_nMuzzleAttachment;
}

void C_ASW_Weapon::SetMuzzleAttachment( int nNewAttachment )
{
	m_nMuzzleAttachment = nNewAttachment;
}
float C_ASW_Weapon::GetMuzzleFlashScale( void )
{
	// if we haven't calculated the muzzle scale based on the carrying marine's skill yet, then do so
	//if (m_fMuzzleFlashScale == -1)
	{
		C_ASW_Marine *pMarine = GetMarine();
		if (pMarine)
			m_fMuzzleFlashScale = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_MUZZLE);
		else
			return 1.5f;
	}
	return m_fMuzzleFlashScale;
}

bool C_ASW_Weapon::GetMuzzleFlashRed()
{
	return GetMuzzleFlashScale() >= 1.9f;	// red if our muzzle flash is the biggest size based on our skill
}

void C_ASW_Weapon::ProcessMuzzleFlashEvent()
{
	// we don't want weapons like the shotgun (which calls this for each pellet) to crates muzzles multiple times a frame
	// but we want PDWs to be able to do this.  We get around this by checking the last muzzle attachment that was used
	// in the case of the PDW, it alternates back and forth, but should still never get called twice for the same attachment in the same frame
	int iAttachment = GetMuzzleAttachment();
	if ( m_fLastMuzzleFlashTime == gpGlobals->curtime && m_nLastMuzzleAttachment == iAttachment )
		return;

	m_nLastMuzzleAttachment = iAttachment;
	m_fLastMuzzleFlashTime = gpGlobals->curtime;	// store when we last fired, used for some clientside animation stuff
	// if this weapon is a flamethrower type weapon, then tell our marine's flame emitter
	//  to work for a while instead of muzzle flashing
	if (ShouldMarineFlame())
	{
		C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOwner());
		if (pMarine)
			pMarine->m_fFlameTime = gpGlobals->curtime + GetFireRate() + 0.02f;
		return;
	}
	else if (ShouldMarineFireExtinguish())
	{
		C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOwner());
		if (pMarine)
			pMarine->m_fFireExtinguisherTime = gpGlobals->curtime + GetFireRate() + 0.02f;
		return;
	}
	// attach muzzle flash particle system effect
	if ( iAttachment > 0 )
	{
		float flScale = GetMuzzleFlashScale();	
		if ( asw_use_particle_tracers.GetBool() )
		{
			FX_ASW_ParticleMuzzleFlashAttached( flScale, GetRefEHandle(), iAttachment, GetMuzzleFlashRed() );
		}
		else
		{
			if (GetMuzzleFlashRed())
			{			
				FX_ASW_RedMuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );
			}
			else
			{
				FX_ASW_MuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );
			}
		}
	}

	// If we have an attachment, then stick a light on it.
	if ( muzzleflash_light.GetBool() )
	{
		Vector vAttachment;
		QAngle dummyAngles;
		if ( GetAttachment( 1, vAttachment, dummyAngles ) )
		{
			// Make an elight
			dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH + index );
			el->origin = vAttachment;
			el->radius = random->RandomFloat( asw_muzzle_light_radius_min.GetFloat(), asw_muzzle_light_radius_max.GetFloat() ); 
			el->decay = el->radius / 0.05f;
			el->die = gpGlobals->curtime + 0.05f;
			Color c = asw_muzzle_light.GetColor();
			el->color.r = c.r();
			el->color.g = c.g();
			el->color.b = c.b();
			el->color.exponent = c.a(); 
		}
	}
	OnMuzzleFlashed();	
}


bool C_ASW_Weapon::Simulate()
{
	SimulateLaserPointer();

	BaseClass::Simulate();

	return asw_laser_sight.GetBool();
}

void C_ASW_Weapon::ClientThink()
{
	bool bShouldGlow = false;
	float flDistanceToMarineSqr = 0.0f;
	float flWithinDistSqr = (ASW_MARINE_USE_RADIUS*4)*(ASW_MARINE_USE_RADIUS*4);

	if ( !GetOwner() )
	{
		C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetMarine() && ASWInput()->GetUseGlowEntity() != this && AllowedToPickup( pLocalPlayer->GetMarine() ) )
		{
			flDistanceToMarineSqr = (pLocalPlayer->GetMarine()->GetAbsOrigin() - WorldSpaceCenter()).LengthSqr();
			if ( flDistanceToMarineSqr < flWithinDistSqr )
				bShouldGlow = true;
		}
		if ( ASWGameRules() && ASWGameRules()->GetMarineDeathCamInterp() > 0.0f )
		{
			bShouldGlow = false;
		}
	}

	m_GlowObject.SetRenderFlags( false, bShouldGlow );

	if ( m_GlowObject.IsRendering() )
	{
		m_GlowObject.SetAlpha( MIN( 0.7f, (1.0f - (flDistanceToMarineSqr / flWithinDistSqr)) * 1.0f) );
	}
}

bool C_ASW_Weapon::ShouldDraw()
{
	// If it's a player, then only show active weapons
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOwner());
	if (pMarine)
	{
		if ( pMarine->IsEffectActive( EF_NODRAW ) )
		{
			return false;
		}
		return (pMarine->GetActiveWeapon() == this);
	}
	if (!BaseClass::ShouldDraw())
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this client's carrying this weapon
//-----------------------------------------------------------------------------
bool C_ASW_Weapon::IsCarriedByLocalPlayer( void )
{
	if ( !GetOwner() )
		return false;

	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>( GetOwner() );
	if ( !pMarine || !pMarine->GetCommander() || !pMarine->IsInhabited() )
		return false;

	return ( C_BasePlayer::IsLocalPlayer( pMarine->GetCommander() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this weapon is the local client's currently wielded weapon
//-----------------------------------------------------------------------------
bool C_ASW_Weapon::IsActiveByLocalPlayer( void )
{
	if ( IsCarriedByLocalPlayer() )
	{
		return (m_iState == WEAPON_IS_ACTIVE);
	}

	return false;
}

ShadowType_t C_ASW_Weapon::ShadowCastType()
{
//#ifdef DEBUG
	//return SHADOWS_NONE;
//#endif
	return SHADOWS_RENDER_TO_TEXTURE;
}

const char* C_ASW_Weapon::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_Rifle.ReloadB"; break;
		case 2: return "ASW_Rifle.ReloadC"; break;
		default: break;
	};
	return "ASW_Rifle.ReloadA";
}

void C_ASW_Weapon::ASWReloadSound(int iType)
{
	int iPitch = 100;
	const char *shootsound = GetPartialReloadSound(iType);
	if ( !shootsound || !shootsound[0] )
		return;
	CSoundParameters params;
		
	if ( !GetParametersForSound( shootsound, params, NULL ) )
		return;

	float soundtime = 0;
	EmitSound_t playparams(params);
	if (soundtime != 0)
		playparams.m_flSoundTime = soundtime;
	playparams.m_nPitch = iPitch;

	C_ASW_Player *pPlayer = GetCommander();

	if ( params.play_to_owner_only )
	{
		// Am I only to play to my owner?
		if ( pPlayer && GetOwner() && pPlayer->GetMarine() == GetOwner() )
		{
			CSingleUserRecipientFilter filter( pPlayer );
			if ( prediction->InPrediction() && IsPredicted() )
			{
				filter.UsePredictionRules();
			}
			EmitSound(filter, GetOwner()->entindex(), playparams);
			//EmitSound( filter, GetOwner()->entindex(), shootsound, NULL, soundtime );
		}
	}
	else
	{
		// Play weapon sound from the owner
		if ( GetOwner() )
		{
			CPASAttenuationFilter filter( GetOwner(), params.soundlevel );
			if ( prediction->InPrediction() && IsPredicted() )
			{
				filter.UsePredictionRules();
			}
			EmitSound(filter, GetOwner()->entindex(), playparams);
		}
		// If no owner play from the weapon (this is used for thrown items)
		else
		{
			CPASAttenuationFilter filter( this, params.soundlevel );
			if ( prediction->InPrediction() && IsPredicted() )
			{
				filter.UsePredictionRules();
			}
			EmitSound( filter, entindex(), shootsound, NULL, soundtime ); 
		}
	}
}

// attach laser sight to all guns with that attachment
void C_ASW_Weapon::CreateLaserSight()
{	
	int iAttachment = LookupAttachment( "laser" );
	if ( iAttachment <= 0 )
	{
		return;
	}

	C_BaseAnimating *pEnt = new C_BaseAnimating;
	if (!pEnt)
	{
		Msg("Error, couldn't create new C_BaseAnimating\n");
		return;
	}
	if (!pEnt->InitializeAsClientEntity( "models/swarm/shouldercone/lasersight.mdl", false ))
	{
		Msg("Error, couldn't InitializeAsClientEntity\n");
		pEnt->Release();
		return;
	}
	pEnt->SetParent( this, iAttachment );
	pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
	pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );	
	pEnt->SetSolid( SOLID_NONE );
	pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	m_hLaserSight = pEnt;
}

//-----------------------------------------------------------------------------
// Dropped weapon functions
//-----------------------------------------------------------------------------

int C_ASW_Weapon::GetUseIconTextureID()
{
	if (m_nUseIconTextureID == -1)
	{
		const CASW_WeaponInfo *pInfo = GetWeaponInfo();
		if ( pInfo )
		{
			m_nUseIconTextureID = vgui::surface()->CreateNewTextureID();
			char buffer[ 256 ];
			Q_snprintf( buffer, sizeof( buffer ), "vgui/%s", pInfo->szEquipIcon );
			vgui::surface()->DrawSetTextureFile( m_nUseIconTextureID, buffer, true, false);
		}
	}

	return m_nUseIconTextureID;
}

bool C_ASW_Weapon::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	if ( !pUser )
		return false;

	action.iUseIconTexture = GetUseIconTextureID();
	action.UseTarget = this;
	action.fProgress = -1;
	action.iInventorySlot = pUser->GetWeaponPositionForPickup( GetClassname() );
	action.bWideIcon = ( action.iInventorySlot != ASW_INVENTORY_SLOT_EXTRA );

	// build the appropriate take string
	const CASW_WeaponInfo *pInfo = GetWeaponInfo();
	if ( pInfo )
	{
		wchar_t wszWeaponName[ 128 ];
		TryLocalize( pInfo->szPrintName, wszWeaponName, sizeof( wszWeaponName ) );
		if ( m_bSwappingWeapon )
		{
			g_pVGuiLocalize->ConstructString( action.wszText, sizeof( action.wszText ), g_pVGuiLocalize->Find("#asw_swap_weapon_format"), 1, wszWeaponName );
		}
		else
		{
			g_pVGuiLocalize->ConstructString( action.wszText, sizeof( action.wszText ), g_pVGuiLocalize->Find("#asw_take_weapon_format"), 1, wszWeaponName );
		}
	}

	if ( AllowedToPickup( pUser ) )
	{
		action.UseIconRed = 66;
		action.UseIconGreen = 142;
		action.UseIconBlue = 192;
		action.bShowUseKey = true;	
	}
	else
	{
		action.UseIconRed = 255;
		action.UseIconGreen = 0;
		action.UseIconBlue = 0;
		action.TextRed = 164;
		action.TextGreen = 164;
		action.TextBlue = 164;
		action.bTextGlow = false;
		if ( ASWGameRules() )
			TryLocalize( ASWGameRules()->GetPickupDenial(), action.wszText, sizeof( action.wszText ) );
		action.bShowUseKey = false;
	}

	return true;
}

int C_ASW_Weapon::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// HACK - avoid the hack in C_BaseCombatWeapon that changes the model inappropriately for Infested
	return BASECOMBATWEAPON_DERIVED_FROM::DrawModel( flags, instance );
}

IClientModelRenderable*	C_ASW_Weapon::GetClientModelRenderable()
{
	// HACK - avoid the hack in C_BaseCombatWeapon that changes the model inappropriately for Infested
	return BASECOMBATWEAPON_DERIVED_FROM::GetClientModelRenderable();
}
//--------------------------------------------------------------------------------------------------------
bool C_ASW_Weapon::ShouldShowLaserPointer()
{
	if ( !asw_laser_sight.GetBool() || IsDormant() || !GetOwner() || !IsOffensiveWeapon() )
	{
		return false;
	}

	//C_ASW_Player *pPlayer = GetCommander();
	C_ASW_Marine *pMarine = GetMarine();
	//if ( !pPlayer || !pMarine || !pPlayer->GetMarine() )
	//	return false;

	return ( pMarine && pMarine->GetActiveWeapon() == this );

	//return ( pPlayer == C_ASW_Player::GetLocalASWPlayer() && pPlayer->GetMarine() == pMarine 
	//	&& pMarine->GetActiveWeapon() == this );
}

bool C_ASW_Weapon::ShouldAlignWeaponToLaserPointer()
{
	if ( !asw_laser_sight.GetBool() || IsDormant() || !GetOwner() || !IsOffensiveWeapon() )
	{
		return false;
	}

	if ( !m_pLaserPointerEffect || !GetWeaponInfo() || !GetWeaponInfo()->m_bOrientToLaser )
		return false;


	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetActiveWeapon() != this )
		return false;
	
	if ( IsReloading() || pMarine->GetCurrentMeleeAttack() || m_bSwitchingWeapons
		|| (gpGlobals->curtime < pMarine->GetStopTime()) || pMarine->IsDoingEmoteGesture()
		|| pMarine->ShouldPreventLaserSight() || GameTimescale()->GetCurrentTimescale() < 1.0f 
		|| pMarine->GetUsingEntity() || pMarine->GetFacingPoint() != vec3_origin )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------
void C_ASW_Weapon::SimulateLaserPointer()
{
	if ( !ShouldShowLaserPointer() )
	{
		RemoveLaserPointerEffect();
		return;
	}

	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		RemoveLaserPointerEffect();
		RemoveMuzzleFlashEffect();
		return;
	}

	bool bLocalPlayer = false;
	C_ASW_Player *pPlayer = GetCommander();
	C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer == pLocalPlayer && pMarine->IsInhabited() )
		bLocalPlayer = true;

	Vector vecOrigin;
	QAngle angWeapon;

	int iAttachment = GetMuzzleAttachment();
	if ( iAttachment <= 0 )
	{
		RemoveLaserPointerEffect();
		RemoveMuzzleFlashEffect();
		return;
	}

	Vector vecDirShooting;
	if ( bLocalPlayer )
	{
		vecDirShooting = pLocalPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	}
	else
	{
		QAngle angMarine = pMarine->ASWEyeAngles();

		Vector vRight;
		Vector vUp;
		AngleVectors( angMarine, &vecDirShooting, &vRight, &vUp );
	}

	GetAttachment( iAttachment, vecOrigin, angWeapon );

	float flDistance = GetLaserPointerRange();
	float alpha = 0.65f;
	float alphaFF = 0;
	
	if ( !bLocalPlayer )
	{
		flDistance = 100;
		alpha = 0.3f;
		alphaFF = 0;
	}
	else if ( IsOffensiveWeapon() )
	{
		C_BaseEntity *pEnt = GetLaserTargetEntity();
		if ( pEnt && pEnt->Classify() == CLASS_ASW_MARINE )
			alphaFF = 0.65f;
	}

	if ( IsReloading() || pMarine->GetCurrentMeleeAttack() || m_bSwitchingWeapons
			|| (gpGlobals->curtime < pMarine->GetStopTime()) || pMarine->IsDoingEmoteGesture()
			|| pMarine->ShouldPreventLaserSight() || GameTimescale()->GetCurrentTimescale() < 1.0f 
			|| pMarine->GetUsingEntity() || pMarine->GetFacingPoint() != vec3_origin )
	{
		alpha = 0;
		alphaFF = 0;
	}
	else if ( IsFiring() )
	{
		alpha *= 0.06f;
		alphaFF *= 1.5f;
	}

	// Only apply the new laser sight correction code if dealing with a local player
	if ( bLocalPlayer )
	{
		if ( pMarine->m_flLaserSightLength > asw_laser_sight_min_distance.GetFloat() )
		{
			//vecDirShooting += pMarine->m_vLaserSightCorrection;
			vecDirShooting = pMarine->m_vLaserSightCorrection;
		}
	}

	trace_t tr;
	// used to call GetWeaponRange() which was defined per weapon
	UTIL_TraceLine( vecOrigin, vecOrigin + (vecDirShooting * flDistance), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	m_hLaserTargetEntity = tr.m_pEnt;

	if ( !m_pLaserPointerEffect )
	{
		CreateLaserPointerEffect( bLocalPlayer, iAttachment );
	}

	if ( m_pLaserPointerEffect )
	{
		// if we switched which marine we are controlling, destroy the old effect and create the new one
		if ( m_bLocalPlayerControlling != bLocalPlayer )
		{
			CreateLaserPointerEffect( bLocalPlayer, iAttachment );
		}

		m_pLaserPointerEffect->SetControlPoint( 1, vecOrigin );
		m_pLaserPointerEffect->SetControlPoint( 2, tr.endpos );
		m_vecLaserPointerDirection = vecDirShooting;
		m_pLaserPointerEffect->SetControlPointForwardVector ( 1, vecDirShooting );
		Vector vecImpactY, vecImpactZ;
		VectorVectors( tr.plane.normal, vecImpactY, vecImpactZ ); 
		vecImpactY *= -1.0f;
		m_pLaserPointerEffect->SetControlPointOrientation( 2, vecImpactY, vecImpactZ, tr.plane.normal );
		m_pLaserPointerEffect->SetControlPoint( 3, Vector( alpha, alphaFF, 0 ) );
	}

	if ( !m_pMuzzleFlashEffect )
	{
		CreateMuzzleFlashEffect( iAttachment );
	}

	if ( m_pMuzzleFlashEffect )
	{
		// scale
		m_pMuzzleFlashEffect->SetControlPoint( 10, Vector( GetMuzzleFlashScale(), 0, 0 ) );

		// color
		Vector vecColor = Vector( 1, 1, 1 );
		if ( GetMuzzleFlashRed() )
		{
			vecColor = Vector( 1, 0.55, 0.55 );
		}
		m_pMuzzleFlashEffect->SetControlPoint( 20, vecColor );

		// Set Alpha control point
		float fAlpha;
		fAlpha = 1.0f - clamp( ( gpGlobals->curtime - m_fMuzzleFlashTime ) / 0.01, 0.0f, 1.0f );
		//m_pMuzzleFlashEffect->SetControlPoint( 15, Vector( fAlpha, 0.0f, 0.0f ) );
	}

	m_bLocalPlayerControlling = bLocalPlayer;
}

void C_ASW_Weapon::CreateLaserPointerEffect( bool bLocalPlayer, int iAttachment )
{
	if ( m_pLaserPointerEffect )
	{
		RemoveLaserPointerEffect();
	}

	if ( !m_pLaserPointerEffect )
	{
		if ( bLocalPlayer )
			m_pLaserPointerEffect = ParticleProp()->Create( "weapon_laser_sight", PATTACH_POINT_FOLLOW, iAttachment );
		else
			m_pLaserPointerEffect = ParticleProp()->Create( "weapon_laser_sight_other", PATTACH_POINT_FOLLOW, iAttachment );

		if ( m_pLaserPointerEffect )
		{
			ParticleProp()->AddControlPoint( m_pLaserPointerEffect, 1, this, PATTACH_CUSTOMORIGIN );
			ParticleProp()->AddControlPoint( m_pLaserPointerEffect, 2, this, PATTACH_CUSTOMORIGIN );
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void C_ASW_Weapon::RemoveLaserPointerEffect( void )
{
	if ( m_pLaserPointerEffect )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_pLaserPointerEffect );
		m_pLaserPointerEffect = NULL;
	}
}

void C_ASW_Weapon::CreateMuzzleFlashEffect( int iAttachment )
{
	if ( !asw_muzzle_flash_new_type.GetBool() )
		return;

	if ( m_pMuzzleFlashEffect )
	{
		RemoveMuzzleFlashEffect();
	}

	if ( !m_pMuzzleFlashEffect )
	{
		m_pMuzzleFlashEffect = ParticleProp()->Create( GetMuzzleEffectName(), PATTACH_POINT_FOLLOW, iAttachment );

		if ( m_pMuzzleFlashEffect )
		{
			ParticleProp()->AddControlPoint( m_pMuzzleFlashEffect, 1, this, PATTACH_CUSTOMORIGIN );
			ParticleProp()->AddControlPoint( m_pMuzzleFlashEffect, 2, this, PATTACH_CUSTOMORIGIN );

			// scale
			m_pMuzzleFlashEffect->SetControlPoint( 10, Vector( GetMuzzleFlashScale(), 0, 0 ) );

			// color
			Vector vecColor = Vector( 1, 1, 1 );
			m_pMuzzleFlashEffect->SetControlPoint( 20, vecColor );
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void C_ASW_Weapon::RemoveMuzzleFlashEffect( void )
{
	if ( m_pMuzzleFlashEffect )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_pMuzzleFlashEffect );
		m_pMuzzleFlashEffect = NULL;
	}
}