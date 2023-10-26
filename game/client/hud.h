//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: CHud handles the message, calculation, and drawing the HUD
//
// $NoKeywords: $
//=============================================================================//
#ifndef HUD_H
#define HUD_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlVector.h"
#include "UtlDict.h"
#include "ConVar.h"
#include <vgui/vgui.h>
#include <Color.h>
#include <bitbuf.h>

namespace vgui
{
	class IScheme;
	class Panel;
}

// basic rectangle struct used for drawing
typedef struct wrect_s
{
	int	left;
	int right;
	int top;
	int bottom;
} wrect_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudTexture
{
public:
	CHudTexture()
	{
		Q_memset( szShortName, 0, sizeof( szShortName ) );
		Q_memset( szTextureFile, 0, sizeof( szTextureFile ) );
		Q_memset( texCoords, 0, sizeof( texCoords ) );
		Q_memset( &rc, 0, sizeof( rc ) );
		textureId = -1;
		bRenderUsingFont = false;
		bPrecached = false;
		cCharacterInFont = 0;
		hFont = NULL;
	}

	CHudTexture& operator =( const CHudTexture& src )
	{
		if ( this == &src )
			return *this;

		Q_strncpy( szShortName, src.szShortName, sizeof( szShortName ) );
		Q_strncpy( szTextureFile, src.szTextureFile, sizeof( szTextureFile ) );
		Q_memcpy( texCoords, src.texCoords, sizeof( texCoords ) );
		textureId = src.textureId;
		rc = src.rc;
		bRenderUsingFont = src.bRenderUsingFont;
		cCharacterInFont = src.cCharacterInFont;
		hFont = src.hFont;

		return *this;
	}

	int Width() const
	{
		return rc.right - rc.left;
	}

	int Height() const
	{
		return rc.bottom - rc.top;
	}

	// causes the font manager to generate the glyph, prevents run time hitches on platforms that have slow font managers
	void Precache( void );

	// returns width & height of icon with scale applied (scale is ignored if font is used to render)
	int EffectiveWidth( float flScale ) const;
	int EffectiveHeight( float flScale ) const;

	void DrawSelf( int x, int y, const Color& clr ) const;
	void DrawSelf( int x, int y, int w, int h, const Color& clr ) const;
	void DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, Color clr ) const;
	// new version to scale the texture over a finalWidth and finalHeight passed in
	void DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, int finalWidth, int finalHeight, Color clr ) const;
	void DrawSelfScalableCorners( int x, int y, int w, int h, int iSrcCornerW, int iSrcCornerH, int iDrawCornerW, int iDrawCornerH, Color clr ) const;

	char		szShortName[ 64 ];
	char		szTextureFile[ 64 ];

	bool		bRenderUsingFont;
	bool		bPrecached;
	char		cCharacterInFont;
	vgui::HFont hFont;

	// vgui texture Id assigned to this item
	int			textureId;
	// s0, t0, s1, t1
	float		texCoords[ 4 ];

	// Original bounds
	wrect_t		rc;
};

#include "hudtexturehandle.h"

class CHudElement;
class CHudRenderGroup;

//-----------------------------------------------------------------------------
// Purpose: Main hud manager
//-----------------------------------------------------------------------------
class CHud 
{
public:
	//For progress bar orientations
	static const int			HUDPB_HORIZONTAL;
	static const int			HUDPB_VERTICAL;
	static const int			HUDPB_HORIZONTAL_INV;

public:
								CHud();
								~CHud();

	// Init's called when the HUD's created at DLL load
	void						Init( void );
	// VidInit's called when the video mode's changed
	void						VidInit( void );
	// Shutdown's called when the engine's shutting down
	void						Shutdown( void );
	// LevelInit's called whenever a new level's starting
	void						LevelInit( void );
	// LevelShutdown's called whenever a level's finishing
	void						LevelShutdown( void );
	
	void						ResetHUD( void );

	// A saved game has just been loaded
	void						OnRestore();

