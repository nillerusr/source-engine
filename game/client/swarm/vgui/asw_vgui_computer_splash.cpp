#include "cbase.h"
#include "asw_vgui_computer_splash.h"
#include "asw_vgui_computer_frame.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui/ILocalize.h"
#include "c_asw_computer_area.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_COMPUTER_GLITCH_INTERVAL 0.01f

CASW_VGUI_Computer_Splash::CASW_VGUI_Computer_Splash( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	m_pLogoImage = new vgui::ImagePanel(this, "SplashImage");
	m_pLogoGlitchImage = new vgui::ImagePanel(this, "SplashImage");
	if (IsPDA())
	{
		m_pSynTekLabel = new vgui::Label(this, "SynTekHeader", "");
		m_pSloganLabel = new vgui::Label(this, "SloganLabel", g_pVGuiLocalize->Find("#asw_SynTekPDA"));
		if (m_pHackComputer && m_pHackComputer->GetComputerArea())
		{
			// set the label based on PDA name
			char namebuffer[64];
			Q_snprintf(namebuffer, sizeof(namebuffer), "%s", m_pHackComputer->GetComputerArea()->m_PDAName.Get());

			wchar_t wnamebuffer[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(namebuffer, wnamebuffer, sizeof( wnamebuffer ));
			
			wchar_t wbuffer[256];		
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
				g_pVGuiLocalize->Find("#asw_SynTekAccount"), 1,
					wnamebuffer);
			m_pSynTekLabel->SetText(wbuffer);
		}
		
	}
	else
	{
		m_pSynTekLabel = new vgui::Label(this, "SynTekHeader", g_pVGuiLocalize->Find("#asw_SynTekMegacorp"));
		m_pSloganLabel = new vgui::Label(this, "SloganLabel", "");
		switch (random->RandomInt(0,4))
		{
		case 0: m_pSloganLabel->SetText("#asw_SynTekSlogan1"); break;
		case 1: m_pSloganLabel->SetText("#asw_SynTekSlogan2"); break;
		case 2: m_pSloganLabel->SetText("#asw_SynTekSlogan3"); break;
		case 3: m_pSloganLabel->SetText("#asw_SynTekSlogan4"); break;
		default: m_pSloganLabel->SetText("#asw_SynTekSlogan5"); break;
		}
	}
	char buffer[1024];
	for (int i=0;i<ASW_SPLASH_SCROLL_LINES;i++)
	{
		m_pScrollLine[i] = new vgui::Label(this, "SplashScrollLine", "");
		Q_snprintf(buffer, sizeof(buffer), "#asw_SynTekScrollLine%d", i);
		m_pScrollLine[i]->SetText(buffer);
	}
	// todo: fill in line X with the marine's name
	m_fLastThinkTime = gpGlobals->curtime;
	m_fNextGlitchTime= gpGlobals->curtime;
	m_fShowGlitchTime = 0;
	m_fSoundTime = gpGlobals->curtime + 0.2f;
	m_bPlayedSound = false;
	m_fSlideOutTime = gpGlobals->curtime + 2.2f;
	//m_fSlideOutTime = gpGlobals->curtime + 0.2f;		// shortcut?
	m_bSlidOut = false;
	m_iNumLines = 0;
	m_fStartScrollingTime = gpGlobals->curtime + 0.7f;
	//m_fStartScrollingTime = gpGlobals->curtime + 0.1f;	// shortcut
	m_fFadeScrollerOutTime = 0;
	m_bSetAlpha = false;
}

CASW_VGUI_Computer_Splash::~CASW_VGUI_Computer_Splash()
{
	if (IsPDA())
		C_BaseEntity::StopSound( -1 /*SOUND_FROM_LOCAL_PLAYER*/, CHAN_STATIC, "ASWComputer.SyntekPDA" );
	else
		C_BaseEntity::StopSound( -1 /*SOUND_FROM_LOCAL_PLAYER*/, CHAN_STATIC, "ASWComputer.Startup" );
}

