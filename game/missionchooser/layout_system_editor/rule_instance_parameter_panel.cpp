//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit leafy data (e.g. ints, strings).
//
//===============================================================================

#include "vgui_controls/Label.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/ComboBox.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "ThemesDialog.h"
#include "LevelTheme.h"
#include "mission_editor/theme_room_picker.h"
#include "layout_system/tilegen_mission_preprocessor.h"
#include "layout_system/tilegen_rule.h"
#include "layout_system/tilegen_enum.h"
#include "layout_system_editor/layout_system_kv_editor.h"
#include "layout_system_editor/rule_instance_node_panel.h"
#include "layout_system_editor/rule_instance_parameter_panel.h"

using namespace vgui;

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

class CEditableValuePanel;

//-----------------------------------------------------------------------------
// Creates & initializes the appropriate C???ValuePanel type given a type name.
// On exit, *pIsChildPanel is true if the panel is a complex type which
// should be added as a child or false if it should be added inline.
//-----------------------------------------------------------------------------
CNodePanel *CreateValuePanel( const CTilegenRule *pRule, int nRuleParameterIndex, CNodePanel *pParent, KeyValues *pData, bool *pIsChildPanel );

//-----------------------------------------------------------------------------
// Creates a new suitable default value for this rule parameter.
//-----------------------------------------------------------------------------
KeyValues *CreateNewValue( const CTilegenRule *pRule, int nRuleParameterIndex );

//////////////////////////////////////////////////////////////////////////
// Local Type Definitions
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Base class for panels which have editable controls to manipulate
// leafy mission data.
//-----------------------------------------------------------------------------
class CEditableValuePanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CEditableValuePanel, CNodePanel )

public:
	CEditableValuePanel( Panel *pParent, const char *pName );

	virtual void CreatePanelContents() { }
};

class CArrayValuePanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CArrayValuePanel, CNodePanel )

public:
	CArrayValuePanel( Panel *pParent, const char *pName, const CTilegenRule *pRule, int nRuleParameterIndex );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void CreatePanelContents();
	void AddNewParameterElementPanel( int nIndex );
	
	MESSAGE_FUNC_PARAMS( OnAddRule, "AddRule", pKV );
	
private:
	const CTilegenRule *m_pRule;
	int m_nRuleParameterIndex;
};

class CBoolValuePanel : public CEditableValuePanel
{
	DECLARE_CLASS_SIMPLE( CBoolValuePanel, CEditableValuePanel )

public:
	CBoolValuePanel( Panel *pParent, const char *pName );

	virtual void ApplySchemeSettings( IScheme *pScheme );

	MESSAGE_FUNC_PTR( OnButtonChecked, "CheckButtonChecked", panel );
	virtual void UpdateState();

private:
	CheckButton *m_pCheckButton;
};

class CStringValuePanel : public CEditableValuePanel
{
	DECLARE_CLASS_SIMPLE( CStringValuePanel, CEditableValuePanel )

public:
	CStringValuePanel( Panel *pParent, const char *pName );

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	virtual void UpdateState();
	void AllowNumericInputOnly() { m_pTextEntry->SetAllowNumericInputOnly( true ); }

private:
	TextEntry *m_pTextEntry;
};

class CEnumValuePanel : public CEditableValuePanel
{
	DECLARE_CLASS_SIMPLE( CEnumValuePanel, CEditableValuePanel )

public:
	CEnumValuePanel( Panel *pParent, const char *pName, const CTilegenEnum *pTilegenEnum );

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	virtual void UpdateState();

private:
	ComboBox *m_pComboBox;
	const CTilegenEnum *m_pTilegenEnum;
};

class CThemeValuePanel : public CEditableValuePanel
{
	DECLARE_CLASS_SIMPLE( CThemeValuePanel, CEditableValuePanel )

public:
	CThemeValuePanel( Panel *pParent, const char *pName );

	virtual void OnCommand( const char *pCommand );
	virtual void UpdateState();

private:
	Button *m_pChangeThemeButton;
	CThemeDetails *m_pThemeDetails;
};

class CFileValuePanel : public CEditableValuePanel
{
	DECLARE_CLASS_SIMPLE( CFileValuePanel, CEditableValuePanel )

public:
	CFileValuePanel( Panel *pParent, const char *pName, const char *pFileTypeName, const char *pFileExtension, const char *pBaseDirectory, bool bStripPath, bool bStripExtension );

