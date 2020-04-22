//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// FileSystemOpenDlg.cpp : implementation file
//

#include "stdafx.h"
#include "FileSystemOpenDlg.h"
#include "jpeglib/jpeglib.h"
#include "utldict.h"
#include "resource.h"
#include "ifilesystemopendialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CFileInfo::CFileInfo()
{
	m_pBitmap = NULL;
}


CFileInfo::~CFileInfo()
{
}


/////////////////////////////////////////////////////////////////////////////
// This caches the thumbnail bitmaps we generate to speed up browsing.
/////////////////////////////////////////////////////////////////////////////

class CBitmapCache
{
public:
	CBitmapCache()
	{
		m_CurMemoryUsage = 0;
		m_MaxMemoryUsage = 1024 * 1024 * 6;
	}

	void AddToCache( CBitmap *pBitmap, const char *pName, int memoryUsage, bool bLock )
	{
		Assert( m_Bitmaps.Find( pName ) == -1 );
		m_CurMemoryUsage += memoryUsage;

		CBitmapCacheEntry newEntry;
		newEntry.m_pBitmap = pBitmap;
		newEntry.m_MemoryUsage = memoryUsage;
		newEntry.m_bLocked = bLock;
		m_Bitmaps.Insert( pName, newEntry );

		EnsureMemoryUsage();
	}

	CBitmap* Find( const char *pName )
	{
		int i = m_Bitmaps.Find( pName );
		if ( i == -1 )
			return NULL;
		else
			return m_Bitmaps[i].m_pBitmap;
	}

	void UnlockAll()
	{
		for ( int i=m_Bitmaps.First(); i != m_Bitmaps.InvalidIndex(); i=m_Bitmaps.Next( i ) )
		{
			m_Bitmaps[i].m_bLocked = false;
		}
	}

private:

	void EnsureMemoryUsage()
	{
		while ( m_CurMemoryUsage > m_MaxMemoryUsage )
		{
			// Free something.
			bool bFreed = false;
			for ( int i=m_Bitmaps.First(); i != m_Bitmaps.InvalidIndex(); i=m_Bitmaps.Next( i ) )
			{
				if ( !m_Bitmaps[i].m_bLocked )
				{
					delete m_Bitmaps[i].m_pBitmap;
					m_CurMemoryUsage -= m_Bitmaps[i].m_MemoryUsage;
					m_Bitmaps.RemoveAt( i );
					break;
				}
			}
			
			// Nothing left to free?			
			if ( !bFreed )
				return;
		}
	}

private:

	class CBitmapCacheEntry
	{
	public:
		CBitmap *m_pBitmap;
		int m_MemoryUsage;
		bool m_bLocked;
	};
	
	CUtlDict<CBitmapCacheEntry,int> m_Bitmaps;
	int m_CurMemoryUsage;
	int m_MaxMemoryUsage;
};

CBitmapCache g_BitmapCache;


/////////////////////////////////////////////////////////////////////////////
// CFileSystemOpenDlg dialog

CFileSystemOpenDlg::CFileSystemOpenDlg(CreateInterfaceFn factory, CWnd* pParent )
	: CDialog(CFileSystemOpenDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFileSystemOpenDlg)
	//}}AFX_DATA_INIT
	m_pFileSystem = (IFileSystem*)factory( FILESYSTEM_INTERFACE_VERSION, NULL );
	if ( !m_pFileSystem )
	{
		Error( "Unable to connect to %s!\n", FILESYSTEM_INTERFACE_VERSION );
	}

	m_bFilterMdlAndJpgFiles = false;
}

void CFileSystemOpenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFileSystemOpenDlg)
	DDX_Control(pDX, IDC_FILENAME_LABEL, m_FilenameLabel);
	DDX_Control(pDX, IDC_FILENAME, m_FilenameControl);
	DDX_Control(pDX, IDC_LOOKIN, m_LookInLabel);
	DDX_Control(pDX, IDC_FILE_LIST, m_FileList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFileSystemOpenDlg, CDialog)
	//{{AFX_MSG_MAP(CFileSystemOpenDlg)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, IDC_FILE_LIST, OnDblclkFileList)
	ON_BN_CLICKED(IDC_UP_BUTTON, OnUpButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_FILE_LIST, OnItemchangedFileList)
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFileSystemOpenDlg message handlers

void CFileSystemOpenDlg::OnOK() 
{
	// Make sure it's a valid filename.
	CString testFilename;
	m_FilenameControl.GetWindowText( testFilename );

	CString fullFilename = m_CurrentDir + "\\" + testFilename;
 	if ( m_pFileSystem->IsDirectory( fullFilename, GetPathID() ) )
	{
		m_CurrentDir = fullFilename;
		PopulateListControl();
	}
	else if ( m_pFileSystem->FileExists( fullFilename, GetPathID() ) )
	{
		m_Filename = fullFilename;
		
		// Translate .jpg to .mdl?
		if ( m_bFilterMdlAndJpgFiles )
		{
			char tempFilename[MAX_PATH];
			V_strcpy_safe( tempFilename, fullFilename );
			char *pPos = strrchr( tempFilename, '.' );
			if ( pPos )
			{
				if ( Q_stricmp( pPos, ".jpeg" ) == 0 || Q_stricmp( pPos, ".jpg" ) == 0 )
				{
					pPos[0] = 0;
					V_strcat_safe( tempFilename, ".mdl" );
					m_Filename = tempFilename;
				}
			}
		}

		EndDialog( IDOK );
	}
	else
	{
		// No file or directory here.
		CString str;
		str.FormatMessage( "File %1!s! doesn't exist.", (const char*)fullFilename );
		AfxMessageBox( str, MB_OK );
	}
}

void CFileSystemOpenDlg::SetInitialDir( const char *pDir, const char *pPathID )
{
	m_CurrentDir = pDir;
	if ( pPathID )
		m_PathIDString = pPathID;
	else
		m_PathIDString = "";
}

CString CFileSystemOpenDlg::GetFilename() const
{
	return m_Filename;
}


BOOL CFileSystemOpenDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// Setup our anchor list.
	AddAnchor( IDC_FILE_LIST, 2, 2 );
	AddAnchor( IDC_FILE_LIST, 3, 3 );
	
	AddAnchor( IDC_FILENAME, 1, 3 );
	AddAnchor( IDC_FILENAME, 3, 3 );
	AddAnchor( IDC_FILENAME, 2, 2 );

	AddAnchor( IDC_FILENAME_LABEL, 0, 0 );
	AddAnchor( IDC_FILENAME_LABEL, 2, 0 );
	AddAnchor( IDC_FILENAME_LABEL, 1, 3 );
	AddAnchor( IDC_FILENAME_LABEL, 3, 3 );

	AddAnchor( IDOK, 0, 2 );
	AddAnchor( IDOK, 2, 2 );
	AddAnchor( IDOK, 1, 3 );
	AddAnchor( IDOK, 3, 3 );
	
	AddAnchor( IDCANCEL, 0, 2 );
	AddAnchor( IDCANCEL, 2, 2 );
	AddAnchor( IDCANCEL, 1, 3 );
	AddAnchor( IDCANCEL, 3, 3 );

	AddAnchor( IDC_LOOKIN, 2, 2 );

	AddAnchor( IDC_UP_BUTTON, 0, 2 );
	AddAnchor( IDC_UP_BUTTON, 2, 2 );


	// Setup our image list.
	m_ImageList.Create( PREVIEW_IMAGE_SIZE, PREVIEW_IMAGE_SIZE, ILC_COLOR32, 0, 512 );
	
	m_BitmapFolder.LoadBitmap( IDB_LABEL_FOLDER );
	m_iLabel_Folder = m_ImageList.Add( &m_BitmapFolder, (CBitmap*)NULL );

	m_BitmapMdl.LoadBitmap( IDB_LABEL_MDL );
	m_iLabel_Mdl = m_ImageList.Add( &m_BitmapMdl, (CBitmap*)NULL );

	m_BitmapFile.LoadBitmap( IDB_LABEL_FILE );
	m_iLabel_File = m_ImageList.Add( &m_BitmapFile, (CBitmap*)NULL );

	m_FileList.SetImageList( &m_ImageList, LVSIL_NORMAL );

	// Populate the list with the contents of our current directory.
	PopulateListControl();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFileSystemOpenDlg::GetEntries( const char *pMask, CUtlVector<CString> &entries, GetEntriesMode_t mode )
{
	CString searchStr = m_CurrentDir + "\\" + pMask;

	// Workaround Steam bug.
	if ( searchStr == ".\\*.*" )
		searchStr = "*.*";
	
	FileFindHandle_t handle;	
	const char *pFile = m_pFileSystem->FindFirst( searchStr, &handle );
	while ( pFile )
	{
		bool bIsDir = m_pFileSystem->FindIsDirectory( handle );
		if ( (mode == GETENTRIES_DIRECTORIES_ONLY && bIsDir) || (mode == GETENTRIES_FILES_ONLY && !bIsDir) )
		{
			entries.AddToTail( pFile );
		}

		pFile = m_pFileSystem->FindNext( handle );
	}
	m_pFileSystem->FindClose( handle );
}



