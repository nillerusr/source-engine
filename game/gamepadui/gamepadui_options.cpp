#include "gamepadui_interface.h"
#include "gamepadui_button.h"
#include "gamepadui_frame.h"
#include "gamepadui_scroll.h"
#include "gamepadui_image.h"
#include "gamepadui_genericconfirmation.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"

#include "vgui_controls/ComboBox.h"

#include "KeyValues.h"
#include "filesystem.h"
#include "utlbuffer.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/materialsystem_config.h"

#if defined( USE_SDL )
#include "SDL.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const int MAX_OPTIONS_TABS = 8;

#define GAMEPADUI_OPTIONS_FILE GAMEPADUI_RESOURCE_FOLDER "options.res"

class GamepadUIWheelyWheel;
void OnResolutionsNeedUpdate( IConVar *var, const char *pOldValue, float flOldValue );

ConVar _gamepadui_water_detail( "_gamepadui_water_detail", "0" );
ConVar _gamepadui_shadow_detail( "_gamepadui_shadow_detail", "0" );
ConVar _gamepadui_antialiasing( "_gamepadui_antialiasing", "0" );
ConVar _gamepadui_aspectratio( "_gamepadui_aspectratio", "0", FCVAR_NONE, "", OnResolutionsNeedUpdate );
ConVar _gamepadui_displaymode( "_gamepadui_displaymode", "0", FCVAR_NONE, "", OnResolutionsNeedUpdate );
ConVar _gamepadui_resolution( "_gamepadui_resolution", "0" );
ConVar _gamepadui_sound_quality( "_gamepadui_sound_quality", "0" );
ConVar _gamepadui_closecaptions( "_gamepadui_closecaptions", "0" );
#ifdef HL2_RETAIL
ConVar _gamepadui_hudaspect( "_gamepadui_hudaspect", "0" );
#endif
ConVar _gamepadui_skill( "_gamepadui_skill", "0" );

struct GamepadUITab
{
    GamepadUIButton *pTabButton;
    CUtlVector< GamepadUIButton* > pButtons;
    GamepadUIScrollState ScrollState;
    bool bAlternating;
    bool bHorizontal;
    CUtlVector<CUtlSymbol> KeysToUnbind;
};

class GamepadUIOptionsPanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUIOptionsPanel, GamepadUIFrame );

public:
    GamepadUIOptionsPanel( vgui::Panel *pParent, const char* pPanelName );
    ~GamepadUIOptionsPanel();

    void OnThink() OVERRIDE;
    void Paint() OVERRIDE;
    void LayoutCurrentTab();
    void OnCommand( char const* pCommand ) OVERRIDE;

    void UpdateGradients() OVERRIDE;

    void LoadOptionTabs( const char* pszOptionsFile );
	
    void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;

    void SetActiveTab( int nTab );
    int GetActiveTab();
    void OnMouseWheeled( int delta ) OVERRIDE;

    void UpdateResolutions();
    void ClearBindings();
    void FillInBindings();
    void ApplyKeyBindings();

    void OnKeyBound( const char *pKey );

    void OnKeyCodePressed( vgui::KeyCode code );

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

    static GamepadUIOptionsPanel *GetInstance()
    {
        return s_pOptionsPanel;
    }

    GamepadUIWheelyWheel *GetResolutionButton()
    {
        return m_pResolutionButton;
    }

private:

    GAMEPADUI_PANEL_PROPERTY( float, m_flTabsOffsetX, "Tabs.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flTabsOffsetY, "Tabs.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flOptionsFade, "Options.Fade", "80", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flScrollBarOffsetX, "Scrollbar.OffsetX", "10", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flScrollBarWidth, "Scrollbar.Width", "80", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flScrollBarHeight, "Scrollbar.Height", "80", SchemeValueTypes::ProportionalFloat );

    GamepadUITab m_Tabs[ MAX_OPTIONS_TABS ];
    int m_nTabCount = 0;

    GamepadUIWheelyWheel* m_pResolutionButton = NULL;

    GamepadUIGlyph m_leftGlyph;
    GamepadUIGlyph m_rightGlyph;

    static GamepadUIOptionsPanel *s_pOptionsPanel;
};

GamepadUIOptionsPanel* GamepadUIOptionsPanel::s_pOptionsPanel = NULL;

ConVar gamepadui_last_options_tab( "gamepadui_last_options_tab", "0", FCVAR_ARCHIVE );

class GamepadUICheckButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUICheckButton, GamepadUIButton );

    GamepadUICheckButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const char *pText, const char *pDescription )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
    {
    }

    virtual void ApplySchemeSettings( vgui::IScheme* pScheme )
    {
        BaseClass::ApplySchemeSettings( pScheme );

        m_hIconFont = pScheme->GetFont( "Button.Check.Font", true );
    }

    virtual void Paint()
    {
        BaseClass::Paint();

        vgui::surface()->DrawSetTextColor( m_colTextColor );
        vgui::surface()->DrawSetTextFont( m_hIconFont );
        vgui::surface()->DrawSetTextPos( m_flCheckOffsetX, m_flHeight / 2 - m_flCheckHeight / 2 );
        vgui::surface()->DrawPrintText( L"j", 1 );
        vgui::surface()->DrawSetTextPos( m_flCheckOffsetX, m_flHeight / 2 - m_flCheckHeight / 2 );
        vgui::surface()->DrawPrintText( L"k", 1 );
    }

private:

    GAMEPADUI_PANEL_PROPERTY( float, m_flCheckOffsetX, "Button.Check.OffsetX", "10", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flCheckHeight, "Button.Check.Height", "18", SchemeValueTypes::ProportionalFloat );

    vgui::HFont m_hIconFont = vgui::INVALID_FONT;
};

struct GamepadUIOption
{
    GamepadUIString strOptionText;
    int nValue = 0;

    union
    {
        struct
        {
            int nWidth;
            int nHeight;
        };
        void *pData;
    } userdata;
};

class GamepadUIKeyButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIKeyButton, GamepadUIButton );

    GamepadUIKeyButton( const char *pszBinding, vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const char *pText, const char *pDescription )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_szBinding( pszBinding )
    {
    }

    void Paint() OVERRIDE
    {
        BaseClass::Paint();

        if ( m_bBeingBound )
        {
            vgui::surface()->DrawSetColor( m_colBinding );
            vgui::surface()->DrawFilledRect( m_flWidth - m_flTextOffsetX - m_flBindingWidth, m_flHeight / 2 - m_flBindingHeight / 2, m_flWidth - m_flTextOffsetX, m_flHeight / 2 + m_flBindingHeight / 2 );
        }
        else
        {
            ButtonState state = GetCurrentButtonState();

            wchar_t wszBuffer[ 128 ];
            V_UTF8ToUnicode( m_szKey.String(), wszBuffer, sizeof( wszBuffer ) );
            V_wcsupr( wszBuffer );

            int nTextSizeX, nTextSizeY;
            vgui::surface()->DrawSetTextFont(state == ButtonStates::Out ? m_hTextFont : m_hTextFontOver);
            vgui::surface()->GetTextSize(state == ButtonStates::Out ? m_hTextFont : m_hTextFontOver, wszBuffer, nTextSizeX, nTextSizeY);

            vgui::surface()->DrawSetTextPos( m_flWidth - m_flTextOffsetX - nTextSizeX, m_flHeight / 2 - nTextSizeY / 2 );
            vgui::surface()->DrawPrintText( wszBuffer, V_wcslen( wszBuffer ) );
        }
    }

    ButtonState GetCurrentButtonState() OVERRIDE
    {
        if ( m_bBeingBound )
            return ButtonStates::Pressed;
        if ( s_bBeingBound )
            return ButtonStates::Out;
        return BaseClass::GetCurrentButtonState();
    }

    void OnMouseDoublePressed(vgui::MouseCode code) OVERRIDE
    {
        if ( !s_bBeingBound )
            StartCapture();
    }

    void OnKeyCodePressed( vgui::KeyCode code )
    {
        ButtonCode_t buttonCode = GetBaseButtonCode( code );
        switch ( buttonCode )
        {
        case KEY_ENTER:
            if ( !s_bBeingBound )
                StartCapture();
            break;
        case KEY_DELETE:
            if ( !s_bBeingBound )
                ClearKey();
            break;
        default:
            BaseClass::OnKeyCodePressed( code );
            break;
        }
    }

    void OnThink() OVERRIDE
    {
	    BaseClass::OnThink();

	    if ( m_bBeingBound )
	    {
		    ButtonCode_t code = BUTTON_CODE_INVALID;
		    if ( GamepadUI::GetInstance().GetEngineClient()->CheckDoneKeyTrapping( code ) )
		    {
                OnKeyBound( code );
		    }
	    }
    }

    void StartCapture()
    {
        m_bBeingBound = true;
        s_bBeingBound = true;
        g_pInputSystem->SetNovintPure( true );
        GamepadUI::GetInstance().GetEngineClient()->StartKeyTrapMode();
        vgui::surface()->SetCursor( vgui::dc_none );
        SetCursor( vgui::dc_none );
        vgui::input()->GetCursorPos( m_iMouseX, m_iMouseY );
    }

    void EndCapture()
    {
        m_bBeingBound = false;
        s_bBeingBound = false;
        vgui::input()->SetMouseCapture( NULL );
        RequestFocus();
        g_pInputSystem->SetNovintPure( false );
        vgui::surface()->SetCursor( vgui::dc_arrow );
        SetCursor( vgui::dc_arrow );
        vgui::input()->SetCursorPos ( m_iMouseX, m_iMouseY );
    }

    void OnKeyBound( ButtonCode_t code )
    {
        EndCapture();

        const char *pKey = g_pInputSystem->ButtonCodeToString( code );
        GamepadUIOptionsPanel *pOptions = GamepadUIOptionsPanel::GetInstance();
        if ( pOptions )
            pOptions->OnKeyBound( pKey );
        m_szKey = pKey;
    }

    const char *GetKeyBinding() { return m_szBinding.String(); }

    const char *GetKey() { return m_szKey.String(); }
    void SetKey( const char *pKey ) { m_szKey = pKey; }

    void ClearKey() { m_szKey = ""; }

