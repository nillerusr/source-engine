#include "cbase.h"
#include "asw_vgui_computer_stocks.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/TextImage.h"
#include "vgui/ILocalize.h"
#include "WrappedLabel.h"
#include <filesystem.h>
#include <keyvalues.h>
#include "controller_focus.h"
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Stocks::CASW_VGUI_Computer_Stocks( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = true;
	}

	m_bMouseOverBackButton = false;
	m_bSetAlpha = false;

	m_pBackButton = new ImageButton(this, "BackButton", "#asw_SynTekBackButton");
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->AddActionSignalTarget(this);
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "Back");
	m_pBackButton->SetCommand(msg->MakeCopy());
	m_pBackButton->SetCancelCommand(msg);
	m_pBackButton->SetAlpha(0);
	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekStocks");
	m_pTitleIcon = new vgui::ImagePanel(this, "TitleIcon");
	m_pTitleIconShadow = new vgui::ImagePanel(this, "TitleIconShadow");		

	for (int i=0;i<ASW_STOCK_ENTRIES;i++)
	{
		m_pSymbol[i] = new vgui::Label(this, "Symbol", "#asw_stock_symbol");
		m_pCorp[i] = new vgui::Label(this, "Corp", "#asw_stock_name");		
		m_pValue[i] = new vgui::Label(this, "Value", "#asw_stock_value");
		m_pChange[i] = new vgui::Label(this, "Change", "#asw_stock_change");
		m_pVolume[i] = new vgui::Label(this, "Volume", "#asw_stock_volume");
	}			

	SetStockLabels();

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}
}

CASW_VGUI_Computer_Stocks::~CASW_VGUI_Computer_Stocks()
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = false;
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pBackButton);
	}
}

void CASW_VGUI_Computer_Stocks::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);
	
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pTitleLabel->SetSize(w, h * 0.2f);	
	m_pTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pTitleLabel->SetZPos(160);	
	
	m_pTitleIcon->SetShouldScaleImage(true);
	int ix,iy,iw,it;
	ix = w*0.05f;//w*0.25f;
	iy = h*0.05f;//h*0.15f;
	iw = w*0.5f;
	it = h*0.5f;
	m_pTitleIcon->SetPos(ix,iy);
	m_pTitleIcon->SetSize(iw, it);	
	m_pTitleIcon->SetZPos(160);
		
	iw = it = 96 * m_fScale;
	m_pTitleIcon->SetShouldScaleImage(true);
	m_pTitleIcon->SetSize(iw,it);
	m_pTitleIcon->SetZPos(155);
	m_pTitleIconShadow->SetShouldScaleImage(true);
	m_pTitleIconShadow->SetSize(iw * 1.3f, it * 1.3f);
	//m_pTitleIconShadow[i]->SetAlpha(m_pMenuIcon[i]->GetAlpha()*0.5f);
	m_pTitleIconShadow->SetZPos(154);
	m_pTitleIconShadow->SetPos(ix - iw * 0.25f, iy + it * 0.0f);

	const float left_edge = 0.05f * w;
	//const float row_height = 0.05f * h;

	const int stock_top = h * 0.25f;
	const int symbol_width = 0.1f * w;
	const int corp_width = 0.36f * w;
	const int value_width = 0.12f * w;
	const int change_width = 0.12f * w;
	const int volume_width = 0.16f * w;
	const int stock_line_height = 0.04f * h;

	for (int i=0;i<ASW_STOCK_ENTRIES;i++)
	{
		int x = left_edge + 0.02f * w;
		int ypos = stock_top + stock_line_height * i;
		if (i > 0)
			ypos += stock_line_height * 0.5f;
		m_pSymbol[i]->SetBounds(x, ypos, symbol_width, stock_line_height);  x+= symbol_width;
		m_pCorp[i]->SetBounds(x, ypos, corp_width, stock_line_height);	  x+= corp_width;
		m_pValue[i]->SetBounds(x, ypos, value_width, stock_line_height);  x+= value_width;
		m_pChange[i]->SetBounds(x, ypos, change_width, stock_line_height);  x+= change_width;
		m_pVolume[i]->SetBounds(x, ypos, volume_width, stock_line_height);  x+= volume_width;

		//m_pValue[i]->SetContentAlignment(vgui::Label::a_northeast);
		//m_pChange[i]->SetContentAlignment(vgui::Label::a_northeast);
		//m_pVolume[i]->SetContentAlignment(vgui::Label::a_northeast);
	}
	
	// make sure all the labels expand to cover the new sizes	
	m_pTitleLabel->InvalidateLayout();
}


void CASW_VGUI_Computer_Stocks::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Stocks" );
}

void CASW_VGUI_Computer_Stocks::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	
	m_pBackButton->SetFont(LabelFont);
	m_pBackButton->SetPaintBackgroundEnabled(true);
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	m_pBackButton->SetColors(white, white, white, white, blue);
	m_pBackButton->SetPaintBackgroundType(2);
		
	vgui::HFont LargeTitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	//vgui::HFont TitleFont = pScheme->GetFont("Default");

	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(LargeTitleFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);			

	for (int i=0;i<ASW_STOCK_ENTRIES;i++)
	{		
		m_pSymbol[i]->SetFont(LabelFont);
		m_pCorp[i]->SetFont(LabelFont);
		m_pValue[i]->SetFont(LabelFont);
		m_pChange[i]->SetFont(LabelFont);
		m_pVolume[i]->SetFont(LabelFont);
		ApplySettingAndFadeLabelIn(m_pSymbol[i]);
		ApplySettingAndFadeLabelIn(m_pCorp[i]);
		ApplySettingAndFadeLabelIn(m_pValue[i]);
		ApplySettingAndFadeLabelIn(m_pChange[i]);
		ApplySettingAndFadeLabelIn(m_pVolume[i]);
	}

	m_pTitleIcon->SetImage("swarm/Computer/IconStocks");
	m_pTitleIconShadow->SetImage("swarm/Computer/IconStocks");
	
	// fade them in
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pBackButton->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		m_pTitleIcon->SetAlpha(0);
		m_pTitleIconShadow->SetAlpha(0);
		m_pTitleLabel->SetAlpha(0);			
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	}
}