class CJpegSourceMgr : public jpeg_source_mgr
{
public:
	CJpegSourceMgr()
	{
		this->init_source = &CJpegSourceMgr::imp_init_source;
		this->fill_input_buffer = &CJpegSourceMgr::imp_fill_input_buffer;
		this->skip_input_data = &CJpegSourceMgr::imp_skip_input_data;
		this->resync_to_restart = &CJpegSourceMgr::imp_resync_to_restart;
		this->term_source = &CJpegSourceMgr::imp_term_source;

		this->next_input_byte = 0;
		this->bytes_in_buffer = 0;
	}

	bool Init( IFileSystem *pFileSystem, FileHandle_t fp )
	{
		m_Data.SetSize( pFileSystem->Size( fp ) );
		return pFileSystem->Read( m_Data.Base(), m_Data.Count(), fp ) == m_Data.Count();
	}

	static void imp_init_source(j_decompress_ptr cinfo)
	{
	}

	static boolean imp_fill_input_buffer(j_decompress_ptr cinfo)
	{
		Assert( false ); // They should never need to call these functions since we give them all the data up front.
		return 0;
	}

	static void imp_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
	{
		AssertOnce( false ); // They should never need to call these functions since we give them all the data up front.
	}

	static boolean imp_resync_to_restart(j_decompress_ptr cinfo, int desired)
	{
		Assert( false ); // They should never need to call these functions since we give them all the data up front.
		return false;
	}

	static void imp_term_source(j_decompress_ptr cinfo)
	{
	}

	static void error_exit( j_common_ptr cptr )
	{
		CJpegSourceMgr *pInstance = (CJpegSourceMgr*)cptr->client_data;
		longjmp( pInstance->m_JmpBuf, 1 );
	}

public:
	jmp_buf m_JmpBuf;
	CUtlVector<char> m_Data;
};


