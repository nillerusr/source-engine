//============= Copyright (c) StargateTC Source Team ==========================
//
// MCP debug console commands. These are designed to be called via RCON from
// the tools/mcp_source_engine/ Python MCP server. They emit structured output
// (one JSON-ish line per record) that is easy to parse from the other side.
//
// Server-side only.
//
//=============================================================================
#include "cbase.h"
#include <typeinfo>
#include "baseentity.h"
#include "player.h"
#include "hl2mp/hl2mp_gamerules.h"
#include "team.h"
#include "in_buttons.h"

#include "tier0/memdbgon.h"

static int SG_CountPlayers()
{
    int n = 0;
    for ( int i = 1; i <= gpGlobals->maxClients; ++i )
    {
        if ( UTIL_PlayerByIndex( i ) ) ++n;
    }
    return n;
}

//-----------------------------------------------------------------------------
// sg_list_entities [class_filter]
// Emits one line per live entity:
//   ENT idx=N class=<classname> name=<targetname> origin=x,y,z
//-----------------------------------------------------------------------------
CON_COMMAND( sg_list_entities, "Dump server entity table (one line per entity)." )
{
    const char *pFilter = args.ArgC() > 1 ? args.Arg( 1 ) : NULL;

    int nMax = gpGlobals->maxEntities;
    int nCount = 0;
    for ( int i = 0; i < nMax; ++i )
    {
        CBaseEntity *pEnt = CBaseEntity::Instance( engine->PEntityOfEntIndex( i ) );
        if ( !pEnt ) continue;

        const char *pClass = pEnt->GetClassname();
        if ( pFilter && pFilter[0] && !Q_stristr( pClass, pFilter ) )
            continue;

        Vector v = pEnt->GetAbsOrigin();
        const char *pName = STRING( pEnt->GetEntityName() );
        Msg( "ENT idx=%d class=%s name=%s origin=%.1f,%.1f,%.1f\n",
             i, pClass, pName && pName[0] ? pName : "-", v.x, v.y, v.z );
        ++nCount;
    }
    Msg( "ENT_COUNT %d\n", nCount );
}

//-----------------------------------------------------------------------------
// sg_dump_entity <index>
// Verbose dump of one entity: class, name, health, origin, angles, model.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_dump_entity, "Dump details for one server entity by index." )
{
    if ( args.ArgC() < 2 )
    {
        Msg( "Usage: sg_dump_entity <index>\n" );
        return;
    }
    int idx = atoi( args.Arg( 1 ) );
    edict_t *pEdict = engine->PEntityOfEntIndex( idx );
    CBaseEntity *pEnt = pEdict ? CBaseEntity::Instance( pEdict ) : NULL;
    if ( !pEnt )
    {
        Msg( "DUMP_FAIL idx=%d (no entity)\n", idx );
        return;
    }

    Vector origin = pEnt->GetAbsOrigin();
    QAngle ang    = pEnt->GetAbsAngles();
    const char *pModel = STRING( pEnt->GetModelName() );
    const char *pName  = STRING( pEnt->GetEntityName() );

    Msg( "DUMP idx=%d class=%s name=%s health=%d/%d origin=%.1f,%.1f,%.1f angles=%.1f,%.1f,%.1f model=%s solid=%d move=%d\n",
         idx,
         pEnt->GetClassname(),
         pName && pName[0] ? pName : "-",
         pEnt->GetHealth(),
         pEnt->GetMaxHealth(),
         origin.x, origin.y, origin.z,
         ang.x, ang.y, ang.z,
         pModel && pModel[0] ? pModel : "-",
         (int)pEnt->GetSolid(),
         (int)pEnt->GetMoveType() );
}

//-----------------------------------------------------------------------------
// sg_player_info — dump first connected player's state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_player_info, "Dump player state (first connected)." )
{
    for ( int i = 1; i <= gpGlobals->maxClients; ++i )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
        if ( !pPlayer ) continue;

        Vector v = pPlayer->GetAbsOrigin();
        QAngle a = pPlayer->GetAbsAngles();
        int team = pPlayer->GetTeamNumber();
        const char *pTeamName = pPlayer->GetTeam() ? pPlayer->GetTeam()->GetName() : "none";
        CBaseCombatWeapon *pWep = pPlayer->GetActiveWeapon();

        Msg( "PLAYER idx=%d name=%s team=%d teamname=%s health=%d armor=%d origin=%.1f,%.1f,%.1f angles=%.1f,%.1f,%.1f weapon=%s\n",
             i,
             pPlayer->GetPlayerName(),
             team,
             pTeamName,
             pPlayer->GetHealth(),
             pPlayer->ArmorValue(),
             v.x, v.y, v.z,
             a.x, a.y, a.z,
             pWep ? pWep->GetClassname() : "-" );
        return;
    }
    Msg( "PLAYER_NONE\n" );
}

