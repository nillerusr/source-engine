#include "cbase.h"
#include "asw_vgui_computer_plant.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui/ILocalize.h"
#include "c_asw_objective.h"
#include "c_asw_game_resource.h"
#include "controller_focus.h"
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Plant::CASW_VGUI_Computer_Plant( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
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
	m_pPlantTitleLabel = new vgui::Label(this, "PlantTitleLabel", "#asw_SynTekPlantStatus");
	m_pPlantIcon = new vgui::ImagePanel(this, "PlantIcon");
	m_pPlantIconShadow = new vgui::ImagePanel(this, "PlantIconShadow");		
	m_pTemperatureNameLabel[0] = new vgui::Label(this, "SynTekReactorLabel", "#asw_SynTekReactorLabel");
	m_pTemperatureNameLabel[1] = new vgui::Label(this, "SynTekHeatExchangerLabel", "#asw_SynTekHeatExchangerLabel");
	m_pTemperatureNameLabel[2] = new vgui::Label(this, "SynTekCoolingTowerLabel", "#asw_SynTekCoolingTowerLabel");
	m_pTemperatureNameLabel[3] = new vgui::Label(this, "SynTekDrivingPistonsLabel", "#asw_SynTekDrivingPistonsLabel");
	m_pTemperatureNameLabel[4] = new vgui::Label(this, "SynTekTemperaturesLabel", "#asw_SynTekTemperaturesLabel");
	for (int i=0;i<4;i++)
	{
		m_pTemperatureValueLabel[i] = new vgui::Label(this, "SynTekTemperatureLabel", "");
	}
	m_pCoolantNameLabel[0] = new vgui::Label(this, "SynTekPrimaryCoolantLabel", "#asw_SynTekPrimaryCoolantLabel");
	m_pCoolantNameLabel[1] = new vgui::Label(this, "SynTekSecondaryCoolantLabel", "#asw_SynTekSecondaryCoolantLabel");
	m_pCoolantNameLabel[2] = new vgui::Label(this, "SynTekEmergencyCoolantLabel", "#asw_SynTekEmergencyCoolantLabel");
	for (int i=0;i<3;i++)
	{
		m_pCoolantValueLabel[i] = new vgui::Label(this, "SynTekCoolantValue", "");
	}
	m_pEnergyBusNameLabel = new vgui::Label(this, "SynTekEnergyBusNameLabel", "#asw_SynTekEnergyBusLabel");
	for (int i=0;i<10;i++)
	{
		m_pEnergyBusLabel[i] = new vgui::Label(this, "SynTekEnergyBusLabel", "");
	}
	m_pReactorStatusLabel = new vgui::Label(this, "SynTekReactorLabel", "#asw_SynTekReactorStatusLabel");
	m_pReactorOnlineLabel = new vgui::Label(this, "SynTekReactorLabel", "#asw_SynTekReactorOnlineLabel");
	m_pEnergyNameLabel = new vgui::Label(this, "SynTekEnergyNameLabel", "#asw_SynTekEnergyNameLabel");
	m_pEnergyValueLabel = new vgui::Label(this, "SynTekEnergyValueLabel", "");
	m_pEfficiencyNameLabel = new vgui::Label(this, "SynTekEfficiencyNameLabel", "#asw_SynTekEfficiencyNameLabel");
	m_pEfficiencyValueLabel = new vgui::Label(this, "SynTekEfficiencyValueLabel", "");
	m_pFlagLabel[0] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel0");
	m_pFlagLabel[1] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel1");
	m_pFlagLabel[2] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel2");
	m_pFlagLabel[3] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel3");
	m_pFlagLabel[4] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel4");
	m_pFlagLabel[5] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel5");
	m_pFlagLabel[6] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel6");
	m_pFlagLabel[7] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel7");
	m_pFlagLabel[8] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel8");
	m_pFlagLabel[9] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel9");
	m_pFlagLabel[10] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel10");
	m_pFlagLabel[11] = new vgui::Label(this, "SynTekFlagLabel", "#asw_SynTekFlagLabel11");

	m_bReactorOnline = GetReactorOnline();
	SetLabels();
	m_fNextScrollRawTableTime = gpGlobals->curtime;

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}
}

