//============= Copyright (c) StargateTC Source Team ==========================
//
// Broad registration pass for custom entities recovered from hl.dll.
//
// This file is intentionally conservative: it keeps the recovered classnames
// spawnable in Source maps without pretending the gameplay is complete. Entities
// with hand-ported behavior live in their own files and are not registered here.
//
// Source list: dll_re_workspace/output/hl/strings_categorized.json
//
//=============================================================================
#include "cbase.h"
#include "eventqueue.h"
#include "triggers.h"

class CSGRecoveredEntity : public CBaseAnimating
{
public:
    DECLARE_CLASS( CSGRecoveredEntity, CBaseAnimating );
    DECLARE_DATADESC();

    void Spawn() override;
    void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) override;

private:
    COutputEvent m_OnUse;
};

BEGIN_DATADESC( CSGRecoveredEntity )
    DEFINE_OUTPUT( m_OnUse, "OnUse" ),
END_DATADESC()

void CSGRecoveredEntity::Spawn()
{
    BaseClass::Spawn();
    SetMoveType( MOVETYPE_NONE );
    SetSolid( SOLID_NONE );
    SetUse( &CSGRecoveredEntity::Use );

    if ( GetModelName() != NULL_STRING )
    {
        PrecacheModel( STRING( GetModelName() ) );
        SetModel( STRING( GetModelName() ) );
    }
}

void CSGRecoveredEntity::Use( CBaseEntity *pActivator, CBaseEntity *, USE_TYPE, float )
{
    m_OnUse.FireOutput( pActivator, this );

    if ( m_target != NULL_STRING )
    {
        variant_t emptyVariant;
        g_EventQueue.AddEvent( STRING( m_target ), "Use", emptyVariant, 0.0f, pActivator, this );
    }
}

class CSGRecoveredBrushEntity : public CSGRecoveredEntity
{
public:
    DECLARE_CLASS( CSGRecoveredBrushEntity, CSGRecoveredEntity );

    void Spawn() override
    {
        BaseClass::Spawn();
        if ( GetModelName() != NULL_STRING )
            SetSolid( SOLID_BSP );
    }
};

class CSGRecoveredClipEntity : public CSGRecoveredBrushEntity
{
public:
    DECLARE_CLASS( CSGRecoveredClipEntity, CSGRecoveredBrushEntity );

    void Spawn() override
    {
        BaseClass::Spawn();
        AddEffects( EF_NODRAW );
        SetCollisionGroup( COLLISION_GROUP_PLAYER_MOVEMENT );
    }
};

class CSGRecoveredTriggerEntity : public CBaseTrigger
{
public:
    DECLARE_CLASS( CSGRecoveredTriggerEntity, CBaseTrigger );
    DECLARE_DATADESC();

    void Spawn() override;
    void TriggerTouch( CBaseEntity *pOther );

private:
    COutputEvent m_OnTrigger;
};

BEGIN_DATADESC( CSGRecoveredTriggerEntity )
    DEFINE_OUTPUT( m_OnTrigger, "OnTrigger" ),
    DEFINE_ENTITYFUNC( TriggerTouch ),
END_DATADESC()

void CSGRecoveredTriggerEntity::Spawn()
{
    BaseClass::Spawn();
    InitTrigger();
    SetTouch( &CSGRecoveredTriggerEntity::TriggerTouch );
}

void CSGRecoveredTriggerEntity::TriggerTouch( CBaseEntity *pOther )
{
    if ( !pOther || !pOther->IsPlayer() )
        return;

    m_OnTrigger.FireOutput( pOther, this );
    if ( m_target != NULL_STRING )
    {
        variant_t emptyVariant;
        g_EventQueue.AddEvent( STRING( m_target ), "Use", emptyVariant, 0.0f, pOther, this );
    }
}

