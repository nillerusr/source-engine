//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Laser Rifle & Shield combo
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_csbase.h"
#include "ammodef.h"
#include "cs_gamerules.h"

#define ALLOW_WEAPON_SPREAD_DISPLAY	0

#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_cs_player.h"
	#include "hud_crosshair.h"
	#include "c_te_effect_dispatch.h"
	#include "c_te_legacytempents.h"

	extern IVModelInfoClient* modelinfo;

#else

	#include "cs_player.h"
	#include "te_effect_dispatch.h"
	#include "KeyValues.h"
	#include "cs_ammodef.h"

	extern IVModelInfo* modelinfo;

#endif


ConVar weapon_accuracy_model( "weapon_accuracy_model", "2", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE );


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //



struct WeaponAliasTranslationInfoStruct
{
	const char* alias;
	const char* translatedAlias;
};

static const WeaponAliasTranslationInfoStruct s_WeaponAliasTranslationInfo[] =
{
	{ "cv47", "ak47" },
	{ "defender", "galil" },
	{ "krieg552", "sg552" },
	{ "magnum", "awp" },
	{ "d3au1", "g3sg1" },
	{ "clarion", "famas" },
	{ "bullpup", "aug" },
	{ "krieg550", "sg550" },
	{ "9x19mm", "glock" },
	{ "km45", "usp" },
	{ "228compact", "p228" },
	{ "nighthawk", "deagle" },
	{ "elites", "elite" },
	{ "fn57", "fiveseven" },
	{ "12gauge", "m3" },
	{ "autoshotgun", "xm1014" },
	{ "mp", "tmp" },
	{ "smg", "mp5navy" },
	{ "mp5", "mp5navy" },
	{ "c90", "p90" },
	{ "vest", "kevlar" },
	{ "vesthelm", "assaultsuit" },
	{ "smokegrenade", "sgren" },
	{ "smokegrenade", "sgren" },
	{ "nvgs", "nightvision" },

	{ "", "" } // this needs to be last
};


struct WeaponAliasInfo
{
	CSWeaponID id;
	const char* alias;
};

WeaponAliasInfo s_weaponAliasInfo[] =
{
	{ WEAPON_P228,				"p228" },
	{ WEAPON_GLOCK,				"glock" },
	{ WEAPON_SCOUT,				"scout" },
	{ WEAPON_XM1014,			"xm1014" },
	{ WEAPON_MAC10,				"mac10" },
	{ WEAPON_AUG,				"aug" },
	{ WEAPON_ELITE,				"elite" },
	{ WEAPON_FIVESEVEN,			"fiveseven" },
	{ WEAPON_UMP45,				"ump45" },
	{ WEAPON_SG550,				"sg550" },
	{ WEAPON_GALIL,				"galil" },
	{ WEAPON_FAMAS,				"famas" },
	{ WEAPON_USP,				"usp" },
	{ WEAPON_AWP,				"awp" },
	{ WEAPON_MP5NAVY,			"mp5navy" },
	{ WEAPON_M249,				"m249" },
	{ WEAPON_M3,				"m3" },
	{ WEAPON_M4A1,				"m4a1" },
	{ WEAPON_TMP,				"tmp" },
	{ WEAPON_G3SG1,				"g3sg1" },
	{ WEAPON_DEAGLE,			"deagle" },
	{ WEAPON_SG552,				"sg552" },
	{ WEAPON_AK47,				"ak47" },
	{ WEAPON_P90,				"p90" },

	{ WEAPON_KNIFE,				"knife" },
	{ WEAPON_C4,				"c4" },
	{ WEAPON_FLASHBANG,			"flashbang" },
	{ WEAPON_SMOKEGRENADE,		"smokegrenade" },
	{ WEAPON_SMOKEGRENADE,		"sgren" },
	{ WEAPON_HEGRENADE,			"hegrenade" },
	{ WEAPON_HEGRENADE,			"hegren" },

	// not sure any of these are needed
	{ WEAPON_SHIELDGUN,			"shield" },
	{ WEAPON_SHIELDGUN,			"shieldgun" },
	{ WEAPON_KEVLAR,			"kevlar" },
	{ WEAPON_ASSAULTSUIT,		"assaultsuit" },
	{ WEAPON_NVG,				"nightvision" },
	{ WEAPON_NVG,				"nvg" },

	{ WEAPON_NONE,				"none" },
};


bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the translated alias.
//
const char * GetTranslatedWeaponAlias( const char *szAlias )
{
	for ( int i = 0; i < ARRAYSIZE(s_WeaponAliasTranslationInfo); ++i )
	{
		if ( Q_stricmp(s_WeaponAliasTranslationInfo[i].alias, szAlias) == 0 )
		{
			return s_WeaponAliasTranslationInfo[i].translatedAlias;
		}
	}

	return szAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a translated alias, return the alias.
//
const char * GetWeaponAliasFromTranslated(const char *translatedAlias)
{
	int i = 0;
	const WeaponAliasTranslationInfoStruct *info = &(s_WeaponAliasTranslationInfo[i]);

	while (info->alias[0] != 0)
	{
		if (Q_stricmp(translatedAlias, info->translatedAlias) == 0)
		{
			return info->alias;
		}
		info = &(s_WeaponAliasTranslationInfo[++i]);
	}

	return translatedAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
CSWeaponID AliasToWeaponID( const char *szAlias )
{
	if ( szAlias )
	{
		for ( int i=0; i < ARRAYSIZE(s_weaponAliasInfo); ++i)
		{
			if ( Q_stricmp( s_weaponAliasInfo[i].alias, szAlias) == 0 )
				return s_weaponAliasInfo[i].id;
		}
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	for ( int i=0; i < ARRAYSIZE(s_weaponAliasInfo); ++i)
	{
		if ( s_weaponAliasInfo[i].id == id )
			return s_weaponAliasInfo[i].alias;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a primary weapon
//
bool IsPrimaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
	{
		return pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a secondary weapon
//
bool IsSecondaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
		return pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL;

	return false;
}

#ifdef CLIENT_DLL
int GetShellForAmmoType( const char *ammoname )
{
	if ( !Q_strcmp( BULLET_PLAYER_762MM, ammoname ) )
		return CS_SHELL_762NATO;

	if ( !Q_strcmp( BULLET_PLAYER_556MM, ammoname ) )
		return CS_SHELL_556;

	if ( !Q_strcmp( BULLET_PLAYER_338MAG, ammoname ) )
		return CS_SHELL_338MAG;

	if ( !Q_strcmp( BULLET_PLAYER_BUCKSHOT, ammoname ) )
		return CS_SHELL_12GAUGE;

	if ( !Q_strcmp( BULLET_PLAYER_57MM, ammoname ) )
		return CS_SHELL_57;

	// default 9 mm
	return CS_SHELL_9MM;
}
#endif


// ----------------------------------------------------------------------------- //
// CWeaponCSBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBase, DT_WeaponCSBase )

BEGIN_NETWORK_TABLE( CWeaponCSBase, DT_WeaponCSBase )
#if !defined( CLIENT_DLL )
SendPropInt( SENDINFO( m_weaponMode ), 1, SPROP_UNSIGNED ),
SendPropFloat(SENDINFO(m_fAccuracyPenalty) ),
// world weapon models have no aminations
SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
//	SendPropExclude( "DT_LocalActiveWeaponData", "m_flTimeWeaponIdle" ),
#else
RecvPropInt( RECVINFO( m_weaponMode ) ),
RecvPropFloat( RECVINFO(m_fAccuracyPenalty)),
#endif
END_NETWORK_TABLE()

#if defined(CLIENT_DLL)
BEGIN_PREDICTION_DATA( CWeaponCSBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextPrimaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextSecondaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_bDelayFire, FIELD_BOOLEAN, 0 ),
	DEFINE_PRED_FIELD( m_flAccuracy, FIELD_FLOAT, 0 ),
	DEFINE_PRED_FIELD( m_weaponMode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_fAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.00005f ),
END_PREDICTION_DATA()
#endif


LINK_ENTITY_TO_CLASS( weapon_cs_base, CWeaponCSBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponCSBase )

		//DEFINE_FUNCTION( DefaultTouch ),
		DEFINE_THINKFUNC( FallThink )

	END_DATADESC()

#endif

#if defined( CLIENT_DLL )
	ConVar cl_crosshaircolor( "cl_crosshaircolor", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Set crosshair color: 0=green, 1=red, 2=blue, 3=yellow, 4=cyan, 5=custom" );
	ConVar cl_dynamiccrosshair( "cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enables dynamic crosshair; 0=off, 1=normal behavior (based on actual weapon accuracy), 2=legacy simulated dynamic behavior, 3=legacy simulated static behavior" );
	ConVar cl_crosshairspreadscale( "cl_crosshairspreadscale", "0.3", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	ConVar cl_scalecrosshair( "cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable crosshair scaling (deprecated)" );
	ConVar cl_crosshairscale( "cl_crosshairscale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair scaling factor (deprecated)" );
	ConVar cl_crosshairalpha( "cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshairusealpha( "cl_crosshairusealpha", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshairsize( "cl_crosshairsize", "5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshairthickness( "cl_crosshairthickness", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshairdot( "cl_crosshairdot", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshaircolor_r( "cl_crosshaircolor_r", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshaircolor_g( "cl_crosshaircolor_g", "250", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshaircolor_b( "cl_crosshaircolor_b", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

#if ALLOW_WEAPON_SPREAD_DISPLAY
	ConVar weapon_debug_spread_show( "weapon_debug_spread_show", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "Enables display of weapon accuracy; 1: show accuracy box, 2: show box with recoil offset" );
	ConVar weapon_debug_spread_gap( "weapon_debug_spread_gap", "0.67", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY );
#endif

	// [paquin] make sure crosshair scales independent of frame rate
	// unless legacy cvar is set
	ConVar cl_legacy_crosshair_recoil( "cl_legacy_crosshair_recoil", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable legacy framerate dependent crosshair recoil");

	// use old scaling behavior
	ConVar cl_legacy_crosshair_scale( "cl_legacy_crosshair_scale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable legacy crosshair scaling");

void DrawCrosshairRect( int x0, int y0, int x1, int y1, bool bAdditive )
{
	if ( bAdditive )
	{
		vgui::surface()->DrawTexturedRect( x0, y0, x1, y1 );
	}
	else
	{
		// Alpha-blended crosshair
		vgui::surface()->DrawFilledRect( x0, y0, x1, y1 );
	}
}

#endif

// must be included after the above macros
#ifndef CLIENT_DLL
	#include "cs_bot.h"
#endif


// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation.
// ----------------------------------------------------------------------------- //
CWeaponCSBase::CWeaponCSBase()
{
	SetPredictionEligible( true );
	m_bDelayFire = true;
	m_nextPrevOwnerTouchTime = 0.0;
	m_prevOwner = NULL;
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

#ifdef CLIENT_DLL
	m_iCrosshairTextureID = 0;
#else
	m_iDefaultExtraAmmo = 0;
#endif

	m_fAccuracyPenalty = 0.0f;

	m_weaponMode = Primary_Mode;
}


#ifndef CLIENT_DLL
bool CWeaponCSBase::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue( szKeyName, szValue ) )
	{
		if ( FStrEq( szKeyName, "ammo" ) )
		{
			int bullets = atoi( szValue );
			if ( bullets < 0 )
				return false;

			m_iDefaultExtraAmmo = bullets;

			return true;
		}
	}

	return false;
}
#endif


bool CWeaponCSBase::IsPredicted() const
{
	return true;
}


bool CWeaponCSBase::IsPistol() const
{
	return GetCSWpnData().m_WeaponType == WEAPONTYPE_PISTOL;
}


bool CWeaponCSBase::IsFullAuto() const
{
	return GetCSWpnData().m_bFullAuto;
}


bool CWeaponCSBase::PlayEmptySound()
{
	//MIKETODO: certain weapons should override this to make it empty:
	//	C4
	//	Flashbang
	//	HE Grenade
	//	Smoke grenade

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	if ( IsPistol() )
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Pistol" );
	}
	else
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	}

	return 0;
}

CCSPlayer* CWeaponCSBase::GetPlayerOwner() const
{
	return dynamic_cast< CCSPlayer* >( GetOwner() );
}

//=============================================================================
// HPE_BEGIN:
//=============================================================================

//[dwenger] Accessors for the prior owner list
void CWeaponCSBase::AddToPriorOwnerList(CCSPlayer* pPlayer)
{
    if ( !IsAPriorOwner( pPlayer ) )
    {
        // Add player to prior owner list
        m_PriorOwners.AddToTail( pPlayer );
    }
}

bool CWeaponCSBase::IsAPriorOwner(CCSPlayer* pPlayer)
{
    return (m_PriorOwners.Find( pPlayer ) != -1);
}

//=============================================================================
// HPE_END
//=============================================================================


void CWeaponCSBase::SecondaryAttack( void )
{
#ifndef CLIENT_DLL
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( pPlayer->HasShield() == false )
		 BaseClass::SecondaryAttack();
	else
	{
		pPlayer->SetShieldDrawnState( !pPlayer->IsShieldDrawn() );

		if ( pPlayer->IsShieldDrawn() )
			 SendWeaponAnim( ACT_SHIELD_UP );
		else
			 SendWeaponAnim( ACT_SHIELD_DOWN );

		m_flNextSecondaryAttack = gpGlobals->curtime + 0.4;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.4;
	}
#endif
}

bool CWeaponCSBase::SendWeaponAnim( int iActivity )
{
#ifdef CS_SHIELD_ENABLED
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer && pPlayer->HasShield() )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel( 1 );

		if ( vm == NULL )
			return false;

		vm->SetWeaponModel( SHIELD_VIEW_MODEL, this );

		int	idealSequence = vm->SelectWeightedSequence( (Activity)iActivity );

		if ( idealSequence >= 0 )
		{
			vm->SendViewModelMatchingSequence( idealSequence );
		}
	}
#endif

	return BaseClass::SendWeaponAnim( iActivity );
}


void CWeaponCSBase::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	UpdateAccuracyPenalty();

	UpdateShieldState();

	if ((m_bInReload) && (pPlayer->m_flNextAttack <= gpGlobals->curtime))
	{
		// complete the reload.
		int j = MIN( GetMaxClip1() - m_iClip1, pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) );

		// Add them to the clip
		m_iClip1 += j;
		pPlayer->RemoveAmmo( j, m_iPrimaryAmmoType );

		m_bInReload = false;
	}

	if ((pPlayer->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		if ( pPlayer->HasShield() )
			CWeaponCSBase::SecondaryAttack();
		else
			SecondaryAttack();

		pPlayer->m_nButtons &= ~IN_ATTACK2;
	}
	else if ((pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime ))
	{
		if ( CSGameRules()->IsFreezePeriod() )	// Can't shoot during the freeze period
			return;

		if ( pPlayer->m_bIsDefusing )
			return;

		if ( pPlayer->State_Get() != STATE_ACTIVE )
			return;

		if ( pPlayer->IsShieldDrawn() ) 
			return;

		// we have to reset the FireOnEmpty flag before we can fire on an empty clip
		if ( m_iClip1 == 0 && !m_bFireOnEmpty )
			return;

		// don't repeat fire if this is not a full auto weapon 
		if ( pPlayer->m_iShotsFired > 0 && !IsFullAuto() )
			return;

#if !defined(CLIENT_DLL)
		// allow the bots to react to the gunfire
		if ( GetCSWpnData().m_WeaponType != WEAPONTYPE_GRENADE )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( (HasAmmo()) ? "weapon_fire" : "weapon_fire_on_empty" );
			if( event )
			{
				const char *weaponName = STRING( m_iClassname );
				if ( strncmp( weaponName, "weapon_", 7 ) == 0 )
				{
					weaponName += 7;
				}

				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetString( "weapon", weaponName );
				gameeventmanager->FireEvent( event );
			}
		}
#endif
		PrimaryAttack();
	}
	else if ( pPlayer->m_nButtons & IN_RELOAD && GetMaxClip1() != WEAPON_NOCLIP && !m_bInReload && m_flNextPrimaryAttack < gpGlobals->curtime)
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.

		//MIKETODO: add code for shields...
		//if ( !FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )

		if ( !pPlayer->IsShieldDrawn() )
		{
			 if ( Reload() )
			 {
#ifndef CLIENT_DLL
				 // allow the bots to react to the reload
				 IGameEvent * event = gameeventmanager->CreateEvent( "weapon_reload" );
				 if( event )
				 {
					 event->SetInt( "userid", pPlayer->GetUserID() );
					 gameeventmanager->FireEvent( event );
				 }
#endif
			 }
		}
	}
	else if ( !(pPlayer->m_nButtons & (IN_ATTACK|IN_ATTACK2) ) )
	{
		if ( weapon_accuracy_model.GetInt() == 2 )
		{
			// Fire button not down -- reset the shots fired count
			if ( pPlayer->m_iShotsFired > 0 && ( !IsFullAuto() || m_iClip1 == 0 ) )
			{
				pPlayer->m_iShotsFired = 0;
			}
		}

		// The following code prevents the player from tapping the firebutton repeatedly
		// to simulate full auto and retaining the single shot accuracy of single fire
		if ( m_bDelayFire )
		{
			m_bDelayFire = false;

			if (pPlayer->m_iShotsFired > 15)
				pPlayer->m_iShotsFired = 15;

			m_flDecreaseShotsFired = gpGlobals->curtime + 0.4;
		}

		m_bFireOnEmpty = true;

		// if it's a pistol then set the shots fired to 0 after the player releases a button
		if ( IsPistol() )
		{
			pPlayer->m_iShotsFired = 0;
		}
		else
		{
			if ( (pPlayer->m_iShotsFired > 0) && (m_flDecreaseShotsFired < gpGlobals->curtime)	)
			{
				m_flDecreaseShotsFired = gpGlobals->curtime + 0.0225;
				pPlayer->m_iShotsFired--;
			}
		}

		if ( (!IsUseable() && m_flNextPrimaryAttack < gpGlobals->curtime) )
		{
			// Intentionally blank -- used to switch weapons here
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 == 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}
}


void CWeaponCSBase::ItemBusyFrame()
{
	UpdateAccuracyPenalty();

	BaseClass::ItemBusyFrame();
}


float CWeaponCSBase::GetInaccuracy() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return 0.0f;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float fMaxSpeed = GetMaxSpeed();
	if ( fMaxSpeed == 0.0f )
		fMaxSpeed = GetCSWpnData().m_flMaxSpeed;

	return m_fAccuracyPenalty + 
		RemapValClamped(pPlayer->GetAbsVelocity().Length2D(), 
		fMaxSpeed * CS_PLAYER_SPEED_DUCK_MODIFIER, 
		fMaxSpeed * 0.95f,							// max out at 95% of run speed to avoid jitter near max speed
		0.0f, weaponInfo.m_fInaccuracyMove[m_weaponMode]);
}


float CWeaponCSBase::GetSpread() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
		return 0.0f;

	return GetCSWpnData().m_fSpread[m_weaponMode];
}


float CWeaponCSBase::GetMaxSpeed() const
{
	// The weapon should have set this in its constructor.
	float flRet = GetCSWpnData().m_flMaxSpeed;
	Assert( flRet > 1 );
	return flRet;
}

const CCSWeaponInfo &CWeaponCSBase::GetCSWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CCSWeaponInfo *pCSInfo;

	#ifdef _DEBUG
		pCSInfo = dynamic_cast< const CCSWeaponInfo* >( pWeaponInfo );
		Assert( pCSInfo );
	#else
		pCSInfo = static_cast< const CCSWeaponInfo* >( pWeaponInfo );
	#endif

	return *pCSInfo;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CWeaponCSBase::GetViewModel( int /*viewmodelindex = 0 -- this is ignored in the base class here*/ ) const
{
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == NULL )
		 return BaseClass::GetViewModel();

	if ( pOwner->HasShield() && GetCSWpnData().m_bCanUseWithShield )
		 return GetCSWpnData().m_szShieldViewModel;
	else
		 return GetWpnData().szViewModel;

	return BaseClass::GetViewModel();

}

void CWeaponCSBase::Precache( void )
{
	BaseClass::Precache();

#ifdef CS_SHIELD_ENABLED
	if ( GetCSWpnData().m_bCanUseWithShield )
	{
		 PrecacheModel( GetCSWpnData().m_szShieldViewModel );
	}
#endif

	PrecacheScriptSound( "Default.ClipEmpty_Pistol" );
	PrecacheScriptSound( "Default.ClipEmpty_Rifle" );

	PrecacheScriptSound( "Default.Zoom" );
}

Activity CWeaponCSBase::GetDeployActivity( void )
{
	return ACT_VM_DRAW;
}

bool CWeaponCSBase::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	// Msg( "deploy %s at %f\n", GetClassname(), gpGlobals->curtime );
	CCSPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
	{
		return false;
	}

	pOwner->SetAnimationExtension( szAnimExt );

	SetViewModel();
	SendWeaponAnim( GetDeployActivity() );

	pOwner->SetNextAttack( gpGlobals->curtime + SequenceDuration() );
	m_flNextPrimaryAttack	= gpGlobals->curtime;
	m_flNextSecondaryAttack	= gpGlobals->curtime;

	SetWeaponVisible( true );
	pOwner->SetShieldDrawnState( false );

	if ( pOwner->HasShield() == true )
		 SetWeaponModelIndex( SHIELD_WORLD_MODEL);
	else
		 SetWeaponModelIndex( szWeaponModel );

	return true;
}

void CWeaponCSBase::UpdateShieldState( void )
{
	//empty by default.
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == NULL )
		 return;

	//ADRIANTODO
	//Make the hitbox set switches here!!!
	if ( pOwner->HasShield() == false )
	{

		pOwner->SetShieldDrawnState( false );
		//pOwner->SetHitBoxSet( 0 );
		return;
	}
	else
	{
		//pOwner->SetHitBoxSet( 1 );
	}
}

void CWeaponCSBase::SetWeaponModelIndex( const char *pName )
{
 	 m_iWorldModelIndex = modelinfo->GetModelIndex( pName );
}

bool CWeaponCSBase::CanBeSelected( void )
{
	if ( !VisibleInWeaponSelection() )
		return false;

	return true;
}

bool CWeaponCSBase::CanDeploy( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer->HasShield() && GetCSWpnData().m_bCanUseWithShield == false )
		 return false;

	return BaseClass::CanDeploy();
}