	virtual void OnCommand( const char *pCommand );

	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", pKeyValues );
	virtual void UpdateState();

private:
	char m_FileTypeName[32];
	char m_FileExtension[32];
	char m_BaseDirectory[MAX_PATH];
	bool m_bStripPath;
	bool m_bStripExtension;
	TextEntry *m_pTextEntry;
	Button *m_pChangeFileButton;
};

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CRuleInstanceParameterPanel::CRuleInstanceParameterPanel( Panel *pParent, const char *pName, const CTilegenRule *pRule, int nRuleParameterIndex ) : 
BaseClass( pParent, pName ),
m_pAddButton( NULL ),
m_pDeleteButton( NULL ),
m_pRule( pRule ),
m_nRuleParameterIndex( nRuleParameterIndex ) 
{	
	// We want a transparent background for the panel (e.g. just render the controls without an ugly border)
	SetPaintBackgroundEnabled( false );
	GetTooltip()->SetText( m_pRule->GetParameterDescription( m_nRuleParameterIndex ) );
	SetChildIndent( 25 );
}

void CRuleInstanceParameterPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( NULL );
}

void CRuleInstanceParameterPanel::OnCommand( const char *pCommand )
{
	if ( Q_stricmp( pCommand, "AddParam" ) == 0 )
	{
		CRuleInstanceNodePanel *pNodePanel = dynamic_cast< CRuleInstanceNodePanel * >( GetParent() );
		Assert( pNodePanel != NULL );

		KeyValues *pParentKV = pNodePanel->GetData();
		if ( Q_stricmp( pParentKV->GetName(), "rule_instance" ) != 0 )
		{
			// Must be within an element container
			pParentKV = pParentKV->FindKey( "rule_instance", NULL );
			Assert( pParentKV != NULL );
			if ( pParentKV == NULL )
				return;
		}

		const char *pParameterName = m_pRule->GetParameterName( m_nRuleParameterIndex );
		
		// Parent shouldn't have a key with this name
		Assert( pParentKV->FindKey( pParameterName ) == NULL );

		const KeyValues *pDefaultKV = m_pRule->GetDefaultValue( m_nRuleParameterIndex );
		KeyValues *pNewValue = NULL;
		if ( pDefaultKV != NULL )
		{
			pNewValue = pDefaultKV->MakeCopy();
		}
		else
		{
			pNewValue = new KeyValues( pParameterName );
			if ( m_pRule->IsExpressionAllowedForParameter( m_nRuleParameterIndex ) )
			{
				pNewValue->AddSubKey( CreateNewValue( m_pRule, m_nRuleParameterIndex ) );
			}
			else
			{
				pNewValue->SetStringValue( "" );
			}
		}
		
		pParentKV->AddSubKey( pNewValue );
		SetData( pNewValue );
		return;
	}
	else if ( Q_stricmp( pCommand, "DeleteParam" ) == 0 )
	{
		SetData( NULL );
	}

	BaseClass::OnCommand( pCommand );
}

