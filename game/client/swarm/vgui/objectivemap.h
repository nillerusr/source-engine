#ifndef _INCLUDED_OBJECTIVE_MAP_H
#define _INCLUDED_OBJECTIVE_MAP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui/IImage.h>
#include "igameevents.h"
#include "asw_hud_minimap.h"

class C_ASW_Objective;
class ObjectiveDetailsPanel;
class ObjectiveMapMarkPanel;
class ObjectiveMapDrawingPanel;
class CASWHudMinimap;
class KeyValues;

// responsible for managing all the ObjectiveTitlePanels


// this panel shows the map on the mission tab of the briefing

class ObjectiveMap : public vgui::Panel, public CASWMap
{
	DECLARE_CLASS_SIMPLE( ObjectiveMap, vgui::Panel );
public:
	ObjectiveMap(Panel *parent, const char *name);
	virtual ~ObjectiveMap();

	virtual void PerformLayout();
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void UpdateMap();
	void SetObjective(C_ASW_Objective* pObjective);
	void SetDetailsPanel(ObjectiveDetailsPanel* pPanel) { m_pDetailsPanel = pPanel; }

	// CASWMap
	virtual Vector2D MapTextureToPanel( const Vector2D &texturepos );
	virtual Vector2D GetMapCornerInPanel( void ) { return vec2_origin; }
	virtual int GetFacingArcTexture( void ) { return m_nFacingArcTexture; }
	virtual int GetWidth( void ) { return GetWide(); }
	virtual int GetMapSize( void ) { return GetWide(); }
	virtual int GetBlipSize( void ) { return GetWide() * 0.02f; }
	virtual int GetArcSize( void );

	vgui::ImagePanel* m_pMapImage;
	C_ASW_Objective* m_pObjective;
	C_ASW_Objective* m_pQueuedObjective;
	ObjectiveDetailsPanel* m_pDetailsPanel;
	vgui::Label* m_pNoMapLabel;

	CPanelAnimationVarAliasType( int, m_nFacingArcTexture, "FacingArcTexture", "vgui/swarm/HUD/MarineFacing", "textureid" );

	bool m_bHaveQueuedItem;	
	Vector	m_MapOrigin;	// read from KeyValues files
	float	m_fMapScale;	// origin and scale used when screenshot was made
		
	void SetMap(const char * levelname);
	KeyValues * m_MapKeyValues; // keyvalues describing overview parameters

	void GetDesiredMapBounds(int &mx, int &my, int &mw, int &mt);

	ObjectiveMapDrawingPanel* m_pMapDrawing;

	// support up to 6 markings
	void FindMapMarks();
	
	int m_iNumMapMarks;
	ObjectiveMapMarkPanel* m_MapMarkPanels[ASW_NUM_MAP_MARKS];
};

// this panel handles drawing on the map during the briefing

class ObjectiveMapDrawingPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveMapDrawingPanel, vgui::Panel );
public:
	ObjectiveMapDrawingPanel(Panel *parent, const char *name);
	void TextureToLinePanel(CASWHudMinimap* pMap, const Vector2D &blip_centre, float &x, float &y);
	void SendMapLine(int x, int y, bool bInitial);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void OnCursorExited();
	virtual void OnCursorMoved(int x, int y);
	virtual void Paint();
	virtual void OnThink();
	bool m_bDrawingMapLines;
	float m_fLastMapLine;
	int m_iMouseX, m_iMouseY;

	CPanelAnimationVarAliasType( int, m_nTopLeftBracketTexture, "TopLeftBracketTexture", "vgui/swarm/Briefing/TopLeftBracket", "textureid" );
};



#endif // _INCLUDED_OBJECTIVE_MAP_H