float CWeaponCSBase::CalculateNextAttackTime( float fCycleTime )
{
	float fCurAttack = m_flNextPrimaryAttack;
	float fDeltaAttack = gpGlobals->curtime - fCurAttack;
	if ( fDeltaAttack < 0 || fDeltaAttack > gpGlobals->interval_per_tick )
	{
		fCurAttack = gpGlobals->curtime;
	}
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = fCurAttack + fCycleTime;

	return fCurAttack;
}

bool CWeaponCSBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer )
		pPlayer->SetFOV( pPlayer, 0 ); // reset the default FOV.

	if ( pPlayer )
		pPlayer->SetShieldDrawnState( false );

	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponCSBase::Deploy()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

#ifdef CLIENT_DLL
	m_iAlpha =  80;
	if ( pPlayer )
	{
		pPlayer->m_iLastZoom = 0;
		pPlayer->SetFOV( pPlayer, 0 );
	}
#else

	m_flDecreaseShotsFired = gpGlobals->curtime;


	if ( pPlayer )
	{
		pPlayer->m_iShotsFired = 0;
		pPlayer->m_bResumeZoom = false;
		pPlayer->m_iLastZoom = 0;
		pPlayer->SetFOV( pPlayer, 0 );
	}
#endif

	m_fAccuracyPenalty = 0.0f;

	return BaseClass::Deploy();
}

