//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_FILEOPENDIALOG_H
#define PANORAMA_FILEOPENDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "../uievent.h"
#include "panorama/controls/button.h"

namespace panorama
{

class CDropDown;
class CLabel;
class CTextEntry;
class CFileOpenDialogEntry;

DECLARE_PANORAMA_EVENT0( FileOpenDialogOpen );
DECLARE_PANORAMA_EVENT0( FileOpenDialogCancel );
DECLARE_PANORAMA_EVENT0( FileOpenDialogClose );
DECLARE_PANORAMA_EVENT0( FileOpenDialogFolderUp );
DECLARE_PANEL_EVENT1( FileOpenDialogSortByColumn, int );
DECLARE_PANEL_EVENT1( FileOpenDialogSelectFile, uint32 );
DECLARE_PANEL_EVENT1( FileOpenDialogDoubleClickFile, uint32 );
DECLARE_PANORAMA_EVENT0( FileOpenDialogFullPathChanged );
DECLARE_PANORAMA_EVENT0( FileOpenDialogFilterChanged );
DECLARE_PANEL_EVENT1( FileOpenDialogFilesSelected, const char * );

//-----------------------------------------------------------------------------
// Purpose: generic open/save as file dialog, by default deletes itself on close
//-----------------------------------------------------------------------------
enum FileOpenDialogType_t
{
	FOD_SAVE = 0,
	FOD_OPEN,
	FOD_SELECT_DIRECTORY,
	FOD_OPEN_MULTIPLE,
};

struct FileData_t
{
	CUtlString		m_FileAttributes;
	CUtlString		m_CreationTime;
	int64			m_nCreationTime;
	CUtlString		m_LastAccessTime;
	CUtlString		m_LastWriteTime;
	int64			m_nLastWriteTime;
	int64			m_nFileSize;
	CUtlString		m_FileName;
	CUtlString		m_FullPath;
	CUtlString		m_FileType;

	bool			m_bDirectory;
};

enum FileOpenDialogSorting_t
{
	FOD_SORT_NAME = 0,
	FOD_SORT_SIZE,
	FOD_SORT_TYPE,
	FOD_SORT_DATE_MODIFIED
};

//-----------------------------------------------------------------------------
// Purpose: FileOpenDialog
//-----------------------------------------------------------------------------
class CFileOpenDialog : public CPanel2D
{
	DECLARE_PANEL2D( CFileOpenDialog, CPanel2D );

public:
	CFileOpenDialog( CPanel2D *parent, const char * pchPanelID, FileOpenDialogType_t type );
	CFileOpenDialog( panorama::IUIWindow *pParent, const char * pchPanelID, FileOpenDialogType_t type );
	virtual ~CFileOpenDialog();

	// Set the directory the file search starts in
	void SetStartDirectory(const char *dir);

	// Sets the start directory context (and resets the start directory in the process)
	// NOTE: If you specify a startdir context, then if you've already opened
	// a file with that same start dir context before, it will start in the
	// same directory it ended up in.
	void SetStartDirectoryContext( const char *pContext, const char *pDefaultDir );

	// Add filters for the drop down combo box
	// The filter info, if specified, gets sent back to the app in the FileSelected message
	void AddFilter( const char *filter, const char *filterName, bool bActive, const char *pFilterInfo = NULL );

	// Get the directory this is currently in
	void GetDirectory( char *buf, int bufSize );

	// Get the last selected file name
	void GetSelectedFileName( char *buf, int bufSize );

	/*
		messages sent:
			"FileSelected"
				"fullpath"	// specifies the fullpath of the file
				"filterinfo"	// Returns the filter info associated with the active filter
			"FileSelectionCancelled"
	*/

	static bool FileNameWildCardMatch( char const *pchFileName, char const *pchPattern );

	// event handlers
	bool EventOpen();
	bool EventCancel();
	bool EventClose();
	bool EventFolderUp();
	bool EventColumnSortingChanged( const CPanelPtr< IUIPanel > &pPanel, int nColumn );
	bool EventSelectFile( const CPanelPtr< IUIPanel > &pPanel, uint32 unModifiers );
	bool EventDoubleClickFile( const CPanelPtr< IUIPanel > &pPanel, uint32 unModifiers );
	bool EventFullPathChanged();
	bool EventFilterChanged();

protected:
	void Init();

	void PopulateFileList();
	void PopulateDriveList();

	void OnOpen();

	// TODO: needs message? hooked up to buttons/rows?
	void OnSelectFolder();
	void OnMatchStringSelected();

	// moves the directory structure up
	void MoveUpFolder();

	// validates that the current path is valid
	void ValidatePath();

private:

	// Does the specified extension match something in the filter list?
	bool ExtensionMatchesFilter( const char *pExt );

	// Choose the first non *.* filter in the filter list
	void ChooseExtension( char *pExt, int nBufLen );

	// Saves the file to the start dir context
	void SaveFileToStartDirContext( const char *pFullPath );

	// Posts a file selected message
	void PostFileSelectedMessage( const char *pFileName );

	// Posts a multiple file selected message
	void PostMultiFileSelectedMessage();

	void BuildFileList();
	void FilterFileList();
	void SortEntries();

	bool PassesFilter( FileData_t *fd );
	int  CountSubstringMatches();

	void DeselectAllEntries();

	CDropDown 		*m_pFullPathDropDown;
	CPanel2D		*m_pFileList;	// TODO: custom spreadsheet style control?
	
	CTextEntry 		*m_pFileNameTextEntry;

	CDropDown 		*m_pFileTypeCombo;
	CButton 		*m_pOpenButton;
	CButton 		*m_pCancelButton;
	CButton 		*m_pFolderUpButton;
	CLabel			*m_pFileTypeLabel;
	CUtlVector<CPanel2D*> m_vecColumnHeaders;

	KeyValues			*m_pContextKeyValues;

	FileOpenDialogSorting_t m_nSorting;
	bool m_bSortingReversed;

	char m_szLastPath[1024];
	unsigned short m_nStartDirContext;
	FileOpenDialogType_t m_DialogType;
	bool m_bFileSelected : 1;

	CUtlVector< FileData_t > m_Files;
	CUtlVector< FileData_t * > m_Filtered;

	CUtlVector< CFileOpenDialogEntry* > m_vecSelectedEntries;

	CUtlString			m_CurrentSubstringFilter;
};

//-----------------------------------------------------------------------------
// Purpose: CFileOpenDialogEntry - single row in the dialog, represents one file
//-----------------------------------------------------------------------------
class CFileOpenDialogEntry : public CButton
{
	DECLARE_PANEL2D( CFileOpenDialogEntry, CButton );

public:
	CFileOpenDialogEntry( CPanel2D *parent, const char * pchPanelID );
	virtual ~CFileOpenDialogEntry();

	void SetFileData( FileData_t *pFileData );
	const FileData_t* GetFileData() const { return &m_FileData; }

	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const panorama::MouseData_t &code ) OVERRIDE;

	bool OnScrolledIntoView( const CPanelPtr< IUIPanel > &panelPtr );

private:
	FileData_t m_FileData;

	bool m_bCreatedControls;
};

} // namespace panorama

#endif // PANORAMA_FILEOPENDIALOG_H
