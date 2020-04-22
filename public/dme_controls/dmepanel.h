//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMEPANEL_H
#define DMEPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "tier0/basetypes.h"
#include "tier1/utlstringmap.h"
#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CBaseDmePanelFactory;

namespace vgui
{
	class Panel;
	class EditablePanel;
	class ComboBox;
	class IScheme;
}


//-----------------------------------------------------------------------------
// Dme Panel factory iteration handle
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( DmeFactoryHandle_t );
#define DMEFACTORY_HANDLE_INVALID ((DmeFactoryHandle_t)0)


//-----------------------------------------------------------------------------
// Dme Panel: used for editing arbitrary dme elements
//-----------------------------------------------------------------------------
class CDmePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmePanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmePanel( vgui::Panel *pParent, const char *pPanelName, bool bComboBoxVisible = true );
	virtual ~CDmePanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void SetDmeElement( CDmElement *pDmeElement, bool bForce = false, const char *pPanelName = NULL );

	// Switch to a new editor
	void SetEditor( const char *pEditorName );

	// Drag/drop
	bool IsDroppable( CUtlVector< KeyValues * >& msglist );
	void OnPanelDropped( CUtlVector< KeyValues * >& msglist );

	// Refreshes the current panel owing to external change
	// Values only means no topological change
	void Refresh( bool bValuesOnly );

	// Sets the default editor type
	void SetDefaultEditorType( const char *pEditorType );

private:
	struct EditorPanelMap_t
	{
		vgui::EditablePanel *m_pEditorPanel;
		CBaseDmePanelFactory *m_pFactory;
	};

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );	
	MESSAGE_FUNC( OnDmeElementChanged, "DmeElementChanged" );	
	MESSAGE_FUNC_PARAMS( OnViewedElementChanged, "NotifyViewedElementChanged", kv );

	// Copy/paste support
	MESSAGE_FUNC( OnCut, "OnCut" );
	MESSAGE_FUNC( OnCopy, "OnCopy" );
	MESSAGE_FUNC( OnPaste, "OnPaste" );
	MESSAGE_FUNC( OnPasteReference, "OnPasteReference" );
	MESSAGE_FUNC( OnPasteInsert, "OnPasteInsert" );
	MESSAGE_FUNC( OnEditDelete, "OnEditDelete" );

	// Context menu support
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", params );

	// Delete cached panels
	void DeleteCachedPanels();

	// Populate editor name combo box
	void PopulateEditorNames( const char *pPanelName = NULL );

	// Deactivates the current editor
	void DeactivateCurrentEditor();

	// Post message to the dme panel
	void PostMessageToDmePanel( const char *pMessage );

	static bool CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement, const char *pEditorName, EditorPanelMap_t *pMap );

	vgui::ComboBox *m_pEditorNames;

	CDmeHandle< CDmElement > m_hElement;
	CUtlStringMap< CUtlString > m_LastUsedEditorType;
	CUtlStringMap< CUtlVector< EditorPanelMap_t > > m_EditorPanelCache;
	vgui::EditablePanel *m_pDmeEditorPanel;
	CUtlString m_CurrentEditorName;
	CUtlString m_DefaultEditorType;
};


//-----------------------------------------------------------------------------
// Dme Panel factory methods
//-----------------------------------------------------------------------------
class CBaseDmePanelFactory
{
public:
	virtual vgui::EditablePanel *CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement ) = 0;
	virtual void SetDmeElement( vgui::EditablePanel *pPanel, CDmElement *pElement ) = 0;

protected:
	// Constructor, protected because these should never be instanced directly
	CBaseDmePanelFactory( const char *pElementType, const char *pEditorName, const char *pEditorDisplayName, bool bIsDefault, bool bIsOverride );

public:
	const char *m_pElementType;
	const char *m_pEditorName;
	const char *m_pEditorDisplayName;
	bool m_bIsDefault : 1;
	bool m_bIsOverride : 1;

	CBaseDmePanelFactory *m_pNext;
	static CBaseDmePanelFactory* s_pFirstDmePanelFactory;
};


template< class PanelType, class ElementType >
class CDmePanelFactory : public CBaseDmePanelFactory
{
	typedef CBaseDmePanelFactory BaseClass;

public:
	// Constructor
	CDmePanelFactory( const char *pElementType, const char *pEditorName, const char *pEditorDisplayName, bool bIsDefault, bool bIsOverride ) :
		BaseClass( pElementType, pEditorName, pEditorDisplayName, bIsDefault, bIsOverride )
	{
	}

	virtual vgui::EditablePanel *CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement )
	{
		ElementType *pTypedElement = CastElement<ElementType>( pElement );
		Assert( pTypedElement && pElement->IsA( m_pElementType ) );

		// NOTE: The panel factory assumes T contains the following method:
		// void SetDmeElement( ElementType *pElement );
		// You'll get compile errors about 'SetDmeElement' not being defined if not
		PanelType *pPanel = new PanelType( pParent, pPanelName );
		pPanel->SetDmeElement( pTypedElement );
		return pPanel;
	}

	virtual void SetDmeElement( vgui::EditablePanel *pPanel, CDmElement *pElement )
	{
		PanelType *pTypedPanel = static_cast< PanelType* >( pPanel );
		ElementType *pTypedElement = static_cast< ElementType* >( pElement );
		pTypedPanel->SetDmeElement( pTypedElement );
	}
};