#ifndef CLIENT_DLL
bool CWeaponCSBase::IsRemoveable()
{
	if ( BaseClass::IsRemoveable() == true )
	{
		if ( m_nextPrevOwnerTouchTime > gpGlobals->curtime )
		{
			return false;
		}
	}

	return BaseClass::IsRemoveable();
}
#endif

void CWeaponCSBase::Drop(const Vector &vecVelocity)
{

#ifdef CLIENT_DLL
	BaseClass::Drop(vecVelocity);
	return;
#else

	// Once somebody drops a gun, it's fair game for removal when/if
	// a game_weapon_manager does a cleanup on surplus weapons in the
	// world.
	SetRemoveable( true );

	StopAnimation();
	StopFollowingEntity( );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	// clear follow stuff, setup for collision
	SetGravity(1.0);
	m_iState = WEAPON_NOT_CARRIED;
	RemoveEffects( EF_NODRAW );
	FallInit();
	SetGroundEntity( NULL );

	m_bInReload = false; // stop reloading

	SetThink( NULL );
	m_nextPrevOwnerTouchTime = gpGlobals->curtime + 0.8f;
	m_prevOwner = GetPlayerOwner();

	SetTouch(&CWeaponCSBase::DefaultTouch);

	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj != NULL )
	{
		AngularImpulse	angImp( 200, 200, 200 );
		pObj->AddVelocity( &vecVelocity, &angImp );
	}
	else
	{
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( gpGlobals->curtime );

	SetOwnerEntity( NULL );
	SetOwner( NULL );
#endif
}

// whats going on here is that if the player drops this weapon, they shouldn't take it back themselves
// for a little while.  But if they throw it at someone else, the other player should get it immediately.
void CWeaponCSBase::DefaultTouch(CBaseEntity *pOther)
{
	if ((m_prevOwner != NULL) && (pOther == m_prevOwner) && (gpGlobals->curtime < m_nextPrevOwnerTouchTime))
	{
		return;
	}

	BaseClass::DefaultTouch(pOther);
}

