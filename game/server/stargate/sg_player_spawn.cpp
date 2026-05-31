//============= Copyright © StargateTC Source Team =============================
//
// info_player_torri / info_player_goauld — team-flavored multiplayer spawn
// markers. The entity translator (pipeline/stargate_pipeline/entities/translator.py)
// rewrites these to info_player_deathmatch + a "team" keyvalue at port time,
// so HL2DM's spawn picker handles them automatically. We still register the
// raw classnames here in case a hand-authored Source-side map uses them.
//
//==============================================================================
#include "cbase.h"
#include "stargate/sg_constants.h"

class CSGSpawnPoint : public CPointEntity
{
public:
    DECLARE_CLASS( CSGSpawnPoint, CPointEntity );
    void Spawn() override
    {
        BaseClass::Spawn();
        SetSolid( SOLID_NONE );
    }
    virtual int PreferredTeam() = 0;
};

class CSGSpawnPointTorri : public CSGSpawnPoint
{
public:
    DECLARE_CLASS( CSGSpawnPointTorri, CSGSpawnPoint );
    int PreferredTeam() override { return StargateTC::TEAM_TORRI; }
};
LINK_ENTITY_TO_CLASS( info_player_torri, CSGSpawnPointTorri );

class CSGSpawnPointGoauld : public CSGSpawnPoint
{
public:
    DECLARE_CLASS( CSGSpawnPointGoauld, CSGSpawnPoint );
    int PreferredTeam() override { return StargateTC::TEAM_GOAULD; }
};
LINK_ENTITY_TO_CLASS( info_player_goauld, CSGSpawnPointGoauld );

class CSGSpawnPointMarine : public CSGSpawnPoint
{
public:
    DECLARE_CLASS( CSGSpawnPointMarine, CSGSpawnPoint );
    int PreferredTeam() override { return StargateTC::TEAM_TORRI; }
};
LINK_ENTITY_TO_CLASS( info_player_marine, CSGSpawnPointMarine );

class CSGSpawnPointUnas : public CSGSpawnPoint
{
public:
    DECLARE_CLASS( CSGSpawnPointUnas, CSGSpawnPoint );
    int PreferredTeam() override { return StargateTC::TEAM_UNAS; }
};
LINK_ENTITY_TO_CLASS( info_player_unas, CSGSpawnPointUnas );

class CSGSpawnPointCoop : public CSGSpawnPoint
{
public:
    DECLARE_CLASS( CSGSpawnPointCoop, CSGSpawnPoint );
    int PreferredTeam() override { return StargateTC::TEAM_NONE; }
};
LINK_ENTITY_TO_CLASS( info_player_coop, CSGSpawnPointCoop );