void CRuleInstanceParameterPanel::UpdateState()
{
	bool bShouldShowOptionalValues = GetEditor()->ShouldShowOptionalValues();
	if ( GetData() == NULL && m_pRule->IsParameterOptional( m_nRuleParameterIndex ) )
	{
		SetVisible( bShouldShowOptionalValues );
	}
	else
	{
		SetVisible( true );
	}

	BaseClass::UpdateState();
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

void CRuleInstanceParameterPanel::CreatePanelContents()
{
	KeyValues *pNodeKV = GetData();
	
	// Cleaned up by the sizers
	m_pAddButton = NULL;
	m_pDeleteButton = NULL;

	Q_strncpy( m_NodeLabel, m_pRule->GetParameterFriendlyName( m_nRuleParameterIndex ), m_nNameLength );

	if ( pNodeKV != NULL )
	{
		bool bIsArray = m_pRule->IsArrayParameter( m_nRuleParameterIndex );

		if ( bIsArray )
		{
			CNodePanel *pChildPanel;
			AddChild( pChildPanel = new CArrayValuePanel( this, "ArrayParameter", m_pRule, m_nRuleParameterIndex ), 0 );
			pChildPanel->SetEditor( GetEditor() );
			pChildPanel->SetData( GetData() );
		}
		else
		{
			bool bIsChildPanel;
			CNodePanel *pNewPanel = CreateValuePanel( m_pRule, m_nRuleParameterIndex, this, GetData(), &bIsChildPanel );

			if ( pNewPanel != NULL )
			{
				if ( bIsChildPanel )
				{
					AddChild( pNewPanel, 0 );
				}
				else
				{
					AddHeadingElement( pNewPanel, 1.0f, 2 );
				}
			}
			else
			{
				Q_strncat( m_NodeLabel, " - NOT FOUND", m_nNameLength );
			}
		}

		if ( m_pRule->IsParameterOptional( m_nRuleParameterIndex ) )
		{
			m_pDeleteButton = new Button( this, "DeleteButton", "X", this, "DeleteParam" );
			AddHeadingSpacer();
			AddHeadingElement( m_pDeleteButton, 0.0f, 2 );
		}
	}
	else
	{
		m_pAddButton = new Button( this, "AddButton", "Add", this, "AddParam" );
		AddHeadingSpacer();
		AddHeadingElement( m_pAddButton, 0.0f, 2 );
	}
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation - Editable Value Panels
//////////////////////////////////////////////////////////////////////////

CNodePanel *CreateValuePanel( const CTilegenRule *pRule, int nRuleParameterIndex, CNodePanel *pParent, KeyValues *pData, bool *pIsChildPanel )
{
	CNodePanel *pNewPanel = NULL;
	*pIsChildPanel = false;
	
	const char *pTypeName = pRule->GetParameterTypeName( nRuleParameterIndex );
	bool bAllowLiterals = pRule->IsLiteralAllowedForParameter( nRuleParameterIndex );
	bool bAllowExpressions = pRule->IsExpressionAllowedForParameter( nRuleParameterIndex );
	bool bIsImmediateRuleInstance = ( Q_stricmp( pData->GetName(), "rule_instance" ) == 0 );
	bool bIsLiteralValue = ( pData->GetFirstSubKey() == NULL ) && !bIsImmediateRuleInstance;
	
	if ( bIsLiteralValue && !bAllowLiterals )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Literal value (%s) is specified in key values file but not allowed for this rule instance parameter (%s).\n", pData->GetString(), pRule->GetParameterName( nRuleParameterIndex ) );
		return NULL;
	}
	else if ( !bIsLiteralValue && !bAllowExpressions )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Non-literal value is specified in key values file but expressions are not allowed for this rule instance parameter (%s).\n", pRule->GetParameterName( nRuleParameterIndex ) );
		return NULL;
	}
	
	if ( bIsLiteralValue )
	{
		if ( Q_stricmp( pTypeName, "bool" ) == 0 )
		{
			pNewPanel = new CBoolValuePanel( pParent, "BoolParameter" );
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "string" ) == 0 )
		{
			pNewPanel = new CStringValuePanel( pParent, "StringParameter" );
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "enum" ) == 0 )
		{
			const char *pEnumName = pRule->GetParameterEnumName( nRuleParameterIndex );
			const CTilegenEnum *pTilegenEnum = pParent->GetEditor()->GetPreprocessor()->FindEnum( pEnumName );
			if ( pTilegenEnum == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Enum %s not found.\n", pEnumName );
				return NULL;
			}
			pNewPanel = new CEnumValuePanel( pParent, "EnumParameter", pTilegenEnum );
			*pIsChildPanel = false;
		}
		else if (
			Q_stricmp( pTypeName, "int" ) == 0 || 
			Q_stricmp( pTypeName, "bool" ) == 0 || 
			Q_stricmp( pTypeName, "float" ) == 0 )
		{
			pNewPanel = new CStringValuePanel( pParent, "StringParameter" );
			( ( CStringValuePanel * )pNewPanel )->AllowNumericInputOnly();
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "room_name" ) == 0 )
		{
			pNewPanel = new CFileValuePanel( pParent, "RoomParameter", "Room Template", "*.roomtemplate", "tilegen\\roomtemplates\\", true, true );
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "layout_name" ) == 0 )
		{
			pNewPanel = new CFileValuePanel( pParent, "LayoutParameter", "Layout", "*.layout", "tilegen\\layouts\\", false, false );
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "instance_name" ) == 0 )
		{
			pNewPanel = new CFileValuePanel( pParent, "InstanceParameter", "Instance", "*.vmf", "tilegen\\instances\\", true, false );
			*pIsChildPanel = false;
		}
		else if ( Q_stricmp( pTypeName, "theme_name" ) == 0 )
		{
			pNewPanel = new CThemeValuePanel( pParent, "ThemeParameter" );
			*pIsChildPanel = true;
		}
		else
		{
			// Unrecognized type
			return NULL;
		}
	}
	else
	{
		if ( !bIsImmediateRuleInstance )
		{
			if ( pData->GetFirstSubKey()->GetNextKey() != NULL || 
				 Q_stricmp( pData->GetFirstSubKey()->GetName(), "rule_instance" ) != 0 )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "No rule_instance found where one was expected.\n" );
				return NULL;
			}
		}

		CRuleInstanceNodePanel *pRuleInstancePanel = new CRuleInstanceNodePanel( pParent, "RuleInstance" );
		pRuleInstancePanel->AddAllowableRuleType( pRule->GetParameterTypeName( nRuleParameterIndex ) );
		pNewPanel = pRuleInstancePanel;
		*pIsChildPanel = true;
	}

	pNewPanel->SetEditor( pParent->GetEditor() );
	pNewPanel->SetData( pData );
	return pNewPanel;
}