void CASW_VGUI_Computer_Splash::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(ScreenWidth()*0.5f - w*0.5f, ScreenHeight()*0.5f - h*0.5f);

	m_pLogoImage->SetShouldScaleImage(true);
	m_pLogoImage->SetPos(w*0.25f,h*0.15f);
	m_pLogoImage->SetSize(w*0.5f, h*0.5f);
	m_pLogoImage->SetImage("swarm/Computer/SynTekLogo2");
	m_pLogoImage->SetZPos(160);
	
	m_pLogoGlitchImage->SetShouldScaleImage(true);
	m_pLogoGlitchImage->SetPos(w*0.25f,h*0.25f);
	m_pLogoGlitchImage->SetSize(w*0.5f, h*0.5f);
	m_pLogoGlitchImage->SetImage("swarm/Computer/SynTekLogo2");
	m_pLogoGlitchImage->SetZPos(159);

	m_pSloganLabel->SetSize(w, h * 0.2f);
	m_pSloganLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSloganLabel->SetPos(0, h*0.75f);
	m_pSloganLabel->SetZPos(160);
	m_pSynTekLabel->SetSize(w, h * 0.2f);
	m_pSynTekLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSynTekLabel->SetPos(0, h*0.65f);
	m_pSynTekLabel->SetZPos(160);
	
	for (int i=0;i<ASW_SPLASH_SCROLL_LINES;i++)
	{		
		m_pScrollLine[i]->SetSize(w, h * 0.2f);
		m_pScrollLine[i]->SetContentAlignment(vgui::Label::a_northwest);
	}

	if (m_bSlidOut)
	{
		m_pLogoImage->SetBounds( w*0.05f, h*0.05f, w*0.13f, h*0.13f);
		m_pSynTekLabel->SetPos(0, 0);
	}

	SetZPos(161);
}

void CASW_VGUI_Computer_Splash::ASWInit()
{
	m_pLogoImage->SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "Alpha", 255, 0.3f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	m_pLogoGlitchImage->SetAlpha(0);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);
	
	m_pSloganLabel->SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(m_pSloganLabel, "Alpha", 255, 0.8f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);

	m_pSynTekLabel->SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(m_pSynTekLabel, "Alpha", 255, 0.3f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void CASW_VGUI_Computer_Splash::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	if (!m_bSetAlpha)
	{
		m_pLogoImage->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "Alpha", 255, 0.9f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		m_pLogoGlitchImage->SetAlpha(0);

		m_pSloganLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pSloganLabel, "Alpha", 255, 0.9f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);

		m_pSynTekLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pSynTekLabel, "Alpha", 255, 0.9f, 1.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}

	vgui::HFont SloganFont = pScheme->GetFont( "CleanHUD", IsProportional() );	
	m_pSloganLabel->SetFont(SloganFont);
	m_pSloganLabel->SetFgColor(Color(255,255,255,255));
	
	vgui::HFont HeaderFont = pScheme->GetFont( "DefaultLarge", IsProportional() );	
	m_pSynTekLabel->SetFont(HeaderFont);
	m_pSynTekLabel->SetFgColor(Color(255,255,255,255));

	vgui::HFont ScrollerFont = pScheme->GetFont( "Courier", IsProportional() );
	for (int i=0;i<ASW_SPLASH_SCROLL_LINES;i++)
	{
		m_pScrollLine[i]->SetFont(ScrollerFont);
		m_pScrollLine[i]->SetFgColor(Color(19,21,41,255));
		m_pScrollLine[i]->SetAlpha(0);	// it's okay for these to be hidden if player changes res and this gets called again
	}

	if (m_bSlidOut)
	{
		m_pSloganLabel->SetAlpha(0);
		m_pLogoGlitchImage->SetAlpha(0);
	}

	m_bSetAlpha = true;
}

bool CASW_VGUI_Computer_Splash::IsPDA()
{
	if (m_pHackComputer && m_pHackComputer->GetComputerArea())
		return m_pHackComputer->GetComputerArea()->IsPDA();

	return false;
}

