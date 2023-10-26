#ifndef _DEFINED_ASW_VGUI_HACK_WIRE_TILE
#define _DEFINED_ASW_VGUI_HACK_WIRE_TILE

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "ImageButton.h"
#include "c_asw_hack_wire_tile.h"
#include "asw_vgui_frame.h"
#include "asw_gamerules.h"

namespace vgui
{
	class ImagePanel;
};
class CASW_Wire_Tile;

// panels used by the wire tile hacking

class CASW_VGUI_Hack_Wire_Tile : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Hack_Wire_Tile, vgui::Panel );

	CASW_VGUI_Hack_Wire_Tile( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Wire_Tile* pHack );
	
	virtual void ASWInit();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void PaintBackground();
	virtual void Paint();
	virtual void PerformLayout();
	void DrawTile(int iWire, int x, int y);
	bool CursorOverTile( int x, int y, int &iWire, int &tilex, int &tiley );
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(const char *command);
	void TileClicked(int iWire, int tilex, int tiley);

	bool IsCursorOverWireTile( int x, int y );
	
	// note: tiles themselves are draw in Paint (since they need to rotate, etc.)

	// todo: entry and exit wires
	// todo: lights for when each wire is unlocked?
	// todo: backdrop, any other deco

	// progress bars for each wire
	vgui::Panel* m_pProgressBarBackdrop;
	vgui::Panel* m_pProgressBar;
	vgui::Label* m_pWireLabel;
	vgui::Label* m_pCompleteLabel;
	bool m_bLastAllLit;

	vgui::ImagePanel* m_pOpenLight[4];

	static void PrecacheHackingTextures();
	//static int s_nBackDropTexture;
	static int s_nTileHoriz;
	static int s_nTileLeft;
	static int s_nTileRight;
	static int s_nLeftConnect;
	static int s_nRightConnect;
	static bool s_bLoadedTextures;

	vgui::ImagePanel *m_pLeftConnect[4];
	vgui::ImagePanel *m_pRightConnect[4];
	vgui::ImagePanel *m_pFastMarker;
	vgui::Label* m_pAccessLoggedLabel;
	CASW_Wire_Tile* m_pTile[4][ASW_MAX_TILE_COLUMNS][ASW_MAX_TILE_ROWS];

	// current door hack
	CHandle<C_ASW_Hack_Wire_Tile> m_hHack;

	// overall scale of this window
	float m_fScale;
	int m_iSpacePerWire;

	float m_fLastThinkTime;
};

class CASW_Wire_Tile : public ImageButton
{
public:
	DECLARE_CLASS_SIMPLE( CASW_Wire_Tile, ImageButton );

	CASW_Wire_Tile(vgui::Panel *pParent, const char *name, CASW_VGUI_Hack_Wire_Tile* pHackWireTile);
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	CASW_VGUI_Hack_Wire_Tile* m_pHackWireTile;
	int m_iWire, m_x, m_y;
	CPanelAnimationVarAliasType( int, m_nWhiteTexture, "White", "vgui/white", "textureid" );
	CPanelAnimationVarAliasType( int, m_nLockedTexture, "Locked", "vgui/swarm/Computer/TVNoise", "textureid" );	
};

class CASW_VGUI_Hack_Wire_Tile_Container : public CASW_VGUI_Frame
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Hack_Wire_Tile_Container, CASW_VGUI_Frame );

	CASW_VGUI_Hack_Wire_Tile_Container(Panel *parent, const char *panelName, C_ASW_Hack_Wire_Tile* pHack);
	virtual ~CASW_VGUI_Hack_Wire_Tile_Container();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();

	virtual void OnThink();

	// current door hack
	CHandle<C_ASW_Hack_Wire_Tile> m_hHack;
};

#endif /* _DEFINED_ASW_VGUI_HACK_WIRE_TILE */