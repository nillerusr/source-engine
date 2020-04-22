//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODOVERVIEW_H
#define DODOVERVIEW_H

#include <mapoverview.h>
#include "dod_shareddefs.h"
#include "c_dod_basegrenade.h"

class CDODMapOverview : public CMapOverview
{
	DECLARE_CLASS_SIMPLE( CDODMapOverview, CMapOverview );

	CDODMapOverview( const char *pElementName );

	int		m_CameraIcons[MAX_TEAMS];
	int		m_CapturePoints[MAX_CONTROL_POINTS];

	void ShowLargeMap( void );
	void HideLargeMap( void );
	void ToggleZoom( void );

	void AddGrenade( C_DODBaseGrenade *pGrenade );
	void RemoveGrenade( C_DODBaseGrenade *pGrenade );

	void PlayerChat( int index );

	void DrawQuad( Vector pos, int scale, float angle, int textureID, int alpha );
	void DrawBombTimerSwipeIcon( Vector pos, int scale, int textureID, float flPercentRemaining );
	void DrawHorizontalSwipe( Vector pos, int scale, int textureID, float flCapPercentage, bool bSwipeLeft );
	bool DrawCapturePoint( int iCP, MapObject_t *obj );

	virtual void VidInit( void );

	virtual bool IsVisible( void );

protected:	
	virtual void SetMode(int mode);
	virtual void InitTeamColorsAndIcons();
	virtual void FireGameEvent( IGameEvent *event );
	virtual void DrawCamera();
	virtual void DrawMapPlayers();
	virtual void Update();
	virtual void UpdateSizeAndPosition();

	// rules that define if you can see a player on the overview or not
	virtual bool CanPlayerBeSeen(MapPlayer_t *player);

	void DrawVoiceIconForPlayer( int playerIndex );

	virtual bool DrawIcon( MapObject_t *obj );

protected:
	void UpdateCapturePoints();

private:
	int m_iLastMode;

	CUtlVector<MapObject_t>	m_Grenades;

	int m_iVoiceIcon;
	int m_iChatIcon;

	float m_flPlayerChatTime[MAX_PLAYERS];

	CHudTexture *m_pC4Icon;
	CHudTexture *m_pExplodedIcon;
	CHudTexture *m_pC4PlantedBG;
	CHudTexture *m_pIconDefended;

#define DOD_MAP_ZOOM_LEVELS	2
};

extern CDODMapOverview *GetDODOverview( void );

#endif // DODOVERVIEW_H
