//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BasePanel.h"
#include "OptionsDialog.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include <vgui_controls/AnalogBar.h>

#ifdef _X360
	#include "xbox/xbox_launch.h"
#endif
#include "IGameUIFuncs.h"
#include "GameUI_Interface.h"
#include "inputsystem/iinputsystem.h"
#include "EngineInterface.h"
#include "KeyValues.h"
#include "ModInfo.h"
#include "matchmaking/matchmakingbasepanel.h"
#include "vgui_controls/AnimationController.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define OPTION_STRING_LENGTH 64


ConVar binds_per_command( "binds_per_command", "1", 0 );

ConVar x360_resolution_widescreen_mode( "x360_resolution_widescreen_mode", "0", 0, "This is only used for reference. Changing this value does nothing" );
ConVar x360_resolution_width( "x360_resolution_width", "640", 0, "This is only used for reference. Changing this value does nothing" );
ConVar x360_resolution_height( "x360_resolution_height", "480", 0, "This is only used for reference. Changing this value does nothing" );
ConVar x360_resolution_interlaced( "x360_resolution_interlaced", "0", 0, "This is only used for reference. Changing this value does nothing" );

ConVar x360_audio_english("x360_audio_english", "0", 0, "Keeps track of whether we're forcing english in a localized language." );


enum OptionType_e
{
	OPTION_TYPE_BINARY = 0,
	OPTION_TYPE_SLIDER,
	OPTION_TYPE_CHOICE,
	OPTION_TYPE_BIND,

	OPTION_TYPE_TOTAL
};


enum SliderHomeType_e
{
	SLIDER_HOME_NONE = 0,
	SLIDER_HOME_PREV,
	SLIDER_HOME_MIN,
	SLIDER_HOME_CENTER,
	SLIDER_HOME_MAX,

	SLIDER_HOME_TYPE_TOTAL
};



struct OptionChoiceData_t
{
	char	szName[ OPTION_STRING_LENGTH ];
	char	szValue[ OPTION_STRING_LENGTH ];
};

struct OptionData_t
{
	char			szName[ OPTION_STRING_LENGTH ];
	char			szDisplayName[ OPTION_STRING_LENGTH ];

	union
	{
		char		szConvar[ OPTION_STRING_LENGTH ];	// Used by all types except binds
		char		szCommand[ OPTION_STRING_LENGTH ];	// Used exclusively by bind types
	};
	
	char			szConvar2[ OPTION_STRING_LENGTH ];	// Used for choice types that use 2 convars

	char			szConvarDef[ OPTION_STRING_LENGTH ];

	int				iPriority;

	OptionType_e	eOptionType;

	bool			bUnchangeable;
	bool			bVocalsLanguage;

	// Slider types exclusively use these
	float			fMinValue;
	float			fMaxValue;
	float			fIncValue;
	float			fSliderHomeValue;

	union
	{
		SliderHomeType_e	eSliderHomeType;	// Slider types exclusively use this int
		int					iCurrentChoice;		// Choice types exclusively use this int
		int					iNumBinds;			// Bind types exclusively use this int
	};

	CUtlVector<OptionChoiceData_t>	m_Choices;
};


class OptionsDataContainer
{
public:

	~OptionsDataContainer()
	{
		for ( int iOption = 0; iOption < m_pOptions.Count(); ++iOption )
		{
			delete (m_pOptions[ iOption ]);
			m_pOptions[ iOption ] = 0;
		}

		for ( int iOption = 0; iOption < m_pControllerOptions.Count(); ++iOption )
		{
			delete (m_pControllerOptions[ iOption ]);
			m_pControllerOptions[ iOption ] = 0;
		}
	}

public:

	CUtlVector<OptionData_t*> m_pOptions;
	CUtlVector<OptionData_t*> m_pControllerOptions;

};


static OptionsDataContainer s_OptionsDataContainer;

static CUtlVector<OptionChoiceData_t> s_DisabledOptions;


const char *UTIL_Parse( const char *data, char *token, int sizeofToken );


bool ActionsAreTheSame( const char *pchAction1, const char *pchAction2 )
{
	if ( Q_stricmp( pchAction1, pchAction2 ) == 0 )
		return true;

	if ( ( Q_stricmp( pchAction1, "+duck" ) == 0 || Q_stricmp( pchAction1, "toggle_duck" ) == 0 ) && 
		 ( Q_stricmp( pchAction2, "+duck" ) == 0 || Q_stricmp( pchAction2, "toggle_duck" ) == 0 ) )
	{
		// +duck and toggle_duck are interchangable
		return true;
	}

	if ( ( Q_stricmp( pchAction1, "+zoom" ) == 0 || Q_stricmp( pchAction1, "toggle_zoom" ) == 0 ) && 
		 ( Q_stricmp( pchAction2, "+zoom" ) == 0 || Q_stricmp( pchAction2, "toggle_zoom" ) == 0 ) )
	{
		// +zoom and toggle_zoom are interchangable
		return true;
	}

	return false;
}


COptionsDialogXbox::COptionsDialogXbox( vgui::Panel *parent, bool bControllerOptions ) : BaseClass( parent, "OptionsDialog" )
{
#ifdef _X360
	// Get out current resolution and stuff it into convars for later reference
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );
	x360_resolution_widescreen_mode.SetValue( videoMode.fIsWideScreen );
	x360_resolution_width.SetValue( static_cast<int>( videoMode.dwDisplayWidth ) );
	x360_resolution_height.SetValue( static_cast<int>( videoMode.dwDisplayHeight ) );
	x360_resolution_interlaced.SetValue( videoMode.fIsInterlaced );