#if defined( CLIENT_DLL )

	//-----------------------------------------------------------------------------
	// Purpose: Draw the weapon's crosshair
	//-----------------------------------------------------------------------------
	void CWeaponCSBase::DrawCrosshair()
	{
		if ( !crosshair.GetInt() )
			return;

		CHudCrosshair *pCrosshair = GET_HUDELEMENT( CHudCrosshair );

		if ( !pCrosshair )
			return;

		// clear crosshair
		pCrosshair->SetCrosshair( 0, Color( 255, 255, 255, 255 ) );

		CCSPlayer* pPlayer = (CCSPlayer*)C_BasePlayer::GetLocalPlayer();

		if ( !pPlayer )
			return;

		// localplayer must be owner if not in Spec mode
		Assert( (pPlayer == GetPlayerOwner()) || ( pPlayer->GetObserverMode()==OBS_MODE_IN_EYE) );

		// Draw the targeting zone around the pCrosshair
		if ( pPlayer->IsInVGuiInputMode() )
			return;

		int	r, g, b;
		switch ( cl_crosshaircolor.GetInt() )
		{
		case 0 :	r = 50;		g = 250;	b = 50;		break;
		case 1 :	r = 250;	g = 50;		b = 50;		break;
		case 2 :	r = 50;		g = 50;		b = 250;	break;
		case 3 :	r = 250;	g = 250;	b = 50;		break;
		case 4 :	r = 50;		g = 250;	b = 250;	break;
		case 5 :
			r = cl_crosshaircolor_r.GetInt();
			g = cl_crosshaircolor_g.GetInt();
			b = cl_crosshaircolor_b.GetInt();
			break;
		default :	r = 50;		g = 250;	b = 50;		break;
		}

		// if user is using nightvision, make the crosshair red.
		if (pPlayer->m_bNightVisionOn)
		{
			r = 250;
			g = 50;
			b = 50;
		}

		int alpha = clamp( cl_crosshairalpha.GetInt(), 0, 255 );
		vgui::surface()->DrawSetColor( r, g, b, alpha );

		if ( !m_iCrosshairTextureID )
		{
			CHudTexture *pTexture = gHUD.GetIcon( "whiteAdditive" );
			if ( pTexture )
			{
				m_iCrosshairTextureID = pTexture->textureId;
			}
		}

		bool bAdditive = !cl_crosshairusealpha.GetBool() && !pPlayer->m_bNightVisionOn;
		if ( bAdditive )
		{
			vgui::surface()->DrawSetColor( r, g, b, 200 );
			vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
		}

		if ( pPlayer->HasShield() && pPlayer->IsShieldDrawn() == true )
			 return;

		// no crosshair for sniper rifles
		bool bCrosshairVisible = crosshair.GetBool() && GetCSWpnData().m_WeaponType != WEAPONTYPE_SNIPER_RIFLE;

		if ( !bCrosshairVisible 
#if ALLOW_WEAPON_SPREAD_DISPLAY
			&& !weapon_debug_spread_show.GetBool()
#endif
			)
			 return;

		float fHalfFov = DEG2RAD(pPlayer->GetFOV()) * 0.5f;

		int iCrosshairDistance;
		int iBarSize = RoundFloatToInt(YRES(cl_crosshairsize.GetFloat()));
		int iBarThickness = MAX( 1, RoundFloatToInt(YRES(cl_crosshairthickness.GetFloat())));

		switch ( cl_dynamiccrosshair.GetInt() )
		{
		case 0:
		default:
			{
				// static crosshair
				float fSpread = (GetCSWpnData().m_fSpread[m_weaponMode] + GetCSWpnData().m_fInaccuracyStand[m_weaponMode]) * 320.0f / tanf(fHalfFov);
				iCrosshairDistance = MAX( 0, RoundFloatToInt( YRES( fSpread * cl_crosshairspreadscale.GetFloat() ) ) );
			}
			break;

		case 1:
			{
				float fSpread = (GetInaccuracy() + GetSpread()) * 320.0f / tanf(fHalfFov);
				iCrosshairDistance = MAX( 0, RoundFloatToInt( YRES( fSpread * cl_crosshairspreadscale.GetFloat() ) ) );
			}
			break;

		case 2:
		case 3:
			{
				float fCrosshairDistanceGoal = GetCSWpnData().m_iCrosshairMinDistance; // The minimum distance the crosshair can achieve...

				// legacy dynamic crosshair
				if ( cl_dynamiccrosshair.GetInt() == 2 )
				{
					if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
						fCrosshairDistanceGoal *= 2.0f;
					else if ( pPlayer->GetFlags() & FL_DUCKING )
						fCrosshairDistanceGoal *= 0.5f;
					else if ( pPlayer->GetAbsVelocity().Length() > 100 )
						fCrosshairDistanceGoal *= 1.5f;
				}

				// [jpaquin] changed to only bump up the crosshair size if the player is still shooting or is spectating someone else
				int iDeltaDistance = GetCSWpnData().m_iCrosshairDeltaDistance; // Amount by which the crosshair expands when shooting (per frame)
				if ( pPlayer->m_iShotsFired > m_iAmmoLastCheck && (pPlayer->m_nButtons & (IN_ATTACK|IN_ATTACK2)) )
					fCrosshairDistanceGoal += iDeltaDistance;

				m_iAmmoLastCheck = pPlayer->m_iShotsFired;

				if ( m_flCrosshairDistance > fCrosshairDistanceGoal )
				{
					// [jpaquin] if we're not in legacy crosshair mode, use an exponential decay function so
					// that the crosshair shrinks at the same rate regardless of the frame rate
					if ( !cl_legacy_crosshair_recoil.GetBool() )
					{
						// .44888 on the next line makes the decay very close to what old method produces at 100fps.
						m_flCrosshairDistance = Lerp(expf(-gpGlobals->frametime / 0.44888f), fCrosshairDistanceGoal, m_flCrosshairDistance);
					}
					else
					{
						m_flCrosshairDistance -= 0.1f + m_flCrosshairDistance * 0.013;
					}
				}

				// clamp max crosshair expansion
				m_flCrosshairDistance = clamp(m_flCrosshairDistance, fCrosshairDistanceGoal, 25.0f);

				if ( cl_legacy_crosshair_scale.GetBool() )
				{
					//scale bar size to the resolution
					int crosshairScale = cl_crosshairscale.GetInt();
					if ( crosshairScale < 1 )
					{
						if ( ScreenHeight() <= 600 )
						{
							crosshairScale = 600;
						}
						else if ( ScreenHeight() <= 768 )
						{
							crosshairScale = 768;
						}
						else
						{
							crosshairScale = 1200;
						}
					}

					float scale;
					if( cl_scalecrosshair.GetBool() == false )
					{
						scale = 1.0f;
					}
					else
					{
						scale = (float)ScreenHeight() / (float)crosshairScale;
					}

					// calculate the inner distance of the crosshair in current screen units
					iCrosshairDistance = (int)ceil( m_flCrosshairDistance * scale );

					iBarSize = XRES(5); //  + (iCrosshairDistance - fCrosshairDistanceGoal) / 2;
					iBarSize = MAX( 1, (int)( (float)iBarSize * scale ) );
					iBarThickness = MAX( 1, (int)floor( scale + 0.5f ) );
				}
				else
				{
					iCrosshairDistance = RoundFloatToInt(m_flCrosshairDistance * ScreenHeight() / 1200.0f);
				}
			}
			break;
		}

		int iCenterX = ScreenWidth() / 2;
		int iCenterY = ScreenHeight() / 2;

		if ( bCrosshairVisible )
		{
			// draw horizontal crosshair lines
			int iInnerLeft	= iCenterX - iCrosshairDistance - iBarThickness / 2;
			int iInnerRight	= iInnerLeft + 2 * iCrosshairDistance + iBarThickness;
			int iOuterLeft	= iInnerLeft - iBarSize;
			int iOuterRight	= iInnerRight + iBarSize;
			int y0 = iCenterY - iBarThickness / 2;
			int y1 = y0 + iBarThickness;
			DrawCrosshairRect( iOuterLeft, y0, iInnerLeft, y1, bAdditive );
			DrawCrosshairRect( iInnerRight, y0, iOuterRight, y1, bAdditive );

			// draw vertical crosshair lines
			int iInnerTop		= iCenterY - iCrosshairDistance - iBarThickness / 2;
			int iInnerBottom	= iInnerTop + 2 * iCrosshairDistance + iBarThickness;
			int iOuterTop		= iInnerTop - iBarSize;
			int iOuterBottom	= iInnerBottom + iBarSize;
			int x0 = iCenterX - iBarThickness / 2;
			int x1 = x0 + iBarThickness;
			DrawCrosshairRect( x0, iOuterTop, x1, iInnerTop, bAdditive );
			DrawCrosshairRect( x0, iInnerBottom, x1, iOuterBottom, bAdditive );

			// draw dot
			if ( cl_crosshairdot.GetBool() )
			{
				int x0 = iCenterX - iBarThickness / 2;
				int x1 = x0 + iBarThickness;
				int y0 = iCenterY - iBarThickness / 2;
				int y1 = y0 + iBarThickness;
				DrawCrosshairRect( x0, y0, x1, y1, bAdditive );
			}
		}

#if ALLOW_WEAPON_SPREAD_DISPLAY
		// show accuracy brackets
		if ( weapon_debug_spread_show.GetInt() == 1 || weapon_debug_spread_show.GetInt() == 2 )
		{
			if ( weapon_debug_spread_show.GetInt() == 2 )
			{
				const QAngle& punchAngles = pPlayer->GetPunchAngle();
				Vector vecDirShooting;
				AngleVectors( punchAngles, &vecDirShooting );

				float iOffsetX = RoundFloatToInt(YRES(vecDirShooting.y * 320.0f / tanf(fHalfFov)));
				float iOffsetY = RoundFloatToInt(YRES(vecDirShooting.z * 320.0f / tanf(fHalfFov)));

				iCenterX -= iOffsetX;
				iCenterY -= iOffsetY;
			}

			// colors
			r = 250;
			g = 250;
			b = 50;
			vgui::surface()->DrawSetColor( r, g, b, alpha );

			int iBarThickness = MAX( 1, RoundFloatToInt(YRES(cl_crosshairthickness.GetFloat())));

			float fSpreadDistance = (GetInaccuracy() + GetSpread()) * 320.0f / tanf(fHalfFov);
			int iSpreadDistance = RoundFloatToInt(YRES(fSpreadDistance));

			// draw vertical spread lines
			int iInnerLeft	= iCenterX - iSpreadDistance;
			int iInnerRight	= iCenterX + iSpreadDistance;
			int iOuterLeft	= iInnerLeft - iBarThickness;
			int iOuterRight	= iInnerRight + iBarThickness;
			int iInnerTop		= iCenterY - iSpreadDistance;
			int iInnerBottom	= iCenterY + iSpreadDistance;
			int iOuterTop		= iInnerTop - iBarThickness;
			int iOuterBottom	= iInnerBottom + iBarThickness;

			int iGap = RoundFloatToInt(weapon_debug_spread_gap.GetFloat() * iSpreadDistance);

			// draw horizontal lines
			DrawCrosshairRect( iOuterLeft, iOuterTop, iCenterX - iGap, iInnerTop, bAdditive );
			DrawCrosshairRect( iCenterX + iGap, iOuterTop, iOuterRight, iInnerTop, bAdditive );
			DrawCrosshairRect( iOuterLeft, iInnerBottom, iCenterX - iGap, iOuterBottom, bAdditive );
			DrawCrosshairRect( iCenterX + iGap, iInnerBottom, iOuterRight, iOuterBottom, bAdditive );

			// draw vertical lines
			DrawCrosshairRect( iOuterLeft, iOuterTop, iInnerLeft, iCenterY - iGap, bAdditive );
			DrawCrosshairRect( iOuterLeft, iCenterY + iGap, iInnerLeft, iOuterBottom, bAdditive );
			DrawCrosshairRect( iInnerRight, iOuterTop, iOuterRight, iCenterY - iGap, bAdditive );
			DrawCrosshairRect( iInnerRight, iCenterY + iGap, iOuterRight, iOuterBottom, bAdditive );
		}
#endif
	}

	void CWeaponCSBase::OnDataChanged( DataUpdateType_t type )
	{
		BaseClass::OnDataChanged( type );

		if ( GetPredictable() && !ShouldPredict() )
			ShutdownPredictable();
	}


	bool CWeaponCSBase::ShouldPredict()
	{
		if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	void CWeaponCSBase::ProcessMuzzleFlashEvent()
	{
		// This is handled from the player's animstate, so it can match up to the beginning of the fire animation
	}


	bool CWeaponCSBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 5001 )
		{
			C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
			if( pPlayer && pPlayer->GetFOV() < pPlayer->GetDefaultFOV() && HideViewModelWhenZoomed() )
				return true;

			CEffectData data;
			data.m_fFlags = 0;
			data.m_hEntity = pViewModel->GetRefEHandle();
			data.m_nAttachmentIndex = 1;
			data.m_flScale = GetCSWpnData().m_flMuzzleScale;

			switch( GetMuzzleFlashStyle() )
			{
			case CS_MUZZLEFLASH_NONE:
				break;

			case CS_MUZZLEFLASH_X:
				{
					DispatchEffect( "CS_MuzzleFlash_X", data );
				}
				break;

			case CS_MUZZLEFLASH_NORM:
			default:
				{
					DispatchEffect( "CS_MuzzleFlash", data );
				}
				break;
			}

			return true;
		}

		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	int CWeaponCSBase::GetMuzzleFlashStyle( void )
	{
		return GetCSWpnData().m_iMuzzleFlashStyle;
	}

	int CWeaponCSBase::GetMuzzleAttachment( void )
	{
		return LookupAttachment( "muzzle_flash" );
	}