KeyValues *CreateNewValue( const CTilegenRule *pRule, int nRuleParameterIndex )
{
	// @TODO: handle creating literals, not just sub-rules
	const char *pElementContainer = pRule->GetParameterElementContainer( nRuleParameterIndex );
	if ( pElementContainer != NULL )
	{
		return new KeyValues( pElementContainer, "rule_instance", "<null>" );
	}
	else
	{
		return new KeyValues( "rule_instance", NULL, "<null>" );
	}
}

CEditableValuePanel::CEditableValuePanel( Panel *pParent, const char *pName ) :
BaseClass( pParent, pName )
{
	SetPaintBackgroundEnabled( false );
}

CArrayValuePanel::CArrayValuePanel( Panel *pParent, const char *pName, const CTilegenRule *pRule, int nRuleParameterIndex ) :
BaseClass( pParent, pName ),
m_pRule( pRule ),
m_nRuleParameterIndex( nRuleParameterIndex )
{
}

void CArrayValuePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( NULL );
}

void CArrayValuePanel::CreatePanelContents()
{
	KeyValues *pNodeKV = GetData();

	bool bOrderedArray = m_pRule->IsOrderedArrayParameter( m_nRuleParameterIndex );

	int nIndex = 0;
	for ( KeyValues *pSubKey = pNodeKV->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		bool bIgnored;
		CNodePanel *pChildPanel = CreateValuePanel( m_pRule, m_nRuleParameterIndex, this, pSubKey, &bIgnored );

		if ( pChildPanel != NULL )
		{
			if ( bOrderedArray )
			{
				AddNewParameterElementPanel( nIndex );
			}
			
			pChildPanel->EnableDeleteButton( true );
			AddChild( pChildPanel, 0 );
		}
		
		++ nIndex;
	}
	AddNewParameterElementPanel( nIndex );
}

void CArrayValuePanel::AddNewParameterElementPanel( int nIndex )
{
	bool bAllowValues = m_pRule->IsLiteralAllowedForParameter( m_nRuleParameterIndex );
	bool bAllowRules = m_pRule->IsExpressionAllowedForParameter( m_nRuleParameterIndex );
	
	CNewElementPanel *pNewElementPanel = new CNewElementPanel( this, "NewElement", nIndex );
	if ( bAllowRules )
	{
		pNewElementPanel->AddButton( "Add Rule", "AddRule" );
	}
	// @TODO: add value support
	bAllowValues;
	
	AddChild( pNewElementPanel, 0 );
}

void CArrayValuePanel::OnAddRule( KeyValues *pKV )
{
	int nIndex = pKV->GetInt( "index", -1 );
	Assert( nIndex != -1 );
	InsertChildData( nIndex, CreateNewValue( m_pRule, m_nRuleParameterIndex ) );
}

CBoolValuePanel::CBoolValuePanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName )
{
	m_pCheckButton = new CheckButton( this, "CheckBox", NULL );
	
	CBoxSizer *pChildSizer = new CBoxSizer( ESLD_HORIZONTAL );
	pChildSizer->AddPanel( m_pCheckButton, SizerAddArgs_t().Padding( 0 ).Expand( 0.0f ) );
	SetSizer( pChildSizer );
}

void CBoolValuePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( NULL );
}

void CBoolValuePanel::OnButtonChecked( Panel *pCheckButton )
{
	Assert( pCheckButton == m_pCheckButton );
	GetData()->SetBool( NULL, m_pCheckButton->IsSelected() );
}

