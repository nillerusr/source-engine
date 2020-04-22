//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_hud_achievement_announce.h"
#include <vgui/IVGui.h>
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "fmtstr.h"
#include "cs_gamerules.h"
#include "view.h"
#include "ivieweffects.h"
#include "viewrender.h"
#include "usermessages.h"
#include "hud_macros.h"
#include "c_baseanimating.h"
#include "achievementmgr.h"
#include "filesystem.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CCSAchievementAnnouncePanel, 1 );

#define CALLOUT_WIDE		(XRES(100))
#define CALLOUT_TALL		(XRES(50))


namespace Interpolators

{
    inline float Linear( float t ) { return t; }

    inline float SmoothStep( float t )
    {
        t = 3 * t * t - 2.0f * t * t * t;
        return t;
    }

    inline float SmoothStep2( float t )
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    inline float SmoothStepStart( float t )
    {
        t = 0.5f * t;
        t = 3 * t * t - 2.0f * t * t * t;
        t = t* 2.0f;
        return t;
    }

    inline float SmoothStepEnd( float t )
    {
        t = 0.5f * t + 0.5f;
        t = 3 * t * t - 2.0f * t * t * t;
        t = (t - 0.5f) * 2.0f;
        return t;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSAchievementAnnouncePanel::CCSAchievementAnnouncePanel( const char *pElementName )
: EditablePanel( NULL, "AchievementAnnouncePanel" ), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	m_bShouldBeVisible = false;
	SetScheme( "ClientScheme" );
    m_currentDisplayedAchievement = CSInvalidAchievement;
    m_pGlowPanel = NULL;

	RegisterForRenderGroup( "hide_for_scoreboard" );

    //Create the grey popup that has the name, text and icon.
    m_pAchievementInfoPanel = new CCSAchivementInfoPanel(this, "InfoPanel");
}

CCSAchievementAnnouncePanel::~CCSAchievementAnnouncePanel()
{
    if (m_pAchievementInfoPanel)
    {
        delete m_pAchievementInfoPanel;
    }
}
 

void CCSAchievementAnnouncePanel::Reset()
{
	//We don't want to hide the UI here since we want the achievement popup to show through death
}

void CCSAchievementAnnouncePanel::Init()
{
	ListenForGameEvent( "achievement_earned_local" );	

	Hide();

	CHudElement::Init();
}

void CCSAchievementAnnouncePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
    
    LoadControlSettings( "Resource/UI/Achievement_Glow.res" );

    //This is the glowy flash that comes up under the popup
    m_pGlowPanel = dynamic_cast<EditablePanel *>( FindChildByName("GlowPanel") );  

    Hide();   
}

ConVar cl_show_achievement_popups( "cl_show_achievement_popups", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );


void CCSAchievementAnnouncePanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( cl_show_achievement_popups.GetBool() )
	{
		if ( Q_strcmp( "achievement_earned_local", pEventName ) == 0 )
		{
			//Add achievement to queue and show the UI (since the UI doesn't "Think()" until it is shown        
			Show();
			m_achievementQueue.Insert((eCSAchievementType)event->GetInt("achievement"));        
		}
	}
}

void CCSAchievementAnnouncePanel::Show()
{	
	m_bShouldBeVisible = true;
}

void CCSAchievementAnnouncePanel::Hide()
{
	m_bShouldBeVisible = false;
}

bool CCSAchievementAnnouncePanel::ShouldDraw( void )
{
	return (m_bShouldBeVisible && CHudElement::ShouldDraw());
}

void CCSAchievementAnnouncePanel::Paint( void )
{
}

