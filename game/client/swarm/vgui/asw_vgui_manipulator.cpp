#include "cbase.h"
#include "asw_vgui_manipulator.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include <vgui/iinput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

// This panel is used to manipulate other panels and report their sizes, for easy coding and positioning of panels
//  I know there's editablepanel, but this class can attach to any panel and is designed for the way I've been coding vgui stuff so far (i.e. code created and driven, with lots of animation, frameless)

// todo:
// [ ] Check our panel doesn't block mouse clicks
// [ ] Figure out how we're going to create and attach one of these

CASW_VGUI_Manipulator *g_pASW_VGUI_Manipulator = NULL;

CASW_VGUI_Manipulator::CASW_VGUI_Manipulator(vgui::Panel *pParent, vgui::Panel *pTarget) :
	Panel(pParent, "ASWManipulator")
{
	SetTarget(pTarget);

	SetMouseInputEnabled(true);
	MoveToFront();

	m_pResizeButton[0] = new vgui::Button(this, "ManipButtonLL", "<", this, "ResizeLeftLeft");
	m_pResizeButton[1] = new vgui::Button(this, "ManipButtonLR", ">", this, "ResizeLeftRight");
	m_pResizeButton[2] = new vgui::Button(this, "ManipButtonRR", ">", this, "ResizeRightRight");
	m_pResizeButton[3] = new vgui::Button(this, "ManipButtonRL", "<", this, "ResizeRightLeft");
	m_pResizeButton[4] = new vgui::Button(this, "ManipButtonUU", "^", this, "ResizeUpperUp");
	m_pResizeButton[5] = new vgui::Button(this, "ManipButtonUD", "v", this, "ResizeUpperDown");
	m_pResizeButton[6] = new vgui::Button(this, "ManipButtonLD", "v", this, "ResizeLowerDown");
	m_pResizeButton[7] = new vgui::Button(this, "ManipButtonLU", "^", this, "ResizeLowerUp");

	m_pPositionLabel = new vgui::Label(this, "ManipPositionLabel", "Position:");	
	m_pPositionLabel->SetMouseInputEnabled(true);
}

CASW_VGUI_Manipulator::~CASW_VGUI_Manipulator()
{

}

void CASW_VGUI_Manipulator::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	ResizeFor(ScreenWidth(), ScreenHeight());
}

void CASW_VGUI_Manipulator::ResizeFor(int width, int height)
{	
	if (!GetParent())
		return;
	SetBounds(0, 0, GetParent()->GetWide(), GetParent()->GetTall());	// make our manipulator full screen
	m_pPositionLabel->SetSize(GetParent()->GetWide(), GetParent()->GetTall() * 0.1f);
	PositionButtons();	// position our arrow buttons around the target panel
}

void CASW_VGUI_Manipulator::Think()
{
	if (!m_pTarget)
		return;

	int x, y, w, t;
	m_pTarget->GetBounds(x,y,w,t);
	//m_pTarget->LocalToScreen(x, y);	
	float fx = float(x) / float(GetWide());
	float fy = float(y) / float(GetTall());
	float fw = float(w) / float(GetWide());
	float ft = float(t) / float(GetTall());
	char buffer[512];
	Q_snprintf(buffer, sizeof(buffer), "Name: %s  Class: %s\nPosition: %d, %d, %d, %d\nFraction: %f, %f, %f, %f  (%f/%f)",
		m_pTarget->GetName(), m_pTarget->GetClassName(),
		x,y,w,t,
		fx, fy, fw, ft, float(GetWide()), float(GetTall()));
	m_pPositionLabel->SetText(buffer);

	int current_posx, current_posy;
	vgui::input()->GetCursorPos(current_posx, current_posy);
	if (current_posy < GetTall() * 0.5f)
	{
		m_pPositionLabel->SetPos(0,GetTall() * 0.9f);
	}
	else
	{
		m_pPositionLabel->SetPos(0,0);
	}

	PositionButtons();
}

void CASW_VGUI_Manipulator::PositionButtons()
{
	if (!m_pTarget)
		return;

	int iButtonSize = ScreenWidth() * 0.02f;
	for (int i=0;i<8;i++)
	{
		m_pResizeButton[i]->SetSize(iButtonSize, iButtonSize);
	}
	int x, y, w, t;
	m_pTarget->GetBounds(x,y,w,t);
	//m_pTarget->LocalToScreen(x,y);	

	m_pResizeButton[0]->SetPos(x - iButtonSize, y + t * 0.5f - iButtonSize);	// left left
	m_pResizeButton[1]->SetPos(x - iButtonSize, y + t * 0.5f);	// left right
	m_pResizeButton[2]->SetPos(x + w, y + t * 0.5f);	// right right
	m_pResizeButton[3]->SetPos(x + w, y + t * 0.5f - iButtonSize);	// right left
	m_pResizeButton[4]->SetPos(x + w * 0.5f - iButtonSize, y - iButtonSize);	// upper up
	m_pResizeButton[5]->SetPos(x + w * 0.5f, y - iButtonSize);	// upper down
	m_pResizeButton[6]->SetPos(x + w * 0.5f, y + t);	// lower down
	m_pResizeButton[7]->SetPos(x + w * 0.5f - iButtonSize, y + t);	// lower up
}

