//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of the IEditorTexture interface for WAD textures.
//
//=============================================================================//

#ifndef WADTEXTURE_H
#define WADTEXTURE_H
#ifdef _WIN32
#pragma once
#endif


#include <afxtempl.h>
#include "BlockArray.h"
#include "IEditorTexture.h"


class IMaterial;


class CWADTexture : public IEditorTexture
{
	public:

		CWADTexture(void);
		virtual ~CWADTexture(void);

		static bool Initialize(void);
		static void ShutDown(void);

		BOOL Init(int, DWORD, BOOL, LPCTSTR);

		BOOL AdjustTexture(char *pLoadBuf);

		inline const char *GetName(void) const
		{
			return(m_szName);
		}
		int GetShortName(char *pszName) const;

		int GetKeywords(char *pszKeywords) const;

		void Draw(CDC *pDC, RECT &rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData);//, DWORD dwFlags = (drawCaption|drawIcons));
		void GetSize(SIZE &size);

		const char *GetFileName(void) const;

		int GetImageDataRGB( void *pImageRGB );
		int GetImageDataRGBA( void *pImageRGBA );

		inline int GetImageWidth() const
		{
			return( m_datawidth );
		}

		inline int GetImageHeight() const
		{
			return( m_dataheight );
		}

		inline int GetWidth() const
		{
			return(m_nWidth);
		}

		inline int GetHeight() const
		{
			return(m_nHeight);
		}

		inline float GetDecalScale() const
		{
			return( 1.0f );
		}
		
		CPalette *GetPalette() const;

		inline int GetTextureID() const
		{
			return( m_nTextureID );
		}
		
		inline TEXTUREFORMAT GetTextureFormat() const
		{
			return(format);
		}

		inline int GetSurfaceAttributes() const
		{
			return(m_WALsurface);
		}

		inline int GetSurfaceContents() const
		{
			return(m_WALcontents);
		}

		inline int GetSurfaceValue() const
		{
			return(m_WALvalue);
		}

		inline bool HasAlpha() const
		{
			return( false );
		}

		inline bool HasData( void ) const
		{
			return(m_pData != NULL);
		}

		inline bool HasPalette() const
		{
			return(m_bLocalPalette == TRUE);
		}

		inline bool IsDummy( void ) const
		{
			return( false );
		}

		bool Load( void );
		void Reload( bool bFullReload ) {}
		bool IsLoaded() const;

		inline void SetTextureFormat(TEXTUREFORMAT eFormat)
		{
			format = eFormat;
		}

		inline void SetTextureID( int nTextureID )
		{
			m_nTextureID = nTextureID;
		}

		bool IsWater( void ) const
		{
			return false;
		}

	protected:

		BOOL Load(int fd, HANDLE hFile);
		void DrawNoImage(CDC *pDC, RECT &rect, int iFontHeight);

		char m_szName[MAX_PATH];
		char m_szFileName[MAX_PATH];

		// additional data for new .WAL textures:
		int m_WALsurface;
		int m_WALvalue;
		int m_WALcontents;

		// Used when the texture is in a .WAD or a .PAK file.
		// Otherwise, texture is loaded automatically.
		DWORD m_ulFileOffset;		// Offset to texture in WAD file.
		DWORD m_ulFileID;			// ID of WAD file the texture is in.

		TEXTUREFORMAT format;

		LOGPALETTE *m_pPalette;		// This texture's palette.
		BOOL m_bLocalPalette;		// Use m_pPalette?

		int m_nTextureID;			// Uniquely identifies this texture in all 3D renderers.

		int m_datawidth;
		int m_dataheight;

		int m_nWidth;
		int m_nHeight;

		void *m_pData;				// Loaded pixel data (NULL if not loaded)
};


#endif // WADTEXTURE_H