bool ReadJpeg( IFileSystem *pFileSystem, const char *pFilename, CUtlVector<unsigned char> &buf, int &width, int &height, const char *pPathID )
{
	width = height = 0;

	// Read the data.
	FileHandle_t fp = pFileSystem->Open( pFilename, "rb", pPathID );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return false;

	CJpegSourceMgr sourceMgr;
	bool bRet = sourceMgr.Init( pFileSystem, fp );
	pFileSystem->Close( fp );
	if ( !bRet )
		return false;

	sourceMgr.bytes_in_buffer = sourceMgr.m_Data.Count();
	sourceMgr.next_input_byte = (unsigned char*)sourceMgr.m_Data.Base();

	// Load the jpeg.
	struct jpeg_decompress_struct jpegInfo;
	struct jpeg_error_mgr jerr;

	memset( &jpegInfo, 0, sizeof( jpegInfo ) );
	jpegInfo.client_data = &sourceMgr;
	jpegInfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = &CJpegSourceMgr::error_exit;
	jpeg_create_decompress(&jpegInfo);
	jpegInfo.src = &sourceMgr;

	if ( setjmp( sourceMgr.m_JmpBuf ) == 1 )
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK)
	{
		return false;
	}	

	// start the decompress with the jpeg engine.
	if (jpeg_start_decompress(&jpegInfo) != TRUE || jpegInfo.output_components != 3)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	// now that we've started the decompress with the jpeg lib, we have the attributes of the
	// image ready to be read out of the decompress struct.
	int row_stride = jpegInfo.output_width * jpegInfo.output_components;
	int mem_required = jpegInfo.image_height * jpegInfo.image_width * jpegInfo.output_components;
	JSAMPROW row_pointer[1];
	int cur_row = 0;
	
	width = jpegInfo.output_width;
	height = jpegInfo.output_height;

	// allocate the memory to read the image data into.
	buf.SetSize( mem_required );

	// read in all the scan lines of the image into our image data buffer.
	bool working = true;
	while (working && (jpegInfo.output_scanline < jpegInfo.output_height))
	{
		row_pointer[0] = &(buf[cur_row * row_stride]);
		if (jpeg_read_scanlines(&jpegInfo, row_pointer, 1) != TRUE)
		{
			working = false;
		}
		++cur_row;
	}

	if (!working)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	jpeg_finish_decompress(&jpegInfo);
	return true;
}

void DownsampleRGBToRGBAImage( 
	CUtlVector<unsigned char> &srcData,
	int srcWidth,
	int srcHeight,
	CUtlVector<unsigned char> &destData,
	int destWidth,
	int destHeight )
{
	int srcPixelSize = 3;
	int destPixelSize = 4;
	destData.SetSize( destWidth * destHeight * destPixelSize );
	memset( destData.Base(), 0xFF, destWidth * destHeight * destPixelSize );

	// This preserves the aspect ratio of the image.
	int scaledDestWidth = destWidth;
	int scaledDestHeight = destHeight;
	int destOffsetX = 0, destOffsetY = 0;
	if ( srcWidth > srcHeight )
	{
		scaledDestHeight = (srcHeight * destHeight) / srcWidth;
		destOffsetY = (destHeight - scaledDestHeight) / 2;
	}
	else if ( srcHeight > srcWidth )
	{
		scaledDestWidth = (srcWidth * destWidth) / srcHeight;
		destOffsetX = (destWidth - scaledDestWidth) / 2;
	}

	for ( int destY=0; destY < scaledDestHeight; destY++ )
	{
		unsigned char *pDestLine = &destData[(destY + destOffsetY) * destWidth * destPixelSize + (destOffsetX * destPixelSize)];
		unsigned char *pDestPos = pDestLine;

		float destYPercent = (float)destY / (scaledDestHeight-1);
		int srcY = (int)( destYPercent * (srcHeight-1) );

		for ( int destX=0; destX < scaledDestWidth; destX++ )
		{
			float destXPercent = (float)destX / (scaledDestWidth-1);

			int srcX = (int)( destXPercent * (srcWidth-1) );
			unsigned char *pSrcPos = &srcData[(srcY * srcWidth + srcX) * srcPixelSize];
			pDestPos[0] = pSrcPos[2];
			pDestPos[1] = pSrcPos[1];
			pDestPos[2] = pSrcPos[0];
			pDestPos[3] = 255;

			pDestPos += destPixelSize;
		}
	}
}


