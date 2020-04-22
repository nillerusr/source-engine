//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_QUEST_EDITOR_PANEL_H
#define TF_QUEST_EDITOR_PANEL_H

#include "cbase.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ComboBox.h"
#include "tf_quest_restriction.h"
#include "clientmode_tf.h"
#include "vgui_controls/ScrollableEditablePanel.h"
#include "tf_controls.h"


#ifdef STAGING_ONLY

using namespace vgui;

#define MAX_QUEST_NAME_LENGTH 256
#define MAX_QUEST_DESC_LENGTH 4096

class CEditorQuest;
class IEditorObjectParameter;
class IEditableDataType;

struct EditorObjectInitStruct
{
	Panel *pParent;
	const char* m_pszKeyName;
	int nFlags;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class IEditorObject : public EditablePanel
{
public:

	DECLARE_CLASS_SIMPLE( IEditorObject, EditablePanel );
	IEditorObject( EditorObjectInitStruct init );

	virtual int GetContentTall() const = 0;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnSizeChanged( int newWide, int newTall ) OVERRIDE;

	virtual void OnThink() OVERRIDE;

	void SetOwningEditable( const IEditableDataType* pEditable ) { m_pOwningEditable = pEditable; }
	void SetFlag( int nFlag, bool bValue ) { bValue ? m_Flags |= nFlag : m_Flags &= (~nFlag); }
	bool IsFlagSet( int nFlag, bool bCheckUpTree = false ) const;

	enum ESerializeAction
	{
		WRITE_THIS_AND_CONTINUE,
		SKIP_THIS_AND_CONTINUE,
		SKIP_THIS_AND_CHILDREN
	};

	virtual void SerializeToKVs( KeyValues* pKV, const IEditableDataType* pCallingEditable ) const = 0;
	ESerializeAction ShouldWrite( const IEditableDataType* pCallingEditable ) const;

	const char* GetKeyName() const { return m_szKeyName; }
	virtual void ClearPendingChangesFlag();

	virtual bool HasChanges( bool bCheckChildren ) const { return m_bHasChanges; }

protected:
	
	const IEditableDataType* GetOwningEditable() const;
	void InvalidateChain();

	mutable bool m_bHasChanges;
	uint32 m_Flags;

	char m_szKeyName[MAX_QUEST_DESC_LENGTH];

private:
	const IEditableDataType* m_pOwningEditable;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class IEditableDataType
{
public:

	enum EType
	{
		TYPE_QUEST = 0,
		TYPE_OBJECTIVE_CONDITIONS,
		NUM_TYPES
	};

	IEditableDataType( KeyValues* pKVData );
	virtual ~IEditableDataType();
	void CreatePanels( Panel* pParent );
	void DestroyPanels();
	bool MatchesCriteria( EType type, const char* pszName ) const;
	void SaveChangesToDisk();	// Writes all changes to disk
	void SaveEdits();			// Saves any changes that have been made with the controls.  Does NOT write to disk.
	void DeleteEntry();
	void RevertChanges();
	void SetButton( Button* pButton ) { m_hButton = pButton; }
	Panel* GetButton() const { return m_hButton.Get(); }
	void CheckForChanges();

	KeyValues* GetLiveData() const { return m_pKVLiveData; }
	virtual EType GetType() const = 0;

protected:
	virtual void WriteDataToDisk( bool bDelete ) = 0;
	void WriteObjectToKeyValues( bool bDelete, KeyValues* pKVExistingFileData );
	virtual IEditorObject* CreateEditableObject_Internal( Panel* pParent ) const = 0;
	void UpdateButton();

	IEditorObject* m_pCurrentObject;
	KeyValues* m_pKVLiveData;
	KeyValues* m_pKVSavedData;
	bool m_bHasUnsavedChanges;
	mutable PHandle m_hButton;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CEditableQuestDataType : public IEditableDataType
{
public:
	CEditableQuestDataType( KeyValues *pKV ) : IEditableDataType( pKV ) {}
private:
	virtual void WriteDataToDisk( bool bDelete ) OVERRIDE;
	virtual EType GetType() const { return IEditableDataType::TYPE_QUEST; }
	virtual IEditorObject* CreateEditableObject_Internal( Panel* pParent ) const OVERRIDE;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CEditableObjectiveConditionDataType : public IEditableDataType
{
public:
	CEditableObjectiveConditionDataType( KeyValues *pKV ) : IEditableDataType( pKV ) {}
private:
	virtual void WriteDataToDisk( bool bDelete ) OVERRIDE;
	virtual EType GetType() const { return IEditableDataType::TYPE_OBJECTIVE_CONDITIONS; }
	virtual IEditorObject* CreateEditableObject_Internal( Panel* pParent ) const OVERRIDE;
};

//-----------------------------------------------------------------------------
// Purpose: Contains children
//-----------------------------------------------------------------------------
class CEditorObjectNode : public IEditorObject
{
public:
	DECLARE_CLASS_SIMPLE( CEditorObjectNode, IEditorObject );
	CEditorObjectNode( EditorObjectInitStruct init );
	virtual ~CEditorObjectNode();

	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void MarkForDeletion() OVERRIDE;

	virtual int GetContentTall() const OVERRIDE;
	virtual void SerializeToKVs( KeyValues* pKV, const IEditableDataType* pCallingEditable ) const OVERRIDE;

	void AddChild( IEditorObject* pChild );
	void RemoveChild( IEditorObject* pChild );

	const CUtlVector< IEditorObject* >& GetChildren() const { return m_vecChildren; }
	int GetNextAvailableKeyNumber() const;

	virtual bool HasChanges( bool bCheckChildren ) const OVERRIDE;

protected:

	virtual void RemoveNode();
	virtual void ClearPendingChangesFlag() OVERRIDE;

	CUtlVector< IEditorObject* > m_vecChildren;

private:

	Button *m_pDeleteButton;
	Button *m_pToggleCollapseButton;
};

//-----------------------------------------------------------------------------
// Purpose: Contains a single parameter that can be edited
//-----------------------------------------------------------------------------
class IEditorObjectParameter : public IEditorObject
{
public:
	DECLARE_CLASS_SIMPLE( IEditorObjectParameter, IEditorObject );
	IEditorObjectParameter( EditorObjectInitStruct init, const char *pszLabelText );
	~IEditorObjectParameter();

	virtual void PerformLayout();

	virtual int GetContentTall() const = 0;
	virtual const char *GetValue() const = 0;
	virtual void SerializeToKVs( KeyValues* pKV, const IEditableDataType* pCallingEditable ) const OVERRIDE;
	void UpdateSavedValue( const char* pszNewValue ) const;

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
protected:

	mutable char m_szSavedValueBuff[MAX_QUEST_DESC_LENGTH]; // This is the value that's saved to disk

private:
	virtual bool CheckForChanges() const;

	Label		*m_pLabel;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTextEntryEditorParam : public IEditorObjectParameter
{
public:

	DECLARE_CLASS_SIMPLE( CTextEntryEditorParam, IEditorObjectParameter );
	CTextEntryEditorParam( EditorObjectInitStruct init, const char *pszLabelText, const char *pszValue );

	virtual void PerformLayout() OVERRIDE;

	virtual int GetContentTall() const OVERRIDE;
	TextEntry*  GetTextEntry() const { return m_pTextEntry; }
	void SetTextEntryValue( const char* pszValue );
	const char *GetValue() const OVERRIDE;

private:

	TextEntry *m_pTextEntry;
	mutable char m_szValueBuff[MAX_QUEST_DESC_LENGTH];
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
DECLARE_AUTO_LIST( ILocalizationEditorParamAutoList );
class CLocalizationEditorParam : public CTextEntryEditorParam, public ILocalizationEditorParamAutoList
{
public:
	DECLARE_CLASS_SIMPLE( CLocalizationEditorParam, CTextEntryEditorParam );
	CLocalizationEditorParam( EditorObjectInitStruct init, const char *pszLabelText, const char *pszLocalizationToken );

	const char *GetValue() const OVERRIDE;
	const char *GetLocalizationValue() const;

private:
	virtual bool CheckForChanges() const OVERRIDE;

	char m_szLocalizationToken[MAX_QUEST_NAME_LENGTH];
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CComboBoxEditorParam : public IEditorObjectParameter
{
public:
	DECLARE_CLASS_SIMPLE( CComboBoxEditorParam, IEditorObjectParameter );
	CComboBoxEditorParam( EditorObjectInitStruct init, const char *pszLabelText );

	virtual void PerformLayout() OVERRIDE;
	const char *GetValue() const OVERRIDE;

	virtual int GetContentTall() const OVERRIDE;
	void ClearComboBoxEntries() { m_pComboBox->RemoveAll(); }
	void AddComboBoxEntry( const char* pszText, bool bSelected, const char* pszWriteValue, const char* pszCommand );
	ComboBox* GetComboBox() const { return m_pComboBox; }

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

protected:

	ComboBox *m_pComboBox;
	mutable char m_szValueBuff[MAX_QUEST_DESC_LENGTH];
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CNewQuestObjectiveParam : public CComboBoxEditorParam
{
public:
	DECLARE_CLASS_SIMPLE( CNewQuestObjectiveParam, CComboBoxEditorParam );
	CNewQuestObjectiveParam( EditorObjectInitStruct init, const char *pszLabelText );

	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

private:
	Button *m_pAddButton;
};

class CQuestObjectiveNode : public CEditorObjectNode
{
public:
	DECLARE_CLASS_SIMPLE( CQuestObjectiveNode, CEditorObjectNode );
	CQuestObjectiveNode( CEditorObjectNode* pParentNode, KeyValues* pKVObjective );

	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_PARAMS( OnObjectiveSelected, "ObjectiveSelected", data );
private:
	void PopulateAndSelectConditionsCombobox( int nSelectedDefIndex );

	CComboBoxEditorParam* m_pDefIndexComboBox;
	Button* m_pSelectObjectiveButton;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class IOptionalExpandableBlock : public CEditorObjectNode
{
public:
	DECLARE_CLASS_SIMPLE( IOptionalExpandableBlock, CEditorObjectNode );
	IOptionalExpandableBlock( EditorObjectInitStruct init, const char* pszButtonText );

	virtual void InitControls( KeyValues* pKVBlock );

	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

protected:
	virtual void CreateNewControl( KeyValues* pKV );

private:
	virtual void CreateNewDefaultControl() = 0;
	virtual int GetMinCount() const = 0;

	Button *m_pAddButton;
	int m_nMinCount;
};


class CQuestDescriptionNode : public IOptionalExpandableBlock
{
public:
	DECLARE_CLASS_SIMPLE( CQuestDescriptionNode, IOptionalExpandableBlock );
	CQuestDescriptionNode( EditorObjectInitStruct init ) : IOptionalExpandableBlock( init, "Add new Description" ) {}

private:
	virtual void CreateNewControl( KeyValues* pKV ) OVERRIDE;
	virtual void CreateNewDefaultControl() OVERRIDE;
	virtual int GetMinCount() const OVERRIDE { return 1; }
};

class CQuestNameNode : public IOptionalExpandableBlock
{
public:
	DECLARE_CLASS_SIMPLE( CQuestNameNode, IOptionalExpandableBlock );
	CQuestNameNode( EditorObjectInitStruct init ) : IOptionalExpandableBlock( init, "Add new Name" ) {}

private:
	virtual void CreateNewControl( KeyValues* pKV ) OVERRIDE;
	virtual void CreateNewDefaultControl() OVERRIDE;
	virtual int GetMinCount() const OVERRIDE { return 1; }
};

class CRequiredItemsParam : public IOptionalExpandableBlock
{
public:
	DECLARE_CLASS_SIMPLE( CRequiredItemsParam, IOptionalExpandableBlock );
	CRequiredItemsParam( EditorObjectInitStruct init ) : IOptionalExpandableBlock( init, "Add new Loaner Item Set" ) {}

private:
	virtual void CreateNewControl( KeyValues* pKV ) OVERRIDE;
	virtual void CreateNewDefaultControl() OVERRIDE;
	virtual int GetMinCount() const OVERRIDE { return 0; }
};

class CQualifyingItemsParam : public IOptionalExpandableBlock
{
public:
	DECLARE_CLASS_SIMPLE( CQualifyingItemsParam, IOptionalExpandableBlock );
	CQualifyingItemsParam( EditorObjectInitStruct init ) : IOptionalExpandableBlock( init, "Add new Qualifying Item" ) {}

private:
	virtual void CreateNewControl( KeyValues* pKV ) OVERRIDE;
	virtual void CreateNewDefaultControl() OVERRIDE;
	virtual int GetMinCount() const OVERRIDE { return 0; }
};

class CObjectiveExpandable : public IOptionalExpandableBlock
{
public:
	DECLARE_CLASS_SIMPLE( CObjectiveExpandable, IOptionalExpandableBlock );
	CObjectiveExpandable( EditorObjectInitStruct init ) : IOptionalExpandableBlock( init, "Add new Objective" ) {}

private:
	virtual void CreateNewControl( KeyValues* pKV ) OVERRIDE;
	virtual void CreateNewDefaultControl() OVERRIDE;
	virtual int GetMinCount() const OVERRIDE { return 1; }
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CQuestObjectiveRestrictionNode : public CEditorObjectNode
{
public:
	DECLARE_CLASS_SIMPLE( CQuestObjectiveRestrictionNode, CEditorObjectNode );
	CQuestObjectiveRestrictionNode( EditorObjectInitStruct init, CTFQuestCondition *pCondition );

	virtual void PerformLayout() OVERRIDE;

	CTFQuestCondition *GetCondition() { return m_pCondition; }

	void SetNewType( const char *pszType );
	void SetNewEvent( const char *pszEvent );

private:

	virtual void RemoveNode() OVERRIDE;
	void CreateControlsForCondition();
	void CreateAddOpportunityParam();

	char m_szEventName[MAX_QUEST_NAME_LENGTH];

	CTFQuestCondition *m_pCondition;
	CNewQuestObjectiveParam *m_pNewCondition;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CEditorQuest : public CEditorObjectNode
{
public:
	CEditorQuest( KeyValues *pKV, Panel* pParent, const IEditableDataType* pEditable );
	~CEditorQuest();

private:
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CQuestEditorPanel : public Frame
{
public:
	DECLARE_CLASS_SIMPLE( CQuestEditorPanel, Frame );

	CQuestEditorPanel( Panel *pParent, const char *pszName );
	~CQuestEditorPanel() {}

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

	virtual void OnThink() OVERRIDE;

	void Deploy();

	IEditableDataType* OpenForEdit( IEditableDataType::EType type, const char* pszName, Panel* pParent );
	void CloseEdit( IEditableDataType* pEditable );
	bool IsOpenForEdit( const IEditableDataType* pEditable ) const;

	const CUtlVector< IEditableDataType* >& GetEditableData() const { return m_vecEditableData; }

	IEditableDataType* CreateNewQuest();
	IEditableDataType* CreateNewObjectiveCondition();

	Button* GetButtonForEditable( const IEditableDataType* pEditable ) const;

	void CheckForChanges();

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_PARAMS( OnSelectQuestObjective, "SelectQuestObjective", data );
private:

	void UpdateButtons( IEditableDataType::EType type );
	void ResetQuestSelectionState() { m_eCurrentSelectionMode = SELECTION_MODE_NONE; }

	template< typename T >
	IEditableDataType* AddNewEditableKVData( KeyValues* pKVConditionsData, const char* pszName );

	void PopulateExistingQuests();
	void OpenEditContextMenu();

	CExScrollingEditablePanel* m_pButtonsContainers[ IEditableDataType::NUM_TYPES ];
	CUtlVector< Button* > m_vecEditableButtons[ IEditableDataType::NUM_TYPES ];
	TextEntry* m_pButtonsFilterTextEntry[ IEditableDataType::NUM_TYPES ];

	CExScrollingEditablePanel *m_pEditingPanel;

	IEditableDataType* m_pCurrentOpenEdit;
	CUtlVector< IEditableDataType* > m_vecEditableData;
	CUtlVector< IEditableDataType* > m_vecOpenEdits;

	enum ESelectionMode_t
	{
		SELECTION_MODE_NONE = 0,
		SELECTION_MODE_QUEST_OBJECTIVE,
	};

	ESelectionMode_t m_eCurrentSelectionMode;
	PHandle m_hSelectionPanel;
};

#endif // STAGING_ONLY
#endif // TF_QUEST_EDITOR_PANEL_H