protected:
    CUtlString m_szBinding;
    CUtlString m_szKey;

    bool m_bBeingBound = false;
    static bool s_bBeingBound;
    int m_iMouseX, m_iMouseY;

    GAMEPADUI_PANEL_PROPERTY( float, m_flBindingWidth, "Button.Binding.Width", "160", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flBindingHeight, "Button.Binding.Height", "11", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( Color, m_colBinding, "Button.Binding", "0 0 0 255", SchemeValueTypes::Color );
};

bool GamepadUIKeyButton::s_bBeingBound = false;

class GamepadUIConvarButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIConvarButton, GamepadUIButton );

    GamepadUIConvarButton( const char *pszCvar, const char* pszCvarDepends, bool bInstantApply, vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const char *pText, const char *pDescription )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_cvar( pszCvar )
        , m_szDependentCVar( pszCvarDepends )
        , m_bInstantApply( bInstantApply )
    {
    }

    virtual void UpdateConVar() = 0;
    virtual bool IsDirty() = 0;
    virtual void SetToDefault() = 0;
    virtual bool IsConVarEnabled() const { return true; }

    const char *GetDependentCVar() const
    {
        return m_szDependentCVar.String();
    }

    const char *GetConVarName() const
    {
        return m_cvar.GetName();
    }

protected:
    ConVarRef m_cvar;
    CUtlString m_szDependentCVar;
    bool m_bInstantApply = false;
};

class GamepadUIWheelyWheel : public GamepadUIConvarButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIWheelyWheel, GamepadUIConvarButton );

    GamepadUIWheelyWheel( const char *pszCvar, const char *pszCvarDepends, bool bInstantApply, bool bSignOnly, vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const char *pText, const char *pDescription )
        : BaseClass( pszCvar, pszCvarDepends, bInstantApply, pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_bSignOnly( bSignOnly )
    {
    }

    void OnKeyCodePressed( vgui::KeyCode code )
    {
        ButtonCode_t buttonCode = GetBaseButtonCode( code );
        switch ( buttonCode )
        {
        case KEY_LEFT:
        case KEY_XBUTTON_LEFT:

#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        case STEAMCONTROLLER_DPAD_LEFT:
#endif
            if ( --m_nSelectedItem < 0 )
                m_nSelectedItem = Max( 0, m_Options.Count() - 1 );
            if ( m_bInstantApply )
                UpdateConVar();
            break;

        case KEY_RIGHT:
        case KEY_XBUTTON_RIGHT:

#ifdef HL2_RETAIL
        case STEAMCONTROLLER_DPAD_RIGHT:
#endif
            if ( m_Options.Count() )
                m_nSelectedItem = ( m_nSelectedItem + 1 ) % m_Options.Count();
            if ( m_bInstantApply )
                UpdateConVar();
            break;

        default:
            BaseClass::OnKeyCodePressed( code );
            break;
        }
    }

    void FireActionSignal()
    {
        BaseClass::FireActionSignal();

        if ( m_Options.Count() )
            m_nSelectedItem = ( m_nSelectedItem + 1 ) % m_Options.Count();
        if ( m_bInstantApply )
            UpdateConVar();
    }

    void UpdateConVar() OVERRIDE
    {
        if ( IsDirty() )
        {
            if ( m_bSignOnly )
                m_cvar.SetValue( abs( m_cvar.GetFloat() ) * m_Options[m_nSelectedItem].nValue );
            else
                m_cvar.SetValue( m_Options[ m_nSelectedItem ].nValue );
        }
    }


    bool IsDirty() OVERRIDE
    {
        return m_cvar.IsValid() && GetCvarValue() != m_Options[m_nSelectedItem].nValue;
    }

    virtual void Paint()
    {
        BaseClass::Paint();

        if ( m_nSelectedItem >= m_Options.Count() )
            return;

        GamepadUIString& strOption = m_Options[ m_nSelectedItem ].strOptionText;
        ButtonState state = GetCurrentButtonState();

        int nTextW, nTextH;
        vgui::surface()->GetTextSize( m_hTextFont, strOption.String(), nTextW, nTextH );

        int nScrollerSize = vgui::surface()->GetCharacterWidth( m_hTextFont, L'<' ) + vgui::surface()->GetCharacterWidth( m_hTextFont, L' ' );

        int nTextY = m_flHeight / 2 - nTextH / 2 + m_flTextOffsetY;

        vgui::surface()->DrawSetTextFont( m_hTextFont );
        if ( state != ButtonStates::Out )
        {
            vgui::surface()->DrawSetTextPos( m_flWidth - m_flTextOffsetX - nTextW - 2 * nScrollerSize, nTextY );
            vgui::surface()->DrawPrintText( L"< ", 2 );
        }
        vgui::surface()->DrawSetTextPos( m_flWidth - m_flTextOffsetX - nTextW - nScrollerSize, nTextY );
        vgui::surface()->DrawPrintText( strOption.String(), strOption.Length() );
        if ( state != ButtonStates::Out )
        {
            vgui::surface()->DrawSetTextPos( m_flWidth - m_flTextOffsetX - nScrollerSize, nTextY );
            vgui::surface()->DrawPrintText( L" >", 2 );
        }
    }

    void ClearOptions()
    {
        m_Options.RemoveAll();
    }

    void AddOptionItem( GamepadUIOption option )
    {
        m_Options.AddToTail( option );
    }

    int GetOptionCount()
    {
        return m_Options.Count();
    }

    int GetCvarValue()
    {
        if ( m_bSignOnly )
            return (int)Sign( m_cvar.GetFloat() );
        else
            return m_cvar.GetInt();
    }

    void SetToDefault() OVERRIDE
    {
        if ( m_cvar.IsValid() )
        {
            const int nCurrentValue = GetCvarValue();
            for ( int i = 0; i < m_Options.Count(); i++)
            {
                if ( m_Options[ i ].nValue == nCurrentValue )
                    m_nSelectedItem = i;
            }
        }
    }

    GamepadUIOption *GetOption( int nIndex )
    {
        if ( nIndex < 0 || nIndex >= m_Options.Count() )
            return NULL;
        return &m_Options[ nIndex ];
    }

    bool IsConVarEnabled() const OVERRIDE
    {
        return !!m_Options[m_nSelectedItem].nValue;
    }

private:

    bool m_bSignOnly = false;
    int m_nSelectedItem = 0;
    CUtlVector< GamepadUIOption > m_Options;

};