void CASW_VGUI_Computer_Stocks::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{
	pLabel->SetFgColor(Color(255,255,255,255));
	pLabel->SetBgColor(Color(0,0,0,96));
	if (!m_bSetAlpha)
	{
		pLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}


void CASW_VGUI_Computer_Stocks::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);
	m_bMouseOverBackButton = false;

	m_bMouseOverBackButton = m_pBackButton->IsCursorOver();

	if (m_bMouseOverBackButton)
	{
		m_pBackButton->SetBgColor(Color(255,255,255,m_pBackButton->GetAlpha()));
		m_pBackButton->SetFgColor(Color(0,0,0,m_pBackButton->GetAlpha()));
	}
	else
	{
		m_pBackButton->SetBgColor(Color(19,20,40,m_pBackButton->GetAlpha()));
		m_pBackButton->SetFgColor(Color(255,255,255,m_pBackButton->GetAlpha()));
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

bool CASW_VGUI_Computer_Stocks::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (m_bMouseOverBackButton && !bDown)
	{
		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}		
		return true;
	}
	return true;
}

void CASW_VGUI_Computer_Stocks::OnCommand(char const* command)
{
	if (!Q_strcmp(command, "Back"))
	{
		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}
		return;
	}
	BaseClass::OnCommand(command);
}

void CASW_VGUI_Computer_Stocks::SetStockLabels()
{
	if (!m_pHackComputer)
		return;

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;
		
	const char* pszStocksSeed = STRING(pArea->m_StocksSeed);
	int iSeed = 0;
	if (Q_strlen(pszStocksSeed) > 0)
	{
		iSeed = atoi(pszStocksSeed);
	}
	m_NumberStream.SetSeed( iSeed );

	m_pSymbol[1]->SetText("CCT"); m_pCorp[1]->SetText( "#asw_so_centralcredit" );
	m_pSymbol[2]->SetText("DLI"); m_pCorp[2]->SetText( "#asw_so_dominic" );
	m_pSymbol[3]->SetText("ENIG"); m_pCorp[3]->SetText( "#asw_so_enigma" );
	m_pSymbol[4]->SetText("FRTR"); m_pCorp[4]->SetText( "#asw_so_frontier" );
	m_pSymbol[5]->SetText("NPRC"); m_pCorp[5]->SetText( "#asw_so_nanoproc" );
	m_pSymbol[6]->SetText("PARA"); m_pCorp[6]->SetText( "#asw_so_paradise" );
	m_pSymbol[7]->SetText("PHIG"); m_pCorp[7]->SetText( "#asw_so_phinvest" );
	m_pSymbol[8]->SetText("PPR"); m_pCorp[8]->SetText( "#asw_so_premier" );
	m_pSymbol[9]->SetText("SOL"); m_pCorp[9]->SetText( "#asw_so_solar" );
	m_pSymbol[10]->SetText("STEL"); m_pCorp[10]->SetText( "#asw_so_stellar" );
	m_pSymbol[11]->SetText("SYN"); m_pCorp[11]->SetText( "#asw_so_syntek" );
	m_pSymbol[12]->SetText("TELT"); m_pCorp[12]->SetText( "#asw_so_teltech" );
	m_pSymbol[13]->SetText("TRST"); m_pCorp[13]->SetText( "#asw_so_trust" );
	m_pSymbol[14]->SetText("UIND"); m_pCorp[14]->SetText( "#asw_so_united" );
	m_pSymbol[15]->SetText("ZRG"); m_pCorp[15]->SetText( "#asw_so_zerog" );

	for (int i=1;i<ASW_STOCK_ENTRIES;i++)
	{
		char buffer[64];
		float fCurrent = 50.0f;
		switch(i)
		{
		case 1: fCurrent = 44.0f; break;
		case 2: fCurrent = 14.0f; break;
		case 3: fCurrent = 96.0f; break;
		case 4: fCurrent = 36.0f; break;
		case 5: fCurrent = 10.0f; break;
		case 6: fCurrent = 46.0f; break;
		case 7: fCurrent = 7.0f; break;
		case 8: fCurrent = 19.0f; break;
		case 9: fCurrent = 30.0f; break;
		case 10: fCurrent = 15.0f; break;
		case 11: fCurrent = 150.0f; break;
		case 12: fCurrent = 12.0f; break;
		case 13: fCurrent = 54.0f; break;
		case 14: fCurrent = 60.0f; break;
		case 15: fCurrent = 18.0f; break;
		default: fCurrent = 50.0f; break;
		}
		fCurrent += (float) m_NumberStream.RandomInt(-500, 500) / 100.0f;
		Q_snprintf(buffer, sizeof(buffer), "%.2f", fCurrent);
		m_pValue[i]->SetText(buffer);
		;
		// set % change
		float fChange = (float) m_NumberStream.RandomInt(-400, 400) / 100.0f;
		if (fChange > 0)
			Q_snprintf(buffer, sizeof(buffer), "+%.2f", fChange);
		else
			Q_snprintf(buffer, sizeof(buffer), "%.2f", fChange);
		m_pChange[i]->SetText(buffer);

		// set volume
		int iVolume = m_NumberStream.RandomInt(25000000, 120000000);		
		Q_snprintf(buffer, sizeof(buffer), "%d", iVolume);
		m_pVolume[i]->SetText(buffer);
	}
}