void CBoolValuePanel::UpdateState()
{
	m_pCheckButton->SetSelected( GetData()->GetBool() );
	BaseClass::UpdateState();
}

CStringValuePanel::CStringValuePanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName )
{
	m_pTextEntry = new TextEntry( this, "TextEntry" );
	m_pTextEntry->SetAutoLocalize( false );

	CBoxSizer *pChildSizer = new CBoxSizer( ESLD_HORIZONTAL );
	pChildSizer->AddPanel( m_pTextEntry, SizerAddArgs_t().Padding( 2 ).MinSize( 20, 20 ).Expand( 1.0f ) );
	SetSizer( pChildSizer );
}

void CStringValuePanel::OnTextChanged( Panel *pTextEntry )
{
	Assert( pTextEntry == m_pTextEntry );

	const int nMaxTextLength = 2048;
	char buf[nMaxTextLength];
	m_pTextEntry->GetText( buf, nMaxTextLength );
	buf[nMaxTextLength - 1] = '\0';
	GetData()->SetString( NULL, buf );
}

void CStringValuePanel::UpdateState()
{
	m_pTextEntry->SetText( GetData()->GetString() );
	BaseClass::UpdateState();
}

CEnumValuePanel::CEnumValuePanel( Panel *pParent, const char *pName, const CTilegenEnum *pTilegenEnum ) :
BaseClass( pParent, pName ),
m_pTilegenEnum( pTilegenEnum )
{
	m_pComboBox = new ComboBox( this, "ComboBox", 6, false );
	m_pComboBox->SetAutoLocalize( false );

	for ( int i = 0; i < pTilegenEnum->GetEnumCount(); ++ i )
	{
		KeyValues *pItemKV = new KeyValues( "EnumValue" );
		pItemKV->SetInt( NULL, pTilegenEnum->GetEnumValue( i ) );
		m_pComboBox->AddItem( pTilegenEnum->GetEnumString( i ), pItemKV );
	}

	CBoxSizer *pChildSizer = new CBoxSizer( ESLD_HORIZONTAL );
	pChildSizer->AddPanel( m_pComboBox, SizerAddArgs_t().Padding( 2 ).MinSize( 20, 20 ).Expand( 1.0f ) );
	SetSizer( pChildSizer );
}

void CEnumValuePanel::OnTextChanged( vgui::Panel *pComboBox )
{
	Assert( pComboBox == m_pComboBox );

	KeyValues *pItemKV = m_pComboBox->GetActiveItemUserData();
	if ( pItemKV != NULL )
	{
		GetData()->SetInt( NULL, pItemKV->GetInt() );
	}
}

void CEnumValuePanel::UpdateState()
{
	int nValue = GetData()->GetInt();
	int nEntry = m_pTilegenEnum->FindEntry( nValue );
	if ( nEntry == -1 )
	{
		m_pComboBox->SetText( "INVALID ENTRY" );
	}
	else
	{
		m_pComboBox->ActivateItem( nEntry );
	}

	BaseClass::UpdateState();
}

CThemeValuePanel::CThemeValuePanel( Panel *pParent, const char *pName ) :
BaseClass( pParent, pName )
{
	m_pChangeThemeButton = new Button( this, "ChangeThemeButton", "Change Theme", this, "ChangeTheme" );
	m_pThemeDetails = new CThemeDetails( this, "ThemeDetails", NULL );
	
	m_pThemeDetails->m_iDesiredWidth = 350;
	m_pThemeDetails->m_iDesiredHeight = 100;
	m_pThemeDetails->InvalidateLayout();
	m_pChangeThemeButton->SetSize( 120, 20 );

	m_pThemeDetails->SetPaintBackgroundEnabled( false );
	m_pChangeThemeButton->SetPaintBackgroundEnabled( false );

	CBoxSizer *pChildSizer = new CBoxSizer( ESLD_HORIZONTAL );
	pChildSizer->AddPanel( m_pThemeDetails, SizerAddArgs_t().Padding( 0 ).MinSize( 350, 100 ) );
	pChildSizer->AddPanel( m_pChangeThemeButton, SizerAddArgs_t().Padding( 0 ).MinSize( 120, 20 ).MinorExpand( false ) );
	SetSizer( pChildSizer );
}

