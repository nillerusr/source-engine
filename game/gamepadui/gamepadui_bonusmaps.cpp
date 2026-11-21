#include "gamepadui_interface.h"
#include "gamepadui_image.h"
#include "gamepadui_util.h"
#include "gamepadui_frame.h"
#include "gamepadui_scrollbar.h"
#include "gamepadui_genericconfirmation.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ScrollBar.h"

#include "KeyValues.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"


#if defined ( GAMEPADUI_ENABLE_BONUSMAPS ) // SDK2013 lacks the necessary files to compile this. (Madi)

#define SDK_2013_HACKS MAPBASE

#if SDK_2013_HACKS
struct ChallengeDescription_t
{
    int iBest;
    int iGold;
    int iSilver;
    int iBronze;
    char szName[64];
    char szComment[64];
    int iType;
};

struct BonusMapDescription_t
{
    char szMapName[128];
    char szMapFileName[128];
    char szImageName[128];
    char szComment[256];
    char szFileName[128];

    bool bIsFolder;
    bool bComplete;
    bool bLocked;

    CUtlVector<ChallengeDescription_t> m_Challenges;

    BonusMapDescription_t *m_pParent;
};
#endif

class GamepadUIBonusButton;

#define GAMEPADUI_BONUS_SCHEME GAMEPADUI_RESOURCE_FOLDER "schemebonusbutton.res"

#define NUM_CHALLENGES 5

ConVar gamepadui_selected_challenge("gamepadui_selected_challenge", "0", FCVAR_ARCHIVE);

const char g_pszMedalNames[4][8] =
{
	"none",
	"bronze",
	"silver",
	"gold"
};

void GetChallengeMedals( const ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal )
{
	iBest = pChallengeDescription->iBest;

	if ( iBest == -1 )
		iEarnedMedal = 0;
	else if ( iBest <= pChallengeDescription->iGold )
		iEarnedMedal = 3;
	else if ( iBest <= pChallengeDescription->iSilver )
		iEarnedMedal = 2;
	else if ( iBest <= pChallengeDescription->iBronze )
		iEarnedMedal = 1;
	else
		iEarnedMedal = 0;

	iNext = -1;

	switch ( iEarnedMedal )
	{
	case 0:
		iNext = pChallengeDescription->iBronze;
		iNextMedal = 1;
		break;
	case 1:
		iNext = pChallengeDescription->iSilver;
		iNextMedal = 2;
		break;
	case 2:
		iNext = pChallengeDescription->iGold;
		iNextMedal = 3;
		break;
	case 3:
		iNext = -1;
		iNextMedal = -1;
		break;
	}
}

void CycleSelectedChallenge()
{
    int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, NUM_CHALLENGES-1 );
    nChallenge = ( nChallenge + 1 ) % NUM_CHALLENGES;
    gamepadui_selected_challenge.SetValue( nChallenge );
}

class GamepadUIBonusMapsPanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUIBonusMapsPanel, GamepadUIFrame );

public:
    GamepadUIBonusMapsPanel( vgui::Panel *pParent, const char* pPanelName );

    void UpdateGradients();

    void OnThink() OVERRIDE;
    void OnCommand( char const* pCommand ) OVERRIDE;

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

    void BuildMapsList();
    void LayoutBonusButtons();
    void Paint() OVERRIDE;
    void ApplySchemeSettings(vgui::IScheme* pScheme) OVERRIDE;

    void OnMouseWheeled( int delta ) OVERRIDE;

#if SDK_2013_HACKS
    void ScanBonusMaps();
    void ScanBonusMapSubDir( const char *pszDir, BonusMapDescription_t *pParent );
    void LoadBonusMapDir( const char *pszDir, BonusMapDescription_t *pParent = NULL, BonusMapDescription_t **ppFolder = NULL );
    void LoadBonusMap( const char *pszMapList, BonusMapDescription_t *pParent = NULL );
    void ClearBonusMapsList();
    int BonusCount();
    BonusMapDescription_t *GetBonusData( int iMapIndex );
    bool IsValidIndex( int iMapIndex );

    BonusMapDescription_t *GetCurrentFolder();
    void BackFolder();
    void EnterFolder( BonusMapDescription_t *pFolder );

    void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold );
    void SetCurrentChallengeNames( const char *pszFileName, const char *pszMapName, const char *pszChallengeName );

private:

    CUtlVector< BonusMapDescription_t > m_BonusFolderHierarchy;
#endif

private:
    CUtlVector< GamepadUIBonusButton* > m_pBonusButtons;
    CUtlVector< BonusMapDescription_t > m_Bonuses;

    int m_nBonusRowSize = 3;

    GamepadUIScrollState m_ScrollState;

    GamepadUIScrollBar *m_pScrollBar;

    GAMEPADUI_PANEL_PROPERTY( float, m_BonusOffsetX, "Bonus.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_BonusOffsetY, "Bonus.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_BonusSpacing, "Bonus.Spacing", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flBonusFade, "Bonus.Fade", "80", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterMedalSize, "FooterMedal.Current.Size", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterMedalNextSize, "FooterMedal.Next.Size", "0", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flBonusProgressOffsetX, "Bonus.Progress.OffsetX", "384", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flBonusProgressOffsetY, "Bonus.Progress.OffsetY", "56", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flBonusProgressWidth, "Bonus.Progress.Width", "256", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flBonusProgressHeight, "Bonus.Progress.Height", "32", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( Color, m_colProgressBackgroundColor, "Bonus.Progress.Background", "32 32 32 128", SchemeValueTypes::Color );
    GAMEPADUI_PANEL_PROPERTY( Color, m_colProgressFillColor, "Bonus.Progress.Fill", "255 192 0 255", SchemeValueTypes::Color );

    vgui::HFont m_hDescFont = vgui::INVALID_FONT;
    vgui::HFont m_hGoalFont = vgui::INVALID_FONT;

    GamepadUIImage m_CachedMedals[2];
    char m_szCachedMedalNames[256][2];

    bool m_bHasChallenges;
    float m_flCompletionPerc;
};

class GamepadUIBonusButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIBonusButton, GamepadUIButton );

    GamepadUIBonusButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const char* pText, const char* pDescription, const char *pBonusImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pBonusImage )
        , m_LockIcon( "gamepadui/lockylock" )
    {
        bCompleted[0] = false;
        bCompleted[1] = false;
        bCompleted[2] = false;
    }

    GamepadUIBonusButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const wchar* pText, const wchar* pDescription, const char *pBonusImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pBonusImage )
        , m_LockIcon( "gamepadui/lockylock" )
    {
        bCompleted[0] = false;
        bCompleted[1] = false;
        bCompleted[2] = false;
    }

    ~GamepadUIBonusButton()
    {
        if ( s_pLastBonusButton == this )
            s_pLastBonusButton = NULL;
    }

    ButtonState GetCurrentButtonState() OVERRIDE
    {
        if ( s_pLastBonusButton == this )
            return ButtonState::Over;
        return BaseClass::GetCurrentButtonState();
    }

    void Paint() OVERRIDE
    {
        int x, y, w, t;
        GetBounds( x, y, w, t );

        PaintButton();

        int iAlpha = GetAlpha();
        vgui::surface()->DrawSetColor( Color( iAlpha, iAlpha, iAlpha, iAlpha ) );
        vgui::surface()->DrawSetTexture( m_Image );
        int imgH = ( w * 9 ) / 16;
        //vgui::surface()->DrawTexturedRect( 0, 0, w, );
        float offset = m_flTextOffsetYAnimationValue[ButtonStates::Out] - m_flTextOffsetY;

        if (m_bTGA)
            vgui::surface()->DrawTexturedRect( 0, 0, w, ( imgH - offset ) );
        else
            vgui::surface()->DrawTexturedSubRect( 0, 0, w, w * 100 / 180, 0.0f, 0.0f, 1.0f * ( 180.0f / 256.0f ), ( ( imgH - offset ) / imgH ) * 100.0f / 128.0f );

        vgui::surface()->DrawSetTexture( 0 );
        if ( !IsEnabled() )
        {
            vgui::surface()->DrawSetColor( Color( 255, 255, 255, 16 ) );
            vgui::surface()->DrawFilledRect( 0, 0, w, w * 100 / 180 );

            vgui::surface()->DrawSetColor( m_colLock );
            vgui::surface()->DrawSetTexture( m_LockIcon );
            int nX = m_flWidth / 2 - m_flLockSize / 2;
            int nY = imgH / 2 - m_flLockSize / 2;
            vgui::surface()->DrawTexturedRect( nX, nY, nX + m_flLockSize, nY + m_flLockSize );
        }

        vgui::surface()->DrawSetColor( m_colProgressColor );
        vgui::surface()->DrawFilledRect( 0, (w * 100 / 180) - m_flProgressHeight, w *  ( IsComplete() ? 1.0f : 0.0f), (w * 100 / 180));

        PaintText();

        int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, NUM_CHALLENGES-1 );
        if ( m_Medals[ nChallenge ].IsValid() )
        {
            vgui::surface()->DrawSetColor( Color( iAlpha, iAlpha, iAlpha, iAlpha ) );
            vgui::surface()->DrawSetTexture( m_Medals[ nChallenge ] );
            vgui::surface()->DrawTexturedRect( m_flMedalOffsetX, m_flMedalOffsetY, m_flMedalOffsetX + m_flMedalSize, m_flMedalOffsetY + m_flMedalSize );
            vgui::surface()->DrawSetTexture( 0 );
        }
    }

    void ApplySchemeSettings( vgui::IScheme *pScheme )
    {
        BaseClass::ApplySchemeSettings( pScheme );

        float flX, flY;
        if (GamepadUI::GetInstance().GetScreenRatio( flX, flY ))
        {
            if (flX != 1.0f)
            {
                // For now, undo the scaling from base class
                m_flWidth /= flX;
                for (int i = 0; i < ButtonStates::Count; i++)
                    m_flWidthAnimationValue[i] /= flX;
            }

            SetSize( m_flWidth, m_flHeight + m_flExtraHeight );
            DoAnimations( true );
        }

        // Use a smaller font if necessary
        int nTextSizeX, nTextSizeY;
        vgui::surface()->GetTextSize( m_hTextFont, m_strButtonText.String(), nTextSizeX, nTextSizeY );
        if ( nTextSizeX >= m_flWidth )
        {
            m_hTextFont        = pScheme->GetFont( "Button.Text.Font.Small", true );
            m_hTextFontOver    = pScheme->GetFont( "Button.Text.Font.Small.Over", true );
            if (m_hTextFontOver == vgui::INVALID_FONT )
                m_hTextFontOver = m_hTextFont;
        }
    }

    void NavigateTo() OVERRIDE
    {
        BaseClass::NavigateTo();
        s_pLastBonusButton = this;
    }

    const BonusMapDescription_t& GetBonusMapDescription() const
    {
        return m_BonusMapDesc;
    }

    bool IsComplete() const
    {
        if ( m_BonusMapDesc.bIsFolder )
            return false;
        int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, NUM_CHALLENGES-1 );
        return m_BonusMapDesc.bComplete || bCompleted[ nChallenge ];
    }

    void SetBonusMapDescription( BonusMapDescription_t* pDesc )
    {
        for ( auto& medal : m_Medals )
            medal.Cleanup();
        m_BonusMapDesc = *pDesc;

#if SDK_2013_HACKS
        if ( m_BonusMapDesc.m_Challenges.Count() == 0 )
#else
        if ( !m_BonusMapDesc.m_pChallenges )
#endif
            return;

        int nNumChallenges = 0;
#if SDK_2013_HACKS
        for ( ChallengeDescription_t& challenge : m_BonusMapDesc.m_Challenges )
#else
        for ( ChallengeDescription_t& challenge : *m_BonusMapDesc.m_pChallenges )
#endif
        {
            int iChallengeNum = challenge.iType != -1 ? challenge.iType : nNumChallenges;
			int iBest, iEarnedMedal, iNext, iNextMedal;
			GetChallengeMedals( &challenge, iBest, iEarnedMedal, iNext, iNextMedal );

            bCompleted[ iChallengeNum ] = iNextMedal == -1;

            char szBuff[256];
            if (iChallengeNum < 10)
                Q_snprintf(szBuff, 256, "vgui/medals/medal_0%i_%s", iChallengeNum, g_pszMedalNames[iEarnedMedal]);
            else
                Q_snprintf(szBuff, 256, "vgui/medals/medal_%i_%s", iChallengeNum, g_pszMedalNames[iEarnedMedal]);
            m_Medals[ iChallengeNum ].SetImage( szBuff );

            nNumChallenges++;
        }
    }

    void SetTGAImage( const char *pszImage )
    {
        m_Image.SetTGAImage( pszImage );
        m_bTGA = true;
    }

    static GamepadUIBonusButton* GetLastBonusButton() { return s_pLastBonusButton; }

