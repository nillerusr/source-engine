//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all texture state for the material system surface to use
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTUREDICTIONARY_H
#define TEXTUREDICTIONARY_H

class IMaterial;
class ITexture;

enum
{
	INVALID_TEXTURE_ID = -1
};


//-----------------------------------------------------------------------------
// A class that manages textures used by the material system surface
//-----------------------------------------------------------------------------
class ITextureDictionary
{
public:
	// Create, destroy textures
	virtual int	CreateTexture( bool procedural = false ) = 0;
	virtual int CreateTextureByTexture( ITexture *pTexture, bool procedural = true ) = 0;
	virtual void DestroyTexture( int id ) = 0;
	virtual void DestroyAllTextures() = 0;

	// Is this a valid id?
	virtual bool IsValidId( int id ) const = 0;

	// Binds a material to a texture
	virtual void BindTextureToFile( int id, const char *pFileName ) = 0;

	// Binds a material to a texture
	virtual void BindTextureToMaterial( int id, IMaterial *pMaterial ) = 0;

	// Binds a material to a texture
	virtual void BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial ) = 0;

	// Texture info
	virtual IMaterial *GetTextureMaterial( int id ) = 0;
	virtual void GetTextureSize(int id, int& iWide, int& iTall ) = 0;
	virtual void GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 ) = 0;

	virtual void SetTextureRGBA( int id, const char* rgba, int wide, int tall ) = 0;

	virtual int	FindTextureIdForTextureFile( char const *pFileName ) = 0;
	virtual void SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall ) = 0;
	virtual void SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format ) = 0;

	virtual void SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, bool bFixupTextCoordsForDimensions ) = 0;

	virtual void UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat ) = 0;
};

ITextureDictionary *TextureDictionary();

#endif // TEXTUREDICTIONARY_H