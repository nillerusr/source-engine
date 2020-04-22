#include "gamepadui_interface.h"
#include "gamepadui_image.h"
#include "gamepadui_util.h"
#include "gamepadui_frame.h"
#include "gamepadui_scroll.h"
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

class GamepadUIBonusButton;

#define GAMEPADUI_BONUS_SCHEME GAMEPADUI_RESOURCE_FOLDER "schemebonusbutton.res"

ConVar gamepadui_selected_challenge("gamepadui_selected_challenge", "0", FCVAR_ARCHIVE);

const char g_pszMedalNames[4][8] =
{
	"none",
	"bronze",
	"silver",
	"gold"
};

void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal )
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
    int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, 2 );
    nChallenge = ( nChallenge + 1 ) % 3;
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

private:
    CUtlVector< GamepadUIBonusButton* > m_pBonusButtons;
    CUtlVector< BonusMapDescription_t > m_Bonuses;

    GamepadUIScrollState m_ScrollState;

    GAMEPADUI_PANEL_PROPERTY( float, m_BonusOffsetX, "Bonus.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_BonusOffsetY, "Bonus.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_BonusSpacing, "Bonus.Spacing", "0", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterMedalSize, "FooterMedal.Current.Size", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterMedalNextSize, "FooterMedal.Next.Size", "0", SchemeValueTypes::ProportionalFloat );

    vgui::HFont m_hGoalFont = vgui::INVALID_FONT;

    GamepadUIImage m_CachedMedals[2];
    char m_szCachedMedalNames[256][2];
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

        vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawSetTexture( m_Image );
        int imgH = ( w * 9 ) / 16;
        //vgui::surface()->DrawTexturedRect( 0, 0, w, );
        float offset = m_flTextOffsetYAnimationValue[ButtonStates::Out] - m_flTextOffsetY;
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

        int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, 2 );
        if ( m_Medals[ nChallenge ].IsValid() )
        {
            vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
            vgui::surface()->DrawSetTexture( m_Medals[ nChallenge ] );
            vgui::surface()->DrawTexturedRect( m_flMedalOffsetX, m_flMedalOffsetY, m_flMedalOffsetX + m_flMedalSize, m_flMedalOffsetY + m_flMedalSize );
            vgui::surface()->DrawSetTexture( 0 );
        }
    }
	
    void ApplySchemeSettings( vgui::IScheme *pScheme )
    {
        BaseClass::ApplySchemeSettings( pScheme );

        if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
        {
            float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();

            // For now, undo the scaling from base class
            m_flWidth /= flScreenRatio;
            for (int i = 0; i < ButtonStates::Count; i++)
                m_flWidthAnimationValue[i] /= flScreenRatio;

            SetSize( m_flWidth, m_flHeight + m_flExtraHeight );
            DoAnimations( true );
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
        int nChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, 2 );
        return m_BonusMapDesc.bComplete || bCompleted[ nChallenge ];
    }

    void SetBonusMapDescription( BonusMapDescription_t* pDesc )
    {
        for ( auto& medal : m_Medals )
            medal.Cleanup();
        m_BonusMapDesc = *pDesc;

        if ( !m_BonusMapDesc.m_pChallenges )
            return;

        int nNumChallenges = 0;
        for ( ChallengeDescription_t& challenge : *m_BonusMapDesc.m_pChallenges )
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

    static GamepadUIBonusButton* GetLastBonusButton() { return s_pLastBonusButton; }

private:
    GamepadUIImage m_Image;
    GamepadUIImage m_Medals[3];
    GamepadUIImage m_LockIcon;

    bool bCompleted[3];

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
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    FooterButtonMask buttons = FooterButtons::Back | FooterButtons::Select;
    SetFooterButtons( buttons, FooterButtons::Select );

    BuildMapsList();
    Activate();

    UpdateGradients();
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

    if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
    {
        float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();
        m_BonusOffsetX *= flScreenRatio;
    }
}

void GamepadUIBonusMapsPanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    // TODO: Scroll
#if 0
    GamepadUIButton *pButton = dynamic_cast< GamepadUIButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        int nParentW, nParentH;
	    GetParent()->GetSize( nParentW, nParentH );

        int nX, nY;
        pButton->GetPos( nX, nY );
        if ( nX + pButton->m_flWidth > nParentW || nX < 0 )
        {
            int nTargetX = pButton->GetPriority() * (pButton->m_flWidth + m_BonusSpacing);

            if ( nX < nParentW / 2 )
            {
                nTargetX += nParentW - m_BonusOffsetX;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetX -= m_BonusSpacing;
            }
            else
            {
                nTargetX += pButton->m_flWidth;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetX += (pButton->m_flWidth / 2) + m_BonusSpacing;
            }


            m_ScrollState.SetScrollTarget( nTargetX - ( nParentW - m_BonusOffsetX ), GamepadUI::GetInstance().GetTime() );
        }
    }
#endif
}

#define MAX_LISTED_BONUS_MAPS 128