private:
    GamepadUIImage m_Image;
    GamepadUIImage m_Medals[NUM_CHALLENGES];
    GamepadUIImage m_LockIcon;

    bool bCompleted[NUM_CHALLENGES];

    bool m_bTGA = false;

    static GamepadUIBonusButton *s_pLastBonusButton;

    BonusMapDescription_t m_BonusMapDesc = {};

    GAMEPADUI_PANEL_PROPERTY( Color, m_colProgressColor, "Button.Background.Progress", "255 0 0 255", SchemeValueTypes::Color );
    GAMEPADUI_PANEL_PROPERTY( float, m_flProgressHeight, "Button.Progress.Height",     "1", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flMedalSize, "Button.Medal.Size", "64", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flMedalOffsetX, "Button.Medal.OffsetX", "8", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flMedalOffsetY, "Button.Medal.OffsetY", "8", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flLockSize, "Button.Lock.Size", "80", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( Color, m_colLock, "Button.Lock", "20 20 20 255", SchemeValueTypes::Color );
};

GamepadUIBonusButton* GamepadUIBonusButton::s_pLastBonusButton = NULL;

GamepadUIBonusMapsPanel::GamepadUIBonusMapsPanel( vgui::Panel *pParent, const char* PanelName ) : BaseClass( pParent, PanelName )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    FooterButtonMask buttons = FooterButtons::Back | FooterButtons::Select;
    SetFooterButtons( buttons, FooterButtons::Select );

    BuildMapsList();
    Activate();

    UpdateGradients();

    m_pScrollBar = new GamepadUIScrollBar(
        this, this,
        GAMEPADUI_RESOURCE_FOLDER "schemescrollbar.res",
        NULL, false );
}

void GamepadUIBonusMapsPanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 0.5f }, flTime );
}

void GamepadUIBonusMapsPanel::OnThink()
{
    BaseClass::OnThink();

    LayoutBonusButtons();
}

void GamepadUIBonusMapsPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );
    m_hGoalFont = pScheme->GetFont( "Goal.Font", true );
    m_hDescFont = pScheme->GetFont( "BonusDesc.Font", true );

    float flX, flY;
    if (GamepadUI::GetInstance().GetScreenRatio( flX, flY ))
    {
        m_BonusOffsetX *= flX;
    }
    
    int nX, nY;
    GamepadUI::GetInstance().GetSizingPanelOffset( nX, nY );
    if (nX > 0)
    {
        GamepadUI::GetInstance().GetSizingPanelScale( flX, flY );
        flX *= 0.5f;

        m_BonusOffsetX += ((float)nX) * flX;
        m_flBonusFade += ((float)nX) * flX;
    }
}

void GamepadUIBonusMapsPanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    // TODO: Scroll
#if 1
    GamepadUIBonusButton *pButton = dynamic_cast< GamepadUIBonusButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        int nParentW, nParentH;
	    GetParent()->GetSize( nParentW, nParentH );
        
        int nX, nY;
        pButton->GetPos( nX, nY );
        if ( nY + m_flFooterButtonsOffsetY + m_nFooterButtonHeight + pButton->GetTall() > nParentH || nY < m_BonusOffsetY )
        {
            int nRow = ( m_pBonusButtons.Find( pButton ) / m_nBonusRowSize );
            if ( nY >= m_BonusOffsetY )
                nRow--;

            float flScrollProgress = (pButton->m_flHeight + m_BonusSpacing) * ((float)nRow);
            m_ScrollState.SetScrollTarget( flScrollProgress, GamepadUI::GetInstance().GetTime() );
        }
    }
#endif
}

#define MAX_LISTED_BONUS_MAPS 128

void GamepadUIBonusMapsPanel::BuildMapsList()
{
    m_pBonusButtons.PurgeAndDeleteElements();

#if SDK_2013_HACKS
    ClearBonusMapsList();
    ScanBonusMaps();
#else
    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->ClearBonusMapsList();
    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->ScanBonusMaps();
#endif

#if !SDK_2013_HACKS
    char szDisplayPath[_MAX_PATH];
	Q_snprintf( szDisplayPath, _MAX_PATH, "%s/", GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath() );
#endif

    //bool bIsRoot = !Q_strcmp( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), "." );
    //if ( bIsRoot )
        GetFrameTitle() = GamepadUIString( "#GameUI_BonusMaps" );
    //else
    //    GetFrameTitle() = GamepadUIString( szDisplayPath );

    m_bHasChallenges = false;

#if SDK_2013_HACKS

    // add to the list
	for ( int iMapIndex = 0; iMapIndex < m_Bonuses.Count() && iMapIndex < MAX_LISTED_BONUS_MAPS; ++iMapIndex)
	{
        BonusMapDescription_t *pDesc = &m_Bonuses[iMapIndex];

        char szImage[MAX_PATH];
        bool bTGA = false;
        if (pDesc->szImageName[0])
        {
            if (pDesc->szImageName[0] == '.' && GetCurrentFolder())
                V_snprintf( szImage, sizeof( szImage ), "%s%c%s", GetCurrentFolder()->szFileName, CORRECT_PATH_SEPARATOR, pDesc->szImageName );
            else
                V_strncpy( szImage, pDesc->szImageName, sizeof( szImage ) );

            if (!g_pFullFileSystem->FileExists( szImage ))
                V_snprintf( szImage, sizeof( szImage ), "vgui/%s", pDesc->szImageName );

            const char *pszExt = V_GetFileExtension( szImage );
            if (pszExt && V_strncmp( pszExt, "tga", 4 ) == 0)
                bTGA = true;
        }
        else
        {
            // Look for a nearby image
            if (pDesc->bIsFolder)
            {
                Q_snprintf( szImage, sizeof( szImage ), "%s%cfoldericon", pDesc->szFileName, CORRECT_PATH_SEPARATOR );
            }
            else
            {
                if (GetCurrentFolder() != NULL)
                {
                    Q_snprintf( szImage, sizeof( szImage ), "%s%c%s", GetCurrentFolder()->szFileName, CORRECT_PATH_SEPARATOR, pDesc->szFileName );
                }
                else
                {
                    V_strncpy( szImage, pDesc->szFileName, sizeof( szImage ) );
                }
            }

            V_SetExtension( szImage, ".tga", sizeof( szImage ) );

            if (!g_pFullFileSystem->FileExists( szImage ))
            {
                // Use default
                V_strncpy( szImage, "vgui/bonusmaps/icon_bonus_map_default", sizeof( szImage ) );
            }
            else
                bTGA = true;
        }

        if ( pDesc->m_Challenges.Count() > 0 )
            m_bHasChallenges = true;

        GamepadUIBonusButton *pChapterButton = new GamepadUIBonusButton(
            this, this,
            GAMEPADUI_BONUS_SCHEME, "action_map",
            pDesc->szMapName, NULL, /*pDesc->szComment*/ szImage);
        pChapterButton->SetPriority( iMapIndex );
        pChapterButton->SetEnabled( !pDesc->bLocked );
        pChapterButton->SetForwardToParent( true );
        pChapterButton->SetBonusMapDescription( pDesc );

        // Make bonus maps render under everything
        pChapterButton->SetZPos( 1 );

        if (bTGA)
            pChapterButton->SetTGAImage( szImage );

		m_pBonusButtons.AddToTail( pChapterButton );
	}
#else
    int iMapCount = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->BonusCount();

	// add to the list
	for ( int iMapIndex = 0; iMapIndex < iMapCount && iMapIndex < MAX_LISTED_BONUS_MAPS; ++iMapIndex )
	{
        BonusMapDescription_t *pDesc = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetBonusData( iMapIndex );

        char szImage[MAX_PATH];
        V_snprintf( szImage, sizeof( szImage ), "vgui/%s", pDesc->szImageName );

        if ( pDesc->m_pChallenges )
            m_bHasChallenges = true;

        GamepadUIBonusButton *pChapterButton = new GamepadUIBonusButton(
            this, this,
            GAMEPADUI_BONUS_SCHEME, "action_map",
            pDesc->szMapName, NULL, /*pDesc->szComment*/ szImage);
        pChapterButton->SetPriority( iMapIndex );
        pChapterButton->SetEnabled( !pDesc->bLocked );
        pChapterButton->SetForwardToParent( true );
        pChapterButton->SetBonusMapDescription( pDesc );

		m_pBonusButtons.AddToTail( pChapterButton );
        m_Bonuses.AddToTail( *pDesc );
	}
#endif

    if ( m_pBonusButtons.Count() > 0 )
        m_pBonusButtons[0]->NavigateTo();

    for ( int i = 1; i < m_pBonusButtons.Count(); i++)
    {
        m_pBonusButtons[i]->SetNavLeft(m_pBonusButtons[i - 1]);
        m_pBonusButtons[i - 1]->SetNavRight(m_pBonusButtons[i] );
    }

    FooterButtonMask buttons = FooterButtons::Back | FooterButtons::Select;

    if (m_bHasChallenges)
    {
        buttons |= FooterButtons::Challenge;

        // Get completion percentage
        int nNumChallenges = 0;
        int nNumComplete = 0;
        for ( int i = 0; i < m_pBonusButtons.Count(); i++ )
        {
            int nChallengeCount = m_pBonusButtons[i]->GetBonusMapDescription().m_Challenges.Count();
            if (nChallengeCount <= 0)
                continue;

            nNumChallenges += (nChallengeCount*3);

            if (m_pBonusButtons[i]->GetBonusMapDescription().bComplete)
            {
                nNumComplete += (nChallengeCount*3);
            }
            else
            {
                const CUtlVector<ChallengeDescription_t> &desc = m_pBonusButtons[i]->GetBonusMapDescription().m_Challenges;
                for (int c = 0; c < nChallengeCount; c++)
                {
                    if (desc[c].iBest != -1)
                    {
                        if (desc[c].iBest <= desc[c].iGold)
                            nNumComplete += 3;
                        else if (desc[c].iBest <= desc[c].iSilver)
                            nNumComplete += 2;
                        else if (desc[c].iBest <= desc[c].iBronze)
                            nNumComplete += 1;
                    }
                }
            }
        }

        m_flCompletionPerc = ((float)nNumComplete) / ((float)nNumChallenges);
    }

    SetFooterButtons( buttons, FooterButtons::Select );
}

