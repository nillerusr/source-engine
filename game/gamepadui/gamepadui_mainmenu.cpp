#include "gamepadui_interface.h"
#include "gamepadui_basepanel.h"
#include "gamepadui_mainmenu.h"

//#include "..\client\cdll_client_int.h"

#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IVGui.h"

#include "KeyValues.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"

#define GAMEPADUI_MAINMENU_SCHEME GAMEPADUI_RESOURCE_FOLDER "schememainmenu.res"
#define GAMEPADUI_MAINMENU_FILE GAMEPADUI_RESOURCE_FOLDER "mainmenu.res"

#ifdef GAMEPADUI_GAME_EZ2
ConVar gamepadui_show_ez2_version( "gamepadui_show_ez2_version", "1", FCVAR_NONE, "Show E:Z2 version in menu" );
ConVar gamepadui_show_old_ui_button( "gamepadui_show_old_ui_button", "1", FCVAR_NONE, "Show button explaining how to switch to the old UI (Changes may not take effect until changing level)" );
#endif

#define BUTTONS_DEVIDE_SIZE 2

float m_flButtonsRealOffsetX = -500;
float m_flButtonsRealAlpha = 0;
int m_flButtonsAlpha = 255;

float m_flLogoRealOffsetX = -500;
int m_flLogoAlpha = 255;

bool ResetFade = false;

int LogoSizeX, LogoSizeY;

int nMaxLogosW = 0, nTotalLogosH = 0;

float TimeDelta = 0;
float LastTime = 0;

float TimeDeltaLogo = 0;
float LastTimeLogo = 0;

float curtime = 0;

void CC_ResetFade()
{
    ResetFade = true;
}

ConCommand gamepadui_resetfade("gamepadui_resetfade", CC_ResetFade);

GamepadUIMainMenu::GamepadUIMainMenu( vgui::Panel* pParent )    
    : BaseClass( pParent, "MainMenu" )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_MAINMENU_SCHEME, "SchemeMainMenu" );
    SetScheme( hScheme );

    KeyValues* pModData = new KeyValues( "ModData" );
    if ( pModData )
    {
        if ( pModData->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
        {
            m_LogoText[ 0 ].SetText(pModData->GetString( "gamepadui_title", pModData->GetString( "title" ) ) );
            m_LogoText[ 1 ].SetText(pModData->GetString( "gamepadui_title2", pModData->GetString( "title2" ) ) );
        }
        pModData->deleteThis();
    }

    LoadMenuButtons();

    TimeDelta = 0;
    LastTime = GamepadUI::GetInstance().GetEngineClient()->Time();

    SetFooterButtons( FooterButtons::Select, FooterButtons::Select );
}

void GamepadUIMainMenu::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
//#if defined(GAMEPADUI_GAME_EZ2) //enabled in city52 as well
    // E:Z2 reduces the gradient so that the background map can be more easily seen
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Left, { 1.0f, GamepadUI::GetInstance().IsInBackgroundLevel() ? 0 : 0.666f }, flTime );
//#else
//    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Left, { 1.0f, 0.666f }, flTime );
//#endif

    // In case a controller is added mid-game
    SetFooterButtons( FooterButtons::Select, FooterButtons::Select );
}

