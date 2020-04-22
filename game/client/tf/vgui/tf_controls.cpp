//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"

#include "ienginevgui.h"
#include <vgui_controls/ScrollBarSlider.h>
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "tf_controls.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/PropertyPage.h"
#include "econ_item_system.h"
#include "iachievementmgr.h"
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <../common/GameUI/cvarslider.h>
#include "filesystem.h"

using namespace vgui;

wchar_t* LocalizeNumberWithToken( const char* pszLocToken, int nValue )
{
	static wchar_t wszOutString[ 128 ];
	wchar_t wszCount[ 16 ];
	_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", nValue );
	const wchar_t *wpszFormat = g_pVGuiLocalize->Find( pszLocToken );
	g_pVGuiLocalize->ConstructString_safe( wszOutString, wpszFormat, 1, wszCount );
	
	return wszOutString;
}

DECLARE_BUILD_FACTORY( CExCheckButton );
DECLARE_BUILD_FACTORY( CTFFooter );

//-----------------------------------------------------------------------------
// Purpose: Xbox-specific panel that displays button icons text labels
//-----------------------------------------------------------------------------
CTFFooter::CTFFooter( Panel *parent, const char *panelName ) : BaseClass( parent, panelName ) 
{
	SetVisible( true );
	SetAlpha( 0 );

	m_nButtonGap = 32;
	m_ButtonPinRight = 100;
	m_FooterTall = 80;

	m_ButtonOffsetFromTop = 0;
	m_ButtonSeparator = 4;
	m_TextAdjust = 0;

	m_bPaintBackground = false;
	m_bCenterHorizontal = true;

	m_szButtonFont[0] = '\0';
	m_szTextFont[0] = '\0';
	m_szFGColor[0] = '\0';
	m_szBGColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFooter::~CTFFooter()
{
	ClearButtons();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hButtonFont = pScheme->GetFont( ( m_szButtonFont[0] != '\0' ) ? m_szButtonFont : "GameUIButtons" );
	m_hTextFont = pScheme->GetFont( ( m_szTextFont[0] != '\0' ) ? m_szTextFont : "MenuLarge" );

	SetFgColor( pScheme->GetColor( m_szFGColor, Color( 255, 255, 255, 255 ) ) );
	SetBgColor( pScheme->GetColor( m_szBGColor, Color( 0, 0, 0, 255 ) ) );

	int x, y, w, h;
	GetParent()->GetBounds( x, y, w, h );
	SetBounds( x, h - m_FooterTall, w, m_FooterTall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// gap between hints
	m_nButtonGap = inResourceData->GetInt( "buttongap", 32 );
	m_ButtonPinRight = inResourceData->GetInt( "button_pin_right", 100 );
	m_FooterTall = inResourceData->GetInt( "tall", 80 );
	m_ButtonOffsetFromTop = inResourceData->GetInt( "buttonoffsety", 0 );
	m_ButtonSeparator = inResourceData->GetInt( "button_separator", 4 );
	m_TextAdjust = inResourceData->GetInt( "textadjust", 0 );

	m_bCenterHorizontal = ( inResourceData->GetInt( "center", 1 ) == 1 );
	m_bPaintBackground = ( inResourceData->GetInt( "paintbackground", 0 ) == 1 );

	// fonts for text and button
	Q_strncpy( m_szTextFont, inResourceData->GetString( "fonttext", "MenuLarge" ), sizeof( m_szTextFont ) );
	Q_strncpy( m_szButtonFont, inResourceData->GetString( "fontbutton", "GameUIButtons" ), sizeof( m_szButtonFont ) );

	// fg and bg colors
	Q_strncpy( m_szFGColor, inResourceData->GetString( "fgcolor", "White" ), sizeof( m_szFGColor ) );
	Q_strncpy( m_szBGColor, inResourceData->GetString( "bgcolor", "Black" ), sizeof( m_szBGColor ) );

	// clear the buttons because we're going to re-add them here
	ClearButtons();

	for ( KeyValues *pButton = inResourceData->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
	{
		const char *pNameButton = pButton->GetName();

		if ( !Q_stricmp( pNameButton, "button" ) )
		{
			// Add a button to the footer
			const char *pName = pButton->GetString( "name", "NULL" );
			const char *pText = pButton->GetString( "text", "NULL" );
			const char *pIcon = pButton->GetString( "icon", "NULL" );
			AddNewButtonLabel( pName, pText, pIcon );
		}
	}

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::AddNewButtonLabel( const char *name, const char *text, const char *icon )
{
	FooterButton_t *button = new FooterButton_t;

	button->bVisible = true;
	Q_strncpy( button->name, name, sizeof( button->name ) );

	// Button icons are a single character
	wchar_t *pIcon = g_pVGuiLocalize->Find( icon );
	if ( pIcon )
	{
		button->icon[0] = pIcon[0];
		button->icon[1] = '\0';
	}
	else
	{
		button->icon[0] = '\0';
	}

	// Set the help text
	wchar_t *pText = g_pVGuiLocalize->Find( text );
	if ( pText )
	{
		wcsncpy( button->text, pText, wcslen( pText ) + 1 );
	}
	else
	{
		button->text[0] = '\0';
	}

	m_Buttons.AddToTail( button );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ShowButtonLabel( const char *name, bool show )
{
	for ( int i = 0; i < m_Buttons.Count(); ++i )
	{
		if ( !Q_stricmp( m_Buttons[ i ]->name, name ) )
		{
			m_Buttons[ i ]->bVisible = show;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::PaintBackground( void )
{
	if ( !m_bPaintBackground )
		return;

	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::Paint( void )
{
	// inset from right edge
	int wide = GetWide();

	// center the text within the button
	int buttonHeight = vgui::surface()->GetFontTall( m_hButtonFont );
	int fontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	int textY = ( buttonHeight - fontHeight )/2 + m_TextAdjust;

	if ( textY < 0 )
	{
		textY = 0;
	}

	int y = m_ButtonOffsetFromTop;

	if ( !m_bCenterHorizontal )
	{
		// draw the buttons, right to left
		int x = wide - m_ButtonPinRight;

		vgui::Label label( this, "temp", L"" );
		for ( int i = m_Buttons.Count() - 1 ; i >= 0 ; --i )
		{
			FooterButton_t *pButton = m_Buttons[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			int iTextWidth = label.GetWide();

			if ( iTextWidth == 0 )
				x += m_nButtonGap;	// There's no text, so remove the gap between buttons
			else
				x -= iTextWidth;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			// Draw the button
			// back up button width and a little extra to leave a gap between button and text
			x -= ( vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator );
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );

			// back up to next string
			x -= m_nButtonGap;
		}
	}
	else
	{
		// center the buttons (as a group)
		int x = wide / 2;
		int totalWidth = 0;
		int i = 0;
		int nButtonCount = 0;

		vgui::Label label( this, "temp", L"" );

		// need to loop through and figure out how wide our buttons and text are (with gaps between) so we can offset from the center
		for ( i = 0; i < m_Buttons.Count(); ++i )
		{
			FooterButton_t *pButton = m_Buttons[i];

			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			totalWidth += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] );
			totalWidth += m_ButtonSeparator;
			totalWidth += label.GetWide();

			nButtonCount++; // keep track of how many active buttons we'll be drawing
		}

		totalWidth += ( nButtonCount - 1 ) * m_nButtonGap; // add in the gaps between the buttons
		x -= ( totalWidth / 2 );

		for ( i = 0; i < m_Buttons.Count(); ++i )
		{
			FooterButton_t *pButton = m_Buttons[i];

			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			int iTextWidth = label.GetWide();

			// Draw the icon
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );
			x += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			x += iTextWidth + m_nButtonGap;
		}
	}
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ClearButtons( void )
{
	m_Buttons.PurgeAndDeleteElements();
}

#define OPTIONS_DIR "cfg"
#define DEFAULT_OPTIONS_FILE OPTIONS_DIR "/user_default.scr"
#define OPTIONS_FILE OPTIONS_DIR "/user.scr"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFAdvancedOptionsDialog::CTFAdvancedOptionsDialog(vgui::Panel *parent) : BaseClass(NULL, "TFAdvancedOptionsDialog")
{
	// Need to use the clientscheme (we're not parented to a clientscheme'd panel)
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	SetProportional( true );

	m_pListPanel = new vgui::PanelListPanel( this, "PanelListPanel" );

	m_pList = NULL;

	m_pToolTip = new CTFTextToolTip( this );
	m_pToolTipEmbeddedPanel = new vgui::EditablePanel( this, "TooltipPanel" );
	m_pToolTipEmbeddedPanel->SetKeyBoardInputEnabled( false );
	m_pToolTipEmbeddedPanel->SetMouseInputEnabled( false );
	m_pToolTip->SetEmbeddedPanel( m_pToolTipEmbeddedPanel );
	m_pToolTip->SetTooltipDelay( 0 );

	m_pDescription = new CInfoDescription();
	m_pDescription->InitFromFile( DEFAULT_OPTIONS_FILE );
	m_pDescription->InitFromFile( OPTIONS_FILE, false );
	m_pDescription->TransferCurrentValues( NULL );

// 	MoveToCenterOfScreen();
// 	SetSizeable( false );
// 	SetDeleteSelfOnClose( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFAdvancedOptionsDialog::~CTFAdvancedOptionsDialog()
{
	delete m_pDescription;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings("resource/ui/TFAdvancedOptionsDialog.res");
	m_pListPanel->SetFirstColumnWidth( 0 );

	CreateControls();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::OnClose()
{
	BaseClass::OnClose();

	TFModalStack()->PopModal( this );
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		// OnApplyChanges();
		SaveValues();
		OnClose();
		return;
	}
	else if ( !stricmp( command, "Close" ) )
	{
		OnClose();
		return;
	}

	BaseClass::OnCommand( command );
}

void CTFAdvancedOptionsDialog::OnKeyCodeTyped(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if ( code == KEY_ESCAPE )
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::OnKeyCodePressed(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if ( GetBaseButtonCode( code ) == KEY_XBUTTON_B || GetBaseButtonCode( code ) == STEAMCONTROLLER_B || GetBaseButtonCode( code ) == STEAMCONTROLLER_START )
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::GatherCurrentValues()
{
	if ( !m_pDescription )
		return;

	// OK
	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;
	CCvarSlider *pSlider;

	mpcontrol_t *pList;

	CScriptObject *pObj;
	CScriptListItem *pItem;

	char szValue[256];
	char strValue[ 256 ];

	pList = m_pList;
	while ( pList )
	{
		pObj = pList->pScrObj;

		if ( pObj->type == O_CATEGORY )
		{
			pList = pList->next;
			continue;
		}

		if ( !pList->pControl )
		{
			pObj->SetCurValue( pObj->defValue );
			pList = pList->next;
			continue;
		}

		switch ( pObj->type )
		{
		case O_BOOL:
			pBox = (CheckButton *)pList->pControl;
			sprintf( szValue, "%s", pBox->IsSelected() ? "1" : "0" );
			break;
		case O_NUMBER:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_STRING:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_LIST:
			{
				pCombo = (ComboBox *)pList->pControl;
				// pCombo->GetText( strValue, sizeof( strValue ) );
				int activeItem = pCombo->GetActiveItem();

				pItem = pObj->pListItems;
				//			int n = (int)pObj->fdefValue;

				while ( pItem )
				{
					if (!activeItem--)
						break;

					pItem = pItem->pNext;
				}

				if ( pItem )
				{
					sprintf( szValue, "%s", pItem->szValue );
				}
				else  // Couln't find index
				{
					//assert(!("Couldn't find string in list, using default value"));
					sprintf( szValue, "%s", pObj->defValue );
				}
				break;
			}
		case O_SLIDER:
			pSlider = ( CCvarSlider * )pList->pControl;
			sprintf( szValue, "%.2f", pSlider->GetSliderValue() );
			break;
		}

		// Remove double quotes and % characters
		UTIL_StripInvalidCharacters( szValue, sizeof(szValue) );

		V_strcpy_safe( strValue, szValue );

		pObj->SetCurValue( strValue );

		pList = pList->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::CreateControls()
{
	DestroyControls();

	// Go through desciption creating controls
	CScriptObject *pObj;

	pObj = m_pDescription->pObjList;

	// Build out the clan dropdown
	CScriptObject *pClanObj = m_pDescription->FindObject( "cl_clanid" );
	ISteamFriends *pFriends = steamapicontext->SteamFriends();
	if ( pFriends && pClanObj )
	{
		pClanObj->RemoveAndDeleteAllItems();
		int iGroupCount = pFriends->GetClanCount();
		pClanObj->AddItem( new CScriptListItem( "#Cstrike_ClanTag_None", "0" ) );
		for ( int k = 0; k < iGroupCount; ++ k )
		{
			CSteamID clanID = pFriends->GetClanByIndex( k );
			const char *pName = pFriends->GetClanName( clanID );
			const char *pTag = pFriends->GetClanTag( clanID );

			char id[12];
			Q_snprintf( id, sizeof( id ), "%d", clanID.GetAccountID() );
			pClanObj->AddItem( new CScriptListItem( CFmtStr( "%s (%s)", pTag, pName ), id ) );
		}
	}

	mpcontrol_t	*pCtrl;

	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;
	CCvarSlider *pSlider;
	CScriptListItem *pListItem;

	Panel *objParent = m_pListPanel;

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	vgui::HFont hTextFont = pScheme->GetFont( "HudFontSmallestBold", true );
	Color tanDark = pScheme->GetColor( "TanDark", Color(255,0,0,255) );

	while ( pObj )
	{
		if ( pObj->type == O_OBSOLETE )
		{
			pObj = pObj->pNext;
			continue;
		}

		pCtrl = new mpcontrol_t( objParent, "mpcontrol_t" );
		pCtrl->type = pObj->type;

		// Force it to invalidate scheme now, so we can change color afterwards and have it persist
		pCtrl->InvalidateLayout( true, true );

		switch ( pCtrl->type )
		{
		case O_BOOL:
			pBox = new CheckButton( pCtrl, "DescCheckButton", pObj->prompt );
			pBox->SetSelected( pObj->fdefValue != 0.0f ? true : false );

			pCtrl->pControl = (Panel *)pBox;
			pBox->SetFont( hTextFont );

			pBox->InvalidateLayout( true, true );

			// This is utterly fucking retarded.
			pBox->SetFgColor( tanDark );
			pBox->SetDefaultColor( tanDark, pBox->GetBgColor() );
			pBox->SetArmedColor( tanDark, pBox->GetBgColor() );
			pBox->SetDepressedColor( tanDark, pBox->GetBgColor() );
			pBox->SetSelectedColor( tanDark, pBox->GetBgColor() );
			pBox->SetHighlightColor( tanDark );
			pBox->GetCheckImage()->SetColor( tanDark );
			break;
		case O_STRING:
		case O_NUMBER:
			pEdit = new TextEntry( pCtrl, "DescTextEntry");
			pEdit->InsertString(pObj->defValue);
			pCtrl->pControl = (Panel *)pEdit;
			pEdit->SetFont( hTextFont );

			pEdit->InvalidateLayout( true, true );
			pEdit->SetBgColor( Color(0,0,0,255) );
			break;
		case O_LIST:
			{
				pCombo = new ComboBox( pCtrl, "DescComboBox", 5, false );

				// track which row matches the current value
				int iRow = -1;
				int iCount = 0;
				pListItem = pObj->pListItems;
				while ( pListItem )
				{
					if ( iRow == -1 && !Q_stricmp( pListItem->szValue, pObj->curValue ) )
						iRow = iCount;

					pCombo->AddItem( pListItem->szItemText, NULL );
					pListItem = pListItem->pNext;
					++iCount;
				}


				pCombo->ActivateItemByRow( iRow );

				pCtrl->pControl = (Panel *)pCombo;
				pCombo->SetFont( hTextFont );
			}
			break;
		case O_SLIDER:
			pSlider = new CCvarSlider( pCtrl, "DescSlider", "Test", pObj->fMin, pObj->fMax, pObj->cvarname, false );
			pCtrl->pControl = (Panel *)pSlider;
			break;
		case O_CATEGORY:
			pCtrl->SetBorder( pScheme->GetBorder("OptionsCategoryBorder") );
			break;
		default:
			break;
		}

		if ( pCtrl->type != O_BOOL )
		{
			pCtrl->pPrompt = new vgui::Label( pCtrl, "DescLabel", "" );
			pCtrl->pPrompt->SetContentAlignment( vgui::Label::a_west );
			pCtrl->pPrompt->SetTextInset( 5, 0 );
			pCtrl->pPrompt->SetText( pObj->prompt );
			pCtrl->pPrompt->SetFont( hTextFont );

			pCtrl->pPrompt->InvalidateLayout( true, true );

			if ( pCtrl->type == O_CATEGORY )
			{
				pCtrl->pPrompt->SetFont( pScheme->GetFont( "HudFontSmallBold", true ) );
				pCtrl->pPrompt->SetFgColor( pScheme->GetColor( "TanLight", Color(255,0,0,255) ) );
			}
			else
			{
				pCtrl->pPrompt->SetFgColor( tanDark );
			}
		}

		pCtrl->pScrObj = pObj;

		switch ( pCtrl->type )
		{
		case O_BOOL:
		case O_STRING:
		case O_NUMBER:
		case O_LIST:
		case O_CATEGORY:
			pCtrl->SetSize( m_iControlW, m_iControlH );
			break;
		case O_SLIDER:
			pCtrl->SetSize( m_iSliderW, m_iSliderH );
			break;
		default:
			break;
		}

		// Hook up the tooltip, if the entry has one
		if ( pObj->tooltip && pObj->tooltip[0] )
		{
			if ( pCtrl->pPrompt )
			{
				pCtrl->pPrompt->SetTooltip( m_pToolTip, pObj->tooltip );
			}
			else
			{
				pCtrl->SetTooltip( m_pToolTip, pObj->tooltip );
				pCtrl->pControl->SetTooltip( m_pToolTip, pObj->tooltip );
			}
		}

		m_pListPanel->AddItem( NULL, pCtrl );

		// Link it in
		if ( !m_pList )
		{
			m_pList = pCtrl;
			pCtrl->next = NULL;
		}
		else
		{
			mpcontrol_t *p;
			p = m_pList;
			while ( p )
			{
				if ( !p->next )
				{
					p->next = pCtrl;
					pCtrl->next = NULL;
					break;
				}
				p = p->next;
			}
		}

		pObj = pObj->pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::DestroyControls()
{
	mpcontrol_t *p, *n;

	p = m_pList;
	while ( p )
	{
		n = p->next;
		//
		if ( p->pControl )
		{
			p->pControl->MarkForDeletion();
			p->pControl = NULL;
		}
		if ( p->pPrompt )
		{
			p->pPrompt->MarkForDeletion();
			p->pPrompt = NULL;
		}
		delete p;
		p = n;
	}

	m_pList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::SaveValues() 
{
	// Get the values from the controls:
	GatherCurrentValues();

	// Create the game.cfg file
	if ( m_pDescription )
	{
		FileHandle_t fp;

		// Add settings to config.cfg
		m_pDescription->WriteToConfig();

		g_pFullFileSystem->CreateDirHierarchy( OPTIONS_DIR );
		fp = g_pFullFileSystem->Open( OPTIONS_FILE, "wb" );
		if ( fp )
		{
			m_pDescription->WriteToScriptFile( fp );
			g_pFullFileSystem->Close( fp );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAdvancedOptionsDialog::Deploy( void )
{
	SetVisible( true );
	MakePopup();
	MoveToFront();
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	TFModalStack()->PushModal( this );

	// Center it, keeping requested size
	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	GetSize(wide, tall);
	SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextToolTip::PerformLayout()
{
	if ( !ShouldLayout() )
		return;

	_isDirty = false;

	// Resize our text labels to fit.
	int iW = m_pEmbeddedPanel->GetWide();
	int iH = 0;
	for (int i = 0; i < m_pEmbeddedPanel->GetChildCount(); i++)
	{
		vgui::Label *pLabel = dynamic_cast<vgui::Label*>( m_pEmbeddedPanel->GetChild(i) );
		if ( !pLabel )
			continue;

		// Only checking to see if we have any text
		char szTmp[2];
		pLabel->GetText( szTmp, sizeof(szTmp) );
		if ( !szTmp[0] )
			continue;

		int iLX, iLY;
		pLabel->GetPos( iLX, iLY );

		int iMaxWidth = m_pEmbeddedPanel->GetWide() - (iLX * 2);
		pLabel->GetTextImage()->ResizeImageToContentMaxWidth( iMaxWidth );
		pLabel->SizeToContents();
		pLabel->SetWide( iMaxWidth );
		pLabel->InvalidateLayout(true);

		int iX, iY;
		pLabel->GetPos( iX, iY );
		iW = MAX( iW, ( pLabel->GetWide() + (iX * 2) ) );

		if ( iH == 0 )
		{
			iH += MAX( iH, pLabel->GetTall() + (iY * 2) );
		}
		else
		{
			iH += MAX( iH, pLabel->GetTall() );
		}
	}
	m_pEmbeddedPanel->SetSize( m_pEmbeddedPanel->GetWide(), iH );

	m_pEmbeddedPanel->SetVisible(true);

	PositionWindow( m_pEmbeddedPanel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextToolTip::PositionWindow( Panel *pTipPanel )
{
	int iTipW, iTipH;
	pTipPanel->GetSize( iTipW, iTipH );

	int cursorX, cursorY;
	input()->GetCursorPos(cursorX, cursorY);

	int px, py, wide, tall;
	ipanel()->GetAbsPos( m_pEmbeddedPanel->GetParent()->GetVPanel(), px, py );
	m_pEmbeddedPanel->GetParent()->GetSize(wide, tall);

	if ( !m_pEmbeddedPanel->IsPopup() )
	{
		// Move the cursor into our parent space
		cursorX -= px;
		cursorY -= py;
	}

	if (wide - iTipW > cursorX)
	{
		cursorY += 20;
		// menu hanging right
		if (tall - iTipH > cursorY)
		{
			// menu hanging down
			pTipPanel->SetPos(cursorX, cursorY);
		}
		else
		{
			// menu hanging up
			pTipPanel->SetPos(cursorX, cursorY - iTipH - 20);
		}
	}
	else
	{
		// menu hanging left
		if (tall - iTipH > cursorY)
		{
			// menu hanging down
			pTipPanel->SetPos( Max( 0, cursorX - iTipW ), cursorY);
		}
		else
		{
			// menu hanging up
			pTipPanel->SetPos( Max( 0, cursorX - iTipW ), cursorY - iTipH - 20 );
		}
	}	
}

static vgui::DHANDLE<CTFAdvancedOptionsDialog> g_pTFAdvancedOptionsDialog;

//-----------------------------------------------------------------------------
// Purpose: Callback to open the game menus
//-----------------------------------------------------------------------------
void CL_OpenTFAdvancedOptionsDialog( const CCommand &args )
{
	if ( g_pTFAdvancedOptionsDialog.Get() == NULL )
	{
		g_pTFAdvancedOptionsDialog = vgui::SETUP_PANEL( new CTFAdvancedOptionsDialog( NULL ) );
	}

	g_pTFAdvancedOptionsDialog->Deploy();
}

// the console commands
static ConCommand opentf2options( "opentf2options", &CL_OpenTFAdvancedOptionsDialog, "Displays the TF2 Advanced Options dialog." );

//-----------------------------------------------------------------------------
// Purpose: A scroll bar that can have specified width
//-----------------------------------------------------------------------------
class CExScrollBar : public ScrollBar
{
	DECLARE_CLASS_SIMPLE( CExScrollBar, ScrollBar );
public:

	CExScrollBar( Panel *parent, const char *name, bool bVertical )
		: ScrollBar( parent, name, bVertical )
	{}
	
	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE
	{
		// Deliberately skip ScrollBar
		Panel::ApplySchemeSettings( pScheme );
	}
};

DECLARE_BUILD_FACTORY( CExScrollingEditablePanel );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CExScrollingEditablePanel::CExScrollingEditablePanel( Panel *pParent, const char *pszName )
	: EditablePanel( pParent, pszName )
	, m_nLastScrollValue( 0 )
	, m_bUseMouseWheelToScroll( true )
{
	m_pScrollBar = new CExScrollBar( this, "ScrollBar", true );
	m_pScrollBar->AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CExScrollingEditablePanel::~CExScrollingEditablePanel()
{}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CExScrollingEditablePanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	KeyValues *pScrollbarKV = inResourceData->FindKey( "Scrollbar" );
	if ( pScrollbarKV )
	{
		m_pScrollBar->ApplySettings( pScrollbarKV );
	}

	m_bUseMouseWheelToScroll = inResourceData->GetBool( "allow_mouse_wheel_to_scroll", true );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CExScrollingEditablePanel::OnSizeChanged( int newWide, int newTall )
{
	BaseClass::OnSizeChanged( newWide, newTall );

	int nDelta = m_nLastScrollValue;
	// Go through all our children and move them BACK into position
	int nNumChildren = GetChildCount();
	for ( int i=0; i < nNumChildren; ++i )
	{
		Panel* pChild = GetChild( i );

		if ( pChild == m_pScrollBar )
			continue;

		EditablePanel* pEditableChild = dynamic_cast< EditablePanel* >( pChild );
		if ( pEditableChild && pEditableChild->ShouldSkipAutoResize() )
			continue;

		int x,y;
		pChild->GetPos( x, y );

		pChild->SetPos( x, y - nDelta );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CExScrollingEditablePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int nFurthestY = 0;

	// Go through all our children and find the lowest point on the lowest child
	// that we'd need to scroll to
	int nNumChildren = GetChildCount();
	for ( int i=0; i < nNumChildren; ++i )
	{
		Panel* pChild = GetChild( i );
	
		if ( pChild == m_pScrollBar )
			continue;

		int x,y,wide,tall;
		pChild->GetBounds( x, y, wide, tall );

		// Offset by our scroll value
		y += m_nLastScrollValue;

		if ( m_bRestrictWidth )
		{
			int nMaxWide = Min( x + wide, GetWide() - m_pScrollBar->GetWide() );
			pChild->SetWide( nMaxWide - x );
		}

		nFurthestY = Max( y + tall, nFurthestY );
	}

	int nMaxRange = nFurthestY + m_iBottomBuffer;
	m_pScrollBar->SetRange( 0, nMaxRange );
	m_pScrollBar->SetRangeWindow( GetTall() );

	OnScrollBarSliderMoved();
}

//-----------------------------------------------------------------------------
// Called when the scroll bar moves
//-----------------------------------------------------------------------------
void CExScrollingEditablePanel::OnScrollBarSliderMoved()
{
	// Figure out how far they just scrolled
	int nScrollAmount = m_pScrollBar->GetValue();
	int nDelta = nScrollAmount - m_nLastScrollValue;

	if ( nDelta == 0 )
		return;

	ShiftChildren( nDelta );

	m_nLastScrollValue = nScrollAmount;
}

void CExScrollingEditablePanel::ShiftChildren( int nDistance )
{
	// Go through all our children and move them 
	int nNumChildren = GetChildCount();
	for ( int i=0; i < nNumChildren; ++i )
	{
		Panel* pChild = GetChild( i );

		if ( pChild == m_pScrollBar )
			continue;

		int x,y;
		pChild->GetPos( x, y );

		pChild->SetPos( x, y - nDistance );
	}
}

//-----------------------------------------------------------------------------
// respond to mouse wheel events
//-----------------------------------------------------------------------------
void CExScrollingEditablePanel::OnMouseWheeled( int delta )
{
	if ( !m_bUseMouseWheelToScroll )
	{
		BaseClass::OnMouseWheeled( delta );
		return;
	}

	int val = m_pScrollBar->GetValue();
	val -= ( delta * m_iScrollStep );
	m_pScrollBar->SetValue( val );
}

DECLARE_BUILD_FACTORY( CScrollableList );
//-----------------------------------------------------------------------------
// Clearnup
//-----------------------------------------------------------------------------
CScrollableList::~CScrollableList()
{
	ClearAutoLayoutPanels();
}

void CScrollableList::PerformLayout()
{
	int nYpos = -m_nLastScrollValue;

	for( int i=0; i<m_vecAutoLayoutPanels.Count(); ++i )
	{
		LayoutInfo_t layout = m_vecAutoLayoutPanels[ i ];
		nYpos += layout.m_nGap;

		layout.m_pPanel->SetPos( layout.m_pPanel->GetXPos(), nYpos );

		nYpos += layout.m_pPanel->GetTall();
	}

	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Add a panel to the bottom
//-----------------------------------------------------------------------------
void CScrollableList::AddPanel( Panel* pPanel, int nGap )
{
	// We're the captain now
	pPanel->SetParent( this );
	pPanel->SetAutoDelete( false );

	auto idx = m_vecAutoLayoutPanels.AddToTail();
	LayoutInfo_t& layout = m_vecAutoLayoutPanels[ idx ];
	layout.m_pPanel = pPanel;
	layout.m_nGap = nGap;

	// Need to do a perform layout so we get sized correctly
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Delete any panels we own
//-----------------------------------------------------------------------------
void CScrollableList::ClearAutoLayoutPanels()
{
	FOR_EACH_VEC( m_vecAutoLayoutPanels, i )
	{
		m_vecAutoLayoutPanels[ i ].m_pPanel->MarkForDeletion();
	}

	m_vecAutoLayoutPanels.Purge();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CExpandablePanel::CExpandablePanel( Panel* pParent, const char* pszName ) 
	: vgui::EditablePanel( pParent, pszName )
	, m_bExpanded( false )
	, m_flAnimEndTime( 0.f )
	, m_flResizeTime( 0.f )
{}


//-----------------------------------------------------------------------------
// Set spcific collapsed state
//-----------------------------------------------------------------------------
void CExpandablePanel::SetCollapsed( bool bCollapsed )
{
	if ( bCollapsed == m_bExpanded )
	{
		ToggleCollapse();
	}
}


//-----------------------------------------------------------------------------
// Toggle collapsed state
//-----------------------------------------------------------------------------
void CExpandablePanel::ToggleCollapse()
{
	m_bExpanded = !m_bExpanded;
	// Allow for quick bounce-back if they click while we're already animating
	float flEndTime = RemapValClamped( GetPercentAnimated(), 0.f, 1.f, 0.f, m_flResizeTime );
	m_flAnimEndTime = Plat_FloatTime() + flEndTime;

	OnToggleCollapse( m_bExpanded );
}

//-----------------------------------------------------------------------------
// Toggle collapsed state
//-----------------------------------------------------------------------------
void CExpandablePanel::OnCommand( const char *command )
{
	if ( FStrEq( "toggle_collapse", command ) )
	{
		ToggleCollapse();

		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Do collapsing interpolation
//-----------------------------------------------------------------------------
void CExpandablePanel::OnThink()
{
	BaseClass::OnThink();

	float flTimeProgress = Gain( GetPercentAnimated(), 0.8f );
		
	const int& nStartHeight = m_bExpanded ? m_nCollapsedHeight : m_nExpandedHeight;
	const int& nEndHeight = m_bExpanded ? m_nExpandedHeight : m_nCollapsedHeight;
	int nCurrentHeight = RemapValClamped( flTimeProgress, 0.f, 1.f, nStartHeight, nEndHeight );

	if ( nCurrentHeight != GetTall() )
	{
		SetTall( nCurrentHeight );
		Panel* pParent = GetParent();
		if ( pParent )
		{
			pParent->InvalidateLayout();
		}
	}
}

//-----------------------------------------------------------------------------
// Where we're at in our interpolation
//-----------------------------------------------------------------------------
float CExpandablePanel::GetPercentAnimated() const
{
	return RemapValClamped( Plat_FloatTime() - ( m_flAnimEndTime - m_flResizeTime ), 0.f, m_flResizeTime, 0.f, 1.f );
}