class GamepadUISlideySlide : public GamepadUIConvarButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUISlideySlide, GamepadUIConvarButton );

    GamepadUISlideySlide( const char *pszCvar, const char* pszCvarDepends, bool bInstantApply, float flMin, float flMax, float flStep, int nTextPrecision, vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const char *pText, const char *pDescription )
        : BaseClass( pszCvar, pszCvarDepends, bInstantApply, pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_flMin( flMin )
        , m_flMax( flMax )
        , m_flStep( flStep )
        , nTextPrecision( nTextPrecision )
    {
    }

    void OnKeyCodePressed( vgui::KeyCode code )
    {
        ButtonCode_t buttonCode = GetBaseButtonCode( code );
        switch ( buttonCode )
        {
        case KEY_LEFT:
        case KEY_XBUTTON_LEFT:

#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        case STEAMCONTROLLER_DPAD_LEFT:
#endif
            m_flValue = Clamp( m_flValue - m_flStep, m_flMin, m_flMax );
            if ( m_bInstantApply )
                UpdateConVar();
            break;

        case KEY_RIGHT:
        case KEY_XBUTTON_RIGHT:

#ifdef HL2_RETAIL
        case STEAMCONTROLLER_DPAD_RIGHT:
#endif
            m_flValue = Clamp( m_flValue + m_flStep, m_flMin, m_flMax );
            if ( m_bInstantApply )
                UpdateConVar();
            break;

        default:
            BaseClass::OnKeyCodePressed( code );
            break;
        }
    }

    void UpdateConVar() OVERRIDE
    {
        if ( IsDirty() )
            m_cvar.SetValue( m_flValue );
    }

    bool IsDirty() OVERRIDE
    {
        return m_cvar.IsValid() && m_cvar.GetFloat() != m_flValue;
    }

    float GetMultiplier() const
    {
        return ( m_flValue - m_flMin ) / ( m_flMax - m_flMin );
    }

    virtual void Paint()
    {
        BaseClass::Paint();

        if ( nTextPrecision >= 0 )
        {
            wchar_t szValue[ 256 ];
            V_snwprintf( szValue, sizeof( szValue ), L"%.*f", nTextPrecision, m_flValue );

            int w, h;
            vgui::surface()->GetTextSize( m_hTextFont, szValue, w, h );
            vgui::surface()->DrawSetTextPos( m_flWidth - 2 * m_flTextOffsetX - m_flSliderWidth - w, m_flHeight / 2 - h / 2 );
            vgui::surface()->DrawPrintText( szValue, V_wcslen( szValue ) );
        }

        vgui::surface()->DrawSetColor( m_colSliderBacking );
        vgui::surface()->DrawFilledRect( m_flWidth - m_flTextOffsetX - m_flSliderWidth, m_flHeight / 2 - m_flSliderHeight / 2, m_flWidth - m_flTextOffsetX, m_flHeight / 2 + m_flSliderHeight / 2 );

        float flFill = m_flSliderWidth * ( 1.0f - GetMultiplier() );

        vgui::surface()->DrawSetColor( m_colSliderFill );
        vgui::surface()->DrawFilledRect( m_flWidth - m_flTextOffsetX - m_flSliderWidth, m_flHeight / 2 - m_flSliderHeight / 2, m_flWidth - m_flTextOffsetX - flFill, m_flHeight / 2 + m_flSliderHeight / 2 );
    }

    void SetToDefault() OVERRIDE
    {
        if ( m_cvar.IsValid() )
            m_flValue = m_cvar.GetFloat();
    }

    void RunAnimations( ButtonState state ) OVERRIDE
    {
        BaseClass::RunAnimations( state );

        GAMEPADUI_RUN_ANIMATION_COMMAND( m_colSliderBacking, vgui::AnimationController::INTERPOLATOR_LINEAR );
        GAMEPADUI_RUN_ANIMATION_COMMAND( m_colSliderFill, vgui::AnimationController::INTERPOLATOR_LINEAR );
    }

private:

    float m_flValue = 0.0f;

    float m_flMin = 0.0f;
    float m_flMax = 1.0f;
    float m_flStep = 0.1f;

    int nTextPrecision = -1;

    GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colSliderBacking, "Slider.Backing", "255 255 255 22", SchemeValueTypes::Color );
    GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colSliderFill, "Slider.Fill", "255 255 255 255", SchemeValueTypes::Color );

    GAMEPADUI_PANEL_PROPERTY( float, m_flSliderWidth, "Slider.Width", "160", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSliderHeight, "Slider.Height", "11", SchemeValueTypes::ProportionalFloat );

};

class GamepadUISkillySkill : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUISkillySkill, GamepadUIButton );

    GamepadUISkillySkill( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const char* pText, const char* pDescription, const char *pImage, int nSkill )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pImage )
        , m_nSkill( nSkill )
    {
    }

    GamepadUISkillySkill( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const wchar* pText, const wchar* pDescription, const char *pImage, int nSkill )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pImage )
        , m_nSkill( nSkill )
    {
    }

    void RunAnimations( ButtonState state ) OVERRIDE
    {
        BaseClass::RunAnimations( state );

        GAMEPADUI_RUN_ANIMATION_COMMAND( m_colTitleBackground, vgui::AnimationController::INTERPOLATOR_LINEAR );
        GAMEPADUI_RUN_ANIMATION_COMMAND( m_colDescriptionBackground, vgui::AnimationController::INTERPOLATOR_LINEAR );
        GAMEPADUI_RUN_ANIMATION_COMMAND( m_colImage, vgui::AnimationController::INTERPOLATOR_LINEAR );
    }

    ButtonState GetCurrentButtonState() OVERRIDE
    {
        if ( _gamepadui_skill.GetInt() == m_nSkill )
            return ButtonState::Pressed;

        return BaseClass::GetCurrentButtonState();
    }

    void NavigateTo() OVERRIDE
    {
        BaseClass::NavigateTo();

        _gamepadui_skill.SetValue( m_nSkill );
    }

    void DoClick() OVERRIDE
    {
        BaseClass::DoClick();

        _gamepadui_skill.SetValue( m_nSkill );
    }

    void Paint() OVERRIDE
    {
        PaintButton();

        int nTextSizeX, nTextSizeY;
        vgui::surface()->GetTextSize(GetCurrentButtonState() == ButtonStates::Out ? m_hTextFont : m_hTextFontOver, m_strButtonText.String(), nTextSizeX, nTextSizeY);
        int y2 = m_flTextOffsetY + m_flHeight / 2 + nTextSizeY / 2;

        vgui::surface()->DrawSetColor(m_colTitleBackground);
        vgui::surface()->DrawFilledRect(0, m_flTextOffsetY + m_flHeight / 2 - nTextSizeY / 2, nTextSizeX + m_flTextOffsetX * 2, y2);

        vgui::surface()->DrawSetColor(m_colDescriptionBackground);
        vgui::surface()->DrawFilledRect(0, y2, m_flWidth, m_flHeight + m_flExtraHeight);

        vgui::surface()->DrawSetColor( m_colImage );
        vgui::surface()->DrawSetTexture( m_Image );
        vgui::surface()->DrawTexturedRect( m_flWidth / 2 - m_flImageWidth / 2, 0, m_flWidth / 2 + m_flImageWidth / 2, m_flImageHeight );
        vgui::surface()->DrawSetTexture( 0 );

        PaintText();

        PaintBorders();
    }

private:
    GamepadUIImage m_Image;

    int m_nSkill = 0;

    GAMEPADUI_PANEL_PROPERTY( float, m_flImageWidth,  "Button.Image.Width",  "40", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flImageHeight, "Button.Image.Height", "40",  SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colImage, "Button.Image", "255 255 255 255",  SchemeValueTypes::Color );
    GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colTitleBackground, "Button.Title.Background", "0 0 0 0",  SchemeValueTypes::Color );
    GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colDescriptionBackground, "Button.Description.Background", "0 0 0 0",  SchemeValueTypes::Color );
};

struct AAMode_t
{
    GamepadUIString strName;
	int m_nNumSamples;
	int m_nQualityLevel;
};

int g_nNumAAModes = 0;
AAMode_t g_AAModes[16];

void InitAAModes()
{
    static bool s_bAAModesInitialized = false;
    if ( s_bAAModesInitialized )
        return;
    s_bAAModesInitialized = true;

    g_nNumAAModes = 0;

    g_AAModes[g_nNumAAModes].strName = "#GameUI_None";
    g_AAModes[g_nNumAAModes].m_nNumSamples = 1;
	g_AAModes[g_nNumAAModes].m_nQualityLevel = 0;
	g_nNumAAModes++;

	if ( materials->SupportsMSAAMode(2) )
	{
        g_AAModes[g_nNumAAModes].strName = "#GameUI_2X";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 2;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 0;
		g_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode(4) )
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_4X";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 4;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 0;
		g_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode(6) )
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_6X";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 6;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 0;
		g_nNumAAModes++;
	}

	if ( materials->SupportsCSAAMode(4, 2) )							// nVidia CSAA			"8x"
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_8X_CSAA";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 4;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 2;
		g_nNumAAModes++;
	}

	if ( materials->SupportsCSAAMode(4, 4) )							// nVidia CSAA			"16x"
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_16X_CSAA";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 4;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 4;
		g_nNumAAModes++;
	}

	if ( materials->SupportsMSAAMode(8) )
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_8X";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 8;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 0;
		g_nNumAAModes++;
	}

	if ( materials->SupportsCSAAMode(8, 2) )							// nVidia CSAA			"16xQ"
	{
		g_AAModes[g_nNumAAModes].strName = "#GameUI_16XQ_CSAA";
		g_AAModes[g_nNumAAModes].m_nNumSamples = 8;
		g_AAModes[g_nNumAAModes].m_nQualityLevel = 2;
		g_nNumAAModes++;
	}
}

struct RatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};
RatioToAspectMode_t g_RatioToAspectModes[] =
{
	{	0,		4.0f / 3.0f },
	{	1,		16.0f / 9.0f },
	{	2,		16.0f / 10.0f },
	{	2,		1.0f },
};

