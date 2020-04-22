//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Simple texture object used for sprites. Handed to the renderer
//			for binding. May become a general purpose texture object.
//
//=============================================================================//

#ifndef TEXTURE_H
#define TEXTURE_H
#ifdef _WIN32
#pragma once
#endif


#include "IEditorTexture.h"


enum
{
	TEXTURE_HAS_ALPHA = 0x01
};


class CTexture : public IEditorTexture
{
	public:

		CTexture( void );
		virtual ~CTexture( void );

		bool Allocate( int nWidth, int nHeight, int nFlags );

		void Draw(CDC *pDC, RECT &rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData);

		inline void *GetImageDataPtr( void )
		{
			return( m_pImageData );
		}

		const char *GetFileName(void) const;

		inline const char *GetName(void) const
		{
			return(m_szName);
		}
		int GetShortName(char *pszName) const;

		int GetKeywords(char *pszKeywords) const;
		
		int GetImageDataRGB( void *pData = NULL );
		int GetImageDataRGBA( void *pData = NULL );

		inline int GetImageWidth( void ) const
		{
			return( m_nWidth );
		}

		inline int GetImageHeight( void ) const
		{
			return( m_nHeight );
		}

		inline int GetWidth( void ) const
		{
			return( m_nWidth );
		}

		inline int GetHeight( void ) const
		{
			return( m_nHeight );
		}

		inline float GetDecalScale( void ) const
		{
			return( 1.0f );
		}
		
		inline CPalette *GetPalette( void ) const
		{
			return( NULL );
		}

		inline int GetSurfaceAttributes( void ) const
		{
			return(0);
		}

		inline int GetSurfaceContents(void ) const
		{
			return(0);
		}

		inline int GetSurfaceValue( void ) const
		{
			return(0);
		}

		inline TEXTUREFORMAT GetTextureFormat( void ) const
		{
			return(tfSprite);
		}

		inline int GetTextureID( void ) const
		{
			return( m_nTextureID );
		}

		inline bool HasAlpha( void ) const
		{
			return( m_bHasAlpha );
		}

		inline bool HasData( void ) const
		{
			return( m_pImageData != NULL );
		}

		inline bool HasPalette( void ) const
		{
			return( false );
		}

		inline bool IsDummy( void ) const
		{
			return(( m_nWidth == 0) || ( m_nHeight == 0) || ( m_pImageData == NULL ));
		}

		bool Load( void );

		inline bool IsLoaded() const
		{
			return true;
		}

		inline void SetTextureID( int nTextureID )
		{
			m_nTextureID = nTextureID;
		}

	protected:

		int m_nTextureID;			// Uniquely identifies this texture in all 3D renderers.

		int m_nWidth;
		int m_nHeight;

		bool m_bHasAlpha;

		char m_szName[MAX_PATH];

		void *m_pImageData;
};


#endif // TEXTURE_H
