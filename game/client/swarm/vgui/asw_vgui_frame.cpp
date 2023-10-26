#include "cbase.h"
#include "asw_vgui_frame.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/TextImage.h"
#include "vgui/ILocalize.h"
#include "WrappedLabel.h"
#include <vgui_controls/ImagePanel.h>
#include <filesystem.h>
#include <keyvalues.h>
#include "c_asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Frame::CASW_VGUI_Frame( vgui::Panel *pParent, const char *pElementName, const char *szTitle ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{
	m_bMouseOverTitleBar = false;

	m_pTitleLabel = new vgui::Label(this, "FrameTitleLabel", szTitle);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_west);
	m_pCloseImage = new vgui::ImagePanel(this, "FrameCloseImage");
	m_pMiniImage = new vgui::ImagePanel(this, "MiniImage");
	m_iTitleBarHeight = 0;
	m_bDragging = false;
	m_bFrameMinimized = false;
	m_pNotifyHackOnClose = NULL;	
	m_bMouseOverCloseIcon = false;
	m_bMouseOverMiniIcon = false;
}

CASW_VGUI_Frame::~CASW_VGUI_Frame()
{
	if (m_pNotifyHackOnClose)
		m_pNotifyHackOnClose->FrameDeleted(this);
}

void CASW_VGUI_Frame::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	// allow creator to size and position us?
	int w = GetWide();

	vgui::HFont title_font = m_pTitleLabel->GetFont();
	int title_tall = vgui::surface()->GetFontTall(title_font);
	int title_bar_left_offset = 15.0f * m_fScale;
	m_iTitleBarHeight = title_tall * 1.2f;
	m_pCloseImage->SetShouldScaleImage(true);
	m_pCloseImage->SetSize(m_iTitleBarHeight, m_iTitleBarHeight);
	m_pCloseImage->SetPos(w - m_iTitleBarHeight, 0);
	m_pMiniImage->SetShouldScaleImage(true);
	m_pMiniImage->SetSize(m_iTitleBarHeight, m_iTitleBarHeight);
	m_pMiniImage->SetPos(w - (m_iTitleBarHeight * 2.1f), 0);
	
	int label_width = w - (title_bar_left_offset + title_tall);
	m_pTitleLabel->SetSize(label_width, title_tall);	// todo: width should size to contents to barright can be correct
	m_pTitleLabel->SetPos(title_bar_left_offset, 0);
}

void CASW_VGUI_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,153) );
	SetMouseInputEnabled(true);

	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );

	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(DefaultFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_west);	

	m_pCloseImage->SetDrawColor( Color(255,255,255,255) );
	m_pCloseImage->SetImage("swarm/Computer/WindowClose");
	m_bMouseOverCloseIcon = false;

	m_pMiniImage->SetDrawColor( Color(255,255,255,255) );
	m_pMiniImage->SetImage("swarm/Computer/WindowMinimise");
	m_bMouseOverMiniIcon = false;
}



void CASW_VGUI_Frame::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	if (m_pCloseImage->IsCursorOver() != m_bMouseOverCloseIcon)
	{
		m_bMouseOverCloseIcon = m_pCloseImage->IsCursorOver();
		if (m_bMouseOverCloseIcon)
		{
			m_pCloseImage->SetImage("swarm/Computer/WindowCloseLit");
		}
		else
		{
			m_pCloseImage->SetImage("swarm/Computer/WindowClose");
		}
	}
	if (m_pMiniImage->IsCursorOver() != m_bMouseOverMiniIcon)
	{
		m_bMouseOverMiniIcon = m_pMiniImage->IsCursorOver();
		if (m_bMouseOverMiniIcon)
		{
			m_pMiniImage->SetImage("swarm/Computer/WindowMinimiseLit");
		}
		else
		{
			m_pMiniImage->SetImage("swarm/Computer/WindowMinimise");
		}
	}
	
	if (!m_pCloseImage->IsCursorOver())
	{
		m_bMouseOverTitleBar = true;	// todo: grab the cursor x y relative to our window top and see if it's less than the title bar height?
	}
	else
	{
		m_bMouseOverTitleBar = false;
	}
	if (m_bDragging && !vgui::input()->IsMouseDown( MOUSE_LEFT ))
	{
		m_bDragging = false;
	}
	if (m_bDragging)
	{
		// set pos to x/y of cursor minus drag offset
		int current_posx, current_posy;
		vgui::input()->GetCursorPos(current_posx, current_posy);
		current_posx -= m_iDragOffsetX;
		current_posy -= m_iDragOffsetY;
		GetParent()->ScreenToLocal(current_posx, current_posy);
		SetPos(current_posx, current_posy);
	}
}

bool CASW_VGUI_Frame::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	//Msg("CASW_VGUI_Frame::MouseClick x=%d y=%d closeover=%d miniover=%d down=%d\n", x, y, m_pCloseImage->IsCursorOver(), m_pMiniImage->IsCursorOver(), bDown);
	if (m_pCloseImage->IsCursorOver() && !bDown)
	{
		// close it all down		
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
			pPlayer->StopUsing();
		return true;
	}
	if (m_pMiniImage->IsCursorOver() && !bDown)
	{
		// toggle minimized state
		m_bFrameMinimized = !m_bFrameMinimized;
		InvalidateLayout(true);
		return true;
	}
	if (!bDown && m_bDragging)
	{
		m_bDragging = false;		// stop dragging the window around
	}
	if (m_bMouseOverTitleBar)
	{		
		if (bDown && !m_bDragging)
		{
			m_bDragging = true;		// start dragging the window around
			// set drag offset (cursor x/y minus top left of frame)
			int wx, wy;
			wx = wy = 0;
			LocalToScreen(wx, wy);
			m_iDragOffsetX = x - wx;
			m_iDragOffsetY = y - wy;
		}
	}
	
	return true;
}