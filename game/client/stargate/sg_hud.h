//========= StargateTC ============================================================//
//
// sg_hud.h - StargateTC custom HUD element.
//
// 1:1 port of the single large GoldSrc client HUD element (decompiled
// FUN_100718df / Init FUN_10071193 / VidInit FUN_10071390, singleton
// &DAT_100e67f0). The original drew ALL Stargate-specific HUD from one element;
// we keep that structure here as one full-screen CHudElement that paints every
// sub-block with ScreenWidth/Height-relative coordinates, matching the original.
//
// See StargateTCSource/docs/HUD_PORT_SPEC.md for the full element/message map.
//=================================================================================//
#ifndef SG_HUD_H
#define SG_HUD_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include "tier1/UtlDict.h"

#define SG_MAX_BLIPS      256
#define SG_MAX_PORTRAITS  33

struct SgBlip_t
{
	int   type;
	float x;        // world-relative offset supplied by server
	float y;
	int   alpha;    // 0..255, decays each frame
};

struct SgPortrait_t
{
	bool  used;
	int   id;       // player entindex
	int   team;
	char  avatar[32];
};

//-----------------------------------------------------------------------------
// CHudStargate - the whole Stargate HUD in one element (faithful to GoldSrc).
//-----------------------------------------------------------------------------
class CHudStargate : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudStargate, vgui::Panel );

public:
	explicit CHudStargate( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual bool ShouldDraw( void );
	virtual void OnThink( void );
	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// --- Server user messages (19, prefixed Sg to avoid HL2MP clashes) ---
	void MsgFunc_SgAddPortrait( bf_read &msg );
	void MsgFunc_SgReset( bf_read &msg );
	void MsgFunc_SgAboutMe( bf_read &msg );
	void MsgFunc_SgTogglePor( bf_read &msg );
	void MsgFunc_SgSetRndTim( bf_read &msg );
	void MsgFunc_SgNewRnd( bf_read &msg );
	void MsgFunc_SgZatTouch( bf_read &msg );
	void MsgFunc_SgRadio( bf_read &msg );
	void MsgFunc_SgArmor( bf_read &msg );
	void MsgFunc_SgStamina( bf_read &msg );
	void MsgFunc_SgSubModel( bf_read &msg );
	void MsgFunc_SgHasKey( bf_read &msg );
	void MsgFunc_SgHideHud( bf_read &msg );
	void MsgFunc_SgHudInfoMode( bf_read &msg );
	void MsgFunc_SgTicketMode( bf_read &msg );
	void MsgFunc_SgGtickets( bf_read &msg );
	void MsgFunc_SgTtickets( bf_read &msg );
	void MsgFunc_SgProgress( bf_read &msg );
	void MsgFunc_SgBlip( bf_read &msg );

private:
	// texture cache + sprite draw helpers
	int   GetTex( const char *pszMat );
	// Draw a HUD sprite sized in GoldSrc-native px (auto pow2-UV from 4x atlas).
	void  DrawSpr( const char *pszMat, float sx, float sy,
	               int nativeW, int nativeH, const Color &c );
	float Scale( void ) const;     // screen-height based HUD scale
	Color TeamColor( int team ) const;

	// draw sub-blocks
	void  DrawFrame( int sw, int sh, float s );
	void  DrawRadar( int sw, int sh, float s );
	void  DrawStatBars( int sw, int sh, float s );
	void  DrawNaquada( int sw, int sh, float s );
	void  DrawTickets( int sw, int sh, float s );
	void  DrawRoundTimer( int sw, int sh, float s );
	void  DrawTac( int sw, int sh, float s );
	void  DrawBombKey( int sw, int sh, float s );
	void  DrawPortraits( int sw, int sh, float s );
	void  DrawZatOverlay( int sw, int sh, float s );
	void  DrawHudNumber( int value, float x, float y, float s, const Color &c );

	CUtlDict< int, int > m_TexIds;     // material name -> surface texture id

	// --- HUD state (mirrors the GoldSrc element's fields) ---
	int   m_iLocalTeam;
	int   m_iLocalId;
	float m_flArmor;
	float m_flStamina;
	int   m_iSubModel;
	bool  m_bHasKey;
	int   m_iHideBits;
	int   m_iInfoMode;       // 0 none / 1 red / 2 yellow / 3 green
	bool  m_bTicketMode;
	int   m_iGtickets;
	int   m_iTtickets;
	int   m_iProgress;       // 0..100
	float m_flRoundLen;
	float m_flRoundStart;
	bool  m_bZat;
	float m_flZatTime;
	float m_flSlideOffset;

	SgBlip_t     m_Blips[ SG_MAX_BLIPS ];
	SgPortrait_t m_Portraits[ SG_MAX_PORTRAITS ];
};

#endif // SG_HUD_H