#else

	//-----------------------------------------------------------------------------
	// Purpose: Get the accuracy derived from weapon and player, and return it
	//-----------------------------------------------------------------------------
	const Vector& CWeaponCSBase::GetBulletSpread()
	{
		static Vector cone = VECTOR_CONE_8DEGREES;
		return cone;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Match the anim speed to the weapon speed while crouching
	//-----------------------------------------------------------------------------
	float CWeaponCSBase::GetDefaultAnimSpeed()
	{
		return 1.0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Draw the laser rifle effect
	//-----------------------------------------------------------------------------
	void CWeaponCSBase::BulletWasFired( const Vector &vecStart, const Vector &vecEnd )
	{
	}


	bool CWeaponCSBase::ShouldRemoveOnRoundRestart()
	{
		if ( GetPlayerOwner() )
			return false;
		else
			return true;
	}


    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Handle round restart processing for the weapon.
    //=============================================================================

    void CWeaponCSBase::OnRoundRestart()
    {
        // Clear out the list of prior owners
        m_PriorOwners.RemoveAll();
    }

    //=============================================================================
    // HPE_END
    //=============================================================================

	//=========================================================
	// Materialize - make a CWeaponCSBase visible and tangible
	//=========================================================
	void CWeaponCSBase::Materialize()
	{
		if ( IsEffectActive( EF_NODRAW ) )
		{
			// changing from invisible state to visible.
			RemoveEffects( EF_NODRAW );
			DoMuzzleFlash();
		}

		AddSolidFlags( FSOLID_TRIGGER );

		//SetTouch( &CWeaponCSBase::DefaultTouch );

		SetThink( NULL );

	}

	//=========================================================
	// AttemptToMaterialize - the item is trying to rematerialize,
	// should it do so now or wait longer?
	//=========================================================
	void CWeaponCSBase::AttemptToMaterialize()
	{
		float time = g_pGameRules->FlWeaponTryRespawn( this );

		if ( time == 0 )
		{
			Materialize();
			return;
		}

		SetNextThink( gpGlobals->curtime + time );
	}

	//=========================================================
	// CheckRespawn - a player is taking this weapon, should
	// it respawn?
	//=========================================================
	void CWeaponCSBase::CheckRespawn()
	{
		//GOOSEMAN : Do not respawn weapons!
		return;
	}


	//=========================================================
	// Respawn- this item is already in the world, but it is
	// invisible and intangible. Make it visible and tangible.
	//=========================================================
	CBaseEntity* CWeaponCSBase::Respawn()
	{
		// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
		// will decide when to make the weapon visible and touchable.
		CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), GetOwner() );

		if ( pNewWeapon )
		{
			pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
			pNewWeapon->SetTouch( NULL );// no touch
			pNewWeapon->SetThink( &CWeaponCSBase::AttemptToMaterialize );

			UTIL_DropToFloor( this, MASK_SOLID );

			// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
			// but when it should respawn is based on conditions belonging to the weapon that was taken.
			pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
		}
		else
		{
			Msg( "Respawn failed to create %s!\n", GetClassname() );
		}

		return pNewWeapon;
	}

	//-----------------------------------------------------------------------------
	// Purpose:
	//-----------------------------------------------------------------------------
	void CWeaponCSBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		
		if ( pPlayer )
		{
			m_OnPlayerUse.FireOutput( pActivator, pCaller );
		}
	}

	bool CWeaponCSBase::Reload()
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return false;

		pPlayer->m_iShotsFired = 0;

		bool retval = BaseClass::Reload();

		return retval;
	}

	void CWeaponCSBase::Spawn()
	{
		BaseClass::Spawn();

		// Override the bloat that our base class sets as it's a little bit bigger than we want.
		// If it's too big, you drop a weapon and its box is so big that you're still touching it
		// when it falls and you pick it up again right away.
		CollisionProp()->UseTriggerBounds( true, 30 );

		// Set this here to allow players to shoot dropped weapons
		SetCollisionGroup( COLLISION_GROUP_WEAPON );

		SetExtraAmmoCount( m_iDefaultExtraAmmo );	//Start with no additional ammo

		m_nextPrevOwnerTouchTime = 0.0;
		m_prevOwner = NULL;

        //=============================================================================
        // HPE_BEGIN:
        //=============================================================================

        // [tj] initialize donor of this weapon
        m_donor = NULL;
        m_donated = false;

		m_weaponMode = Primary_Mode;

        //=============================================================================
        // HPE_END
        //=============================================================================
	}

	bool CWeaponCSBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
	{
		if ( BaseClass::DefaultReload( iClipSize1, iClipSize2, iActivity ) )
		{
			SendReloadEvents();
			return true;
		}
		else
		{
			return false;
		}
	}

	void CWeaponCSBase::SendReloadEvents()
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( GetOwner() );
		if ( !pPlayer )
			return;

		// Send a message to any clients that have this entity to play the reload.
		CPASFilter filter( pPlayer->GetAbsOrigin() );
		filter.RemoveRecipient( pPlayer );

		UserMessageBegin( filter, "ReloadEffect" );
			WRITE_SHORT( pPlayer->entindex() );
		MessageEnd();

		// Make the player play his reload animation.
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
	}

