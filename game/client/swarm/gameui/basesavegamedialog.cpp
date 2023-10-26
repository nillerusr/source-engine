//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "BaseSaveGameDialog.h"
#include "FileSystem.h"
#include "savegame_version.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/tgaimagepanel.h"
#include "tier1/utlbuffer.h"
#include "tier2/resourceprecacher.h"
#include <stdio.h>
#include <stdlib.h>
#include "FileSystem.h"

#include "MouseMessageForwardingPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define TGA_IMAGE_PANEL_WIDTH 180
#define TGA_IMAGE_PANEL_HEIGHT 100

#define MAX_LISTED_SAVE_GAMES	128

PRECACHE_REGISTER_BEGIN( GLOBAL, BaseSaveGameDialog )
PRECACHE( MATERIAL, "vgui/resource/autosave.vmt" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: Describes the layout of a same game pic
//-----------------------------------------------------------------------------
class CSaveGamePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CSaveGamePanel, vgui::EditablePanel );
public:
	CSaveGamePanel( PanelListPanel *parent, const char *name, int saveGameListItemID ) : BaseClass( parent, name )
	{
		m_iSaveGameListItemID = saveGameListItemID;
		m_pParent = parent;
		m_pSaveGameImage = new CTGAImagePanel( this, "SaveGameImage" );
		m_pAutoSaveImage = new ImagePanel( this, "AutoSaveImage" );
		m_pSaveGameScreenshotBackground = new ImagePanel( this, "SaveGameScreenshotBackground" );
		m_pChapterLabel = new Label( this, "ChapterLabel", "" );
		m_pTypeLabel = new Label( this, "TypeLabel", "" );
		m_pElapsedTimeLabel = new Label( this, "ElapsedTimeLabel", "" );
		m_pFileTimeLabel = new Label( this, "FileTimeLabel", "" );

		CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
		panel->SetZPos(2);

		SetSize( 200, 140 );

		LoadControlSettings( "resource/SaveGamePanel.res" );

		m_FillColor = m_pSaveGameScreenshotBackground->GetFillColor();
	}

	void SetSaveGameInfo( SaveGameDescription_t &save )
	{
		// set the bitmap to display
		char tga[_MAX_PATH];
		Q_strncpy( tga, save.szFileName, sizeof(tga) );
		char *ext = strstr( tga, ".sav" );
		if ( ext )
		{
			strcpy( ext, ".tga" );
		}

		// If a TGA file exists then it is a user created savegame
		if ( g_pFullFileSystem->FileExists( tga ) )
		{
			m_pSaveGameImage->SetTGAFilename( tga );
		}
		// If there is no TGA then it is either an autosave or the user TGA file has been deleted
		else
		{
			m_pSaveGameImage->SetVisible( false );
			m_pAutoSaveImage->SetVisible( true );
			m_pAutoSaveImage->SetImage( "resource\\autosave" );
		}

		// set the title text
		m_pChapterLabel->SetText( save.szComment );

		// type
		SetControlString( "TypeLabel", save.szType );
		SetControlString( "ElapsedTimeLabel", save.szElapsedTime );
		SetControlString( "FileTimeLabel", save.szFileTime );
	}

	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			// set the text color to be orange, and the pic border to be orange
			m_pSaveGameScreenshotBackground->SetFillColor( m_SelectedColor );
			m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pTypeLabel->SetFgColor( m_SelectedColor );
			m_pElapsedTimeLabel->SetFgColor( m_SelectedColor );
			m_pFileTimeLabel->SetFgColor( m_SelectedColor );
		}
		else
		{
			m_pSaveGameScreenshotBackground->SetFillColor( m_FillColor );
			m_pChapterLabel->SetFgColor( m_TextColor );
			m_pTypeLabel->SetFgColor( m_TextColor );
			m_pElapsedTimeLabel->SetFgColor( m_TextColor );
			m_pFileTimeLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		m_pParent->SetSelectedPanel( this );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		m_TextColor = pScheme->GetColor( "NewGame.TextColor", Color(255, 255, 255, 255) );
		m_SelectedColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

		BaseClass::ApplySchemeSettings( pScheme );
	}

	virtual void OnMouseDoublePressed( vgui::MouseCode code )
	{
		// call the panel
		OnMousePressed( code );
		PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "loadsave") );
	}

	int GetSaveGameListItemID()
	{
		return m_iSaveGameListItemID;
	}