int GetScreenAspectMode( int width, int height )
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(g_RatioToAspectModes); i++)
	{
		float dist = fabs( g_RatioToAspectModes[i].aspectRatio - aspectRatio );
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = g_RatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}

void OnResolutionsNeedUpdate( IConVar *var, const char *pOldValue, float flOldValue )
{
    GamepadUIOptionsPanel* pOptionsPanel = GamepadUIOptionsPanel::GetInstance();
    if ( pOptionsPanel )
        pOptionsPanel->UpdateResolutions();
}

static int GetSDLDisplayIndex()
{
#if defined( USE_SDL )
	static ConVarRef sdl_displayindex( "sdl_displayindex" );

	Assert( sdl_displayindex.IsValid() );
	return sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;
#else
    return 0;
#endif
}

static int GetSDLDisplayIndexFullscreen()
{
#if defined( USE_SDL )
	static ConVarRef sdl_displayindex_fullscreen( "sdl_displayindex_fullscreen" );

	Assert( sdl_displayindex_fullscreen.IsValid() );
	return sdl_displayindex_fullscreen.IsValid() ? sdl_displayindex_fullscreen.GetInt() : -1;
#else
    return -1;
#endif
}

int GetCurrentWaterDetail()
{
    ConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
    ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );

    if ( r_waterforcereflectentities.GetBool() )
        return 2;

    if ( r_waterforceexpensive.GetBool() )
        return 1;

    return 0;
}

int GetCurrentShadowDetail()
{
    ConVarRef r_shadowrendertotexture( "r_shadowrendertotexture" );
    ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );

    if ( r_flashlightdepthtexture.GetBool() )
        return 2;

    if ( r_shadowrendertotexture.GetBool() )
        return 1;

    return 0;
}

int GetCurrentAntialiasing()
{
    ConVarRef mat_antialias( "mat_antialias" );
    ConVarRef mat_aaquality( "mat_aaquality" );
    int nAASamples = mat_antialias.GetInt();
    int nAAQuality = mat_aaquality.GetInt();

	// Run through the AA Modes supported by the device
    for ( int nAAMode = 0; nAAMode < g_nNumAAModes; nAAMode++ )
	{
		// If we found the mode that matches what we're looking for, return the index
		if ( ( g_AAModes[nAAMode].m_nNumSamples == nAASamples) && ( g_AAModes[nAAMode].m_nQualityLevel == nAAQuality) )
		{
			return nAAMode;
		}
	}

	return 0;	// Didn't find what we're looking for, so no AA
}

int GetCurrentAspectRatio()
{
    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
    return GetScreenAspectMode( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );
}

int GetCurrentDisplayMode()
{
    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
    if ( config.Windowed() )
    {
#ifdef HL2_RETAIL // SDK2013 does not natively support borderless windowed (Madi)
        if ( config.NoWindowBorder() )
            return 1;
#endif

        return 0;
    }

#ifdef HL2_RETAIL
    return 2 + GetSDLDisplayIndex();
#else
    // TODO FIXME: SDK2013 uses the inverse here...1 is "windowed" and 0 is "fullscreen" in materialsystem.
    // but our values mean the opposite in gamepadUI code. (Madi)
    return 1 + GetSDLDisplayIndex();
#endif
}


int GetCurrentSoundQuality()
{
	ConVarRef snd_pitchquality( "snd_pitchquality" );
	ConVarRef dsp_slow_cpu( "dsp_slow_cpu" );

    if ( snd_pitchquality.GetInt() )
        return 2;

    if ( !dsp_slow_cpu.GetBool() )
        return 1;

    return 0;
}

int GetCurrentCloseCaptions()
{
    ConVarRef closecaption( "closecaption" );
    ConVarRef cc_subtitles( "cc_subtitles" );

    if ( closecaption.GetBool() )
        return cc_subtitles.GetBool() ? 1 : 2;
    return 0;
}

#ifdef HL2_RETAIL
int GetCurrentHudAspectRatio()
{
    ConVarRef hud_aspect( "hud_aspect" );
    float flHudAspect = hud_aspect.GetFloat();
	if ( flHudAspect > 0.0f )
	{
		if ( flHudAspect < 1.4f )
			return 1;
		else if (flHudAspect < 1.7f)
			return 3;
		else
			return 2;
	}
	else
		return 0;
}
#endif

int GetCurrentSkill()
{
    ConVarRef skill( "skill" );
    return skill.GetInt();
}

void FlushPendingAntialiasing()
{
    ConVarRef mat_antialias( "mat_antialias" );
    ConVarRef mat_aaquality( "mat_aaquality" );

    int nAAMode = Clamp( _gamepadui_antialiasing.GetInt(), 0, Max( 0, g_nNumAAModes - 1 ) );
    if ( mat_antialias.GetInt() != g_AAModes[ nAAMode ].m_nNumSamples )
        mat_antialias.SetValue( g_AAModes[ nAAMode ].m_nNumSamples );

    if ( mat_aaquality.GetInt() != g_AAModes[ nAAMode ].m_nQualityLevel )
        mat_aaquality.SetValue( g_AAModes[ nAAMode ].m_nQualityLevel );
}

void FlushPendingWaterDetail()
{
    ConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
    ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );

    bool bForceExpensive = _gamepadui_water_detail.GetInt() > 0;
    bool bForceReflect = _gamepadui_water_detail.GetInt() > 1;

    if ( r_waterforceexpensive.GetBool() != bForceExpensive )
        r_waterforceexpensive.SetValue( bForceExpensive );

    if ( r_waterforcereflectentities.GetBool() != bForceReflect )
        r_waterforcereflectentities.SetValue(bForceReflect);
}

void FlushPendingShadowDetail()
{
    ConVarRef r_shadowrendertotexture( "r_shadowrendertotexture" );
    ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );

    bool bShadowRenderToTexture = _gamepadui_shadow_detail.GetInt() > 0;
    bool bFlashlightDepthTexture = _gamepadui_shadow_detail.GetInt() > 1;

    if ( r_shadowrendertotexture.GetBool() != bShadowRenderToTexture )
        r_shadowrendertotexture.SetValue( bShadowRenderToTexture );

    if ( r_flashlightdepthtexture.GetBool() != bFlashlightDepthTexture )
        r_flashlightdepthtexture.SetValue( bFlashlightDepthTexture );
}

void FlushPendingResolution()
{
    GamepadUIOptionsPanel *pOptions = GamepadUIOptionsPanel::GetInstance();
    if ( !pOptions )
        return;

    GamepadUIWheelyWheel *pResButton = pOptions->GetResolutionButton();
    if ( !pResButton )
        return;

    GamepadUIOption *pOption = pResButton->GetOption( _gamepadui_resolution.GetInt() );
    if ( !pOption )
        return;

    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
    bool bDirty = false;

    if ( GetCurrentDisplayMode() != _gamepadui_displaymode.GetInt() )
        bDirty = true;

    if ( pOption->userdata.nWidth != config.m_VideoMode.m_Width || pOption->userdata.nHeight != config.m_VideoMode.m_Height )
        bDirty = true;

    if ( !bDirty )
        return;

    const int nWidth = pOption->userdata.nWidth;
    const int nHeight = pOption->userdata.nHeight;
#ifdef HL2_RETAIL
    const int nWindowed = _gamepadui_displaymode.GetInt() < 2 ? 1 : 0;
    const int nBorderless = _gamepadui_displaymode.GetInt() == 1 ? 1 : 0;
#else
    // TODO FIXME: SDK2013 uses the inverse here...1 is "windowed" and 0 is "fullscreen" in materialsystem.
    // but our values mean the opposite in gamepadUI code. (Madi)
    const int nWindowed = _gamepadui_displaymode.GetInt() == 1 ? 0 /* fullscreen*/ : 1 /* windowed */;
#endif

    char szCmd[ 256 ];

#ifdef HL2_RETAIL
	Q_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i %i\n", nWidth, nHeight, nWindowed, nBorderless );
#else
    Q_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i\n", nWidth, nHeight, nWindowed );
#endif

	GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( szCmd );
}

void FlushPendingSoundQuality()
{
	ConVarRef snd_pitchquality( "snd_pitchquality" );
	ConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
    ConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
    ConVarRef snd_surround_speakers( "snd_surround_speakers" );

    int nSoundQuality = _gamepadui_sound_quality.GetInt();
    switch ( nSoundQuality )
    {
        default:
        case 2:
            // Headphones at high quality get enhanced stereo turned on
            dsp_enhance_stereo.SetValue( snd_surround_speakers.GetInt() == 0 );
            snd_pitchquality.SetValue( true );
            dsp_slow_cpu.SetValue( false );
            break;
        case 1:
            snd_pitchquality.SetValue( false );
            dsp_slow_cpu.SetValue( false );
            break;
        case 0:
            snd_pitchquality.SetValue( false );
            dsp_slow_cpu.SetValue( true );
            break;
    }
}

