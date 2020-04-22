//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI_VPROFPANEL_H
#define VGUI_VPROFPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/TreeViewListControl.h>
#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class CVProfNode;
class CVProfile;

class CProfileHierarchyPanel : public vgui::CTreeViewListControl 
{
	DECLARE_CLASS_SIMPLE( CProfileHierarchyPanel, vgui::CTreeViewListControl );

public:

	CProfileHierarchyPanel(vgui::Panel *parent, const char *panelName);
	~CProfileHierarchyPanel();

struct PanelEntry_t
	{
		PanelEntry_t() :
			label( 0 ),
			dataname( UTL_INVAL_SYMBOL )
		{
		}

		vgui::Label		*label;
		CUtlSymbol	dataname;
	};

	struct ColumnPanels_t
	{
		ColumnPanels_t();
		ColumnPanels_t( const ColumnPanels_t& src );
		void AddColumn( int index, char const *name, vgui::Label *label );
		void Refresh( KeyValues *kv );

		int							treeViewItem;
		CUtlVector< PanelEntry_t >	m_Columns;
	};

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual int AddItem( KeyValues *data, int parentItemIndex, ColumnPanels_t& columnPanels );
	virtual void ModifyItem( KeyValues *data, int itemIndex );
	virtual void SetItemColors( int id, const Color& fg, const Color& bg );
	virtual void SetItemColumnColors( int id, int col, const Color& fg, const Color& bg );

	virtual void PerformLayout();

	virtual void RemoveAll();

	virtual void	PostChildPaint();

    virtual void ExpandItem(int itemIndex, bool bExpand);
	virtual bool IsItemExpanded( int itemIndex );

	virtual KeyValues *GetItemData(int itemIndex);

public:

	void	 HideAll();

	

	static bool PanelsLessFunc( const ColumnPanels_t& lhs, const ColumnPanels_t& rhs )
	{
		return lhs.treeViewItem < rhs.treeViewItem;
	}

	CUtlRBTree< ColumnPanels_t, int >	m_Panels;

	vgui::HFont			m_itemFont;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CVProfPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CVProfPanel, vgui::Frame );

public:
	CVProfPanel( vgui::Panel *pParent, const char *pElementName );
	~CVProfPanel();

	void UpdateProfile( float filteredtime );
	
	// Command handlers
	void UserCmd_ShowVProf( void );
	void UserCmd_HideVProf( void );

	// Inherited from vgui::Frame
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Close();
	virtual void Paint();
	virtual void OnTick( void );
	virtual void OnCommand( const char *command );

	void ExpandAll( void );
	void CollapseAll( void );
	void ExpandGroup( const char *pGroupName );
	void Reset();

protected:

	virtual void PerformLayout();

private:
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

private:
	void AddColumns( CProfileHierarchyPanel::ColumnPanels_t& cp );

	void ExpandGroupRecursive( int nBudgetGroupID, CVProfNode *pNode );
	void FillTree( KeyValues *pKeyValues, CVProfNode *pNode, int parent );
	int UpdateVProfTreeEntry( KeyValues *pKeyValues, CVProfNode *pNode, int parent );

	// Populates the budget group combo box
	void PopulateBudgetGroupComboBox();

private:
	int m_fShowVprofHeld;
	CProfileHierarchyPanel *m_pHierarchy;
	int		m_RootItem;
	vgui::ComboBox *m_pVProfCategory;
	vgui::ComboBox *m_pVProfSort;
	vgui::CheckButton *m_pHierarchicalView;
	vgui::CheckButton *m_pVerbose;
	int m_nLastBudgetGroupCount;
	int m_nCurrentBudgetGroup;
	bool m_bHierarchicalView;


	vgui::Button *m_pStepForward;
	vgui::Button *m_pStepBack;
	vgui::Button *m_pGotoButton;
	vgui::Label *m_pPlaybackLabel;
	vgui::Button	*m_pRedoSort;
	vgui::ScrollBar *m_pPlaybackScroll;

	int m_iLastPlaybackTick;

	CVProfile *m_pVProfile;
};


//-----------------------------------------------------------------------------
// Global accessor
//-----------------------------------------------------------------------------
CVProfPanel *GetVProfPanel();

#endif // VGUI_VPROFPANEL_H