CASW_VGUI_Computer_Plant::~CASW_VGUI_Computer_Plant()
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = false;
	}

	/*
	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pBackButton);
	}
	*/
}

void CASW_VGUI_Computer_Plant::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);
	
	m_pPlantTitleLabel->SetSize(w, h * 0.2f);
	m_pPlantTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pPlantTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pPlantTitleLabel->SetZPos(160);
	
	m_pPlantIcon->SetShouldScaleImage(true);
	int ix,iy,iw,it;
	ix = w*0.05f;//w*0.25f;
	iy = h*0.05f;//h*0.15f;
	iw = w*0.5f;
	it = h*0.5f;
	m_pPlantIcon->SetPos(ix,iy);
	m_pPlantIcon->SetSize(iw, it);	
	m_pPlantIcon->SetZPos(160);
		
	iw = it = 96 * m_fScale;
	m_pPlantIcon->SetShouldScaleImage(true);
	m_pPlantIcon->SetSize(iw,it);
	m_pPlantIcon->SetZPos(155);
	m_pPlantIconShadow->SetShouldScaleImage(true);
	m_pPlantIconShadow->SetSize(iw * 1.3f, it * 1.3f);
	//m_pMenuIconShadow[i]->SetAlpha(m_pMenuIcon[i]->GetAlpha()*0.5f);
	m_pPlantIconShadow->SetZPos(154);
	m_pPlantIconShadow->SetPos(ix - iw * 0.25f, iy + it * 0.0f);

	const float left_edge = 0.05f * w;
	const float left_edge_inset = 0.065f * w;
	const float temperature_edge = 0.35f * w;
	const float temperature_top = 0.17f * h;
	const float coolant_top = 0.41f * h;
	const float row_height = 0.032f * h;
	const float energy_bus_top = 0.55f * h;
	const float default_width = 0.45f * w;
	const float default_value_width = 0.15f * w;
	for (int i=0;i<4;i++)
	{
		m_pTemperatureNameLabel[i]->SetPos(left_edge_inset, temperature_top + (i+2) * row_height);
		m_pTemperatureValueLabel[i]->SetPos(temperature_edge, temperature_top + (i+2) * row_height);
		m_pTemperatureNameLabel[i]->SetSize(default_width, row_height * 2);
		m_pTemperatureValueLabel[i]->SetSize(default_value_width, row_height * 2);
		m_pTemperatureNameLabel[i]->InvalidateLayout();
		m_pTemperatureValueLabel[i]->InvalidateLayout();
	}
	m_pTemperatureNameLabel[4]->SetPos(left_edge, temperature_top);
	m_pTemperatureNameLabel[4]->SetSize(default_width, row_height * 3);
	for (int i=0;i<3;i++)
	{
		m_pCoolantNameLabel[i]->SetPos(left_edge_inset, coolant_top + i * row_height);
		m_pCoolantValueLabel[i]->SetPos(temperature_edge, coolant_top + i * row_height);
		m_pCoolantNameLabel[i]->SetSize(default_width, row_height * 2);
		m_pCoolantValueLabel[i]->SetSize(default_value_width, row_height * 2);
		m_pCoolantNameLabel[i]->InvalidateLayout();
		m_pCoolantValueLabel[i]->InvalidateLayout();
	}
	m_pEnergyBusNameLabel->SetPos(left_edge, energy_bus_top);	
	m_pEnergyBusNameLabel->SetSize(default_width, row_height * 2);
	for (int i=0;i<10;i++)
	{
		m_pEnergyBusLabel[i]->SetPos(left_edge_inset, energy_bus_top + (i+1) * row_height);
		m_pEnergyBusLabel[i]->SetSize(default_width, row_height * 2);
		m_pEnergyBusLabel[i]->InvalidateLayout();
	}
	const float right_labels_edge = 0.6f * w;
	const float right_labels_width = 0.35f * w;
	const float efficiency_top = 0.4f * h;
	const float efficiency_number_edge = 0.85f * w;
	const float flags_top = 0.58f * h;
	m_pReactorStatusLabel->SetPos(right_labels_edge, 0.05f * h);
	m_pReactorStatusLabel->SetSize(right_labels_width, 0.1f * h);
	m_pReactorOnlineLabel->SetPos(right_labels_edge, 0.05f * h + row_height);
	m_pReactorOnlineLabel->SetSize(right_labels_width, 0.1f * h);
	m_pEnergyNameLabel->SetPos(right_labels_edge, temperature_top + row_height * 2);
	m_pEnergyValueLabel->SetPos(right_labels_edge, temperature_top + row_height * 3.5f);
	m_pEfficiencyNameLabel->SetPos(right_labels_edge, efficiency_top);
	m_pEfficiencyValueLabel->SetPos(efficiency_number_edge, efficiency_top);
	m_pEnergyNameLabel->SetSize(right_labels_width, 0.1f * h);
	m_pEnergyValueLabel->SetSize(right_labels_width, 0.1f * h);
	m_pEfficiencyNameLabel->SetSize(right_labels_width*0.5f, 0.15f * h);
	m_pEfficiencyValueLabel->SetSize(right_labels_width*0.5f, 0.15f * h);
	for (int i=0;i<6;i++)
	{
		m_pFlagLabel[i]->SetPos(0.45f * w, flags_top + i * row_height);
		m_pFlagLabel[i+6]->SetPos(0.67f * w, flags_top + i * row_height);
		m_pFlagLabel[i]->SetSize(default_width, row_height * 2);
		m_pFlagLabel[i+6]->SetSize(default_width, row_height * 2);
		m_pFlagLabel[i]->InvalidateLayout();
		m_pFlagLabel[i+6]->InvalidateLayout();
	}

	// make sure all the labels expand to cover the new sizes	 
	m_pBackButton->InvalidateLayout();
	m_pPlantTitleLabel->InvalidateLayout();
	m_pPlantIcon->InvalidateLayout();
	m_pPlantIconShadow->InvalidateLayout();		
	m_pEnergyBusNameLabel->InvalidateLayout();
	m_pReactorStatusLabel->InvalidateLayout();
	m_pReactorOnlineLabel->InvalidateLayout();
	m_pEnergyNameLabel->InvalidateLayout();
	m_pEnergyValueLabel->InvalidateLayout();
	m_pEfficiencyNameLabel->InvalidateLayout();
	m_pEfficiencyValueLabel->InvalidateLayout();
}


