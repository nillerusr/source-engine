//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


#ifndef ITEMTEST_CONTROLS_H
#define ITEMTEST_CONTROLS_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "itemtest/itemtest.h"
#include "tier1/utlstring.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/WizardPanel.h"
#include "vgui_controls/WizardSubPanel.h"
#include "vgui_controls/FileOpenStateMachine.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class vgui::CheckButton;
class vgui::PanelListPanel;
class vgui::TextEntry;
class vgui::ComboBox;
class vgui::TextEntry;
class CDualPanelList;
class CVmtEntry;


//=============================================================================
//
//=============================================================================
class CStatusLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CStatusLabel, vgui::Label );

public:
	CStatusLabel( vgui::Panel *pPanel, const char *pszName, bool bValid = false );

	// From vgui::Label
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// New functions
	virtual void SetValid( bool bValid );
	virtual bool GetValid() const;

protected:
	bool m_bValid;
	Color m_cValid;
	Color m_cInvalid;

	void UpdateColors();

};


//=============================================================================
//
//=============================================================================
class CItemUploadSubPanel : public vgui::WizardSubPanel
{
	DECLARE_CLASS_SIMPLE( CItemUploadSubPanel, vgui::WizardSubPanel );

public:
	CItemUploadSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName );

	// From vgui::Panel
	virtual void PerformLayout();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// From vgui::WizardSubPanel
	virtual void OnDisplay();

	virtual vgui::WizardSubPanel *GetNextSubPanel();

	// Sucks data from the asset into the GUI
	virtual void UpdateGUI()
	{
		AssertMsg1( 0, "Implement UpdateGUI in base class: %s", GetPanelClassName() );
	}

	// Sets the status labels
	virtual bool UpdateStatus();

protected:
	CUtlString m_sNextName;							// The name of the next wizard sub panel

	vgui::Label *m_pLabel;							// Label at the top of the wizard sub panel
	vgui::PanelListPanel *m_pPanelListPanel;		// Standard list of widgets to set parameters
	CStatusLabel *m_pStatusLabel;					// Whether the overall panel is valid or not
	vgui::Label *m_pStatusText;						// The text for the overall status

	// New functions

	void AddStatusPanels( const char *pszPrefix );
	void SetStatus( bool bValid, const char *pszPrefix, const char *pszMessage = NULL, bool bHide = false );

};


//=============================================================================
//
//=============================================================================
class CGlobalSubPanel : public CItemUploadSubPanel
{
	DECLARE_CLASS_SIMPLE( CGlobalSubPanel, CItemUploadSubPanel );

public:
	CGlobalSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName );

	// From CItemUploadSubPanel
	virtual void UpdateGUI();
	virtual bool UpdateStatus();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

protected:
	vgui::TextEntry *m_pNameTextEntry;
	vgui::ComboBox *m_pClassComboBox;
	vgui::TextEntry *m_pSteamTextEntry;
	vgui::CheckButton *m_pAutoSkinCheckButton;

};


//=============================================================================
//
//=============================================================================
class CGeometrySubPanel : public CItemUploadSubPanel, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CGeometrySubPanel, CItemUploadSubPanel );

public:
	CGeometrySubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName );

	// From CItemUploadSubpanel
	virtual void UpdateGUI();
	virtual bool UpdateStatus();

	// From IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pszFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pszFileName, const char *pszFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pszFileName, const char *pszFileFormat, KeyValues *pContextKeyValues );

	MESSAGE_FUNC_INT( OnOpen, "Open", nLodIndex );
	MESSAGE_FUNC_INT( OnDelete, "Delete", nLodIndex );

protected:
	vgui::FileOpenStateMachine *m_pFileOpenStateMachine;

	void AddGeometry();
};


//=============================================================================
//
//=============================================================================
class CMaterialSubPanel : public CItemUploadSubPanel, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CMaterialSubPanel, CItemUploadSubPanel );

public:
	CMaterialSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName );

	// From CItemUploadSubpanel
	virtual void InvalidateLayout();

	virtual void UpdateGUI();
	virtual bool UpdateStatus();

	// From IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pszFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pszFileName, const char *pszFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pszFileName, const char *pszFileFormat, KeyValues *pContextKeyValues );

	enum Browse_t
	{
		kCommon,
		kRed,
		kBlue,
		kNormal
	};

	void Browse( CVmtEntry *pVmtEntry, Browse_t nBrowseType );

	CTargetVMT *GetTargetVMT( int nTargetVMTIndex );

protected:
	vgui::FileOpenStateMachine *m_pFileOpenStateMachine;

	void AddMaterial();

};


//=============================================================================
//
//=============================================================================
class CFinalSubPanel : public CItemUploadSubPanel
{
	DECLARE_CLASS_SIMPLE( CFinalSubPanel, CItemUploadSubPanel );

public:
	CFinalSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName );

	// From CItemUploadSubPanel
	virtual void UpdateGUI();
	virtual bool UpdateStatus();

	virtual void OnCommand( const char *command );

	virtual void PerformLayout();

	MESSAGE_FUNC( OnQuitApp, "QuitApp" );

	void OnZip();
protected:
	void OnGather();
	void OnStudioMDL();
	void OnHlmv();
	void OnExplore( bool bMaterial, bool bContent);

	bool GetHlmvCmd( CFmtStrMax &sHlmvCmd );

	vgui::Button *m_pHLMVButton;
	vgui::Label *m_pHLMVLabel;

	vgui::Button *m_pExploreMaterialContentButton;
	vgui::Label *m_pExploreMaterialContentLabel;

	vgui::Button *m_pExploreModelContentButton;
	vgui::Label *m_pExploreModelContentLabel;

	vgui::Button *m_pExploreMaterialGameButton;
	vgui::Label *m_pExploreMaterialGameLabel;

	vgui::Button *m_pExploreModelGameButton;
	vgui::Label *m_pExploreModelGameLabel;
};


//=============================================================================
//
//=============================================================================
class CItemUploadWizard : public vgui::WizardPanel
{
	DECLARE_CLASS_SIMPLE( CItemUploadWizard, vgui::WizardPanel );

public:

	CItemUploadWizard( vgui::Panel *pParent, const char *pszName );

	virtual ~CItemUploadWizard();

	void Run();

	void UpdateGUI();

	// This should be templatized but this is taking too long to write already...
	CAssetTF &Asset() { return m_asset; }

	virtual void OnFinishButton();

	CItemUploadSubPanel *GetCurrentItemUploadSubPanel()
	{
		return dynamic_cast< CItemUploadSubPanel * >( GetCurrentSubPanel() );
	}

protected:
	friend class CItemUploadSubPanel;

	CUtlVector< vgui::DHANDLE< vgui::WizardSubPanel > > m_hSubPanelList;

	CAssetTF m_asset;

	CFinalSubPanel *m_pFinalSubPanel;

};


//=============================================================================
//
//=============================================================================
class CItemUploadDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CItemUploadDialog, vgui::Frame );

public:

	CItemUploadDialog( vgui::Panel *pParent, const char *pszName );

	~CItemUploadDialog();

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
extern CItemUploadDialog *g_pItemUploadDialog;


#endif // ITEMTEST_CONTROLS_H