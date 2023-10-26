#include "cbase.h"

#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextImage.h"
#include "WrappedLabel.h"
#include "c_asw_game_resource.h"
#include "ObjectiveTitlePanel.h"
#include "c_asw_player.h"
#include "c_asw_objective.h"
#include "vgui_controls/AnimationController.h"
#include <vgui/ISurface.h>
#include "ObjectiveListBox.h"
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ObjectiveTitlePanel::ObjectiveTitlePanel(Panel *parent, const char *name) : Panel(parent, name)
{	
	m_pListBox = dynamic_cast<ObjectiveListBox*>(parent);

	m_pCheckbox = new vgui::ImagePanel( this, "Checkbox" );
	m_pCheckbox->SetShouldScaleImage( true );
	m_pCheckbox->SetMouseInputEnabled( false );

	// create a blank imagepanel for the objective pic
	m_ObjectiveImagePanel = new vgui::ImagePanel(this, "ObjectiveTitlePanelImage");
	m_ObjectiveImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
	m_ObjectiveImagePanel->SetShouldScaleImage(true);
	m_ObjectiveImagePanel->SetMouseInputEnabled(false);

	// create the blank objective text - note, label isn't actually a child of this class!
	wchar_t *text = L"<objectivetitle>";
	m_ObjectiveLabel = new vgui::Label(this, "ObjectiveTitlePanelLabel", text);
	m_ObjectiveLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_ObjectiveLabel->SetMouseInputEnabled(false);

	m_bObjectiveSelected = false;
	m_szImageName[0] = NULL;	
	m_hFont = m_hSmallFont = vgui::INVALID_FONT;

}
	
ObjectiveTitlePanel::~ObjectiveTitlePanel()
{
	if (GetControllerFocus())
		GetControllerFocus()->RemoveFromFocusList(this);
}

// a single panel showing a marine
void ObjectiveTitlePanel::PerformLayout()
{
	int height = ScreenHeight();	
	int iLineTall = vgui::surface()->GetFontTall(m_hFont);
	int iPadding = 7.0f * (ScreenHeight() / 768.0f);	// 10 pixels of padding

	m_pCheckbox->SetBounds( YRES( 3 ), YRES( 3 ), YRES( 10 ), YRES( 10 ) ); 

	m_ObjectiveLabel->SetSize( YRES( 140 ), iLineTall);
	m_ObjectiveLabel->SetPos( YRES( 15 ), iPadding);
	int image_high = height * 0.23;
	int image_wide = image_high * (1.25f);	// 4:3 ratio
	//if (image_wide > width * 0.23)
		//image_wide = width * 0.23;
	m_ObjectiveImagePanel->SetSize(image_wide, image_high);
	m_ObjectiveImagePanel->SetPos( YRES( 80 ) - (image_wide * 0.5f), iLineTall + iPadding * 2);

	int text_wide = 0;
	int text_tall = 0;
	wchar_t buffer[256];
	m_ObjectiveLabel->GetText(buffer, sizeof(buffer));
	vgui::surface()->GetTextSize(m_hFont, buffer, text_wide, text_tall);
	//Msg("PL %d text_wide = %d label wide = %f\n", m_Index, text_wide, m_ObjectiveLabel->GetWide());
	//if (text_wide > m_ObjectiveLabel->GetWide())
		//m_ObjectiveLabel->SetFont(m_hSmallFont);
	//else
		m_ObjectiveLabel->SetFont(m_hFont);
	m_ObjectiveLabel->SetText(buffer);
	m_ObjectiveLabel->InvalidateLayout(true);
}


// sets this roster entry to show the picture/name of the specified profile
void ObjectiveTitlePanel::SetObjective(C_ASW_Objective* pObjective)
{
	C_ASW_Objective* pCur = m_hObjective.Get();
	if (pCur != pObjective && pObjective)
	{
		if (GetControllerFocus())
			GetControllerFocus()->AddToFocusList(this, true);
	}
	m_hObjective = pObjective;		
	UpdateElements();
}

void ObjectiveTitlePanel::OnThink()
{
	BaseClass::OnThink();

	if (IsCursorOver() || m_bObjectiveSelected)
		m_ObjectiveLabel->SetFgColor(Color(255,255,255,255));
	else
		m_ObjectiveLabel->SetFgColor(m_BlueColor);

	// check for setcting the text or image
	if (m_hObjective.Get())
	{
		UpdateElements();	
	}
}

