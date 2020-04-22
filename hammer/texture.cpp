//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Simple texture object used for sprites. Handed to the renderer
//			for binding. May become a general purpose texture object.
//
//=============================================================================//

#include "stdafx.h"
#include "Texture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constuctor. Initializes data members.
//-----------------------------------------------------------------------------
CTexture::CTexture( void )
{
	m_nWidth = 0;
	m_nHeight = 0;

	m_bHasAlpha = false;

	m_pImageData = NULL;

	m_nTextureID = TEXTURE_ID_NONE;

	m_szName[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees the texture image data.
//-----------------------------------------------------------------------------
CTexture::~CTexture( void )
{
	if ( m_pImageData != NULL )
	{
		delete [] m_pImageData;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allocates image data for a given texture size and pixel format.
// Input  : nWidth - Desired width of image, in texels.
//			nHeight - Desired height of image, in texels.
//			nFlags - Flags indicating the pixel format of the image data. The default
//					is 24 bit RGB, no alpha component, but can be set to:
//
//				TEXTURE_HAS_ALPHA: the image has an alpha component. Each texel is 32 bits.
//
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTexture::Allocate( int nWidth, int nHeight, int nFlags )
{
	if ( m_pImageData != NULL )
	{
		delete [] m_pImageData;

		m_pImageData = NULL;
		m_nWidth = 0;
		m_nHeight = 0;
	}

	if (( nWidth == 0 ) || ( nHeight == 0 ))
	{
		return( false );
	}

	if ( nFlags & TEXTURE_HAS_ALPHA )
	{
		m_pImageData = new unsigned char [nWidth * nHeight * 4];
		m_bHasAlpha = true;
	}
	else
	{
		m_pImageData = new unsigned char [nWidth * nHeight * 3];
		m_bHasAlpha = false;
	}

	if ( m_pImageData != NULL )
	{
		m_nWidth = nWidth;
		m_nHeight = nHeight;
	}

	return( m_pImageData != NULL );
}
		

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDC - 
//			rect - 
//			iFontHeight - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CTexture::Draw(CDC *pDC, RECT &rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData)
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns the full path of the file from which this texture was loaded.
//-----------------------------------------------------------------------------
const char *CTexture::GetFileName() const
{
	static char szEmpty[] = "";
	return(szEmpty);
}


//-----------------------------------------------------------------------------
// Purpose: Gets the image data in RGB format, or queries the image data size.
// Input  : pData - Buffer to receive image data. If NULL, no image data is
//				copied. Otherwise, the buffer must be large enough to hold the
//				image data.
// Output : Returns the image data size in bytes.
//-----------------------------------------------------------------------------
int CTexture::GetImageDataRGB( void *pData )
{
	int nSize = 0;

	if ( m_bHasAlpha )
	{
		// Conversion from 32 to 24 bits not implemented.
		Assert( FALSE );
	}
	else
	{
		nSize = m_nWidth * m_nHeight * 3;

		if (( pData != NULL ) && ( nSize > 0 ))
		{
			memcpy( pData, m_pImageData, nSize );
		}
	}

	return( nSize );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the image data in RGBA format, or queries the image data size.
// Input  : pData - Buffer to receive image data. If NULL, no image data is
//				copied. Otherwise, the buffer must be large enough to hold the
//				image data.
// Output : Returns the image data size in bytes.
//-----------------------------------------------------------------------------
int CTexture::GetImageDataRGBA( void *pData )
{
	int nSize = 0;

	if ( m_bHasAlpha )
	{
		nSize = m_nWidth * m_nHeight * 4;

		if (( pData != NULL ) && ( nSize > 0 ))
		{
			memcpy( pData, m_pImageData, nSize );
		}
	}
	else
	{
		// Conversion from 24 to 32 bits not implemented.
		Assert( FALSE );
	}

	return( nSize );
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CTexture::GetKeywords(char *pszKeywords) const
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
int CTexture::GetShortName(char *pszName) const
{
	if (pszName != NULL)
	{
		strcpy(pszName, m_szName);
	}

	return(strlen(m_szName));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTexture::Load(void)
{
	return(true);
}
