#ifndef _INCLUDED_ASW_HUD_MINIMAP_H
#define _INCLUDED_ASW_HUD_MINIMAP_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_hudelement.h"
#include <vgui_controls/Panel.h>
#include "hud_numericdisplay.h"
#include "asw_gamerules.h"
#include "asw_vgui_ingame_panel.h"

namespace vgui
{
	class IScheme;
	class ImagePanel;
	class Label;
};

class CASWHudMinimapLinePanel;
class CASWHudMinimapFramePanel;
class ScanLinePanel;
class CASWHudMinimap;

#define MAP_LINE_INTERVAL 0.04f
#define MAP_LINE_SOLID_TIME 5.0f
#define MAP_LINE_FADE_TIME 3.0f

#define DRAWING_LINE_R 255
#define DRAWING_LINE_G 255
#define DRAWING_LINE_B 0

// a point for the lines drawn by players on the minimap
class MapLine
{
public:
	MapLine();

	Vector2D worldpos;		// blip in world space
	Vector2D blipcentre;	// blip in map texture space
	int player_index;
	Vector2D linkpos;			// link blip in world space
	Vector2D linkblipcentre;	// link blip in map texture space
	bool bLink;
	bool bSetBlipCentre;	// have we calculated the blip in map texture space yet?
	bool bSetLinkBlipCentre;	// have we calculated the link blip in map texture space yet?
	float created_time;
};


class CASWHudMinimap_Border : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudMinimap_Border, vgui::Panel );

public:
	CASWHudMinimap_Border( vgui::Panel *pParent, const char *pElementName, CASWHudMinimap* pMinimap );	
	virtual void PaintBackground();
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );

	CASWHudMinimap* m_pMinimap;
};


enum MapBlipTexture_t
{
	MAP_BLIP_TEXTURE_NORMAL = 0,
	MAP_BLIP_TEXTURE_USABLE,
	MAP_BLIP_TEXTURE_ITEM,
	MAP_BLIP_TEXTURE_DEATH,
	MAP_BLIP_TEXTURE_FRIENDLY_DAMAGE,
	MAP_BLIP_TEXTURE_KILL,
	MAP_BLIP_TEXTURE_HEAL,
	MAP_BLIP_TEXTURE_FOUND_AMMO,
	MAP_BLIP_TEXTURE_NO_AMMO,

	MAP_BLIP_TEXTURE_TOTAL
};

struct MapBlip_t
{
	MapBlip_t( Vector p_vWorldPos, Color p_rgbaColor, MapBlipTexture_t p_nTextureType )
		: vWorldPos( p_vWorldPos ), 
		  rgbaColor( p_rgbaColor ), 
		  nTextureType( p_nTextureType )
	{
	}

	Vector vWorldPos;
	Color rgbaColor;
	MapBlipTexture_t nTextureType;
};


abstract_class CASWMap
{
public:
	CASWMap( void )
	{
		LoadBlipTextures();
		m_pMinimap = NULL;
	}

	Vector2D WorldToMapTexture( const Vector &worldpos );
	virtual Vector2D MapTextureToPanel( const Vector2D &texturepos ) = 0;

	void AddBlip( const MapBlip_t &blip );
	void ClearBlips( void );

protected:
	void PaintMarineBlips();
	void PaintExtraBlips();
	void PaintWorldBlip(const Vector &worldpos, float fBlipStrength, Color BlipColor, MapBlipTexture_t nBlipTexture = MAP_BLIP_TEXTURE_NORMAL );
	void PaintWorldFacingArc(const Vector &worldpos, float fFacingYaw, Color FacingColor);

	void LoadBlipTextures();
	int m_nBlipTexture[7];
	int m_nTriBlipTexture[7];
	int m_nBlipTextureDeath;
	int m_nBlipTextureFriendlyDamage;
	int m_nBlipTextureKill;
	int m_nBlipTextureHeal;
	int m_nBlipTextureFoundAmmo;
	int m_nBlipTextureNoAmmo;

	virtual Vector2D GetMapCornerInPanel( void ) = 0;
	virtual int GetFacingArcTexture( void ) = 0;
	virtual int GetWidth( void ) = 0;
	virtual int GetMapSize( void ) = 0;
	virtual int GetBlipSize( void ) = 0;
	virtual int GetArcSize( void ) = 0;

	CUtlVector< MapBlip_t > m_MapBlips;

	CASWHudMinimap *m_pMinimap;
};


