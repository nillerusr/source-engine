//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_FILESYSTEMOPENDLG_H__01CFDE04_321F_4F1E_94ED_933B2B32C193__INCLUDED_)
#define AFX_FILESYSTEMOPENDLG_H__01CFDE04_321F_4F1E_94ED_933B2B32C193__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FileSystemOpenDlg.h : header file
//

#include "utlvector.h"
#include "resource.h"
#include "filesystem.h"

/////////////////////////////////////////////////////////////////////////////
// CFileSystemOpenDlg dialog

class CWindowAnchor
{
public:
	CWnd *m_pWnd;
	int m_Side;			//0=left, 1=top, 2=right, 3=bottom
	int m_ParentSide;	//which side to anchor to parent
	int m_OriginalDist;	//original distance from the parent side
};

class CFileInfo
{
public:
	CFileInfo();
	~CFileInfo();

	bool m_bIsDir;
	CString m_Name;
	CBitmap *m_pBitmap;
};


class CFileSystemOpenDlg : public CDialog
{
friend class CFileSystemOpenDialogWrapper;

// Construction
public:
	CFileSystemOpenDlg(CreateInterfaceFn factory, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFileSystemOpenDlg)
	enum { IDD = IDD_FILESYSTEM_OPENDIALOG };
	CEdit m_FilenameLabel;
	CEdit	m_FilenameControl;
	CEdit	m_LookInLabel;
	CListCtrl	m_FileList;
	//}}AFX_DATA

	void AddFileMask( const char *pMask );
	
	void SetInitialDir( const char *pDir, const char *pPathID = NULL );

	void SetFilterMdlAndJpgFiles( bool bFilter );
	CString GetFilename() const;	// Get the filename they chose.


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFileSystemOpenDlg)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


private:

	enum GetEntriesMode_t
	{
		GETENTRIES_FILES_ONLY,
		GETENTRIES_DIRECTORIES_ONLY
	};
	void GetEntries( const char *pMask, CUtlVector<CString> &entries, GetEntriesMode_t mode );
	void PopulateListControl();
	int SetupLabelImage( CFileInfo *pInfo, CString name, bool bIsDir );

	void AddAnchor( int iDlgItem, int iSide, int anchorSide );
	void ProcessAnchor( CWindowAnchor *pAnchor );

// Implementation
protected:

	const char* GetPathID();

	friend int CALLBACK FileListSortCallback( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend class CFilenameShortcut;

	CUtlVector<CWindowAnchor> m_Anchors;

	enum
	{
		PREVIEW_IMAGE_SIZE=96
	};

	IFileSystem *m_pFileSystem;

	// These are indexed by the lParam or userdata of each item in m_FileList.
	CUtlVector<CFileInfo> m_FileInfos;

	int m_iLabel_Folder;
	int m_iLabel_Mdl;
	int m_iLabel_File;
	CBitmap m_BitmapMdl;
	CBitmap m_BitmapFile;
	CBitmap m_BitmapFolder;

	CImageList m_ImageList;
	CString m_CurrentDir;
	CString m_Filename;
	CString m_PathIDString;
	CUtlVector<CString> m_FileMasks;

	// If this is true, then we get rid of .mdl files if there is a corresponding .jpg file.
	bool m_bFilterMdlAndJpgFiles;

	// Generated message map functions
	//{{AFX_MSG(CFileSystemOpenDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDblclkFileList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpButton();
	afx_msg void OnItemchangedFileList(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILESYSTEMOPENDLG_H__01CFDE04_321F_4F1E_94ED_933B2B32C193__INCLUDED_)