void FlushPendingCloseCaptions()
{
    ConVarRef closecaption( "closecaption" );
    ConVarRef cc_subtitles( "cc_subtitles" );

    int nCloseCaptions = _gamepadui_closecaptions.GetInt();
    bool bCloseCaptionConvarValue = false;
    switch ( nCloseCaptions )
    {
        default:
        case 2:
            cc_subtitles.SetValue( false );
            bCloseCaptionConvarValue = true;
            break;
        case 1:
            cc_subtitles.SetValue(true);
            bCloseCaptionConvarValue = true;
            break;
        case 0:
            cc_subtitles.SetValue(false);
            bCloseCaptionConvarValue = false;
            break;
    }

	// Stuff the close caption change to the console so that it can be
	// sent to the server (FCVAR_USERINFO) so that you don't have to restart
	// the level for the change to take effect.
	char szCmd[ 64 ];
	Q_snprintf( szCmd, sizeof( szCmd ), "closecaption %i\n", bCloseCaptionConvarValue ? 1 : 0 );
	GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( szCmd );
}

#ifdef HL2_RETAIL
void FlushPendingHudAspectRatio()
{
    ConVarRef hud_aspect( "hud_aspect" );

    int nHudAspect = _gamepadui_hudaspect.GetInt();
    switch ( nHudAspect )
    {
    default:
    case 0:
        hud_aspect.SetValue( 0.0f );
        break;
    case 1:
        hud_aspect.SetValue( 4.0f / 3.0f );
        break;
    case 2:
        hud_aspect.SetValue( 16.0f / 9.0f );
        break;
    case 3:
        hud_aspect.SetValue( 16.0f / 10.0f );
        break;
    }
}
#endif

void FlushPendingSkill()
{
    ConVarRef skill( "skill" );
    skill.SetValue( _gamepadui_skill.GetInt() );
}

void UpdateHelperConvars()
{
    _gamepadui_water_detail.SetValue( GetCurrentWaterDetail() );
    _gamepadui_shadow_detail.SetValue( GetCurrentShadowDetail() );
    _gamepadui_antialiasing.SetValue( GetCurrentAntialiasing() );
    _gamepadui_aspectratio.SetValue( GetCurrentAspectRatio() );
    _gamepadui_displaymode.SetValue( GetCurrentDisplayMode() );
    _gamepadui_sound_quality.SetValue( GetCurrentSoundQuality() );
    _gamepadui_closecaptions.SetValue( GetCurrentCloseCaptions() );
#ifdef HL2_RETAIL
    _gamepadui_hudaspect.SetValue( GetCurrentHudAspectRatio() );
#endif
    _gamepadui_skill.SetValue( GetCurrentSkill() );
}

void FlushHelperConVars()
{
    FlushPendingWaterDetail();
    FlushPendingShadowDetail();
    FlushPendingAntialiasing();
    FlushPendingResolution();
    FlushPendingSoundQuality();
    FlushPendingCloseCaptions();
#ifdef HL2_RETAIL
    FlushPendingHudAspectRatio();
#endif
    FlushPendingSkill();
}

GamepadUIOptionsPanel::GamepadUIOptionsPanel( vgui::Panel* pParent, const char* pPanelName )
    : BaseClass( pParent, pPanelName )
{
    s_pOptionsPanel = this;

    vgui::HScheme Scheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( Scheme );

    GetFrameTitle() = GamepadUIString( "#GameUI_Options" );
    SetFooterButtons( FooterButtons::Apply | FooterButtons::Back );

    Activate();

    InitAAModes();
    UpdateHelperConvars();

    LoadOptionTabs( GAMEPADUI_OPTIONS_FILE );
    FillInBindings();

    SetActiveTab( GetActiveTab() );

    UpdateGradients();
}

GamepadUIOptionsPanel::~GamepadUIOptionsPanel()
{
    s_pOptionsPanel = NULL;
}

void GamepadUIOptionsPanel::OnThink()
{
    BaseClass::OnThink();

    LayoutCurrentTab();
}

void GamepadUIOptionsPanel::Paint()
{
    BaseClass::Paint();

    if ( !m_nTabCount )
        return;

    const int nLastTabX = m_flTabsOffsetX + m_nTabCount * m_Tabs[0].pTabButton->GetWide();

    const int nTabSize = m_Tabs[0].pTabButton->GetTall();
    const int nGlyphSize = nTabSize * 0.90f;
    const int nGlyphOffsetX = nGlyphSize / 4.0f;
    const int nGlyphOffsetY = nTabSize - nGlyphSize;

    if ( m_leftGlyph.SetupGlyph( nGlyphSize, "menu_lb", true ) )
        m_leftGlyph.PaintGlyph( m_flTabsOffsetX - nGlyphSize - nGlyphOffsetX, m_flTabsOffsetY + nGlyphOffsetY / 2, nGlyphSize, 255 );

    if ( m_rightGlyph.SetupGlyph( nGlyphSize, "menu_rb", true ) )
        m_rightGlyph.PaintGlyph( nLastTabX + nGlyphOffsetX, m_flTabsOffsetY + nGlyphOffsetY / 2, nGlyphSize, 255 );
}

void GamepadUIOptionsPanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 0.5f }, flTime );
}

void GamepadUIOptionsPanel::LayoutCurrentTab()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    int x = m_flTabsOffsetX;
    int y = 0;
    for ( int i = 0; i < m_nTabCount; i++ )
    {
        GamepadUITab& tab = m_Tabs[ i ];
        tab.pTabButton->SetPos( x, m_flTabsOffsetY );
        tab.pTabButton->SetVisible( true );
        x += tab.pTabButton->GetWide();
        y = m_flTabsOffsetY + tab.pTabButton->GetTall();

        for ( GamepadUIButton *pButton : tab.pButtons )
            pButton->SetVisible( false );
    }

    int yMax = 0;
    int nActiveTab = GetActiveTab();
    {
        int nScrollCount = m_Tabs[nActiveTab].pButtons.Count() - 8;
        for ( int i = 0; i < m_Tabs[nActiveTab].pButtons.Count(); i++ )
        {
            GamepadUIButton *pButton = m_Tabs[ nActiveTab ].pButtons[ i ];
            if ( i < nScrollCount )
                yMax += pButton->GetTall();
        }
        m_Tabs[ nActiveTab ].ScrollState.UpdateScrollBounds( 0.0f, yMax );
    }

    int i = 0;
    int previousSizes = 0;
    int buttonWide = 0;
    for ( GamepadUIButton *pButton : m_Tabs[ nActiveTab ].pButtons )
    {
        int fade = 255;

        int buttonY = y;
        int buttonX = m_flTabsOffsetX;
        if ( m_Tabs[ nActiveTab ].bHorizontal )
        {
            buttonX += previousSizes;
        }
        else
        {
            buttonY = y + previousSizes - m_Tabs[nActiveTab].ScrollState.GetScrollProgress();
            if ( buttonY < y )
                fade = RemapValClamped( y - buttonY, m_flOptionsFade - pButton->GetTall(), 0, 0, 255 );
            else if ( buttonY > ( nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight - m_flOptionsFade ) )
                fade = RemapValClamped( ( nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight ) - ( buttonY + pButton->GetTall() ), 0, m_flOptionsFade, 0, 255 );
            if ( ( pButton->HasFocus() && pButton->IsEnabled() ) && fade != 0 )
                fade = 255;
        }

        GamepadUIConvarButton* pCvarButton = dynamic_cast<GamepadUIConvarButton*>(pButton);
        bool bHasDependencies = true;
        if ( pCvarButton )
        {
            const char *pszDependentCVar = pCvarButton->GetDependentCVar();
            if ( pszDependentCVar && *pszDependentCVar )
            {
                bHasDependencies = false;

                bool bFound = false;

                for ( GamepadUIButton *pOtherButton : m_Tabs[ nActiveTab ].pButtons )
                {
                    GamepadUIConvarButton* pOtherCvarButton = dynamic_cast<GamepadUIConvarButton*>(pOtherButton);
                    if ( pOtherCvarButton )
                    {
                        const char* pszConVarName = pOtherCvarButton->GetConVarName();
                        if ( pszConVarName && *pszConVarName && !V_strcmp( pszDependentCVar, pszConVarName ) )
                        {
                            bHasDependencies = pOtherCvarButton->IsConVarEnabled();
                            bFound = true;
                            break;
                        }
                    }
                }

                if ( !bFound )
                {
                    ConVarRef cvar(pszDependentCVar);
                    bHasDependencies = cvar.GetBool();
                }
            }
        }

        pButton->SetAlpha(fade);
        pButton->SetVisible(bHasDependencies);
        pButton->SetPos( buttonX, buttonY );

        if ( pButton->IsEnabled() && pButton->IsVisible() && fade && m_Tabs[nActiveTab].bAlternating)
        {
            buttonWide = pButton->GetWide();
            if ( i % 2 )
                vgui::surface()->DrawSetColor( Color( 0, 0, 0, ( 20 * Min( 255, fade + 127 ) ) / 255 ) );
            else
                vgui::surface()->DrawSetColor( Color( fade, fade, fade, fade > 64 ? 1 : 0 ) );

            vgui::surface()->DrawFilledRect( buttonX, buttonY, buttonX + buttonWide, buttonY + pButton->GetTall() );
        }

        if ( m_Tabs[ nActiveTab ].bHorizontal )
            previousSizes += pButton->GetWide();
        else
            previousSizes += pButton->GetTall();
        i++;
    }

    if ( yMax != 0 )
    {
        vgui::surface()->DrawSetColor( Color( 255, 255, 255, 200 ) );
        int scrollbarY = RemapValClamped( m_Tabs[ nActiveTab ].ScrollState.GetScrollProgress(), 0, yMax, y, nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight - m_flScrollBarHeight );
        vgui::surface()->DrawFilledRect( m_flTabsOffsetX + m_flScrollBarOffsetX, scrollbarY, m_flTabsOffsetX + m_flScrollBarOffsetX + m_flScrollBarWidth, scrollbarY + m_flScrollBarHeight );
    }

    m_Tabs[nActiveTab].ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUIOptionsPanel::OnMouseWheeled( int delta )
{
    m_Tabs[ GetActiveTab() ].ScrollState.OnMouseWheeled( delta * 100.0f, GamepadUI::GetInstance().GetTime() );
}