//-----------------------------------------------------------------------------
// sg_dump_gamerules — dump CHL2MPRules state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_dump_gamerules, "Dump Stargate gamerules state." )
{
    if ( !g_pGameRules )
    {
        Msg( "GAMERULES none\n" );
        return;
    }
    Msg( "GAMERULES class=%s description=%s teamplay=%d multiplayer=%d\n",
         typeid( *g_pGameRules ).name(),
         g_pGameRules->GetGameDescription(),
         (int)g_pGameRules->IsTeamplay(),
         (int)g_pGameRules->IsMultiplayer() );

    // Dump each registered team
    extern CUtlVector< CTeam * > g_Teams;
    for ( int i = 0; i < g_Teams.Count(); ++i )
    {
        CTeam *t = g_Teams[ i ];
        if ( !t ) continue;
        Msg( "TEAM idx=%d name=%s score=%d players=%d\n",
             i, t->GetName(), t->GetScore(), t->GetNumPlayers() );
    }
}

//-----------------------------------------------------------------------------
// sg_give_all_stargate — give all 29 Stargate weapons to the calling player.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_give_all_stargate, "Give all Stargate weapons to player (cheat).", FCVAR_CHEAT )
{
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer )
    {
        Msg( "GIVE_FAIL no player\n" );
        return;
    }

    static const char *kStargateWeapons[] = {
        "weapon_9mmAR", "weapon_batsup", "weapon_c4", "weapon_coujaf",
        "weapon_couteau", "weapon_flashbang", "weapon_gacgoa", "weapon_glstaff",
        "weapon_goaregen", "weapon_invis", "weapon_lance", "weapon_larve",
        "weapon_m16", "weapon_m92s", "weapon_main", "weapon_medic",
        "weapon_mine", "weapon_mustardgrenade", "weapon_narcogrenade",
        "weapon_nervegrenade", "weapon_p90", "weapon_psg1", "weapon_reetou",
        "weapon_smokegrenade", "weapon_spas", "weapon_tacgun", "weapon_usas",
        "weapon_zat", "weapon_zatarc"
    };
    int nGiven = 0;
    for ( int i = 0; i < ARRAYSIZE( kStargateWeapons ); ++i )
    {
        pPlayer->GiveNamedItem( kStargateWeapons[ i ] );
        ++nGiven;
    }
    Msg( "GIVE_ALL_STARGATE count=%d\n", nGiven );
}

//-----------------------------------------------------------------------------
// sg_assert — trigger a controlled DebugBreak so we can verify the debugger
// is attached.  No-op in release if no debugger.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_debugbreak, "Trigger DebugBreak() — for verifying attached debugger.", FCVAR_CHEAT )
{
    Msg( "About to DebugBreak — if a debugger is attached it will catch this.\n" );
    DebuggerBreak();
}

//-----------------------------------------------------------------------------
// sg_teleport <x> <y> <z> — teleport calling player to absolute coords.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_teleport, "Teleport calling player to <x> <y> <z>.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 4 )
    {
        Msg( "Usage: sg_teleport <x> <y> <z>\n" );
        return;
    }
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer )
    {
        Msg( "TELEPORT_FAIL no_player\n" );
        return;
    }
    Vector v( atof( args.Arg(1) ), atof( args.Arg(2) ), atof( args.Arg(3) ) );
    pPlayer->Teleport( &v, NULL, &vec3_origin );
    Msg( "TELEPORT ok x=%.1f y=%.1f z=%.1f\n", v.x, v.y, v.z );
}

//-----------------------------------------------------------------------------
// sg_give <weapon_class> — give a single weapon to the calling player.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_give, "Give one weapon by classname.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 )
    {
        Msg( "Usage: sg_give <weapon_class>\n" );
        return;
    }
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer )
    {
        Msg( "GIVE_FAIL no_player\n" );
        return;
    }
    const char *pName = args.Arg(1);
    pPlayer->GiveNamedItem( pName );
    Msg( "GIVE ok weapon=%s\n", pName );
}

