//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( MAPDATA_H )
#define MAPDATA_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui/IImage.h>
#include "mathlib/vector.h"
#include "mapdata_shared.h"
#include "sharedinterface.h"

#define MAX_ZONES		32
#define MAX_CLASSES		32
#define MAX_MAP_TEAMS	8
#define MINIMAP_MATERIAL_STRING_SIZE 64
#define MINIMAP_STRING_SIZE 128

class Vector;
class C_BaseEntity;
class BitmapImage;

class CMapClassColors
{
public:
	BitmapImage		*m_pClassImage;
	Color		m_clrClass;
};

class CMapTeamColors
{
public:
	CMapClassColors	m_ClassColors[ MAX_CLASSES ];

	BitmapImage		*m_pImage;
	Color		m_clrBlip;
	Color		m_clrTeam;
};

class CMapPlayers
{
public:
	int				m_nTeam;
	bool			m_bVisible;
	bool			m_bSelected;
	int				m_nSquadNumber;
	int				m_nXPos, m_nYPos;

	BitmapImage		*m_pImage;
	Color		m_clrPlayer;
};

class CMapZones
{
public:
	int				m_nControllingTeam;

	BitmapImage		*m_pZoneImage;
};

struct MinimapData_t
{
	char	m_szBackgroundMaterial[MINIMAP_MATERIAL_STRING_SIZE];
};

class CMapData : public IMapData
{
public:
					CMapData( void );
	virtual			~CMapData( void );

	void			Init( void );
	void			Clear( void );
	void			LevelInit( const char *map );
	void			LevelShutdown( void );

	void			Update( );
	bool			IsEntityVisibleToTactical( C_BaseEntity* pEnt ) const;

	void			UseDefaults( void );
	void			SetTeamDefaultColor( int iTeamNumber, int r, int g, int b );

	void			UserCmd_ForceMapReload( void );

	// map dimensions
	void			GetMapBounds(Vector& mins, Vector& maxs);
	void			GetMapOrigin(Vector& org);
	void			GetMapSize(Vector& size);

	// 3d skybox
	void			Get3DSkyboxOrigin( Vector &vecOrigin );
	float			Get3DSkyboxScale( void );

	// Indicates the area currently visible in commander mode
	void			SetVisibleArea( const Vector& mins, const Vector& maxs );
	void			GetVisibleArea( Vector& mins, Vector& maxs );

public:
	CMapZones		m_Zones[ MAX_ZONES ];
	CMapPlayers		m_Players[ MAX_PLAYERS ];
	CMapTeamColors	m_TeamColors[ MAX_MAP_TEAMS+1 ];
	MinimapData_t	m_Minimap;

private:
	char	m_szMap[ MINIMAP_STRING_SIZE ];

	// World size + visible size in commander mode
	Vector	m_WorldMins;
	Vector	m_WorldMaxs;
	Vector	m_VisibleMins;
	Vector	m_VisibleMaxs;
};


//-----------------------------------------------------------------------------
// Gets at the singleton map data 
//-----------------------------------------------------------------------------
CMapData& MapData();


#endif // MAPDATA_H