#endif

	//Figure out which way duck is bound, and set the option_duck_method convar the correct way.
	const char *pDuckKey = engine->Key_LookupBinding( "+duck" );
	ButtonCode_t code = g_pInputSystem->StringToButtonCode( pDuckKey );
	const char *pDuckMode = engine->Key_BindingForKey( code );

	// NOW. If duck key is bound to +DUCK, set the convar to 0. Else, set it to 1.
	ConVarRef varOption( "option_duck_method" );

	if( pDuckMode != NULL )
	{
		if( !Q_stricmp(pDuckMode,"+duck") )
		{
			varOption.SetValue( 0 );
		}
		else
		{
			varOption.SetValue( 1 );
		}
	}

	SetSize( 32, 32 );
	SetDeleteSelfOnClose( true );
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );

	m_pFooter = new CFooterPanel( parent, "OptionsFooter" );
	m_pFooter->SetStandardDialogButtons();

	m_bControllerOptions = bControllerOptions;

	m_bOptionsChanged = false;

	// Store the old vocal language setting
	m_bOldForceEnglishAudio = x360_audio_english.GetBool();

	// Get the correct options list
	if ( !m_bControllerOptions )
		m_pOptions = &s_OptionsDataContainer.m_pOptions;
	else
		m_pOptions = &s_OptionsDataContainer.m_pControllerOptions;

	if ( m_pOptions->Count() == 0 )
	{
		// Populate it if it's hasn't been filled
		ReadOptionsFromFile( "scripts/mod_options.360.txt" );
		ReadOptionsFromFile( "scripts/options.360.txt" );
		SortOptions();
	}

	m_pSelectedOption = NULL;

	m_iSelection = 0;
	m_iScroll = 0;

	m_iXAxisState = 0;
	m_iYAxisState = 0;
	m_fNextChangeTime = 0.0f;

	m_pOptionsSelectionLeft = SETUP_PANEL( new Panel( this, "OptionsSelectionLeft" ) );
	m_pOptionsSelectionLeft2 = SETUP_PANEL( new Panel( this, "OptionsSelectionLeft2" ) );
	m_pOptionsUpArrow = new vgui::Label( this, "UpArrow", "" );
	m_pOptionsDownArrow = new vgui::Label( this, "DownArrow", "" );

	for ( int iLabel = 0; iLabel < OPTIONS_MAX_NUM_ITEMS; ++iLabel )
	{
		char szLabelName[ 64 ];

		Q_snprintf( szLabelName, sizeof( szLabelName), "OptionLabel%i", iLabel );
		m_pOptionLabels[ iLabel ] = new vgui::Label( this, szLabelName, "" );

		Q_snprintf( szLabelName, sizeof( szLabelName), "ValueLabel%i", iLabel );
		m_pValueLabels[ iLabel ] = new vgui::Label( this, szLabelName, "" );

		Q_snprintf( szLabelName, sizeof( szLabelName), "ValueBar%i", iLabel );
		m_pValueBars[ iLabel ] = new AnalogBar( this, szLabelName );
	}

	// Faster repeats for sideways 
	m_KeyRepeat.SetKeyRepeatTime( KEY_XBUTTON_LEFT, 0.08 );
	m_KeyRepeat.SetKeyRepeatTime( KEY_XBUTTON_RIGHT, 0.08 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::InitializeSliderDefaults( void )
{
	for ( int iOption = 0; iOption < m_pOptions->Count(); ++iOption )
	{
		OptionData_t *pOption = (*m_pOptions)[ iOption ];

		if ( pOption->eOptionType != OPTION_TYPE_SLIDER )
			continue;

		if( pOption->eSliderHomeType != SLIDER_HOME_PREV )
			continue;

		const char *pszConvarName = pOption->szConvar;
		if ( pOption->szConvarDef && pOption->szConvarDef[0] )
		{
			// They've specified a different convar to use as the default 
			pszConvarName = pOption->szConvarDef;
		}

		ConVarRef varOption( pszConvarName );
		pOption->fSliderHomeValue = varOption.GetFloat();
	}
}

COptionsDialogXbox::~COptionsDialogXbox()
{
	if ( m_bOldForceEnglishAudio != x360_audio_english.GetBool() )
	{
#ifdef _X360
		XboxLaunch()->SetForceEnglish( x360_audio_english.GetBool() );
#endif
		PostMessage( BasePanel()->GetVPanel(), new KeyValues( "command", "command", "QuitRestartNoConfirm" ), 0.0f );
	}

	delete m_pFooter;
	m_pFooter = NULL;
}


void COptionsDialogXbox::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nButtonGap = inResourceData->GetInt( "footer_buttongap", -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues *pControlSettings = BasePanel()->GetConsoleControlSettings()->FindKey( "OptionsDialog.res" );
	LoadControlSettings( "null", NULL, pControlSettings );

	if ( m_pFooter )
	{
		KeyValues *pFooterControlSettings = BasePanel()->GetConsoleControlSettings()->FindKey( "OptionsFooter.res" );
		m_pFooter->LoadControlSettings( "null", NULL, pFooterControlSettings );
	}

	m_SelectedColor = pScheme->GetColor( "SectionedListPanel.SelectedBgColor", Color(255, 255, 255, 255) );

	m_pOptionsSelectionLeft->SetBgColor( m_SelectedColor );
	m_pOptionsSelectionLeft->SetAlpha( 96 );

	m_pOptionsSelectionLeft2->SetBgColor( Color(0,0,0,96) );

	int iX, iY;
	m_pOptionsSelectionLeft->GetPos( iX, m_iSelectorYStart );
	m_iOptionSpacing = (m_pOptionLabels[ 0 ])->GetTall(); 

	m_hLabelFont = pScheme->GetFont( "MenuLarge" );
	m_hButtonFont = pScheme->GetFont( "GameUIButtons" );

	// Decide how many items will fit
	int iTall = GetTall();
	m_iNumItems = ( iTall - 70 ) / m_iOptionSpacing;

	if ( m_iNumItems > m_pOptions->Count() )
	{
		// There's more space in the dialog than needed, shrink it down
		m_iNumItems = m_pOptions->Count();
		iTall = m_iNumItems * m_iOptionSpacing + 70;
		SetTall( iTall );
	}

	MoveToCenterOfScreen();

	// Adjust sizes for the number of items
	vgui::Panel *pPanel = FindChildByName( "OptionsBackgroundLeft" );
	pPanel->SetTall( iTall - 70 );
	pPanel = FindChildByName( "OptionsBackgroundRight" );
	pPanel->SetTall( iTall - 70 );

	m_pOptionsUpArrow->GetPos( iX, iY );
	m_pOptionsUpArrow->SetPos( iX, iTall - 32 );
	m_pOptionsDownArrow->GetPos( iX, iY );
	m_pOptionsDownArrow->SetPos( iX, iTall - 32 );

	m_pValueBars[ 0 ]->SetFgColor( Color( 255, 255, 255, 200 ) );
	m_pValueBars[ 0 ]->SetBgColor( Color( 255, 255, 255, 32 ) );
	m_pValueBars[ 0 ]->SetHomeColor( Color( m_SelectedColor[0], m_SelectedColor[1], m_SelectedColor[2], 96 ) );

	// Get proper setting for items for orginal in res file
	vgui::Panel *(pPanelList[3]) = { m_pOptionLabels[ 0 ], m_pValueLabels[ 0 ], m_pValueBars[ 0 ] };

	for ( int iPanelList = 0; iPanelList < 3; ++iPanelList )
	{
		pPanel = pPanelList[ iPanelList ];

		int  iZ, iWide;
		bool bVisible;
		pPanel->GetPos( iX, iY );
		iZ = ipanel()->GetZPos( pPanel->GetVPanel() );
		iWide = pPanel->GetWide();
		iTall = pPanel->GetTall();
		bVisible = pPanel->IsVisible();

		for ( int iLabel = 1; iLabel < m_iNumItems; ++iLabel )
		{
			if ( iPanelList == 0 )
			{
				pPanel = m_pOptionLabels[ iLabel ];
				m_pOptionLabels[ iLabel ]->SetFont( m_pOptionLabels[ 0 ]->GetFont() );
			}
			else if ( iPanelList == 1 )
			{
				pPanel = m_pValueLabels[ iLabel ];
				m_pValueLabels[ iLabel ]->SetFont( m_pValueLabels[ 0 ]->GetFont() );
			}
			else if ( iPanelList == 2 )
			{
				pPanel = m_pValueBars[ iLabel ];
				m_pValueBars[ iLabel ]->SetFgColor( m_pValueBars[ 0 ]->GetFgColor() );
				m_pValueBars[ iLabel ]->SetBgColor( m_pValueBars[ 0 ]->GetBgColor() );
				m_pValueBars[ iLabel ]->SetHomeColor( m_pValueBars[ 0 ]->GetHomeColor() );
			}

			pPanel->SetPos( iX, iY + iLabel * m_iOptionSpacing );
			pPanel->SetZPos( iZ );
			pPanel->SetWide( iWide );
			pPanel->SetTall( iTall );
			pPanel->SetVisible( bVisible );
		}
	}

	// Make unused items invisible
	for ( int iLabel = m_iNumItems; iLabel < OPTIONS_MAX_NUM_ITEMS; ++iLabel )
	{
		m_pOptionLabels[ iLabel ]->SetVisible( false );
		m_pValueLabels[ iLabel ]->SetVisible( false );
		m_pValueBars[ iLabel ]->SetVisible( false );
	}

	InitializeSliderDefaults();
	UpdateScroll();
	DeactivateSelection();
	UpdateFooter();

	// Don't fade the background for options so that brightness can be easily adjusted
	if ( !m_bControllerOptions )
		vgui::GetAnimationController()->RunAnimationCommand( BasePanel(), "m_flBackgroundFillAlpha", 0.01f, 0.0f, 1.0f, AnimationController::INTERPOLATOR_LINEAR );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::OnClose( void )
{
	CMatchmakingBasePanel *pBase = BasePanel()->GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->ShowFooter( true );
	}

	ConVarRef varOption( "option_duck_method" );

	char szCommand[ 256 ];

	for ( int iCode = 0; iCode < BUTTON_CODE_LAST; ++iCode )
	{
		ButtonCode_t code = static_cast<ButtonCode_t>( iCode );

		const char *pDuckKeyName = gameuifuncs->GetBindingForButtonCode( code );

		// Check if there's a binding for this key
		if ( !pDuckKeyName || !pDuckKeyName[0] )
			continue;

		// If we use this binding, display the key in our list
		if ( ActionsAreTheSame( pDuckKeyName, "+duck" ) )
		{
			if( varOption.GetBool() )
			{
				// Bind DUCK key to toggle_duck
				Q_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( code ), "toggle_duck" );
				engine->ClientCmd_Unrestricted( szCommand );
			}
			else
			{
				// Bind DUCK key to +DUCK
				Q_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( code ), "+DUCK" );
				engine->ClientCmd_Unrestricted( szCommand );
			}	
		}
	}

	// Save these settings!
	if ( m_bOptionsChanged )
		engine->ClientCmd_Unrestricted( "host_writeconfig" );

	BasePanel()->RunCloseAnimation( "CloseOptionsDialog_OpenMainMenu" );
	if ( !m_bControllerOptions )
		vgui::GetAnimationController()->RunAnimationCommand( BasePanel(), "m_flBackgroundFillAlpha", 120.0f, 0.0f, 1.0f, AnimationController::INTERPOLATOR_LINEAR );

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( IsX360() )
	{
		if ( GetAlpha() != 255 )
		{
			// inhibit key activity during transitions
			return;
		}
	}

	if ( !m_bSelectionActive )
		HandleInactiveKeyCodePressed( code );
	else if ( m_pSelectedOption->eOptionType != OPTION_TYPE_BIND )
		HandleActiveKeyCodePressed( code );
	else
		HandleBindKeyCodePressed( code );
}