CON_COMMAND( _gamepadui_resetkeys, "" )
{
    GamepadUIOptionsPanel::GetInstance()->ClearBindings();
    GamepadUIOptionsPanel::GetInstance()->FillInBindings();
}

void GamepadUIOptionsPanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
        Close();
    }
    else if ( !V_strcmp( pCommand, "action_apply" ) )
    {
        for ( int i = 0; i < m_Tabs[ GetActiveTab() ].pButtons.Count(); i++ )
        {
            GamepadUIConvarButton *pConVarButton = dynamic_cast< GamepadUIConvarButton* >( m_Tabs[ GetActiveTab() ].pButtons[ i ] );
            if ( pConVarButton )
                pConVarButton->UpdateConVar();
        }

        FlushHelperConVars();
        ApplyKeyBindings();
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( "exec userconfig.cfg\nhost_writeconfig\nmat_savechanges\n" );
    }
    else if ( !V_strcmp( pCommand, "action_usedefaults" ) )
    {
        new GamepadUIGenericConfirmationPanel( GamepadUIOptionsPanel::GetInstance(), "UseDefaultsConfirm", GamepadUIString( "#GameUI_KeyboardSettings" ).String(), GamepadUIString("#GameUI_KeyboardSettingsText").String(),
		[](){
                GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( "exec config_default.cfg\n_gamepadui_resetkeys\n" );
            }, false, true);
    }
    else if ( StringHasPrefixCaseSensitive( pCommand, "tab " ) )
    {
        const char *pszTab = &pCommand[4];
        if ( *pszTab )
            SetActiveTab( atoi( pszTab ) );
    }
    else if ( !V_strcmp( pCommand, "open_steaminput" ) )
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        uint64_t nController = g_pInputSystem->GetActiveSteamInputHandle();
        if ( !nController )
        {
            InputHandle_t nControllers[ STEAM_INPUT_MAX_COUNT ];
            GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetConnectedControllers( nControllers );
            nController = nControllers[0];
        }
        if ( nController )
            GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->ShowBindingPanel( nController );
#endif // HL2_RETAIL
    }
    else if ( !V_strcmp( pCommand, "open_techcredits" ) )
    {
        GamepadUIString title = "#GameUI_ThirdPartyTechCredits";
        GamepadUIString bink = "#GameUI_Bink";
        GamepadUIString miles = "#GameUI_Miles_Audio";
        GamepadUIString voice = "#GameUI_Miles_Voice";
        wchar_t wszBuf[4096];
#ifdef WIN32
        V_snwprintf( wszBuf, 4096, L"%s\n\n%s\n\n%s", bink.String(), miles.String(), voice.String() );
#else
        V_snwprintf( wszBuf, 4096, L"%S\n\n%S\n\n%S", bink.String(), miles.String(), voice.String() );
#endif
		new GamepadUIGenericConfirmationPanel( GamepadUIOptionsPanel::GetInstance(), "TechCredits", title.String(), wszBuf,
		[](){}, true, false);
    }
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

int GamepadUIOptionsPanel::GetActiveTab()
{
    int nUserTab = gamepadui_last_options_tab.GetInt();

    int nActiveTab = Clamp( nUserTab, 0, Max( 0, m_nTabCount - 1 ) );
    if ( nUserTab != nActiveTab )
        gamepadui_last_options_tab.SetValue( nActiveTab );
    return nActiveTab;
}

const char *UTIL_Parse( const char *data, char *token, int sizeofToken )
{
	data = GamepadUI::GetInstance().GetEngineClient()->ParseFile( data, token, sizeofToken );
	return data;
}

static void GetResolutionName( vmode_t *mode, char *sz, int sizeofsz, int desktopWidth, int desktopHeight )
{
	Q_snprintf( sz, sizeofsz, "%i x %i%s", mode->width, mode->height,
				( mode->width == desktopWidth ) && ( mode->height == desktopHeight ) ? " (native)": "" );
}

static void GetCurrentAndDesktopSize( int &currentWidth, int &currentHeight, int &desktopWidth, int &desktopHeight )
{
    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    currentWidth = config.m_VideoMode.m_Width;
    currentHeight = config.m_VideoMode.m_Height;

	// Windowed is the last item in the combobox.
	GamepadUI::GetInstance().GetGameUIFuncs()->GetDesktopResolution( desktopWidth, desktopHeight );

#if defined( USE_SDL )
    bool bWindowed = _gamepadui_displaymode.GetInt() < 2;

    int nNewDisplayIndex = _gamepadui_displaymode.GetInt() - 2;
	if ( !bWindowed )
	{

		SDL_Rect rect;
		if ( !SDL_GetDisplayBounds( nNewDisplayIndex, &rect ) )
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
	}

	bool bNewFullscreenDisplay = ( !bWindowed && ( GetSDLDisplayIndexFullscreen() != nNewDisplayIndex ) );
	if ( bNewFullscreenDisplay )
	{
		currentWidth = desktopWidth;
		currentHeight = desktopHeight;
	}
#endif
}

void GamepadUIOptionsPanel::UpdateResolutions()
{
    if ( !m_pResolutionButton )
        return;

    m_pResolutionButton->ClearOptions();

    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    int currentWidth, currentHeight, desktopWidth, desktopHeight;
    GetCurrentAndDesktopSize( currentWidth, currentHeight, desktopWidth, desktopHeight );

	// get full video mode list
	vmode_t *plist = NULL;
	int count = 0;
	GamepadUI::GetInstance().GetGameUIFuncs()->GetVideoModes(&plist, &count);

    int nFilteredModeCount = 0;
    int nSelectedDefaultMode = -1;
    for ( int i = 0; i < count; i++, plist++ )
    {
		char sz[ 256 ];
		GetResolutionName( plist, sz, sizeof( sz ), desktopWidth, desktopHeight );

        int iAspectMode = GetScreenAspectMode( plist->width, plist->height );

        if ( iAspectMode == _gamepadui_aspectratio.GetInt() )
        {
		    if ( ( plist->width == currentWidth && plist->height == currentHeight ) ||
                 ( nSelectedDefaultMode == -1 && plist->width == config.m_VideoMode.m_Width && plist->height == config.m_VideoMode.m_Height ) )
		    {
                nSelectedDefaultMode = nFilteredModeCount;
		    }

            GamepadUIOption option;
            option.nValue = nFilteredModeCount++;
            option.strOptionText.SetRawUTF8( sz );
            option.userdata.nWidth = plist->width;
            option.userdata.nHeight = plist->height;
            m_pResolutionButton->AddOptionItem( option );
        }
    }

    if ( nSelectedDefaultMode == -1 )
        nSelectedDefaultMode = Max( m_pResolutionButton->GetOptionCount() - 1, 0 );

    _gamepadui_resolution.SetValue( nSelectedDefaultMode );
    m_pResolutionButton->SetToDefault();
}

void GamepadUIOptionsPanel::ClearBindings()
{
    for ( int i = 0; i < m_nTabCount; i++ )
    {
        for ( GamepadUIButton* pButton : m_Tabs[i].pButtons )
        {
            GamepadUIKeyButton* pKeyButton = dynamic_cast<GamepadUIKeyButton*>(pButton);
            if ( pKeyButton )
                pKeyButton->ClearKey();
        }
    }
}