void CASW_VGUI_Computer_Plant::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.PlantStatus" );
}

void CASW_VGUI_Computer_Plant::ApplySchemeSettings(vgui::IScheme *pScheme)
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
	
	
	vgui::HFont RawFont = pScheme->GetFont( "Courier", IsProportional() );
	vgui::HFont LargeTitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	vgui::HFont TitleFont = pScheme->GetFont( "Default", IsProportional() );

	m_pPlantTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pPlantTitleLabel->SetFont(LargeTitleFont);
	m_pPlantTitleLabel->SetContentAlignment(vgui::Label::a_center);

	for (int i=0;i<4;i++)
	{
		m_pTemperatureNameLabel[i]->SetFont(LabelFont);
		m_pTemperatureValueLabel[i]->SetFont(LabelFont);
		m_pTemperatureValueLabel[i]->SetContentAlignment(vgui::Label::a_east);
		ApplySettingAndFadeLabelIn(m_pTemperatureNameLabel[i]);
		ApplySettingAndFadeLabelIn(m_pTemperatureValueLabel[i]);
	}
	m_pTemperatureNameLabel[4]->SetFont(TitleFont);
	ApplySettingAndFadeLabelIn(m_pTemperatureNameLabel[4]);
	for (int i=0;i<3;i++)
	{
		m_pCoolantNameLabel[i]->SetFont(LabelFont);
		m_pCoolantValueLabel[i]->SetFont(LabelFont);
		m_pCoolantValueLabel[i]->SetContentAlignment(vgui::Label::a_east);
		ApplySettingAndFadeLabelIn(m_pCoolantNameLabel[i]);
		ApplySettingAndFadeLabelIn(m_pCoolantValueLabel[i]);
	}
	m_pEnergyBusNameLabel->SetFont(LabelFont);
	ApplySettingAndFadeLabelIn(m_pEnergyBusNameLabel);
	for (int i=0;i<10;i++)
	{
		m_pEnergyBusLabel[i]->SetFont(RawFont);
		ApplySettingAndFadeLabelIn(m_pEnergyBusLabel[i]);
	}
	m_pReactorStatusLabel->SetFont(LargeTitleFont);
	m_pReactorOnlineLabel->SetFont(LargeTitleFont);	
	m_pReactorStatusLabel->SetContentAlignment(vgui::Label::a_center);
	m_pReactorOnlineLabel->SetContentAlignment(vgui::Label::a_center);
	m_pEnergyNameLabel->SetFont(TitleFont);
	m_pEnergyValueLabel->SetFont(TitleFont);
	m_pEnergyNameLabel->SetContentAlignment(vgui::Label::a_center);
	m_pEnergyValueLabel->SetContentAlignment(vgui::Label::a_center);
	m_pEfficiencyNameLabel->SetFont(TitleFont);
	m_pEfficiencyValueLabel->SetFont(LargeTitleFont);
	ApplySettingAndFadeLabelIn(m_pReactorStatusLabel);
	ApplySettingAndFadeLabelIn(m_pReactorOnlineLabel);
	m_pReactorStatusLabel->SetFgColor(Color(255,0,0,255));
	m_pReactorOnlineLabel->SetFgColor(Color(255,0,0,255));
	ApplySettingAndFadeLabelIn(m_pEnergyNameLabel);
	ApplySettingAndFadeLabelIn(m_pEnergyValueLabel);
	ApplySettingAndFadeLabelIn(m_pEfficiencyNameLabel);
	ApplySettingAndFadeLabelIn(m_pEfficiencyValueLabel);
	for (int i=0;i<12;i++)
	{
		m_pFlagLabel[i]->SetFont(LabelFont);
		ApplySettingAndFadeLabelIn(m_pFlagLabel[i]);
	}
	m_pPlantIcon->SetImage("swarm/Computer/IconCog");
	m_pPlantIconShadow->SetImage("swarm/Computer/IconCog");
	
	// fade them in
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pBackButton->SetAlpha(0);
		m_pPlantIcon->SetAlpha(0);
		m_pPlantIconShadow->SetAlpha(0);
		m_pPlantTitleLabel->SetAlpha(0);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
		vgui::GetAnimationController()->RunAnimationCommand(m_pPlantTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pPlantIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pPlantIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	}
}