void COptionsDialogXbox::OnCommand(const char *command)
{
	m_KeyRepeat.Reset();

	if ( !Q_stricmp( command, "DefaultControls" ) )
	{
		vgui::surface()->PlaySound( "UI/buttonclick.wav" );
		FillInDefaultBindings();
	}
	else if ( !Q_stricmp( command, "RefreshOptions" ) )
	{
		UncacheChoices();
		UpdateScroll();
	}
	else if ( !Q_stricmp( command, "AcceptVocalsLanguageChange" ) )
	{
		OnClose();
	}
	else if ( !Q_stricmp( command, "CancelVocalsLanguageChange" ) )
	{
		vgui::surface()->PlaySound( "UI/buttonclick.wav" );
		x360_audio_english.SetValue( m_bOldForceEnglishAudio );
		OnCommand( "RefreshOptions" );
		OnClose();
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::OnKeyCodeReleased( vgui::KeyCode code )
{
	m_KeyRepeat.KeyUp( code );

	BaseClass::OnKeyCodeReleased( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsDialogXbox::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		OnKeyCodePressed( code );
	}

	BaseClass::OnThink();
}

void COptionsDialogXbox::HandleInactiveKeyCodePressed( vgui::KeyCode code )
{
	m_KeyRepeat.KeyDown( code );

	OptionType_e eOptionType = (*m_pOptions)[ m_iSelection ]->eOptionType;
	switch( code )
	{
		// Change the selected value
		case KEY_XBUTTON_A:
		case STEAMCONTROLLER_A:
			if ( !(*m_pOptions)[ m_iSelection ]->bUnchangeable )
			{
				// Don't allow more binds if it's already maxed out
				if ( eOptionType == OPTION_TYPE_BIND && ( (*m_pOptions)[ m_iSelection ]->iNumBinds < binds_per_command.GetInt() || binds_per_command.GetInt() == 1 ) )
				{
					ActivateSelection();
					vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
				}
				else if ( eOptionType == OPTION_TYPE_BINARY || eOptionType == OPTION_TYPE_CHOICE )
				{
					ActivateSelection();
					ChangeValue( 1 );
					DeactivateSelection();
				}
			}
			else if ( !(*m_pOptions)[ m_iSelection ]->bVocalsLanguage )
			{
				BasePanel()->ShowMessageDialog( MD_OPTION_CHANGE_FROM_X360_DASHBOARD, this );
			}
			break;

		// To the main menu
		case KEY_XBUTTON_B:
		case STEAMCONTROLLER_B:
			if ( m_bOldForceEnglishAudio != x360_audio_english.GetBool() )
			{
				// Pop up a dialog to confirm changing the language
				vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
				BasePanel()->ShowMessageDialog( MD_SAVE_BEFORE_LANGUAGE_CHANGE, this );
			}
			else
			{
				OnClose();
			}
			break;

		// Move the selection up and down
		case KEY_XBUTTON_UP:
		case KEY_XSTICK1_UP:
		case STEAMCONTROLLER_DPAD_UP:
			ChangeSelection( -1 );
			break;

		case KEY_XBUTTON_DOWN:
		case KEY_XSTICK1_DOWN:
		case STEAMCONTROLLER_DPAD_DOWN:
			ChangeSelection( 1 );
			break;

		// Quickly change in the negative direction
		case KEY_XBUTTON_LEFT:
		case KEY_XSTICK1_LEFT:
		case STEAMCONTROLLER_DPAD_LEFT:
			if ( !(*m_pOptions)[ m_iSelection ]->bUnchangeable )
			{
				if ( eOptionType != OPTION_TYPE_BIND )
				{
					ActivateSelection();
					ChangeValue( -1 );
					DeactivateSelection();
				}
			}
			else if ( !(*m_pOptions)[ m_iSelection ]->bVocalsLanguage )
			{
				BasePanel()->ShowMessageDialog( MD_OPTION_CHANGE_FROM_X360_DASHBOARD, this );
			}
			break;

		// Quickly change in the positive direction
		case KEY_XBUTTON_RIGHT:
		case KEY_XSTICK1_RIGHT:
		case STEAMCONTROLLER_DPAD_RIGHT:
			if ( !(*m_pOptions)[ m_iSelection ]->bUnchangeable )
			{
				if ( eOptionType != OPTION_TYPE_BIND )
				{
					ActivateSelection();
					ChangeValue( 1 );
					DeactivateSelection();
				}
			}
			else if ( !(*m_pOptions)[ m_iSelection ]->bVocalsLanguage )
			{
				BasePanel()->ShowMessageDialog( MD_OPTION_CHANGE_FROM_X360_DASHBOARD, this );
			}
			break;

		case KEY_XBUTTON_X:
		case STEAMCONTROLLER_X:
			if ( !(*m_pOptions)[ m_iSelection ]->bUnchangeable )
			{
				if ( eOptionType == OPTION_TYPE_BIND )
				{
					if ( (*m_pOptions)[ m_iSelection ]->iNumBinds != 0 )
					{
						ActivateSelection();
						m_bOptionsChanged = true;
						vgui::surface()->PlaySound( "UI/buttonclick.wav" );
						UnbindOption( m_pSelectedOption, GetSelectionLabel() );
						DeactivateSelection();
					}
				}
			}
			break;

		case KEY_XBUTTON_Y:
		case STEAMCONTROLLER_Y:
			if ( m_bControllerOptions )
			{
				m_bOptionsChanged = true;
				vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
				BasePanel()->ShowMessageDialog( MD_DEFAULT_CONTROLS_CONFIRM, this );
			}
			else
			{
				BasePanel()->OnChangeStorageDevice();
			}
			break;

		default:
			break;
	}
}

void COptionsDialogXbox::HandleActiveKeyCodePressed( vgui::KeyCode code )
{
	// Only binds types should become active!
	DeactivateSelection();
}

void COptionsDialogXbox::HandleBindKeyCodePressed( vgui::KeyCode code )
{
	// Don't let stick movements be bound
	if ( ( code >= KEY_XSTICK1_RIGHT && code <= KEY_XSTICK1_UP ) || 
		 ( code >= KEY_XSTICK2_RIGHT && code <= KEY_XSTICK2_UP ) )
		return;

	if ( code == KEY_XBUTTON_START )
	{
		vgui::surface()->PlaySound( "UI/buttonrollover.wav" );
		UpdateValue( m_pSelectedOption, GetSelectionLabel() );
	}
	else
		ChangeValue( static_cast<int>( code ) );

	DeactivateSelection();
}

void COptionsDialogXbox::ActivateSelection( void )
{
	m_bSelectionActive = true;
	m_pSelectedOption = (*m_pOptions)[ m_iSelection ];

	if ( !m_pSelectedOption || m_pSelectedOption->eOptionType != OPTION_TYPE_BIND )
		return;

	m_KeyRepeat.Reset();

	m_pOptionsSelectionLeft->SetAlpha( 255 );

	int iLabel = GetSelectionLabel();

	m_pOptionLabels[ iLabel ]->SetFgColor( Color( 0, 0, 0, 255 ) );

	m_pValueLabels[ iLabel ]->SetFont( m_hLabelFont );
	m_pValueLabels[ iLabel ]->SetText( "#GameUI_SetNewButton" );

	UpdateFooter();
}

void COptionsDialogXbox::DeactivateSelection( void )
{
	m_bSelectionActive = false;

	if ( !m_pSelectedOption || m_pSelectedOption->eOptionType != OPTION_TYPE_BIND )
		return;

	m_pOptionsSelectionLeft->SetAlpha( 96 );

	int iLabel = GetSelectionLabel();

	m_pOptionLabels[ iLabel ]->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pValueLabels[ iLabel ]->SetFgColor( Color( 255, 255, 255, 255 ) );

	UpdateFooter();
}

void COptionsDialogXbox::ChangeSelection( int iChange )
{
	m_iSelection += iChange;

	if ( m_iSelection < 0 )
	{
		m_iSelection = 0;
		vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
	}
	else if ( m_iSelection >= m_pOptions->Count() )
	{
		m_iSelection = m_pOptions->Count() - 1;
		vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
	}
	else
	{
		vgui::surface()->PlaySound( "UI/buttonrollover.wav" );
	}

	// Make sure the selected item in in the window
	if ( m_iSelection < m_iScroll )
	{
		m_iScroll = m_iSelection;
		UpdateScroll();
	}
	else if ( GetSelectionLabel() >= m_iNumItems )
	{
		m_iScroll = m_iSelection - m_iNumItems + 1;
		UpdateScroll();
	}

	UpdateFooter();

	UpdateSelection();
}

void COptionsDialogXbox::UpdateFooter( void )
{
	m_pFooter->ClearButtons();

	if ( !m_bSelectionActive )
	{
		if ( !(*m_pOptions)[ m_iSelection ]->bUnchangeable )
		{
			switch ( (*m_pOptions)[ m_iSelection ]->eOptionType )
			{
				case OPTION_TYPE_BINARY:
					m_pFooter->AddNewButtonLabel( "#GameUI_Modify", "#GameUI_Icons_DPAD" );
					break;

				case OPTION_TYPE_SLIDER:
					m_pFooter->AddNewButtonLabel( "#GameUI_Modify", "#GameUI_Icons_DPAD" );
					break;

				case OPTION_TYPE_CHOICE:
					m_pFooter->AddNewButtonLabel( ( (*m_pOptions)[ m_iSelection ]->m_Choices.Count() == 2 ) ? ( "#GameUI_Toggle" ) : ( "#GameUI_Modify" ), "#GameUI_Icons_DPAD" );
					break;

				case OPTION_TYPE_BIND:
				{
					if ( (*m_pOptions)[ m_iSelection ]->iNumBinds < binds_per_command.GetInt() || binds_per_command.GetInt() == 1 )
						m_pFooter->AddNewButtonLabel( "#GameUI_Modify", "#GameUI_Icons_A_BUTTON" );
					if ( (*m_pOptions)[ m_iSelection ]->iNumBinds != 0 )
						m_pFooter->AddNewButtonLabel( "#GameUI_ClearButton", "#GameUI_Icons_X_BUTTON" );
					break;
				}
			}
		}

		if ( m_bControllerOptions )
			m_pFooter->AddNewButtonLabel( "#GameUI_DefaultButtons", "#GameUI_Icons_Y_BUTTON" );
		else
			m_pFooter->AddNewButtonLabel( "#GameUI_Console_StorageChange", "#GameUI_Icons_Y_BUTTON" );

		m_pFooter->AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
	}
	else if ( m_pSelectedOption )
	{
		switch ( m_pSelectedOption->eOptionType )
		{
			case OPTION_TYPE_BIND:
				m_pFooter->AddNewButtonLabel( "#GameUI_Cancel", "#GameUI_Icons_START" );			
				break;
		}
	}

	if ( m_nButtonGap > 0 )
	{
		m_pFooter->SetButtonGap( m_nButtonGap );
	}
	else
	{
		m_pFooter->UseDefaultButtonGap();
	}

	CMatchmakingBasePanel *pBase = BasePanel()->GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->ShowFooter( false );
	}
}

void COptionsDialogXbox::UpdateSelection( void )
{
	int iYPos = m_iSelectorYStart + m_iOptionSpacing * ( GetSelectionLabel() );

	int iX, iY;
	m_pOptionsSelectionLeft->GetPos( iX, iY );
	m_pOptionsSelectionLeft->SetPos( iX, iYPos );
	m_pOptionsSelectionLeft2->GetPos( iX, iY );
	m_pOptionsSelectionLeft2->SetPos( iX, iYPos + 2 );
}

void COptionsDialogXbox::UpdateScroll( void )
{
	for ( int iLabel = 0; iLabel < m_iNumItems; ++iLabel )
	{
		int iOption = m_iScroll + iLabel;

		if ( iOption >= m_pOptions->Count() )
			break;

		OptionData_t *pOption = (*m_pOptions)[ iOption ];

		m_pOptionLabels[ iLabel ]->SetText( pOption->szDisplayName );

		UpdateValue( pOption, iLabel );

		if ( pOption->bUnchangeable )
		{
			m_pOptionLabels[ iLabel ]->SetFgColor( Color( 128, 128, 128, 255 ) );
			m_pValueLabels[ iLabel ]->SetFgColor( Color( 128, 128, 128, 255 ) );
		}
		else
		{
			m_pOptionLabels[ iLabel ]->SetFgColor( Color( 200, 200, 200, 255 ) );
			m_pValueLabels[ iLabel ]->SetFgColor( Color( 200, 200, 200, 255 ) );
		}

	}

	// Draw arrows if there's more items to scroll to
	m_pOptionsUpArrow->SetAlpha( ( m_iScroll > 0 ) ? ( 255 ) : ( 64 ) );
	m_pOptionsDownArrow->SetAlpha( ( m_iScroll + m_iNumItems < m_pOptions->Count() ) ? ( 255 ) : ( 64 ) );
}

void COptionsDialogXbox::UncacheChoices( void )
{
	for ( int iOption = 0; iOption < m_pOptions->Count(); ++iOption )
	{
		OptionData_t *pOption = (*m_pOptions)[ iOption ];

		if ( pOption->eOptionType != OPTION_TYPE_CHOICE )
			continue;

		pOption->iCurrentChoice = -1;
	}
}

void COptionsDialogXbox::GetChoiceFromConvar( OptionData_t *pOption )
{
	if ( pOption->iCurrentChoice < 0 )
	{
		// Don't have a proper choice yet, so see if the convars value matches one of our choices
		if ( pOption->szConvar2[ 0 ] == '\0' )
		{
			ConVarRef varOption( pOption->szConvar );
			const char *pchValue = varOption.GetString();

			// Only one convar to check
			for ( int iChoice = 0; iChoice < pOption->m_Choices.Count(); ++iChoice )
			{
				if ( Q_stricmp( pOption->m_Choices[ iChoice ].szValue, pchValue ) == 0 )
				{
					pOption->iCurrentChoice = iChoice;
					break;
				}

				// We need to compare values in case we have "0" & "0.00000". 
				if ( (pchValue[0] >= '0' && pchValue[0] <= '9') || pchValue[0] == '-' )
				{
					float flVal = atof(pchValue);
					float flChoiceVal = atof(pOption->m_Choices[ iChoice ].szValue);
					if ( flVal == flChoiceVal )
					{
						pOption->iCurrentChoice = iChoice;
						break;
					}
				}
			}
		}
		else
		{
			// Two convars to contend with
			ConVarRef varOption( pOption->szConvar );
			ConVarRef varOption2( pOption->szConvar2 );

			const char *pchValue = varOption.GetString();
			const char *pchValue2 = varOption2.GetString();

			for ( int iChoice = 0; iChoice < pOption->m_Choices.Count(); ++iChoice )
			{
				char szOptionValue[ OPTION_STRING_LENGTH ];
				Q_strncpy( szOptionValue, pOption->m_Choices[ iChoice ].szValue, sizeof( szOptionValue ) );

				char *pchOptionValue2 = const_cast<char*>( Q_strnchr( szOptionValue, ';', sizeof( szOptionValue ) ) );

				AssertMsg( pchOptionValue2, "Option uses 2 convars but an option doesn't have 2 ; separated values!" );

				pchOptionValue2[ 0 ] = '\0';	// Set this as the end of the other value
				++pchOptionValue2;				// Point at next char

				if ( Q_stricmp( szOptionValue, pchValue ) == 0 && Q_stricmp( pchOptionValue2, pchValue2 ) == 0 )
				{
					pOption->iCurrentChoice = iChoice;
					break;
				}
			}
		}
	}
}

#if !defined(_X360)
// BUGBUG: This won't compile because it includes windows.h and this interface function is #defined to something else
// see below system()->GetCurrentTime
#undef GetCurrentTime
#endif

void COptionsDialogXbox::ChangeValue( float fChange )
{
	switch ( m_pSelectedOption->eOptionType )
	{
		case OPTION_TYPE_BINARY:
		{
			ConVarRef varOption( m_pSelectedOption->szConvar );
			varOption.SetValue( !varOption.GetBool() );

			UpdateValue( m_pSelectedOption, GetSelectionLabel() );

			m_bOptionsChanged = true;
			vgui::surface()->PlaySound( "UI/buttonclick.wav" );

			break;
		}

		case OPTION_TYPE_SLIDER:
		{
			if ( system()->GetCurrentTime() > m_fNextChangeTime )
			{
				ConVarRef varOption( m_pSelectedOption->szConvar );

				float fNumSegments = m_pValueBars[ 0 ]->GetTotalSegmentCount();
				if ( fNumSegments <= 0.0f )
					fNumSegments = 1.0f;

				float fIncValue;

				if ( m_pSelectedOption->fIncValue == 0.0f )
					fIncValue = ( m_pSelectedOption->fMaxValue - m_pSelectedOption->fMinValue ) * ( 1.0f / fNumSegments );
				else
					fIncValue = m_pSelectedOption->fIncValue * ( m_pSelectedOption->fMaxValue - m_pSelectedOption->fMinValue ) * ( 1.0f / fNumSegments );

				fIncValue *= fChange;

				float fOldValue = varOption.GetFloat();
				float fValue = clamp( fOldValue + fIncValue, m_pSelectedOption->fMinValue, m_pSelectedOption->fMaxValue );

				if ( fOldValue != fValue )
				{
					m_bOptionsChanged = true;
					vgui::surface()->PlaySound( "UI/buttonclick.wav" );

					varOption.SetValue( fValue );
					UpdateValue( m_pSelectedOption, GetSelectionLabel() );
				}
				else
				{
					// Play the invalid sound
					vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
				}
			}

			break;
		}

		case OPTION_TYPE_CHOICE:
		{
			GetChoiceFromConvar( m_pSelectedOption );

			if ( m_pSelectedOption->iCurrentChoice >= 0 )
			{
				m_pSelectedOption->iCurrentChoice += fChange;

				while ( m_pSelectedOption->iCurrentChoice >= m_pSelectedOption->m_Choices.Count() )
					m_pSelectedOption->iCurrentChoice -= m_pSelectedOption->m_Choices.Count();

				while ( m_pSelectedOption->iCurrentChoice < 0 )
					m_pSelectedOption->iCurrentChoice += m_pSelectedOption->m_Choices.Count();

				if ( m_pSelectedOption->szConvar2[ 0 ] == '\0' )
				{
					// Only one convar to set
					ConVarRef varOption( m_pSelectedOption->szConvar );
					varOption.SetValue( m_pSelectedOption->m_Choices[ m_pSelectedOption->iCurrentChoice ].szValue );
				}
				else
				{
					// Two convars to deal with
					char szOptionValue[ OPTION_STRING_LENGTH ];
					Q_strncpy( szOptionValue, m_pSelectedOption->m_Choices[ m_pSelectedOption->iCurrentChoice ].szValue, sizeof( szOptionValue ) );

					char *pchOptionValue2 = const_cast<char*>( Q_strnchr( szOptionValue, ';', sizeof( szOptionValue ) ) );

					AssertMsg( pchOptionValue2, "Option uses to convars but an option doesn't have 2 ; separated values!" );

					pchOptionValue2[ 0 ] = '\0';	// Set this as the end of the other value
					++pchOptionValue2;				// Point at next char

					ConVarRef varOption( m_pSelectedOption->szConvar );
					ConVarRef varOption2( m_pSelectedOption->szConvar2 );

					varOption.SetValue( szOptionValue );
					varOption2.SetValue( pchOptionValue2 );
				}
			}
			else
			{
				DevWarning( "ConVar \"%s\" set to value that's not a choice used by \"%s\" option.", m_pSelectedOption->szConvar, m_pSelectedOption->szDisplayName );
			}

			UpdateValue( m_pSelectedOption, GetSelectionLabel() );

			m_bOptionsChanged = true;
			vgui::surface()->PlaySound( "UI/buttonclick.wav" );

			break;
		}

		case OPTION_TYPE_BIND:
		{
			// If we only allow one bind replace the previous
			if ( binds_per_command.GetInt() == 1 )
				UnbindOption( m_pSelectedOption, GetSelectionLabel() );

			ButtonCode_t code = static_cast<ButtonCode_t>( static_cast<int>( fChange ) );

			char szCommand[ 256 ];
			Q_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( code ), m_pSelectedOption->szCommand );
			engine->ClientCmd_Unrestricted( szCommand );

			// After binding we need to update all bindings so they display the correct keys
			UpdateAllBinds( code );

			m_bOptionsChanged = true;
			vgui::surface()->PlaySound( "UI/buttonclick.wav" );
			
			break;
		}
	}
}