void ObjectiveTitlePanel::UpdateElements()
{
	if (!m_hObjective.Get())
		return;
	wchar_t buffer[256];
	wchar_t buffer2[256];
	const wchar_t* pTitle = buffer2;

	if ( m_hObjective->IsObjectiveDummy() )
	{
		m_pCheckbox->SetVisible( false );
	}
	else
	{
		m_pCheckbox->SetVisible( true );
		if ( m_hObjective->IsObjectiveComplete() )
		{
			m_pCheckbox->SetImage("swarm/HUD/TickBoxTicked");
		}
		else
		{
			m_pCheckbox->SetImage("swarm/HUD/TickBoxEmpty");
		}
	}
	m_ObjectiveLabel->GetText(buffer, 255);
	if (m_hObjective->IsObjectiveDummy())
		pTitle = m_hObjective->GetObjectiveTitle();		
	else
	{
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", m_Index);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));
		
		wchar_t*pLocal = g_pVGuiLocalize->Find("#asw_objective_titlef");
		if (!pLocal)
			pLocal = L"";
		g_pVGuiLocalize->ConstructString( buffer2, sizeof(buffer2),
			pLocal, 1,
				m_hObjective->GetObjectiveTitle());		
	}
	if (Q_wcscmp(buffer, pTitle))
	{
		int text_wide = 0;
		int text_tall = 0;
		vgui::surface()->GetTextSize(m_hFont, pTitle, text_wide, text_tall);
		//Msg("%d text_wide = %d label wide = %f\n", m_Index, text_wide, m_ObjectiveLabel->GetWide());
		//if (text_wide > m_ObjectiveLabel->GetWide())
			//m_ObjectiveLabel->SetFont(m_hSmallFont);
		//else
			m_ObjectiveLabel->SetFont(m_hFont);
		m_ObjectiveLabel->SetText(pTitle);
		m_ObjectiveLabel->InvalidateLayout(true);		
	}
	char sbuffer[256];
	if (Q_strlen(m_hObjective->GetObjectiveImage())<1)
	{		
		Q_snprintf(sbuffer, sizeof(sbuffer), "swarm/MissionPics/UnknownMissionPic");
	}
	else
	{
		Q_snprintf(sbuffer, sizeof(sbuffer), "swarm/ObjectivePics/%s", m_hObjective->GetObjectiveImage());
	}
	if (Q_strcmp(sbuffer, m_szImageName))
	{
		Q_snprintf(m_szImageName, sizeof(m_szImageName), "%s", sbuffer);
		m_ObjectiveImagePanel->SetImage(m_szImageName);
	}
}

void ObjectiveTitlePanel::OnMouseReleased(vgui::MouseCode code)
{
	//Msg("Clicked on %s\n", m_Profile->m_ShortName);
	BaseClass::OnMouseReleased(code);

	if ( code != MOUSE_LEFT )
		return;
	
	if (m_pListBox)
		m_pListBox->ClickedTitle(this);	
}

void ObjectiveTitlePanel::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	
	// todo: highlight
}

void ObjectiveTitlePanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_BlueColor = pScheme->GetColor("LightBlue", Color(66,142,192,255));
	m_ObjectiveLabel->SetFgColor(m_BlueColor);	
	m_hFont = pScheme->GetFont( "Default", IsProportional() );
	m_hSmallFont = pScheme->GetFont( "DefaultSmall", IsProportional() );
	m_ObjectiveLabel->SetFont(m_hFont);	// temp was normal font
	SetBgColor(Color(30,66,89,164));

	int iLineTall = vgui::surface()->GetFontTall(m_hFont);	
	int iPadding = 7.0f * (ScreenHeight() / 768.0f);	// 10 pixels of padding
	SetSize( YRES( 160 ), iLineTall + iPadding * 2);
}

void ObjectiveTitlePanel::SetSelected(bool bSelected)
{
	m_bObjectiveSelected = bSelected;
	float fDuration = 0.3f;
	int iLineTall = vgui::surface()->GetFontTall(m_hFont);
	int iPadding = 7.0f * (ScreenHeight() / 768.0f);	// 10 pixels of padding
	if (bSelected)
	{
		int h = iLineTall + iPadding * 3 + ScreenHeight() * 0.23;
		vgui::GetAnimationController()->RunAnimationCommand(this, "tall", h, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.ObjectiveSlide" );		
	}
	else
	{		
		vgui::GetAnimationController()->RunAnimationCommand(this, "tall", iLineTall + iPadding * 2, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	}	
}