CBitmap* SetupJpegLabel( IFileSystem *pFileSystem, CString filename, int labelSize, const char *pPathID )
{
	CBitmap *pBitmap = g_BitmapCache.Find( filename );
	if ( pBitmap )
		return pBitmap;

	CUtlVector<unsigned char> data;
	int width, height;
	if ( !ReadJpeg( pFileSystem, filename, data, width, height, pPathID ) )
		return NULL;

	CUtlVector<unsigned char> downsampled;
	DownsampleRGBToRGBAImage( data, width, height, downsampled, labelSize, labelSize );

	pBitmap = new CBitmap;
	if ( pBitmap->CreateBitmap( labelSize, labelSize, 1, 32, downsampled.Base() ) )
	{
		g_BitmapCache.AddToCache( pBitmap, filename, downsampled.Count(), true );
		return pBitmap;
	}
	else
	{
		delete pBitmap;
		return NULL;
	}
}

int CFileSystemOpenDlg::SetupLabelImage( CFileInfo *pInfo, CString name, bool bIsDir )
{
	if ( bIsDir )
		return m_iLabel_Folder;

	CString extension = name.Right( 4 );
	extension.MakeLower();
	if ( extension == ".jpg" || extension == ".jpeg" )
	{
		pInfo->m_pBitmap = SetupJpegLabel( m_pFileSystem, m_CurrentDir + "\\" + name, PREVIEW_IMAGE_SIZE, GetPathID() );
		if ( pInfo->m_pBitmap )
			return m_ImageList.Add( pInfo->m_pBitmap, (CBitmap*)NULL );
		else
			return m_iLabel_File;
	}
	else
	{
		return (extension == ".mdl") ? m_iLabel_Mdl : m_iLabel_File;
	}
}

void FilterMdlAndJpgFiles( CUtlVector<CString> &files )
{
	// Build a dictionary with all the .jpeg files.
	CUtlDict<int,int> jpgFiles;
	for ( int i=0; i < files.Count(); i++ )
	{
		CString extension = files[i].Right( 4 );
		extension.MakeLower();
		if ( extension == ".jpg" || extension == ".jpeg" )
		{
			CString base = files[i].Left( files[i].GetLength() - 4 );
			jpgFiles.Insert( base, 1 );
		}
	}

	// Now look for all mdls and remove them if they have a jpg.
	for ( int i=0; i < files.Count(); i++ )
	{
		CString extension = files[i].Right( 4 );
		extension.MakeLower();
		if ( extension == ".mdl" )
		{
			CString base = files[i].Left( files[i].GetLength() - 4 );
			if ( jpgFiles.Find( base ) != -1 )
			{
				files.Remove( i );
				--i;
			}
		}		
	}
}


int CALLBACK FileListSortCallback( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CFileSystemOpenDlg *pDlg = (CFileSystemOpenDlg*)lParamSort;
	CFileInfo *pInfo1 = &pDlg->m_FileInfos[lParam1];
	CFileInfo *pInfo2 = &pDlg->m_FileInfos[lParam2];
	if ( pInfo1->m_bIsDir != pInfo2->m_bIsDir )
		return pInfo1->m_bIsDir ? -1 : 1;

	return Q_stricmp( pInfo1->m_Name, pInfo2->m_Name );
}


void RemoveDuplicates( CUtlVector<CString> &files )
{
	CUtlDict<int,int> uniqueFilenames;
	for ( int i=0; i < files.Count(); i++ )
	{
		int iPreviousIndex = uniqueFilenames.Find( files[i] );
		if ( iPreviousIndex == -1 )
		{
			uniqueFilenames.Insert( files[i], i );
		}
		else
		{
			files.Remove( i );
			--i;
		}
	}
}	
		