#endif


bool CWeaponCSBase::DefaultPistolReload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
		return true;

	if ( !DefaultReload( GetCSWpnData().iDefaultClip1, 0, ACT_VM_RELOAD ) )
		return false;

	pPlayer->m_iShotsFired = 0;

	return true;
}

bool CWeaponCSBase::IsUseable()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( Clip1() <= 0 )
	{
		if ( pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 && GetMaxClip1() != -1 )
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type.
			return false;
		}
	}

	return true;
}


#if defined( CLIENT_DLL )

	float	g_lateralBob = 0;
	float	g_verticalBob = 0;

	static ConVar	cl_bobcycle( "cl_bobcycle","0.8", FCVAR_CHEAT );
	static ConVar	cl_bob( "cl_bob","0.002", FCVAR_CHEAT );
	static ConVar	cl_bobup( "cl_bobup","0.5", FCVAR_CHEAT );

	//-----------------------------------------------------------------------------
	// Purpose:
	// Output : float
	//-----------------------------------------------------------------------------
	float CWeaponCSBase::CalcViewmodelBob( void )
	{
		static	float bobtime;
		static	float lastbobtime;
		static  float lastspeed;
		float	cycle;

		CBasePlayer *player = ToBasePlayer( GetOwner() );
		//Assert( player );

		//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

		if ( ( !gpGlobals->frametime ) ||
			 ( player == NULL ) ||
			 ( cl_bobcycle.GetFloat() <= 0.0f ) ||
			 ( cl_bobup.GetFloat() <= 0.0f ) ||
			 ( cl_bobup.GetFloat() >= 1.0f ) )
		{
			//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
			return 0.0f;// just use old value
		}

		//Find the speed of the player
		float speed = player->GetLocalVelocity().Length2D();
		float flmaxSpeedDelta = MAX( 0, (gpGlobals->curtime - lastbobtime) * 320.0f );

		// don't allow too big speed changes
		speed = clamp( speed, lastspeed-flmaxSpeedDelta, lastspeed+flmaxSpeedDelta );
		speed = clamp( speed, -320, 320 );

		lastspeed = speed;

		//FIXME: This maximum speed value must come from the server.
		//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw



		float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );

		bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
		lastbobtime = gpGlobals->curtime;

		//Calculate the vertical bob
		cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
		cycle /= cl_bobcycle.GetFloat();

		if ( cycle < cl_bobup.GetFloat() )
		{
			cycle = M_PI * cycle / cl_bobup.GetFloat();
		}
		else
		{
			cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
		}

		g_verticalBob = speed*0.005f;
		g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

		g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

		//Calculate the lateral bob
		cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat()*2)*cl_bobcycle.GetFloat()*2;
		cycle /= cl_bobcycle.GetFloat()*2;

		if ( cycle < cl_bobup.GetFloat() )
		{
			cycle = M_PI * cycle / cl_bobup.GetFloat();
		}
		else
		{
			cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
		}

		g_lateralBob = speed*0.005f;
		g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
		g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );

		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;

	}

	//-----------------------------------------------------------------------------
	// Purpose:
	// Input  : &origin -
	//			&angles -
	//			viewmodelindex -
	//-----------------------------------------------------------------------------
	void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
	{
		Vector	forward, right;
		AngleVectors( angles, &forward, &right, NULL );

		CalcViewmodelBob();

		// Apply bob, but scaled down to 40%
		VectorMA( origin, g_verticalBob * 0.4f, forward, origin );

		// Z bob a bit more
		origin[2] += g_verticalBob * 0.1f;

		// bob the angles
		angles[ ROLL ]	+= g_verticalBob * 0.5f;
		angles[ PITCH ]	-= g_verticalBob * 0.4f;

		angles[ YAW ]	-= g_lateralBob  * 0.3f;

	//	VectorMA( origin, g_lateralBob * 0.2f, right, origin );
	}