// Mainly from GameUI
void GamepadUIOptionsPanel::FillInBindings()
{
    for ( int i = 0; i < m_nTabCount; i++ )
        m_Tabs[ i ].KeysToUnbind.RemoveAll();

	bool bJoystick = false;
	ConVarRef var( "joystick" );
	if ( var.IsValid() )
	{
		bJoystick = var.GetBool();
	}

	// NVNT see if we have a falcon connected.
	bool bFalcon = false;
	ConVarRef falconVar("hap_HasDevice");
	if ( var.IsValid() )
	{
		bFalcon = var.GetBool();
	}

    // Fill in bindings
    for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		// Look up binding
		const char *binding = GamepadUI::GetInstance().GetGameUIFuncs()->GetBindingForButtonCode((ButtonCode_t)i);
		if ( !binding )
			continue;

		// See if there is an item for this one?
        GamepadUIKeyButton *pButton = NULL;
        int nTab = 0;
        for ( int j = 0; j < m_nTabCount; j++ )
        {
            for ( GamepadUIButton *pFindButton : m_Tabs[ j ].pButtons )
            {
                GamepadUIKeyButton *pFindKeyButton = dynamic_cast< GamepadUIKeyButton* >( pFindButton );
                if ( pFindKeyButton && !V_strcmp( pFindKeyButton->GetKeyBinding(), binding ) )
                {
                    pButton = pFindKeyButton;
                    nTab = j;
                    break;
                }
            }
        }

		if ( pButton )
		{
			const char *pKeyName = g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
            const char *pCurrentKey = pButton->GetKey();
            if ( *pCurrentKey )
            {
				ButtonCode_t currentBC = (ButtonCode_t)GamepadUI::GetInstance().GetGameUIFuncs()->GetButtonCodeForBind( pCurrentKey );

				bool bShouldOverride = bJoystick && IsJoystickCode((ButtonCode_t)i) && !IsJoystickCode(currentBC);
				if( !bShouldOverride && bFalcon && IsNovintCode((ButtonCode_t)i) && !IsNovintCode(currentBC) )
					bShouldOverride = true;

				if ( !bShouldOverride )
					continue;

				m_Tabs[ nTab ].KeysToUnbind.FindAndRemove( pCurrentKey );
            }
			pButton->SetKey( pKeyName );
            m_Tabs[ nTab ].KeysToUnbind.AddToTail( pKeyName );
		}
	}
}

void GamepadUIOptionsPanel::ApplyKeyBindings()
{
    const int nTab = GetActiveTab();
    for ( int i = 0; i < m_Tabs[ nTab ].KeysToUnbind.Count(); i++ )
	{
	    char buff[256];
	    Q_snprintf( buff, sizeof(buff), "unbind \"%s\"\n", m_Tabs[ nTab].KeysToUnbind[ i ].String() );
	    GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( buff );
	}
    m_Tabs[ nTab ].KeysToUnbind.RemoveAll();

    for ( GamepadUIButton *pButton : m_Tabs[ nTab ].pButtons )
    {
        GamepadUIKeyButton *pKeyButton = dynamic_cast< GamepadUIKeyButton* >( pButton );
        if ( !pKeyButton )
            continue;

        const char *pKey = pKeyButton->GetKey();
        if ( !pKey )
            continue;

	    char buff[256];
	    Q_snprintf( buff, sizeof(buff), "bind \"%s\" \"%s\"\n", pKey, pKeyButton->GetKeyBinding() );
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( buff );
    }
}

void GamepadUIOptionsPanel::OnKeyBound( const char *pKey )
{
    for ( int i = 0; i < m_nTabCount; i++ )
    {
        for ( GamepadUIButton* pFindButton : m_Tabs[i].pButtons )
        {
            GamepadUIKeyButton* pFindKeyButton = dynamic_cast<GamepadUIKeyButton*>(pFindButton);
            if ( pFindKeyButton && !V_strcmp( pFindKeyButton->GetKey(), pKey ) )
                pFindKeyButton->ClearKey();
        }
    }
}

void GamepadUIOptionsPanel::OnKeyCodePressed( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );


    switch ( buttonCode )
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        case STEAMCONTROLLER_LEFT_BUMPER:
#else
        case KEY_XBUTTON_LEFT_SHOULDER:
#endif
            SetActiveTab( GetActiveTab() - 1 );
            break;

#ifdef HL2_RETAIL
        case STEAMCONTROLLER_RIGHT_BUMPER:
#else
        case KEY_XBUTTON_RIGHT_SHOULDER:
#endif
            SetActiveTab( GetActiveTab() + 1 );
            break;
        default:
            return BaseClass::OnKeyCodePressed( code );
    }
}

void GamepadUIOptionsPanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    GamepadUIButton *pButton = dynamic_cast< GamepadUIButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        int nParentW, nParentH;
	    GetParent()->GetSize( nParentW, nParentH );

        const float flTabButtonHeight = m_Tabs[ GetActiveTab() ].pTabButton->m_flHeight;

        float flScrollRegion = nParentH - (m_flTabsOffsetY + m_flFooterButtonsOffsetY + m_nFooterButtonHeight + flTabButtonHeight);
        int nX, nY;
        pButton->GetPos( nX, nY );
        if ( nY + m_flFooterButtonsOffsetY + m_nFooterButtonHeight + pButton->GetTall() > nParentH || nY < m_flTabsOffsetY + flTabButtonHeight )
        {
            int nTargetY = 0;
            int nThisButton = -1;
            int nHeader = -1;
            for ( int i = 0; i < m_Tabs[ GetActiveTab() ].pButtons.Count(); i++ )
            {
                if ( m_Tabs[ GetActiveTab() ].pButtons[i] == pButton)
                {
                    nThisButton = i;
                    break;
                }
				
                // For now, headers can be identified as disabled buttons
                if (!m_Tabs[GetActiveTab()].pButtons[i]->IsEnabled())
                    nHeader = i;
                else
                    nHeader = -1;

                nTargetY += m_Tabs[ GetActiveTab() ].pButtons[i]->m_flHeight;
            }

            // This button isn't part of the current tab, so don't scroll
            if (nThisButton == -1)
                return;

            // If this button has a section header above it and we're going up, scroll to it
            if ( nHeader != -1 && nY < nParentH / 2 )
                nTargetY -= m_Tabs[ GetActiveTab() ].pButtons[ nHeader ]->m_flHeight;

            if ( nY < nParentH / 2 )
            {
                nTargetY -= (pButton->m_flHeightAnimationValue[ButtonStates::Over] / 2);
            }
            else
            {
                nTargetY -= flScrollRegion;
                nTargetY += pButton->m_flHeight;
                nTargetY += (pButton->m_flHeightAnimationValue[ButtonStates::Over] / 2);
            }

            m_Tabs[ GetActiveTab() ].ScrollState.SetScrollTarget( nTargetY, GamepadUI::GetInstance().GetTime());
        }
    }
}

void GamepadUIOptionsPanel::SetActiveTab( int nTab )
{
    nTab = Clamp( nTab, 0, Max( m_nTabCount - 1, 0 ) );

    gamepadui_last_options_tab.SetValue( nTab );
    int nActiveTab = GetActiveTab();
    for ( int i = 0; i < m_nTabCount; i++ )
        m_Tabs[ i ].pTabButton->ForceDepressed( i == nActiveTab );

    FooterButtonMask buttons = FooterButtons::Apply | FooterButtons::Back;
    if ( V_strncmp( m_Tabs[nActiveTab].pTabButton->GetName(), "Keyboard", 8 ) == 0 )
        buttons |= FooterButtons::UseDefaults;
    SetFooterButtons( buttons );

    for ( GamepadUIButton *pButton : m_Tabs[ nActiveTab ].pButtons )
    {
        if ( pButton->GetCurrentButtonState() == ButtonState::Pressed )
        {
            pButton->NavigateTo();
            return;
        }
    }

    for ( GamepadUIButton *pButton : m_Tabs[ nActiveTab ].pButtons )
    {
        if ( pButton->IsEnabled() )
        {
            pButton->NavigateTo();
            return;
        }
    }
}