void GamepadUIMainMenu::LoadMenuButtons()
{


    KeyValues* pDataFile = new KeyValues( "MainMenuScript" );
    if ( pDataFile )
    {
        if ( pDataFile->LoadFromFile( g_pFullFileSystem, GAMEPADUI_MAINMENU_FILE ) )
        {
            for ( KeyValues* pData = pDataFile->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
            {
                GamepadUIButton* pButton = new GamepadUIButton(
                    this, this,
                    GAMEPADUI_MAINMENU_SCHEME,
                    pData->GetString( "command" ),
                    pData->GetString( "text", "Sample Text" ),
                    pData->GetString( "description", "" ) );
                pButton->SetName( pData->GetName() );
                pButton->SetPriority( V_atoi( pData->GetString( "priority", "0" ) ) );
                pButton->SetVisible( true );

                //pButton->SetSize(pButton->GetWide() / BUTTONS_DEVIDE_SIZE , pButton->GetTall() / BUTTONS_DEVIDE_SIZE);

                const char* pFamily = pData->GetString( "family", "all" );
                if ( !V_strcmp( pFamily, "ingame" ) || !V_strcmp( pFamily, "all" ) )
                    m_Buttons[ GamepadUIMenuStates::InGame ].AddToTail( pButton );
                if ( !V_strcmp( pFamily, "mainmenu" ) || !V_strcmp( pFamily, "all" ) )
                    m_Buttons[ GamepadUIMenuStates::MainMenu ].AddToTail( pButton );
            }
        }

        pDataFile->deleteThis();
    }

#ifdef GAMEPADUI_GAME_EZ2
    {
        m_pSwitchToOldUIButton = new GamepadUIButton(
                        this, this,
                        GAMEPADUI_RESOURCE_FOLDER "schememainmenu_olduibutton.res",
                        "cmd gamepadui_opengenerictextdialog #GameUI_SwitchToOldUI_Title #GameUI_SwitchToOldUI_Info 1",
                        "#GameUI_GameMenu_SwitchToOldUI", "" );
        m_pSwitchToOldUIButton->SetPriority( 0 );
        m_pSwitchToOldUIButton->SetVisible( true );
    }
#endif

    UpdateButtonVisibility();
}

void GamepadUIMainMenu::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    /*float flX, flY;
    if (GamepadUI::GetInstance().GetScreenRatio(flX, flY))
    {
        m_flButtonsOffsetX *= (flX * flX);
    }

    int nX, nY;
    GamepadUI::GetInstance().GetSizingPanelOffset(nX, nY);
    if (nX > 0)
    {
        GamepadUI::GetInstance().GetSizingPanelScale(flX, flY);
        m_flButtonsOffsetX += ((float)nX) * flX * 0.5f;
    }*/

    


    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );
    SetBounds( 0, 0, nParentW, nParentH );

    const char *pImage = pScheme->GetResourceString( "Logo.Image" );

    Msg(pImage);

    if (pImage && *pImage)
    {
        m_LogoImage.SetImage(pImage);
        m_LogoImage.GetImageSize(LogoSizeX, LogoSizeY);
    }
    m_hLogoFont = pScheme->GetFont( "Logo.Font", true );

#ifdef GAMEPADUI_GAME_EZ2
    m_hVersionFont = pScheme->GetFont( "Version.Font", true );

    ConVarRef ez2_version( "ez2_version" );
    m_strEZ2Version = ez2_version.GetString();
#endif

    if (m_flButtonsStartOffsetX == 0)
        m_flButtonsRealOffsetX = m_flButtonsStartOffsetX = m_flButtonsOffsetX;
    else
        m_flButtonsRealOffsetX = m_flButtonsStartOffsetX;

    m_flButtonsRealAlpha = 0;

    if (m_flLogoStartOffsetX == 0)
        m_flLogoRealOffsetX = m_flLogoOffsetX;
    else
        m_flLogoRealOffsetX = m_flLogoStartOffsetX;
}

int LogoID;