template< class PanelType, class ElementType, class DisplayType >
class CDmePanelConverterFactory : public CBaseDmePanelFactory
{
	typedef CBaseDmePanelFactory BaseClass;

public:
	// Constructor
	CDmePanelConverterFactory( const char *pElementType, const char *pDisplayType, const char *pEditorName, const char *pEditorDisplayName, bool bIsDefault, bool bIsOverride ) :
		BaseClass( pElementType, pEditorName, pEditorDisplayName, bIsDefault, bIsOverride )
	{
		m_pDisplayType = pDisplayType;
	}

	virtual vgui::EditablePanel *CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement )
	{
		ElementType *pTypedElement = CastElement<ElementType>( pElement );
		Assert( pTypedElement && pElement->IsA( m_pElementType ) );

		// NOTE: To use the converter factory, the element must implement a method
		// CDmElement *GetDmePanelElement( const char *pDisplayType );
		DisplayType *pDisplayElement = CastElement<DisplayType>( pTypedElement->GetDmePanelElement( m_pDisplayType ) );

		// NOTE: The panel factory assumes T contains the following method:
		// void SetDmeElement( ElementType *pElement );
		// You'll get compile errors about 'SetDmeElement' not being defined if not
		PanelType *pPanel = new PanelType( pParent, pPanelName );
		pPanel->SetDmeElement( pDisplayElement );
		return pPanel;
	}

	virtual void SetDmeElement( vgui::EditablePanel *pPanel, CDmElement *pElement )
	{
		PanelType *pTypedPanel = static_cast< PanelType* >( pPanel );
		ElementType *pTypedElement = static_cast< ElementType* >( pElement );
		DisplayType *pDisplayElement = CastElement<DisplayType>( pTypedElement->GetDmePanelElement( m_pDisplayType ) );
		pTypedPanel->SetDmeElement( pDisplayElement );
	}

private:
	const char *m_pDisplayType;
};


//-----------------------------------------------------------------------------
// Helper macro to create the panel factory
// IMPLEMENT_DMEPANEL_FACTORY_OVERRIDE is used by applications to override
// DmePanels implemented in libraries
//-----------------------------------------------------------------------------
#define IMPLEMENT_DMEPANEL_FACTORY( _panelClassName, _dmeLookupName, _editorName, _editorDisplayName, _isDefault )	\
	CDmePanelFactory< _panelClassName, C##_dmeLookupName > g_##_panelClassName##_##_dmeLookupName##_Factory( #_dmeLookupName, _editorName, _editorDisplayName, _isDefault, false );	\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack = NULL;

#define IMPLEMENT_DMEPANEL_FACTORY_OVERRIDE( _panelClassName, _dmeLookupName, _editorName, _editorDisplayName, _isDefault )	\
	CDmePanelFactory< _panelClassName, C##_dmeLookupName > g_##_panelClassName##_##_dmeLookupName##_Factory( #_dmeLookupName, _editorName, _editorDisplayName, _isDefault, true );	\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack = NULL;

#define USING_DMEPANEL_FACTORY( _panelClassName, _dmeLookupName )\
	class _panelClassName;										\
	extern _panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack;		\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##PullInModule = g_##_panelClassName##_##_dmeLookupName##LinkerHack;


//-----------------------------------------------------------------------------
// Helper macro to create the converter panel factory 
// IMPLEMENT_DMEPANEL_CONVERSION_FACTORY_OVERRIDE is used by applications to override
// DmePanels implemented in libraries
//-----------------------------------------------------------------------------
#define IMPLEMENT_DMEPANEL_CONVERSION_FACTORY( _panelClassName, _dmeLookupName, _dmeDisplayName, _editorName, _editorDisplayName, _isDefault )	\
	CDmePanelConverterFactory< _panelClassName, C##_dmeLookupName, C##_dmeDisplayName > g_##_panelClassName##_##_dmeLookupName##_Factory( #_dmeLookupName, #_dmeDisplayName, _editorName, _editorDisplayName, _isDefault, false );	\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack = NULL;

#define IMPLEMENT_DMEPANEL_CONVERSION_FACTORY_OVERRIDE( _panelClassName, _dmeLookupName, _dmeDisplayName, _editorName, _editorDisplayName, _isDefault )	\
	CDmePanelConverterFactory< _panelClassName, C##_dmeLookupName, C##_dmeDisplayName > g_##_panelClassName##_##_dmeLookupName##_Factory( #_dmeLookupName, #_dmeDisplayName, _editorName, _editorDisplayName, _isDefault, true );	\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack = NULL;

#define USING_DMEPANEL_CONVERSION_FACTORY( _panelClassName, _dmeLookupName )	\
	class _panelClassName;											\
	extern _panelClassName *g_##_panelClassName##_##_dmeLookupName##LinkerHack;		\
	_panelClassName *g_##_panelClassName##_##_dmeLookupName##PullInModule = g_##_panelClassName##_##_dmeLookupName##LinkerHack;



//-----------------------------------------------------------------------------
// Get Dme Factories for a particular element type
//-----------------------------------------------------------------------------
DmeFactoryHandle_t DmePanelFirstFactory( CDmElement *pElement = NULL );
DmeFactoryHandle_t DmePanelNextFactory( DmeFactoryHandle_t h, CDmElement *pElement = NULL );
const char *DmePanelFactoryName( DmeFactoryHandle_t h );
const char *DmePanelFactoryDisplayName( DmeFactoryHandle_t h );
const char *DmePanelFactoryElementType( DmeFactoryHandle_t h );
bool DmePanelFactoryIsDefault( DmeFactoryHandle_t h );


//-----------------------------------------------------------------------------
// Dme Panel factory methods
//-----------------------------------------------------------------------------
vgui::EditablePanel *CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement, const char *pEditorName = NULL );


#endif // DMEPANEL_H
