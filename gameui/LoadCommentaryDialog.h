//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef LOADCOMMENTARYDIALOG_H
#define LOADCOMMENTARYDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "filesystem.h"
#include "utlvector.h"


#define COMMENTARY_MAPFILENAME_MAX_LEN MAX_PATH
#define COMMENTARY_MAPNAME_MAX_LEN 32
#define COMMENTARY_DESCRIP_MAX_LEN 512
#define MAX_LISTED_COMMENTARY_ITEMS	32

struct CommentaryItem_t
{
	char szMapFileName[COMMENTARY_MAPFILENAME_MAX_LEN];		// file location of the map
	char szMapName[COMMENTARY_MAPNAME_MAX_LEN];				// clean name of the map, eg "dod_kalt"
	char szPrintName[COMMENTARY_MAPNAME_MAX_LEN];			// printable name "Kalt"
	char szDescription[COMMENTARY_DESCRIP_MAX_LEN];			// track description "This map is fun, and people like it!"

	//int iChannel;		// TODO: multiple channels within a single map, loaded into separate commentary tracks
};

//-----------------------------------------------------------------------------
// Purpose: Base class for save & load game dialogs
//-----------------------------------------------------------------------------
class CLoadCommentaryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CLoadCommentaryDialog, vgui::Frame );

public:
	CLoadCommentaryDialog( vgui::Panel *parent );

protected:
	CUtlVector<CommentaryItem_t> m_CommentaryItems;
	vgui::PanelListPanel *m_pGameList;

	virtual void OnCommand( const char *command );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	void ScanCommentaryFiles();
	void CreateCommentaryItemList();
	int GetSelectedItemIndex();
	void AddCommentaryItemToList( int saveIndex );

	void ParseCommentaryFile( char const *pszFileName, char const *pszShortName );
	static int __cdecl SaveGameSortFunc( const void *lhs, const void *rhs );

private:
	MESSAGE_FUNC( OnPanelSelected, "PanelSelected" );
};


#endif // LOADCOMMENTARYDIALOG_H
