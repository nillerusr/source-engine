#include "cbase.h"
#include "asw_spawn_manager.h"
#include "asw_arena.h"
#include "asw_gamerules.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "ai_network.h"
#include "ai_networkmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_arena( "asw_arena", "0", FCVAR_CHEAT, "Will spawn waves of enemies (requires appropriate test map)" );
ConVar asw_arena_quantity_scale( "asw_arena_quantity_scale", "1.0", FCVAR_CHEAT, "Scales number of aliens spawned in arena mode" );
ConVar asw_arena_shuffle_walls( "asw_arena_shuffle_walls", "3", FCVAR_CHEAT, "Shuffle walls after how many waves?" );
ConVar asw_arena_wall_density_min( "asw_arena_wall_density_min", "0.0", FCVAR_CHEAT, "Max density of random wall brushes in arena mode" );
ConVar asw_arena_wall_density_max( "asw_arena_wall_density_max", "0.7", FCVAR_CHEAT, "Max density of random wall brushes in arena mode" );
ConVar asw_arena_waves_per_difficulty( "asw_arena_waves_per_difficulty", "3", FCVAR_CHEAT, "How many waves before difficulty ramps up" );

CASW_Arena g_ASWArena;
CASW_Arena* ASWArena() { return &g_ASWArena; }

CASW_Arena::CASW_Arena( void ) : CAutoGameSystemPerFrame( "CASW_Arena" )
{
	InitArenaAlienTypes();
}

CASW_Arena::~CASW_Arena()
{
	m_ArenaAliens.PurgeAndDeleteElements();
}

bool CASW_Arena::Init()
{
	m_ArenaRestTimer.Invalidate();
	m_ArenaCheckTimer.Invalidate();
	m_ArenaShuffleWallsTimer.Invalidate();
	m_bStartedArenaMode = false;
	m_iArenaWave = 0;

	return true;
}

void CASW_Arena::Shutdown()
{

}

void CASW_Arena::LevelInitPreEntity()
{
	asw_arena.SetValue( Q_strnicmp( STRING(gpGlobals->mapname), "aiarena", 7 ) ? "0" : "1" );
	Init();
}

void CASW_Arena::FrameUpdatePostEntityThink()
{
	// only think when we're in-game
	if ( !ASWGameRules() || ASWGameRules()->GetGameState() != ASW_GS_INGAME )
		return;

	if ( asw_arena.GetBool() )
	{
		UpdateArena();
	}
}

void CASW_Arena::InitArenaAlienTypes()
{
	m_ArenaAliens.PurgeAndDeleteElements();
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_drone",			5, 10 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_drone_jumper",	5, 10 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_shieldbug",		1, 3 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_mortarbug",		1, 5 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_parasite",		1, 5 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_harvester",		1, 6 ) );
	m_ArenaAliens.AddToTail( new CASW_ArenaAlien( "asw_buzzer",			3, 6 ) );
}

void CASW_Arena::UpdateArena()
{
	// has arena mode just been turned on?
	if ( !m_bStartedArenaMode )
	{
		for ( int i=0; i< m_ArenaAliens.Count(); i++ )
		{
			UTIL_PrecacheOther( m_ArenaAliens[i]->m_szAlienClass );
		}
		m_bStartedArenaMode = true;
		m_ArenaRestTimer.Start( 3.0f );
	}

	if ( m_ArenaShuffleWallsTimer.HasStarted() && m_ArenaShuffleWallsTimer.IsElapsed() )
	{
		TeleportPlayersToSpawn();
		ShuffleArenaWalls();
		m_ArenaShuffleWallsTimer.Invalidate();
		m_ArenaRestTimer.Start( 2.5f );
	}

	if ( m_ArenaRestTimer.HasStarted() && m_ArenaRestTimer.IsElapsed() )
	{
		SpawnArenaWave();
		m_ArenaRestTimer.Invalidate();
	}

	if ( m_ArenaCheckTimer.HasStarted() && m_ArenaCheckTimer.IsElapsed() )
	{
		// count live aliens
		CBaseEntity* pEntity = NULL;
		int iAliens = 0;
		while ((pEntity = gEntList.NextEnt( pEntity )) != NULL)
		{
			if ( pEntity && pEntity->IsNPC() && pEntity->Classify() != CLASS_ASW_MARINE && pEntity->GetHealth() > 0 )
				iAliens++;
		}

		if ( iAliens <= 0 && !m_ArenaRestTimer.HasStarted() && !m_ArenaShuffleWallsTimer.HasStarted() )
		{						
			RefillMarineAmmo();	

			if ( asw_arena_shuffle_walls.GetInt() > 0 && ( m_iArenaWave % asw_arena_shuffle_walls.GetInt() ) == 0 && ( m_iArenaWave != 0 ) )
			{
				UTIL_CenterPrintAll( UTIL_VarArgs("Wave %d clear!\nThe arena is changing...", m_iArenaWave ) );
				m_ArenaShuffleWallsTimer.Start( 5.0f );
			}
			else
			{
				UTIL_CenterPrintAll( UTIL_VarArgs("Wave %d clear!", m_iArenaWave ) );
				m_ArenaRestTimer.Start( RandomFloat( 8, 12 ) );
			}

			m_ArenaCheckTimer.Invalidate();
		}	
	}
}