void GamepadUIMainMenu::LayoutMainMenu()
{
    int nY = GetCurrentButtonOffset();
    CUtlVector<GamepadUIButton*>& currentButtons = GetCurrentButtons();


    //HACK if we have more than 0.7 sec of delay between frames, we possybly lagging
    if (GamepadUI::GetInstance().GetTime() - LastTime <= 0.7f)
        TimeDelta = GamepadUI::GetInstance().GetTime() - LastTime;

    curtime += TimeDelta;
    //curtime = clamp(curtime, 0, m_flButtonsAnimTime); 



    int i = 0;
    for ( GamepadUIButton *pButton : currentButtons )
    {

        nY += pButton->GetTall();
        pButton->SetPos( m_flButtonsRealOffsetX, GetTall() - nY );
        pButton->SetAlpha(m_flButtonsRealAlpha);

        //FIXME ive tried to make this anim seperatly for each button so they whould move next to each other,
        //but the life planned other plans, so when im subtracting 0.5*i it stopping them at some distance and they dont go all the way.
        float TdC;

        float func;


        if (GetCurrentMenuState() == GamepadUIMenuStates::InGame && !GamepadUI::GetInstance().IsInBackgroundLevel())
        {
            TdC = clamp(curtime, 0, m_flButtonsAnimTimeInGame);
            func = clamp(pow((TdC / m_flButtonsAnimTimeInGame),
                m_flButtonsAnimPowerInGame) / (pow((TdC / m_flButtonsAnimTimeInGame),
                m_flButtonsAnimPowerInGame) + pow(1 - (TdC / m_flButtonsAnimTimeInGame),
                m_flButtonsAnimPowerInGame)), 0, 1);
        }
        else
        {
            TdC = clamp(curtime, 0, m_flButtonsAnimTime);
            func = clamp(pow((TdC / m_flButtonsAnimTime),
                m_flButtonsAnimPower) / (pow((TdC / m_flButtonsAnimTime),
                m_flButtonsAnimPower) + pow(1 - (TdC / m_flButtonsAnimTime),
                m_flButtonsAnimPower)), 0, 1);
        }

        m_flButtonsRealOffsetX = RemapVal(func, 0, 1, m_flButtonsStartOffsetX, m_flButtonsOffsetX);
        m_flButtonsRealAlpha = RemapVal(func, 0, 1, 0, m_flButtonsAlpha);

        //pButton->SetPos( m_flButtonsOffsetX, GetTall() - nY );
        i++;
        nY += m_flButtonSpacing;
    }



    
    LastTime = GamepadUI::GetInstance().GetTime();

    
   


#ifdef GAMEPADUI_GAME_EZ2
    if ( m_pSwitchToOldUIButton && m_pSwitchToOldUIButton->IsVisible() )
    {
        int nParentW, nParentH;
        GetParent()->GetSize( nParentW, nParentH );

        m_pSwitchToOldUIButton->SetPos( m_flOldUIButtonOffsetX, nParentH - m_pSwitchToOldUIButton->m_flHeight - m_flOldUIButtonOffsetY );
    }
#endif
}

