
#ifndef TILEGENDIALOG_H
#define TILEGENDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/PHandle.h>
#include <FileSystem.h>
#include "vgui/mousecode.h"
#include "vgui/IScheme.h"

#include "ConfigManager.h"

class CThemeDetails;
class CRoomTemplate;
class CRoomTemplateListPanel;
class CScrollingWindow;
class CMapLayoutPanel;
class CRoomTemplatePanel;
class VMFExporter;
class CMapLayout;
class CRoom;
class CEnterSeedDialog;
class CKV_Editor;
class CTileGenLayoutPage;
class CTilegenLayoutSystemPage;
class CLayoutSystem;
class CTilegenMissionPreprocessor;

namespace vgui
{
	class PanelListPanel;
	class ImagePanel;
	class MenuBar;
	class CheckButton;
	class Button;
	class Menu;
	class PropertySheet;
	class TextEntry;
};

using namespace vgui;

// determines max tile grid size
#define MAP_LAYOUT_TILES_WIDE 120

enum TileGen_FileSelectType
{
	FST_LAYOUT_OPEN,
	FST_LAYOUT_SAVE_AS
};

//-----------------------------------------------------------------------------
// Purpose: Main dialog for tile based map generator
//-----------------------------------------------------------------------------
class CTileGenDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( CTileGenDialog, Frame );

public:
	CTileGenDialog( Panel *parent, const char *name );
	virtual ~CTileGenDialog();

	virtual void PerformLayout();
	virtual void OnTick();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	
	// main menu bar
	MenuBar *m_pMenuBar;
	
	// themes and room template browser
	CThemeDetails* m_pCurrentThemeDetails;
	CScrollingWindow* m_pTemplateListContainer;
	vgui::TextEntry *m_pTemplateFilter;
	CRoomTemplateListPanel* m_pTemplateListPanel;

	// Tabbed area
	vgui::PropertySheet* m_pPropertySheet;
	CTileGenLayoutPage *m_pLayoutPage;
	CTilegenLayoutSystemPage *m_pLayoutSystemPage;

	// the current room template attached to the cursor.  if this is set, then clicking over the map will stamp down a room with that template
	void SetCursorRoomTemplate( const CRoomTemplate *pRoomTemplate );
	const CRoomTemplate *m_pCursorTemplate;		// the current room template we're stamping down
	CRoomTemplatePanel *m_pCursorPanel;		// the panel showing the current room template (attaches to mouse cursor)	

	// the currently selected placed rooms within out map layout
	void ToggleRoomSelection(CRoom* pRoom);	// adds or removes a room from the current selection
	void SetRoomSelection(CRoom* pRoom);	// sets the room selection to just the specified room
	void ClearRoomSelection();
	CUtlVector<CRoom*> m_SelectedRooms;
	void OnStartDraggingSelectedRooms();	// the user may have started dragging a selected room (we will monitor the mouse from here to see if he actually does)
	bool IsASelectedRoom( CRoom *pRoom );
	int m_iStartDragX;
	int m_iStartDragY;

	// rubber band selection
	void StartRubberBandSelection( int mx, int my );
	void EndRubberBandSelection( int mx, int my );
	bool GetRubberBandStart( int &sx, int &sy );
	int m_iStartRubberBandX;
	int m_iStartRubberBandY;

	// options
	vgui::CheckButton* m_pShowExitsCheck;
	vgui::CheckButton* m_pShowTileSquaresCheck;
	bool m_bShowExits;		// show the blue exit markers on the edges of room templates?
	bool m_bShowTileSquares;	// show the black square corners on each tile of a room template?	
	vgui::Button* m_pZoomInButton;
	vgui::Button* m_pZoomOutButton;
	vgui::Label* m_pCoordinateLabel;
	vgui::Menu* m_pToolsMenu;
	
	MESSAGE_FUNC_PARAMS( OnUpdateCurrentTheme, "UpdateCurrentTheme", params );
	MESSAGE_FUNC_PARAMS( OnAddToP4, "AddToP4", pKV );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	DECLARE_PANELMAP();	

	float RoomTemplatePanelTileSize() { return m_fTileSize; }		// size of our tiles in the UI
	int MapLayoutTilesWide() { return MAP_LAYOUT_TILES_WIDE; }

	// io
	VMFExporter* m_VMFExporter;
	void DoFileSaveAs();
	void DoFileOpen();
	void OnFileSelected(const char *fullpath);
	TileGen_FileSelectType m_FileSelectType;	// set when the file dialog is launched in save mode

	void GenerateMission( const char *szMissionFile );
	bool IsGenerating();
	CMapLayout* GetMapLayout() { return m_pMapLayout; }

	void GenerateRoomThumbnails( bool bAddToPerforce = false );

	void Zoom( int iChange );

	bool m_bFirstPerformLayout;

	CScrollingWindow* GetScrollingWindow();
	CMapLayoutPanel* GetMapLayoutPanel();
	
protected:
	virtual void OnClose();
	virtual void OnCommand( const char *command );

	// New layout system stuff
	CLayoutSystem *m_pLayoutSystem;
	int m_nShowDefaultParametersMenuItemID;
	int m_nShowAddButtonsMenuItemID;

	CMapLayout *m_pMapLayout;
	KeyValues *m_pGenerationOptions;
	char m_szGenerationOptionsFileName[MAX_PATH];
	float m_fTileSize;
};

extern CTileGenDialog *g_pTileGenDialog;

void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... );
char* TileGenCopyString( const char *szString );

#endif // TILEGENDIALOG_H