void COptionsDialogXbox::UnbindOption( OptionData_t *pOption, int iLabel )
{
	if ( pOption->eOptionType != OPTION_TYPE_BIND )
		return;

	for ( int iCode = 0; iCode < BUTTON_CODE_LAST; ++iCode )
	{
		ButtonCode_t code = static_cast<ButtonCode_t>( iCode );

		const char *pBinding = gameuifuncs->GetBindingForButtonCode( code );

		// Check if there's a binding for this key
		if ( !pBinding || !pBinding[0] )
			continue;

		// If we use this binding, display the key in our list
		if ( ActionsAreTheSame( pBinding, pOption->szCommand ) )
		{
			char szCommand[ 256 ];
			Q_snprintf( szCommand, sizeof( szCommand ), "unbind %s", g_pInputSystem->ButtonCodeToString( code ) );
			engine->ClientCmd_Unrestricted( szCommand );
		}
	}

	pOption->iNumBinds = 0;

	if ( iLabel < 0 || iLabel >= m_iNumItems )
		return;

	m_pValueLabels[ iLabel ]->SetFont( m_hLabelFont );
	m_pValueLabels[ iLabel ]->SetText( "" );
}

void COptionsDialogXbox::UpdateValue( OptionData_t *pOption, int iLabel )
{
	if ( pOption->bVocalsLanguage )
	{
		// Can't change vocal language while in game
		pOption->bUnchangeable = GameUI().IsInLevel();
	}

	switch ( pOption->eOptionType )
	{
		case OPTION_TYPE_BINARY:
		{
			ConVarRef varOption( pOption->szConvar );

			m_pValueBars[ iLabel ]->SetVisible( false );
			m_pValueLabels[ iLabel ]->SetVisible( true );
			m_pValueLabels[ iLabel ]->SetFont( m_hLabelFont );
			m_pValueLabels[ iLabel ]->SetText( ( varOption.GetBool() ) ? ( "#GameUI_Enable" ) : ( "#GameUI_Disable" ) );
			break;
		}

		case OPTION_TYPE_SLIDER:
		{
			ConVarRef varOption( pOption->szConvar );

			m_pValueLabels[ iLabel ]->SetVisible( false );
			m_pValueBars[ iLabel ]->SetVisible( true );

			// If it's very close to the home value set it as the home value
			float fNumSegments = m_pValueBars[ 0 ]->GetTotalSegmentCount();
			if ( fNumSegments <= 0.0f )
				fNumSegments = 1.0f;

			float fIncValue;

			if ( pOption->fIncValue == 0.0f )
				fIncValue = ( pOption->fMaxValue - pOption->fMinValue ) * ( 1.0f / fNumSegments );
			else
				fIncValue = pOption->fIncValue * ( pOption->fMaxValue - pOption->fMinValue ) * ( 1.0f / fNumSegments );

			float fNormalizeHome = fabsf( ( pOption->fMinValue - pOption->fSliderHomeValue ) / ( pOption->fMaxValue - pOption->fMinValue ) );
			if ( pOption->fIncValue < 0.0f )
				fNormalizeHome = 1.0f - fNormalizeHome;
			m_pValueBars[ iLabel ]->SetHomeValue( fNormalizeHome );

			float fNormalizeValue = fabsf( ( pOption->fMinValue - varOption.GetFloat() ) / ( pOption->fMaxValue - pOption->fMinValue ) );
			if ( pOption->fIncValue < 0.0f )
				fNormalizeValue = 1.0f - fNormalizeValue;

			// atof accuracy for convar isn't so good, so snap to the home value if we're near it.
			if ( fabsf( fNormalizeValue - fNormalizeHome ) < fabsf( fIncValue * 0.1f ) )
				fNormalizeValue = fNormalizeHome;

			m_pValueBars[ iLabel ]->SetAnalogValue( fNormalizeValue );

			break;
		}

		case OPTION_TYPE_CHOICE:
		{
			GetChoiceFromConvar( pOption );

			m_pValueBars[ iLabel ]->SetVisible( false );
			m_pValueLabels[ iLabel ]->SetVisible( true );
			m_pValueLabels[ iLabel ]->SetFont( m_hLabelFont );
			if ( pOption->iCurrentChoice >= 0 )
			{
				m_pValueLabels[ iLabel ]->SetText( pOption->m_Choices[ pOption->iCurrentChoice ].szName );
			}
			else
			{
				if ( pOption->bUnchangeable )
				{
					if ( pOption->szConvar2[ 0 ] == '\0' )
					{
						ConVarRef varOption( pOption->szConvar );
						m_pValueLabels[ iLabel ]->SetText( varOption.GetString() );
					}
					else
					{
						// This is used for displaying the resolution settings
						ConVarRef varOption( pOption->szConvar );
						ConVarRef varOption2( pOption->szConvar2 );
						char szBuff[ 256 ];
						Q_snprintf( szBuff, sizeof( szBuff ), "%sx%s%s", varOption.GetString(), varOption2.GetString(), ( x360_resolution_interlaced.GetBool() ) ? ( "i" ) : ( "p" ) );
						m_pValueLabels[ iLabel ]->SetText( szBuff );
					}
				}
				else
				{
					DevWarning( "ConVar \"%s\" set to value that's not a choice used by \"%s\" option.", pOption->szConvar, pOption->szDisplayName );
					m_pValueLabels[ iLabel ]->SetText( "#GameUI_NoOptionsYet" );
				}
			}

			break;
		}

		case OPTION_TYPE_BIND:
		{
			UpdateBind( pOption, iLabel );
			break;
		}
	}
}