void GamepadUIMainMenu::PaintLogo()
{
#ifdef GAMEPADUI_GAME_HL2 //a little hack to make default hl2 logo be yellow and still be able to change it later in res file
    vgui::surface()->DrawSetTextColor(m_colLogoNewColor);
#else
    vgui::surface()->DrawSetTextColor(m_colLogoColor);
#endif
    vgui::surface()->DrawSetTextFont( m_hLogoFont );

    int nMaxLogosW = 0, nTotalLogosH = 0;
    int nLogoW[ 2 ], nLogoH[ 2 ];
    for ( int i = 0; i < 2; i++ )
    {
        nLogoW[ i ] = 0;
        nLogoH[ i ] = 0;
        if ( !m_LogoText[ i ].IsEmpty() )
            vgui::surface()->GetTextSize( m_hLogoFont, m_LogoText[ i ].String(), nLogoW[ i ], nLogoH[ i ] );
        nMaxLogosW = Max( nLogoW[ i ], nMaxLogosW );
        nTotalLogosH += nLogoH[ i ];
    }

    int nLogoY = GetTall() - ( GetCurrentLogoOffset() + nTotalLogosH );

    if ( m_LogoImage.IsValid() )
    {
        int nY1 = nLogoY;
        int nY2 = nY1 + /*nLogoH[0]*/ m_flLogoSizeY;
        //int nX1 = m_flLogoOffsetX;

        int nX1 = m_flLogoRealOffsetX;
        int nX2 = nX1 + /*(nLogoH[0] * 3)*/ m_flLogoSizeX;

        vgui::surface()->DrawSetColor(Color(255, 255, 255, 255));
        vgui::surface()->DrawSetTexture(m_LogoImage);
        vgui::surface()->DrawTexturedRect(nX1, nY1, nX2, nY2);
        vgui::surface()->DrawSetTexture(0);

        //TimeDeltaLogo = GamepadUI::GetInstance().GetTime() - LastTimeLogo;
        //TimeDeltaLogo = clamp(TimeDeltaLogo, 0.2, 0.5);

        float TdC;

        //m_flLogoRealOffsetX = Lerp<float>(m_flLogoLerp * TimeDeltaLogo, m_flLogoRealOffsetX, m_flLogoOffsetX);
        float func;

        if (GetCurrentMenuState() == GamepadUIMenuStates::InGame)
        {
            TdC = clamp(curtime, 0, m_flLogoAnimTimeInGame);
            func = clamp(pow((TdC / m_flLogoAnimTimeInGame), 
                m_flLogoAnimPowerInGame) / (pow((TdC / m_flLogoAnimTimeInGame), 
                m_flLogoAnimPowerInGame) + pow(1 - (TdC / m_flLogoAnimTimeInGame), 
                m_flLogoAnimPowerInGame)), 0, 1);
        }
        else
        {
            TdC = clamp(curtime, 0, m_flLogoAnimTime);
            func = clamp(pow((TdC / m_flLogoAnimTime),
                m_flLogoAnimPower) / (pow((TdC / m_flLogoAnimTime),
                m_flLogoAnimPower) + pow(1 - (TdC / m_flLogoAnimTime),
                m_flLogoAnimPower)), 0, 1);
        }

        m_flLogoRealOffsetX = RemapVal(func, 0, 1, -m_flLogoSizeX, m_flLogoOffsetX);

        //LastTimeLogo = GamepadUI::GetInstance().GetTime();

    }
    else
    {
        for ( int i = 1; i >= 0; i-- )
        {
            vgui::surface()->DrawSetTextPos( m_flLogoOffsetX, nLogoY );
            int aboba = m_LogoText[i].Length();
            vgui::surface()->DrawPrintText( m_LogoText[ i ].String(), aboba);

            nLogoY -= nLogoH[ i ];
        }
    }

#ifdef GAMEPADUI_GAME_EZ2
    if (gamepadui_show_ez2_version.GetBool() && !m_strEZ2Version.IsEmpty())
    {
        int nVersionW, nVersionH;
        vgui::surface()->GetTextSize( m_hVersionFont, m_strEZ2Version.String(), nVersionW, nVersionH );

        vgui::surface()->DrawSetTextColor( m_colVersionColor );
        vgui::surface()->DrawSetTextFont( m_hVersionFont );
        vgui::surface()->DrawSetTextPos( m_flLogoOffsetX + m_flVersionOffsetX + nLogoW[0], nLogoY + (nLogoH[0] * 2) - nVersionH);
        vgui::surface()->DrawPrintText( m_strEZ2Version.String(), m_strEZ2Version.Length() );
    }
#endif
}

void GamepadUIMainMenu::OnThink()
{
    BaseClass::OnThink();

    LayoutMainMenu();

}

void GamepadUIMainMenu::Paint()
{

    BaseClass::Paint();

    if (ResetFade)
        {
            curtime = 0;

            if (m_flButtonsStartOffsetX == 0)
                m_flButtonsRealOffsetX = m_flButtonsStartOffsetX = m_flButtonsOffsetX;
            else
                m_flButtonsRealOffsetX = m_flButtonsStartOffsetX;

            m_flButtonsRealAlpha = 0;

            if (m_flLogoStartOffsetX == 0)
                m_flLogoRealOffsetX = m_flLogoOffsetX;
            else
                m_flLogoRealOffsetX = m_flLogoStartOffsetX;

            ResetFade = false;
        }

    PaintLogo();
    
}

