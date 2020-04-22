//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "mapdata.h"
#include "hud.h"
#include "hud_macros.h"
#include <KeyValues.h>
#include "playeroverlay.h"
#include "iclientmode.h"
#include "hud_technologytreedoc.h"
#include "C_World.h"
#include "c_basetfplayer.h"
#include "c_team.h"
#include "c_tfteam.h"
#include "c_func_resource.h"
#include "vgui_bitmapimage.h"
#include "C_Shield.h"
#include "c_obj_respawn_station.h"

bool IsEntityVisibleToTactical( int iLocalTeamNumber, int iLocalTeamPlayers, 
	int iLocalTeamObjects, int iEntIndex, const char *pEntName, int pEntTeamNumber, const Vector &pEntOrigin );

// All of the parsing occurs mapdata_parse.cpp
bool ParseMinimapData( const char *filename, MinimapData_t *pMinimap, CMapZones *pZones, CMapTeamColors *pTeamColors, KeyValues *pKV );


//-----------------------------------------------------------------------------
// Gets at the singleton map data 
//-----------------------------------------------------------------------------

static CMapData g_MapData;
CMapData& MapData()
{
	// Singleton object
	return g_MapData;
}

IMapData *g_pMapData = &g_MapData;

DECLARE_COMMAND( g_MapData, ForceMapReload );

