//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_quest_editor_panel.h"
#include "econ_item_tools.h"
#include "tier2/p4helpers.h"
#include "tier2/fileutils.h"
#include "vgui/IInput.h"

#ifdef STAGING_ONLY

using namespace vgui;

ConVar tf_quest_editor_element_height( "tf_quest_editor_element_height", "20", FCVAR_ARCHIVE );
ConVar tf_quest_editor_indent_width( "tf_quest_editor_indent_width", "20", FCVAR_ARCHIVE );
ConVar tf_quest_editor_entry_inset( "tf_quest_editor_entry_inset", "120" );

static const char *g_skItemNameFormat = "quest%d";
static const char *g_skNameTokenFormat = "#quest%dname%d";
static const char *g_skDescTokenFormat = "#quest%ddesc%d";
static const char *g_skObjectiveDescTokenFormat = "#quest%dobjectivedesc%d";
static const char *g_skLocalizationFile = "resource/tf_quests_english.txt";
static const char *g_skQuestDefFile = "tf/scripts/items/unencrypted/_items_quests.txt";
static const char *g_skQuestObjectivesConditionsDefFile = "tf/scripts/items/unencrypted/_items_quest_objective_definitions.txt";
static const char *g_skQuestObjectivesHouseKeepingFile = "items_quest_objective_housekeeping.csv";
static const int g_nkFirstQuestDef = 25000;

static const char* g_skQuestPrefabs[] = { "quest_prefab_1st_operation_pauling"
										, "quest_prefab_halloween_2015"
										, "quest_prefab_2nd_operation_pauling" };

#define FLAGS_NONE 0
#define FLAG_HIDDEN 1<<0
#define FLAG_COLLAPSED 1<<1
#define FLAG_COLLAPSABLE 1<<2
#define FLAG_MUST_BE_LAST 1<<3
#define FLAG_NOT_DELETABLE 1<<4
#define FLAG_OUTPUT_LAST 1<<5
#define FLAG_DELETED 1<<6
#define FLAG_HIGHLIGHT_MOUSEOVER 1<<7
#define FLAG_DONT_EXPORT 1<<8

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static CQuestEditorPanel* g_pQuestEditor = NULL;
static void cc_tf_quest_editor()
{
	g_pQuestEditor = new CQuestEditorPanel( NULL, "QuestEditor" );
	g_pQuestEditor->MakeReadyForUse();
	g_pQuestEditor->Deploy();
}
ConCommand tf_quest_editor( "tf_quest_editor", cc_tf_quest_editor );