void COptionsDialogXbox::UpdateBind( OptionData_t *pOption, int iLabel, ButtonCode_t codeIgnore, ButtonCode_t codeAdd )
{
	int iNumBinds = 0;
	char szBinds[ OPTION_STRING_LENGTH ];

	char szBuff[ 512 ];
	wchar_t szWideBuff[ 64 ];

	for ( int iCode = 0; iCode < BUTTON_CODE_LAST; ++iCode )
	{
		ButtonCode_t code = static_cast<ButtonCode_t>( iCode );

		// Don't show this key in our list
		if ( code == codeIgnore )
			continue;

		bool bUseThisKey = ( codeAdd == code );

		if ( !bUseThisKey )
		{
			// If this binding is being replaced only allow the new binding to show
			if ( binds_per_command.GetInt() == 1 && codeAdd != BUTTON_CODE_INVALID )
				continue;

			// Only check against bind name if we haven't already forced this binding to be used
			const char *pBinding = gameuifuncs->GetBindingForButtonCode( code );

			if ( !pBinding )
				continue;

			bUseThisKey = ActionsAreTheSame( pBinding, pOption->szCommand );
		}

		// Don't use this bind in out list
		if ( !bUseThisKey )
			continue;

		// Turn localized string into icon character
		Q_snprintf( szBuff, sizeof( szBuff ), "#GameUI_Icons_%s", g_pInputSystem->ButtonCodeToString( static_cast<ButtonCode_t>( iCode ) ) );
		g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( szBuff ), 0 );
		g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff, sizeof( szBuff ) );

		// Add this icon to our list of keys to display
		szBinds[ iNumBinds ] = szBuff[ 0 ];
		++iNumBinds;
	}

	if ( iNumBinds == 0 )
	{
		// No keys for this bind
		pOption->iNumBinds = 0;
		m_pValueBars[ iLabel ]->SetVisible( false );
		m_pValueLabels[ iLabel ]->SetVisible( true );
		m_pValueLabels[ iLabel ]->SetFont( m_hLabelFont );
		m_pValueLabels[ iLabel ]->SetText( "" );
	}
	else
	{
		// Show icons for list of keys
		szBinds[ iNumBinds ] = '\0';
		pOption->iNumBinds = iNumBinds;
		m_pValueBars[ iLabel ]->SetVisible( false );
		m_pValueLabels[ iLabel ]->SetVisible( true );
		m_pValueLabels[ iLabel ]->SetFont( m_hButtonFont );
		m_pValueLabels[ iLabel ]->SetText( szBinds );
	}
}