void CCSAchievementAnnouncePanel::OnThink( void )
{
	BaseClass::OnThink();	

    if (!m_pAchievementInfoPanel)
    {
        return;
    }

    //if we have an achievement, update it
    if (m_currentDisplayedAchievement != CSInvalidAchievement)
    {        
		//If the match restarts, we need to move the start time for the achievement back.
		if (m_displayStartTime > gpGlobals->curtime)
		{	 
			m_displayStartTime = gpGlobals->curtime;
		}
        float timeSinceDisplayStart = gpGlobals->curtime - m_displayStartTime;
        float glowAlpha;
        float achievementPanelAlpha; 

        //Update the flash        
        if (m_pGlowPanel)        
        {
            GetGlowAlpha(timeSinceDisplayStart, glowAlpha);
            m_pGlowPanel->SetAlpha(glowAlpha);            
        }

        if (GetAchievementPanelAlpha(timeSinceDisplayStart, achievementPanelAlpha))
        {
            m_pAchievementInfoPanel->SetAlpha(achievementPanelAlpha);                        
        }
        else
        {
            //achievement is faded, so we are done
            m_pAchievementInfoPanel->SetAlpha(0);                        
            m_currentDisplayedAchievement = CSInvalidAchievement;

            if (m_achievementQueue.Count() == 0)
            {
                Hide();
            }
        }
    }    
    else
    {
        //Check if we need to start the next announcement in the queue. We won't update it this frame, but no time has elapsed anyway.
        if (m_achievementQueue.Count() > 0)
        {
            m_currentDisplayedAchievement = m_achievementQueue.RemoveAtHead();
            m_displayStartTime = gpGlobals->curtime;      

            //=============================================================================
            // HPE_BEGIN:
            // [dwenger] Play the achievement earned sound effect
            //=============================================================================

            vgui::surface()->PlaySound( "UI/achievement_earned.wav" );

            //=============================================================================
            // HPE_END
            //=============================================================================

            //Here we get the achievement to be displayed and set that in the popup windows
            CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
            if ( !pAchievementMgr )
                return;
            
            IAchievement *pAchievement = pAchievementMgr->GetAchievementByID( m_currentDisplayedAchievement );
            if ( pAchievement )
            {
                m_pAchievementInfoPanel->SetAchievement(pAchievement);
            }
        }
    }
}

bool CCSAchievementAnnouncePanel::GetAlphaFromTime(float elapsedTime, float delay, float fadeInTime, float holdTime, float fadeOutTime, float&alpha)
{
    //We just pass through each phase, subtracting off the full time if we are already done with that phase
    //We return whether the control should still be active

    //I. Delay before fading in
    if (elapsedTime > delay)
    {
        elapsedTime -= delay;
    }
    else
    {
        alpha = 0;
        return true;
    }


    //II. Fade in time
    if (elapsedTime > fadeInTime)
    {
        elapsedTime -= fadeInTime;
    }
    else
    {
        alpha = 255.0f * elapsedTime / fadeInTime;
        return true;
    }


    //III. Hold at full alpha time
    if (elapsedTime > holdTime)
    {
        elapsedTime -= holdTime;
    }
    else
    {
        alpha = 255.0f;
        return true;
    }

    //IV. Fade out time
    if (elapsedTime > fadeOutTime)
    {
        alpha = 0;
        return false;
    }
    else
    {
        alpha = 1.0f - (elapsedTime / fadeOutTime);
        alpha = Interpolators::SmoothStepStart(alpha);
        alpha *= 255.0f;  

		if (m_achievementQueue.Count() > 0 && alpha < 128.0f)
		{
			return false;
		}
        return true;
    }
}

