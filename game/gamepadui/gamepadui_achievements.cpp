
#include "gamepadui_button.h"
#include "gamepadui_frame.h"
#include "gamepadui_scroll.h"
#include "gamepadui_interface.h"
#include "gamepadui_image.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "fmtstr.h"

#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ScrollBar.h"

#include "KeyValues.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"

class GamepadUIAchievement;

class GamepadUIAchievementsPanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUIAchievementsPanel, GamepadUIFrame );

public:
    GamepadUIAchievementsPanel( vgui::Panel *pParent, const char* pPanelName );

    void UpdateGradients() OVERRIDE;

    void OnThink() OVERRIDE;
    void OnCommand( char const* pCommand ) OVERRIDE;

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

    void LayoutAchievementPanels();

    void OnMouseWheeled( int nDelta ) OVERRIDE;

private:
    CUtlVector< GamepadUIAchievement* > m_pAchievementPanels;

    GamepadUIScrollState m_ScrollState;

    int m_nTotalAchievements = 0;
    int m_nUnlockedAchievements = 0;

    GAMEPADUI_PANEL_PROPERTY( float, m_AchievementsFade, "Achievements.Fade", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_AchievementsOffsetX, "Achievements.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_AchievementsOffsetY, "Achievements.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flAchievementsSpacing, "Achievements.Spacing", "0", SchemeValueTypes::ProportionalFloat );
};

class GamepadUIAchievement : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIAchievement, GamepadUIButton );

    GamepadUIAchievement( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const char* pText, const char* pDescription, const char *pChapterImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pChapterImage )
    {
    }

    GamepadUIAchievement( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const wchar* pText, const wchar* pDescription, const char *pChapterImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pChapterImage )
    {
    }

    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE
    {
        BaseClass::ApplySchemeSettings( pScheme );

        m_hProgressFont = pScheme->GetFont( "Button.Progress.Font", true );
    }

    void Paint() OVERRIDE
    {
        int x, y, w, t;
        GetBounds( x, y, w, t );

        PaintButton();

        vgui::surface()->DrawSetColor( m_colProgressColor );
        vgui::surface()->DrawFilledRect( 0, GetDrawHeight()  - m_flProgressHeight, m_flWidth * m_flProgress, GetDrawHeight() );

        if ( m_nGoal > 1 && m_flProgress > 0.0f )
        {
            vgui::surface()->DrawSetColor( m_colUnprogressColor );
            vgui::surface()->DrawFilledRect( m_flWidth * m_flProgress, GetDrawHeight() - m_flProgressHeight, m_flWidth, GetDrawHeight() );
        }

        vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawSetTexture( m_Image );
        int nImageSize = m_flHeight - m_flIconInset * 2;
        vgui::surface()->DrawTexturedRect( m_flIconInset, m_flIconInset, m_flIconInset + nImageSize, m_flIconInset + nImageSize );
        vgui::surface()->DrawSetTexture( 0 );

        PaintText();

        if ( m_nGoal > 1 )
        {
            wchar_t wcsProgress[32];
            int nLength = V_swprintf_safe( wcsProgress, L"%d/%d", m_nCount, m_nGoal );
            vgui::surface()->DrawSetTextColor( m_colTextColor );
            vgui::surface()->DrawSetTextFont( m_hProgressFont );

            int32 textSizeX = 0, textSizeY = 0;
            vgui::surface()->GetTextSize( m_hProgressFont, wcsProgress, textSizeX, textSizeY );
            int textPosX = m_flWidth - textSizeX - m_flProgressOffsetX;
            int textPosY = GetDrawHeight() / 2 - textSizeY / 2;

            vgui::surface()->DrawSetTextPos( textPosX, textPosY );
            vgui::surface()->DrawPrintText( wcsProgress, nLength );
        }
    }

    void SetProgress( bool bAchieved, int nCount, int nGoal )
    {
        m_nCount = nCount;
        m_nGoal = nGoal;

        if (bAchieved)
        {
            m_flProgress = 1.0f;
            m_nCount = m_nGoal;
        }
        else
            m_flProgress = min( float( nCount ) / float( nGoal ), 1.0f );
    }

private:
    GamepadUIImage m_Image;

    float m_flProgress = 0.0f;
    int m_nCount = 0;
    int m_nGoal = 0;

    vgui::HFont m_hProgressFont = vgui::INVALID_FONT;

    GAMEPADUI_PANEL_PROPERTY( Color, m_colProgressColor,   "Button.Background.Progress",   "255 0 0 255", SchemeValueTypes::Color );
    GAMEPADUI_PANEL_PROPERTY( Color, m_colUnprogressColor, "Button.Background.Unprogress", "255 0 0 255", SchemeValueTypes::Color );

    GAMEPADUI_PANEL_PROPERTY( float, m_flProgressHeight,  "Button.Progress.Height",  "1", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flIconInset,       "Button.Icon.Inset",       "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flProgressOffsetX, "Button.Progress.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
};

