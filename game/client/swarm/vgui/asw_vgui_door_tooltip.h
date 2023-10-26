#ifndef _INCLUDED_ASW_VGUI_DOOR_TOOLTIP
#define _INCLUDED_ASW_VGUI_DOOR_TOOLTIP

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};
class C_ASW_Marine;
class C_ASW_Door;

// shows a small health bar over a door

class CASW_VGUI_Door_Tooltip : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Door_Tooltip, vgui::Panel );

	CASW_VGUI_Door_Tooltip( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Door_Tooltip();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void Paint();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void SetDoor(C_ASW_Door* pMarine);
	virtual void DrawHorizontalBar(int x, int y, int width, int height, float fFraction, int r, int g, int b, int a);
	bool GetDoorHealthBarPosition( C_ASW_Door *pDoor, int &screen_x, int &screen_y );

	C_ASW_Door* GetDoor();
	EHANDLE m_hDoor;
	EHANDLE m_hQueuedDoor;
	bool m_bQueuedDoor;
	float m_fNextUpdateTime;
	bool m_bDoingSlowFade;
	int m_iOldDoorHealth;
	float m_fDoorHealthFraction;
	int m_iDoorType;
	bool m_bShowDoorUnderCursor;			// if set to true, this panel will automatically update its door with the one near the mouse cursor

	CPanelAnimationVarAliasType( int, m_nNormalDoorHealthTexture, "DoorHealthBar",	 "vgui/swarm/HUD/HorizDoorHealthBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nReinforcedDoorHealthTexture, "DoorHealthBarReinf", "vgui/swarm/HUD/HorizDoorHealthBarR", "textureid" );
	CPanelAnimationVarAliasType( int, m_nIndestDoorHealthTexture, "DoorHealthBarInd", "vgui/swarm/HUD/HorizDoorHealthBarI", "textureid" );
	CPanelAnimationVarAliasType( int, m_nDoorShieldIconTexture, "DoorShieldIcon", "vgui/swarm/HUD/DoorHealth", "textureid" );
	CPanelAnimationVarAliasType( int, m_nDoorWeldIconTexture, "DoorWeldIcon", "vgui/swarm/HUD/DoorWeld", "textureid" );
};

#endif /* _INCLUDED_ASW_VGUI_DOOR_TOOLTIP */