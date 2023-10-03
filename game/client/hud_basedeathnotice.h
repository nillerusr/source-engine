//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_BASEDEATHNOTICE_H
#define HUD_BASEDEATHNOTICE_H
#ifdef _WIN32
#pragma once
#endif

// Player entries in a death notice
struct DeathNoticePlayer
{
	DeathNoticePlayer()
	{
		szName[0] = 0;
		iTeam = TEAM_UNASSIGNED;
	}
	char		szName[MAX_PLAYER_NAME_LENGTH*2];	// big enough for player name and additional information
	int			iTeam;								// team #	
};

// Contents of each entry in our list of death notices
struct DeathNoticeItem 
{
	DeathNoticeItem() 
	{
		szIcon[0]=0;
		wzInfoText[0]=0;
		iconDeath = NULL;
		bSelfInflicted = false;
		flCreationTime = 0;
		bLocalPlayerInvolved = false;
	}

	float GetExpiryTime();

	DeathNoticePlayer	Killer;
	DeathNoticePlayer   Victim;
	char		szIcon[32];		// name of icon to display
	wchar_t		wzInfoText[32];	// any additional text to display next to icon
	CHudTexture *iconDeath;
	bool		bSelfInflicted;
	float		flCreationTime;
	bool		bLocalPlayerInvolved;
};

#define NUM_CORNER_COORD 10
#define NUM_BACKGROUND_COORD NUM_CORNER_COORD*4

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudBaseDeathNotice : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBaseDeathNotice, vgui::Panel );
public:
	CHudBaseDeathNotice( const char *pElementName );

	void VidInit( void );
	virtual void Init( void );
	virtual bool ShouldDraw( void );
	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	void RetireExpiredDeathNotices( void );

	void FireGameEvent( IGameEvent *event );

protected:
	virtual Color GetTeamColor( int iTeamNumber );
	virtual void OnGameEvent( IGameEvent *event, DeathNoticeItem &deathNoticeItem ) {};
	void DrawText( int x, int y, vgui::HFont hFont, Color clr, const wchar_t *szText );
	int AddDeathNoticeItem();
	void GetBackgroundPolygonVerts( int x0, int y0, int x1, int y1, int iVerts, vgui::Vertex_t vert[] );
	void CalcRoundedCorners();
	CHudTexture *GetIcon( const char *szIcon, bool bInvert );

	void GetLocalizedControlPointName( IGameEvent *event, char *namebuf, int namelen );

	CPanelAnimationVarAliasType( float, m_flLineHeight, "LineHeight", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLineSpacing, "LineSpacing", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flCornerRadius, "CornerRadius", "3", "proportional_float" );
	CPanelAnimationVar( float, m_flMaxDeathNotices, "MaxDeathNotices", "4" );
	CPanelAnimationVar( bool, m_bRightJustify, "RightJustify", "1" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "Default" );
	CPanelAnimationVar( Color, m_clrIcon, "IconColor", "255 80 0 255" );
	CPanelAnimationVar( Color, m_clrBaseBGColor, "BaseBackgroundColor", "46 43 42 220" );
	CPanelAnimationVar( Color, m_clrLocalBGColor, "LocalBackgroundColor", "245 229 196 200" );

	CUtlVector<DeathNoticeItem> m_DeathNotices;

	Vector2D	m_CornerCoord[NUM_CORNER_COORD];
};

#endif	// HUD_BASEDEATHNOTICE_H