#else

	void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
	{

	}

	float CWeaponCSBase::CalcViewmodelBob( void )
	{
		return 0.0f;
	}

#endif

#ifndef CLIENT_DLL
bool CWeaponCSBase::PhysicsSplash( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed )
{
	if ( rawSpeed > 20 )
	{

		float size = 4.0f;
		if ( !IsPistol() )
			size += 2.0f;

		// adjust splash size based on speed
		size += RemapValClamped( rawSpeed, 0, 400, 0, 3 );

		CEffectData	data;
 		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		data.m_flScale = random->RandomFloat( size, size + 1.0f );

		if ( GetWaterType() & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "gunshotsplash", data );

		return true;
	}

	return false;
}
#endif // !CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pPicker -
//-----------------------------------------------------------------------------
void CWeaponCSBase::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
#if !defined( CLIENT_DLL )
	RemoveEffects( EF_ITEM_BLINK );

	if( pNewOwner->IsPlayer() && pNewOwner->IsAlive() )
	{
		m_OnPlayerPickup.FireOutput(pNewOwner, this);

		// Play the pickup sound for 1st-person observers
		CRecipientFilter filter;
		for ( int i=0; i<gpGlobals->maxClients; ++i )
		{
			CBasePlayer *player = UTIL_PlayerByIndex(i);
			if ( player && !player->IsAlive() && player->GetObserverMode() == OBS_MODE_IN_EYE )
			{
				filter.AddRecipient( player );
			}
		}
		if ( filter.GetRecipientCount() )
		{
			CBaseEntity::EmitSound( filter, pNewOwner->entindex(), "Player.PickupWeapon" );
		}

		// Robin: We don't want to delete weapons the player has picked up, so
		// clear the name of the weapon. This prevents wildcards that are meant
		// to find NPCs finding weapons dropped by the NPCs as well.
		SetName( NULL_STRING );
	}

	// Someone picked me up, so make it so that I can't be removed.
	SetRemoveable( false );
#endif
}


void CWeaponCSBase::UpdateAccuracyPenalty()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float fNewPenalty = 0.0f;

	// on ladder?
	if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
	{
		fNewPenalty += weaponInfo.m_fInaccuracyStand[m_weaponMode] + weaponInfo.m_fInaccuracyLadder[m_weaponMode];
	}
	// in the air?
// 	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
// 	{
// 		fNewPenalty += weaponInfo.m_fInaccuracyStand[m_weaponMode] + weaponInfo.m_fInaccuracyJump[m_weaponMode];
// 	}
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING) )
	{
		fNewPenalty += weaponInfo.m_fInaccuracyCrouch[m_weaponMode];
	}
	else
	{
		fNewPenalty += weaponInfo.m_fInaccuracyStand[m_weaponMode];
	}

	if ( m_bInReload )
	{
 		fNewPenalty += weaponInfo.m_fInaccuracyReload;
	}

	if ( fNewPenalty > m_fAccuracyPenalty )
	{
		m_fAccuracyPenalty = fNewPenalty;
	}
	else
	{
		float fDecayFactor;

		if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
		{
			fDecayFactor = logf(10.0f) / weaponInfo.m_fRecoveryTimeStand;
		}
		else if ( !FBitSet(pPlayer->GetFlags(), FL_ONGROUND) )	// in air
		{
			// enforce a large recovery speed penalty (300%) for players in the air; this helps to provide
			// comparable in-air accuracy to the old weapon model
			fDecayFactor = logf(10.0f) / (weaponInfo.m_fRecoveryTimeCrouch * 3.0f);
		}
		else if ( FBitSet(pPlayer->GetFlags(), FL_DUCKING) )
		{
			fDecayFactor = logf(10.0f) / weaponInfo.m_fRecoveryTimeCrouch;
		}
		else
		{
			fDecayFactor = logf(10.0f) / weaponInfo.m_fRecoveryTimeStand;
		}
		m_fAccuracyPenalty = Lerp(expf(TICK_INTERVAL * -fDecayFactor), fNewPenalty, (float)m_fAccuracyPenalty);
	}
}


const float kJumpVelocity = sqrtf(2.0f * 800.0f * 57.0f);	// see CCSGameMovement::CheckJumpButton()

void CWeaponCSBase::OnJump( float fImpulse )
{
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyJump[m_weaponMode] * fImpulse / kJumpVelocity;
}

void CWeaponCSBase::OnLand( float fVelocity )
{
	float fPenalty = GetCSWpnData().m_fInaccuracyLand[m_weaponMode] * fVelocity / kJumpVelocity;
	m_fAccuracyPenalty += fPenalty;

/*
	// this bit of code is only if we want to punch the player view on all landings

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	QAngle angle = pPlayer->GetPunchAngle();
	float fVKick = RAD2DEG(asinf(fPenalty)) * 0.4f;
	float fHKick = SharedRandomFloat("LandPunchAngleYaw", -1.0f, +1.0f) * fVKick * 0.1f;

	angle.x += fVKick;	// pitch
	angle.y += fHKick;	// yaw
	pPlayer->SetPunchAngle( angle );
*/
}