void CASW_Arena::RefillMarineAmmo()
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) != NULL && pGameResource->GetMarineResource(i)->GetMarineEntity())
		{
			CASW_Marine *pMarine = pGameResource->GetMarineResource(i)->GetMarineEntity();
			for (int k=0;k<ASW_MAX_MARINE_WEAPONS;k++)
			{
				CASW_Weapon *pWeapon = pMarine->GetASWWeapon(k);
				if (!pWeapon)
					continue;

				// refill bullets in the gun
				pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
				pWeapon->m_iClip2 = pWeapon->GetMaxClip2();

				// give the marine a load of ammo of that type
				pMarine->GiveAmmo(10000, pWeapon->GetPrimaryAmmoType());
				pMarine->GiveAmmo(10000, pWeapon->GetSecondaryAmmoType());
			}
		}
	}
}

void CASW_Arena::SpawnArenaWave()
{
	if ( !ASWSpawnManager() )
		return;

	if ( ASWGameRules() )
	{
		ASWGameRules()->BroadcastSound( "Spawner.Horde" );
	}

	// find the 4 corridor spawn points
	CUtlVector<CBaseEntity*> arenaSpawns[4];
	int arenaSpawnsUsed[4];
	memset( arenaSpawnsUsed, 0, sizeof( arenaSpawnsUsed ) );

	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "info_target" )) != NULL)
	{
		if ( !stricmp( STRING( pEntity->GetEntityName() ), "Spawn_Front" ) )
		{
			arenaSpawns[0].AddToTail( pEntity );
		}
		else if ( !stricmp( STRING( pEntity->GetEntityName() ), "Spawn_Right" ) )
		{
			arenaSpawns[1].AddToTail( pEntity );
		}
		else if ( !stricmp( STRING( pEntity->GetEntityName() ), "Spawn_Below" ) )
		{
			arenaSpawns[2].AddToTail( pEntity );
		}
		else if ( !stricmp( STRING( pEntity->GetEntityName() ), "Spawn_Left" ) )
		{
			arenaSpawns[3].AddToTail( pEntity );
		}
	}
	Msg( "Found arena spawns: N:%d E:%d S:%d W:%d\n", arenaSpawns[0].Count(), arenaSpawns[1].Count(), arenaSpawns[2].Count(), arenaSpawns[3].Count() );

	// decide how many alien types we're going to spawn
	int iAlienTypes = 2;
	float fRandom = RandomFloat();
	if ( fRandom < 0.1f )
	{
		iAlienTypes = 4;
	}
	else if ( fRandom < 0.30f )
	{
		iAlienTypes = 3;
	}

	for ( int i=0 ; i<iAlienTypes; i++ )
	{
		// decide on a direction
		int iDirection = RandomInt( 0, 3 );
		// decide on an alien type
		int iAlienType = RandomInt( 0, m_ArenaAliens.Count() - 1 );
		int iQuantity = asw_arena_quantity_scale.GetFloat() * RandomInt( m_ArenaAliens[iAlienType]->m_iQuantityMin, m_ArenaAliens[iAlienType]->m_iQuantityMax );
		int iArenaLevel = m_iArenaWave / asw_arena_waves_per_difficulty.GetInt();
		iQuantity += iArenaLevel;

		for ( int k=0 ; k < iQuantity ; k++ )
		{
			if ( arenaSpawnsUsed[iDirection] < arenaSpawns[iDirection].Count() )
			{
				CBaseEntity *pSpawnPoint = arenaSpawns[iDirection][arenaSpawnsUsed[iDirection]];
				if ( !pSpawnPoint )
					continue;

				if ( ASWSpawnManager()->SpawnAlienAt( m_ArenaAliens[iAlienType]->m_szAlienClass, pSpawnPoint->GetAbsOrigin(), pSpawnPoint->GetAbsAngles() ) )
				{
					arenaSpawnsUsed[iDirection]++;
				}
			}
		}
	}

	m_iArenaWave++;
}

void CASW_Arena::TeleportPlayersToSpawn()
{
	if ( !ASWGameRules() )
		return;

	CBaseEntity *pSpot = NULL;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) != NULL && pGameResource->GetMarineResource(i)->GetMarineEntity())
		{
			CASW_Marine *pMarine = pGameResource->GetMarineResource(i)->GetMarineEntity();
			if ( pMarine->GetHealth() > 0 )
			{
				pSpot = ASWGameRules()->GetMarineSpawnPoint( pSpot );
				if ( pSpot )
				{
					pMarine->Teleport( &pSpot->GetAbsOrigin(), &pSpot->GetAbsAngles(), &vec3_origin );
				}
			}
		}
	}
}

void CASW_Arena::ShuffleArenaWalls()
{
	CUtlVector<CBaseEntity*> walls;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "func_brush" )) != NULL)
	{
		walls.AddToTail(pEntity);
	}

	// decide on level density
	float flDensity = RandomFloat( asw_arena_wall_density_min.GetFloat(), asw_arena_wall_density_max.GetFloat() );

	// turn on and off blocks
	variant_t emptyVariant;
	for (int i=0;i<walls.Count();i++)
	{
		if ( RandomFloat() < flDensity )
		{
			walls[i]->AcceptInput( "Enable", NULL, NULL, emptyVariant, 0 );
		}
		else
		{
			walls[i]->AcceptInput( "Disable", NULL, NULL, emptyVariant, 0 );
		}
		walls[i]->AddFlag( FL_WORLDBRUSH );
		walls[i]->SetMoveType( MOVETYPE_NONE );
	}

	g_pAINetworkManager->BuildNetworkGraph();
}

void CASW_Arena::Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info )
{
	if ( asw_arena.GetBool() )
	{
		m_ArenaCheckTimer.Start( 2.0f );
	}
}

void asw_arena_wave_f( const CCommand& args )
{
	ASWArena()->SpawnArenaWave();
}
static ConCommand asw_arena_wave("asw_arena_wave", asw_arena_wave_f, "Creates a wave of aliens on the arena test map", FCVAR_GAMEDLL | FCVAR_CHEAT);