void COptionsDialogXbox::UpdateAllBinds( ButtonCode_t code )
{
	// Loop through all the items
	for ( int iLabel = 0; iLabel < m_iNumItems; ++iLabel )
	{
		int iOption = m_iScroll + iLabel;

		if ( iOption >= m_pOptions->Count() )
			break;

		OptionData_t *pOption = (*m_pOptions)[ iOption ];

		if ( pOption->eOptionType == OPTION_TYPE_BIND )
		{
			if ( pOption == m_pSelectedOption )
			{
				// We just added a key to this binding so add it to the list
				UpdateBind( pOption, iLabel, BUTTON_CODE_INVALID, code );
			}
			else
			{
				// We just added a key so don't let it show up for any other bindings
				UpdateBind( pOption, iLabel, code );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read in defaults from game's default config file and populate list 
//			using those defaults
//-----------------------------------------------------------------------------
void COptionsDialogXbox::FillInDefaultBindings( void )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( "cfg/config.360.cfg", NULL, buf ) )
		return;

	// Clear out all current bindings
	for ( int iOption = 0; iOption < m_pOptions->Count(); ++iOption )
		UnbindOption( (*m_pOptions)[ iOption ], iOption - m_iScroll );

	const char *data = (const char*)buf.Base();

	// loop through all the binding
	while ( data != NULL )
	{
		char cmd[64];
		data = UTIL_Parse( data, cmd, sizeof(cmd) );
		if ( strlen( cmd ) <= 0 )
			break;

		if ( !stricmp(cmd, "bind") )
		{
			// Key name
			char szKeyName[256];
			data = UTIL_Parse( data, szKeyName, sizeof(szKeyName) );
			if ( strlen( szKeyName ) <= 0 )
				break; // Error

			char szBinding[256];
			data = UTIL_Parse( data, szBinding, sizeof(szBinding) );
			if ( strlen( szKeyName ) <= 0 )  
				break; // Error

			// Bind it
			char szCommand[ 256 ];
			Q_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", szKeyName, szBinding );
			engine->ClientCmd_Unrestricted( szCommand );

			// Loop through all the items
			for ( int iLabel = 0; iLabel < m_iNumItems; ++iLabel )
			{
				int iOption = m_iScroll + iLabel;

				if ( iOption >= m_pOptions->Count() )
					break;

				OptionData_t *pOption = (*m_pOptions)[ iOption ];

				// Check if this bind is for this option
				if ( pOption->eOptionType == OPTION_TYPE_BIND && ActionsAreTheSame( pOption->szCommand, szBinding ) )
				{
					char szBuff[ 512 ];
					wchar_t szWideBuff[ 64 ];
					char szBinds[ OPTION_STRING_LENGTH ];
					if ( pOption->iNumBinds > 0 )
						m_pValueLabels[ iLabel ]->GetText( szBinds, sizeof( szBinds ) );

					// Turn localized string into icon character
					Q_snprintf( szBuff, sizeof( szBuff ), "#GameUI_Icons_%s", szKeyName );
					g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( szBuff ), 0 );
					g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff, sizeof( szBuff ) );

					// Add this icon to our list of keys to display
					szBinds[ pOption->iNumBinds ] = szBuff[ 0 ];
					++pOption->iNumBinds;

					// Show icons for list of keys
					szBinds[ pOption->iNumBinds ] = '\0';
					m_pValueBars[ iLabel ]->SetVisible( false );
					m_pValueLabels[ iLabel ]->SetVisible( true );
					m_pValueLabels[ iLabel ]->SetFont( m_hButtonFont );
					m_pValueLabels[ iLabel ]->SetText( szBinds );
				}
			}
		}
	}

	// Reset any options with default convar values
	for ( int iLabel = 0; iLabel < m_iNumItems; ++iLabel )
	{
		int iOption = m_iScroll + iLabel;

		if ( iOption >= m_pOptions->Count() )
			break;

		OptionData_t *pOption = (*m_pOptions)[ iOption ];

		if ( pOption->szConvarDef && pOption->szConvarDef[0] )
		{
			ConVarRef varDefault( pOption->szConvarDef );
			ConVarRef varOption( pOption->szConvar );
			varOption.SetValue( varDefault.GetFloat() );
			pOption->iCurrentChoice = -1;
			UpdateValue( pOption, iLabel );
		}
	}
}