	void						Think();

	void						ProcessInput( bool bActive );
	void						UpdateHud( bool bActive );

	void						InitColors( vgui::IScheme *pScheme );

	// Hud element registration
	void						AddHudElement( CHudElement *pHudElement );
	void						RemoveHudElement( CHudElement *pHudElement );
	// Search list for "name" and return the hud element if it exists
	CHudElement					*FindElement( const char *pName );
	
	bool						IsHidden( int iHudFlags );

	float						GetSensitivity();
	float						GetFOVSensitivityAdjust();

	void						DrawProgressBar( int x, int y, int width, int height, float percentage, Color& clr, unsigned char type );
	void						DrawIconProgressBar( int x, int y, CHudTexture *icon, CHudTexture *icon2, float percentage, Color& clr, int type );

	// User messages
	void						MsgFunc_ResetHUD(bf_read &msg);
	void 						MsgFunc_SendAudio(bf_read &msg);

	// Hud Render group
	int							LookupRenderGroupIndexByName( const char *pszGroupName );
	bool						LockRenderGroup( int iGroupIndex, CHudElement *pLocker = NULL );
	bool						UnlockRenderGroup( int iGroupIndex, CHudElement *pLocker = NULL );
	bool						IsRenderGroupLockedFor( CHudElement *pHudElement, int iGroupIndex );
	int							RegisterForRenderGroup( const char *pszGroupName );
	int							AddHudRenderGroup( const char *pszGroupName );
	bool						DoesRenderGroupExist( int iGroupIndex );

	void						SetScreenShotTime( float flTime ){ m_flScreenShotTime = flTime; }

	CUtlVector< CHudElement * > &GetHudList();
	const CUtlVector< CHudElement * > &GetHudList() const;
	CUtlVector< vgui::Panel * > &GetHudPanelList();
	const CUtlVector< vgui::Panel * > &GetHudPanelList() const;
	void						OnSplitScreenStateChanged();

public:

	int							m_iKeyBits;
#ifndef _XBOX
	float						m_flMouseSensitivity;
	float						m_flMouseSensitivityFactor;
#endif
	float						m_flFOVSensitivityAdjust;

	Color						m_clrNormal;
	Color						m_clrCaution;
	Color						m_clrYellowish;

	CUtlVector< CHudElement * >	m_HudList;
	// Same list as above, but with vgui::Panel dynamic_cast precomputed.  These should all be non-NULL!!!
	CUtlVector< vgui::Panel * >	m_HudPanelList; 

private:
	void						InitFonts();


	CUtlVector< const char * >				m_RenderGroupNames;
	CUtlMap< int, CHudRenderGroup * >		m_RenderGroups;

	float						m_flScreenShotTime; // used to take end-game screenshots
	int							m_nSplitScreenSlot;
	bool						m_bEngineIsInGame;
};

CHud &GetHud( int nSlot = -1 );

class CHudIcons
{
public:
	CHudIcons();
	~CHudIcons();

	void						Init();
	void						Shutdown();

	CHudTexture					*GetIcon( const char *szIcon );

	// loads a new icon into the list, without duplicates
	CHudTexture					*AddUnsearchableHudIconToList( CHudTexture& texture );
	CHudTexture					*AddSearchableHudIconToList( CHudTexture& texture );

	void						RefreshHudTextures();

private:

	void						SetupNewHudTexture( CHudTexture *t );
	bool						m_bHudTexturesLoaded;
	// Global list of known icons
	CUtlDict< CHudTexture *, int >		m_Icons;

};

CHudIcons &HudIcons();

//-----------------------------------------------------------------------------
// Global fonts used in the client DLL
//-----------------------------------------------------------------------------
extern vgui::HFont g_hFontTrebuchet24;

void LoadHudTextures( CUtlDict< CHudTexture *, int >& list, char *szFilenameWithoutExtension, const unsigned char *pICEKey );

void GetHudSize( int& w, int &h );

#endif // HUD_H
