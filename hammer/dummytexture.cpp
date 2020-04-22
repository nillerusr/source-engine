//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for placeholder textures.
//			Placeholder textures are used for textures that are referenced in
//			the map file but not found in storage.
//
//=============================================================================//

#include "stdafx.h"
#include <process.h>
#include <afxtempl.h>
#include <io.h>
#include <sys\stat.h>
#include <fcntl.h>
#include "DummyTexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CDummyTexture::CDummyTexture(const char *pszName, TEXTUREFORMAT eFormat)
{
	if (pszName != NULL)
	{
		strcpy(m_szName, pszName);
	}
	else
	{
		strcpy(m_szName, "Missing texture");
	}

	m_eTextureFormat = eFormat;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CDummyTexture::~CDummyTexture()
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns an empty string, since we have no source file.
//-----------------------------------------------------------------------------
const char *CDummyTexture::GetFileName() const
{
	static char *pszEmpty = "";
	return(pszEmpty);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CDummyTexture::GetKeywords(char *pszKeywords) const
{
	if (pszKeywords != NULL)
	{
		*pszKeywords = '\0';
	}

	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
// Output : 
//-----------------------------------------------------------------------------
// dvs: move into a common place for CWADTexture & CDummyTexture
int CDummyTexture::GetShortName(char *pszName) const
{
	char szBuf[MAX_PATH];

	if (pszName == NULL)
	{
		pszName = szBuf;
	}

	if (m_eTextureFormat == tfWAL)
	{
		const char *psz = strstr(m_szName, "textures");
		if (psz == NULL)
		{
			psz = m_szName;
		}
		else
		{
			psz += strlen("textures\\");
		}

		strcpy(pszName, psz);

		// remove extension
		char *pszExtension = strstr(pszName, ".wal");
		if (pszExtension)
		{
			*pszExtension = 0;
		}
	}
	else
	{
		strcpy(pszName, m_szName);
	}

	return(strlen(pszName));
}


//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGB format to the buffer
// Input  : pImageRGB - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGB image in bytes.
//-----------------------------------------------------------------------------
int CDummyTexture::GetImageDataRGB( void *pImageRGB )
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGBA format to the buffer
// Input  : pImageRGBA - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGBA image in bytes.
//-----------------------------------------------------------------------------
int CDummyTexture::GetImageDataRGBA( void *pImageRGBA )
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : size - 
//-----------------------------------------------------------------------------
void CDummyTexture::GetSize( SIZE &size ) const
{
	size.cx = 0;
	size.cy = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Renders "No Image" into a device context as a placeholder for the
//			missing texture.
// Input  : pDC - 
//			rect - 
//			iFontHeight - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CDummyTexture::Draw(CDC *pDC, RECT &rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData)
{
	CFont *pOldFont = (CFont *)pDC->SelectStockObject(ANSI_VAR_FONT);
	COLORREF crText = pDC->SetTextColor(RGB(0xff, 0xff, 0xff));
	COLORREF crBack = pDC->SetBkColor(RGB(0, 0, 0));

	pDC->FillRect(&rect, CBrush::FromHandle(HBRUSH(GetStockObject(BLACK_BRUSH))));
	pDC->TextOut(rect.left + 2, rect.top + 2, "No Image", 8);

	pDC->SelectObject(pOldFont);
	pDC->SetTextColor(crText);
	pDC->SetBkColor(crBack);
}


//-----------------------------------------------------------------------------
// Purpose: Loads this texture from disk if it is not already loaded.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDummyTexture::Load( void )
{
	return(true);
}