void GamepadUIBonusMapsPanel::LayoutBonusButtons()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    float flScrollClamp = ((float)m_pBonusButtons[0]->GetTall() + m_BonusSpacing) * Max(1.0f, ceilf(((float)m_pBonusButtons.Count() - m_nBonusRowSize*2) / ((float)m_nBonusRowSize)) );

    m_ScrollState.UpdateScrollBounds( 0.0f, flScrollClamp );

	if (m_pBonusButtons.Count() > 0)
	{
		m_pScrollBar->InitScrollBar( &m_ScrollState, nParentW - m_BonusOffsetX - m_BonusSpacing, m_BonusOffsetY );
		m_pScrollBar->UpdateScrollBounds( 0.0f, flScrollClamp,
			((m_pBonusButtons[0]->GetTall() + m_BonusSpacing) * m_nBonusRowSize*2), nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight - m_BonusOffsetY );
	}

    int x = m_BonusOffsetX;
    int y = m_BonusOffsetY - m_ScrollState.GetScrollProgress();
    CUtlVector< CUtlVector< GamepadUIBonusButton* > > pButtonRows;
    int j = 0;
    for ( int i = 0; i < m_pBonusButtons.Count(); i++ )
    {
        int size = ( m_pBonusButtons[0]->GetWide() + m_BonusSpacing );

        if ( x + size > nParentW - m_BonusOffsetX )
        {
            j = 0;
            x = m_BonusOffsetX;
            y += m_pBonusButtons[0]->GetTall() + m_BonusSpacing;
        }

        int fade = 255;
        if ( y < m_BonusOffsetY )
            fade = RemapValClamped( m_BonusOffsetY - y, m_flBonusFade, 0, 0, 255 );
        else if ( y > ( nParentH - (int)m_flFooterButtonsOffsetY - m_nFooterButtonHeight - (int)m_flBonusFade ) )
            fade = RemapValClamped( y - ( nParentH - (int)m_flFooterButtonsOffsetY - m_nFooterButtonHeight - (int)m_flBonusFade ), 0, m_flBonusFade, 255, 0 );
        if ( ( m_pBonusButtons[i]->HasFocus() && m_pBonusButtons[i]->IsEnabled() ) && fade != 0 )
            fade = 255;

        m_pBonusButtons[i]->SetAlpha( fade );

        m_pBonusButtons[i]->SetPos( x, y );
        m_pBonusButtons[i]->SetVisible( true );
        j++;

        x += size + m_BonusSpacing;
        while (pButtonRows.Size() <= j)
            pButtonRows.AddToTail();
        pButtonRows[j].AddToTail( m_pBonusButtons[i] );
    }

    for ( int i = 0; i < pButtonRows.Count(); i++ )
    {
        for (int j = 1; j < pButtonRows[i].Count(); j++)
        {
            pButtonRows[i][j]->SetNavUp(pButtonRows[i][j - 1]);
            pButtonRows[i][j - 1]->SetNavDown(pButtonRows[i][j]);
        }
    }

    if (pButtonRows.Count() > 1)
        m_nBonusRowSize = pButtonRows.Count()-1;
    else
        m_nBonusRowSize = 1;

    m_ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUIBonusMapsPanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
#if SDK_2013_HACKS
        bool bIsRoot = GetCurrentFolder() == NULL;
#else
        bool bIsRoot = !Q_strcmp( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), "." );