class CSGTriggerRandom : public CSGRecoveredEntity
{
public:
    DECLARE_CLASS( CSGTriggerRandom, CSGRecoveredEntity );
    DECLARE_DATADESC();

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void Spawn() override;
    void RandomThink();

private:
    void ScheduleNextThink();

    float m_flMinDelay { 0.0f };
    float m_flMaxDelay { 5.0f };
};

BEGIN_DATADESC( CSGTriggerRandom )
    DEFINE_FIELD( m_flMinDelay, FIELD_FLOAT ),
    DEFINE_FIELD( m_flMaxDelay, FIELD_FLOAT ),
    DEFINE_THINKFUNC( RandomThink ),
END_DATADESC()

bool CSGTriggerRandom::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( !Q_stricmp( szKeyName, "min" ) || !Q_stricmp( szKeyName, "mindelay" ) )
    {
        m_flMinDelay = atof( szValue );
        return true;
    }

    if ( !Q_stricmp( szKeyName, "max" ) || !Q_stricmp( szKeyName, "maxdelay" ) || !Q_stricmp( szKeyName, "wait" ) )
    {
        m_flMaxDelay = atof( szValue );
        return true;
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CSGTriggerRandom::Spawn()
{
    BaseClass::Spawn();
    SetThink( &CSGTriggerRandom::RandomThink );
    ScheduleNextThink();
}

void CSGTriggerRandom::RandomThink()
{
    if ( m_target != NULL_STRING )
    {
        variant_t emptyVariant;
        g_EventQueue.AddEvent( STRING( m_target ), "Use", emptyVariant, 0.0f, this, this );
    }

    ScheduleNextThink();
}

void CSGTriggerRandom::ScheduleNextThink()
{
    const float flMin = MIN( m_flMinDelay, m_flMaxDelay );
    const float flMax = MAX( m_flMinDelay, m_flMaxDelay );
    SetNextThink( gpGlobals->curtime + random->RandomFloat( flMin, flMax ) );
}

class CSGTriggerGoal : public CSGRecoveredTriggerEntity
{
public:
    DECLARE_CLASS( CSGTriggerGoal, CSGRecoveredTriggerEntity );
    DECLARE_DATADESC();

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void Spawn() override;
    void GoalTouch( CBaseEntity *pOther );

private:
    int m_iGoalTeam { 0 };
};

BEGIN_DATADESC( CSGTriggerGoal )
    DEFINE_KEYFIELD( m_iGoalTeam, FIELD_INTEGER, "team" ),
    DEFINE_ENTITYFUNC( GoalTouch ),
END_DATADESC()

bool CSGTriggerGoal::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( !Q_stricmp( szKeyName, "team_no" ) || !Q_stricmp( szKeyName, "goalteam" ) )
    {
        m_iGoalTeam = atoi( szValue );
        return true;
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CSGTriggerGoal::Spawn()
{
    BaseClass::Spawn();
    SetTouch( &CSGTriggerGoal::GoalTouch );
}

void CSGTriggerGoal::GoalTouch( CBaseEntity *pOther )
{
    if ( !pOther || !pOther->IsPlayer() )
        return;

    if ( m_iGoalTeam != 0 && pOther->GetTeamNumber() != m_iGoalTeam )
        return;

    TriggerTouch( pOther );
}

class CSGPickupEntity : public CSGRecoveredEntity
{
public:
    DECLARE_CLASS( CSGPickupEntity, CSGRecoveredEntity );
    DECLARE_DATADESC();

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void Spawn() override;
    void PickupTouch( CBaseEntity *pOther );
    void RespawnThink();

private:
    COutputEvent m_OnPickup;
    float m_flRespawnTime { -1.0f };
    bool m_bActive { true };
};

BEGIN_DATADESC( CSGPickupEntity )
    DEFINE_FIELD( m_flRespawnTime, FIELD_FLOAT ),
    DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
    DEFINE_OUTPUT( m_OnPickup, "OnPickup" ),
    DEFINE_ENTITYFUNC( PickupTouch ),
    DEFINE_THINKFUNC( RespawnThink ),
END_DATADESC()

bool CSGPickupEntity::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( !Q_stricmp( szKeyName, "respawn" ) || !Q_stricmp( szKeyName, "wait" ) )
    {
        m_flRespawnTime = atof( szValue );
        return true;
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CSGPickupEntity::Spawn()
{
    BaseClass::Spawn();
    SetSolid( SOLID_BBOX );
    UTIL_SetSize( this, Vector( -16, -16, 0 ), Vector( 16, 16, 32 ) );
    SetTouch( &CSGPickupEntity::PickupTouch );
}

void CSGPickupEntity::PickupTouch( CBaseEntity *pOther )
{
    if ( !m_bActive || !pOther || !pOther->IsPlayer() )
        return;

    m_OnPickup.FireOutput( pOther, this );

    if ( m_target != NULL_STRING )
    {
        variant_t emptyVariant;
        g_EventQueue.AddEvent( STRING( m_target ), "Use", emptyVariant, 0.0f, pOther, this );
    }

    if ( m_flRespawnTime > 0.0f )
    {
        m_bActive = false;
        AddEffects( EF_NODRAW );
        SetSolid( SOLID_NONE );
        SetThink( &CSGPickupEntity::RespawnThink );
        SetNextThink( gpGlobals->curtime + m_flRespawnTime );
    }
    else
    {
        UTIL_Remove( this );
    }
}

void CSGPickupEntity::RespawnThink()
{
    m_bActive = true;
    RemoveEffects( EF_NODRAW );
    SetSolid( SOLID_BBOX );
    SetThink( NULL );
}

class CSGEnvToggleEntity : public CSGRecoveredEntity
{
public:
    DECLARE_CLASS( CSGEnvToggleEntity, CSGRecoveredEntity );
    DECLARE_DATADESC();

    void InputEnable( inputdata_t & )  { RemoveEffects( EF_NODRAW ); }
    void InputDisable( inputdata_t & ) { AddEffects( EF_NODRAW ); }
    void InputToggle( inputdata_t & )
    {
        if ( IsEffectActive( EF_NODRAW ) )
            RemoveEffects( EF_NODRAW );
        else
            AddEffects( EF_NODRAW );
    }
};

BEGIN_DATADESC( CSGEnvToggleEntity )
    DEFINE_INPUTFUNC( FIELD_VOID, "Enable",  InputEnable ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Toggle",  InputToggle ),
END_DATADESC()

class CSGTargetCDAudio : public CSGRecoveredEntity
{
public:
    DECLARE_CLASS( CSGTargetCDAudio, CSGRecoveredEntity );
    DECLARE_DATADESC();

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void InputPlay( inputdata_t & );
    void InputStop( inputdata_t & );

private:
    int m_iTrack { -1 };
    COutputInt m_OnTrackChanged;
};

BEGIN_DATADESC( CSGTargetCDAudio )
    DEFINE_FIELD( m_iTrack, FIELD_INTEGER ),
    DEFINE_OUTPUT( m_OnTrackChanged, "OnTrackChanged" ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Play", InputPlay ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Stop", InputStop ),
END_DATADESC()

bool CSGTargetCDAudio::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( !Q_stricmp( szKeyName, "track" ) )
    {
        m_iTrack = atoi( szValue );
        return true;
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CSGTargetCDAudio::InputPlay( inputdata_t &inputdata )
{
    m_OnTrackChanged.Set( m_iTrack, inputdata.pActivator ? inputdata.pActivator : this, this );
}

void CSGTargetCDAudio::InputStop( inputdata_t &inputdata )
{
    m_OnTrackChanged.Set( -1, inputdata.pActivator ? inputdata.pActivator : this, this );
}

class CSGMonsterProxy : public CBaseAnimating
{
public:
    DECLARE_CLASS( CSGMonsterProxy, CBaseAnimating );
    DECLARE_DATADESC();

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void Spawn() override;
    int OnTakeDamage( const CTakeDamageInfo &info ) override;
    void ProxyTouch( CBaseEntity *pOther );

private:
    int m_iStartHealth { 60 };
    int m_iTouchDamage { 0 };
    COutputEvent m_OnKilled;
};

BEGIN_DATADESC( CSGMonsterProxy )
    DEFINE_KEYFIELD( m_iStartHealth, FIELD_INTEGER, "health" ),
    DEFINE_KEYFIELD( m_iTouchDamage, FIELD_INTEGER, "touchdamage" ),
    DEFINE_OUTPUT( m_OnKilled, "OnKilled" ),
    DEFINE_ENTITYFUNC( ProxyTouch ),
END_DATADESC()

bool CSGMonsterProxy::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( !Q_stricmp( szKeyName, "dmg" ) )
    {
        m_iTouchDamage = atoi( szValue );
        return true;
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CSGMonsterProxy::Spawn()
{
    BaseClass::Spawn();
    SetMoveType( MOVETYPE_STEP );
    SetSolid( SOLID_BBOX );
    UTIL_SetSize( this, Vector( -16, -16, 0 ), Vector( 16, 16, 72 ) );
    m_takedamage = DAMAGE_YES;
    m_iHealth = m_iStartHealth;
    SetTouch( &CSGMonsterProxy::ProxyTouch );

    if ( GetModelName() != NULL_STRING )
    {
        PrecacheModel( STRING( GetModelName() ) );
        SetModel( STRING( GetModelName() ) );
    }
}

int CSGMonsterProxy::OnTakeDamage( const CTakeDamageInfo &info )
{
    const int result = BaseClass::OnTakeDamage( info );
    if ( m_iHealth <= 0 )
    {
        m_OnKilled.FireOutput( info.GetAttacker() ? info.GetAttacker() : this, this );
        UTIL_Remove( this );
    }
    return result;
}

void CSGMonsterProxy::ProxyTouch( CBaseEntity *pOther )
{
    if ( !pOther || !pOther->IsPlayer() || m_iTouchDamage <= 0 )
        return;

    CTakeDamageInfo dmgInfo( this, this, float( m_iTouchDamage ), DMG_SLASH );
    pOther->TakeDamage( dmgInfo );
}

class CSGTriggerSecret : public CSGRecoveredTriggerEntity
{
public:
    DECLARE_CLASS( CSGTriggerSecret, CSGRecoveredTriggerEntity );
    DECLARE_DATADESC();

    void SecretTouch( CBaseEntity *pOther );

private:
    static int s_iSecretsFound;
    COutputInt m_OnSecretFound;
    bool m_bTriggered { false };
};

int CSGTriggerSecret::s_iSecretsFound = 0;

BEGIN_DATADESC( CSGTriggerSecret )
    DEFINE_FIELD( m_bTriggered, FIELD_BOOLEAN ),
    DEFINE_OUTPUT( m_OnSecretFound, "OnSecretFound" ),
    DEFINE_ENTITYFUNC( SecretTouch ),
END_DATADESC()

void CSGTriggerSecret::SecretTouch( CBaseEntity *pOther )
{
    if ( m_bTriggered || !pOther || !pOther->IsPlayer() )
        return;

    m_bTriggered = true;
    ++s_iSecretsFound;
    m_OnSecretFound.Set( s_iSecretsFound, pOther, this );
    TriggerTouch( pOther );
    SetTouch( NULL );
}

// Ammo and items.
LINK_ENTITY_TO_CLASS( ammo_9mmAR, CSGPickupEntity );
LINK_ENTITY_TO_CLASS( ammo_psg1, CSGPickupEntity );
LINK_ENTITY_TO_CLASS( item_NVG, CSGPickupEntity );
LINK_ENTITY_TO_CLASS( item_airtank, CSGPickupEntity );
LINK_ENTITY_TO_CLASS( item_objective, CSGPickupEntity );

// Cyclers. (cycler/cycler_weapon/cycler_wreckage are registered in h_cycler.cpp — omitted to avoid double-registration heap corruption)
LINK_ENTITY_TO_CLASS( cycler_prdroid, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( cycler_sprite, CSGRecoveredEntity );

// Environment helpers.
LINK_ENTITY_TO_CLASS( env_debris, CSGEnvToggleEntity );
LINK_ENTITY_TO_CLASS( env_fog, CSGEnvToggleEntity );
LINK_ENTITY_TO_CLASS( env_fountain, CSGEnvToggleEntity );
LINK_ENTITY_TO_CLASS( env_lensflare, CSGEnvToggleEntity );
LINK_ENTITY_TO_CLASS( env_model, CSGEnvToggleEntity );
LINK_ENTITY_TO_CLASS( env_particule, CSGEnvToggleEntity );

// Brush and movement helpers.
// (func_guntarget/func_wall_toggle/func_traincontrols/func_trackchange/func_trackautochange
//  are all registered in SDK base — omitted to avoid double-registration heap corruption)
LINK_ENTITY_TO_CLASS( func_mortar_field, CSGRecoveredBrushEntity );
LINK_ENTITY_TO_CLASS( func_tankgoauld, CSGRecoveredBrushEntity );
LINK_ENTITY_TO_CLASS( func_monsterclip, CSGRecoveredClipEntity );

// Game event hooks.
LINK_ENTITY_TO_CLASS( game_nuke, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( game_playerdie, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( game_playerjoin, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( game_playerkill, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( game_playerleave, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( game_playerspawn, CSGRecoveredEntity );

// Info and marker entities. info_player_* team spawns have a more
// specific spawnpoint implementation in sg_player_spawn.cpp.
LINK_ENTITY_TO_CLASS( info_cap_rules, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( info_flash, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( info_marker, CSGRecoveredEntity );
// info_null is registered in SDK base (subs.cpp) — omitted to avoid double-registration
LINK_ENTITY_TO_CLASS( info_observer, CSGRecoveredEntity );
LINK_ENTITY_TO_CLASS( info_radar_block, CSGRecoveredEntity );
// target_cdaudio is registered in SDK base (controlentities.cpp) — omitted to avoid double-registration

// Monsters. AI/abilities remain to be ported, but these now have damage and touch behavior.
LINK_ENTITY_TO_CLASS( monster_barney_dead, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_bloater, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine2_hvyweapons, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine2_scientist, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine2_slave, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine3_barney, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine3_scientist, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine_barney, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine_panther, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cine_scientist, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_cockroach, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_flyer, CSGMonsterProxy );
// monster_furniture is registered in SDK base (genericmonster.cpp) — omitted to avoid double-registration
LINK_ENTITY_TO_CLASS( monster_grunt_repel, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_hgrunt_dead, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_implanted, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_larve, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_mine, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_mortar, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_nihilanth, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_replic, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_replicateur, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_replicator, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_satchel, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_scientist_dead, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_skull, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_tac, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_tentaclemaw, CSGMonsterProxy );
LINK_ENTITY_TO_CLASS( monster_vortigaunt, CSGMonsterProxy );

// Additional triggers. trigger_adv_teleport is hand-ported in gate_travel.cpp.
LINK_ENTITY_TO_CLASS( trigger_goal, CSGTriggerGoal );
LINK_ENTITY_TO_CLASS( trigger_monsterjump, CSGRecoveredTriggerEntity );
LINK_ENTITY_TO_CLASS( trigger_random, CSGTriggerRandom );
LINK_ENTITY_TO_CLASS( trigger_secret, CSGTriggerSecret );
// trigger_transition is registered in SDK base (triggers.cpp) — omitted to avoid double-registration

// Weapons are registered as real predicted client/server HL2MP weapons in
// sourcecode/shared/stargate/sg_weapons.cpp.