void GamepadUIOptionsPanel::LoadOptionTabs( const char *pszOptionsFile )
{
    KeyValues* pDataFile = new KeyValues( "Options" );
    if ( pDataFile->LoadFromFile( g_pFullFileSystem, pszOptionsFile ) )
    {
        for ( KeyValues* pTabData = pDataFile->GetFirstSubKey(); pTabData != NULL; pTabData = pTabData->GetNextKey() )
        {
            {
                char buttonCmd[64];
                V_snprintf( buttonCmd, sizeof( buttonCmd ), "tab %d", m_nTabCount );

                auto button = new GamepadUIButton(
                    this, this,
                    GAMEPADUI_RESOURCE_FOLDER "schemetab.res",
                    buttonCmd,
                    pTabData->GetString( "title" ), "" );
                button->SetZPos( 50 );
                //button->SetFooterButton( true );
                if ( m_nTabCount == gamepadui_last_options_tab.GetInt() )
                    button->ForceDepressed( true );
                m_Tabs[ m_nTabCount ].pTabButton = button;
            }

            m_Tabs[ m_nTabCount ].pTabButton->SetName( pTabData->GetName() );
            m_Tabs[ m_nTabCount ].bAlternating = pTabData->GetBool( "alternating" );
            m_Tabs[ m_nTabCount ].bHorizontal = pTabData->GetBool( "horizontal" );

            if ( !V_strcmp( pTabData->GetString( "items_from" ), "keyboard" ) )
            {
	            char szBinding[256];
	            char szDescription[256];

	            // Load the default keys list
	            CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	            if ( !g_pFullFileSystem->ReadFile( "scripts/kb_act.lst", NULL, buf ) )
		            return;

	            const char *data = ( const char* )buf.Base();

	            char token[512];
	            while ( 1 )
	            {
		            data = UTIL_Parse( data, token, sizeof( token ) );
		            // Done.
		            if ( strlen( token ) <= 0 )
			            break;

		            Q_strncpy( szBinding, token, sizeof( szBinding ) );

		            data = UTIL_Parse( data, token, sizeof( token ) );
		            if ( strlen( token ) <= 0 )
		            {
			            break;
		            }

		            Q_strncpy( szDescription, token, sizeof( szDescription ) );

		            // Skip '======' rows
		            if ( szDescription[ 0 ] != '=' )
		            {
			            // Flag as special header row if binding is "blank"
			            if ( !stricmp( szBinding, "blank" ) )
			            {
				            // add header item
                            auto button = new GamepadUIButton(
                                this, this,
                                GAMEPADUI_RESOURCE_FOLDER "schemeoptions_sectiontitle.res",
                                "button_pressed",
                                szDescription, "" );
                            //button->SetFooterButton( true );
                            button->SetEnabled( false );
                            m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
			            }
			            else
			            {
				            // Add to list
                            auto button = new GamepadUIKeyButton(
                                szBinding, this, this,
                                GAMEPADUI_RESOURCE_FOLDER "schemeoptions_wheelywheel.res",
                                "button_pressed",
                                szDescription, "" );
                            m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
			            }
		            }
	            }

                FillInBindings();
            }

            KeyValues* pTabItems = pTabData->FindKey( "items" );
            if ( pTabItems )
            {
                for ( KeyValues* pItemData = pTabItems->GetFirstSubKey(); pItemData != NULL; pItemData = pItemData->GetNextKey() )
                {
                    const char *pItemType = pItemData->GetString( "type", "droppydown" );
                    if ( !V_strcmp( pItemType, "checkybox" ) )
                    {
                        auto button = new GamepadUICheckButton(
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_checkybox.res",
                            "button_pressed",
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ) );
                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                    else if ( !V_strcmp( pItemType, "skillyskill" ) )
                    {
                        auto button = new GamepadUISkillySkill(
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_skillyskill.res",
                            "button_pressed",
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ),
                            pItemData->GetString( "image", "" ), V_atoi( pItemData->GetString( "skill", "" ) ) );
                        button->SetMouseNavigate( false );
                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                    else if ( !V_strcmp( pItemType, "slideyslide" ) )
                    {
                        const char *pszCvar = pItemData->GetString( "convar" );
                        const char *pszCvarDepends = pItemData->GetString( "depends_on" );
                        bool bInstantApply = pItemData->GetBool( "instantapply" );
                        float flMin = pItemData->GetFloat( "min", 0.0f );
                        float flMax = pItemData->GetFloat( "max", 1.0f );
                        float flStep = pItemData->GetFloat( "step", 0.1f );
                        int nTextPrecision = pItemData->GetInt( "textprecision", -1 );
                        auto button = new GamepadUISlideySlide(
                            pszCvar, pszCvarDepends, bInstantApply, flMin, flMax, flStep, nTextPrecision,
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_slideyslide.res",
                            "button_pressed",
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ) );
                        button->SetToDefault();
                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                    else if ( !V_strcmp( pItemType, "headeryheader" ) )
                    {
				        // add header item
                        auto button = new GamepadUIButton(
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_sectiontitle.res",
                            "button_pressed",
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ) );
                        //button->SetFooterButton( true );
                        button->SetEnabled( false );
                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                    else if ( !V_strcmp( pItemType, "wheelywheel" ) )
                    {
                        const char *pszCvar = pItemData->GetString( "convar" );
                        const char *pszCvarDepends = pItemData->GetString( "depends_on" );
                        bool bInstantApply = pItemData->GetBool( "instantapply" );
                        bool bSignOnly = pItemData->GetBool( "signonly" );
                        auto button = new GamepadUIWheelyWheel(
                            pszCvar, pszCvarDepends, bInstantApply, bSignOnly,
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_wheelywheel.res",
                            "button_pressed",
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ) );

                        const char *pszOptionsFrom = pItemData->GetString( "options_from" );
                        KeyValues *pOptions = pItemData->FindKey( "options" );
                        if ( pOptions )
                        {
                            for ( KeyValues* pOptionData = pOptions->GetFirstSubKey(); pOptionData != NULL; pOptionData = pOptionData->GetNextKey() )
                            {
                                GamepadUIOption option;
                                option.nValue = V_atoi( pOptionData->GetName() );
                                option.strOptionText = GamepadUIString( pOptionData->GetString() );
                                button->AddOptionItem( option );
                            }
                        }
                        else if ( pszOptionsFrom && *pszOptionsFrom )
                        {
                            if ( !V_strcmp( pszOptionsFrom, "antialiasing" ) )
                            {
                                for ( int i = 0; i < g_nNumAAModes; i++ )
                                {
                                    GamepadUIOption option;
                                    option.nValue = i;
                                    option.strOptionText = g_AAModes[ i ].strName;
                                    button->AddOptionItem( option );
                                }
                            }
                            else if ( !V_strcmp( pszOptionsFrom, "displaymode" ) )
                            {
                                int i = 0;

                                GamepadUIOption option;
                                option.nValue = i++;
                                option.strOptionText = "#GameUI_Windowed";
                                button->AddOptionItem( option );

#ifdef HL2_RETAIL // SDK2013 does not support borderless windowed (Madi)
	                            wchar_t wszNoBorderText[ 256 ];
	                            V_swprintf_safe( wszNoBorderText, L"%ls (No Border)", option.strOptionText.String() );
                                option.nValue = i++;
                                option.strOptionText = wszNoBorderText;
                                button->AddOptionItem( option );
#endif // HL2_RETAIL

#if defined( USE_SDL ) && defined( DX_TO_GL_ABSTRACTION )
                                GamepadUIString strFullscreen = "#GameUI_Fullscreen";
	                            int numVideoDisplays = SDL_GetNumVideoDisplays();

	                            if ( numVideoDisplays <= 1 )
	                            {
                                    option.nValue = i++;
                                    option.strOptionText = strFullscreen;
                                    button->AddOptionItem( option );
	                            }
	                            else
	                            {
		                            for ( int display = 0; display < numVideoDisplays; display++ )
		                            {
			                            wchar_t wszItemText[ 256 ];
			                            V_swprintf_safe( wszItemText, L"%ls (%d)", strFullscreen.String(), display);

                                        option.nValue = i++;
                                        option.strOptionText = wszItemText;
                                        button->AddOptionItem( option );
		                            }

	                            }
#else
                                option.nValue = i++;
                                option.strOptionText = "#GameUI_Fullscreen";
                                button->AddOptionItem( option );
#endif
                            }
                            else if ( !V_strcmp( pszOptionsFrom, "resolutions" ) )
                            {
                                m_pResolutionButton = button;

                                UpdateResolutions();
                            }
                        }
                        button->SetToDefault();

                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                    else if ( !V_strcmp( pItemType, "button" ) )
                    {
                        auto button = new GamepadUIButton(
                            this, this,
                            GAMEPADUI_RESOURCE_FOLDER "schemeoptions_wheelywheel.res",
                            pItemData->GetString( "command", "button_pressed" ),
                            pItemData->GetString( "text", "" ), pItemData->GetString( "description", "" ) );
                        m_Tabs[ m_nTabCount ].pButtons.AddToTail( button );
                    }
                }
            }

            CUtlVector< GamepadUIButton* >& pButtons = m_Tabs[ m_nTabCount ].pButtons;
            for ( int i = 1; i < pButtons.Count(); i++ )
            {
                if ( m_Tabs[ m_nTabCount ].bHorizontal )
                {
                    pButtons[ i ]->SetNavLeft( pButtons[ i - 1 ] );
                    pButtons[ i - 1 ]->SetNavRight( pButtons[ i ] );
                }
                else
                {
                    pButtons[ i ]->SetNavUp( pButtons[ i - 1 ] );
                    pButtons[ i - 1 ]->SetNavDown( pButtons[ i ] );
                }
            }

            m_nTabCount++;
        }
    }
}

void GamepadUIOptionsPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );
    
    if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
    {
        float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();
        m_flTabsOffsetX *= (flScreenRatio * flScreenRatio);
    }
}

CON_COMMAND( gamepadui_openoptionsdialog, "" )
{
    new GamepadUIOptionsPanel( GamepadUI::GetInstance().GetBasePanel(), "" );
}