#endif
        if ( bIsRoot )
            Close();
        else
        {
#if SDK_2013_HACKS
            BackFolder();
#else
            GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->BackPath();
#endif

            BuildMapsList();
        }
    }
    else if ( !V_strcmp( pCommand, "action_challenges" ) )
    {
        CycleSelectedChallenge();
    }
    else if ( !V_strcmp( pCommand, "action_map" ) )
    {
        GamepadUIBonusButton* pButton = GamepadUIBonusButton::GetLastBonusButton();
        if ( pButton )
        {
		    int mapIndex = pButton->GetPriority();
#if SDK_2013_HACKS
		    if ( IsValidIndex( mapIndex ) )
#else
		    if ( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->IsValidIndex( mapIndex ) )
#endif
		    {
#if SDK_2013_HACKS
                BonusMapDescription_t *pBonusMap = GetBonusData( mapIndex );
#else
			    BonusMapDescription_t *pBonusMap = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetBonusData( mapIndex );
#endif

			    // Don't do anything with locked items
			    if ( pBonusMap->bLocked )
				    return;

#if SDK_2013_HACKS
                const char *shortName = pBonusMap->szFileName;
#else
                const char *shortName = pBonusMap->szShortName;
#endif
			    if ( shortName && shortName[ 0 ] )
			    {
				    if ( pBonusMap->bIsFolder )
				    {
#if SDK_2013_HACKS
                        EnterFolder( pBonusMap );
#else
					    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->AppendPath( shortName );
#endif

					    BuildMapsList();
                    }
                    else
                    {
					    // Load the game, return to top and switch to engine
					    char sz[ 256 ];

					    // Set the challenge mode if one is selected
                        int iChallenge = 0;

                        GamepadUIBonusButton* pButton = GamepadUIBonusButton::GetLastBonusButton();
                        if ( pButton )
                        {
                            const BonusMapDescription_t& desc = pButton->GetBonusMapDescription();
#if SDK_2013_HACKS
                            if ( desc.m_Challenges.Count() > 0 )
#else
                            if ( desc.m_pChallenges )
#endif
                            {
                                iChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, NUM_CHALLENGES-1 ) + 1;
                            }
                        }

                        // Set commentary
                        ConVarRef commentary( "commentary" );
                        commentary.SetValue( 0 );

                        ConVarRef sv_cheats( "sv_cheats" );
                        sv_cheats.SetValue( 0 );

					    if ( iChallenge > 0 )
					    {
						    Q_snprintf( sz, sizeof( sz ), "sv_bonus_challenge %i\n", iChallenge );
						    GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted(sz);

#if SDK_2013_HACKS
                            ChallengeDescription_t *pChallengeDescription = &pBonusMap->m_Challenges[iChallenge - 1];
#else
						    ChallengeDescription_t *pChallengeDescription = &((*pBonusMap->m_pChallenges)[ iChallenge - 1 ]);
#endif

						    // Set up medal goals
#if SDK_2013_HACKS
                            SetCurrentChallengeObjectives( pChallengeDescription->iBronze, pChallengeDescription->iSilver, pChallengeDescription->iGold );
                            SetCurrentChallengeNames( pBonusMap->szFileName, pBonusMap->szMapName, pChallengeDescription->szName );
#else
						    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->SetCurrentChallengeObjectives( pChallengeDescription->iBronze, pChallengeDescription->iSilver, pChallengeDescription->iGold );
						    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->SetCurrentChallengeNames( pBonusMap->szFileName, pBonusMap->szMapName, pChallengeDescription->szName );
#endif
					    }

					    if ( pBonusMap->szMapFileName[ 0 ] != '.' )
					    {
                            Q_snprintf( sz, sizeof( sz ), "progress_enable\nmap %s\n", pBonusMap->szMapFileName );
					    }
					    else
					    {
#if SDK_2013_HACKS
                            const char *pchSubDir = Q_strnchr( pBonusMap->szFileName, '/', Q_strlen( pBonusMap->szFileName ) );
#else
						    const char *pchSubDir = Q_strnchr( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), '/', Q_strlen( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath() ) );
#endif

						    if ( pchSubDir )
						    {
							    pchSubDir = Q_strnchr( pchSubDir + 1, '/', Q_strlen( pchSubDir ) );

							    if ( pchSubDir )
							    {
								    ++pchSubDir;
								    const char *pchMapFileName = pBonusMap->szMapFileName + 2;
								    Q_snprintf( sz, sizeof( sz ), "progress_enable\nmap %s/%s\n", pchSubDir, pchMapFileName );
							    }
						    }
					    }

                        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( sz );

					    OnClose();
                    }
                }
            }
        }
    }
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