//-----------------------------------------------------------------------------
// Purpose: This is a total hack, but should allow reloading the mapfile.txt stuff on the fly
//-----------------------------------------------------------------------------
void CMapData::UserCmd_ForceMapReload( void )
{
	if ( m_szMap[0] )
	{
		LevelInit( m_szMap );
		// Force other data to reinit itself
		GetTechnologyTreeDoc().Init();
		
		// Force any needed viewport fixups
		g_pClientMode->Disable();
		g_pClientMode->Enable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapData::CMapData( void )
{
	m_szMap[0]=0;

	int i, j;
	m_Minimap.m_szBackgroundMaterial[0] = 0;

	for ( i = 0; i < MAX_ZONES; i++ )
	{
		m_Zones[ i ].m_pZoneImage = NULL;
		m_Zones[ i ].m_nControllingTeam = -1;
	}

	for ( i = 0; i < MAX_PLAYERS; i++ )
	{
		m_Players[ i ].m_pImage			= NULL;
		m_Players[ i ].m_nTeam			= 0;
		m_Players[ i ].m_bSelected		= false;
		m_Players[ i ].m_nSquadNumber	= 0;

	}

	for ( i = 0; i <= MAX_MAP_TEAMS; i++ )
	{
		m_TeamColors[ i ].m_pImage = NULL;

		for ( j = 0; j < MAX_CLASSES; j++ )
		{
			if ( m_TeamColors[ i ].m_ClassColors[ j ].m_pClassImage )
			{
				m_TeamColors[ i ].m_ClassColors[ j ].m_pClassImage = NULL;
			}
		}
	}

	UseDefaults();
}

HOOK_COMMAND( forcemapreload, ForceMapReload );

//-----------------------------------------------------------------------------
// Purpose: One time init
//-----------------------------------------------------------------------------
void CMapData::Init( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Delete all dynamic images, but leave rest of data structures
//-----------------------------------------------------------------------------
void CMapData::Clear( void )
{
	int i, j;

	// Delete old zones...
	for ( i = 0; i < MAX_ZONES; i++ )
	{
		delete m_Zones[ i ].m_pZoneImage;
		m_Zones[ i ].m_pZoneImage = NULL;
	}

	for ( i = 0; i <= MAX_MAP_TEAMS; i++ )
	{
		delete m_TeamColors[ i ].m_pImage;
		m_TeamColors[ i ].m_pImage = NULL;

		for ( j = 0; j < MAX_CLASSES; j++ )
		{
			delete m_TeamColors[ i ].m_ClassColors[ j ].m_pClassImage;
			m_TeamColors[ i ].m_ClassColors[ j ].m_pClassImage = NULL;
		}
	}

	m_Minimap.m_szBackgroundMaterial[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapData::~CMapData( void )
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Set a team's default colors
//-----------------------------------------------------------------------------
void CMapData::SetTeamDefaultColor( int iTeamNumber, int r, int g, int b )
{
	m_TeamColors[ iTeamNumber ].m_clrBlip.SetColor( r,g,b, 0 );
	m_TeamColors[ iTeamNumber ].m_clrTeam.SetColor( r,g,b, 128 );

	for ( int j = 0; j < MAX_CLASSES; j++ )
	{
		m_TeamColors[ iTeamNumber ].m_ClassColors->m_clrClass.SetColor( r,g,b, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fill in placeholder colors, etc.
//-----------------------------------------------------------------------------
void CMapData::UseDefaults( void )
{
	// Init colors for all teams
	SetTeamDefaultColor( 1, 255, 0, 0 );
	SetTeamDefaultColor( 2, 0, 0, 255 );
	SetTeamDefaultColor( 3, 0, 0, 0 );
	SetTeamDefaultColor( 4, 0, 0, 0 );
	SetTeamDefaultColor( 5, 0, 0, 0 );
	SetTeamDefaultColor( 6, 0, 0, 0 );
	SetTeamDefaultColor( 7, 0, 0, 0 );
	SetTeamDefaultColor( 8, 0, 0, 0 );

	m_Minimap.m_szBackgroundMaterial[0] = 0;
}


//-----------------------------------------------------------------------------
// Get the bounding box of the world
//-----------------------------------------------------------------------------
void CMapData::GetMapBounds(Vector &mins, Vector& maxs)
{
	C_BaseEntity *ent = cl_entitylist->GetEnt( 0 );
	C_World* pWorld = dynamic_cast<C_World*>(ent);
	if (pWorld)
	{
		VectorCopy( pWorld->m_WorldMins, mins );
		VectorCopy( pWorld->m_WorldMaxs, maxs );

		// Backward compatability...
		if ((mins.LengthSqr() == 0.0f) && (maxs.LengthSqr() == 0.0f))
		{
			mins.Init( -6500, -6500, -6500 );
			maxs.Init( 6500, 6500, 6500 );
		}
	}
	else
	{
		Assert(0);
		mins.Init( 0, 0, 0 );
		maxs.Init( 1, 1, 1 );
	}
}

void CMapData::GetMapOrigin(Vector &org)
{
	Vector mins, maxs;
	GetMapBounds( mins, maxs );
	VectorAdd( mins, maxs, org );
	VectorMultiply( org, 0.5, org );
}

void CMapData::GetMapSize(Vector &size) 
{
	Vector mins, maxs;
	GetMapBounds( mins, maxs );
	VectorSubtract( maxs, mins, size );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapData::Get3DSkyboxOrigin( Vector &vecOrigin )
{
	// NOTE: If the player hasn't been created yet -- this doesn't work!!!
	//       We need to pass the data along in the map - requires a tool change.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		CPlayerLocalData *pLocalData = &pPlayer->m_Local;
		VectorCopy( pLocalData->m_skybox3d.origin, vecOrigin );
	}
	else
	{
		// Debugging!
		Assert( 0 );
		vecOrigin.Init();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CMapData::Get3DSkyboxScale( void )
{
	// NOTE: If the player hasn't been created yet -- this doesn't work!!!
	//       We need to pass the data along in the map - requires a tool change.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		CPlayerLocalData *pLocalData = &pPlayer->m_Local;
		return pLocalData->m_skybox3d.scale;
	}
	else
	{
		// Debugging!
		Assert( 0 );
		return ( 1.0f );
	}
}

//-----------------------------------------------------------------------------
// What's my visible area?
//-----------------------------------------------------------------------------
void CMapData::SetVisibleArea( const Vector& mins, const Vector& maxs )
{
	m_VisibleMins = mins;
	m_VisibleMaxs = maxs;
}

void CMapData::GetVisibleArea( Vector& mins, Vector& maxs )
{
	mins = m_VisibleMins;
	maxs = m_VisibleMaxs;
}


//-----------------------------------------------------------------------------
// Purpose: The client is about to change maps
// Input  : *map - name of the new map
//-----------------------------------------------------------------------------
void CMapData::LevelInit( const char *map )
{
	Q_strncpy( m_szMap, map, MINIMAP_STRING_SIZE );

	// Clear leftover data
	Clear();
	
	// The name of the background material for the map is well-defined
	Q_snprintf( m_Minimap.m_szBackgroundMaterial, MINIMAP_MATERIAL_STRING_SIZE, 
		"hud/minimap/%s/%s", map, map );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapData::LevelShutdown( void )
{
	Clear();
}


//-----------------------------------------------------------------------------
// Update fog of war
//-----------------------------------------------------------------------------
void CMapData::Update( void ) 
{
}


//-----------------------------------------------------------------------------
// Is entity visible to tactical?
//-----------------------------------------------------------------------------
bool CMapData::IsEntityVisibleToTactical( C_BaseEntity* pEnt ) const
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if (!pPlayer)
		return false;

	// If the local player hasn't chosen a class or a team, nothing's visible
	if ((pPlayer->GetClass() == TFCLASS_UNDECIDED) || (pPlayer->GetTeamNumber() == 0))
		return false;

	C_TFTeam *pTeam = (C_TFTeam *)pPlayer->GetTeam();
	if (!pTeam)
		return false;

	// Local player is always visible, as long as he's chosen a class.
	if (pEnt == pPlayer)
		return true;

	int iNumberObjects = 0;
	int iNumberPlayers = 0;
	if ( pTeam )
	{
		iNumberObjects = pTeam->GetNumObjects();
		iNumberPlayers = pTeam->Get_Number_Players();
	}

	int localteam = pPlayer->GetTeamNumber();

	// If on our team, not on a team, or is a resource zone, can't be cloaked
	if ( !pEnt->GetTeamNumber() || pEnt->GetTeamNumber() == localteam )
	{
		return true;
	}

	// NOTE: The global IsEntityVisibleToTactical returns true in situations
	// where the entity would otherwise not be visible
	bool bRet = ::IsEntityVisibleToTactical( localteam, iNumberPlayers, 
		iNumberObjects, pEnt->entindex(), pEnt->GetClassname(), pEnt->GetTeamNumber(), pEnt->GetAbsOrigin() );

	if ( bRet )
	{
		return true;
	}

	// Make sure it's within a well-defined radius of the local player...
	Vector2D dist;
	Vector2DSubtract( pEnt->GetAbsOrigin().AsVector2D(), pPlayer->GetAbsOrigin().AsVector2D(), dist );

	// Cloaked by this object?
	if ( dist.LengthSqr() > LOCAL_PLAYER_SCANNER_RANGE * LOCAL_PLAYER_SCANNER_RANGE )
		return false;

	// On other team, and not cloaked by technician
	return true;
}
