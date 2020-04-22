//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VCDBROWSER_H
#define VCDBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mxListView.h"
#include "commctrl.h"
#include "utldict.h"
#include "faceposertoolwindow.h"
#include "filesystem.h"
#include "tier1/UtlSortVector.h"

class CVCDList;
class CUtlSymbolTree;
class CVCDOptionsWindow;
// class CChoreoEvent;

struct _IMAGELIST;
typedef struct _IMAGELIST NEAR* HIMAGELIST;

enum
{
///	IMAGE_WORKSPACE = 0,
//	IMAGE_WORKSPACE_CHECKEDOUT,
//	IMAGE_PROJECT,
//	IMAGE_PROJECT_CHECKEDOUT,
//	IMAGE_SCENE,
//	IMAGE_SCENE_CHECKEDOUT,
//	IMAGE_VCD,
//	IMAGE_VCD_CHECKEDOUT,
//	IMAGE_WAV,
//	IMAGE_WAV_CHECKEDOUT,
//	IMAGE_SPEAK,
//	IMAGE_SPEAK_CHECKEDOUT,

	VCD_NUM_IMAGES,
};

class CVCDBrowser : public mxWindow, public IFacePoserToolWindow
{
	typedef mxWindow BaseClass;
public:

	CVCDBrowser( mxWindow *parent );

	virtual		int handleEvent( mxEvent *event );
	virtual		void OnDelete();

	void		RepopulateTree();

	void		BuildSelectionList( CUtlVector< FileNameHandle_t >& selected );

	void		OnOpen();

	void		JumpToItem( const FileNameHandle_t& vcd );

	int			GetVCDCount() const;
	FileNameHandle_t GetVCD( int index );

	void		OnSearch();
	void		OnCancelSearch();

	HIMAGELIST		CreateImageList();

	void		SetCurrent( char const *fn );

private:

	class CNameLessFunc
	{
	public:
		bool Less( const FileNameHandle_t &name1, const FileNameHandle_t &name2, void *pContext );
	};


	void		OpenVCD( const FileNameHandle_t& handle );

	char const	*GetSearchString();

	bool		LoadVCDsFilesInDirectory( CUtlSortVector< FileNameHandle_t, CNameLessFunc >& soundlist, char const* pDirectoryName, int nDirectoryNameLen );
	bool		InitDirectoryRecursive( CUtlSortVector< FileNameHandle_t, CNameLessFunc >& soundlist, char const* pDirectoryName );

	void		PopulateTree( char const *subdirectory );

	void		ShowContextMenu( void );

	void		LoadAllSounds();
	void		RemoveAllSounds();

	CVCDList	*m_pListView;

	enum
	{
		NUM_BITMAPS = 6,
	};

	CUtlSortVector< FileNameHandle_t, CNameLessFunc > m_AllVCDs;
	CUtlSymbolTable			m_ScriptTable;

	CUtlVector< CUtlSymbol >	m_Scripts;

	CVCDOptionsWindow			*m_pOptions;
	CUtlSymbolTree				*m_pFileTree;

	CUtlVector< FileNameHandle_t > m_CurrentSelection;

	int				m_nPrevProcessed;
	bool			m_bTextSearch;
};

extern CVCDBrowser	*g_pVCDBrowser;

#endif // VCDBROWSER_H