//-----------------------------------------------------------------------------
// Purpose: Shows the minimap in the corner of the screen
//-----------------------------------------------------------------------------
extern ConVar asw_draw_hud;
class CASWHudMinimap : public CASW_HudElement, public CHudNumericDisplay, public CASW_VGUI_Ingame_Panel, public CASWMap
{
	DECLARE_CLASS_SIMPLE( CASWHudMinimap, CHudNumericDisplay );

public:
	CASWHudMinimap( const char *pElementName );
	virtual ~CASWHudMinimap();
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	virtual void Paint();
	virtual void PaintMapSection();
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }
	//virtual void PaintFrame();
	virtual void PaintObjectiveMarkers();
	virtual void PaintBlips();
	virtual void PaintScannerBlips();
//	void PaintFollowLine(C_BaseEntity *pMarine, C_BaseEntity *pTarget);
	void PaintRect( int nX, int nY, int nWidth, int nHeight, Color color );
	virtual void CheckBlipSpeech(int iMarine);
	virtual void FireGameEvent(IGameEvent * event);
	void SetMap(const char * levelname);
	void GetScaledOffset(int &ox, int &oy);	// returns the top left corner of our map panel (moves to the lower right as the map scales down)
	float WorldDistanceToPixelDistance(float fWorldDistance);
	bool UseDrawCrosshair(float x, float y);
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);	
	bool IsWithinMapBounds(int x, int y);
	void ClipToMapBounds(int &x, int &y);
	void SendMapLine(int x, int y, bool bInitial);

	// CASWMap
	virtual Vector2D MapTextureToPanel( const Vector2D &texturepos );
	virtual Vector2D GetMapCornerInPanel( void ) { return m_MapCornerInPanel; }
	virtual int GetFacingArcTexture( void ) { return m_nFacingArcTexture; }
	virtual int GetWidth( void ) { return GetWide(); }
	virtual int GetMapSize( void ) { return m_iMapSize; }
	virtual int GetBlipSize( void ) { return m_iMapSize * 0.05f; }
	virtual int GetArcSize( void );

	//CPanelAnimationVarAliasType( int, m_nFrameTexture, "FrameTexture", "vgui/swarm/HUD/ASWHUDMinimap", "textureid" );
	CPanelAnimationVarAliasType( int, m_nFacingArcTexture, "FacingArcTexture", "vgui/swarm/HUD/MarineFacing", "textureid" );
	
	virtual void PerformLayout();
	CASWHudMinimap_Border *m_pBackdrop;

	Vector	m_MapOrigin;	// read from KeyValues files
	float	m_fMapScale;	// origin and sacle used when screenshot was made
	int m_nMapTextureID;
	int m_nWhiteTexture;
	float m_iMapSize;
	Vector2D  m_MapCentre;			// point on the map texture where the minimap is centred
	Vector2D  m_MapCornerInPanel;	// top left corner of the minimap within our panel
	Vector2D  m_MapCentreInPanel;	// centre of the minimap within our panel

	float m_fLastBlipSpeechTime;
	float m_fLastBlipHitTime;
	bool m_bDrawingMapLines;
	float m_fLastMapLine;
	bool m_bHasOverview;

	KeyValues * m_MapKeyValues; // keyvalues describing overview parameters
	CASWHudMinimapLinePanel *m_pLinePanel;
	CASWHudMinimapFramePanel *m_pFramePanel;	// for the border around the edge
	ScanLinePanel *m_pScanLinePanel;
	vgui::ImagePanel* m_pInterlacePanel;
	vgui::ImagePanel* m_pNoisePanel;

	char m_szMissionTitle[64];

	CUtlVector<MapLine> m_MapLines;	
	float m_fLastMinimapDrawSound;

	// store the servername here (used by PlayerListPanel)
	char m_szServerName[128];

private:
};


#endif // _INCLUDED_ASW_HUD_MINIMAP_H
