//============= Copyright (c) StargateTC Source Team ==========================
//
// Broad Source-side weapon port for StargateTC's recovered GoldSrc weapon
// classnames. The decompile provides the original entity names and model assets
// (`weapon_*`, `models/v_*`, `models/p_*`, `models/w_*`) plus method families
// for melee weapons, grenades, projectiles, and visibility/support devices.
//
// This file keeps every recovered weapon as a real predicted HL2MP weapon on
// client and server. Weapon-specific script files provide damage, ammo, clips,
// HUD buckets, and model paths.
//
//=============================================================================
#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "in_buttons.h"
#include "npcevent.h"

#ifdef CLIENT_DLL
    #include "c_hl2mp_player.h"
#else
    #include "hl2mp_player.h"
    #include "grenade_frag.h"
#endif

#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
#define CSTCWeapon9mmAR C_STCWeapon9mmAR
#define CSTCWeaponBatonSupplice C_STCWeaponBatonSupplice
#define CSTCWeaponC4 C_STCWeaponC4
#define CSTCWeaponCoujaf C_STCWeaponCoujaf
#define CSTCWeaponCouteau C_STCWeaponCouteau
#define CSTCWeaponFlashbang C_STCWeaponFlashbang
#define CSTCWeaponGacGoa C_STCWeaponGacGoa
#define CSTCWeaponGlStaff C_STCWeaponGlStaff
#define CSTCWeaponGoaRegen C_STCWeaponGoaRegen
#define CSTCWeaponInvis C_STCWeaponInvis
#define CSTCWeaponLance C_STCWeaponLance
#define CSTCWeaponLarve C_STCWeaponLarve
#define CSTCWeaponM16 C_STCWeaponM16
#define CSTCWeaponM92S C_STCWeaponM92S
#define CSTCWeaponMain C_STCWeaponMain
#define CSTCWeaponMedic C_STCWeaponMedic
#define CSTCWeaponMine C_STCWeaponMine
#define CSTCWeaponMustardGrenade C_STCWeaponMustardGrenade
#define CSTCWeaponNarcoGrenade C_STCWeaponNarcoGrenade
#define CSTCWeaponNerveGrenade C_STCWeaponNerveGrenade
#define CSTCWeaponP90 C_STCWeaponP90
#define CSTCWeaponPSG1 C_STCWeaponPSG1
#define CSTCWeaponReetou C_STCWeaponReetou
#define CSTCWeaponSmokeGrenade C_STCWeaponSmokeGrenade
#define CSTCWeaponSPAS C_STCWeaponSPAS
#define CSTCWeaponTacGun C_STCWeaponTacGun
#define CSTCWeaponUSAS C_STCWeaponUSAS
#define CSTCWeaponZat C_STCWeaponZat
#define CSTCWeaponZatArc C_STCWeaponZatArc
#endif

enum STCWeaponKind
{
    STC_WEAPON_HITSCAN,
    STC_WEAPON_MELEE,
    STC_WEAPON_GRENADE,
    STC_WEAPON_SUPPORT,
};

struct STCWeaponConfig
{
    STCWeaponKind kind;
    float fireRate;
    float meleeRange;
    float meleeDamage;
    Vector spread;
};

static const STCWeaponConfig kSTCWeaponRifle      = { STC_WEAPON_HITSCAN, 0.10f, 0.0f,  0.0f, VECTOR_CONE_4DEGREES };
static const STCWeaponConfig kSTCWeaponPistol     = { STC_WEAPON_HITSCAN, 0.22f, 0.0f,  0.0f, VECTOR_CONE_3DEGREES };
static const STCWeaponConfig kSTCWeaponShotgun    = { STC_WEAPON_HITSCAN, 0.80f, 0.0f,  0.0f, VECTOR_CONE_10DEGREES };
static const STCWeaponConfig kSTCWeaponSniper     = { STC_WEAPON_HITSCAN, 1.20f, 0.0f,  0.0f, VECTOR_CONE_1DEGREES };
static const STCWeaponConfig kSTCWeaponEnergy     = { STC_WEAPON_HITSCAN, 0.75f, 0.0f,  0.0f, VECTOR_CONE_2DEGREES };
static const STCWeaponConfig kSTCWeaponMelee      = { STC_WEAPON_MELEE,   0.45f, 72.0f, 35.0f, vec3_origin };
static const STCWeaponConfig kSTCWeaponHeavyMelee = { STC_WEAPON_MELEE,   0.65f, 88.0f, 55.0f, vec3_origin };
static const STCWeaponConfig kSTCWeaponGrenade    = { STC_WEAPON_GRENADE, 1.00f, 0.0f,  0.0f, vec3_origin };
static const STCWeaponConfig kSTCWeaponSupport    = { STC_WEAPON_SUPPORT, 0.80f, 0.0f,  0.0f, vec3_origin };

