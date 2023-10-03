#ifndef TILEGEN_ROOMTEMPLATELISTPANEL_H
#define TILEGEN_ROOMTEMPLATELISTPANEL_H
#ifdef _WIN32
#pragma once
#endif

class CRoomTemplate;
class CRoomTemplatePanel;
namespace vgui
{
	class Button;
};

// arranges all room template panels to fit in rows inside this panel
class CRoomTemplateListPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CRoomTemplateListPanel, vgui::Panel );

public:

	CRoomTemplateListPanel(Panel *parent, const char *name);
	virtual ~CRoomTemplateListPanel();

	virtual void PerformLayout();

	void UpdatePanelsWithTemplate( const CRoomTemplate *pTemplate );
	void UpdateRoomList();

	MESSAGE_FUNC( OnRefreshList, "RefreshList" );
	MESSAGE_FUNC( OnExpandAll, "ExpandAllFolders" );
	MESSAGE_FUNC( OnCollapseAll, "CollapseAllFolders" );
	MESSAGE_FUNC_PARAMS( OnToggleFolder, "ToggleFolder", pKV );

	static const int m_nFilterTextLength = 256;
	void SetFilterText( const char *pText );

private:
	void AddFolder( const char *pFolderName );
	bool FilterTemplate( const CRoomTemplate *pTemplate );

	char m_FilterText[m_nFilterTextLength];

	CUtlVector< CRoomTemplatePanel * > m_Thumbnails;

	vgui::Button *m_pRefreshList;
	vgui::Button *m_pExpandAll;
	vgui::Button *m_pCollapseAll;
	
	struct RoomTemplateFolder_t
	{
		char m_FolderName[MAX_PATH];
		bool m_bExpanded;
		vgui::Button *m_pFolderButton;
		
		void SetButtonText();
	};

	static int CompareFolders( const RoomTemplateFolder_t *pFolder1, const RoomTemplateFolder_t *pFolder2 )
	{
		return Q_stricmp( pFolder1->m_FolderName, pFolder2->m_FolderName );
	}

	CUtlVector< RoomTemplateFolder_t > m_RoomTemplateFolders;
};

#endif TILEGEN_ROOMTEMPLATELISTPANEL_H