void GamepadUIBonusMapsPanel::Paint()
{
    BaseClass::Paint();

    GamepadUIBonusButton* pButton = GamepadUIBonusButton::GetLastBonusButton();
    if ( !pButton )
        return;

    const BonusMapDescription_t& desc = pButton->GetBonusMapDescription();

    // Draw description
    if (desc.szComment[0])
    {
        int nParentW, nParentH;
        GetParent()->GetSize( nParentW, nParentH );

        float flX = m_flFooterButtonsOffsetX + m_nFooterButtonWidth + m_flFooterButtonsSpacing;
        float flY = nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight;

        int nMaxWidth = 0;

        if (desc.m_Challenges.Count() > 0)
        {
            flY += m_flFooterMedalSize + m_flFooterButtonsSpacing;

            /*
            flX += m_flFooterMedalSize + (m_flFooterButtonsSpacing*4) + m_flFooterDescMedalSpace;

#ifdef STEAM_INPUT
            const bool bController = GamepadUI::GetInstance().GetSteamInput()->IsEnabled();
#elif defined(HL2_RETAIL) // Steam input and Steam Controller are not supported in SDK2013 (Madi)
            const bool bController = g_pInputSystem->IsSteamControllerActive();
#else
            const bool bController = (g_pInputSystem->GetJoystickCount() >= 1);
#endif
            if (bController)
            {
                // Need extra room for the extra button
                nMaxWidth -= m_nFooterButtonWidth + m_flFooterButtonsSpacing;
            }
            */
        }

        vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawSetTextFont( m_hDescFont );
        vgui::surface()->DrawSetTextPos( flX, flY );

        nMaxWidth += nParentW - flX - (m_flFooterButtonsOffsetX + m_nFooterButtonWidth + m_flFooterButtonsSpacing);

        GamepadUIString strComment( desc.szComment );
        DrawPrintWrappedText( m_hDescFont, flX, flY, strComment.String(), strComment.Length(), nMaxWidth, true);
    }

#if SDK_2013_HACKS
    if (desc.m_Challenges.Count() == 0)
#else
    if ( !desc.m_pChallenges )
#endif
        return;

    // Draw progress bar
    if (m_bHasChallenges)
    {
        float w = m_flBonusProgressOffsetX + m_flBonusProgressWidth;
        float h = m_flBonusProgressOffsetY + m_flBonusProgressHeight;
        vgui::surface()->DrawSetColor( m_colProgressBackgroundColor );
        vgui::surface()->DrawFilledRect( m_flBonusProgressOffsetX, m_flBonusProgressOffsetY, w, h );

        Color colFill = m_colProgressFillColor;
        for (int i = 0; i < 3; i++)
            colFill[i] *= m_flCompletionPerc;

        vgui::surface()->DrawSetColor( colFill );
        vgui::surface()->DrawFilledRect( m_flBonusProgressOffsetX, m_flBonusProgressOffsetY, m_flBonusProgressOffsetX + (m_flBonusProgressWidth * m_flCompletionPerc), h );

        vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawOutlinedRect( m_flBonusProgressOffsetX, m_flBonusProgressOffsetY, w, h );

        // Text
        wchar_t szWideBuff[64];
        V_snwprintf( szWideBuff, sizeof( szWideBuff ), L"%i%% %ls", (int)(100.0f * m_flCompletionPerc), g_pVGuiLocalize->Find( "#GameUI_BonusMapsCompletion" ) );

        int nTextW, nTextH;
        vgui::surface()->GetTextSize( m_hGoalFont, szWideBuff, nTextW, nTextH );

        vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawSetTextFont( m_hGoalFont );
        vgui::surface()->DrawSetTextPos( m_flBonusProgressOffsetX + (m_flBonusProgressWidth/2) - (nTextW/2), m_flBonusProgressOffsetY + (m_flBonusProgressHeight/2) - (nTextH/2) );
        vgui::surface()->DrawPrintText( szWideBuff, V_wcslen( szWideBuff ) );
    }

    int nCurrentChallenge = Clamp(gamepadui_selected_challenge.GetInt(), 0, NUM_CHALLENGES-1);

    int nNumChallenges = 0;
#if SDK_2013_HACKS
    for (const ChallengeDescription_t& challenge : desc.m_Challenges)
#else
    for (ChallengeDescription_t& challenge : *desc.m_pChallenges)
#endif
    {
        int iChallengeNum = challenge.iType != -1 ? challenge.iType : nNumChallenges;

        if ( iChallengeNum == nCurrentChallenge )
        {
            int iBest, iEarnedMedal, iNext, iNextMedal;
            GetChallengeMedals( &challenge, iBest, iEarnedMedal, iNext, iNextMedal );

            int nParentW, nParentH;
	        GetParent()->GetSize( nParentW, nParentH );

            char szBuff[256];
            if ( iEarnedMedal != -1 )
            {
                if (iChallengeNum < 10)
                    Q_snprintf(szBuff, 256, "vgui/medals/medal_0%i_%s", iChallengeNum, g_pszMedalNames[iEarnedMedal]);
                else
                    Q_snprintf(szBuff, 256, "vgui/medals/medal_%i_%s", iChallengeNum, g_pszMedalNames[iEarnedMedal]);
                if ( V_strcmp( m_szCachedMedalNames[0], szBuff))
                {
                    m_CachedMedals[ 0 ].SetImage( szBuff );
                    V_strcpy( m_szCachedMedalNames[0], szBuff);
                }
            }
            else
            {
                m_CachedMedals[0].Cleanup();
                V_strcpy(m_szCachedMedalNames[0], "");
            }

            if ( iNextMedal != -1 )
            {
                if (iChallengeNum < 10)
                    Q_snprintf(szBuff, 256, "vgui/medals/medal_0%i_%s", iChallengeNum, g_pszMedalNames[iNextMedal]);
                else
                    Q_snprintf(szBuff, 256, "vgui/medals/medal_%i_%s", iChallengeNum, g_pszMedalNames[iNextMedal]);
                if ( V_strcmp( m_szCachedMedalNames[1], szBuff))
                {
                    m_CachedMedals[ 1 ].SetImage( szBuff );
                    V_strcpy( m_szCachedMedalNames[1], szBuff);
                }
            }
            else
            {
                m_CachedMedals[1].Cleanup();
                V_strcpy(m_szCachedMedalNames[1], "");
            }

            float flX = m_flFooterButtonsOffsetX + m_nFooterButtonWidth + m_flFooterButtonsSpacing;
            float flY = nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight;

            // Josh: I should clean this later...
            int iBestWide = 0;
            int iBestTall = 0;
            if (iBest != -1)
            {
                char szBuff[256];
                wchar_t szWideBuff[256];
                wchar_t szWideBuff2[256];

#ifdef MAPBASE
                // Negative ints are used for hacks related to upwards goals
                if (iBest < 0)
                    iBest = -iBest;
#endif

                Q_snprintf(szBuff, sizeof(szBuff), "%i", iBest);
                g_pVGuiLocalize->ConvertANSIToUnicode(szBuff, szWideBuff2, sizeof(szWideBuff2));
                g_pVGuiLocalize->ConstructString(szWideBuff, sizeof(szWideBuff), g_pVGuiLocalize->Find("#GameUI_BonusMapsBest"), 1, szWideBuff2);

                if ( m_CachedMedals[0].IsValid() )
                {
                    vgui::surface()->DrawSetTexture( m_CachedMedals[ 0 ] );
                    vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
                    vgui::surface()->DrawTexturedRect( flX, flY, flX + m_flFooterMedalSize, flY + m_flFooterMedalSize );
                }

                vgui::surface()->DrawSetTextColor(Color(255, 255, 255, 255));
                vgui::surface()->DrawSetTextFont( m_hGoalFont );
                vgui::surface()->DrawSetTextPos( flX + m_flFooterMedalSize + m_flFooterButtonsSpacing, flY );
                vgui::surface()->GetTextSize( m_hGoalFont, szWideBuff, iBestWide, iBestTall);
                vgui::surface()->DrawPrintText( szWideBuff, V_wcslen(szWideBuff) );
            }

            int iNextWide = 0;
            int iNextTall = 0;
            if (iNext != -1)
            {
                char szBuff[256];
                wchar_t szWideBuff[256];
                wchar_t szWideBuff2[256];

#ifdef MAPBASE
                // Negative ints are used for hacks related to upwards goals
                if (iNext < 0)
                    iNext = -iNext;
#endif

                Q_snprintf(szBuff, sizeof(szBuff), "%i", iNext);
                g_pVGuiLocalize->ConvertANSIToUnicode(szBuff, szWideBuff2, sizeof(szWideBuff2));
                g_pVGuiLocalize->ConstructString(szWideBuff, sizeof(szWideBuff), g_pVGuiLocalize->Find("#GameUI_BonusMapsGoal"), 1, szWideBuff2);

                vgui::surface()->DrawSetTextColor(Color(255, 255, 255, 255));
                vgui::surface()->DrawSetTextFont( m_hGoalFont );
                vgui::surface()->DrawSetTextPos( flX + m_flFooterMedalSize + m_flFooterButtonsSpacing, flY + iBestTall );
                vgui::surface()->GetTextSize( m_hGoalFont, szWideBuff, iNextWide, iNextTall);
                vgui::surface()->DrawPrintText( szWideBuff, V_wcslen(szWideBuff) );

                if ( m_CachedMedals[1].IsValid() )
                {
                    vgui::surface()->DrawSetTexture( m_CachedMedals[ 1 ] );
                    vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
                    float flX2 = flX + m_flFooterMedalSize + Max(iNextWide, iBestWide) + 2 * m_flFooterButtonsSpacing;
                    float flY2 = flY + iBestTall;
                    vgui::surface()->DrawTexturedRect( flX2, flY2, flX2 + m_flFooterMedalNextSize, flY2 + m_flFooterMedalNextSize);
                }
            }

            return;
        }

        nNumChallenges++;
    }
}

