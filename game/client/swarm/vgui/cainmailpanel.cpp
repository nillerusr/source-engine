#include "cbase.h"
#include "CainMailPanel.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "vgui/ISurface.h"
#include "WrappedLabel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CainMailPanel::CainMailPanel(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{
	char buffer[32];
	for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
	{
		Q_snprintf(buffer, sizeof(buffer), "#asw_cain_mail_%d", i);
		m_MailLines[i]  = new vgui::Label(this, "MailHeader", buffer);		
		m_MailLines[i]->SetAlpha(0);
	}
		
	m_fFadeOutTime = gpGlobals->curtime + 16.0f;
}

void CainMailPanel::PerformLayout()
{	
	SetBounds(0, 0, ScreenWidth(), ScreenHeight());
	
	int inset = ScreenWidth() * 0.1f;
	int line_height = ScreenHeight() * 0.035f;
	for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
	{
		m_MailLines[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_MailLines[i]->SetBounds(inset, ScreenHeight() * 0.1f + line_height * i,
								ScreenWidth() - (inset * 2), line_height);
	}
}

void CainMailPanel::StartFadeIn(vgui::IScheme *pScheme)
{
	//for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
	//{
		//m_MailLines[i]->SetFgColor(Color(255,255,255,255));
		//m_MailLines[i]->SetAlpha(0);
		//vgui::GetAnimationController()->RunAnimationCommand(m_MailLines[i], "Alpha", 255, 4.0f + 0.5f * i, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		//m_MailLines[i]->SetContentAlignment(vgui::Label::a_north);				
	//}
}

void CainMailPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	vgui::HFont font = pScheme->GetFont( "Default", IsProportional() );
	for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
	{
		m_MailLines[i]->SetFgColor(Color(255,255,255,255));
		m_MailLines[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_MailLines[i]->SetFont(font);
		m_MailLines[i]->SetAlpha(0);
		float fDelay = 0.0f;
		switch(i)
		{
		case 0: break;
		case 1: fDelay += 1.0f; break;
		case 2: fDelay += 2.0f; break;
		case 3: fDelay += 4.0f; break;	// lt cain
		case 4: fDelay += 6.0f; break;
		case 5: fDelay += 8.0f; break;
		case 6: fDelay += 10.0f; break; // eta
		case 7: fDelay += 14.0f; break; 
		default: fDelay += 15.0f; break;
		}
		vgui::GetAnimationController()->RunAnimationCommand(m_MailLines[i], "Alpha", 255, fDelay, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	}	
}

void CainMailPanel::OnThink()
{
	//PerformLayout();
	for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
	{
		//m_MailLines[i]->InvalidateLayout(true);
	}

	// check for adding a new credits message
	if (gpGlobals->curtime > m_fFadeOutTime)
	{
		if (m_fFadeOutTime != 0)
		{
			// start fading out
			for (int i=0;i<ASW_CAIN_MAIL_LINES;i++)
			{
				vgui::GetAnimationController()->RunAnimationCommand(m_MailLines[i], "Alpha", 0, 1.0f + i * 0.1f, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}			
			m_fFadeOutTime = 0;
		}
		else
		{
			// if we've faded out, close this panel
			if (m_MailLines[ASW_CAIN_MAIL_LINES-1]->GetAlpha() == 0)
			{
				SetVisible(false);
				MarkForDeletion();
			}
		}
	}	
}