private:
	vgui::PanelListPanel *m_pParent;
	vgui::Label *m_pChapterLabel;
	CTGAImagePanel *m_pSaveGameImage;
	ImagePanel *m_pAutoSaveImage;
	
	// things to change color when the selection changes
	ImagePanel *m_pSaveGameScreenshotBackground;
	Label *m_pTypeLabel;
	Label *m_pElapsedTimeLabel;
	Label *m_pFileTimeLabel;
	Color m_TextColor, m_FillColor, m_SelectedColor;

	int m_iSaveGameListItemID;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseSaveGameDialog::CBaseSaveGameDialog( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	CreateSavedGamesList();
	ScanSavedGames();

	new vgui::Button( this, "loadsave", "" );
	SetControlEnabled( "loadsave", false );
}

//-----------------------------------------------------------------------------
// Purpose: Creates the load game display list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::CreateSavedGamesList()
{
	m_pGameList = new vgui::PanelListPanel( this, "listpanel_loadgame" );
	m_pGameList->SetFirstColumnWidth( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: returns the save file name of the selected item
//-----------------------------------------------------------------------------
int CBaseSaveGameDialog::GetSelectedItemSaveIndex()
{
	CSaveGamePanel *panel = dynamic_cast<CSaveGamePanel *>(m_pGameList->GetSelectedPanel());
	if ( panel )
	{
		// find the panel in the list
		for ( int i = 0; i < m_SaveGames.Count(); i++ )
		{
			if ( i == panel->GetSaveGameListItemID() )
			{
				return i;
			}
		}
	}
	return m_SaveGames.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: builds save game list from directory
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::ScanSavedGames()
{
	// populate list box with all saved games on record:
	char	szDirectory[_MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "save/*.sav" );

	// clear the current list
	m_pGameList->DeleteAllItems();
	m_SaveGames.RemoveAll();
	
	// iterate the saved files
	FileFindHandle_t handle;
	const char *pFileName = g_pFullFileSystem->FindFirst( szDirectory, &handle );
	while (pFileName)
	{
		if ( !Q_strnicmp(pFileName, "HLSave", strlen( "HLSave" ) ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}

		char szFileName[_MAX_PATH];
		Q_snprintf(szFileName, sizeof( szFileName ), "save/%s", pFileName);

		// Only load save games from the current mod's save dir
		if( !g_pFullFileSystem->FileExists( szFileName, "MOD" ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}
		
		SaveGameDescription_t save;
		if ( ParseSaveData( szFileName, pFileName, save ) )
		{
			m_SaveGames.AddToTail( save );
		}
		
		pFileName = g_pFullFileSystem->FindNext( handle );
	}
	
	g_pFullFileSystem->FindClose( handle );

	// notify derived classes that save games are being scanned (so they can insert their own)
	OnScanningSaveGames();

	// sort the save list
	qsort( m_SaveGames.Base(), m_SaveGames.Count(), sizeof(SaveGameDescription_t), &SaveGameSortFunc );

	// add to the list
	for ( int saveIndex = 0; saveIndex < m_SaveGames.Count() && saveIndex < MAX_LISTED_SAVE_GAMES; saveIndex++ )
	{
		// add the item to the panel
		AddSaveGameItemToList( saveIndex );
	}

	// display a message if there are no save games
	if ( !m_SaveGames.Count() )
	{
		vgui::Label *pNoSavesLabel = SETUP_PANEL(new Label(m_pGameList, "NoSavesLabel", "#GameUI_NoSaveGamesToDisplay"));
		pNoSavesLabel->SetTextColorState(vgui::Label::CS_DULL);
		m_pGameList->AddItem( NULL, pNoSavesLabel );
	}

	SetControlEnabled( "loadsave", false );
	SetControlEnabled( "delete", false );
}

//-----------------------------------------------------------------------------
// Purpose: Adds an item to the list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::AddSaveGameItemToList( int saveIndex )
{
	// create the new panel and add to the list
	CSaveGamePanel *saveGamePanel = new CSaveGamePanel( m_pGameList, "SaveGamePanel", saveIndex );
	saveGamePanel->SetSaveGameInfo( m_SaveGames[saveIndex] );
	m_pGameList->AddItem( NULL, saveGamePanel );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
bool CBaseSaveGameDialog::ParseSaveData( char const *pszFileName, char const *pszShortName, SaveGameDescription_t &save )
{
	char    szMapName[SAVEGAME_MAPNAME_LEN];
	char    szComment[SAVEGAME_COMMENT_LEN];
	char    szElapsedTime[SAVEGAME_ELAPSED_LEN];

	if ( !pszFileName || !pszShortName )
		return false;

	Q_strncpy( save.szShortName, pszShortName, sizeof(save.szShortName) );
	Q_strncpy( save.szFileName, pszFileName, sizeof(save.szFileName) );

	FileHandle_t fh = g_pFullFileSystem->Open( pszFileName, "rb", "MOD" );
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return false;

	int readok = SaveReadNameAndComment( fh, szMapName, szComment );
	g_pFullFileSystem->Close(fh);

	if ( !readok )
	{
		return false;
	}

	Q_strncpy( save.szMapName, szMapName, sizeof(save.szMapName) );

	// Elapsed time is the last 6 characters in comment. (mmm:ss)
	int i;
	i = strlen( szComment );
	Q_strncpy( szElapsedTime, "??", sizeof( szElapsedTime ) );
	if (i >= 6)
	{
		Q_strncpy( szElapsedTime, (char *)&szComment[i - 6], 7 );
		szElapsedTime[6] = '\0';

		// parse out
		int minutes = atoi( szElapsedTime );
		int seconds = atoi( szElapsedTime + 4);

		// reformat
		if ( minutes )
		{
			Q_snprintf( szElapsedTime, sizeof(szElapsedTime), "%d %s %d seconds", minutes, minutes > 1 ? "minutes" : "minute", seconds );
		}
		else
		{
			Q_snprintf( szElapsedTime, sizeof(szElapsedTime), "%d seconds", seconds );
		}

		// Chop elapsed out of comment.
		int n;

		n = i - 6;
		szComment[n] = '\0';
	
		n--;

		// Strip back the spaces at the end.
		while ((n >= 1) &&
			szComment[n] &&
			szComment[n] == ' ')
		{
			szComment[n--] = '\0';
		}
	}

	// calculate the file name to print
	const char *pszType = "";
	if (strstr(pszFileName, "quick"))
	{
		pszType = "#GameUI_QuickSave";
	}
	else if (strstr(pszFileName, "autosave"))
	{
		pszType = "#GameUI_AutoSave";
	}

	Q_strncpy( save.szType, pszType, sizeof(save.szType) );
	Q_strncpy( save.szComment, szComment, sizeof(save.szComment) );
	Q_strncpy( save.szElapsedTime, szElapsedTime, sizeof(save.szElapsedTime) );

	// Now get file time stamp.
	long fileTime = g_pFullFileSystem->GetFileTime(pszFileName);
	char szFileTime[32];
	g_pFullFileSystem->FileTimeToString(szFileTime, sizeof(szFileTime), fileTime);
	char *newline = strstr(szFileTime, "\n");
	if (newline)
	{
		*newline = 0;
	}
	Q_strncpy( save.szFileTime, szFileTime, sizeof(save.szFileTime) );
	save.iTimestamp = fileTime;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: timestamp sort function for savegames
//-----------------------------------------------------------------------------
int CBaseSaveGameDialog::SaveGameSortFunc( const void *lhs, const void *rhs )
{
	const SaveGameDescription_t *s1 = (const SaveGameDescription_t *)lhs;
	const SaveGameDescription_t *s2 = (const SaveGameDescription_t *)rhs;

	if (s1->iTimestamp < s2->iTimestamp)
		return 1;
	else if (s1->iTimestamp > s2->iTimestamp)
		return -1;

	// timestamps are equal, so just sort by filename
	return strcmp(s1->szFileName, s2->szFileName);
}

#define MAKEID(d,c,b,a)	( ((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)) )

int SaveReadNameAndComment( FileHandle_t f, char *name, char *comment )
{
	int i, tag, size, tokenSize, tokenCount;
	char *pSaveData, *pFieldName, **pTokenList;

	g_pFullFileSystem->Read( &tag, sizeof(int), f );
	if ( tag != MAKEID('J','S','A','V') )
	{
		return 0;
	}
		
	g_pFullFileSystem->Read( &tag, sizeof(int), f );
	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
	{
		return 0;
	}

	name[0] = '\0';
	comment[0] = '\0';
	g_pFullFileSystem->Read( &size, sizeof(int), f );
	
	g_pFullFileSystem->Read( &tokenCount, sizeof(int), f );	// These two ints are the token list
	g_pFullFileSystem->Read( &tokenSize, sizeof(int), f );
	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > 1024*1024*32  )
	{
		return 0;
	}

	if ( tokenSize < 0 || tokenSize > 1024*1024*32  )
	{
		return 0;
	}

	pSaveData = (char *)new char[size];
	g_pFullFileSystem->Read(pSaveData, size, f);

	int nNumberOfFields;

	char *pData;
	int nFieldSize;
	
	pData = pSaveData;

	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		pTokenList = new char *[tokenCount];

		// Make sure the token strings pointed to by the pToken hashtable.
		for( i=0; i<tokenCount; i++ )
		{
			pTokenList[i] = *pData ? pData : NULL;	// Point to each string in the pToken table
			while( *pData++ );				// Find next token (after next null)
		}
	}
	else
		pTokenList = NULL;

	// short, short (size, index of field name)
	nFieldSize = *(short *)pData;
	pData += sizeof(short);
	pFieldName = pTokenList[ *(short *)pData ];

	if (stricmp(pFieldName, "GameHeader"))
	{
		delete[] pSaveData;
		return 0;
	};

	// int (fieldcount)
	pData += sizeof(short);
	nNumberOfFields = *(int*)pData;
	pData += nFieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for (i = 0; i < nNumberOfFields; i++)
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		nFieldSize = *(short *)pData;
		pData += sizeof(short);

		pFieldName = pTokenList[ *(short *)pData ];
		pData += sizeof(short);

		if (!stricmp(pFieldName, "comment"))
		{
			Q_strncpy(comment, pData, nFieldSize);
		}
		else if (!stricmp(pFieldName, "mapName"))
		{
			Q_strncpy(name, pData, nFieldSize);
		};

		// Move to Start of next field.
		pData += nFieldSize;
	};

	// Delete the string table we allocated
	delete[] pTokenList;
	delete[] pSaveData;
	
	if (strlen(name) > 0 && strlen(comment) > 0)
		return 1;
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: deletes an existing save game
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::DeleteSaveGame( const char *fileName )
{
	if ( !fileName || !fileName[0] )
		return;

	// delete the save game file
	g_pFullFileSystem->RemoveFile( fileName, "MOD" );

	// delete the associated tga
	char tga[_MAX_PATH];
	Q_strncpy( tga, fileName, sizeof(tga) );
	char *ext = strstr( tga, ".sav" );
	if ( ext )
	{
		strcpy( ext, ".tga" );
	}
	g_pFullFileSystem->RemoveFile( tga, "MOD" );
}

//-----------------------------------------------------------------------------
// Purpose: One item has been selected
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::OnPanelSelected()
{
	SetControlEnabled( "loadsave", true );
	SetControlEnabled( "delete", true );
}