void CASW_VGUI_Manipulator::OnCommand(const char *command)
{
	int iShiftAmount = 1;
	int x, y, w, t;
	m_pTarget->GetBounds(x,y,w,t);
	// todo: if shift/ctrl is held down, adjust the size accordingly
	if (!stricmp(command, "ResizeLeftRight"))
	{
		x += iShiftAmount;
	}
	else if (!stricmp(command, "ResizeLeftLeft"))
	{
		x -= iShiftAmount;
	}
	else if (!stricmp(command, "ResizeRightRight"))
	{
		w += iShiftAmount;
	}
	else if (!stricmp(command, "ResizeRightLeft"))
	{
		w -= iShiftAmount;
	}
	else if (!stricmp(command, "ResizeUpperDown"))
	{
		y += iShiftAmount;
	}
	else if (!stricmp(command, "ResizeUpperUp"))
	{
		y -= iShiftAmount;
	}
	else if (!stricmp(command, "ResizeLowerDown"))
	{
		t += iShiftAmount;
	}
	else if (!stricmp(command, "ResizeLowerUp"))
	{
		t -= iShiftAmount;
	}
	else
	{
		BaseClass::OnCommand(command);
	}
	m_pTarget->SetBounds(x,y,w,t);
}


void CASW_VGUI_Manipulator::EditPanel(vgui::Panel *pParent, vgui::Panel *pTargetPanel)
{
	Msg("EditPanel request\n");
	if (g_pASW_VGUI_Manipulator != NULL)
	{
		delete g_pASW_VGUI_Manipulator;
	}

	if (pTargetPanel != NULL && pParent != NULL)
	{
		g_pASW_VGUI_Manipulator = new CASW_VGUI_Manipulator(pParent, pTargetPanel);
		g_pASW_VGUI_Manipulator->ResizeFor(ScreenWidth(), ScreenHeight());
	}
}

void CASW_VGUI_Manipulator::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	for (int i=0;i<8;i++)
	{
		m_pResizeButton[i]->SetDefaultColor(Color(68,140,203,255), Color(0,0,0,255));
		m_pResizeButton[i]->SetArmedColor(Color(255,255,255,255), Color(65,74,96,255));
		m_pResizeButton[i]->SetDepressedColor(Color(255,255,255,255), Color(65,74,96,255));
		m_pResizeButton[i]->SetDefaultBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
		m_pResizeButton[i]->SetDepressedBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
		m_pResizeButton[i]->SetKeyFocusBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));		
	}
	
	m_pPositionLabel->SetPaintBackgroundEnabled(true);
	m_pPositionLabel->SetBgColor(Color(0,0,255,64));
}

void CASW_VGUI_Manipulator::OnMouseReleased(vgui::MouseCode code)
{
	if ( code == MOUSE_LEFT )
	{
		// iterate through all of our parent's children, seeing if any are under the mouse and sets one as the target if so
		FindAllPanelsUnderCursor();
	}
}

void CASW_VGUI_Manipulator::FindAllPanelsUnderCursor()
{
	m_UnderCursorList.RemoveAll();

	FindAllPanelsUnderCursorRecurse(GetParent());

	// is our current target in the list?
	int c = m_UnderCursorList.Count();
	for (int i=0;i<c-1;i++)
	{
		if (m_UnderCursorList[i] == m_pTarget)
		{
			SetTarget(m_UnderCursorList[i+1]);
			return;
		}
	}
	// otherwise just grab the first one in the list
	if (c > 0)
	{
		SetTarget(m_UnderCursorList[0]);
		return;
	}

	Msg("No panel under cursor!");
}

// 
void CASW_VGUI_Manipulator::FindAllPanelsUnderCursorRecurse(vgui::Panel *pPanel)
{
	if (!pPanel)
		return;
	
	for (int i=0;i<pPanel->GetChildCount();i++)
	{
		Panel *pChild = pPanel->GetChild(i);
		// add child to the list?
		if (pChild->IsCursorOver() && !ShouldSkipPanel(pChild))
			m_UnderCursorList.AddToTail(pChild);
		// get all the child's children too
		FindAllPanelsUnderCursorRecurse(pChild);
	}
}

bool CASW_VGUI_Manipulator::ShouldSkipPanel(vgui::Panel *pPanel)
{
	// Don't manipulate ourselves!  We'll go blind
	if (pPanel == this && pPanel->GetParent() == this)
		return true;

	// try to skip backdrops and the window itself
	int x,y,w,t;
	pPanel->GetBounds(x,y,w,t);
	if (x == 0 && y == 0 && w>=GetWide() && t>=GetTall())
		return true;

	return false;
}

void CASW_VGUI_Manipulator::SetTarget(vgui::Panel *pPanel)
{
	m_pTarget = pPanel;
}
