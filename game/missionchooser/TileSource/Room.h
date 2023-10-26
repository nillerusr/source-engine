#ifndef TILEGEN_ROOM_H
#define TILEGEN_ROOM_H
#ifdef _WIN32
#pragma once
#endif

#include "Utlvector.h"
#include "ChunkFile.h"
#include "missionchooser/iasw_random_missions.h"

class CRoomTemplate;
class CMapLayout;
class CPlacedRoomTemplatePanel;

namespace vgui
{
	class Panel;
};

// an instance of a CRoomTemplate, i.e. an actual stamped down room on the map
class CRoom : public IASW_Room_Details
{
public:
	CRoom();
	CRoom( CMapLayout *pMapLayout, const CRoomTemplate* pRoomTemplate, int TileX, int TileY );
	virtual ~CRoom();
	
	// tile position we're in
	int m_iPosX;
	int m_iPosY;

	const CRoomTemplate *m_pRoomTemplate;

	// used by the layout generator
	int m_iNumChildren;

	// A number indicating the placement order index of this room in the map layout, or -1 if not applicable.
	int m_nPlacementIndex;

	// todo: lighting scale
	// todo: spawner scaling, etc., etc.

	// io
	static bool LoadRoomFromKeyValues( KeyValues *pRoomKeys, CMapLayout *pMapLayout );
	KeyValues *GetKeyValuesCopy();
	bool SaveRoomToFile(CChunkFile *pFile);

	// not saved
	void SetHasAlienEncounter( bool bEncounter ) { m_bHasAlienEncounter = bEncounter; }
	bool m_bHasAlienEncounter;

	// UI specific
	vgui::Panel* m_pPlacedRoomPanel;

	CMapLayout *m_pMapLayout;

	// =================================
	// IASW_Room_Details interface
	// =================================
	
	// tags
	virtual bool			HasTag( const char *szTag );
	virtual int				GetNumTags();
	virtual const char*		GetTag( int i );
	virtual int				GetSpawnWeight();
	virtual int				GetNumExits();
	virtual IASW_Room_Details *GetAdjacentRoom( int nExit );
	virtual bool			GetThumbnailName( char* szOut, int iBufferSize );
	virtual bool			GetFullRoomName( char* szOut, int iBufferSize );
	virtual void			GetSoundscape( char* szOut, int iBufferSize );
	virtual void			GetTheme( char* szOut, int iBufferSize );
	virtual const Vector&	GetAmbientLight();
	virtual bool			HasAlienEncounter() { return m_bHasAlienEncounter; }
	virtual int				GetTileType();
	virtual const char*		GetTileTypeName( int nType );
	virtual int				GetRoomIndex() const { return m_iNumChildren; }

	// location
	virtual void			GetWorldBounds( Vector *vecWorldMins, Vector *vecWorldMaxs );
	virtual const Vector&	WorldSpaceCenter();
};

#endif TILEGEN_ROOM_H