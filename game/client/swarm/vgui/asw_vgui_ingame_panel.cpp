#include "cbase.h"
#include "vgui/ISurface.h"
#include <vgui/IInput.h>
#include "asw_vgui_ingame_panel.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// singleton panel manager
CASW_VGUI_Ingame_Panel_Manager g_IngamePanelManager;

CASW_VGUI_Ingame_Panel::CASW_VGUI_Ingame_Panel() //vgui::Panel *pParent, const char *pElementName)
//: Panel(pParent, pElementName),
: m_bMouseIsOver(false),
	m_iMouseX(0),
	m_iMouseY(0)
{
	g_IngamePanelManager.AddPanel(this);
}

CASW_VGUI_Ingame_Panel::~CASW_VGUI_Ingame_Panel()
{
	g_IngamePanelManager.RemovePanel(this);
}

bool CASW_VGUI_Ingame_Panel::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	// subclasses can decide if they want to swallow this mouse click or not
	return false;
}

// =============
// Panel Manager
// =============

void CASW_VGUI_Ingame_Panel_Manager::AddPanel(CASW_VGUI_Ingame_Panel *pPanel)
{
	if (m_IngamePanelList.Find(pPanel) != m_IngamePanelList.InvalidIndex())
	{
		Warning("Tried to add Ingame_Panel that is already on the list\n");
		return;
	}

	m_IngamePanelList.AddToTail(pPanel);
}
void CASW_VGUI_Ingame_Panel_Manager::RemovePanel(CASW_VGUI_Ingame_Panel *pPanel)
{
	if (!m_IngamePanelList.HasElement(pPanel))
	{
		Warning("Tried to remove an Ingame_Panel that wasn't on the list\n");
		return;
	}
	m_IngamePanelList.FindAndRemove(pPanel);	
}

void CASW_VGUI_Ingame_Panel_Manager::MovePanelToTop(CASW_VGUI_Ingame_Panel *pPanel)
{
	if (!pPanel)
	{
		Warning("Tried to move a non-existent Ingame Panel to the top\n");
		return;
	}
	vgui::Panel *pVGUIPanel = dynamic_cast<vgui::Panel *>(pPanel);
	if (pVGUIPanel)
		pVGUIPanel->MoveToFront();
	m_IngamePanelList.FindAndRemove(pPanel);
	m_IngamePanelList.AddToTail(pPanel);
}

void CASW_VGUI_Ingame_Panel_Manager::UpdateMouseOvers()
{
	int current_posx, current_posy;
	vgui::input()->GetCursorPos(current_posx, current_posy);

	// go through the panels in reverse order
	int iNumPanels = m_IngamePanelList.Count();
	bool bFoundTopPanel = false;
	for (int i=iNumPanels-1;i>=0;i--)
	{
		CASW_VGUI_Ingame_Panel* pPanel = m_IngamePanelList.Element(i);
		if (pPanel)
		{
			if (!bFoundTopPanel)
			{
				int x,y,wide,tall;
				vgui::Panel *pVGUIPanel = dynamic_cast<vgui::Panel *>(pPanel);
				if (pVGUIPanel)
				{
					pVGUIPanel->GetBounds(x,y,wide,tall);	// combination of GetPos/GetSize
					x=y=0;
					pVGUIPanel->LocalToScreen(x, y);
					if (current_posx > x && current_posx < (x+wide) &&
						current_posy > y && current_posy < (y+tall))
					{
						pPanel->SetMouseIsOver(current_posx, current_posy, true);
						bFoundTopPanel = true;
					}
				}
			}
			else
			{
				pPanel->SetMouseIsOver(current_posx, current_posy, false);
			}
		}
	}
}

extern bool MarineBusy();
bool CASW_VGUI_Ingame_Panel_Manager::SendMouseClick(bool bRightClick, bool bDown)
{		
	int current_posx, current_posy;
	vgui::input()->GetCursorPos(current_posx, current_posy);

	// go through the panels in reverse order
	int iNumPanels = m_IngamePanelList.Count();
	for (int i=iNumPanels-1;i>=0;i--)
	{
		CASW_VGUI_Ingame_Panel* pPanel = m_IngamePanelList.Element(i);
		if (pPanel)
		{
			int x,y,wide,tall;
			vgui::Panel *pVGUIPanel = dynamic_cast<vgui::Panel *>(pPanel);
			if (pVGUIPanel && IngamePanelVisible(pVGUIPanel))
			{
				x = 0;
				y = 0;
				wide = pVGUIPanel->GetWide();
				tall = pVGUIPanel->GetTall();
				pVGUIPanel->LocalToScreen(x, y);
				//Msg("Current x=%d y=%d | panel=%s x=%d y=%d w=%d h=%d\n", current_posx, current_posy, pVGUIPanel->GetName(), x, y, wide, tall);
				if (current_posx > x && current_posx < (x+wide) &&
					current_posy > y && current_posy < (y+tall))
				{
					if (pPanel->MouseClick(current_posx, current_posy, bRightClick, bDown))
						return true;
				}
			}
		}
	}
	if (MarineBusy() && bDown)	// we want to swallow all mouse clicks if the marine is busy with some panel on the screen, to stop firing getting stuck on etc
		return true;
	return false;
}


bool CASW_VGUI_Ingame_Panel_Manager::IngamePanelVisible(vgui::Panel* pPanel)
{
	if (!pPanel || !pPanel->IsVisible())
		return false;

	if (pPanel == GetClientMode()->GetViewport())
		return true;

	if (pPanel->GetParent())
		return IngamePanelVisible(pPanel->GetParent());

	return true;
}