bool COptionsDialogXbox::ShouldSkipOption( KeyValues *pKey )
{
	// Skip the option if not in developer mode and this is a developer only option
	ConVarRef developer( "developer" );
	if ( !developer.GetBool() && ( pKey->GetInt( "dev", 0 ) != 0 ) )
		return true;

	// Skip the option if it doesn't match the controller/non-controller list
	if ( m_bControllerOptions != ( pKey->GetInt( "control", 0 ) != 0 ) )
		return true;

	// Skip portal options if this mod doesn't have portals (defined in gameinfo.txt)
	if ( !ModInfo().HasPortals() && ( pKey->GetInt( "portals", 0 ) != 0 ) )
		return true;

	// Skip multiplayer only options for single player (or combo) games
	if ( ModInfo().IsSinglePlayerOnly() && !ModInfo().IsMultiplayerOnly() && ( pKey->GetInt( "multiplayer", 0 ) != 0 ) )
		return true;

	// Skip difficulty options for games that don't want them
	if ( !( ModInfo().IsSinglePlayerOnly() && !ModInfo().NoDifficulty() ) && ( pKey->GetInt( "difficulty", 0 ) != 0 ) )
		return true;

	// Skip voice options for single player games
	if ( ModInfo().IsSinglePlayerOnly() && !ModInfo().IsMultiplayerOnly() && ( pKey->GetInt( "voice", 0 ) != 0 ) )
		return true;

	// Skip if it's the vocal language option but we're not in german or french
	if ( pKey->GetInt( "vocalslanguage", 0 ) != 0 )
	{
		if ( !XBX_IsLocalized() )
		{
			return true;
		}
	}

	// Don't create options if there's already an entry by the same name
	char szName[ OPTION_STRING_LENGTH ];
	Q_strncpy( szName, pKey->GetName(), sizeof( szName ) );

	for ( int iOption = 0; iOption < m_pOptions->Count(); ++iOption )
	{
		OptionData_t *pOption = (*m_pOptions)[ iOption ];
		if ( Q_strcmp( pOption->szName, szName ) == 0 )
			return true;
	}

	for ( int iOption = 0; iOption < s_DisabledOptions.Count(); ++iOption )
	{
		OptionChoiceData_t *pOption = &(s_DisabledOptions[ iOption ]);
		if ( Q_strcmp( pOption->szName, szName ) == 0 )
			return true;
	}

	return false;
}