//-----------------------------------------------------------------------------
// sg_set_health <hp> — set player health.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_set_health, "Set player health.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_set_health <hp>\n" ); return; }
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer ) { Msg( "HEALTH_FAIL no_player\n" ); return; }
    int hp = atoi( args.Arg(1) );
    pPlayer->SetHealth( hp );
    Msg( "HEALTH ok hp=%d\n", hp );
}

//-----------------------------------------------------------------------------
// sg_set_team <team_idx> — change calling player's team.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_set_team, "Set player team (2=Tau'ri, 3=Goa'uld, 4=Jaffa, 5=Unas).", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_set_team <team_idx>\n" ); return; }
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer ) { Msg( "TEAM_FAIL no_player\n" ); return; }
    int team = atoi( args.Arg(1) );
    pPlayer->ChangeTeam( team );
    Msg( "TEAM ok team=%d name=%s\n",
         team,
         pPlayer->GetTeam() ? pPlayer->GetTeam()->GetName() : "?" );
}

//-----------------------------------------------------------------------------
// sg_force_attack — fire primary attack with the active weapon.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_force_attack, "Make calling player fire primary attack.", FCVAR_CHEAT )
{
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer ) { Msg( "ATTACK_FAIL no_player\n" ); return; }
    CBaseCombatWeapon *pWep = pPlayer->GetActiveWeapon();
    if ( !pWep ) { Msg( "ATTACK_FAIL no_weapon\n" ); return; }
    pWep->PrimaryAttack();
    Msg( "ATTACK ok weapon=%s\n", pWep->GetClassname() );
}

CON_COMMAND_F( sg_force_attack2, "Make calling player fire secondary attack.", FCVAR_CHEAT )
{
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer ) { Msg( "ATTACK2_FAIL no_player\n" ); return; }
    CBaseCombatWeapon *pWep = pPlayer->GetActiveWeapon();
    if ( !pWep ) { Msg( "ATTACK2_FAIL no_weapon\n" ); return; }
    pWep->SecondaryAttack();
    Msg( "ATTACK2 ok weapon=%s\n", pWep->GetClassname() );
}

//-----------------------------------------------------------------------------
// sg_kill_self — force kill the calling player.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_kill_self, "Force kill the calling player.", FCVAR_CHEAT )
{
    CBasePlayer *pPlayer = UTIL_GetCommandClient();
    if ( !pPlayer ) { Msg( "KILL_FAIL no_player\n" ); return; }
    pPlayer->CommitSuicide();
    Msg( "KILL ok\n" );
}

//-----------------------------------------------------------------------------
// sg_spawn <classname> [x y z] — spawn an entity. Defaults to player position.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_spawn, "Spawn an entity at <x y z> (or player pos).", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_spawn <classname> [x y z]\n" ); return; }
    const char *pClass = args.Arg(1);
    Vector pos = vec3_origin;
    if ( args.ArgC() >= 5 )
    {
        pos.Init( atof( args.Arg(2) ), atof( args.Arg(3) ), atof( args.Arg(4) ) );
    }
    else
    {
        CBasePlayer *pPlayer = UTIL_GetCommandClient();
        if ( !pPlayer ) { Msg( "SPAWN_FAIL no_player_for_default_pos\n" ); return; }
        pos = pPlayer->GetAbsOrigin();
    }
    CBaseEntity *pEnt = CreateEntityByName( pClass );
    if ( !pEnt ) { Msg( "SPAWN_FAIL unknown_class=%s\n", pClass ); return; }
    pEnt->SetAbsOrigin( pos );
    DispatchSpawn( pEnt );
    pEnt->Activate();
    Msg( "SPAWN ok class=%s idx=%d origin=%.1f,%.1f,%.1f\n",
         pClass, pEnt->entindex(), pos.x, pos.y, pos.z );
}

//-----------------------------------------------------------------------------
// sg_net_stats — dump server-side networking + tickrate state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_net_stats, "Dump server networking stats." )
{
    Msg( "NET_STATS tickcount=%d tickinterval=%.4f curtime=%.2f frametime=%.4f maxClients=%d\n",
         gpGlobals->tickcount,
         gpGlobals->interval_per_tick,
         gpGlobals->curtime,
         gpGlobals->frametime,
         gpGlobals->maxClients );
    // Per-player edict info
    for ( int i = 1; i <= gpGlobals->maxClients; ++i )
    {
        CBasePlayer *p = UTIL_PlayerByIndex( i );
        if ( !p ) continue;
        edict_t *e = p->edict();
        Msg( "NET_PLAYER idx=%d name=%s edict_free=%d edict_dirty=%d\n",
             i, p->GetPlayerName(),
             ( e && e->IsFree() ) ? 1 : 0,
             ( e && e->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) ? 1 : 0 );
    }
}

