//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSSPECTATORGUI_H
#define CSSPECTATORGUI_H
#ifdef _WIN32
#pragma once
#endif

#include "spectatorgui.h"
#include "mapoverview.h"
#include "cs_shareddefs.h"

extern ConVar mp_playerid; // in cs_gamerules.h
extern ConVar mp_forcecamera; // in gamevars_shared.h
extern ConVar mp_fadetoblack;


//-----------------------------------------------------------------------------
// Purpose: Cstrike Spectator UI
//-----------------------------------------------------------------------------
class CCSSpectatorGUI : public CSpectatorGUI
{
private:
	DECLARE_CLASS_SIMPLE( CCSSpectatorGUI, CSpectatorGUI );

public:
	CCSSpectatorGUI( IViewPort *pViewPort );
		
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void UpdateSpectatorPlayerList( void );
	virtual void Update( void );
	virtual bool NeedsUpdate( void );
	//=============================================================================
	// HPE_BEGIN:
	// [smessick]
	//=============================================================================
	virtual void ShowPanel( bool bShow );
	//=============================================================================
	// HPE_END
	//=============================================================================

protected:

	void UpdateTimer();
	void UpdateAccount();

	int		m_nLastAccount;
	int		m_nLastTime;
	int		m_nLastSpecMode;
	CBaseEntity	*m_nLastSpecTarget;

	void StoreWidths( void );
	void ResizeControls( void );
	bool ControlsPresent( void ) const;

	vgui::Label *m_pCTLabel;
	vgui::Label *m_pCTScore;
	vgui::Label *m_pTerLabel;
	vgui::Label *m_pTerScore;
	vgui::Label *m_pTimer;
	vgui::Label *m_pTimerLabel;
	vgui::Panel *m_pDivider;
	vgui::Label *m_pExtraInfo;

	bool m_modifiedWidths;

	int m_scoreWidth;
	int m_extraInfoWidth;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define DESIRED_RADAR_RESOLUTION 450 

class CCSMapOverview : public CMapOverview
{
	DECLARE_CLASS_SIMPLE( CCSMapOverview, CMapOverview );

public:	

	enum
	{
		MAP_ICON_T = 0,
		MAP_ICON_CT,
		MAP_ICON_HOSTAGE,
		MAP_ICON_COUNT
	};

	CCSMapOverview( const char *pElementName );
	virtual ~CCSMapOverview();

	virtual bool ShouldDraw( void );
	vgui::Panel *GetAsPanel(){ return this; }
	virtual bool AllowConCommandsWhileAlive(){return false;}
	virtual void SetPlayerPreferredMode( int mode );
	virtual void SetPlayerPreferredViewSize( float viewSize );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

protected:	// private structures & types

	// list of game events the hLTV takes care of

	typedef struct {
		int		xpos;
		int		ypos;
	} FootStep_t;	

	// Extra stuff in a this-level parallel array
	typedef struct CSMapPlayer_s {
		int		overrideIcon; // if not -1, the icon to use instead
		int		overrideIconOffscreen; // to use with overrideIcon
		float	overrideFadeTime; // Time to start fading the override icon
		float	overrideExpirationTime; // Time to not use the override icon any more
		Vector	overridePosition; // Where the overridden icon will draw
		QAngle	overrideAngle; // And at what angle
		bool	isDead;		// Death latch, since client can be behind the times on health messages.
		float	timeLastSeen; // curtime that we last saw this guy.
		float	timeFirstSeen; // curtime that we started seeing this guy
		bool	isHostage;	// Not a full player, a hostage.  Special icon, different death event
		float	flashUntilTime;
		float	nextFlashPeakTime;
		int		currentFlashAlpha;
	} CSMapPlayer_t;

	typedef struct CSMapBomb_s
	{
		Vector position;

		enum BombState
		{
			BOMB_PLANTED,	//planted and ticking down
			BOMB_DROPPED,	//dropped and lying loose
			BOMB_CARRIED,	//in the arms of a player
			BOMB_GONE,		//defused or exploded, but was planted
			BOMB_INVALID	//no bomb
		};
		BombState state;

		float timeLastSeen;
		float timeFirstSeen;
		float timeFade;
		float timeGone;

		float currentRingRadius;
		float currentRingAlpha;
		float maxRingRadius;
		float ringTravelTime;
	} CSMapBomb_t;