void CThemeValuePanel::OnCommand( const char *pCommand )
{
	if ( Q_stricmp( pCommand, "ChangeTheme") == 0 )
	{
		CThemeRoomPicker *pPicker = new CThemeRoomPicker( GetParent(), "ThemeRoomPicker", GetData(), false );
		pPicker->AddActionSignalTarget( this );
		pPicker->DoModal();
		return;
	}
	else if ( Q_stricmp( pCommand, "Update" ) == 0 )
	{
		m_pThemeDetails->SetTheme( CLevelTheme::FindTheme( GetData()->GetString() ) );
		m_pThemeDetails->InvalidateLayout();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CThemeValuePanel::UpdateState()
{
	m_pThemeDetails->SetTheme( CLevelTheme::FindTheme( GetData()->GetString() ) );
	BaseClass::UpdateState();
}

CFileValuePanel::CFileValuePanel( Panel *pParent, const char *pName, const char *pFileTypeName, const char *pFileExtension, const char *pBaseDirectory, bool bStripPath, bool bStripExtension ) :
BaseClass( pParent, pName ),
m_bStripPath( bStripPath ),
m_bStripExtension( bStripExtension )
{
	m_pTextEntry = new TextEntry( this, "TextEntry" );
	m_pTextEntry->SetAutoLocalize( false );
	m_pTextEntry->SetEditable( false );
	
	m_pChangeFileButton = new Button( this, "ChangeFileButton", "...", this, "ChangeFile" );
	
	CBoxSizer *pChildSizer = new CBoxSizer( ESLD_HORIZONTAL );
	pChildSizer->AddPanel( m_pTextEntry, SizerAddArgs_t().Padding( 2 ).MinSize( 20, 20 ).Expand( 1.0f ) );
	pChildSizer->AddPanel( m_pChangeFileButton, SizerAddArgs_t().Padding( 2 ).MinSize( 20, 20 ) );
	SetSizer( pChildSizer );
	
	Q_strncpy( m_FileTypeName, pFileTypeName, _countof( m_FileTypeName ) );
	Q_strncpy( m_FileExtension, pFileExtension, _countof( m_FileExtension ) );
	Q_strncpy( m_BaseDirectory, pBaseDirectory, _countof( m_BaseDirectory ) );
}

void CFileValuePanel::OnCommand( const char *pCommand )
{
	extern char	g_gamedir[1024];

	if ( Q_stricmp( pCommand, "ChangeFile" ) == 0 )
	{
		// Dispaly a file dialog
		char buf[MAX_PATH];
		Q_snprintf( buf, MAX_PATH, "Pick %s", m_FileTypeName );
		FileOpenDialog *pFileDialog = new FileOpenDialog( this, buf, true );

		char templateDirectory[MAX_PATH];
		Q_snprintf( templateDirectory, _countof( templateDirectory ), "%s\\%s", g_gamedir, m_BaseDirectory );
		pFileDialog->SetStartDirectory( templateDirectory );
		Q_snprintf( buf, MAX_PATH, "%s (%s)", m_FileTypeName, m_FileExtension );
		pFileDialog->AddFilter( m_FileExtension, buf, true );
		pFileDialog->AddActionSignalTarget( this );
		pFileDialog->DoModal( false );
		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CFileValuePanel::OnFileSelected( KeyValues *pKeyValues )
{
	const char *pFullPath = pKeyValues->GetString( "fullpath", NULL );
	if ( pFullPath != NULL )
	{
		char buffer[MAX_PATH];
		if ( g_pFullFileSystem->FullPathToRelativePath( pFullPath, buffer, MAX_PATH ) )
		{
			const char *pFileName = buffer;
			Q_FixSlashes( buffer );
			if ( m_bStripExtension )
			{
				Q_StripExtension( buffer, buffer, MAX_PATH );
			}
			if ( m_bStripPath )
			{	
				const char *pFileStart = Q_stristr( buffer, m_BaseDirectory );
				if ( pFileStart != NULL )
				{
					pFileName = pFileStart + Q_strlen( m_BaseDirectory );
				}
			}
			if ( pFileName != NULL )
			{
				m_pTextEntry->SetText( pFileName );

				GetData()->SetString( NULL, pFileName );
				return;
			}		
		}
		Log_Warning( LOG_TilegenLayoutSystem, "Could not set file to %s (is it underneath the /tilegen/roomtemplates directory?)", pFullPath );
	}
}

void CFileValuePanel::UpdateState()
{
	m_pTextEntry->SetText( GetData()->GetString() );
	BaseClass::UpdateState();
}