//-----------------------------------------------------------------------------
// sg_ent_messages <ent_idx> — dump entity's I/O message queue / connections.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_ent_messages, "Dump an entity's outputs/inputs." )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_ent_messages <idx>\n" ); return; }
    int idx = atoi( args.Arg(1) );
    edict_t *e = engine->PEntityOfEntIndex( idx );
    CBaseEntity *pEnt = e ? CBaseEntity::Instance( e ) : NULL;
    if ( !pEnt ) { Msg( "ENT_MSG_FAIL idx=%d\n", idx ); return; }
    // Report classname + datadesc fields (basic introspection without
    // recursing into nested classes; useful as a starting point).
    Msg( "ENT_MSG idx=%d class=%s name=%s\n",
         idx,
         pEnt->GetClassname(),
         STRING( pEnt->GetEntityName() ) );
    datamap_t *map = pEnt->GetDataDescMap();
    while ( map )
    {
        Msg( "ENT_MSG_MAP class=%s field_count=%d\n",
             map->dataClassName ? map->dataClassName : "?",
             map->dataNumFields );
        map = map->baseMap;
    }
}

//-----------------------------------------------------------------------------
// sg_set_angles <pitch> <yaw> [roll] — set player view angles.
// Use snap_view (no smoothing — instantaneous like the user mouse-looked).
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_set_angles, "Set player view angles <pitch> <yaw> [roll].", FCVAR_CHEAT )
{
    if ( args.ArgC() < 3 ) { Msg( "Usage: sg_set_angles <pitch> <yaw> [roll]\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "ANGLES_FAIL no_player\n" ); return; }
    QAngle a( atof( args.Arg(1) ), atof( args.Arg(2) ),
              args.ArgC() >= 4 ? atof( args.Arg(3) ) : 0.0f );
    p->SnapEyeAngles( a );
    Msg( "ANGLES ok pitch=%.1f yaw=%.1f roll=%.1f\n", a.x, a.y, a.z );
}

//-----------------------------------------------------------------------------
// sg_step <forward> <right> [up] — relative-move the player.
// Uses the player's current forward/right vectors. Performs a collision-safe
// teleport (Source's Teleport sets origin and handles collision internally).
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_step, "Step player by <forward> <right> [up] units relative to view.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 3 ) { Msg( "Usage: sg_step <forward> <right> [up]\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "STEP_FAIL no_player\n" ); return; }
    float fwd  = atof( args.Arg(1) );
    float rgt  = atof( args.Arg(2) );
    float up   = args.ArgC() >= 4 ? atof( args.Arg(3) ) : 0.0f;
    Vector vForward, vRight, vUp;
    AngleVectors( p->EyeAngles(), &vForward, &vRight, &vUp );
    Vector newPos = p->GetAbsOrigin() + vForward * fwd + vRight * rgt + vUp * up;
    p->Teleport( &newPos, NULL, &vec3_origin );
    Msg( "STEP ok origin=%.1f,%.1f,%.1f\n", newPos.x, newPos.y, newPos.z );
}

//-----------------------------------------------------------------------------
// sg_aim_at <x> <y> <z> — orient player to look at a world position.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_aim_at, "Aim player at world position <x> <y> <z>.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 4 ) { Msg( "Usage: sg_aim_at <x> <y> <z>\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "AIM_FAIL no_player\n" ); return; }
    Vector target( atof( args.Arg(1) ), atof( args.Arg(2) ), atof( args.Arg(3) ) );
    Vector dir = target - p->EyePosition();
    QAngle ang;
    VectorAngles( dir, ang );
    p->SnapEyeAngles( ang );
    Msg( "AIM ok target=%.1f,%.1f,%.1f angles=%.1f,%.1f,%.1f\n",
         target.x, target.y, target.z, ang.x, ang.y, ang.z );
}