void GamepadUIBonusMapsPanel::OnMouseWheeled( int nDelta )
{
    m_ScrollState.OnMouseWheeled( nDelta * m_BonusSpacing * 20.0f, GamepadUI::GetInstance().GetTime() );
}

#if SDK_2013_HACKS
void GamepadUIBonusMapsPanel::ScanBonusMaps()
{
    if (GetCurrentFolder() == NULL)
    {
        // Load bonus maps from the bonus maps manifest
        KeyValues *pBonusMapsManifest = new KeyValues( "BonusMapsManifest" );
        if (pBonusMapsManifest->LoadFromFile( g_pFullFileSystem, "scripts/bonus_maps_manifest.txt" ))
        {
            for (KeyValues *pSubKey = pBonusMapsManifest->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey())
            {
                if (Q_stricmp( pSubKey->GetName(), "dir" ) == 0)
                {
                    LoadBonusMapDir( pSubKey->GetString() );
                }
                else if (Q_stricmp( pSubKey->GetName(), "search" ) == 0)
                {
                    char szSearch[MAX_PATH];
                    Q_snprintf( szSearch, sizeof( szSearch ), "%s/*", pSubKey->GetString() );

                    FileFindHandle_t dirHandle;
                    const char *pszFileName = g_pFullFileSystem->FindFirst( szSearch, &dirHandle );
                    while (pszFileName)
                    {
                        if (g_pFullFileSystem->FindIsDirectory( dirHandle ))
                        {
                            char szBonusDir[MAX_PATH];
                            Q_snprintf( szBonusDir, sizeof( szBonusDir ), "%s/%s", pSubKey->GetString(), pszFileName );
                            LoadBonusMapDir( szBonusDir );
                        }
                        else if (Q_stricmp( Q_GetFileExtension( pszFileName ), "bns" ) == 0)
                        {
                            char szBonusDir[MAX_PATH];
                            Q_snprintf( szBonusDir, sizeof( szBonusDir ), "%s/%s", pSubKey->GetString(), pszFileName );
                            LoadBonusMap( szBonusDir );
                        }

                        pszFileName = g_pFullFileSystem->FindNext( dirHandle );
                    }
                    g_pFullFileSystem->FindClose( dirHandle );
                }
            }
        }
        pBonusMapsManifest->deleteThis();
    }
    else
    {
        // Just look in the folder's directory
        ScanBonusMapSubDir( GetCurrentFolder()->szFileName, GetCurrentFolder() );
    }

    // Load save data and modify each entry accordingly
    KeyValues *pBonusSaveData = new KeyValues( "BonusSaveData" );
    if (pBonusSaveData->LoadFromFile( g_pFullFileSystem, "save/bonus_maps_data.bmd" ))
    {
        KeyValues *pBonusFiles = pBonusSaveData->FindKey( "bonusfiles" );
        if (pBonusFiles)
        {
            for (KeyValues *pSubKey = pBonusFiles->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey())
            {
                // Look for each map and modify data based on what's saved
                for (KeyValues *pMap = pSubKey->GetFirstSubKey(); pMap != NULL; pMap = pMap->GetNextKey())
                {
                    for ( int iMapIndex = 0; iMapIndex < m_Bonuses.Count() && iMapIndex < MAX_LISTED_BONUS_MAPS; ++iMapIndex)
	                {
                        BonusMapDescription_t *pDesc = &m_Bonuses[iMapIndex];
                        if (Q_strcmp( pDesc->szMapName, pMap->GetName() ) != 0)
                            continue;

                        pDesc->bLocked = pMap->GetBool( "lock" );
                        pDesc->bComplete = pMap->GetBool( "complete" );

                        for (int iChallenge = 0; iChallenge < pDesc->m_Challenges.Count(); iChallenge++)
                        {
                            pDesc->m_Challenges[iChallenge].iBest = pMap->GetInt( pDesc->m_Challenges[iChallenge].szName, -1 );
                        }

                        break;
	                }
                }
            }
        }
    }
    pBonusSaveData->deleteThis();
}

void GamepadUIBonusMapsPanel::ScanBonusMapSubDir( const char *pszDir, BonusMapDescription_t *pParent )
{
    char szSearch[MAX_PATH];
    Q_snprintf( szSearch, sizeof( szSearch ), "%s/*", pszDir );

    FileFindHandle_t dirHandle;
    const char *pszFileName = g_pFullFileSystem->FindFirst( szSearch, &dirHandle );
    while (pszFileName)
    {
        if (g_pFullFileSystem->FindIsDirectory( dirHandle ))
        {
            if (pszFileName[0] != '.')
            {
                char szBonusDir[MAX_PATH];
                Q_snprintf( szBonusDir, sizeof( szBonusDir ), "%s/%s", pszDir, pszFileName );

                //BonusMapDescription_t *pFolder = NULL;
                LoadBonusMapDir( szBonusDir, pParent/*, &pFolder*/ );
                //ScanBonusMapSubDir( szBonusDir, pFolder );
            }
        }
        else if (Q_strcmp( Q_GetFileExtension( pszFileName ), "bns" ) == 0 && !Q_strstr(pszFileName, "folderinfo"))
        {
            char szBonusDir[MAX_PATH];
            Q_snprintf( szBonusDir, sizeof( szBonusDir ), "%s/%s", pszDir, pszFileName );
            LoadBonusMap( szBonusDir, pParent );
        }

        pszFileName = g_pFullFileSystem->FindNext( dirHandle );
    }
    g_pFullFileSystem->FindClose( dirHandle );
}

