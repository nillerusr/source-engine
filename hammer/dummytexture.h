//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for placeholder textures.
//			Placeholder textures are used for textures that are referenced in
//			the map file but not found in storage.
//
//=============================================================================//

#ifndef DUMMYTEXTURE_H
#define DUMMYTEXTURE_H
#ifdef _WIN32
#pragma once
#endif


#include <afxtempl.h>
#include "IEditorTexture.h"


enum TEXTUREFORMAT;


class CDummyTexture : public IEditorTexture
{
	public:

		CDummyTexture(const char *pszName, TEXTUREFORMAT eFormat);
		virtual ~CDummyTexture();

		inline const char *GetName() const
		{
			return(m_szName);
		}
		int GetShortName(char *pszName) const;

		int GetKeywords(char *pszKeywords) const;

		void Draw(CDC *pDC, RECT &rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData);

		const char *GetFileName(void) const;

		void GetSize(SIZE &size) const;

		inline bool IsDummy() const
		{
			return(true);
		}

		int GetImageDataRGB( void *pImageRGB );
		int GetImageDataRGBA( void *pImageRGBA );

		inline int GetImageWidth() const
		{
			return(0);
		}

		inline int GetImageHeight() const
		{
			return(0);
		}

		inline float GetDecalScale() const
		{
			return(1.0f);
		}

		CPalette *GetPalette() const
		{
			return(NULL);
		}

		inline int GetWidth() const
		{
			return(0);
		}

		inline int GetHeight() const
		{
			return(0);
		}

		inline int GetTextureID() const
		{
			return(0);
		}

		inline TEXTUREFORMAT GetTextureFormat() const
		{
			return(m_eTextureFormat);
		}

		inline int GetSurfaceAttributes() const
		{
			return(0);
		}

		inline int GetSurfaceContents() const
		{
			return(0);
		}

		inline int GetSurfaceValue() const
		{
			return(0);
		}

		inline bool HasAlpha() const
		{
			return(false);
		}

		inline bool HasData() const
		{
			return(false);
		}

		inline bool HasPalette() const
		{
			return(false);
		}

		bool Load( void );
		void Reload( bool bFullReload ) {}

		inline bool IsLoaded() const
		{
			return true;
		}

		inline void SetTextureFormat(TEXTUREFORMAT eFormat)
		{
			m_eTextureFormat = eFormat;
		}

		inline void SetTextureID( int nTextureID )
		{
		}

		bool IsWater( void ) const
		{
			return false;
		}

	protected:

		char m_szName[MAX_PATH];
		char m_szFileName[MAX_PATH];

		TEXTUREFORMAT m_eTextureFormat;
};


#endif // DUMMYTEXTURE_H
