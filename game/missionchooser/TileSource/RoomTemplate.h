#ifndef TILEGEN_ROOM_TEMPLATE_H
#define TILEGEN_ROOM_TEMPLATE_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_random_missions.h"

class CLevelTheme;
class KeyValues;

enum ExitDirection_t 
{
	EXITDIR_BEGIN = 0,
	EXITDIR_NORTH = 0,
	EXITDIR_EAST = 1,
	EXITDIR_SOUTH = 2,
	EXITDIR_WEST = 3,
	EXITDIR_END = 4,
};

ExitDirection_t GetDirectionFromString( const char *pDirectionString );

#define MAX_EXIT_TAG_LENGTH 64

static const int MIN_SPAWN_WEIGHT = 0;
static const int MAX_SPAWN_WEIGHT = 5;

// describes the location and facing of an exit within a room
class CRoomTemplateExit
{
public:
	CRoomTemplateExit()
	{
		m_iXPos = 0;
		m_iYPos = 0;
		m_ExitDirection = EXITDIR_NORTH;
		m_iZChange = 0;
		m_szExitTag[0] = 0;
		m_bChokepointGrowSource = false;
	}

	static void GetExitOffset( ExitDirection_t Direction, int *pX, int *pY );
	static ExitDirection_t GetOppositeDirection( ExitDirection_t Direction );

	int m_iXPos;						// exit's position within its room template
	int m_iYPos;						// note: y-axis is reversed compared to vgui to match hammer's co-ord style
	ExitDirection_t m_ExitDirection;
	int m_iZChange;						// z-change (in number of our fixed z-steps) that this exit is, compared to the room's base z pos (currently unused)
	char m_szExitTag[MAX_EXIT_TAG_LENGTH];
	bool m_bChokepointGrowSource;
};

// describes a particular room template.  Mapper places instances of these over the level to create the desired map layout.
class CRoomTemplate
{
public:
	CRoomTemplate(CLevelTheme* pLevelTheme);
	~CRoomTemplate();

	void Clear();

	void LoadFromKeyValues( const char *pRoomName, KeyValues *pKeyValues );
	bool SaveRoomTemplate();

	const char *GetFullName() const { return m_FullName; }
	const char *GetFolderName() const { return m_SubFolder; }
	const char *GetDescription() const { return m_Description; }
	const char *GetSoundscape() const { return m_Soundscape; }
	
	void SetFullName( const char *pFullName );
	void SetDescription( const char *pDescription );
	void SetSoundscape( const char *pSoundscape );

	// Room type queries
	bool IsStartRoom() const;
	bool IsEscapeRoom() const;
	bool IsBorderRoom() const;
	bool ShouldOnlyPlaceByRequest() const;

	int GetArea() const { return m_nTilesX * m_nTilesY; }
	int GetTilesX() const { return m_nTilesX; }
	int GetTilesY() const { return m_nTilesY; }
	void SetTileSize( int x, int y ) { m_nTilesX = x; m_nTilesY = y; }

	CUtlVector<CRoomTemplateExit*> m_Exits;	// list of exits	

	CLevelTheme* m_pLevelTheme;		// pointer to the loaded in theme	

	// Tag queries
	bool HasTag( const char *pTag ) const;
	int GetNumTags() const { return m_Tags.Count(); }
	const char* GetTag( int i ) const { return m_Tags[i]; }
	bool AddTag( const char *pTag );
	bool RemoveTag( const char *pTag );

	int GetSpawnWeight() const { return m_nSpawnWeight; }
	void SetSpawnWeight( int nSpawnWeight ) { m_nSpawnWeight = MAX( MIN( nSpawnWeight, MAX_SPAWN_WEIGHT ), MIN_SPAWN_WEIGHT ); }

	// Tile types.
	int GetTileType() const			{ return m_nTileType; }
	void SetTileType( int nType )	{ Assert( nType >= ASW_TILETYPE_UNKNOWN && nType < ASW_TILETYPE_COUNT ); m_nTileType = nType; }

	static const int m_nMaxDescriptionLength = 512;
	static const int m_nMaxSoundscapeLength = 64;

private:
	
	CUtlVector< const char* > m_Tags;	// List of tags
	int m_nSpawnWeight;					// The weighting that should be given to this room with respect to spawning (higher means more aliens, 0 means none)

	char m_TemplateName[MAX_PATH];		// Filename of this room, without extension (e.g. '8x8_cap_innerSE')
	char m_SubFolder[MAX_PATH];			// Folder path, relative to the theme directory. Given a room template located 
										// at /tilegen/roomtemplates/theme_name/folder1/folder2/foo.vmf, this will be 'folder1/folder2/'
	char m_FullName[MAX_PATH];			// The concatenation of m_szSubFolder + m_szTemplateName (also without an extension)

	char m_Description[m_nMaxDescriptionLength];
	char m_Soundscape[m_nMaxSoundscapeLength];

	int m_nTilesX;
	int m_nTilesY;

	int	m_nTileType;
};

#endif TILEGEN_ROOM_TEMPLATE_H