class CSTCWeaponBase : public CBaseHL2MPCombatWeapon
{
public:
    DECLARE_CLASS( CSTCWeaponBase, CBaseHL2MPCombatWeapon );

    CSTCWeaponBase()
    {
        m_bFiresUnderwater = true;
    }

    virtual const STCWeaponConfig &GetSTCConfig() const = 0;

    void Precache() override
    {
        BaseClass::Precache();
#ifndef CLIENT_DLL
        if ( GetSTCConfig().kind == STC_WEAPON_GRENADE )
            UTIL_PrecacheOther( "npc_grenade_frag" );
#endif
    }

    void PrimaryAttack() override
    {
        const STCWeaponConfig &config = GetSTCConfig();

        if ( config.kind == STC_WEAPON_MELEE )
        {
            MeleeAttack();
            return;
        }

        if ( config.kind == STC_WEAPON_GRENADE )
        {
            ThrowGrenade();
            return;
        }

        if ( config.kind == STC_WEAPON_SUPPORT )
        {
            SupportUse();
            return;
        }

        BaseClass::PrimaryAttack();
    }

    float GetFireRate() override
    {
        return GetSTCConfig().fireRate;
    }

    const Vector &GetBulletSpread() override
    {
        return GetSTCConfig().spread;
    }

    Activity GetPrimaryAttackActivity() override
    {
        return ACT_VM_PRIMARYATTACK;
    }

private:
    void MeleeAttack()
    {
        CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
        if ( !pOwner )
            return;

        WeaponSound( MELEE_MISS );
        SendWeaponAnim( ACT_VM_HITCENTER );
        pOwner->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
        Vector vecForward;
        AngleVectors( pOwner->EyeAngles(), &vecForward );

        const Vector vecSrc = pOwner->Weapon_ShootPosition();
        const Vector vecEnd = vecSrc + vecForward * GetSTCConfig().meleeRange;

        trace_t tr;
        UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tr );
        if ( tr.fraction < 1.0f && tr.m_pEnt )
        {
            CTakeDamageInfo info( pOwner, pOwner, GetSTCConfig().meleeDamage, DMG_CLUB );
            tr.m_pEnt->TakeDamage( info );
            WeaponSound( MELEE_HIT );
        }
#endif

        m_flNextPrimaryAttack = gpGlobals->curtime + GetSTCConfig().fireRate;
        m_flNextSecondaryAttack = m_flNextPrimaryAttack;
    }

    void ThrowGrenade()
    {
        CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
        if ( !pOwner )
            return;

        if ( UsesClipsForAmmo1() && m_iClip1 <= 0 )
        {
            Reload();
            return;
        }

        if ( !UsesClipsForAmmo1() && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
            return;

        WeaponSound( SINGLE );
        SendWeaponAnim( ACT_VM_THROW );
        pOwner->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
        Vector vecForward;
        AngleVectors( pOwner->EyeAngles(), &vecForward );

        const Vector vecSrc = pOwner->Weapon_ShootPosition() + vecForward * 16.0f;
        const Vector vecThrow = vecForward * 900.0f + pOwner->GetAbsVelocity();
        Fraggrenade_Create(
            vecSrc,
            vec3_angle,
            vecThrow,
            AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ),
            pOwner,
            3.0f,
            false );
#endif

        if ( UsesClipsForAmmo1() )
            --m_iClip1;
        else
            pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );

        m_flNextPrimaryAttack = gpGlobals->curtime + GetSTCConfig().fireRate;
        m_flNextSecondaryAttack = m_flNextPrimaryAttack;
    }

    void SupportUse()
    {
        CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
        if ( !pOwner )
            return;

        WeaponSound( SINGLE );
        SendWeaponAnim( ACT_VM_PRIMARYATTACK );
        pOwner->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
        if ( !Q_stricmp( GetClassname(), "weapon_medic" ) || !Q_stricmp( GetClassname(), "weapon_goaregen" ) )
        {
            pOwner->TakeHealth( 20.0f, DMG_GENERIC );
        }
        else if ( !Q_stricmp( GetClassname(), "weapon_invis" ) || !Q_stricmp( GetClassname(), "weapon_reetou" ) )
        {
            pOwner->AddEffects( EF_NODRAW );
            pOwner->SetRenderMode( kRenderTransTexture );
            pOwner->SetRenderColorA( 80 );
        }
#endif

        m_flNextPrimaryAttack = gpGlobals->curtime + GetSTCConfig().fireRate;
        m_flNextSecondaryAttack = m_flNextPrimaryAttack;
    }
};