void CFileSystemOpenDlg::PopulateListControl()
{
	m_FileList.DeleteAllItems();
	g_BitmapCache.UnlockAll();
	m_LookInLabel.SetWindowText( CString( "[ROOT]\\" ) + m_CurrentDir );

	int iItem = 0;

	// First add directories at the top.
	CUtlVector<CString> directories;
	GetEntries( "*.*", directories, GETENTRIES_DIRECTORIES_ONLY );
	RemoveDuplicates( directories );

	for ( int i=0; i < directories.Count(); i++ )
	{
		if ( directories[i] == "." || directories[i] == ".." )
			continue;

		LVITEM item;
		item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
		item.iItem = iItem++;
		item.iSubItem = 0;
		item.pszText = directories[i].GetBuffer(0);
		
		item.lParam = m_FileInfos.AddToTail();
		m_FileInfos[item.lParam].m_bIsDir = true;
		m_FileInfos[item.lParam].m_Name = directories[i];
		item.iImage = SetupLabelImage( &m_FileInfos[item.lParam], directories[i], true );

		m_FileList.InsertItem( &item );
	}

	CUtlVector<CString> files;
	for ( int iMask=0; iMask < m_FileMasks.Count(); iMask++ )
	{
		GetEntries( m_FileMasks[iMask], files, GETENTRIES_FILES_ONLY );
	}

	RemoveDuplicates( files );
	if ( m_bFilterMdlAndJpgFiles )
		FilterMdlAndJpgFiles( files );

	for ( int i=0; i < files.Count(); i++ )
	{
		LVITEM item;
		item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
		item.iItem = iItem++;
		item.iSubItem = 0;
		item.iImage = m_iLabel_Mdl;
		item.pszText = files[i].GetBuffer(0);

		item.lParam = m_FileInfos.AddToTail();
		m_FileInfos[item.lParam].m_bIsDir = false;
		m_FileInfos[item.lParam].m_Name = files[i];
		item.iImage = SetupLabelImage( &m_FileInfos[item.lParam], files[i], false );

		m_FileList.InsertItem( &item );
	}

	m_FileList.SortItems( FileListSortCallback, (DWORD)this );
}

void CFileSystemOpenDlg::AddFileMask( const char *pMask )
{
	m_FileMasks.AddToTail( pMask );
}


BOOL CFileSystemOpenDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	return CDialog::Create(IDD, pParentWnd);
}

int CFileSystemOpenDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}


LONG& GetSideCoord( RECT &rect, int iSide )
{
	if ( iSide == 0 )
		return rect.left;
	else if ( iSide == 1 )
		return rect.top;
	else if ( iSide == 2 )
		return rect.right;
	else
		return rect.bottom;
}


LONG GetSideScreenCoord( CWnd *pWnd, int iSide )
{
	RECT rect;
	pWnd->GetWindowRect( &rect );
	return GetSideCoord( rect, iSide );
}


void CFileSystemOpenDlg::ProcessAnchor( CWindowAnchor *pAnchor )
{
	RECT rect, parentRect;
	GetWindowRect( &parentRect );
	pAnchor->m_pWnd->GetWindowRect( &rect );
	
	GetSideCoord( rect, pAnchor->m_Side ) = GetSideCoord( parentRect, pAnchor->m_ParentSide ) + pAnchor->m_OriginalDist;

	ScreenToClient( &rect );
	pAnchor->m_pWnd->MoveWindow( &rect );
}


void CFileSystemOpenDlg::AddAnchor( int iDlgItem, int iSide, int iParentSide )
{
	CWnd *pItem = GetDlgItem( iDlgItem );
	if ( !pItem )
		return;

	CWindowAnchor *pAnchor = &m_Anchors[m_Anchors.AddToTail()];
	pAnchor->m_pWnd = pItem;
	pAnchor->m_Side = iSide;
	pAnchor->m_ParentSide = iParentSide;
	pAnchor->m_OriginalDist = GetSideScreenCoord( pItem, iSide ) - GetSideScreenCoord( this, iParentSide );
}



void CFileSystemOpenDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	for ( int i=0; i < m_Anchors.Count(); i++ )
		ProcessAnchor( &m_Anchors[i] );	

	if ( m_FileList.GetSafeHwnd() )
		PopulateListControl();
}

void CFileSystemOpenDlg::OnDblclkFileList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	/*int iSelected = m_FileList.GetNextItem( -1, LVNI_SELECTED );
	if ( iSelected != -1 )
	{
		DWORD iItem = m_FileList.GetItemData( iSelected );
		if ( iItem < (DWORD)m_FileInfos.Count() )
		{
			CFileInfo *pInfo = &m_FileInfos[iItem];
			if ( pInfo->m_bIsDir )
			{
				m_CurrentDir += "\\" + m_FileInfos[iItem].m_Name;
				PopulateListControl();
			}
			else
			{
				m_Filename = m_CurrentDir + "\\" + m_FileInfos[iItem].m_Name;
				EndDialog( IDOK );
			}
		}
		else
		{
			Assert( false );
		}
	}*/
	OnOK();
	
	*pResult = 0;
}

void CFileSystemOpenDlg::OnUpButton() 
{
	char str[MAX_PATH];
	V_strcpy_safe( str, m_CurrentDir );
	Q_StripLastDir( str, sizeof( str ) );

	if ( str[0] == 0 )
		V_strcpy_safe( str, "." );
	
	if ( str[strlen(str)-1] == '\\' || str[strlen(str)-1] == '/' )
		str[strlen(str)-1] = 0;

	m_CurrentDir = str;
	PopulateListControl();
}

void CFileSystemOpenDlg::OnItemchangedFileList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	DWORD iItem = m_FileList.GetItemData( pNMListView->iItem );
	if ( iItem < (DWORD)m_FileInfos.Count() )
	{
		CFileInfo *pInfo = &m_FileInfos[iItem];
		if ( (pNMListView->uChanged & LVIF_STATE) && 
			 (pNMListView->uNewState & LVIS_SELECTED) )
		{
			m_FilenameControl.SetWindowText( pInfo->m_Name );
		}
	}
	
	*pResult = 0;
}

void CFileSystemOpenDlg::SetFilterMdlAndJpgFiles( bool bFilter )
{
	m_bFilterMdlAndJpgFiles = bFilter;
}

const char* CFileSystemOpenDlg::GetPathID()
{
	if ( m_PathIDString == "" )
		return NULL;
	else
		return (const char*)m_PathIDString;
}



// ------------------------------------------------------------------------------------------------ //
// Implementation of IFileSystemOpenDialog.
// ------------------------------------------------------------------------------------------------ //

// IFileSystemOpenDialog implementation.
class CFileSystemOpenDialogWrapper : public IFileSystemOpenDialog
{
public:
	CFileSystemOpenDialogWrapper()
	{
		m_pDialog = 0;
		m_bLastModalWasWindowsDialog = false;
	}