void GamepadUIBonusMapsPanel::BuildMapsList()
{
    m_pBonusButtons.PurgeAndDeleteElements();

    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->ClearBonusMapsList();
    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->ScanBonusMaps();

    char szDisplayPath[_MAX_PATH];
	Q_snprintf( szDisplayPath, _MAX_PATH, "%s/", GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath() );

    //bool bIsRoot = !Q_strcmp( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), "." );
    //if ( bIsRoot )
        GetFrameTitle() = GamepadUIString( "#GameUI_BonusMaps" );
    //else
    //    GetFrameTitle() = GamepadUIString( szDisplayPath );

	int iMapCount = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->BonusCount();

    bool bHasChallenges = false;

	// add to the list
	for ( int iMapIndex = 0; iMapIndex < iMapCount && iMapIndex < MAX_LISTED_BONUS_MAPS; ++iMapIndex )
	{
        BonusMapDescription_t *pDesc = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetBonusData( iMapIndex );

        char szImage[MAX_PATH];
        V_snprintf( szImage, sizeof( szImage ), "vgui/%s", pDesc->szImageName );

        if ( pDesc->m_pChallenges )
            bHasChallenges = true;

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

    if ( m_pBonusButtons.Count() > 0 )
        m_pBonusButtons[0]->NavigateTo();

    for ( int i = 1; i < m_pBonusButtons.Count(); i++)
    {
        m_pBonusButtons[i]->SetNavLeft(m_pBonusButtons[i - 1]);
        m_pBonusButtons[i - 1]->SetNavRight(m_pBonusButtons[i] );
    }

    FooterButtonMask buttons = FooterButtons::Back | FooterButtons::Select;
    if (bHasChallenges)
        buttons |= FooterButtons::Challenge;
    SetFooterButtons( buttons, FooterButtons::Select );
}

void GamepadUIBonusMapsPanel::LayoutBonusButtons()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    float flScrollClamp = 0.0f;
    for ( int i = 0; i < m_pBonusButtons.Count(); i++ )
    {
        int nSize = ( m_pBonusButtons[0]->GetWide() + m_BonusSpacing );

        if ( i < m_pBonusButtons.Count() - 2 )
            flScrollClamp += nSize;
    }

    m_ScrollState.UpdateScrollBounds( 0.0f, flScrollClamp );

    int x = m_BonusOffsetX;
    int y = m_BonusOffsetY;
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

    m_ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUIBonusMapsPanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
        bool bIsRoot = !Q_strcmp( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), "." );
        if ( bIsRoot )
            Close();
        else
        {
            GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->BackPath();

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
		    if ( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->IsValidIndex( mapIndex ) )
		    {
			    BonusMapDescription_t *pBonusMap = GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetBonusData( mapIndex );

			    // Don't do anything with locked items
			    if ( pBonusMap->bLocked )
				    return;

			    const char *shortName = pBonusMap->szShortName;
			    if ( shortName && shortName[ 0 ] )
			    {
				    if ( pBonusMap->bIsFolder )
				    {
					    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->AppendPath( shortName );

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
                            if ( desc.m_pChallenges )
                            {
                                iChallenge = Clamp( gamepadui_selected_challenge.GetInt(), 0, 2 ) + 1;
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

						    ChallengeDescription_t *pChallengeDescription = &((*pBonusMap->m_pChallenges)[ iChallenge - 1 ]);

						    // Set up medal goals
						    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->SetCurrentChallengeObjectives( pChallengeDescription->iBronze, pChallengeDescription->iSilver, pChallengeDescription->iGold );
						    GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->SetCurrentChallengeNames( pBonusMap->szFileName, pBonusMap->szMapName, pChallengeDescription->szName );
					    }

					    if ( pBonusMap->szMapFileName[ 0 ] != '.' )
					    {
						    Q_snprintf( sz, sizeof( sz ), "map %s\n", pBonusMap->szMapFileName );
					    }
					    else
					    {
						    const char *pchSubDir = Q_strnchr( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath(), '/', Q_strlen( GamepadUI::GetInstance().GetGameUI()->GetBonusMapsDatabase()->GetPath() ) );

						    if ( pchSubDir )
						    {
							    pchSubDir = Q_strnchr( pchSubDir + 1, '/', Q_strlen( pchSubDir ) );

							    if ( pchSubDir )
							    {
								    ++pchSubDir;
								    const char *pchMapFileName = pBonusMap->szMapFileName + 2;
								    Q_snprintf( sz, sizeof( sz ), "map %s/%s\n", pchSubDir, pchMapFileName );
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
    if ( !desc.m_pChallenges )
        return;

    int nCurrentChallenge = Clamp(gamepadui_selected_challenge.GetInt(), 0, 2);

    int nNumChallenges = 0;
    for (ChallengeDescription_t& challenge : *desc.m_pChallenges)
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
    // TODO: Scroll
    //m_ScrollState.OnMouseWheeled( nDelta * m_BonusSpacing * 20.0f, GamepadUI::GetInstance().GetTime() );
}

CON_COMMAND( gamepadui_openbonusmapsdialog, "" )
{
    new GamepadUIBonusMapsPanel( GamepadUI::GetInstance().GetBasePanel(), "" );
}

#endif // GAMEPADUI_ENABLE_BONUSMAPS