#define DECLARE_STC_WEAPON( className, networkName, entityName, configName ) \
    class className : public CSTCWeaponBase \
    { \
    public: \
        DECLARE_CLASS( className, CSTCWeaponBase ); \
        DECLARE_NETWORKCLASS(); \
        DECLARE_PREDICTABLE(); \
        const STCWeaponConfig &GetSTCConfig() const override { return configName; } \
    }; \
    IMPLEMENT_NETWORKCLASS_ALIASED( networkName, DT_##networkName ) \
    BEGIN_NETWORK_TABLE( className, DT_##networkName ) \
    END_NETWORK_TABLE() \
    BEGIN_PREDICTION_DATA( className ) \
    END_PREDICTION_DATA() \
    LINK_ENTITY_TO_CLASS( entityName, className ); \
    PRECACHE_WEAPON_REGISTER( entityName )

DECLARE_STC_WEAPON( CSTCWeapon9mmAR,          STCWeapon9mmAR,          weapon_9mmAR,          kSTCWeaponRifle );
DECLARE_STC_WEAPON( CSTCWeaponBatonSupplice,  STCWeaponBatonSupplice,  weapon_batsup,         kSTCWeaponHeavyMelee );
DECLARE_STC_WEAPON( CSTCWeaponC4,             STCWeaponC4,             weapon_c4,             kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponCoujaf,         STCWeaponCoujaf,         weapon_coujaf,         kSTCWeaponHeavyMelee );
DECLARE_STC_WEAPON( CSTCWeaponCouteau,        STCWeaponCouteau,        weapon_couteau,        kSTCWeaponMelee );
DECLARE_STC_WEAPON( CSTCWeaponFlashbang,      STCWeaponFlashbang,      weapon_flashbang,      kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponGacGoa,         STCWeaponGacGoa,         weapon_gacgoa,         kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponGlStaff,        STCWeaponGlStaff,        weapon_glstaff,        kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponGoaRegen,       STCWeaponGoaRegen,       weapon_goaregen,       kSTCWeaponSupport );
DECLARE_STC_WEAPON( CSTCWeaponInvis,          STCWeaponInvis,          weapon_invis,          kSTCWeaponSupport );
DECLARE_STC_WEAPON( CSTCWeaponLance,          STCWeaponLance,          weapon_lance,          kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponLarve,          STCWeaponLarve,          weapon_larve,          kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponM16,            STCWeaponM16,            weapon_m16,            kSTCWeaponRifle );
DECLARE_STC_WEAPON( CSTCWeaponM92S,           STCWeaponM92S,           weapon_m92s,           kSTCWeaponPistol );
DECLARE_STC_WEAPON( CSTCWeaponMain,           STCWeaponMain,           weapon_main,           kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponMedic,          STCWeaponMedic,          weapon_medic,          kSTCWeaponSupport );
DECLARE_STC_WEAPON( CSTCWeaponMine,           STCWeaponMine,           weapon_mine,           kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponMustardGrenade, STCWeaponMustardGrenade, weapon_mustardgrenade, kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponNarcoGrenade,   STCWeaponNarcoGrenade,   weapon_narcogrenade,   kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponNerveGrenade,   STCWeaponNerveGrenade,   weapon_nervegrenade,   kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponP90,            STCWeaponP90,            weapon_p90,            kSTCWeaponRifle );
DECLARE_STC_WEAPON( CSTCWeaponPSG1,           STCWeaponPSG1,           weapon_psg1,           kSTCWeaponSniper );
DECLARE_STC_WEAPON( CSTCWeaponReetou,         STCWeaponReetou,         weapon_reetou,         kSTCWeaponSupport );
DECLARE_STC_WEAPON( CSTCWeaponSmokeGrenade,   STCWeaponSmokeGrenade,   weapon_smokegrenade,   kSTCWeaponGrenade );
DECLARE_STC_WEAPON( CSTCWeaponSPAS,           STCWeaponSPAS,           weapon_spas,           kSTCWeaponShotgun );
DECLARE_STC_WEAPON( CSTCWeaponTacGun,         STCWeaponTacGun,         weapon_tacgun,         kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponUSAS,           STCWeaponUSAS,           weapon_usas,           kSTCWeaponShotgun );
DECLARE_STC_WEAPON( CSTCWeaponZat,            STCWeaponZat,            weapon_zat,            kSTCWeaponEnergy );
DECLARE_STC_WEAPON( CSTCWeaponZatArc,         STCWeaponZatArc,         weapon_zatarc,         kSTCWeaponEnergy );