//-----------------------------------------------------------------------------
// sg_aim_at_entity <idx> — aim player at the origin of an entity.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_aim_at_entity, "Aim player at entity by index.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_aim_at_entity <idx>\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "AIM_FAIL no_player\n" ); return; }
    int idx = atoi( args.Arg(1) );
    edict_t *e = engine->PEntityOfEntIndex( idx );
    CBaseEntity *pTarget = e ? CBaseEntity::Instance( e ) : NULL;
    if ( !pTarget ) { Msg( "AIM_FAIL no_entity idx=%d\n", idx ); return; }
    Vector dir = pTarget->WorldSpaceCenter() - p->EyePosition();
    QAngle ang;
    VectorAngles( dir, ang );
    p->SnapEyeAngles( ang );
    Msg( "AIM ok target_idx=%d class=%s angles=%.1f,%.1f,%.1f\n",
         idx, pTarget->GetClassname(), ang.x, ang.y, ang.z );
}

//-----------------------------------------------------------------------------
// sg_use_weapon <classname> — switch active weapon.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_use_weapon, "Switch active weapon by classname.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_use_weapon <weapon_class>\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "USEWEAPON_FAIL no_player\n" ); return; }
    const char *pName = args.Arg(1);
    CBaseCombatWeapon *pWep = p->Weapon_OwnsThisType( pName );
    if ( !pWep )
    {
        // Try to give it first, then switch
        p->GiveNamedItem( pName );
        pWep = p->Weapon_OwnsThisType( pName );
    }
    if ( !pWep ) { Msg( "USEWEAPON_FAIL not_owned=%s\n", pName ); return; }
    p->Weapon_Switch( pWep );
    Msg( "USEWEAPON ok weapon=%s\n", pName );
}

//-----------------------------------------------------------------------------
// sg_press_button <button> [duration_seconds] — schedule a button hold.
// button: "jump", "duck", "use", "attack", "attack2", "reload", "walk".
// The button stays pressed for the duration, then releases automatically.
//-----------------------------------------------------------------------------
static const struct { const char *name; int mask; } kButtonMap[] = {
    { "attack",   IN_ATTACK   },
    { "attack2",  IN_ATTACK2  },
    { "jump",     IN_JUMP     },
    { "duck",     IN_DUCK     },
    { "use",      IN_USE      },
    { "reload",   IN_RELOAD   },
    { "walk",     IN_WALK     },
    { "speed",    IN_SPEED    },
    { "forward",  IN_FORWARD  },
    { "back",     IN_BACK     },
    { "left",     IN_LEFT     },
    { "right",    IN_RIGHT    },
    { "moveleft", IN_MOVELEFT },
    { "moveright",IN_MOVERIGHT},
};
static int SG_ButtonMaskByName( const char *pName )
{
    for ( int i = 0; i < ARRAYSIZE( kButtonMap ); ++i )
        if ( !Q_stricmp( kButtonMap[i].name, pName ) ) return kButtonMap[i].mask;
    return 0;
}

CON_COMMAND_F( sg_press_button, "Force-hold a player button for N seconds.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 )
    {
        Msg( "Usage: sg_press_button <attack|attack2|jump|duck|use|reload|forward|back|moveleft|moveright> [seconds]\n" );
        return;
    }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "BTN_FAIL no_player\n" ); return; }
    int mask = SG_ButtonMaskByName( args.Arg(1) );
    if ( !mask ) { Msg( "BTN_FAIL unknown=%s\n", args.Arg(1) ); return; }
    // Set the button bit immediately. The user-cmd processing will see it
    // on the next tick. We do NOT clear it — the caller can call again
    // with the opposite by passing a negative mask, or use sg_release_button.
    p->m_nButtons |= mask;
    Msg( "BTN ok button=%s mask=0x%x\n", args.Arg(1), mask );
}

CON_COMMAND_F( sg_release_button, "Release a previously-held button.", FCVAR_CHEAT )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_release_button <name>\n" ); return; }
    CBasePlayer *p = UTIL_GetCommandClient();
    if ( !p ) { Msg( "BTN_FAIL no_player\n" ); return; }
    int mask = SG_ButtonMaskByName( args.Arg(1) );
    if ( !mask ) { Msg( "BTN_FAIL unknown=%s\n", args.Arg(1) ); return; }
    p->m_nButtons &= ~mask;
    Msg( "BTN_RELEASE ok button=%s\n", args.Arg(1) );
}

//-----------------------------------------------------------------------------
// sg_ping — simple liveness probe.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_ping, "Echo PONG with server tick + map name." )
{
    Msg( "PONG tick=%d map=%s players=%d/%d gamerules=%s\n",
         gpGlobals->tickcount,
         STRING( gpGlobals->mapname ),
         SG_CountPlayers(),
         gpGlobals->maxClients,
         g_pGameRules ? g_pGameRules->GetGameDescription() : "none" );
}