	virtual void Release()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		delete m_pDialog;
		delete this;
	}

	// You must call this first to set the hwnd.
	virtual void Init( CreateInterfaceFn factory, void *parentHwnd )
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( !m_pDialog );

		m_hParentWnd = (HWND)parentHwnd;
		m_pDialog = new CFileSystemOpenDlg( factory, CWnd::FromHandle( m_hParentWnd ) );
	}

	virtual void AddFileMask( const char *pMask )
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( m_pDialog );
		m_pDialog->AddFileMask( pMask );
	}

	virtual void SetInitialDir( const char *pDir, const char *pPathID )
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( m_pDialog );
		m_pDialog->SetInitialDir( pDir, pPathID );
	}

	virtual void SetFilterMdlAndJpgFiles( bool bFilter )
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( m_pDialog );
		m_pDialog->SetFilterMdlAndJpgFiles( bFilter );
	}

	virtual void GetFilename( char *pOut, int outLen ) const
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( m_pDialog );
		
		if ( m_bLastModalWasWindowsDialog )
		{
			Q_strncpy( pOut, m_RelativeFilename, outLen );
		}
		else
		{
			Q_strncpy( pOut, m_pDialog->GetFilename(), outLen );
		}
	}

	virtual bool DoModal()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		Assert( m_pDialog );

		m_bLastModalWasWindowsDialog = false;
		return m_pDialog->DoModal() == IDOK;
	}

	virtual bool DoModal_WindowsDialog()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		Assert( m_pDialog );

		m_bLastModalWasWindowsDialog = true;

		// Get the full filename, then make sure it's a relative path.
		char defExt[MAX_PATH] = {0};
		if ( m_pDialog->m_FileMasks.Count() > 0 )
		{
			CString ext = m_pDialog->m_FileMasks[m_pDialog->m_FileMasks.Count()-1].Right( 4 );
			const char *pStr = ext;
			if ( pStr[0] == '.' )
				V_strcpy_safe( defExt, pStr+1 );
		}

		char pFileNameBuf[MAX_PATH];
		const char *pFileName = m_pDialog->m_pFileSystem->RelativePathToFullPath( m_pDialog->m_CurrentDir, m_pDialog->m_PathIDString, pFileNameBuf, MAX_PATH );
		V_strcat_safe( pFileNameBuf, "\\" );
	
		// Build the list of file filters.
		char filters[1024];
		if ( m_pDialog->m_FileMasks.Count() == 0 )
		{
			V_strcpy_safe( filters, "All Files (*.*)|*.*||" );
		}
		else
		{
			filters[0] = 0;
			for ( int i=0; i < m_pDialog->m_FileMasks.Count(); i++ )
			{
				if ( i > 0 )
					V_strcat_safe( filters, "|" );

				V_strcat_safe( filters, m_pDialog->m_FileMasks[i] );
				V_strcat_safe( filters, "|" );
				V_strcat_safe( filters, m_pDialog->m_FileMasks[i] );
				if ( pFileName )
				{
					V_strcat_safe( pFileNameBuf, m_pDialog->m_FileMasks[i] );
					V_strcat_safe( pFileNameBuf, ";" );
				}

			}
			V_strcat_safe( filters, "||" );
		}

		CFileDialog dlg( 
			true,								// open dialog?
			defExt[0]==0 ? NULL : defExt,		// default file extension
			pFileName,							// initial filename
			OFN_ENABLESIZING,					// flags
			filters,
			CWnd::FromHandle( m_hParentWnd ) );

		while ( dlg.DoModal() == IDOK )
		{
			// Make sure we can make this into a relative path.
			if ( m_pDialog->m_pFileSystem->FullPathToRelativePath( dlg.GetPathName(), m_RelativeFilename, sizeof( m_RelativeFilename ) ) )
			{
				// Replace .jpg or .jpeg extension with .mdl?
				char *pEnd = m_RelativeFilename;
				while ( Q_stristr( pEnd+1, ".jpeg" ) || Q_stristr( pEnd+1, ".jpg" ) )
					pEnd = max( Q_stristr( pEnd, ".jpeg" ), Q_stristr( pEnd, ".jpg" ) );

				if ( pEnd && pEnd != m_RelativeFilename )
					Q_strncpy( pEnd, ".mdl", sizeof( m_RelativeFilename ) - (pEnd - m_RelativeFilename) );

				return true;
			}
			else
			{
				AfxMessageBox( IDS_NO_RELATIVE_PATH );
			}
		}

		return false;
	}

private:
	CFileSystemOpenDlg *m_pDialog;
	HWND m_hParentWnd;

	char m_RelativeFilename[MAX_PATH];
	bool m_bLastModalWasWindowsDialog;
};

EXPOSE_INTERFACE( CFileSystemOpenDialogWrapper, IFileSystemOpenDialog, FILESYSTEMOPENDIALOG_VERSION );