void CASW_VGUI_Computer_Plant::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{
	pLabel->SetFgColor(Color(255,255,255,255));
	if (!m_bSetAlpha)
	{
		pLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}

void CASW_VGUI_Computer_Plant::SetLabels()
{
	if (m_bReactorOnline)
	{
		m_pReactorOnlineLabel->SetText("#asw_SynTekReactorOnlineLabel");
		m_pEfficiencyValueLabel->SetText("78%");
		m_pEnergyValueLabel->SetText("36,734,738 MW");
		m_pCoolantValueLabel[0]->SetText("74%");
		m_pCoolantValueLabel[1]->SetText("82%");
		m_pCoolantValueLabel[2]->SetText("99%");
		m_pTemperatureValueLabel[0]->SetText("1010344K");
		m_pTemperatureValueLabel[1]->SetText("936K");
		m_pTemperatureValueLabel[2]->SetText("210K");
		m_pTemperatureValueLabel[3]->SetText("298K");
		for (int i=0;i<10;i++)
		{
			m_pEnergyBusLabel[i]->SetText("");
		}
	}
	else
	{
		m_pReactorOnlineLabel->SetText("#asw_SynTekReactorOfflineLabel");
		m_pEfficiencyValueLabel->SetText("0%");
		m_pEnergyValueLabel->SetText("0 MW");
		m_pCoolantValueLabel[0]->SetText("74%");
		m_pCoolantValueLabel[1]->SetText("82%");
		m_pCoolantValueLabel[2]->SetText("99%");
		m_pTemperatureValueLabel[0]->SetText("295K");
		m_pTemperatureValueLabel[1]->SetText("295K");
		m_pTemperatureValueLabel[2]->SetText("110K");
		m_pTemperatureValueLabel[3]->SetText("295K");
		for (int i=0;i<10;i++)
		{
			m_pEnergyBusLabel[i]->SetText("");
		}
	}
}

void CASW_VGUI_Computer_Plant::ScrollRawTable()
{
	char buffer[255];
	// make a random string to go in the first slot
	if (m_bReactorOnline)
	{
		Q_snprintf(buffer, sizeof(buffer), "%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X",
			random->RandomInt(0, 255), random->RandomInt(0, 255), random->RandomInt(0, 255), random->RandomInt(0, 255), 
			random->RandomInt(0, 255), random->RandomInt(0, 255), random->RandomInt(0, 255), random->RandomInt(0, 255), 
			random->RandomInt(0, 255), random->RandomInt(0, 255));
	}
	else
	{
		Q_snprintf(buffer, sizeof(buffer), "%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X",
			0, 0, 0, 0, 
			0, 0, 0, 0, 
			0, 0);
	}
	m_pEnergyBusLabel[0]->SetText(buffer);
	for (int i=9;i>=1;i--)
	{
		m_pEnergyBusLabel[i-1]->GetText(buffer, 255);
		m_pEnergyBusLabel[i]->SetText(buffer);
	}
}

#define RAW_SCROLL_INTERVAL 0.3f

void CASW_VGUI_Computer_Plant::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);

	m_bMouseOverBackButton = false;


	m_bMouseOverBackButton = m_pBackButton->IsCursorOver();

	if (m_bMouseOverBackButton)
	{
		m_pBackButton->SetBgColor(Color(255,255,255,255));
		m_pBackButton->SetFgColor(Color(0,0,0,255));
	}
	else
	{
		m_pBackButton->SetBgColor(Color(19,20,40,255));
		m_pBackButton->SetFgColor(Color(255,255,255,255));
	}
	
	if (gpGlobals->curtime > m_fNextScrollRawTableTime)
	{
		ScrollRawTable();
		m_fNextScrollRawTableTime = gpGlobals->curtime + RAW_SCROLL_INTERVAL;
	}
	bool bReactorOnline = GetReactorOnline();
	if (bReactorOnline != m_bReactorOnline)
	{
		m_bReactorOnline = bReactorOnline;
		SetLabels();
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

bool CASW_VGUI_Computer_Plant::MouseClick(int x, int y, bool bRightClick, bool bDown)
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

bool CASW_VGUI_Computer_Plant::GetReactorOnline()
{
	if ( !ASWGameResource() )
		return false;
	
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	for (int i=0;i<12;i++)
	{
		C_ASW_Objective* pObjective = pGameResource->GetObjective(i);
		
		// if another objective isn't optional and isn't complete, then we're not ready to escape
		if (pObjective && pObjective->IsObjectiveComplete()
			&& !Q_strcmp(pObjective->GetObjectiveImage(), "ObReactor2"))
			//&& !Q_wcscmp(pObjective->GetObjectiveTitle(), L"Get the reactor online"))
		{
			return true;
		}
	}
	return false;
}

void CASW_VGUI_Computer_Plant::OnCommand(char const* command)
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