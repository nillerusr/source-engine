#include "cbase.h"
#include "CreditsPanel.h"
#include "asw_gamerules.h"
#include "asw_campaign_info.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "vgui/ISurface.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CreditsPanel::CreditsPanel(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{
	m_pLogo = new vgui::ImagePanel(this, "Logo");
	m_pLogo->SetImage("../console/valve_logo");
	m_pLogo->SetShouldScaleImage(true);
	m_pLogo->SetAlpha(0);

	m_fNextMessage = gpGlobals->curtime + ASW_MESSAGE_INTERVAL;
	m_iCurMessage = 0;
	m_Font = vgui::INVALID_FONT;
	m_bShowCreditsLogo = true;

	for (int i=0;i<ASW_LABEL_POOL_SIZE;i++)
	{
		m_LabelPool[i] = new vgui::Label(this, "CreditLine", "");
		m_LabelPool[i]->SetContentAlignment(vgui::Label::a_north);
		m_LabelPool[i]->SetAlpha(0);
	}

	char szCreditsPath[512];
	CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();
	if ( pCampaign )
		Q_snprintf(szCreditsPath,sizeof(szCreditsPath), "%s.txt", STRING(pCampaign->m_CustomCreditsFile));
	else
		Q_snprintf(szCreditsPath,sizeof(szCreditsPath), "scripts/asw_credits.txt");
		

	pCreditsText = new KeyValues( "Credits" );
	pCreditsText->LoadFromFile( filesystem, szCreditsPath, "GAME" );

	pCurrentCredit = pCreditsText->GetFirstSubKey();

	char const *keyname = pCurrentCredit->GetName();
	if ( !Q_stricmp( keyname, "show_credits_logo" ) )
	{
		m_bShowCreditsLogo = pCurrentCredit->GetBool();
		pCurrentCredit = pCurrentCredit->GetNextKey();
	}

	char const *keyname2 = pCurrentCredit->GetName();
	if ( !Q_stricmp( keyname2, "custom_logo" ) )
	{
		string_t CustomLogoFile = MAKE_STRING( pCreditsText->GetString( "custom_logo", "../console/valve_logo" ) );
		m_pLogo->SetImage( CustomLogoFile );
		pCurrentCredit = pCurrentCredit->GetNextKey();
	}
}

CreditsPanel::~CreditsPanel()
{
	pCreditsText->deleteThis();
}

void CreditsPanel::PerformLayout()
{
	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall() - (GetParent()->GetTall()*0.11f);
	SetBounds(0, h * 0.09f, w, h );
	float LogoWidth = w * 0.7f;
	m_pLogo->SetBounds( ( w - LogoWidth ) * 0.5f, LogoWidth*0.065f, LogoWidth, LogoWidth * 0.25f);

	//m_hASWLogo->SetBounds((wide - LogoWidth) * 0.5f,tall * 0.12f, LogoWidth, LogoWidth * 0.25f);
}

void CreditsPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_Font = pScheme->GetFont( "Default", IsProportional() );
	if ( m_bShowCreditsLogo )
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogo, "Alpha", 255, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);

	for (int i=0;i<ASW_LABEL_POOL_SIZE;i++)
	{
		m_LabelPool[i]->SetContentAlignment(vgui::Label::a_north);		
		m_LabelPool[i]->SetFont(m_Font);
		m_LabelPool[i]->SetFgColor(Color(255,255,255,255));
		//m_LabelPool[i]->SetAlpha(0);
	}
}

void CreditsPanel::OnThink()
{
	// check for adding a new credits message
	if ( gpGlobals->curtime > m_fNextMessage && pCurrentCredit )
	{
		vgui::Label *pLabel = m_LabelPool[m_iCurMessage % ASW_LABEL_POOL_SIZE];
		
		const unsigned char *pchCurrentChar = static_cast< const unsigned char * >( static_cast< const void * >( pCurrentCredit->GetString() ) );
		wchar_t msg[ 128 ];
		int nCharNumber = 0;

		while ( *pchCurrentChar && nCharNumber < 127 )
		{
			msg[ nCharNumber ] = *pchCurrentChar;
			pchCurrentChar++;
			nCharNumber++;
		}

		msg[ nCharNumber ] = L'\0';

		pLabel->SetText(msg);
		pLabel->SetAlpha(0);
		
		int t = vgui::surface()->GetFontTall(m_Font);
		pLabel->SetPos(0, GetTall() - t);
		
		pLabel->SetSize(GetWide(), t * 1.5f);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "ypos", 0, 0, 10.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		m_Scrolling.AddToTail(pLabel);
		m_fNextMessage = gpGlobals->curtime + ASW_MESSAGE_INTERVAL;
		m_iCurMessage++;
		pCurrentCredit = pCurrentCredit->GetNextKey();
	}
	// check for fading out panels that have reached the top area of the screen
	if (m_Scrolling.Count() > 0)
	{
		int x, y;
		vgui::Label* pLabel = m_Scrolling[0];
		pLabel->GetPos(x, y);
		if (y <= GetTall() * 0.5f)
		{
			if (y <= GetTall() * 0.25f)	// remove it immediately if it's too far up
			{
				m_Scrolling.Remove(0);
				m_FadingOut.AddToTail(pLabel);
				pLabel->SetAlpha(0);
			}
			else
			{
				m_Scrolling.Remove(0);
				m_FadingOut.AddToTail(pLabel);
				vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 0, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
			}
		}		
	}
	// check for removing panels that have faded out
	if (m_FadingOut.Count() > 0)
	{
		vgui::Label* pLabel = m_FadingOut[0];
		if (pLabel->GetAlpha() <= 0)
		{
			m_FadingOut.Remove(0);
			//pLabel->SetVisible(false);
			//pLabel->MarkForDeletion();
		}
	}
}