	typedef struct CSMapGoal_s
	{
		Vector position;
		int iconToUse;
	} CSMapGoal_t;

public: // IViewPortPanel interface:

	virtual void Update();
	virtual void Init( void );

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent(vgui::VPANEL parent) { BaseClass::SetParent(parent); }

	// IGameEventListener

	virtual void FireGameEvent( IGameEvent *event);

	// VGUI overrides

	// Player settings:
	void SetPlayerSeen( int index );
	void SetBombSeen( bool seen );

	// general settings:
	virtual void SetMap(const char * map);
	virtual void SetMode( int mode );

	// Object settings
	virtual void	FlashEntity( int entityID );

	// rules that define if you can see a player on the overview or not
	virtual bool CanPlayerBeSeen(MapPlayer_t *player);

	virtual int GetIconNumberFromTeamNumber( int teamNumber );

protected:

	virtual void	DrawCamera();
	virtual void	DrawMapTexture();
	virtual void	DrawMapPlayers();
	void			DrawHostages();
	void			DrawBomb();
	void			DrawGoalIcons();
	virtual void	ResetRound();
	virtual void	InitTeamColorsAndIcons();
	virtual void	UpdateSizeAndPosition();
	void			UpdateGoalIcons();
	void			ClearGoalIcons();
	virtual bool	IsRadarLocked();
	Vector2D		PanelToMap( const Vector2D &panelPos );

	bool			AdjustPointToPanel(Vector2D *pos);
	MapPlayer_t*	GetPlayerByEntityID( int entityID );
	MapPlayer_t*	GetHostageByEntityID( int entityID );
	virtual void	UpdatePlayers();
	void			UpdateHostages();///< Update hostages in the MapPlayer list
	void			UpdateBomb();
	void			UpdateFlashes();
	bool			CreateRadarImage(const char *mapName, const char *radarFileName);
	virtual bool	RunHudAnimations(){ return false; }

private:
	bool			DrawIconCS(	int textureID,
		int offscreenTextureID,
		Vector pos,
		float scale,
		float angle,
		int alpha,
		bool allowRotation = true,
		const char *text = NULL,
		Color *textColor = NULL,
		float status = -1,
		Color *statusColor = NULL 
		);

	int GetMasterAlpha( void );// The main alpha that the map part should be, determined by using the mode to look at the right convar
	int GetBorderSize( void );// How far in from the edge of the panel we draw, based on mode.  Let's the background fancy corners show.
	CSMapPlayer_t* GetCSInfoForPlayerIndex( int index );
	CSMapPlayer_t* GetCSInfoForPlayer(MapPlayer_t *player);
	CSMapPlayer_t* GetCSInfoForHostage(MapPlayer_t *hostage);
	bool CanHostageBeSeen(MapPlayer_t *hostage);

	CSMapPlayer_t	m_PlayersCSInfo[MAX_PLAYERS];
	CSMapBomb_t		m_bomb;
	MapPlayer_t		m_Hostages[MAX_HOSTAGES];
	CSMapPlayer_t	m_HostagesCSInfo[MAX_HOSTAGES];

	CUtlVector< CSMapGoal_t > m_goalIcons;
	bool m_goalIconsLoaded;

	int		m_TeamIconsSelf[MAP_ICON_COUNT];
	int		m_TeamIconsDead[MAP_ICON_COUNT];
	int		m_TeamIconsOffscreen[MAP_ICON_COUNT];
	int		m_TeamIconsDeadOffscreen[MAP_ICON_COUNT];

	int		m_bombIconPlanted;
	int		m_bombIconDropped;
	int		m_bombIconCarried;
	int		m_bombRingPlanted;
	int		m_bombRingDropped;
	int		m_bombRingCarried;
	int		m_bombRingCarriedOffscreen;
	int		m_radioFlash;
	int		m_radioFlashOffscreen;
	int		m_radarTint;
	int		m_hostageFollowing;
	int		m_hostageFollowingOffscreen;
	int		m_playerFacing;
	int		m_cameraIconFirst;
	int		m_cameraIconThird;
	int		m_cameraIconFree;
	int		m_hostageRescueIcon;
	int		m_bombSiteIconA;
	int		m_bombSiteIconB;

	int	 m_nRadarMapTextureID;	// texture id for radar version of current overview image

	int m_playerPreferredMode; // The mode the player wants to be in for when we aren't being the radar
};

#endif // CSSPECTATORGUI_H
