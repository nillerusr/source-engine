#include "cbase.h"
#include "asw_vgui_computer_weather.h"
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

CASW_VGUI_Computer_Weather::CASW_VGUI_Computer_Weather( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = true;
	}

	m_pBackButton = new ImageButton(this, "BackButton", "#asw_SynTekBackButton");
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->AddActionSignalTarget(this);
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "Back");
	m_pBackButton->SetCommand(msg->MakeCopy());
	m_pBackButton->SetCancelCommand(msg);
	m_pBackButton->SetAlpha(0);
	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekAtmospherics");
	m_pTitleIcon = new vgui::ImagePanel(this, "TitleIcon");
	m_pTitleIconShadow = new vgui::ImagePanel(this, "TitleIconShadow");		
	m_pWarningLabel = new vgui::WrappedLabel(this, "Warning", "#asw_weather_warning");	

	for (int i=0;i<ASW_WEATHER_ENTRIES;i++)
	{
		m_pWeatherLabel[i] = new vgui::Label(this, "WLabel", "");
		m_pWeatherValue[i] = new vgui::Label(this, "WValue", "");				
	}			

	SetWeatherLabels();

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}

	m_bSetAlpha = false;
}

CASW_VGUI_Computer_Weather::~CASW_VGUI_Computer_Weather()
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

void CASW_VGUI_Computer_Weather::PerformLayout()
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
	
	//const float row_height = 0.05f * h;

	const int weather_top = h * 0.28f;
	const int label_width = h * 0.4f;
	const int value_width = 0.4f * w;	
	const int weather_line_height = 0.05f * h;

	const float left_edge = 0.5 * w - ((label_width + value_width) * 0.5f);

	for (int i=0;i<ASW_WEATHER_ENTRIES;i++)
	{
		int x = left_edge;// + 0.02f * w;
		int ypos = weather_top + weather_line_height * i;
		//if (i > 0)
			//ypos += weather_line_height * 0.5f;
		m_pWeatherLabel[i]->SetBounds(x, ypos, label_width, weather_line_height);  x+= label_width;
		m_pWeatherValue[i]->SetBounds(x, ypos, value_width, weather_line_height);	  x+= value_width;
	}

	const int warning_width = 0.8f * w;
	const int warning_edge = 0.5f * w - (warning_width * 0.5f);

	m_pWarningLabel->GetTextImage()->SetDrawWidth(warning_width);
	int texwide, texttall;		
	m_pWarningLabel->GetTextImage()->GetContentSize(texwide, texttall);
	m_pWarningLabel->SetSize(texwide, texttall);
	m_pWarningLabel->SetPos(warning_edge, weather_top + weather_line_height * (ASW_WEATHER_ENTRIES + 1));
	
	// make sure all the labels expand to cover the new sizes	
	m_pTitleLabel->InvalidateLayout();
}

void CASW_VGUI_Computer_Weather::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.AtmosphericReport" );
}

void CASW_VGUI_Computer_Weather::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	
	m_pWarningLabel->SetFont(LabelFont);
	m_pBackButton->SetPaintBackgroundEnabled(true);

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

	for (int i=0;i<ASW_WEATHER_ENTRIES;i++)
	{		
		m_pWeatherLabel[i]->SetFont(LabelFont);
		m_pWeatherValue[i]->SetFont(LabelFont);		
		ApplySettingAndFadeLabelIn(m_pWeatherLabel[i]);
		ApplySettingAndFadeLabelIn(m_pWeatherValue[i]);		
	}

	ApplySettingAndFadeLabelIn(m_pWarningLabel);
	
	m_pTitleIcon->SetImage("swarm/Computer/IconWeather");
	m_pTitleIconShadow->SetImage("swarm/Computer/IconWeather");

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