void COptionsDialogXbox::ReadOptionsFromFile( const char *pchFileName )
{
	KeyValues *pOptionKeys = new KeyValues( "options_x360" );
	pOptionKeys->LoadFromFile( g_pFullFileSystem, pchFileName, NULL );

	KeyValues *pKey = NULL;
	for ( pKey = pOptionKeys->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		// Skip disabled options
		if ( pKey->GetInt( "disable", 0 ) != 0 )
		{
			// Remember disabled options so we don't create another with the same name
			int iDisabledOption = s_DisabledOptions.AddToTail();
			OptionChoiceData_t *pDisabledOption = &(s_DisabledOptions[ iDisabledOption ]);
			Q_strncpy( pDisabledOption->szName, pKey->GetName(), sizeof( pDisabledOption->szName ) );

			continue;
		}

		if ( ShouldSkipOption( pKey ) )
			continue;

		int iOption = m_pOptions->AddToTail();
		OptionData_t **pNewOption = &((*m_pOptions)[ iOption ]);
		*pNewOption = new OptionData_t;

		// Get common values
		Q_strncpy( (*pNewOption)->szName, pKey->GetName(), sizeof( (*pNewOption)->szName ) );
		Q_strncpy( (*pNewOption)->szDisplayName, pKey->GetString( "name", "" ), sizeof( (*pNewOption)->szDisplayName ) );

		Q_strncpy( (*pNewOption)->szConvar, pKey->GetString( "convar", "" ), sizeof( (*pNewOption)->szConvar ) );
		if ( (*pNewOption)->szConvar[ 0 ] == '\0' )
			Q_strncpy( (*pNewOption)->szCommand, pKey->GetString( "command", "" ), sizeof( (*pNewOption)->szCommand ) );

		Q_strncpy( (*pNewOption)->szConvarDef, pKey->GetString( "convar_def", "" ), sizeof( (*pNewOption)->szConvarDef ) );

		(*pNewOption)->iPriority = pKey->GetInt( "priority", 0 );
		(*pNewOption)->bUnchangeable = ( pKey->GetInt( "unchangeable", 0 ) != 0 );
		(*pNewOption)->bVocalsLanguage = ( pKey->GetInt( "vocalslanguage", 0 ) != 0 );

		// Determine the type
		char szType[ OPTION_STRING_LENGTH ];
		Q_strncpy( szType, pKey->GetString( "type", "binary" ), sizeof( szType ) );

		if ( Q_stricmp( szType, "binary" ) == 0 )
			(*pNewOption)->eOptionType = OPTION_TYPE_BINARY;
		else if ( Q_stricmp( szType, "slider" ) == 0 )
			(*pNewOption)->eOptionType = OPTION_TYPE_SLIDER;
		else if ( Q_stricmp( szType, "choice" ) == 0 )
			(*pNewOption)->eOptionType = OPTION_TYPE_CHOICE;
		else if ( Q_stricmp( szType, "bind" ) == 0 )
			(*pNewOption)->eOptionType = OPTION_TYPE_BIND;

		// Get type specific values
		switch ( (*pNewOption)->eOptionType )
		{
			case OPTION_TYPE_SLIDER:
			{
				(*pNewOption)->fMinValue = pKey->GetFloat( "minvalue", 0.0f );
				(*pNewOption)->fMaxValue = pKey->GetFloat( "maxvalue", 1.0f );
				(*pNewOption)->fIncValue = pKey->GetFloat( "incvalue", 0.0f );

				char szSliderHomeType[ OPTION_STRING_LENGTH ];
				Q_strncpy( szSliderHomeType, pKey->GetString( "sliderhome", "none" ), sizeof( szSliderHomeType ) );

				if ( Q_stricmp( szSliderHomeType, "prev" ) == 0 )
					(*pNewOption)->eSliderHomeType = SLIDER_HOME_PREV;
				else if ( Q_stricmp( szSliderHomeType, "min" ) == 0 )
					(*pNewOption)->eSliderHomeType = SLIDER_HOME_MIN;
				else if ( Q_stricmp( szSliderHomeType, "center" ) == 0 )
					(*pNewOption)->eSliderHomeType = SLIDER_HOME_CENTER;
				else if ( Q_stricmp( szSliderHomeType, "max" ) == 0 )
					(*pNewOption)->eSliderHomeType = SLIDER_HOME_MAX;
				else
					(*pNewOption)->eSliderHomeType = SLIDER_HOME_NONE;

				switch ( (*pNewOption)->eSliderHomeType )
				{
					case SLIDER_HOME_MIN:
						(*pNewOption)->fSliderHomeValue = (*pNewOption)->fMinValue;
						break;

					case SLIDER_HOME_CENTER:
						(*pNewOption)->fSliderHomeValue = ( (*pNewOption)->fMaxValue + (*pNewOption)->fMinValue ) * 0.5f;
						break;

					case SLIDER_HOME_MAX:
						(*pNewOption)->fSliderHomeValue = (*pNewOption)->fMaxValue;
						break;

					default:
						(*pNewOption)->fSliderHomeValue = 0.0f;
				}

				break;
			}

			case OPTION_TYPE_CHOICE:
			{
				Q_strncpy( (*pNewOption)->szConvar2, pKey->GetString( "convar2", "" ), sizeof( (*pNewOption)->szConvar ) );

				if ( (*pNewOption)->bVocalsLanguage )
				{
					(*pNewOption)->iCurrentChoice = -1;
					int iChoice = (*pNewOption)->m_Choices.AddToTail();
					OptionChoiceData_t *pNewOptionChoice = &((*pNewOption)->m_Choices[ iChoice ]);

					Q_strncpy( pNewOptionChoice->szName, "#GameUI_Language_English", sizeof( pNewOptionChoice->szName ) );
					Q_strncpy( pNewOptionChoice->szValue, "1", sizeof( pNewOptionChoice->szValue ) );

					iChoice = (*pNewOption)->m_Choices.AddToTail();
					pNewOptionChoice = &((*pNewOption)->m_Choices[ iChoice ]);

					char szLanguage[ 256 ];
					Q_snprintf( szLanguage, sizeof( szLanguage ), "#GameUI_Language_%s", XBX_GetLanguageString() );
					Q_strncpy( pNewOptionChoice->szName, szLanguage, sizeof( pNewOptionChoice->szName ) );
					Q_strncpy( pNewOptionChoice->szValue, "0", sizeof( pNewOptionChoice->szValue ) );
				}
				else
				{
					(*pNewOption)->iCurrentChoice = -1;
					KeyValues *pChoicesKey = pKey->FindKey( "choices" );

					if ( pChoicesKey )
					{
						KeyValues *pSubKey = NULL;
						for ( pSubKey = pChoicesKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
						{
							int iChoice = (*pNewOption)->m_Choices.AddToTail();
							OptionChoiceData_t *pNewOptionChoice = &((*pNewOption)->m_Choices[ iChoice ]);

							Q_strncpy( pNewOptionChoice->szName, pSubKey->GetName(), sizeof( pNewOptionChoice->szName ) );
							Q_strncpy( pNewOptionChoice->szValue, pSubKey->GetString(), sizeof( pNewOptionChoice->szValue ) );
						}

						if ( (*pNewOption)->m_Choices.Count() < 2 )
						{
							DevWarning( "Option \"%s\" is type CHOICE but has less than 2 choices!", (*pNewOption)->szDisplayName );
						}
					}
				}
				break;
			}
		}
	}
}

int __cdecl SortByPriority( OptionData_t * const *pLeft, OptionData_t * const *pRight )
{
	return (*pLeft)->iPriority - (*pRight)->iPriority;
}

void COptionsDialogXbox::SortOptions( void )
{
	m_pOptions->Sort( SortByPriority );
}