GamepadUIAchievementsPanel::GamepadUIAchievementsPanel( vgui::Panel *pParent, const char* pPanelName ) : BaseClass( pParent, pPanelName )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( "#GameUI_Achievements_Title" );
    SetFooterButtons( FooterButtons::Back );

    Activate();

    GamepadUI::GetInstance().GetAchievementMgr()->EnsureGlobalStateLoaded();
    int nAllAchievements = GamepadUI::GetInstance().GetAchievementMgr()->GetAchievementCount();
    for ( int i = 0; i < nAllAchievements; i++ )
    {
        IAchievement *pCurAchievement = GamepadUI::GetInstance().GetAchievementMgr()->GetAchievementByIndex( i );
        if ( !pCurAchievement )
            continue;

        if ( pCurAchievement->IsAchieved() )
            m_nUnlockedAchievements++;

        // Don't show hidden achievements if not achieved.
        if ( pCurAchievement->ShouldHideUntilAchieved() && !pCurAchievement->IsAchieved() )
            continue;

        char szIconName[MAX_PATH];
        V_sprintf_safe( szIconName, "gamepadui/achievements/%s%s.vmt", pCurAchievement->GetName(), pCurAchievement->IsAchieved() ? "" : "_bw" );

        char szMaterialName[MAX_PATH];
        V_sprintf_safe( szMaterialName, "materials/%s", szIconName );
        if ( !g_pFullFileSystem->FileExists( szMaterialName ) )
            V_sprintf_safe( szIconName, "vgui/achievements/%s%s.vmt", pCurAchievement->GetName(), pCurAchievement->IsAchieved() ? "" : "_bw" );

        V_sprintf_safe( szMaterialName, "materials/%s", szIconName );
        if ( !g_pFullFileSystem->FileExists( szMaterialName ) )
            V_strcpy_safe( szIconName, "vgui/hud/icon_locked.vmt" );

        auto pAchievementPanel = new GamepadUIAchievement(
            this, this,
            GAMEPADUI_RESOURCE_FOLDER "schemeachievement.res",
            "",
            ACHIEVEMENT_LOCALIZED_NAME( pCurAchievement ),
            ACHIEVEMENT_LOCALIZED_DESC( pCurAchievement ),
            szIconName );
        pAchievementPanel->SetProgress( pCurAchievement->IsAchieved(), pCurAchievement->GetCount(), pCurAchievement->GetGoal() );
        pAchievementPanel->SetPriority( m_nTotalAchievements );
        m_pAchievementPanels.AddToTail( pAchievementPanel );

        m_nTotalAchievements++;
    }

    if ( m_pAchievementPanels.Count() )
        m_pAchievementPanels[0]->NavigateTo();

    for ( int i = 1; i < m_pAchievementPanels.Count(); i++ )
    {
        m_pAchievementPanels[i]->SetNavUp( m_pAchievementPanels[i - 1] );
        m_pAchievementPanels[i - 1]->SetNavDown( m_pAchievementPanels[i] );
    }

    UpdateGradients();
}

void GamepadUIAchievementsPanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 0.5f }, flTime );
}

void GamepadUIAchievementsPanel::OnThink()
{
    BaseClass::OnThink();

    LayoutAchievementPanels();
}

void GamepadUIAchievementsPanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    GamepadUIButton *pButton = dynamic_cast< GamepadUIButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        if ( pButton->GetAlpha() != 255 )
        {
            int nParentW, nParentH;
	        GetParent()->GetSize( nParentW, nParentH );

            int nX, nY;
            pButton->GetPos( nX, nY );

            int nTargetY = pButton->GetPriority() * (pButton->m_flHeightAnimationValue[ButtonStates::Out] + m_flAchievementsSpacing);

            if ( nY < nParentH / 2 )
            {
                nTargetY += nParentH - m_AchievementsOffsetY;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetY -= m_flAchievementsSpacing;
            }
            else
            {
                nTargetY += pButton->GetMaxHeight();
                // Add a bit of spacing to make this more visually appealing :)
                nTargetY += (pButton->GetMaxHeight() / 2) + 2 * m_flAchievementsSpacing;
            }


            m_ScrollState.SetScrollTarget( nTargetY - ( nParentH - m_AchievementsOffsetY ), GamepadUI::GetInstance().GetTime() );
        }
    }
}

void GamepadUIAchievementsPanel::LayoutAchievementPanels()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    float flScrollClamp = 0.0f;
    for ( int i = 0; i < m_pAchievementPanels.Count(); i++ )
    {
        int size = ( m_pAchievementPanels[i]->GetTall() + m_flAchievementsSpacing );

        if ( i < m_pAchievementPanels.Count() - 2 )
            flScrollClamp += size;
    }

    m_ScrollState.UpdateScrollBounds( 0.0f, flScrollClamp );

    int previousSizes = 0;
    for ( int i = 0; i < m_pAchievementPanels.Count(); i++ )
    {
        int tall = m_pAchievementPanels[i]->GetTall();
        int size = ( tall + m_flAchievementsSpacing );

        int y = m_AchievementsOffsetY + previousSizes - m_ScrollState.GetScrollProgress();
        int fade = 255;
        if ( y < m_AchievementsOffsetY )
            fade = ( 1.0f - clamp( -( y - m_AchievementsOffsetY ) / m_AchievementsFade, 0.0f, 1.0f ) ) * 255.0f;
        if ( y > nParentH - m_AchievementsFade )
            fade = ( 1.0f - clamp(( y - ( nParentH - m_AchievementsFade - size ) ) / m_AchievementsFade, 0.0f, 1.0f ) ) * 255.0f;
        if ( m_pAchievementPanels[i]->HasFocus() && fade != 0 )
            fade = 255;
        m_pAchievementPanels[i]->SetAlpha( fade );
        m_pAchievementPanels[i]->SetPos( m_AchievementsOffsetX, y );
        m_pAchievementPanels[i]->SetVisible( true );
        previousSizes += size;
    }

    m_ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUIAchievementsPanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
        Close();
    }
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

void GamepadUIAchievementsPanel::OnMouseWheeled( int nDelta )
{
    m_ScrollState.OnMouseWheeled( nDelta * 160.0f, GamepadUI::GetInstance().GetTime() );
}

CON_COMMAND( gamepadui_openachievementsdialog, "" )
{
    new GamepadUIAchievementsPanel( GamepadUI::GetInstance().GetBasePanel(), "AchievementsPanel" );
}