void CASW_VGUI_Computer_Weather::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{
	pLabel->SetFgColor(Color(255,255,255,255));
	pLabel->SetBgColor(Color(0,0,0,96));
	if (!m_bSetAlpha)
	{
		pLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}


void CASW_VGUI_Computer_Weather::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);
	SetPos(0,0);

	if (m_pBackButton->IsCursorOver())
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

bool CASW_VGUI_Computer_Weather::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (m_pBackButton->IsCursorOver() && !bDown)
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

void CASW_VGUI_Computer_Weather::OnCommand(char const* command)
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

void CASW_VGUI_Computer_Weather::SetWeatherLabels()
{
	if (!m_pHackComputer)
		return;

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;
		
	const char* pszWeatherSeed = STRING(pArea->m_WeatherSeed);
	int iSeed = 0;
	if (Q_strlen(pszWeatherSeed) > 0)
	{
		iSeed = atoi(pszWeatherSeed);
	}
	m_NumberStream.SetSeed( iSeed );

	m_pWeatherLabel[0]->SetText("#asw_weather_temp");
	m_pWeatherLabel[1]->SetText("#asw_weather_wind");
	m_pWeatherLabel[2]->SetText("#asw_weather_pressure");
	m_pWeatherLabel[3]->SetText("#asw_weather_humidity");
	m_pWeatherLabel[4]->SetText("#asw_weather_cloud");
	m_pWeatherLabel[5]->SetText("#asw_weather_vis");
	m_pWeatherLabel[6]->SetText("#asw_weather_weather");

	float fTemp = -6 -((float) random->RandomInt(0, 140) / 10.0f) + 273.15;
	float fWindSpeed = (float) m_NumberStream.RandomInt(0, 200) / 10.0f;
	int iPressure = m_NumberStream.RandomInt(1000, 1030);
	float fHumidity = (float) m_NumberStream.RandomInt(6000, 9000) / 100.0f;
	float fVis = (float) m_NumberStream.RandomInt(10, 90) / 10.0f;
	bool bModerateSnow = (m_NumberStream.RandomInt(0, 10) <= 4);
	bool bFewClouds = (m_NumberStream.RandomInt(0, 10) <= 5);
	int iCloudHeight = m_NumberStream.RandomInt(450, 2000);

	SetFloatLabel(m_pWeatherValue[0], "#asw_weather_temp_r", fTemp);
	SetFloatLabel(m_pWeatherValue[1], "#asw_weather_wind_r", fWindSpeed);
	SetIntLabel(m_pWeatherValue[2], "#asw_weather_pressure_r", iPressure);
	SetFloatLabel(m_pWeatherValue[3], "#asw_weather_humidity_r", fHumidity);
	if (bFewClouds)
		SetIntLabel(m_pWeatherValue[4], "#asw_weather_few_cloud_r", iCloudHeight);
	else
		SetIntLabel(m_pWeatherValue[4], "#asw_weather_broken_cloud_r", iCloudHeight);	
	SetFloatLabel(m_pWeatherValue[5], "#asw_weather_vis_r", fVis);
	if (bModerateSnow)
		m_pWeatherValue[6]->SetText("#asw_weather_weather_light");
	else
		m_pWeatherValue[6]->SetText("#asw_weather_weather_moderate");	
}

void CASW_VGUI_Computer_Weather::SetFloatLabel(vgui::Label *pLabel, const char *string, float fValue)
{
	char buffer[16];
	Q_snprintf(buffer, sizeof(buffer), "%.2f", fValue);

	wchar_t wnumber[16];
	g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

	wchar_t wbuffer[96];		
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find(string), 1,
			wnumber);

	pLabel->SetText(wbuffer);
}

void CASW_VGUI_Computer_Weather::SetIntLabel(vgui::Label *pLabel, const char *string, int iValue)
{
	char buffer[16];
	Q_snprintf(buffer, sizeof(buffer), "%d", iValue);

	wchar_t wnumber[16];
	g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

	wchar_t wbuffer[96];		
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find(string), 1,
			wnumber);

	pLabel->SetText(wbuffer);
}