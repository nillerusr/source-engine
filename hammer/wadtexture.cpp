//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of the IEditorTexture interface for WAD textures.
//
//=============================================================================//


#include "stdafx.h"
#include <process.h>
#include <afxtempl.h>
#include <io.h>
#include <sys\stat.h>
#include <fcntl.h>
#include "hammer.h"
#include "MapDoc.h"
#include "Options.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "WADTypes.h"
#include "BSPFile.h"
#include "bitmap/imageformat.h" // hack : don't want to include this just for ImageFormat
#include "TextureSystem.h"
#include "WADTexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)


#define _GraphicCacheAllocate(n)	malloc(n)


//-----------------------------------------------------------------------------
// Stuff for loading WAD3 files.
//-----------------------------------------------------------------------------
typedef struct WAD3miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[4];			// four mip maps stored
} WAD3miptex_t;


//-----------------------------------------------------------------------------
// Stuff for loading WAL files.
//-----------------------------------------------------------------------------
typedef struct					// Mip Graphic
{
	char name[32];				// Name of the Graphic.
	DWORD width;				// width of picture, must be a multiple of 8
	DWORD height;				// height of picture, must be a multiple of 8
	DWORD offset1;				// offset to u_char Pix[width   * height]
	DWORD offset2;				// offset to u_char Pix[width/2 * height/2]
	DWORD offset4;				// offset to u_char Pix[width/4 * height/4]
	DWORD offset8;				// offset to u_char Pix[width/8 * height/8]
	char animname[32];
	DWORD surface;
	DWORD contents;
	DWORD value;
} walhdr_t;


static char *g_pLoadBuf = NULL;
static int g_nLoadSize = 128 * 1024;