//-----------------------------------------------------------------------------
// Purpose:	Writes out all of the localization data for each quest intro "resource/tf_quests_english.txt"
//-----------------------------------------------------------------------------
void WriteLocalizationData()
{
	char szQuestEnglishFile[MAX_PATH];
	if ( !GenerateFullPath( g_skLocalizationFile, "MOD", szQuestEnglishFile, ARRAYSIZE( szQuestEnglishFile ) ) )
	{
		Warning( "Failed to GenerateFullPath %s\n", g_skLocalizationFile );
		return;
	}
	
	FOR_EACH_VEC( ILocalizationEditorParamAutoList::AutoList(), i )
	{
		CLocalizationEditorParam* pLocalizationEntry = static_cast< CLocalizationEditorParam* >( ILocalizationEditorParamAutoList::AutoList()[i] );
		// We set the filename to "deleted" for deleted entries so that they don't get written out
		// into tf_quests_english.txt
		const char* pszFile = pLocalizationEntry->IsFlagSet( FLAG_DELETED, true ) ? "deleted" : szQuestEnglishFile;

		wchar_t wszLocalizedValue[MAX_QUEST_DESC_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( pLocalizationEntry->GetLocalizationValue(), wszLocalizedValue, sizeof( wszLocalizedValue ) );
		
		const char *pszTokenName = pLocalizationEntry->GetValue();
		if ( pszTokenName[0] == '#' )
		{
			pszTokenName++;
		}
		g_pVGuiLocalize->AddString( pszTokenName, wszLocalizedValue, pszFile );
	}

	// Get the name of the file and p4 check it out
	char szCorrectCaseFilePath[MAX_PATH];
	g_pFullFileSystem->GetCaseCorrectFullPath( szQuestEnglishFile, szCorrectCaseFilePath );
	CP4AutoEditFile a( szCorrectCaseFilePath );

	g_pVGuiLocalize->SaveToFile( szQuestEnglishFile );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditorObject::IEditorObject( EditorObjectInitStruct init )
	: EditablePanel( init.pParent, init.m_pszKeyName )
	, m_Flags( init.nFlags )
	, m_pOwningEditable( NULL )
	, m_bHasChanges( false )
{
	memset( m_szKeyName, 0, sizeof( m_szKeyName ) );
	if ( init.m_pszKeyName )
	{
		V_sprintf_safe( m_szKeyName, "%s", init.m_pszKeyName );
	}

	SetWide( init.pParent->GetWide() );

	SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_RIGHT, 6, 6, -6, -6 );
}

void IEditorObject::PerformLayout()
{
	BaseClass::PerformLayout();

	SetTall( GetContentTall() );
}

void IEditorObject::OnSizeChanged( int newWide, int newTall )
{
	BaseClass::OnSizeChanged( newWide, newTall );

	GetParent()->InvalidateLayout();
}


bool IEditorObject::IsFlagSet( int nFlag, bool bCheckUpTree ) const
{ 
	bool bIsFlagSet = m_Flags & nFlag;
	if ( !bIsFlagSet && bCheckUpTree )
	{
		IEditorObject* pParent = dynamic_cast< IEditorObject* >( const_cast<IEditorObject*>(this)->GetParent() );
		if ( pParent )
		{
			bIsFlagSet |= pParent->IsFlagSet( nFlag, bCheckUpTree );
		}
	}
	return bIsFlagSet;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditorObject::ESerializeAction IEditorObject::ShouldWrite( const IEditableDataType* pCallingEditable ) const
{
	if ( IsFlagSet( FLAG_DONT_EXPORT ) )
		return SKIP_THIS_AND_CHILDREN;

	if ( IsFlagSet( FLAG_DELETED ) )
		return SKIP_THIS_AND_CHILDREN;

	if ( GetOwningEditable() != pCallingEditable )
		return SKIP_THIS_AND_CONTINUE;
		
	return WRITE_THIS_AND_CONTINUE;
}

//-----------------------------------------------------------------------------
// Purpose: If we're set to highlight on mouseover, do so!
//-----------------------------------------------------------------------------
void IEditorObject::OnThink()
{
	if ( IsFlagSet( FLAG_HIGHLIGHT_MOUSEOVER, false ) )
	{
		if ( IsCursorOver() )
		{
			SetBgColor( Color( 100, 150, 100, 30 ) );
		}
		else
		{
			SetBgColor( Color( 235, 226, 202, 2 ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditorObjectNode::CEditorObjectNode( EditorObjectInitStruct init )
	: BaseClass( init )
{
	m_pToggleCollapseButton = new Button( this, "togglecollapse", "-", this, "togglecollapse" );
	m_pToggleCollapseButton->SetVisible( false );
	m_pToggleCollapseButton->SetWide( 20 );
	m_pToggleCollapseButton->SetZPos( 100 );

	m_pDeleteButton = new Button( this, "deletebutton", "x", this, "delete" );
	m_pDeleteButton->SetWide( 20 );
	m_pDeleteButton->SetTall( 20 );

	CEditorObjectNode* pNode = dynamic_cast< CEditorObjectNode* >( init.pParent );
	if ( pNode )
	{
		pNode->AddChild( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditorObjectNode::~CEditorObjectNode()
{
	CEditorObjectNode* pEditorPanel = dynamic_cast<CEditorObjectNode*>( GetParent() );
	if ( pEditorPanel )
	{
		pEditorPanel->RemoveChild( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const IEditableDataType* IEditorObject::GetOwningEditable() const
{
	const IEditableDataType* pOwningEditable = NULL;
	const IEditorObject* pPanel = this;
	
	do 
	{
		pOwningEditable = pPanel->m_pOwningEditable;
		pPanel = dynamic_cast< const IEditorObject* >( const_cast< IEditorObject* >( pPanel )->GetParent() ); // Good lord, VGUI.  GetParent() isn't const?  Really?
	} 
	while ( pOwningEditable == NULL && pPanel != NULL );

	Assert( pOwningEditable != NULL );
		
	return pOwningEditable;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObject::InvalidateChain()
{
	InvalidateLayout();
		
	Panel* pParent = this;

	do
	{
		pParent = pParent->GetParent();
		pParent->InvalidateLayout();
	}
	while( dynamic_cast< IEditorObject* >( pParent ) );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObject::ClearPendingChangesFlag()
{
	m_bHasChanges = false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditorObjectNode::AddChild( IEditorObject* pChild )
{
	m_bHasChanges = true;
	m_vecChildren.AddToTail( pChild );
	if ( !IsFlagSet( FLAG_HIDDEN ) && IsFlagSet( FLAG_COLLAPSABLE ) )
	{
		m_pToggleCollapseButton->SetVisible( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditorObjectNode::RemoveChild( IEditorObject* pChild )
{
	FOR_EACH_VEC( m_vecChildren, i )
	{
		if ( m_vecChildren[i] == pChild )
		{
			m_vecChildren.Remove( i );
			if ( m_vecChildren.Count() == 0 )
			{
				m_pToggleCollapseButton->SetVisible( false );
			}

			m_bHasChanges = true;

			return;
		}
	}

	Assert( !"Couldn't find child to remove!" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CEditorObjectNode::GetNextAvailableKeyNumber() const
{
	int nNextAvailable = 0;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		if ( !m_vecChildren[i]->IsFlagSet( FLAG_DONT_EXPORT ) )
		{
			nNextAvailable = Max( nNextAvailable, atoi( m_vecChildren[i]->GetName() ) );
		}
	}

	return nNextAvailable;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEditorObjectNode::HasChanges( bool bCheckChildren ) const
{
	bool bHasChanges = m_bHasChanges;

	if ( bCheckChildren )
	{
		FOR_EACH_VEC( m_vecChildren, i )
		{
			bHasChanges |= m_vecChildren[i]->HasChanges( bCheckChildren );
		}
	}

	return bHasChanges;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CEditorObjectNode::GetContentTall() const
{
	// Hidden panels have 0 height
	int nTall = IsFlagSet( FLAG_HIDDEN ) ? 0 : tf_quest_editor_element_height.GetInt();

	// Collapsed panels just return their height.
	if ( IsFlagSet( FLAG_COLLAPSED ) )
		return nTall;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		nTall += m_vecChildren[i]->GetContentTall();
	}

	return nTall;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditorObjectNode::SerializeToKVs( KeyValues* pKV, const IEditableDataType* pCallingEditable ) const
{
	m_bHasChanges = false;

	// End of the Line if deleted
	if ( IsFlagSet( FLAG_DELETED ) )
		return;

	ESerializeAction action = ShouldWrite( pCallingEditable );
	if ( action == SKIP_THIS_AND_CHILDREN )
		return;

	if ( action != SKIP_THIS_AND_CONTINUE )
	{
		pKV = pKV->CreateNewKey();
		pKV->SetName( GetKeyName() );
	}

	CUtlVector< IEditorObject* > vecLastOutputs;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		if ( m_vecChildren[ i ]->IsFlagSet( FLAG_OUTPUT_LAST ) )
		{
			vecLastOutputs.AddToTail( m_vecChildren[i] );
			continue;
		}

		m_vecChildren[i]->SerializeToKVs( pKV, pCallingEditable );

	}

	FOR_EACH_VEC( vecLastOutputs, i )
	{
		vecLastOutputs[ i ]->SerializeToKVs( pKV, pCallingEditable );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditorObjectNode::PerformLayout() 
{
	m_pDeleteButton->SetVisible( m_vecChildren.Count() > 0 && !IsFlagSet( FLAG_HIDDEN ) && !IsFlagSet( FLAG_NOT_DELETABLE ) );
	m_pDeleteButton->SetPos( 470, 0 );

	BaseClass::PerformLayout();

	int nTall = IsFlagSet( FLAG_HIDDEN ) ? 0 : tf_quest_editor_element_height.GetInt();

	CUtlVector< IEditorObject* > vecLastParams;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		if ( IsFlagSet( FLAG_MUST_BE_LAST ) )
		{
			vecLastParams.AddToTail( m_vecChildren[i] );
			continue;
		}

		m_vecChildren[i]->SetPos( IsFlagSet( FLAG_HIDDEN ) ? 0 : tf_quest_editor_indent_width.GetInt(), nTall );
		nTall += m_vecChildren[i]->GetContentTall();
	}

	FOR_EACH_VEC( vecLastParams, i )
	{
		vecLastParams[i]->SetPos( IsFlagSet( FLAG_HIDDEN ) ? 0 : tf_quest_editor_indent_width.GetInt(), nTall );
		nTall += vecLastParams[i]->GetContentTall();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditorObjectNode::OnCommand( const char *command )
{
	if ( V_stricmp( "togglecollapse", command ) == 0 )
	{
		if ( IsFlagSet( FLAG_COLLAPSED ) )
		{
			m_pToggleCollapseButton->SetText( "-" );
		}
		else
		{
			m_pToggleCollapseButton->SetText( "+" );
		}		

		SetFlag( FLAG_COLLAPSED, !IsFlagSet( FLAG_COLLAPSED ) );
		InvalidateChain();

		return;
	}
	if ( V_stricmp( command, "delete" ) == 0 )
	{
		RemoveNode();
		return;
	}

	BaseClass::OnCommand( command );
}

void CEditorObjectNode::MarkForDeletion()
{
	BaseClass::MarkForDeletion();

	FOR_EACH_VEC( m_vecChildren, i )
	{
		m_vecChildren[i]->MarkForDeletion();
	}
}

void CEditorObjectNode::RemoveNode()
{
	MarkForDeletion();

	InvalidateChain();	
}

void CEditorObjectNode::ClearPendingChangesFlag()
{
	BaseClass::ClearPendingChangesFlag();

	FOR_EACH_VEC( m_vecChildren, i )
	{
		m_vecChildren[ i ]->ClearPendingChangesFlag();
	}
}

IEditorObjectParameter::IEditorObjectParameter( EditorObjectInitStruct init, const char *pszLabelText )
	: BaseClass( init )
{
	memset( m_szSavedValueBuff, 0, sizeof( m_szSavedValueBuff ) );

	SetTall( tf_quest_editor_element_height.GetInt() );

	m_pLabel = new Label( this, "paramlabel", pszLabelText );
	m_pLabel->SetWide( 100 );
	m_pLabel->SetPos( 20, 0 );
	m_pLabel->SetVisible( pszLabelText != NULL );

	CEditorObjectNode* pNode = dynamic_cast< CEditorObjectNode* >( init.pParent );
	if ( pNode )
	{
		pNode->AddChild( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditorObjectParameter::~IEditorObjectParameter()
{
	CEditorObjectNode* pEditorPanel = dynamic_cast<CEditorObjectNode*>( GetParent() );
	if ( pEditorPanel )
	{
		pEditorPanel->RemoveChild( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObjectParameter::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pLabel->SetPos( 20, 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObjectParameter::SerializeToKVs( KeyValues* pKV, const IEditableDataType* pCallingEditable ) const
{
	m_bHasChanges = false;

	if ( ShouldWrite( pCallingEditable ) == WRITE_THIS_AND_CONTINUE )
	{
		KeyValues* pKVStomp = pKV->FindKey( GetKeyName() );
		if ( pKVStomp )
		{
			AssertMsg2( false, "STOMPING existing key: %s with %s\n", pKVStomp->GetName(), GetValue() );
			KeyValuesDumpAsDevMsg( pKVStomp );
		}

		pKV->SetString( GetKeyName(), GetValue() );

		UpdateSavedValue( GetValue() );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObjectParameter::UpdateSavedValue( const char* pszNewValue ) const
{
	// Update saved string
	memset( m_szSavedValueBuff, 0, sizeof( m_szSavedValueBuff ) );
	V_sprintf_safe( m_szSavedValueBuff, "%s", pszNewValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IEditorObjectParameter::OnTextChanged( KeyValues *data )
{
	m_bHasChanges = CheckForChanges();

	IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	Color labelNormalFGColor = GetSchemeColor("Label.TextColor", Color(0, 0, 0, 255), pScheme);
	Color labelChangesFGColor = GetSchemeColor("RichText.SelectedBgColor", Color(0, 0, 0, 255), pScheme);

	m_pLabel->SetFgColor( m_bHasChanges ? labelChangesFGColor : labelNormalFGColor );

	g_pQuestEditor->CheckForChanges();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool IEditorObjectParameter::CheckForChanges() const
{
	return !FStrEq( GetValue(), m_szSavedValueBuff );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTextEntryEditorParam::CTextEntryEditorParam( EditorObjectInitStruct init, const char *pszLabelText, const char *pszValue )
	: BaseClass( init, pszLabelText )
{
	UpdateSavedValue( pszValue );

	SetTall( tf_quest_editor_element_height.GetInt() );
	m_pTextEntry = new TextEntry( this, "entry" );
	m_pTextEntry->SetPos( tf_quest_editor_entry_inset.GetInt(), 0 );
	m_pTextEntry->SetWide( 400 );
	m_pTextEntry->SetTall( tf_quest_editor_element_height.GetInt() );
	m_pTextEntry->SetMultiline( true );
	m_pTextEntry->SetCatchEnterKey( true );
	SetTextEntryValue( pszValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextEntryEditorParam::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pTextEntry->SetPos( tf_quest_editor_entry_inset.GetInt(), 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CTextEntryEditorParam::GetContentTall() const
{
	return IsFlagSet( FLAG_HIDDEN ) ? 0 : m_pTextEntry->GetTall();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextEntryEditorParam::SetTextEntryValue( const char* pszValue )
{
	m_pTextEntry->SetText( pszValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CTextEntryEditorParam::GetValue() const 
{
	m_pTextEntry->GetText( m_szValueBuff, sizeof( m_szValueBuff ) );
	return m_szValueBuff;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IMPLEMENT_AUTO_LIST( ILocalizationEditorParamAutoList );
CLocalizationEditorParam::CLocalizationEditorParam( EditorObjectInitStruct init, const char *pszLabelText, const char *pszLocalizationToken )
	: BaseClass( init, pszLabelText, NULL )
{
	char szBuff[MAX_QUEST_DESC_LENGTH];
	memset( szBuff, 0, sizeof( szBuff ) );
	g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( pszLocalizationToken ), szBuff, sizeof( szBuff ) );
	SetTextEntryValue( szBuff );
	UpdateSavedValue( szBuff );

	V_sprintf_safe( m_szLocalizationToken, "%s", pszLocalizationToken );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CLocalizationEditorParam::GetValue() const
{
	// Always return the localization token
	return m_szLocalizationToken;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CLocalizationEditorParam::GetLocalizationValue() const
{
	return BaseClass::GetValue();
}

bool CLocalizationEditorParam::CheckForChanges() const
{
	return !FStrEq( m_szSavedValueBuff, GetLocalizationValue() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CComboBoxEditorParam::CComboBoxEditorParam( EditorObjectInitStruct init, const char *pszLabelText )
	: BaseClass( init, pszLabelText )
	, m_pComboBox( NULL )
{
	m_pComboBox = new ComboBox( this, "combobox", 10, false );
	m_pComboBox->AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CComboBoxEditorParam::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pComboBox->SetWide( 350 );
	m_pComboBox->SetPos( tf_quest_editor_entry_inset.GetInt(), 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CComboBoxEditorParam::GetContentTall() const 
{
	return IsFlagSet( FLAG_HIDDEN ) ? 0 : m_pComboBox->GetTall();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CComboBoxEditorParam::GetValue() const 
{
	return m_pComboBox->GetActiveItemUserData()->GetString( "write" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CComboBoxEditorParam::AddComboBoxEntry( const char* pszText, bool bSelected, const char* pszWriteValue, const char* pszCommand )
{
	KeyValues *pKVData = new KeyValues( "data" );
	pKVData->SetString( "write", pszWriteValue );
	pKVData->SetString( "command", pszCommand );

	int nIndex = m_pComboBox->AddItem( pszText, pKVData );

	if ( bSelected )
	{
		m_pComboBox->SilentActivateItemByRow( nIndex );
		UpdateSavedValue( pszWriteValue );
	}

	if ( pszWriteValue == NULL )
	{
		pszWriteValue = pszText;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CComboBoxEditorParam::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pComboBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if ( pComboBox == m_pComboBox )
	{
		CQuestObjectiveRestrictionNode* pParent = dynamic_cast< CQuestObjectiveRestrictionNode* >( GetParent() );
		if ( pParent )
		{
			// What to do
			const char* pszCommand = m_pComboBox->GetActiveItemUserData()->GetString( "command" );
			// What to write
			const char* pszWriteValue = m_pComboBox->GetActiveItemUserData()->GetString( "write" );
			if ( V_stricmp( pszCommand, "changetype" ) == 0 )
			{
				pParent->SetNewType( pszWriteValue );
			}
			else if ( V_stricmp( pszCommand, "changeevent" ) == 0 )
			{
				pParent->SetNewEvent( pszWriteValue );
			}
		}
	}

	BaseClass::OnTextChanged( data );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CNewQuestObjectiveParam::CNewQuestObjectiveParam( EditorObjectInitStruct init, const char *pszLabelText )
	: BaseClass( init, NULL )
{
	SetFlag( FLAG_DONT_EXPORT, true );

	m_pAddButton = new Button( this, "add", "Add New Filter", this, "add" );
	m_pAddButton->SetTall( 20 );
	m_pComboBox->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNewQuestObjectiveParam::PerformLayout()
{
	BaseClass::PerformLayout();
	m_pAddButton->SetWide( 200 );
	m_pAddButton->SetPos( tf_quest_editor_indent_width.GetInt(), 0 );
	m_pAddButton->SetContentAlignment( Label::a_center );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNewQuestObjectiveParam::OnCommand( const char *command )
{
	if ( FStrEq( command, "add" ) )
	{
		CQuestObjectiveRestrictionNode *pObjectiveParent = dynamic_cast< CQuestObjectiveRestrictionNode* >( GetParent() );
		Assert( pObjectiveParent );
		if ( pObjectiveParent )
		{
			const char *pszWriteValue = m_pComboBox->GetActiveItemUserData()->GetString( "write" );
			CTFQuestCondition *pNewCondition = pObjectiveParent->GetCondition()->AddChildByName( pszWriteValue );
			if ( pNewCondition )
			{
				new CQuestObjectiveRestrictionNode( { GetParent(), CFmtStr( "%d", pObjectiveParent->GetNextAvailableKeyNumber() ), FLAGS_NONE }, pNewCondition );
				InvalidateChain();
			}
		}
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IOptionalExpandableBlock::IOptionalExpandableBlock( EditorObjectInitStruct init, const char* pszButtonText )
	: BaseClass( init )
{ 
	m_pAddButton = new Button( this, "add", pszButtonText, this, "add" );
}

void IOptionalExpandableBlock::InitControls( KeyValues* pKVBlock )
{
	int nNumControlsCreated = 0;
	if ( pKVBlock )
	{
		FOR_EACH_TRUE_SUBKEY( pKVBlock, pKVKey )
		{
			CreateNewControl( pKVKey );
			++nNumControlsCreated;
		}
	}

	while( nNumControlsCreated < GetMinCount() )
	{
		CreateNewDefaultControl();
		++nNumControlsCreated;
	}

	InvalidateChain();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IOptionalExpandableBlock::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pAddButton->SetPos( 0, 0 );
	m_pAddButton->SetWide( 200 );
	m_pAddButton->SetContentAlignment(Label::a_center);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IOptionalExpandableBlock::OnCommand( const char *command )
{
	if ( FStrEq( "add", command ) )
	{
		CreateNewDefaultControl();
		InvalidateChain();
		return;
	}

	BaseClass::OnCommand( command );
}

void IOptionalExpandableBlock::CreateNewControl( KeyValues* pKV )
{
	SetFlag( FLAG_DONT_EXPORT, false );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CQuestObjectiveNode::CQuestObjectiveNode( CEditorObjectNode* pParentNode, KeyValues* pKVObjective )
	: BaseClass( { pParentNode, pKVObjective->GetName(), FLAG_COLLAPSABLE } )
	, m_pDefIndexComboBox( NULL )
	, m_pSelectObjectiveButton( NULL )
{
	// Create the description field
	new CLocalizationEditorParam( { this, "description_string", FLAGS_NONE }, "Objective Desc:", pKVObjective->GetString( "description_string" ) );
	new CTextEntryEditorParam( { this, "defindex", FLAG_HIDDEN }, "Defindex", pKVObjective->GetString( "defindex" ) );

	CComboBoxEditorParam * pOptionalComboBox = new CComboBoxEditorParam( { this, "optional", FLAGS_NONE }, "Optional:" );
	pOptionalComboBox->AddComboBoxEntry( "False", !pKVObjective->GetBool( "optional", false ), "0", NULL );
	pOptionalComboBox->AddComboBoxEntry( "True", pKVObjective->GetBool( "optional", false ), "1", NULL );

	CComboBoxEditorParam * pAdvancedComboBox = new CComboBoxEditorParam( { this, "advanced", FLAGS_NONE }, "Advanced:" );
	pAdvancedComboBox->AddComboBoxEntry( "False", !pKVObjective->GetBool( "advanced", false ), "0", NULL );
	pAdvancedComboBox->AddComboBoxEntry( "True", pKVObjective->GetBool( "advanced", false ), "1", NULL );

	CTextEntryEditorParam *pPointsNode = new CTextEntryEditorParam( { this, "points", FLAGS_NONE }, "Points:", CFmtStr( "%d", pKVObjective->GetInt( "points", 1 ) ) );
	pPointsNode->GetTextEntry()->SetAllowNumericInputOnly( true );

	// The condition def index this objective wants to use
	ObjectiveConditionDefIndex_t nConditionsDefIndex = pKVObjective->GetInt( "conditions_def_index", INVALID_QUEST_OBJECTIVE_CONDITIONS_INDEX );
	Assert( nConditionsDefIndex != INVALID_QUEST_OBJECTIVE_CONDITIONS_INDEX );

	m_pDefIndexComboBox = new CComboBoxEditorParam( { this, "conditions_def_index", FLAGS_NONE }, "Conditions:" );
	m_pDefIndexComboBox->GetComboBox()->AddActionSignalTarget( this );
	

	m_pSelectObjectiveButton = new Button( m_pDefIndexComboBox, "selectobjective", "Pick", this, "selectobjective" );
	m_pSelectObjectiveButton->AddActionSignalTarget( this );
	

	PopulateAndSelectConditionsCombobox( nConditionsDefIndex );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveNode::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pSelectObjectiveButton->SetPos( m_pDefIndexComboBox->GetComboBox()->GetXPos() + m_pDefIndexComboBox->GetComboBox()->GetWide(), 0 );
	m_pSelectObjectiveButton->SetWide( 36 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveNode::PopulateAndSelectConditionsCombobox( int nSelectedDefIndex )
{
	// Clean out oldies
	m_pDefIndexComboBox->GetComboBox()->RemoveAll();

	// Go through all of the current data and fill out our combo box with all of the objective conditions
	const CUtlVector< IEditableDataType* >& vecEditableData = g_pQuestEditor->GetEditableData();
	FOR_EACH_VEC( vecEditableData, i )
	{
		IEditableDataType* pEditable = vecEditableData[i];
		if ( pEditable->GetType() == IEditableDataType::TYPE_OBJECTIVE_CONDITIONS && !g_pQuestEditor->IsOpenForEdit( pEditable ) )
		{
			KeyValues* pKVData = pEditable->GetLiveData();
			bool bSelected = atoi( pKVData->GetName() ) == nSelectedDefIndex;
			m_pDefIndexComboBox->AddComboBoxEntry( CFmtStr( "\t%s", pKVData->GetString( "name", "ERROR" ) ), bSelected, pKVData->GetName(), NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveNode::OnCommand( const char *command )
{
	if ( FStrEq( "selectobjective", command ) )
	{
		KeyValues* pKV = new KeyValues( "SelectQuestObjective" );
		pKV->SetPtr( "panel", this );

		PostMessage( g_pQuestEditor, pKV );
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Pump the command from the menu panel into us as a command
//-----------------------------------------------------------------------------
void CQuestObjectiveNode::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );

	ComboBox *pComboBox = dynamic_cast<ComboBox*>( pPanel );
	if ( pComboBox )
	{
		if ( pComboBox == m_pDefIndexComboBox->GetComboBox() )
		{
			KeyValues* pKVUserData = pComboBox->GetActiveItemUserData();
			OnCommand( pKVUserData->GetString( "command" ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestObjectiveNode::OnObjectiveSelected( KeyValues *data )
{
	int nDesiredDefIndex = data->GetInt( "defindex", INVALID_ITEM_DEF_INDEX );
	Assert( nDesiredDefIndex != INVALID_ITEM_DEF_INDEX );

	ComboBox* pComboBox = m_pDefIndexComboBox->GetComboBox();
	if ( pComboBox )
	{
		for( int i=0; i < pComboBox->GetItemCount(); ++i )
		{
			KeyValues* pKVUserData = pComboBox->GetItemUserData( i );
			int nDefIndex = pKVUserData->GetInt( "write", INVALID_ITEM_DEF_INDEX );

			if ( nDefIndex == nDesiredDefIndex )
			{
				pComboBox->ActivateItemByRow( i );
				return;
			}
		}
	}

	// Didn't find the matching defindex!
	Assert( false );
}

void CQuestDescriptionNode::CreateNewControl( KeyValues* pKV )
{
	CEditorObjectNode* pNode = new CEditorObjectNode( EditorObjectInitStruct{ this, pKV->GetName(), FLAGS_NONE } );
	CLocalizationEditorParam *pLocParam = new CLocalizationEditorParam( { pNode, "token", FLAGS_NONE }, "Description:", pKV->GetString( "token" ) );
	pLocParam->GetTextEntry()->SetTall( 200 );
	pLocParam->SetTall( 200 );
	pLocParam->GetTextEntry()->SetVerticalScrollbar( true );

	BaseClass::CreateNewControl( pKV );
}

void CQuestDescriptionNode::CreateNewDefaultControl()
{
	int nDefindex = GetNextAvailableKeyNumber() + 1;
	int nObjectDefIndex = atoi( GetOwningEditable()->GetLiveData()->GetName() );

	KeyValuesAD pKVDesc( CFmtStr( "%d", nDefindex ) );
	pKVDesc->SetString( "token", CFmtStr( g_skDescTokenFormat, nObjectDefIndex, nDefindex ) );
	
	CreateNewControl( pKVDesc );
}

void CQuestNameNode::CreateNewControl( KeyValues* pKV )
{
	CEditorObjectNode* pNode = new CEditorObjectNode( EditorObjectInitStruct{ this, pKV->GetName(), FLAGS_NONE } );
	new CLocalizationEditorParam( { pNode, "token", FLAGS_NONE }, "Name:", pKV->GetString( "token" ) );

	BaseClass::CreateNewControl( pKV );
}

void CQuestNameNode::CreateNewDefaultControl()
{
	int nDefindex = GetNextAvailableKeyNumber() + 1;
	int nObjectDefIndex = atoi( GetOwningEditable()->GetLiveData()->GetName() );

	KeyValuesAD pKVName( CFmtStr( "%d", nDefindex ) );
	pKVName->SetString( "token", CFmtStr( g_skNameTokenFormat, nObjectDefIndex, nDefindex ) );

	CreateNewControl( pKVName );
}


CComboBoxEditorParam* CreateWeaponComboBox( EditorObjectInitStruct init, const char* pszLabelText, item_definition_index_t defIndexSelect )
{
	CComboBoxEditorParam* pCombo = new CComboBoxEditorParam( init, pszLabelText );
	pCombo->GetComboBox()->SetEditable( true ); // Allow for typing in the combo box.  There's a lot of stuff

	CUtlDict< item_definition_index_t > weaponDict;
	const CEconItemSchema::SortedItemDefinitionMap_t& mapItemDefs = GetItemSchema()->GetSortedItemDefinitionMap();
	FOR_EACH_MAP( mapItemDefs, i )
	{
		CTFItemDefinition* pDef = (CTFItemDefinition*)mapItemDefs[i];

		int iSlot = pDef->GetDefaultLoadoutSlot();
		if ( pDef->GetEquipType() == EEquipType_t::EQUIP_TYPE_ACCOUNT )
			continue;

		if ( !( iSlot == LOADOUT_POSITION_PRIMARY
		     || iSlot == LOADOUT_POSITION_SECONDARY 
		     || iSlot == LOADOUT_POSITION_MELEE 
		     || iSlot == LOADOUT_POSITION_PDA
		     || iSlot == LOADOUT_POSITION_PDA2
		     || iSlot == LOADOUT_POSITION_BUILDING ) )
			continue;

		// sort weapon by name by adding to weaponDict
		weaponDict.Insert( pDef->GetDefinitionName(), pDef->GetDefinitionIndex() );
	}

	// add sorted weapon to the combobox
	FOR_EACH_DICT( weaponDict, i )
	{
		item_definition_index_t weaponDefIndex = weaponDict.Element( i );
		bool bSelect = defIndexSelect == weaponDefIndex;
		pCombo->AddComboBoxEntry( weaponDict.GetElementName( i ), bSelect, CFmtStr( "%d", weaponDefIndex ), NULL );
	}

	return pCombo;
}

void CRequiredItemsParam::CreateNewControl( KeyValues* pKV )
{
	CEditorObjectNode* pRoot = new CEditorObjectNode( EditorObjectInitStruct{ this, pKV->GetName(), FLAGS_NONE } );

	item_definition_index_t defIndexCurrent = (item_definition_index_t)pKV->GetInt( "loaner_defindex", INVALID_ITEM_DEF_INDEX );
	CreateWeaponComboBox( { pRoot, "loaner_defindex", FLAG_NOT_DELETABLE }, "Loaner Item:", defIndexCurrent );

	CQualifyingItemsParam* pQualifying = new CQualifyingItemsParam( EditorObjectInitStruct{ pRoot, "qualifying_items", FLAG_NOT_DELETABLE } );
	pQualifying->InitControls( pKV->FindKey( "qualifying_items" ) );

	BaseClass::CreateNewControl( pKV );
}

void CRequiredItemsParam::CreateNewDefaultControl()
{
	int nDefindex = GetNextAvailableKeyNumber() + 1;
	int nObjectDefIndex = atoi( GetOwningEditable()->GetLiveData()->GetName() );

	KeyValuesAD pKVName( CFmtStr( "%d", nDefindex ) );
	pKVName->SetString( "token", CFmtStr( g_skNameTokenFormat, nObjectDefIndex, nDefindex ) );
	CreateNewControl( pKVName );
}


void CQualifyingItemsParam::CreateNewControl( KeyValues* pKV )
{
	CEditorObjectNode* pNode = new CEditorObjectNode( EditorObjectInitStruct{ this, pKV->GetName(), FLAGS_NONE } );

	item_definition_index_t defIndexCurrent = (item_definition_index_t)pKV->GetInt( "defindex", INVALID_ITEM_DEF_INDEX );
	CreateWeaponComboBox( { pNode, "defindex", FLAGS_NONE }, "Qualifying Item:", defIndexCurrent );

	BaseClass::CreateNewControl( pKV );
}

void CQualifyingItemsParam::CreateNewDefaultControl()
{
	int nDefindex = GetNextAvailableKeyNumber() + 1;

	KeyValuesAD pKVQualifying( CFmtStr( "%d", nDefindex ) );
	pKVQualifying->SetInt( "defindex", 0 );

	CreateNewControl( pKVQualifying );
}

void CObjectiveExpandable::CreateNewControl( KeyValues* pKV )
{
	new CQuestObjectiveNode( this, pKV );
}

void CObjectiveExpandable::CreateNewDefaultControl()
{
	int nDefindex = GetNextAvailableKeyNumber() + 1;

	KeyValuesAD pKVObjective( CFmtStr( "%d", nDefindex ) );
	pKVObjective->SetInt( "description_string", 0 );

	const int nQuestDefIndex = atoi( GetOwningEditable()->GetLiveData()->GetName() );
	const int nObjDefIndex = ( ( nQuestDefIndex - g_nkFirstQuestDef ) + 1 ) * 100 + nDefindex;
	pKVObjective->SetInt( "defindex", nObjDefIndex );
	pKVObjective->SetString( "description_string", CFmtStr( g_skObjectiveDescTokenFormat, nQuestDefIndex, nDefindex ) );
	pKVObjective->SetBool( "optional", false );
	pKVObjective->SetInt( "conditions_def_index", INVALID_QUEST_OBJECTIVE_CONDITIONS_INDEX );
	pKVObjective->SetInt( "advanced", 0 );

	CreateNewControl( pKVObjective );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CQuestObjectiveRestrictionNode::CQuestObjectiveRestrictionNode( EditorObjectInitStruct init, CTFQuestCondition *pCondition )
	: BaseClass( { init.pParent, init.m_pszKeyName, init.nFlags | FLAG_HIGHLIGHT_MOUSEOVER } )
	, m_pCondition( pCondition )
	, m_pNewCondition( NULL )
{
	CreateControlsForCondition();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveRestrictionNode::CreateControlsForCondition()
{
	CComboBoxEditorParam* pTypeParam = new CComboBoxEditorParam( { this, "type", FLAGS_NONE }, "Type:" );

	CUtlVector< const char* > vecValidChildren;
	m_pCondition->GetValidTypes( vecValidChildren );
	FOR_EACH_VEC( vecValidChildren, i )
	{
		bool bSelected = FStrEq( m_pCondition->GetConditionName(), vecValidChildren[ i ] );
		pTypeParam->AddComboBoxEntry( vecValidChildren[ i ], bSelected, vecValidChildren[ i ], "changetype" );
	}

	// Parameters for this node
	KeyValuesAD KVParams( "params" );
	m_pCondition->GetRequiredParamKeys( KVParams );

	KeyValuesAD outputKey( "output" );
	m_pCondition->GetOutputKeyValues( outputKey );

	FOR_EACH_TRUE_SUBKEY( KVParams, params )
	{
		const char *pszParamEnglishName = params->GetString( "english_name", NULL );
		const char *pszParamName = params->GetName();
		const char *pszLabelText = pszParamEnglishName ? pszParamEnglishName : pszParamName;
		const char *pszAction = FStrEq( pszParamName, "event_name" ) ? "changeevent" : NULL;

		const char* pszControlType = params->GetString( "control_type", "combo_box" );	// Default to combobox

		if ( FStrEq( "text_entry", pszControlType ) )
		{
			const char *pszOutput = outputKey->GetString( pszParamName, "" );
			new CTextEntryEditorParam( { this, params->GetName(), FLAGS_NONE }, pszLabelText, pszOutput );
		}
		else if ( FStrEq( "combo_box", pszControlType ) )
		{
			CComboBoxEditorParam* pNewParam = new CComboBoxEditorParam( { this, params->GetName(), FLAGS_NONE }, pszLabelText );
	
			const char *pszOutput = outputKey->GetString( pszParamName, "" );

			pNewParam->ClearComboBoxEntries();
		
			FOR_EACH_TRUE_SUBKEY( params, pChoiceKey )
			{
				const char *pszChoiceName = pChoiceKey->GetName();
				const char *pszChoiceEnglishName = pChoiceKey->GetString( "english_name", NULL );

				bool bSelect = FStrEq( pszOutput, pszChoiceName ) || pChoiceKey == params->GetFirstTrueSubKey();

				pNewParam->AddComboBoxEntry( pszChoiceEnglishName ? pszChoiceEnglishName : pszChoiceName, bSelect, pszChoiceName, pszAction );
			}

			// Hide parameters where there's only 1 option
			pNewParam->SetFlag( FLAG_HIDDEN, pNewParam->GetComboBox()->GetItemCount() <= 1 );
		}
	}

	// Children of this node
	CUtlVector< CTFQuestCondition* > vecChildConditions;
	m_pCondition->GetChildren( vecChildConditions );
	FOR_EACH_VEC( vecChildConditions, iChild )
	{
		CTFQuestCondition* pChildCondition = vecChildConditions[ iChild ];
		new CQuestObjectiveRestrictionNode( { this, CFmtStr( "%d", iChild ), FLAGS_NONE }, pChildCondition );
	}

	CreateAddOpportunityParam();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveRestrictionNode::CreateAddOpportunityParam()
{
	int nChildCount = 0;
	FOR_EACH_VEC( GetChildren(), i )
	{
		if ( dynamic_cast< CQuestObjectiveRestrictionNode* >( GetChildren()[i] ) )
		{
			++nChildCount;
		}
	}

	bool bShowAddOpportunity = m_pCondition && nChildCount < m_pCondition->GetMaxInputCount() ;
	if ( m_pNewCondition && !bShowAddOpportunity )
	{
		delete m_pNewCondition;
		m_pNewCondition = NULL;
	}
	else if ( !m_pNewCondition && bShowAddOpportunity )
	{
		// Add empty potential node
		CUtlVector< const char* > vecValidChildren;
		m_pCondition->GetValidChildren( vecValidChildren );
		if ( vecValidChildren.Count() )
		{
			m_pNewCondition = new CNewQuestObjectiveParam( { this, NULL, FLAGS_NONE }, NULL );
			FOR_EACH_VEC( vecValidChildren, i )
			{
				m_pNewCondition->AddComboBoxEntry( vecValidChildren[ i ], false, vecValidChildren [ i ], NULL );
			}
		}
	}
}

void CQuestObjectiveRestrictionNode::RemoveNode()
{
	// tell parent to remove me and my children
	if ( m_pCondition )
	{
		CTFQuestCondition *pParentCondition = m_pCondition->GetParent();
		if ( pParentCondition )
		{
			pParentCondition->RemoveAndDeleteChild( m_pCondition );
		}
	}

	m_pCondition = NULL;

	BaseClass::RemoveNode();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveRestrictionNode::SetNewType( const char *pszType )
{
	// Delete all children
	FOR_EACH_VEC_BACK( m_vecChildren, i )
	{
		m_vecChildren[i]->MarkForDeletion();
	}

	m_pNewCondition = NULL;


	// Create new restriction
	CTFQuestCondition* pParent = m_pCondition->GetParent();
	if ( pParent && pParent->RemoveAndDeleteChild( m_pCondition ) )
	{
		m_pCondition = pParent->AddChildByName( pszType );
	}
	else
	{
		delete m_pCondition;
		m_pCondition = CreateEvaluatorByName( pszType, NULL );
	}

	CreateControlsForCondition();
	InvalidateChain();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveRestrictionNode::SetNewEvent( const char *pszEvent )
{
	// We only have to blow up everything if we're an evaluator
	if ( !m_pCondition->IsEvaluator() )
	{
		return;
	}

	// Delete all children
	FOR_EACH_VEC_BACK( m_vecChildren, i )
	{
		m_vecChildren[i]->MarkForDeletion();
	}

	m_pNewCondition = NULL;

	char szType[256];
	V_sprintf_safe( szType, "%s", m_pCondition->GetConditionName() );

	// Create new restriction
	CTFQuestCondition* pParent = m_pCondition->GetParent();
	if ( pParent && pParent->RemoveAndDeleteChild( m_pCondition ) )
	{
		m_pCondition = pParent->AddChildByName( szType );
	}
	else
	{
		delete m_pCondition;
		m_pCondition = CreateEvaluatorByName( szType, NULL );
	}

	V_sprintf_safe( m_szEventName, "%s", pszEvent );

	m_pCondition->SetEventName( m_szEventName );
	CreateControlsForCondition();
	InvalidateChain();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestObjectiveRestrictionNode::PerformLayout()
{
	BaseClass::PerformLayout();

	CreateAddOpportunityParam();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditorQuest::CEditorQuest( KeyValues *pKV, Panel* pParent, const IEditableDataType* pEditable )
	: CEditorObjectNode( EditorObjectInitStruct{ pParent, pKV->GetName(), FLAGS_NONE }  )
{
	SetOwningEditable( pEditable );

	new CTextEntryEditorParam( { this, "name", FLAG_HIDDEN }, "Item Def Name:", pKV->GetString( "name" ) );
	new CLocalizationEditorParam( { this, "item_name", FLAGS_NONE }, "Item Name:", pKV->GetString( "item_name" ) );

	CComboBoxEditorParam *pPrefabParam = new CComboBoxEditorParam( { this, "prefab", FLAGS_NONE }, "Prefab:" );
	int nChosenIndex = -1;
	for( int i=0; i < ARRAYSIZE( g_skQuestPrefabs ); ++i )
	{
		if ( FStrEq( g_skQuestPrefabs[i], pKV->GetString( "prefab" ) ) )
		{
			nChosenIndex = i;
		}

		bool bSelect = nChosenIndex == i || ( nChosenIndex == -1 && i == 0 );
		pPrefabParam->AddComboBoxEntry( g_skQuestPrefabs[i], bSelect, g_skQuestPrefabs[i], NULL );
	}

	// Get the quest keys
	KeyValues *pKVQuest = pKV->FindKey( "quest" );

	// Create the panels to hold the quest def
	CEditorObjectNode *pQuestNode = new CEditorObjectNode( EditorObjectInitStruct{ this, "quest", FLAG_HIDDEN } );

	CQuestNameNode* pNameNode = new CQuestNameNode( EditorObjectInitStruct{ pQuestNode, "names", FLAG_NOT_DELETABLE | FLAG_COLLAPSABLE } );
	pNameNode->InitControls( pKVQuest->FindKey( "names" ) );

	new CTextEntryEditorParam( { pQuestNode, "max_standard_points", FLAGS_NONE }, "Max Standard Points:", pKVQuest->GetString( "max_standard_points" ) );
	new CTextEntryEditorParam( { pQuestNode, "max_bonus_points", FLAGS_NONE }, "Max Bonus Points:", pKVQuest->GetString( "max_bonus_points" ) );
	new CTextEntryEditorParam( { pQuestNode, "reward", FLAGS_NONE }, "Reward:", pKVQuest->GetString( "reward" ) );
	new CTextEntryEditorParam( { pQuestNode, "objectives_to_roll", FLAGS_NONE }, "Objectives to roll:", pKVQuest->GetString( "objectives_to_roll" ) );
	new CTextEntryEditorParam( { pQuestNode, "mm_map", FLAGS_NONE }, "Casual MM Map:", pKVQuest->GetString( "mm_map" ) );
	CComboBoxEditorParam *pThemeParam = new CComboBoxEditorParam( { pQuestNode, "theme", FLAGS_NONE }, "Theming:" ); 
	nChosenIndex = -1;
	{
		const auto& mapThemes = GetItemSchema()->GetQuestThemes();
		FOR_EACH_MAP( mapThemes, i )
		{
			if ( FStrEq( mapThemes[i]->GetName(), pKVQuest->GetString( "theme" ) ) )
			{
				nChosenIndex = i;
			}

			bool bSelect = nChosenIndex == i || ( nChosenIndex == -1 && i == 0 );
			pThemeParam->AddComboBoxEntry( mapThemes[i]->GetName(), bSelect, mapThemes[i]->GetName(), NULL );
		}
	}

	CComboBoxEditorParam *pCorrespondingOperation = new CComboBoxEditorParam( { pQuestNode, "operation", FLAGS_NONE }, "Operation:" ); 
	nChosenIndex = -1;
	{
		const auto& mapOperations = GetItemSchema()->GetOperationDefinitions();
		FOR_EACH_MAP( mapOperations, i )
		{
			if ( FStrEq( mapOperations[i]->GetName(), pKVQuest->GetString( "operation" ) ) )
			{
				nChosenIndex = i;
			}

			bool bSelect = nChosenIndex == i || ( nChosenIndex == -1 && i == 0 );
			pCorrespondingOperation->AddComboBoxEntry( mapOperations[i]->GetName(), bSelect, mapOperations[i]->GetName(), NULL );
		}
	}

	CQuestDescriptionNode* pDescNode = new CQuestDescriptionNode( EditorObjectInitStruct{ pQuestNode, "descriptions", FLAG_NOT_DELETABLE | FLAG_COLLAPSABLE | FLAG_COLLAPSED } );
	pDescNode->InitControls( pKVQuest->FindKey( "descriptions" ) );

	CRequiredItemsParam* pRequiredExpandable = new CRequiredItemsParam( EditorObjectInitStruct{ pQuestNode, "required_items", FLAG_NOT_DELETABLE | FLAG_COLLAPSABLE | FLAG_DONT_EXPORT } );
	pRequiredExpandable->InitControls( pKVQuest->FindKey( "required_items" ) );

	CObjectiveExpandable* pObjectiveExpandable = new CObjectiveExpandable( EditorObjectInitStruct{ pQuestNode, "objectives", FLAG_NOT_DELETABLE | FLAG_COLLAPSABLE } );
	pObjectiveExpandable->InitControls( pKVQuest->FindKey( "objectives" ) );

	// Gets the scrollbar to be the right height
	pParent->InvalidateLayout();
}


CEditorQuest::~CEditorQuest()
{}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditableDataType::IEditableDataType( KeyValues* pKVData )
	: m_pKVLiveData( pKVData->MakeCopy() )
	, m_pKVSavedData( pKVData->MakeCopy() )
	, m_pCurrentObject( NULL )
	, m_bHasUnsavedChanges( false )
{}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditableDataType::~IEditableDataType()
{
	// We own the keys
	if ( m_pKVLiveData )
	{
		m_pKVLiveData->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create the panels and store a pointer to them
//-----------------------------------------------------------------------------
void IEditableDataType::CreatePanels( Panel* pParent )
{
	Assert( m_pCurrentObject == NULL );
	m_pCurrentObject = CreateEditableObject_Internal( pParent );
	m_pCurrentObject->ClearPendingChangesFlag();

	UpdateButton();
}

void IEditableDataType::DestroyPanels()
{
	if ( m_pCurrentObject )
	{
		m_pCurrentObject->MarkForDeletion();
		m_pCurrentObject = NULL;
	}

	UpdateButton();
}


//-----------------------------------------------------------------------------
// Purpose: Given a type and a name, see if we match
//-----------------------------------------------------------------------------
bool IEditableDataType::MatchesCriteria( EType type, const char* pszName ) const
{
	return GetType() == type && FStrEq( pszName, m_pKVLiveData->GetName() );
}

class CKeyValuesDumpToString : public IKeyValuesDumpContextAsText
{
public:
	// Overrides developer level to dump in DevMsg, zero to dump as Msg
	CKeyValuesDumpToString()	{}

public:

	virtual bool KvWriteValue( KeyValues *val, int nIndentLevel ) OVERRIDE
	{
		if ( !val )
		{
			return
				KvWriteIndent( nIndentLevel ) &&
				KvWriteText( "<< NULL >>\n" );
		}

		if ( !KvWriteIndent( nIndentLevel ) )
			return false;

		if ( !KvWriteText( val->GetName() ) )
			return false;

		if ( !KvWriteText( " " ) )
			return false;

		if ( !KvWriteText( val->GetString() ) )
					return false;

		return KvWriteText( "\n" );
	}

	virtual bool KvWriteText( char const *szText ) OVERRIDE
	{
		m_strDump.Append( szText );
		return true;
	}

	CUtlString m_strDump;
};

//-----------------------------------------------------------------------------
// Purpose: Saves any changes that have been made with the controls.  Does NOT write to disk.
//-----------------------------------------------------------------------------
void IEditableDataType::SaveEdits()
{
	// Get the new keys
	KeyValues* tempKV = new KeyValues( "edits" );
	tempKV->AddSubKey( m_pKVLiveData->MakeCopy() );

	WriteObjectToKeyValues( false, tempKV );

	// Update the live keys
	m_pKVLiveData->deleteThis();
	m_pKVLiveData = tempKV->GetFirstTrueSubKey()->MakeCopy();

	CKeyValuesDumpToString dumpOrig;
	CKeyValuesDumpToString dumpNew;
	
	// Dump into strings
	m_pKVLiveData->Dump( &dumpNew, 0, true );
	m_pKVSavedData->Dump( &dumpOrig, 0, true );

	// Compare the strings to check for a difference
	m_bHasUnsavedChanges = !FStrEq( dumpOrig.m_strDump, dumpNew.m_strDump );

	if ( m_bHasUnsavedChanges )
	{
		ConColorMsg( Color( 0, 173, 53, 255 ), "Old:\n%s", dumpOrig.m_strDump.String() );
		ConColorMsg( Color( 218, 128, 56, 255 ), "New:\n%s", dumpNew.m_strDump.String() );
	}

	tempKV->deleteThis();

	UpdateButton();
}

void IEditableDataType::UpdateButton()
{
	Button* pButton = g_pQuestEditor->GetButtonForEditable( this );
	if ( pButton )
	{
		IScheme *pScheme = vgui::scheme()->GetIScheme( pButton->GetScheme() );
		Color buttonBGColor = pButton->GetSchemeColor("Button.BgColor", Color(0, 0, 0, 255), pScheme);
		Color buttonFGColor = pButton->GetSchemeColor("Button.FGColor", Color(0, 0, 0, 255), pScheme);

		if ( m_pCurrentObject )
		{
			buttonBGColor[0] = Min( buttonBGColor[0] + 70, 255 );
			buttonBGColor[1] = Min( buttonBGColor[1] + 70, 255 );
			buttonBGColor[2] = Min( buttonBGColor[2] + 70, 255 );
		}

		if ( m_bHasUnsavedChanges )
		{
			buttonFGColor[0] = Min( buttonFGColor[0] + 150, 255 );	// Lil more red
		}

		pButton->SetDefaultColor( buttonFGColor, buttonBGColor );
		pButton->SetArmedColor( buttonFGColor, pButton->GetButtonArmedBgColor() );

		if ( GetType() == IEditableDataType::TYPE_QUEST )
		{
			char szItemName[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( m_pKVLiveData->GetString( "item_name" ) ), szItemName, sizeof( szItemName ) );
			pButton->SetText( CFmtStr( "%s: %s", m_pKVLiveData->GetName(), szItemName ) );
		}
		else
		{
			pButton->SetText( CFmtStr( "%s: %s", m_pKVLiveData->GetName(), m_pKVLiveData->GetString( "name" ) ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Serialize our object and save changes to disk
//-----------------------------------------------------------------------------
void IEditableDataType::SaveChangesToDisk()
{
	SaveEdits();

	WriteDataToDisk( false );

	// Clean out old save data and copy the new keys
	if ( m_pKVSavedData )
	{
		m_pKVSavedData->deleteThis();
	}

	m_pKVSavedData = m_pKVLiveData->MakeCopy();

	UpdateButton();
}

//-----------------------------------------------------------------------------
// Purpose: Remove our entry from any file we exist in.  Remove our panels.
//			Delete our button that activates us.
//-----------------------------------------------------------------------------
void IEditableDataType::DeleteEntry()
{
	WriteDataToDisk( true );

	if ( m_pCurrentObject )
	{
		m_pCurrentObject->SetFlag( FLAG_DELETED, true );
		WriteLocalizationData();
	}

	UpdateButton();
}

void IEditableDataType::RevertChanges()
{
	Assert( m_pCurrentObject );
	if ( !m_pCurrentObject )
		return;

	// Delete old panels
	Panel* pCurrentParent = m_pCurrentObject->GetParent();
	m_pCurrentObject->MarkForDeletion();
	m_pCurrentObject = NULL;

	// Delete live keys, and copy over the saved ones to live
	m_pKVLiveData->deleteThis();
	m_pKVLiveData = m_pKVSavedData->MakeCopy();

	// Recreate panels
	CreatePanels( pCurrentParent );
	CheckForChanges();
}

void IEditableDataType::CheckForChanges()
{
	if ( m_pCurrentObject )
	{
		m_bHasUnsavedChanges = m_pCurrentObject->HasChanges( true );
		UpdateButton();
	}
}

void IEditableDataType::WriteObjectToKeyValues( bool bDelete, KeyValues* pKVExistingFileData )
{
	KeyValuesAD tempKV( "temp" );
	m_pCurrentObject->SerializeToKVs( tempKV, this );

	FOR_EACH_SUBKEY( tempKV, pNewObjectKey )
	{
		KeyValues *pCurrentObjectKey = pKVExistingFileData->FindKey( pNewObjectKey->GetName() );
		if ( pCurrentObjectKey )
		{
			if ( bDelete )
			{
				pCurrentObjectKey->deleteThis();
			}
			else
			{
				// copy all elems
				pCurrentObjectKey->Clear();
				FOR_EACH_SUBKEY( pNewObjectKey, pElem )
				{
					pCurrentObjectKey->AddSubKey( pElem->MakeCopy() );
				}
			}
		}
		else
		{
			pKVExistingFileData->AddSubKey( pNewObjectKey->MakeCopy() );
		}
	}
}


int Sort_DefIndex( KeyValues* const* p1, KeyValues* const* p2 )
{
	int n1 = atoi( (*p1)->GetName() );
	int n2 = atoi( (*p2)->GetName() );

	return n1 - n2;
}

//-----------------------------------------------------------------------------
// Purpose: Write quest data to "tf/scripts/items/unencrypted/_items_quests.txt"
//-----------------------------------------------------------------------------
void CEditableQuestDataType::WriteDataToDisk( bool bDelete )
{
	// Wrap all of the quest in a "quests" keyvalue
	CUtlBuffer bufRawData;
	bufRawData.SetBufferType( true, true );
	bufRawData.PutString( CFmtStr( "\"%s\"\n{\n", "quests") );
	
	bool bReadFileOK = g_pFullFileSystem->ReadFile( g_skQuestDefFile, NULL, bufRawData );
	if ( !bReadFileOK )
	{
		AssertMsg1( false, "Couldn't load file %s for saving!", g_skQuestDefFile );
		return;
	}

	bufRawData.PutString( "\n}" );
	
	// Load the buffer into keyvalues
	KeyValuesAD pKVQuestItemData( "quests" );
	pKVQuestItemData->LoadFromBuffer( NULL, bufRawData );

	WriteObjectToKeyValues( bDelete, pKVQuestItemData );

	// Sort by defindex
	CUtlVector< KeyValues* > vecQuests;
	FOR_EACH_SUBKEY( pKVQuestItemData, pItemKey )
	{
		vecQuests.AddToTail( pItemKey );
	}
	vecQuests.Sort( &Sort_DefIndex );


	// Write housekeeping CSV file
	FileHandle_t fileHandle = g_pFullFileSystem->Open( g_skQuestObjectivesHouseKeepingFile, "wt" );

	// Write them out in order
	CUtlBuffer buffer;
	FOR_EACH_VEC( vecQuests, i )
	{
		// Write a row as we go through each quest item
		KeyValues* pKVQuestBlock = vecQuests[i]->FindKey( "quest" );
		if ( pKVQuestBlock )
		{
			KeyValues* pKVObjectives = pKVQuestBlock->FindKey( "objectives" );
			if ( pKVObjectives )
			{
				FOR_EACH_TRUE_SUBKEY( pKVObjectives, pKVEntry )
				{	
					char szDesc[256];
					g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( pKVEntry->GetString( "description_string" ) ), szDesc, sizeof( szDesc ) );
					g_pFullFileSystem->FPrintf( fileHandle, "%d,%d,%d,%s,\n", pKVEntry->GetInt( "DefIndex" ), pKVEntry->GetInt( "points" ), pKVEntry->GetInt( "advanced" ), szDesc );
				}
			}
		}

		vecQuests[i]->RecursiveSaveToFile( buffer, 0, false, true );
	}

	g_pFullFileSystem->Close( fileHandle );

	char szCorrectCaseFilePath[MAX_PATH];
	g_pFullFileSystem->GetCaseCorrectFullPath( g_skQuestDefFile, szCorrectCaseFilePath );
	CP4AutoEditFile a( szCorrectCaseFilePath );

	if ( !g_pFullFileSystem->WriteFile( szCorrectCaseFilePath, NULL, buffer ) )
	{
		Warning( "Failed to write data to %s", g_skQuestDefFile );
	}
}

IEditorObject* CEditableQuestDataType::CreateEditableObject_Internal( Panel* pParent ) const
{
	CEditorQuest* pNewQuest = new CEditorQuest( m_pKVLiveData, pParent, this );
	return pNewQuest;
}

//-----------------------------------------------------------------------------
// Purpose: Write quest data to "tf/scripts/items/unencrypted/_items_quest_objective_definitions.txt"
//-----------------------------------------------------------------------------
void CEditableObjectiveConditionDataType::WriteDataToDisk( bool bDelete )
{
	// Wrap all of the quest in a "quests" keyvalue
	CUtlBuffer bufRawData;
	bufRawData.SetBufferType( true, true );

	bool bReadFileOK = g_pFullFileSystem->ReadFile( g_skQuestObjectivesConditionsDefFile, NULL, bufRawData );
	if ( !bReadFileOK )
	{
		AssertMsg1( false, "Couldn't load file %s for saving!", g_skQuestObjectivesConditionsDefFile );
		return;
	}

	// Load the buffer into keyvalues
	KeyValuesAD pKVQuestObjectivesData( "quest_objective_conditions" );
	pKVQuestObjectivesData->LoadFromBuffer( NULL, bufRawData );

	WriteObjectToKeyValues( bDelete, pKVQuestObjectivesData );

	// Sort by defindex
	CUtlVector< KeyValues* > vecQuestObjectives;
	FOR_EACH_SUBKEY( pKVQuestObjectivesData, pItemKey )
	{
		vecQuestObjectives.AddToTail( pItemKey );
	}
	vecQuestObjectives.Sort( &Sort_DefIndex );

	KeyValuesAD pKVSorted( "quest_objective_conditions" );

	// Write them out in order
	FOR_EACH_VEC( vecQuestObjectives, i )
	{
		pKVSorted->AddSubKey( vecQuestObjectives[i]->MakeCopy() );
	}

	// Write the definitions
	{
		CUtlBuffer buffer;
		pKVSorted->RecursiveSaveToFile( buffer, 0, false, true );

		char szCorrectCaseFilePath[MAX_PATH];
		g_pFullFileSystem->GetCaseCorrectFullPath( g_skQuestObjectivesConditionsDefFile, szCorrectCaseFilePath );
		CP4AutoEditFile a( szCorrectCaseFilePath );

		if ( !g_pFullFileSystem->WriteFile( szCorrectCaseFilePath, NULL, buffer ) )
		{
			Warning( "Failed to write data to %s", g_skQuestObjectivesConditionsDefFile );
		}
	}
}

IEditorObject* CEditableObjectiveConditionDataType::CreateEditableObject_Internal( Panel* pParent ) const
{
	CEditorObjectNode *pConditionDef = new CEditorObjectNode( EditorObjectInitStruct{ pParent, m_pKVLiveData->GetName(), FLAG_HIDDEN } );
	pConditionDef->SetOwningEditable( this );

	new CTextEntryEditorParam( { pConditionDef, "name", FLAGS_NONE }, "Conditions name:", m_pKVLiveData->GetString( "name" ) );

	CRequiredItemsParam* pRequiredExpandable = new CRequiredItemsParam( EditorObjectInitStruct{ pConditionDef, "required_items", FLAG_NOT_DELETABLE | FLAG_COLLAPSABLE | FLAG_DONT_EXPORT } );
	pRequiredExpandable->InitControls( m_pKVLiveData->FindKey( "required_items" ) );

	KeyValues* pKVConditionLogic = m_pKVLiveData->FindKey( "condition_logic" );

	const char *pszType = pKVConditionLogic->GetString( "type" );
	CTFQuestCondition *pCondition = CreateEvaluatorByName( pszType, NULL );
		
	if ( !pCondition->BInitFromKV( pKVConditionLogic, NULL ) )
	{
		Assert( false );
	}

	new CQuestObjectiveRestrictionNode( { pConditionDef, "condition_logic", FLAGS_NONE }, pCondition );

	return pConditionDef;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CQuestEditorPanel::CQuestEditorPanel( Panel *pParent, const char *pszName )
	: Frame( pParent, pszName )
	, m_pCurrentOpenEdit( NULL )
	, m_eCurrentSelectionMode( SELECTION_MODE_NONE )
{
	SetTitle( "Quest Editor", true );

	memset( m_pButtonsFilterTextEntry, NULL, sizeof( m_pButtonsFilterTextEntry ) );
	m_pEditingPanel = new CExScrollingEditablePanel( this, "EditingPanel" );
	m_pButtonsContainers[ IEditableDataType::TYPE_QUEST ] = new CExScrollingEditablePanel( this, "QuestListContainer" );
	m_pButtonsContainers[ IEditableDataType::TYPE_OBJECTIVE_CONDITIONS ] = new CExScrollingEditablePanel( this, "QuestObjectiveConditionsContainer" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::Deploy()
{
	PopulateExistingQuests();

	SetVisible( true );
	MakePopup();
	MoveToFront();
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	InvalidateLayout( true, true );

	// Center it, keeping requested size
	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	GetSize(wide, tall);
	SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/econ/QuestEditor.res" );

	m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_QUEST ] = FindControl< TextEntry >( "QuestsFilter" );
	if ( m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_QUEST ] )
	{
		m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_QUEST ]->AddActionSignalTarget( this );
	}
	m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_OBJECTIVE_CONDITIONS ] = FindControl< TextEntry >( "ConditionsFilter" );
	if ( m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_OBJECTIVE_CONDITIONS ] )
	{
		m_pButtonsFilterTextEntry[ IEditableDataType::TYPE_OBJECTIVE_CONDITIONS ]->AddActionSignalTarget( this );
	}

	Label *pP4Warning = FindControl< Label >( "P4Warning" );
	if ( pP4Warning )
	{
		pP4Warning->SetVisible( p4 == NULL );
	}

	Button* pSaveButton = FindControl< Button >( "SaveButton" );
	if ( pSaveButton )
	{
		pSaveButton->SetEnabled( p4 != NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	for ( int nType = 0; nType < IEditableDataType::NUM_TYPES; ++nType )
	{
		UpdateButtons( (IEditableDataType::EType)nType );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::UpdateButtons( IEditableDataType::EType eType )
{
	m_pButtonsContainers[ eType ]->ResetScrollAmount();

	// Get the text out of the text entry
	char szFilterText[256];
	m_pButtonsFilterTextEntry[ eType ]->GetText( szFilterText, sizeof( szFilterText ) );
	Q_strlower( szFilterText );

	int yPos = 5;
	FOR_EACH_VEC( m_vecEditableButtons[ eType ], i )
	{
		Button* pButton = m_vecEditableButtons[ eType ][ i ];

		// Get the text out of the button
		char szButtonText[256];
		pButton->GetText( szButtonText, sizeof( szButtonText ) );
		Q_strlower( szButtonText );

		// Substring search for the text entry text in the button text
		if ( V_strstr( szButtonText, szFilterText ) == NULL )
		{
			pButton->SetVisible( false );
			continue;
		}

		pButton->SetVisible( true );

		float flWideScale = 1.f;
		pButton->SetWide( pButton->GetParent()->GetWide() * flWideScale );
		pButton->SetTall( 20 );
		pButton->SetPos( 0, yPos );
		yPos += pButton->GetTall() + 5 ;
	}

	m_pButtonsContainers[ eType ]->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::OnCommand( const char *command )
{
	if ( Q_strnicmp( "select", command, 6 ) == 0 )
	{	
		// comes in the form "select(type) (name)"
		const char* pszType = command + 6;
		IEditableDataType::EType eType = (IEditableDataType::EType)atoi( pszType );
		// Jump past the first ' ' and grab whatever is left as the name
		const char* pszName = strchr( pszType, ' ' ) + 1;

		if ( m_eCurrentSelectionMode == SELECTION_MODE_QUEST_OBJECTIVE && eType == IEditableDataType::TYPE_OBJECTIVE_CONDITIONS )
		{
			m_eCurrentSelectionMode = SELECTION_MODE_NONE;

			Panel* pSelectionpanel = m_hSelectionPanel.Get();
			if ( pSelectionpanel )
			{
				PostMessage( pSelectionpanel, new KeyValues( "ObjectiveSelected", "defindex", pszName ) );
			}
		}
		else
		{
			if ( m_pCurrentOpenEdit )
			{
				// This the one that's already open?  Don't do anything
				if ( FStrEq( pszName, m_pCurrentOpenEdit->GetLiveData()->GetName() ) )
				{
					return;
				}

				// We have something open.  Close it down, saving live edits
				CloseEdit( m_pCurrentOpenEdit );
				m_pCurrentOpenEdit = NULL;
			}

			m_pEditingPanel->ResetScrollAmount();
			m_pCurrentOpenEdit = OpenForEdit( eType, pszName, m_pEditingPanel );
			m_pEditingPanel->InvalidateLayout();
		}

		return;
	}
	else if ( Q_stricmp( "revert", command ) == 0 )
	{
		if ( m_pCurrentOpenEdit )
		{
			m_pCurrentOpenEdit->RevertChanges();
		}

		ResetQuestSelectionState();
	}
	else if ( Q_stricmp( "save", command ) == 0 )
	{
		WriteLocalizationData();

		FOR_EACH_VEC( m_vecOpenEdits, i )
		{
			m_vecOpenEdits[ i ]->SaveChangesToDisk();
		}

		ResetQuestSelectionState();

		return;
	}
	else if ( Q_stricmp( "newquest", command ) == 0 )
	{
		CloseEdit( m_pCurrentOpenEdit );
		m_pEditingPanel->ResetScrollAmount();
		IEditableDataType* pNewEditable = CreateNewQuest();
		m_pCurrentOpenEdit = OpenForEdit( IEditableDataType::TYPE_QUEST, CFmtStr( "%s", pNewEditable->GetLiveData()->GetName() ), m_pEditingPanel );
		ResetQuestSelectionState();
	}
	else if ( Q_stricmp( "newobjcond", command ) == 0 )
	{
		CloseEdit( m_pCurrentOpenEdit );
		m_pEditingPanel->ResetScrollAmount();
		IEditableDataType* pNewEditable = CreateNewObjectiveCondition();
		m_pCurrentOpenEdit = OpenForEdit( IEditableDataType::TYPE_OBJECTIVE_CONDITIONS, CFmtStr( "%s", pNewEditable->GetLiveData()->GetName() ), m_pEditingPanel );
		ResetQuestSelectionState();
	}
	else if ( Q_stricmp( "delete", command ) == 0 )
	{
		if ( m_pCurrentOpenEdit )
		{
			m_pCurrentOpenEdit->DeleteEntry();
			CloseEdit( m_pCurrentOpenEdit );

			Button* pButton = GetButtonForEditable( m_pCurrentOpenEdit );
			Assert( pButton );
			// Delete the button associated with this editable
			if( pButton )
			{
				if ( !m_vecEditableButtons[ m_pCurrentOpenEdit->GetType() ].FindAndRemove( pButton ) )
				{
					AssertMsg( false, "Could not find button to remove when deleting editable!" );
				}
				pButton->MarkForDeletion();
				InvalidateLayout();
			}

			// Remove this editable from the list
			if ( !m_vecEditableData.FindAndRemove( m_pCurrentOpenEdit ) )
			{
				AssertMsg( false, "Could not find editable to remove when deleting editable!" );
			}

			delete m_pCurrentOpenEdit;
			m_pCurrentOpenEdit = NULL;
		}
		ResetQuestSelectionState();
	}
	else if ( Q_stricmp( "open_edit_context", command ) == 0 )
	{
		OpenEditContextMenu();
		ResetQuestSelectionState();
	}

	BaseClass::OnCommand( command );
}

void CQuestEditorPanel::OnThink()
{
	FOR_EACH_VEC( m_vecOpenEdits, i )
	{
		engine->Con_NPrintf( i, CFmtStr( "%s %s", m_vecOpenEdits[i]->GetType() == IEditableDataType::TYPE_OBJECTIVE_CONDITIONS ? "Condition" : "Quest", m_vecOpenEdits[i]->GetLiveData()->GetName() ) );
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Try to find the right editable to open
//-----------------------------------------------------------------------------
IEditableDataType* CQuestEditorPanel::OpenForEdit( IEditableDataType::EType type, const char* pszName, Panel* pParent )
{
	FOR_EACH_VEC( m_vecEditableData, i )
	{
		if ( m_vecEditableData[i]->MatchesCriteria( type, pszName ) )
		{
			m_vecEditableData[i]->CreatePanels( pParent );

			FOR_EACH_VEC( m_vecOpenEdits, j )
			{
				if ( m_vecOpenEdits[j] == m_vecEditableData[i] )
				{
					AssertMsg1( false, "Editable with name %s was already open for edit!", m_vecEditableData[i]->GetLiveData()->GetName() );
					m_vecOpenEdits.Remove( j );
				}
			}

			m_vecOpenEdits.AddToTail( m_vecEditableData[i] );
			return m_vecEditableData[i];
		}
	}

	Assert( !"Failed to find editable for edit!" );
	return NULL;
}

void CQuestEditorPanel::CloseEdit( IEditableDataType* pEditable )
{
	if ( !pEditable )
		return;

	FOR_EACH_VEC( m_vecOpenEdits, i )
	{
		if ( m_vecOpenEdits[i] == pEditable )
		{
			m_vecOpenEdits.Remove( i );
			pEditable->SaveEdits();
			pEditable->DestroyPanels();
			return;
		}
	}

	AssertMsg1( false, "Editable with name %s wasn't open for edit!", pEditable->GetLiveData()->GetName() );
}

bool CQuestEditorPanel::IsOpenForEdit( const IEditableDataType* pEditable ) const
{
	Assert( pEditable );
	if ( !pEditable )
		return false;

	FOR_EACH_VEC( m_vecOpenEdits, i )
	{
		if ( m_vecOpenEdits[i] == pEditable )
		{
			return true;
		}
	}

	return false;
}

Button* CQuestEditorPanel::GetButtonForEditable( const IEditableDataType* pEditable ) const
{
	IEditableDataType::EType eType = pEditable->GetType();

	auto& vecButtons = m_vecEditableButtons[ eType ];
	FOR_EACH_VEC( vecButtons, i )
	{
		if ( vecButtons[i] == pEditable->GetButton() )
		{
			return vecButtons[i];
		}
	}

	return NULL;
}

template< typename T >
IEditableDataType* CQuestEditorPanel::AddNewEditableKVData( KeyValues* pKVData, const char* pszName )
{
	IEditableDataType* pNewEditable = new T( pKVData );
	IEditableDataType::EType eType = pNewEditable->GetType();
	m_vecEditableData.AddToTail( pNewEditable );

	Button *pButton = new Button( m_pButtonsContainers[ eType ]
								, "objectivebutton"
								, CFmtStr( "%s: %s", pKVData->GetName(), pszName )
								, this
								, CFmtStr( "select%d %s", eType, pKVData->GetName() ) );

	pNewEditable->SetButton( pButton );
	m_vecEditableButtons[ eType ].AddToTail( pButton );

	InvalidateLayout();

	return pNewEditable;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CQuestEditorPanel::PopulateExistingQuests()
{
	m_vecEditableData.PurgeAndDeleteElements();

	// Quest data
	{
		// Quests are a little strange.  In the quest file, there is no root key because quests
		// are items and are spliced right into the middle of all the other items.  To handle this
		// We add "quests"
		//		  {
		//				<< all the stuff from the file >>
		//		  }
		// To make parsing easier
		CUtlBuffer bufRawData;
		bufRawData.SetBufferType( true, true );
		bufRawData.PutString( "\"quests\"\n{\n" );
		bool bReadFileOK = g_pFullFileSystem->ReadFile( g_skQuestDefFile, NULL, bufRawData );
		if ( !bReadFileOK )
		{
			AssertMsg1( false, "Couldn't load quest file %s for saving!", g_skQuestDefFile );
			return;
		}
		bufRawData.PutString( "\n}" );

		// Load quest data from file
		KeyValuesAD pKVQuestDefinitions( "quests" );
		pKVQuestDefinitions->LoadFromBuffer( NULL, bufRawData );

		// Create editable objects and buttons for each
		FOR_EACH_TRUE_SUBKEY( pKVQuestDefinitions, pKVQuest )
		{
			char szItemName[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( pKVQuest->GetString( "item_name" ) ), szItemName, sizeof( szItemName ) );

			AddNewEditableKVData< CEditableQuestDataType >( pKVQuest, szItemName );
		}
	}

	// Objective data
	{
		CUtlBuffer bufRawData;
		bufRawData.SetBufferType( true, true );

		// Now go read the objective conditions file
		bool bReadFileOK = g_pFullFileSystem->ReadFile( g_skQuestObjectivesConditionsDefFile, NULL, bufRawData );
		if ( !bReadFileOK )
		{
			AssertMsg1( false, "Couldn't load quest file %s for saving!", g_skQuestObjectivesConditionsDefFile );
			return;
		}

		// Load objective data from file
		KeyValuesAD pKVObjectiveConditions( "objectives" );
		pKVObjectiveConditions->LoadFromBuffer( NULL, bufRawData );

		// Create editable objects and buttons for each
		FOR_EACH_TRUE_SUBKEY( pKVObjectiveConditions, pKVCondition )
		{
			AddNewEditableKVData< CEditableObjectiveConditionDataType >( pKVCondition, pKVCondition->GetString( "name" ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a new quest with the next available itemdefindex
//-----------------------------------------------------------------------------
IEditableDataType* CQuestEditorPanel::CreateNewQuest()
{
	// Find the next item def to use
	int nNextQuestDefIndex = 0;
	FOR_EACH_VEC( m_vecEditableData, i )
	{
		IEditableDataType* pData = m_vecEditableData[ i ];
		if ( pData->GetType() == IEditableDataType::TYPE_QUEST )
		{
			nNextQuestDefIndex = Max( nNextQuestDefIndex, atoi( pData->GetLiveData()->GetName() ) );
		}
	}

	++nNextQuestDefIndex;
	
	// Create keyvalues to describe a default quest
	KeyValuesAD pKVNewQuest( CFmtStr( "%d", nNextQuestDefIndex ) );
	pKVNewQuest->SetString( "name", CFmtStr( g_skItemNameFormat, nNextQuestDefIndex ) );
	pKVNewQuest->SetString( "item_name", CFmtStr( g_skNameTokenFormat, nNextQuestDefIndex ) );
	pKVNewQuest->SetString( "prefab", g_skQuestPrefabs[0] );

	// Tool keys
	KeyValues *pKeyQuestBlock = pKVNewQuest->CreateNewKey();	pKeyQuestBlock->SetName( "quest" );
	pKeyQuestBlock->SetInt( "max_standard_points", 100 );
	pKeyQuestBlock->SetInt( "max_bonus_points", 30 );
	pKeyQuestBlock->SetInt( "objectives_to_roll", 1 );
	pKeyQuestBlock->SetString( "reward", "contract_lootlist_1" );
	pKeyQuestBlock->SetString( "theme", GetItemSchema()->GetQuestThemes()[0]->GetName() );
	KeyValues *pKeyObjectiveBlock = pKeyQuestBlock->CreateNewKey(); pKeyObjectiveBlock->SetName( "objectives" );

	return AddNewEditableKVData< CEditableQuestDataType >( pKVNewQuest, pKVNewQuest->GetString( "name" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new conditions with the next available defindex
//-----------------------------------------------------------------------------
IEditableDataType* CQuestEditorPanel::CreateNewObjectiveCondition()
{
	int nNextDefIndex = 0;
	FOR_EACH_VEC( m_vecEditableData, i )
	{
		IEditableDataType* pData = m_vecEditableData[ i ];
		if ( pData->GetType() == IEditableDataType::TYPE_OBJECTIVE_CONDITIONS )
		{
			nNextDefIndex = Max( nNextDefIndex, atoi( pData->GetLiveData()->GetName() ) );
		}
	}

	++nNextDefIndex;

	KeyValuesAD pKVNewObjCond( CFmtStr( "%d", nNextDefIndex ) );
	pKVNewObjCond->SetString( "name", "*** New Objective ***" );

	KeyValues* pKVConditionsBlock = pKVNewObjCond->CreateNewKey(); 
	pKVConditionsBlock->SetName( "condition_logic" );
	pKVConditionsBlock->SetString( "event_name", "player_score_changed" );
	pKVConditionsBlock->SetString( "score_key_name", "delta" );
	pKVConditionsBlock->SetString( "type", "event_listener" );

	return AddNewEditableKVData< CEditableObjectiveConditionDataType >( pKVNewObjCond, pKVNewObjCond->GetString( "name" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Open up a context menu with actions for the editing item
//-----------------------------------------------------------------------------
void CQuestEditorPanel::OpenEditContextMenu()
{
	Menu *pContextMenu = new Menu( this, "ContextMenu" );

	pContextMenu->AddMenuItem( "Save", this, new KeyValues( "save" ) );
	pContextMenu->AddMenuItem( "Revert", this, new KeyValues( "revert" ) );
	pContextMenu->AddSeparator();
	pContextMenu->AddMenuItem( "Delete", this, new KeyValues( "delete" ) );
	
	// Position to the cursor's position
	int nX, nY;
	g_pVGuiInput->GetCursorPosition( nX, nY );
	pContextMenu->SetPos( nX - 1, nY - 1 );
	
	pContextMenu->SetVisible(true);
	pContextMenu->AddActionSignalTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestEditorPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast< vgui::Panel* >( data->GetPtr( "panel" ) );

	for ( int nType = 0; nType < IEditableDataType::NUM_TYPES; ++nType )
	{
		if ( pPanel == m_pButtonsFilterTextEntry[ nType ] )
		{
			UpdateButtons( (IEditableDataType::EType)nType );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestEditorPanel::OnSelectQuestObjective( KeyValues *data )
{
	m_hSelectionPanel = reinterpret_cast< vgui::Panel* >( data->GetPtr( "panel" ) );
	m_eCurrentSelectionMode = SELECTION_MODE_QUEST_OBJECTIVE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestEditorPanel::CheckForChanges()
{
	FOR_EACH_VEC( m_vecEditableData, i )
	{
		m_vecEditableData[i]->CheckForChanges();
	}
}

#endif // STAGING_ONLY