void GamepadUIMainMenu::OnCommand( char const* pCommand )
{
    if ( StringHasPrefixCaseSensitive( pCommand, "cmd " ) )
    {
        const char* pszClientCmd = &pCommand[ 4 ];
        if ( *pszClientCmd )
            GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( pszClientCmd );

        // This is a hack to reset bonus challenges in the event that the player disconnected before the map loaded.
        // We have no known way of detecting that event and differentiating between a bonus level and non-bonus level being loaded,
        // so for now, we just reset this when the player presses any menu button, as that indicates they are in the menu and no longer loading a bonus level
        // (note that this does not cover loading a map through other means, like through the console)
        ConVarRef sv_bonus_challenge( "sv_bonus_challenge" );
        if (sv_bonus_challenge.GetInt() != 0)
        {
            GamepadUI_Log( "Resetting sv_bonus_challenge\n" );
            sv_bonus_challenge.SetValue( 0 );
        }
    }
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

void GamepadUIMainMenu::OnSetFocus()
{
    BaseClass::OnSetFocus();
    OnMenuStateChanged();
}

void GamepadUIMainMenu::OnMenuStateChanged()
{
    UpdateGradients();
    UpdateButtonVisibility();
}

void GamepadUIMainMenu::UpdateButtonVisibility()
{

    for ( CUtlVector<GamepadUIButton*>& buttons : m_Buttons )
    {
        for ( GamepadUIButton* pButton : buttons )
        {
            pButton->NavigateFrom();
            pButton->SetVisible( false );
        }
    }

    CUtlVector<GamepadUIButton*>& currentButtons = GetCurrentButtons();
    currentButtons.Sort( []( GamepadUIButton* const* a, GamepadUIButton* const* b ) -> int
    {
        return ( ( *a )->GetPriority() > ( *b )->GetPriority() );
    });

    for ( int i = 1; i < currentButtons.Count(); i++ )
    {
        currentButtons[i]->SetNavDown( currentButtons[i - 1] );
        currentButtons[i - 1]->SetNavUp( currentButtons[i] );
    }

    for ( GamepadUIButton* pButton : currentButtons )
        pButton->SetVisible( true );

    if ( !currentButtons.IsEmpty() )
        currentButtons[ currentButtons.Count() - 1 ]->NavigateTo();

#ifdef GAMEPADUI_GAME_EZ2
    if ( m_pSwitchToOldUIButton )
    {
        if ( (!GamepadUI::GetInstance().GetSteamInput() || !GamepadUI::GetInstance().GetSteamInput()->IsSteamRunningOnSteamDeck()) && gamepadui_show_old_ui_button.GetBool() )
        {
            m_pSwitchToOldUIButton->SetVisible( true );

            if (!currentButtons.IsEmpty())
            {
                currentButtons[ 0 ]->SetNavDown( m_pSwitchToOldUIButton );
                m_pSwitchToOldUIButton->SetNavUp( currentButtons[0] );
            }
        }
        else
        {
            m_pSwitchToOldUIButton->SetVisible( false );
        }
    }
#endif
}

void GamepadUIMainMenu::OnKeyCodeReleased( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );
    switch (buttonCode)
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    case STEAMCONTROLLER_B:
#endif

    case KEY_XBUTTON_B:
        if ( GamepadUI::GetInstance().IsInLevel() )
        {
            GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( "gamemenucommand resumegame" );
            // I tried it and didn't like it.
            // Oh well.
            //vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
        }
        break;
    default:
        BaseClass::OnKeyCodeReleased( code );
        break;
    }
}

GamepadUIMenuState GamepadUIMainMenu::GetCurrentMenuState() const
{
    if ( GamepadUI::GetInstance().IsInLevel() )
        return GamepadUIMenuStates::InGame;
    return GamepadUIMenuStates::MainMenu;
}

CUtlVector<GamepadUIButton*>& GamepadUIMainMenu::GetCurrentButtons()
{
    return m_Buttons[ GetCurrentMenuState() ];
}

float GamepadUIMainMenu::GetCurrentButtonOffset()
{
    return GetCurrentMenuState() == GamepadUIMenuStates::InGame ? m_flButtonsOffsetYInGame : m_flButtonsOffsetYMenu;
}

float GamepadUIMainMenu::GetCurrentLogoOffset()
{
    return GetCurrentMenuState() == GamepadUIMenuStates::InGame ? m_flLogoOffsetYInGame : m_flLogoOffsetYMenu;
}