ConVar glow_delay( "glow_delay", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar glow_fadeInTime( "glow_fadeInTime", "0.1", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar glow_holdTime( "glow_holdTime", "0.1", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar glow_fadeOutTime( "glow_fadeOutTime", "2.5", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);

bool CCSAchievementAnnouncePanel::GetGlowAlpha (float time, float& alpha)
{    
    

    /*
    float delay = 0.0f;
    float fadeInTime = 0.3f;
    float holdTime = 0.1f;
    float fadeOutTime = 0.4f;
    */

    //return GetAlphaFromTime(time, delay, fadeInTime, holdTime, fadeOutTime, alpha);

    return GetAlphaFromTime(time, glow_delay.GetFloat(), glow_fadeInTime.GetFloat(), glow_holdTime.GetFloat(), glow_fadeOutTime.GetFloat(), alpha);
}


ConVar popup_delay( "popup_delay", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar popup_fadeInTime( "popup_fadeInTime", "0.3", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar popup_holdTime( "popup_holdTime", "3.5", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);
ConVar popup_fadeOutTime( "popup_fadeOutTime", "3.0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY);

bool CCSAchievementAnnouncePanel::GetAchievementPanelAlpha (float time, float& alpha)
{
    /*
    float delay = 0.2f;
    float fadeInTime = 0.3f;
    float holdTime = 3.0f;
    float fadeOutTime = 2.0f;
    */

    //return GetAlphaFromTime(time, delay, fadeInTime, holdTime, fadeOutTime, alpha);

    return GetAlphaFromTime(time, popup_delay.GetFloat(), popup_fadeInTime.GetFloat(), popup_holdTime.GetFloat(), popup_fadeOutTime.GetFloat(), alpha);
    
}

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CCSAchivementInfoPanel::CCSAchivementInfoPanel( vgui::Panel *parent, const char* name ) : BaseClass( parent, name )
{
    //Create the main components of the display and attach them to the panel
    m_pAchievementIcon = SETUP_PANEL(new vgui::ScalableImagePanel( this, "Icon" ));
    m_pAchievementNameLabel = new vgui::Label( this, "Name", "name" );
    m_pAchievementDescLabel = new vgui::Label( this, "Description", "desc" );     
}

CCSAchivementInfoPanel::~CCSAchivementInfoPanel()
{   
    if (m_pAchievementIcon)
    {
        delete m_pAchievementIcon;
    }

    if (m_pAchievementNameLabel)
    {
        delete m_pAchievementNameLabel;
    }

    if (m_pAchievementDescLabel)
    {
        delete m_pAchievementDescLabel;
    }    
}

void CCSAchivementInfoPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{    
    LoadControlSettings( "Resource/UI/Achievement_Popup.res" );

    BaseClass::ApplySchemeSettings( pScheme );

    //Override the automatic scheme setting that happen above
    SetBgColor( pScheme->GetColor( "AchievementsLightGrey", Color(255, 0, 0, 255) ) );
    m_pAchievementNameLabel->SetFgColor(pScheme->GetColor("SteamLightGreen", Color(255, 255, 255, 255)));
    m_pAchievementDescLabel->SetFgColor(pScheme->GetColor("White", Color(255, 255, 255, 255)));
}

void CCSAchivementInfoPanel::SetAchievement(IAchievement* pAchievement)
{
    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Get achievement name and description text from the localized file
    //=============================================================================

    // Set name and description
    m_pAchievementNameLabel->SetText( ACHIEVEMENT_LOCALIZED_NAME( pAchievement ) );
    m_pAchievementDescLabel->SetText( ACHIEVEMENT_LOCALIZED_DESC( pAchievement ) );

    //Find, load and show the achievement icon
    char imagePath[_MAX_PATH];
    Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
    Q_strncat( imagePath, pAchievement->GetName(), sizeof(imagePath), COPY_ALL_CHARACTERS );    
    Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

    char checkFile[_MAX_PATH];
    Q_snprintf( checkFile, sizeof(checkFile), "materials\\vgui\\%s", imagePath );
    if ( !g_pFullFileSystem->FileExists( checkFile ) )
    {
        Q_snprintf( imagePath, sizeof(imagePath), "hud\\icon_locked.vtf" );
    }

    m_pAchievementIcon->SetImage( imagePath );
    m_pAchievementIcon->SetVisible( true );

    //=============================================================================
    // HPE_END
    //=============================================================================
}