void GamepadUIBonusMapsPanel::LoadBonusMapDir( const char *pszDir, BonusMapDescription_t *pParent, BonusMapDescription_t **ppFolder )
{
    char szFolderName[MAX_PATH];
    Q_FileBase( pszDir, szFolderName, sizeof( szFolderName ) );

    char szFolderInfo[MAX_PATH];
    //char szMapList[MAX_PATH];
    Q_snprintf( szFolderInfo, sizeof( szFolderInfo ), "%s/folderinfo.bns", pszDir );
    //Q_snprintf( szMapList, sizeof( szMapList ), "%s/%s.bns", pszDir, szFolderName );

    KeyValues *pFolderInfoKV = new KeyValues( "FolderInfo" );
    if (!pFolderInfoKV->LoadFromFile( g_pFullFileSystem, szFolderInfo ))
    {
        pFolderInfoKV->deleteThis();
        return;
    }

    int iFolder = m_Bonuses.AddToTail();
    m_Bonuses[iFolder].bIsFolder = true;
    m_Bonuses[iFolder].bComplete = false;
    m_Bonuses[iFolder].bLocked = pFolderInfoKV->GetBool( "lock" );
    Q_strncpy( m_Bonuses[iFolder].szMapName, pFolderInfoKV->GetName(), sizeof( m_Bonuses[iFolder].szMapName ) );
    Q_strncpy( m_Bonuses[iFolder].szImageName, pFolderInfoKV->GetString( "image" ), sizeof( m_Bonuses[iFolder].szImageName ) );
    Q_strncpy( m_Bonuses[iFolder].szComment, pFolderInfoKV->GetString( "comment" ), sizeof( m_Bonuses[iFolder].szComment ) );
    Q_strncpy( m_Bonuses[iFolder].szFileName, pszDir, sizeof( m_Bonuses[iFolder].szFileName ) );

    m_Bonuses[iFolder].m_pParent = pParent;

    pFolderInfoKV->deleteThis();

    //LoadBonusMap( szMapList, &m_Bonuses[iFolder] );

    if (ppFolder)
    {
        *ppFolder = &m_Bonuses[iFolder];
    }
}

void GamepadUIBonusMapsPanel::LoadBonusMap( const char *pszMapList, BonusMapDescription_t *pParent )
{
    KeyValues *pMapListKV = new KeyValues( "MapList" );
    if (!pMapListKV->LoadFromFile( g_pFullFileSystem, pszMapList ))
    {
        pMapListKV->deleteThis();
        return;
    }

    for (KeyValues *pSubKey = pMapListKV; pSubKey != NULL; pSubKey = pSubKey->GetNextKey())
    {
        int i = m_Bonuses.AddToTail();

        m_Bonuses[i].bIsFolder = false;
        m_Bonuses[i].bComplete = false;
        m_Bonuses[i].bLocked = pSubKey->GetBool( "lock" );
        Q_strncpy( m_Bonuses[i].szMapName, pSubKey->GetName(), sizeof( m_Bonuses[i].szMapName ) );
        Q_strncpy( m_Bonuses[i].szMapFileName, pSubKey->GetString( "map" ), sizeof( m_Bonuses[i].szMapFileName ) );
        Q_strncpy( m_Bonuses[i].szImageName, pSubKey->GetString( "image" ), sizeof( m_Bonuses[i].szImageName ) );
        Q_strncpy( m_Bonuses[i].szComment, pSubKey->GetString( "comment" ), sizeof( m_Bonuses[i].szComment ) );
        Q_strncpy( m_Bonuses[i].szFileName, pszMapList, sizeof( m_Bonuses[i].szFileName ) );

        m_Bonuses[i].m_pParent = pParent;

        if (KeyValues *pChallenges = pSubKey->FindKey( "challenges" ))
        {
            for (KeyValues *pChallenge = pChallenges->GetFirstSubKey(); pChallenge != NULL; pChallenge = pChallenge->GetNextKey())
            {
                int i2 = m_Bonuses[i].m_Challenges.AddToTail();

                Q_strncpy( m_Bonuses[i].m_Challenges[i2].szName, pChallenge->GetName(), sizeof( m_Bonuses[i].m_Challenges[i2].szName ) );
                Q_strncpy( m_Bonuses[i].m_Challenges[i2].szComment, pChallenge->GetString("comment"), sizeof(m_Bonuses[i].m_Challenges[i2].szComment));
                m_Bonuses[i].m_Challenges[i2].iType = pChallenge->GetInt("type");
                m_Bonuses[i].m_Challenges[i2].iBronze = pChallenge->GetInt("bronze");
                m_Bonuses[i].m_Challenges[i2].iSilver = pChallenge->GetInt("silver");
                m_Bonuses[i].m_Challenges[i2].iGold = pChallenge->GetInt("gold");
            }
        }
    }
    pMapListKV->deleteThis();
}

void GamepadUIBonusMapsPanel::ClearBonusMapsList()
{
    m_Bonuses.Purge();
    m_ScrollState.SetScrollTarget( 0, 0 );
}

inline int GamepadUIBonusMapsPanel::BonusCount()
{
    return m_Bonuses.Count();
}

inline BonusMapDescription_t *GamepadUIBonusMapsPanel::GetBonusData( int iMapIndex )
{
    return &m_Bonuses[iMapIndex];
}

inline bool GamepadUIBonusMapsPanel::IsValidIndex( int iMapIndex )
{
    return m_Bonuses.IsValidIndex( iMapIndex );
}

inline BonusMapDescription_t *GamepadUIBonusMapsPanel::GetCurrentFolder()
{
    if (m_BonusFolderHierarchy.Count() > 0)
        return &m_BonusFolderHierarchy[0];
    return NULL;
}

inline void GamepadUIBonusMapsPanel::BackFolder()
{
    if (m_BonusFolderHierarchy.Count() > 0)
        m_BonusFolderHierarchy.Remove( 0 );
}

inline void GamepadUIBonusMapsPanel::EnterFolder( BonusMapDescription_t *pFolder )
{
    int i = m_BonusFolderHierarchy.AddToHead();
    memcpy( &m_BonusFolderHierarchy[i], pFolder, sizeof( m_BonusFolderHierarchy[i] ) );
    if (m_BonusFolderHierarchy.Count() > 1)
        m_BonusFolderHierarchy[i].m_pParent = &m_BonusFolderHierarchy.Element( i + 1 );
}

void GamepadUIBonusMapsPanel::SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold )
{
    GamepadUI::GetInstance().SetCurrentChallengeObjectives( iBronze, iSilver, iGold );
}

void GamepadUIBonusMapsPanel::SetCurrentChallengeNames( const char *pszFileName, const char *pszMapName, const char *pszChallengeName )
{
    GamepadUI::GetInstance().SetCurrentChallengeNames( pszFileName, pszMapName, pszChallengeName );
}
#endif

CON_COMMAND( gamepadui_openbonusmapsdialog, "" )
{
    new GamepadUIBonusMapsPanel( GamepadUI::GetInstance().GetBasePanel(), "" );
}

#endif // GAMEPADUI_ENABLE_BONUSMAPS