#define ASW_SPLASH_SCROLL_INTERVAL 0.03f
void CASW_VGUI_Computer_Splash::OnThink()
{	
	if (!m_bPlayedSound && m_fSoundTime <= gpGlobals->curtime)
	{
		CLocalPlayerFilter filter;
		if (IsPDA())
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.SyntekPDA" );
		else
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Startup" );
		m_bPlayedSound = true;
	}

	int x,y,w,t;
	GetBounds(x,y,w,t);	
	if (!m_bSlidOut && m_fSlideOutTime <= gpGlobals->curtime)
	{
		m_bSlidOut = true;
		// slide/fade out our elements		
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "xpos", w*0.05f, 0.0f, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "ypos", t*0.05f, 0.0f, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "wide", w*0.13f, 0.0f, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pLogoImage, "tall", t*0.13f, 0.0f, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pSynTekLabel, "ypos", 0, 0.0f, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pSloganLabel, "Alpha", 0, 0.0f, 0.4f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}

	if (m_iNumLines < ASW_SPLASH_SCROLL_LINES && m_fStartScrollingTime <= gpGlobals->curtime)
	{
		m_iNumLines++;
		//m_iNumLines = clamp(((gpGlobals->curtime - m_fStartScrollingTime) / ASW_SPLASH_SCROLL_INTERVAL) + 1, 0, ASW_SPLASH_SCROLL_LINES);
		if (m_iNumLines == ASW_SPLASH_SCROLL_LINES)
			m_fFadeScrollerOutTime = gpGlobals->curtime + 0.8f;
		float font_tall = vgui::surface()->GetFontTall(m_pScrollLine[0]->GetFont());
		for (int i=0;i<m_iNumLines;i++)
		{
			m_pScrollLine[i]->SetAlpha(128);			
			m_pScrollLine[i]->SetPos(0.02f * w, t - ((m_iNumLines - i) * font_tall + 0.02f * w));
		}
		m_fStartScrollingTime = gpGlobals->curtime + ASW_SPLASH_SCROLL_INTERVAL * (random->RandomInt(1,5));
	}
	if (m_fFadeScrollerOutTime != 0 && gpGlobals->curtime >= m_fFadeScrollerOutTime)
	{
		m_fFadeScrollerOutTime = 0;
		for (int i=0;i<ASW_SPLASH_SCROLL_LINES;i++)
		{
			vgui::GetAnimationController()->RunAnimationCommand(m_pScrollLine[i], "Alpha", 0, 0.0f, 0.4f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		// notify the main frame we're done
		CASW_VGUI_Computer_Frame *pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
		if (pFrame && !IsPDA())
			pFrame->SplashFinished();
	}

	m_pLogoImage->GetBounds(x,y,w,t);
	// make our glitch less bright
	if (m_bSlidOut || gpGlobals->curtime > m_fShowGlitchTime)
	{
		m_pLogoGlitchImage->SetAlpha(0);

		if (gpGlobals->curtime > m_fNextGlitchTime)
		{
			if (random->RandomFloat() < 0.03f)
				m_fShowGlitchTime = gpGlobals->curtime + 0.4f;
			m_fNextGlitchTime += ASW_COMPUTER_GLITCH_INTERVAL;
		}
	}
	else
	{
		m_pLogoGlitchImage->SetAlpha(m_pLogoImage->GetAlpha() * 0.07f);
		// scale it randomly
		if (gpGlobals->curtime > m_fNextGlitchTime)
		{
			float x_scale = random->RandomFloat(0.7f, 1.3f);
			float y_scale = random->RandomFloat(0.7f, 1.3f);
			m_pLogoGlitchImage->SetSize(w * x_scale, t * y_scale);
			m_pLogoGlitchImage->SetPos(x - w * 0.5f * x_scale + w * 0.5f, y - t * 0.5f * y_scale + t * 0.5f);
			m_fNextGlitchTime += ASW_COMPUTER_GLITCH_INTERVAL;
		}
	}

	//float deltatime = gpGlobals->curtime - m_fLastThinkTime;
	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_VGUI_Computer_Splash::SetHidden(bool bHidden)
{
	if (bHidden && GetAlpha() > 0)
	{
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, 0.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	else if (!bHidden && GetAlpha() <= 0)
	{
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0, 0.7f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}