extern void ScaleBitmap(CSize sizeSrc, CSize sizeDest, char *src, char *dest);


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSize - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool AllocateLoadBuffer(int nSize)
{
	if (nSize > g_nLoadSize)
	{
		g_nLoadSize = nSize;

		if (g_pLoadBuf != NULL)
		{
			delete[] g_pLoadBuf;
			g_pLoadBuf = NULL;
		}
	}

	if (g_pLoadBuf == NULL)
	{
		g_pLoadBuf = new char[g_nLoadSize];
	}

	if (g_pLoadBuf == NULL)
	{
		return(false);
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CWADTexture::CWADTexture(void)
{
	memset(m_szName, 0, sizeof(m_szName));
	memset(m_szFileName, 0, sizeof(m_szFileName));

	m_datawidth = 0;
	m_dataheight = 0;

	m_WALsurface = 0;
	m_WALvalue = 0;
	m_WALcontents = 0;

	m_ulFileOffset = 0;
	m_ulFileID = 0;

	memset(&format, 0, sizeof(format));

	m_pPalette = NULL;
	m_bLocalPalette = false;

	m_nWidth = 0;
	m_nHeight = 0;

	m_pData = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees texture image data and palette.
//-----------------------------------------------------------------------------
CWADTexture::~CWADTexture(void)
{
	//
	// Free image data.
	//
	if (m_pData != NULL)
	{
		free(m_pData);
		m_pData = NULL;
	}

	//
	// Free palette.
	//
	if (m_pPalette != NULL)
	{
		free(m_pPalette);
		m_pPalette = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the full path of the file (WAD, PAK, or WAL) from which this
//			texture was loaded.
//-----------------------------------------------------------------------------
const char *CWADTexture::GetFileName( void ) const
{
	return(m_szFileName);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fd - 
//			ulFileID - 
//			bLoad - 
//			pszName - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CWADTexture::Init(int fd, DWORD ulFileID, BOOL bLoad, LPCTSTR pszName)
{
	//
	// Load header and copy needed data into members variables.
	//
	GRAPHICSFILESTRUCT FileInfo;
	bool bFound = g_Textures.FindGraphicsFile(&FileInfo, ulFileID);
	if (!bFound)
	{
		miptex_t hdr;
		_read(fd, (char *)&hdr, sizeof(hdr));

		m_nWidth = hdr.width;
		m_nHeight = hdr.height;
	}
	else if (FileInfo.format == tfWAD3)
	{
		WAD3miptex_t hdr;
		_read(fd, (char *)&hdr, sizeof(hdr));

		m_nWidth = hdr.width;
		m_nHeight = hdr.height;

		if (m_nHeight < 0)
		{
			return(FALSE);
		}
	}
	else
	{
		return(FALSE);
	}

	m_ulFileID = ulFileID;

	strcpy(m_szName, pszName);

	if (bFound)
	{
		strcpy(m_szFileName, FileInfo.filename);
	}

	if (m_nWidth * m_nHeight > MAX_TEXTURESIZE)
	{
		return(FALSE);
	}

	if (!m_szName[0])
	{
		return(FALSE);
	}

	// set offset
	m_ulFileOffset = _tell(fd);
	
	if (bLoad)
	{
		return(Load());
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : CPalette *
//-----------------------------------------------------------------------------
CPalette *CWADTexture::GetPalette(void) const
{
	static CPalette pal;
	pal.DeleteObject();
	pal.CreatePalette(m_pPalette);
	return &pal;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CWADTexture::GetKeywords(char *pszKeywords) const
{
	//
	// Set the keywords to the WAD file name.
	//
	if (pszKeywords != NULL)
	{
		const char *pszLastSlash = strrchr(m_szFileName, '\\');
		if (pszLastSlash != NULL)
		{
			pszLastSlash++;
			strcpy(pszKeywords, pszLastSlash);
		}
		else
		{
			strcpy(pszKeywords, m_szFileName);
		}
	}

	return(strlen(m_szFileName));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
// Output : Returns the length of the short name in characters.
//-----------------------------------------------------------------------------
int CWADTexture::GetShortName(char *pszName) const
{
	char szBuf[MAX_PATH];
	szBuf[0] = '\0';

	if (pszName == NULL)
	{
		pszName = szBuf;
	}

	if (format == tfWAL)
	{
		const char *pszCopy = strstr(m_szName, "textures");
		if (pszCopy == NULL)
		{
			pszCopy = m_szName;
		}
		else
		{
			pszCopy += strlen("textures\\");
		}

		strcpy(pszName, pszCopy);

		// remove extension
		char *psz = strstr(szBuf, ".wal");
		if (psz != NULL)
		{
			*psz = 0;
		}
	}
	else
	{
		strcpy(pszName, m_szName);
	}

	return(strlen(pszName));
}


//-----------------------------------------------------------------------------
// Purpose: Resizes a texture to be even powers of 2 in width and height.
// Input  : pLoadBuf - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CWADTexture::AdjustTexture(char *pLoadBuf)
{
	// make height/width power of two
	int i, i2;

	for (i = 0; ; i++)
	{
		i2 = 1 << i;
		if (i2 >= m_nWidth)
		{
			m_datawidth = i2;
			break;
		}
	}

	for (i = 0; ; i++)
	{
		i2 = 1 << i;
		if (i2 >= m_nHeight)
		{
			m_dataheight = i2;	
			break;
		}
	}

	// allocate data
	m_pData = _GraphicCacheAllocate(m_datawidth * m_dataheight);

	if (m_pData == NULL)
	{
		CString errmsg;
		errmsg.Format(IDS_ERRLOADGRAPHIC, m_szName);
		AfxMessageBox(errmsg);
		return FALSE;
	}

	// scale up to data
	ScaleBitmap(CSize(m_nWidth, m_nHeight), CSize(m_datawidth, m_dataheight), pLoadBuf, (char *)m_pData);

	return TRUE;
}

bool CWADTexture::IsLoaded() const
{
	return (m_pData != NULL);
}

//-----------------------------------------------------------------------------
// Purpose: Load data from file associated with m_ulFileID.
// Input  : fd - 
//			hFile - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CWADTexture::Load(int fd, HANDLE hFile)
{
	if (m_pData)
	{
		return TRUE;	// already loaded
	}
	
	// if fd is -1, get it from base file.. otherwise we've been
	//  given an fd by caller, so use that in loading
	GRAPHICSFILESTRUCT fileInfo;
	Q_memset( &fileInfo, 0, sizeof(fileInfo));


	if (fd == -1)
	{
		// find graphics file - different loading based on wad type.
		if (!g_Textures.FindGraphicsFile(&fileInfo, m_ulFileID))
		{	
			return(FALSE);
		}

		// keep fd
		fd = fileInfo.fd;
		
		// seek to offset
		_lseek(fd, m_ulFileOffset, SEEK_SET);
	}

	m_bLocalPalette = FALSE;

	// dvs: if fd != -1, using FileInfo without initializing it!!
	if (!AllocateLoadBuffer(m_nWidth * m_nHeight))
	{
		AfxMessageBox("Couldn't allocate a texture loading buffer.");
		return FALSE;
	}

	// load bitmap
	_read(fd, g_pLoadBuf, m_nWidth * m_nHeight);

	//
	// If WAD3, read the palette.
	//
	if (fileInfo.format == tfWAD3)
	{
		WORD nPal;

		_lseek(fd, (m_nWidth / 2 * m_nHeight / 2) + (m_nWidth / 4 * m_nHeight / 4) + (m_nWidth / 8 * m_nHeight / 8), SEEK_CUR);

		_read(fd, &nPal, sizeof nPal);

		Assert(nPal <= 256);

		if ((nPal > 0) && (nPal < 1024))
		{
			m_bLocalPalette = TRUE;
			
			// setup palette
			m_pPalette = (LOGPALETTE *)malloc(sizeof(WORD) * 2 + sizeof(PALETTEENTRY) * nPal);

			// fast load - throw into buffer
			static unsigned char PalBuf[3 * 1024];
			_read(fd, PalBuf, nPal * 3);

			// convert to LOGPALETTE
			for (int i = 0; i < nPal; i++)
			{
				m_pPalette->palPalEntry[i].peRed = PalBuf[i*3];
				m_pPalette->palPalEntry[i].peGreen = PalBuf[i*3+1];
				m_pPalette->palPalEntry[i].peBlue = PalBuf[i*3+2];
				m_pPalette->palPalEntry[i].peFlags = D3DRMPALETTE_READONLY | PC_NOCOLLAPSE;
			}

			m_pPalette->palVersion = 0x300;
			m_pPalette->palNumEntries = nPal;
		}
	}

	AdjustTexture(g_pLoadBuf);

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGB format to the buffer
// Input  : pImageRGB - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGB image in bytes.
//-----------------------------------------------------------------------------
int CWADTexture::GetImageDataRGB( void *pImageRGB )
{
	if ( pImageRGB != NULL )
	{
		Load();

		unsigned char *puchImage = ( unsigned char * )m_pData;
		unsigned char *pIndex = ( unsigned char * )pImageRGB;

		for (int y = 0; y < m_dataheight; y++)
		{
			for (int x = 0; x < m_datawidth; x++)
			{
				unsigned char chPaletteEntry = puchImage[y * m_datawidth + x];

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peRed;
				pIndex++;

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peGreen;
				pIndex++;

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peBlue;
				pIndex++;
			}
		}
	}

	return(	m_datawidth * m_dataheight * 3 );
}


//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGBA format to the buffer
// Input  : pImageRGBA - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGBA image in bytes.
//-----------------------------------------------------------------------------
int CWADTexture::GetImageDataRGBA( void *pImageRGBA )
{
	if ( pImageRGBA != NULL )
	{
		unsigned char *puchImage = (unsigned char *)m_pData;
		unsigned char *pIndex = (unsigned char *)pImageRGBA;

		for (int y = 0; y < m_dataheight; y++)
		{
			for (int x = 0; x < m_datawidth; x++)
			{
				unsigned char chPaletteEntry = puchImage[y * m_datawidth + x];

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peRed;
				pIndex++;

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peGreen;
				pIndex++;

				*pIndex = m_pPalette->palPalEntry[chPaletteEntry].peBlue;
				pIndex++;

				*pIndex = 0;
				pIndex++;
			}
		}
	}

	return(	m_datawidth * m_dataheight * 4 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : size - 
//-----------------------------------------------------------------------------
void CWADTexture::GetSize(SIZE& size)
{
	size.cx = m_nWidth;
	size.cy = m_nHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Draws white "No Image" text in a black rectangle.
// Input  : pDC - 
//			rect - 
//			iFontHeight - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CWADTexture::DrawNoImage(CDC *pDC, RECT& rect, int iFontHeight)
{
	// draw "no data"
	CFont *pOldFont = (CFont*) pDC->SelectStockObject(ANSI_VAR_FONT);
	COLORREF cr = pDC->SetTextColor(RGB(0xff, 0xff, 0xff));
	COLORREF cr2 = pDC->SetBkColor(RGB(0, 0, 0));

	// draw black rect first
	pDC->FillRect(&rect, CBrush::FromHandle(HBRUSH(GetStockObject(BLACK_BRUSH))));

	// then text
	pDC->TextOut(rect.left+2, rect.top+2, "No Image", 8);
	pDC->SelectObject(pOldFont);
	pDC->SetTextColor(cr);
	pDC->SetBkColor(cr2);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDC - 
//			rect - 
//			iFontHeight - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CWADTexture::Draw(CDC *pDC, RECT& rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData)
{
	if (!m_nWidth)
	{
		DrawNoImage(pDC, rect, iFontHeight);
		return;
	}

	// no data -
	if (!m_pData)
	{
		// try to load -
		if (!Load())
		{
			DrawNoImage(pDC, rect, iFontHeight);
			return;
		}
	}

	static struct
	{
		BITMAPINFOHEADER bmih;
		unsigned short colorindex[256];
	} bmi;

	BITMAPINFOHEADER& bmih = bmi.bmih;
	memset(&bmih, 0, sizeof bmih);
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = m_datawidth;
	bmih.biHeight = -m_dataheight;	// top-down DIB
	bmih.biCompression = BI_RGB;
	bmih.biBitCount = 8;

	bmih.biPlanes = 1;

	static BOOL bInit = FALSE;
	if (!bInit)
	{
		bInit = TRUE;
		for (int i = 0; i < 256; i++)
		{
			bmi.colorindex[i] = i;
		}
	}

	int dest_width = rect.right - rect.left;
	int dest_height = rect.bottom - rect.top;

	if (DrawTexData.nFlags & drawCaption)
	{
		dest_height -= iFontHeight + 4;
	}

	if (!(DrawTexData.nFlags & drawResizeAlways))
	{
		if (m_nWidth < dest_width)
		{
			dest_width = m_nWidth;
		}

		if (m_nHeight < dest_height)
		{
			dest_height = m_nHeight;
		}
	}

	SetStretchBltMode(pDC->m_hDC, COLORONCOLOR);

	if (StretchDIBits(pDC->m_hDC, rect.left, rect.top, dest_width, dest_height, 0, 0, m_datawidth, m_dataheight, m_pData, (BITMAPINFO*)&bmi, DIB_PAL_COLORS, SRCCOPY) == GDI_ERROR)
	{
		Msg(mwError, "CWADTexture::Draw(): StretchDIBits failed.");
	}

	//
	// Caption.
	//
	if (DrawTexData.nFlags & drawCaption)
	{
		// draw background for name
		CBrush brCaption(RGB(0, 0, 255));
		CRect rcCaption(rect);
		
		rcCaption.top = rcCaption.bottom - (iFontHeight + 5);
		pDC->FillRect(rcCaption, &brCaption);

		// draw name
		char szShortName[MAX_PATH];
		int iLen = GetShortName(szShortName);
		pDC->TextOut(rect.left, rect.bottom - (iFontHeight + 4), szShortName, iLen);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Frees the static load buffer.
//-----------------------------------------------------------------------------
bool CWADTexture::Initialize(void)
{
	return(AllocateLoadBuffer(g_nLoadSize));
}


//-----------------------------------------------------------------------------
// Purpose: Loads this texture from disk, if it is not already loaded.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWADTexture::Load( void )
{
	if (m_pData != NULL)
	{
		// Already loaded.
		return(true);
	}

	return(Load(-1, NULL) == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Frees the static load buffer.
//-----------------------------------------------------------------------------
void CWADTexture::ShutDown(void)
{
	if (g_pLoadBuf != NULL)
	{
		delete[] g_pLoadBuf;
		g_pLoadBuf = NULL;
	}
}

