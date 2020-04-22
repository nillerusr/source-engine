//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <time.h>

#include "MultiplayerAdvancedDialog.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ListPanel.h>
#include <KeyValues.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include "cvarslider.h"
#include "PanelListPanel.h"
#include <vgui/IInput.h>
#include <steam/steam_api.h>
#include "EngineInterface.h"
#include "fmtstr.h"

#include "filesystem.h"

#include <tier0/vcrmode.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define OPTIONS_DIR "cfg"
#define DEFAULT_OPTIONS_FILE OPTIONS_DIR "/user_default.scr"
#define OPTIONS_FILE OPTIONS_DIR "/user.scr"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedDialog::CMultiplayerAdvancedDialog(vgui::Panel *parent) : BaseClass(NULL, "MultiplayerAdvancedDialog")
{
	SetBounds(0, 0, 372, 160);
	SetSizeable( false );

	SetTitle("#GameUI_MultiplayerAdvanced", true);

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	Button *ok = new Button( this, "OK", "#GameUI_OK" );
	ok->SetCommand( "Ok" );

	m_pListPanel = new CPanelListPanel( this, "PanelListPanel" );

	m_pList = NULL;

	m_pDescription = new CInfoDescription();
	m_pDescription->InitFromFile( DEFAULT_OPTIONS_FILE );
	m_pDescription->InitFromFile( OPTIONS_FILE );
	m_pDescription->TransferCurrentValues( NULL );

	LoadControlSettings("Resource\\MultiplayerAdvancedDialog.res");
	CreateControls();

	MoveToCenterOfScreen();
	SetSizeable( false );
	SetDeleteSelfOnClose( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedDialog::~CMultiplayerAdvancedDialog()
{
	delete m_pDescription;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::Activate()
{
	BaseClass::Activate();
	input()->SetAppModalSurface(GetVPanel());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		// OnApplyChanges();
		SaveValues();
		OnClose();
		return;
	}

	BaseClass::OnCommand( command );
}

void CMultiplayerAdvancedDialog::OnKeyCodeTyped(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (code == KEY_ESCAPE)
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::GatherCurrentValues()
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

		strcpy( strValue, szValue );

		pObj->SetCurValue( strValue );

		pList = pList->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::CreateControls()
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

	while ( pObj )
	{
		if ( pObj->type == O_OBSOLETE || pObj->type == O_CATEGORY )
		{
			pObj = pObj->pNext;
			continue;
		}

		pCtrl = new mpcontrol_t( objParent, "mpcontrol_t" );
		pCtrl->type = pObj->type;

		switch ( pCtrl->type )
		{
		case O_BOOL:
			pBox = new CheckButton( pCtrl, "DescCheckButton", pObj->prompt );
			pBox->SetSelected( pObj->fdefValue != 0.0f ? true : false );
			
			pCtrl->pControl = (Panel *)pBox;
			break;
		case O_STRING:
		case O_NUMBER:
			pEdit = new TextEntry( pCtrl, "DescTextEntry");
			pEdit->InsertString(pObj->defValue);
			pCtrl->pControl = (Panel *)pEdit;
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
			}
			break;
		case O_SLIDER:
			pSlider = new CCvarSlider( pCtrl, "DescSlider", "Test", pObj->fMin, pObj->fMax, pObj->cvarname, false );
			pCtrl->pControl = (Panel *)pSlider;
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
		}

		pCtrl->pScrObj = pObj;

		switch ( pCtrl->type )
		{
		case O_BOOL:
		case O_STRING:
		case O_NUMBER:
		case O_LIST:
			pCtrl->SetSize( 100, 28 );
			break;
		case O_SLIDER:
			pCtrl->SetSize( 100, 40 );
			break;
		default:
			break;
		}
		//pCtrl->SetBorder( scheme()->GetBorder(1, "DepressedButtonBorder") );
		m_pListPanel->AddItem( pCtrl );

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
void CMultiplayerAdvancedDialog::DestroyControls()
{
	mpcontrol_t *p, *n;

	p = m_pList;
	while ( p )
	{
		n = p->next;
		//
		delete p->pControl;
		delete p->pPrompt;
		delete p;
		p = n;
	}

	m_pList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::SaveValues() 
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

