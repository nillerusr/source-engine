//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all texture state for the material system surface to use
//
//===========================================================================//

#include "bitmap/imageformat.h"
#include "TextureDictionary.h"
#include "utllinkedlist.h"
#include "checksum_crc.h"
#include "materialsystem/imaterial.h"
#include "vguimatsurface.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/dbg.h"
#include "KeyValues.h"
#include "pixelwriter.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TEXTURE_ID_UNKNOWN	-1

class CMatSystemTexture;

// Case-sensitive string checksum
static CRC32_t Texture_CRCName( const char *string )
{
	CRC32_t crc;
	
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, (void *)string, strlen( string ) );
	CRC32_Final( &crc );

	return crc;
}

class CFontTextureRegen;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMatSystemTexture
{
public:
	CMatSystemTexture( void );
	~CMatSystemTexture( void );

	void SetId( int id ) { m_ID = id; };

	CRC32_t	GetCRC() const;
	void SetCRC( CRC32_t val );

	void SetMaterial( const char *pFileName );
	void SetMaterial( IMaterial *pMaterial );

	void SetTexture( ITexture *pTexture ) { SafeAssign( &m_pOverrideTexture, pTexture ); }

	// This is used when we want different rendering state sharing the same procedural texture (fonts)
	void ReferenceOtherProcedural( CMatSystemTexture *pTexture, IMaterial *pMaterial );

	IMaterial *GetMaterial() { return m_pMaterial; }
	int Width() const { return m_iWide; }
	int Height() const { return m_iTall; }

	bool IsProcedural( void ) const;
	void SetProcedural( bool proc );

 	bool IsReference() const { return m_Flags & TEXTURE_IS_REFERENCE; }

	void SetTextureRGBA( const char* rgba, int wide, int tall, ImageFormat format, bool bFixupTextCoordsForDimensions );
	void SetSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall );
	void SetSubTextureRGBAEx( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );
	void UpdateSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );

	float	m_s0, m_t0, m_s1, m_t1;

private:
	void CreateRegen( int nWidth, int nHeight, ImageFormat format );
	void ReleaseRegen( void );
	void CleanUpMaterial();

	ITexture *GetTextureValue( void );

private:
	enum
	{
		TEXTURE_IS_PROCEDURAL = 0x1,
		TEXTURE_IS_REFERENCE = 0x2
	};

	CRC32_t				m_crcFile;
	IMaterial			*m_pMaterial;
	ITexture			*m_pTexture;
	ITexture			*m_pOverrideTexture;

	int					m_iWide;
	int					m_iTall;
	int					m_iInputWide;
	int					m_iInputTall;
	int					m_ID;
	int					m_Flags;
	CFontTextureRegen	*m_pRegen;
};


//-----------------------------------------------------------------------------
// A class that manages textures used by the material system surface
//-----------------------------------------------------------------------------
class CTextureDictionary : public ITextureDictionary
{
public:
	CTextureDictionary( void );

	// Create, destroy textures
	int	CreateTexture( bool procedural = false );
	int CreateTextureByTexture( ITexture *pTexture, bool procedural = true ) OVERRIDE;
	void DestroyTexture( int id );
	void DestroyAllTextures();

	// Is this a valid id?
	bool IsValidId( int id ) const;

	// Binds a material to a texture
	virtual void BindTextureToFile( int id, const char *pFileName );
	virtual void BindTextureToMaterial( int id, IMaterial *pMaterial );
	virtual void BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial );

	// Texture info
	IMaterial *GetTextureMaterial( int id );
	void GetTextureSize(int id, int& iWide, int& iTall );
	void GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 );

	void SetTextureRGBA( int id, const char* rgba, int wide, int tall );
	void SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, bool bFixupTextCoordsForDimensions );
	void SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall );
	void SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );
	void UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );

	int	FindTextureIdForTextureFile( char const *pFileName );

public:
	CMatSystemTexture	*GetTexture( int id );

private:
	CUtlLinkedList< CMatSystemTexture, unsigned short >	m_Textures;
};

static CTextureDictionary s_TextureDictionary;


//-----------------------------------------------------------------------------
// A texture regenerator that holds onto the bits at all times
//-----------------------------------------------------------------------------
class CFontTextureRegen : public ITextureRegenerator
{
public:
	CFontTextureRegen( int nWidth, int nHeight, ImageFormat format )
	{
		m_nFormat = format;
		m_nWidth  = nWidth;
		m_nHeight = nHeight;

		if ( IsPC() )
		{
			int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
			m_pTextureBits = new unsigned char[size];
			memset( m_pTextureBits, 0, size );
		}
		else
		{
			// will be allocated as needed
			m_pTextureBits = NULL;
		}
	}

	~CFontTextureRegen( void )
	{
		DeleteTextureBits();
	}

	void UpdateBackingBits( Rect_t &subRect, const unsigned char *pBits, Rect_t &uploadRect, ImageFormat format )
	{
		int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
		if ( IsPC() )
		{
			if ( !m_pTextureBits )
				return;
		}
		else
		{
			Assert( !m_pTextureBits );
			m_pTextureBits = new unsigned char[size];
			memset( m_pTextureBits, 0, size );
		}

		// Copy subrect into backing bits storage
		// source data is expected to be in same format as backing bits
		int y;
		if ( ImageLoader::SizeInBytes( m_nFormat ) == 4 )
		{
			bool bIsInputFullRect = ( subRect.width != uploadRect.width || subRect.height != uploadRect.height );
			Assert( (subRect.x >= 0) && (subRect.y >= 0) );
			Assert( (subRect.x + subRect.width <= m_nWidth) && (subRect.y + subRect.height <= m_nHeight) );
			for ( y=0; y < subRect.height; ++y )
			{
				int idx = ( (subRect.y + y) * m_nWidth + subRect.x ) << 2;
				unsigned int *pDst = (unsigned int*)(&m_pTextureBits[ idx ]);
				int offset = bIsInputFullRect ? (subRect.y+y)*uploadRect.width + subRect.x : y*uploadRect.width;
				const unsigned int *pSrc = (const unsigned int *)(&pBits[ offset << 2 ]);
				ImageLoader::ConvertImageFormat( (const unsigned char *)pSrc, format,(unsigned char *)pDst, m_nFormat, subRect.width, 1 );
			}
		}
		else
		{
			// cannot subrect copy when format is not RGBA
			if ( subRect.width != m_nWidth || subRect.height != m_nHeight )
			{
				Assert( 0 );
				return;
			}
			Q_memcpy( m_pTextureBits, pBits, size );
		}
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		if ( !m_pTextureBits )
			return;

		Assert( (pVTFTexture->Width() == m_nWidth) && (pVTFTexture->Height() == m_nHeight) );

		int nFormatBytes = ImageLoader::SizeInBytes( m_nFormat );
		if ( nFormatBytes == 4 )
		{
			if ( m_nFormat == pVTFTexture->Format() )
			{
				int ymax = pSubRect->y + pSubRect->height;
				for( int y = pSubRect->y; y < ymax; ++y )
				{
					// copy each row across for the update
					char *pchData = (char *)pVTFTexture->ImageData( 0, 0, 0, 0, y ) + pSubRect->x *nFormatBytes;
					int size = ImageLoader::GetMemRequired( pSubRect->width, 1, 1, m_nFormat, false );
					V_memcpy( pchData, m_pTextureBits + (y * m_nWidth + pSubRect->x) * nFormatBytes, size );
				}
			}
			else
			{
				// formats don't match so do a pixel by pixel swizel
				CPixelWriter pixelWriter;
				pixelWriter.SetPixelMemory( 
					pVTFTexture->Format(), 
					pVTFTexture->ImageData( 0, 0, 0 ), 
					pVTFTexture->RowSizeInBytes( 0 ) );

				// Now upload the part we've been asked for
				int xmax = pSubRect->x + pSubRect->width;
				int ymax = pSubRect->y + pSubRect->height;
				int x, y;

				for( y = pSubRect->y; y < ymax; ++y )
				{
 					pixelWriter.Seek( pSubRect->x, y );
					unsigned char *rgba = &m_pTextureBits[ (y * m_nWidth + pSubRect->x) * nFormatBytes ];

					for ( x=pSubRect->x; x < xmax; ++x )
					{
						pixelWriter.WritePixel( rgba[0], rgba[1], rgba[2], rgba[3] );
						rgba += nFormatBytes;
					}
				}
			}
		}
		else
		{
			// cannot subrect copy when format is not RGBA
			if ( pSubRect->width != m_nWidth || pSubRect->height != m_nHeight )
			{
				Assert( 0 );
				return;
			}
			int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
			Q_memcpy( pVTFTexture->ImageData( 0, 0, 0 ), m_pTextureBits, size );
		}
	}

	virtual void Release()
	{
		// Called by the material system when this needs to go away
		DeleteTextureBits();
	}

	void DeleteTextureBits()
	{
		if ( m_pTextureBits )
		{
			delete [] m_pTextureBits;
			m_pTextureBits = NULL;
		}
	}

private:
	unsigned char	*m_pTextureBits;
	short			m_nWidth;
	short			m_nHeight;
	ImageFormat		m_nFormat;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatSystemTexture::CMatSystemTexture( void )
{
	m_pMaterial = NULL;
	m_pTexture = NULL;
	m_pOverrideTexture = NULL;
	m_crcFile = (CRC32_t)0;
	m_iWide = m_iTall = 0;
	m_s0 = m_t0 = 0;
	m_s1 = m_t1 = 1;

	m_Flags = 0;
	m_pRegen = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatSystemTexture::~CMatSystemTexture( void )
{
	if ( m_pOverrideTexture )
	{
		m_pOverrideTexture->Release();
		m_pOverrideTexture->DeleteIfUnreferenced();
		m_pOverrideTexture = NULL;
	}

	CleanUpMaterial();
}

bool CMatSystemTexture::IsProcedural( void ) const
{
	return (m_Flags & TEXTURE_IS_PROCEDURAL) != 0;
}

void CMatSystemTexture::SetProcedural( bool proc )
{
	if (proc)
	{
		m_Flags |= TEXTURE_IS_PROCEDURAL;
	}
	else
	{
		m_Flags &= ~TEXTURE_IS_PROCEDURAL;
	}
}

void CMatSystemTexture::CleanUpMaterial()
{
	if ( m_pMaterial )
	{
		// causes the underlying texture (if unreferenced) to be deleted as well
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial->DeleteIfUnreferenced();
		m_pMaterial = NULL;
	}

	if ( m_pTexture )
	{
		m_pTexture->SetTextureRegenerator( NULL );
		m_pTexture->DecrementReferenceCount();
		m_pTexture->DeleteIfUnreferenced();
		m_pTexture = NULL;
	}

	ReleaseRegen();
}

void CMatSystemTexture::CreateRegen( int nWidth, int nHeight, ImageFormat format )
{
	Assert( IsProcedural() );

	if ( !m_pRegen )
	{
		m_pRegen = new CFontTextureRegen( nWidth, nHeight, format );
	}
}

void CMatSystemTexture::ReleaseRegen( void )
{
	if (m_pRegen)
	{
		if (!IsReference())
		{
			delete m_pRegen;
		}

		m_pRegen = NULL;
	}
}

void CMatSystemTexture::SetTextureRGBA( const char *rgba, int wide, int tall, ImageFormat format, bool bFixupTextCoords )
{
	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	if ( !m_pMaterial )
	{
		int width = wide;
		int height = tall;

		// find the minimum power-of-two to fit this in
		int i;
		for ( i = 0; i < 32 ; i++ )
		{
			width = (1<<i);
			if (width >= wide)
			{
				break;
			}
		}

		for (i = 0; i < 32; i++ )
		{
			height= (1<<i);
			if (height >= tall)
			{
				break;
			}
		}

		// create a procedural material to fit this texture into
		static int nTextureId = 0;
		char pTextureName[64];
		Q_snprintf( pTextureName, sizeof( pTextureName ), "__vgui_texture_%d", nTextureId );
		++nTextureId;

		ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( 
			pTextureName, 
			TEXTURE_GROUP_VGUI, 
			width,
			height,
			format,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | 
			TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_POINTSAMPLE );

		KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$vertexalpha", 1 );
		pVMTKeyValues->SetInt( "$ignorez", 1 );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
		pVMTKeyValues->SetString( "$basetexture", pTextureName );

		IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( pTextureName, pVMTKeyValues );
		pMaterial->Refresh();

		// Has to happen after the refresh
		pTexture->DecrementReferenceCount();

		SetMaterial( pMaterial );
		m_iInputTall = tall;
		m_iInputWide = wide;
		if ( bFixupTextCoords && ( wide != width || tall != height ) )
		{
			m_s1 = (double)wide / width;
			m_t1 = (double)tall / height;
		}

		// undo the extra +1 refCount
		pMaterial->DecrementReferenceCount();
	}

	Assert( wide <= m_iWide );
	Assert( tall <= m_iTall );

	// Just replace the whole thing
	SetSubTextureRGBAEx( 0, 0, (const unsigned char *)rgba, wide, tall, format );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ITexture *CMatSystemTexture::GetTextureValue( void )
{
	Assert( IsProcedural() );
	if ( !m_pMaterial )
		return NULL;

	if ( m_pOverrideTexture )
		return m_pOverrideTexture;

	return m_pTexture;
}

void CMatSystemTexture::SetSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	SetSubTextureRGBAEx( drawX, drawY, rgba, subTextureWide, subTextureTall, IMAGE_FORMAT_RGBA8888 );
}

void CMatSystemTexture::SetSubTextureRGBAEx( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format )
{
		ITexture *pTexture = GetTextureValue();
	if ( !pTexture )
		return;

	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	Assert( drawX < m_iWide );
	Assert( drawY < m_iTall );
	Assert( drawX + subTextureWide <= m_iWide );
	Assert( drawY + subTextureTall <= m_iTall );

	Assert( m_pRegen );

	Assert( rgba );

	Rect_t subRect;
	subRect.x = drawX;
	subRect.y = drawY;
	subRect.width = subTextureWide;
	subRect.height = subTextureTall;

	Rect_t textureSize;
	textureSize.x = 0;
	textureSize.y = 0;
	textureSize.width = subTextureWide;
	textureSize.height = subTextureTall;

	
	m_pRegen->UpdateBackingBits( subRect, rgba, textureSize, format );
	pTexture->Download( &subRect );

	if ( IsX360() )
	{	
		// xboxissue - no need to persist "backing bits", saves memory
		// the texture (commonly font page) "backing bits" are allocated during UpdateBackingBits() which get blitted
		// into by procedural regeneration in preparation for download() which then subrect blits
		// out of and into target texture (d3d upload)
		// the "backing bits" are then no longer required
		m_pRegen->DeleteTextureBits();
	}
}

void CMatSystemTexture::UpdateSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat )
{
	ITexture *pTexture = GetTextureValue();
	if ( !pTexture )
		return;

	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	Assert( drawX < m_iWide );
	Assert( drawY < m_iTall );
	Assert( drawX + subTextureWide <= m_iWide );
	Assert( drawY + subTextureTall <= m_iTall );

	Assert( m_pRegen );

	Assert( rgba );

	Rect_t subRect;
	subRect.x = drawX;
	subRect.y = drawY;
	subRect.width = subTextureWide;
	subRect.height = subTextureTall;

	Rect_t textureSize;
	textureSize.x = 0;
	textureSize.y = 0;
	textureSize.width = m_iInputWide;
	textureSize.height = m_iInputTall;

	m_pRegen->UpdateBackingBits( subRect, rgba, textureSize, imageFormat );
	pTexture->Download( &subRect );

	if ( IsX360() )
	{	
		// xboxissue - no need to persist "backing bits", saves memory
		// the texture (commonly font page) "backing bits" are allocated during UpdateBackingBits() which get blitted
		// into by procedural regeneration in preparation for download() which then subrect blits
		// out of and into target texture (d3d upload)
		// the "backing bits" are then no longer required
		m_pRegen->DeleteTextureBits();
	}
}



void CMatSystemTexture::SetCRC( CRC32_t val )
{
	m_crcFile = val;
}

CRC32_t CMatSystemTexture::GetCRC() const
{ 
	return m_crcFile; 
}

void CMatSystemTexture::SetMaterial( IMaterial *pMaterial )
{
	// Decrement references to old texture
	CleanUpMaterial();

	m_pMaterial = pMaterial;

	if (!m_pMaterial)
	{
		m_iWide = m_iTall = 0;
		m_s0 = m_t0 = 0.0f;
		m_s1 = m_t1 = 1.0f;
		return;
	}

	// Increment its reference count
	m_pMaterial->IncrementReferenceCount();

	// Compute texture size
	m_iWide = m_pMaterial->GetMappingWidth();
	m_iTall = m_pMaterial->GetMappingHeight();

	// Compute texture coordinates
	float flPixelCenterX = 0.0f;
	float flPixelCenterY = 0.0f;

	if ( m_iWide > 0.0f && m_iTall > 0.0f)
	{
		flPixelCenterX = 0.5f / m_iWide;
		flPixelCenterY = 0.5f / m_iTall;
	}

	m_s0 = flPixelCenterX;
	m_t0 = flPixelCenterY;

	// FIXME: Old code used +, it should be - yes?!??!
	m_s1 = 1.0 - flPixelCenterX;
	m_t1 = 1.0 - flPixelCenterY;

	if ( IsProcedural() )
	{
		bool bFound;
		IMaterialVar *tv = m_pMaterial->FindVar( "$baseTexture", &bFound );
		if ( bFound && tv->IsTexture() )
		{
			m_pTexture = tv->GetTextureValue();
			if ( m_pTexture )
			{
				m_pTexture->IncrementReferenceCount();

				// Upload new data
				CreateRegen( m_iWide, m_iTall, m_pTexture->GetImageFormat() );
				m_pTexture->SetTextureRegenerator( m_pRegen );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// This is used when we want different rendering state sharing the same procedural texture (fonts)
//-----------------------------------------------------------------------------
void CMatSystemTexture::ReferenceOtherProcedural( CMatSystemTexture *pTexture, IMaterial *pMaterial )
{
	// Decrement references to old texture
	CleanUpMaterial();

	Assert( pTexture->IsProcedural() );

	m_Flags |= TEXTURE_IS_REFERENCE;

	m_pMaterial = pMaterial;

	if (!m_pMaterial)
	{
		m_iWide = m_iTall = 0;
		m_s0 = m_t0 = 0.0f;
		m_s1 = m_t1 = 1.0f;
		return;
	}

	m_iWide = pTexture->m_iWide;
	m_iTall = pTexture->m_iTall;
	m_s0 = pTexture->m_s0;
	m_t0 = pTexture->m_t0;
	m_s1 = pTexture->m_s1;
	m_t1 = pTexture->m_t1;

	Assert( (pMaterial->GetMappingWidth() == m_iWide) && (pMaterial->GetMappingHeight() == m_iTall) );

	// Increment its reference count
	m_pMaterial->IncrementReferenceCount();

	bool bFound;
	IMaterialVar *tv = m_pMaterial->FindVar( "$baseTexture", &bFound );
	if ( bFound )
	{
		m_pTexture = tv->GetTextureValue();
		if ( m_pTexture )
		{
			m_pTexture->IncrementReferenceCount();
			Assert( m_pTexture == pTexture->m_pTexture );

			// Reference, but do *not* create a new one!!!
			m_pRegen = pTexture->m_pRegen;
		}
	}
}

void CMatSystemTexture::SetMaterial( const char *pFileName )
{
	// Get a pointer to the new material
	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( pFileName, TEXTURE_GROUP_VGUI );

	if ( IsErrorMaterial( pMaterial ) )
	{
		if (IsOSX())
		{
			printf( "\n ##### Missing Vgui material %s\n", pFileName );
		}
		Msg( "--- Missing Vgui material %s\n", pFileName );
	}

	SetMaterial( pMaterial );
}

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
ITextureDictionary *TextureDictionary()
{
	return &s_TextureDictionary;
}

CTextureDictionary::CTextureDictionary( void )
{
	// First entry is bogus texture
	m_Textures.AddToTail();
}

//-----------------------------------------------------------------------------
// Create, destroy textures
//-----------------------------------------------------------------------------
int	CTextureDictionary::CreateTexture( bool procedural /*=false*/ )
{
	int idx = m_Textures.AddToTail();
	CMatSystemTexture &texture = m_Textures[idx];
	texture.SetProcedural( procedural );
	texture.SetId( idx );

	return idx;
}

int CTextureDictionary::CreateTextureByTexture( ITexture *pTexture, bool procedural /*= true*/ )
{
	int idx = m_Textures.AddToTail();
	CMatSystemTexture &texture = m_Textures[idx];
	texture.SetProcedural( procedural );
	texture.SetId( idx );
	texture.SetTexture( pTexture );

	return idx;
}

void CTextureDictionary::DestroyTexture( int id )
{
	if ( m_Textures.Count() <= 1 )
	{
		// TextureDictionary already destroyed all the textures before this (something destructed late in shutdown)
		return;
	}

	if (id != INVALID_TEXTURE_ID)
	{
		Assert( id != m_Textures.InvalidIndex() );
		m_Textures.Remove( (unsigned short)id );
	}
}

void CTextureDictionary::DestroyAllTextures()
{
	m_Textures.RemoveAll();
	// First entry is bogus texture
	m_Textures.AddToTail();	
	CMatSystemTexture &texture = m_Textures[0];
	texture.SetId( 0 );
}

void CTextureDictionary::SetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	SetTextureRGBAEx( id, rgba, wide, tall, IMAGE_FORMAT_RGBA8888, false );
}

void CTextureDictionary::SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, bool bFixupTextCoordsForDimensions )
{
	if (!IsValidId(id))
	{
		Msg( "SetTextureRGBA: Invalid texture id %i\n", id );
		return;
	}
	CMatSystemTexture &texture = m_Textures[id];
	texture.SetTextureRGBA( rgba, wide, tall, format, bFixupTextCoordsForDimensions );
}

void CTextureDictionary::SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	SetSubTextureRGBAEx( id, drawX, drawY, rgba, subTextureWide, subTextureTall, IMAGE_FORMAT_RGBA8888 );
}


void CTextureDictionary::SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format )
{
	if (!IsValidId(id))
	{
		Msg( "SetSubTextureRGBA: Invalid texture id %i\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	texture.SetSubTextureRGBAEx( drawX, drawY, rgba, subTextureWide, subTextureTall, format );
}


void CTextureDictionary::UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat )
{
	if (!IsValidId(id))
	{
		Msg( "UpdateSubTextureRGBA: Invalid texture id %i\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	texture.UpdateSubTextureRGBA( drawX, drawY, rgba, subTextureWide, subTextureTall, imageFormat  );
}


//-----------------------------------------------------------------------------
// Returns true if the id is valid
//-----------------------------------------------------------------------------
bool CTextureDictionary::IsValidId( int id ) const
{
	Assert( id != 0 );
	if ( id == 0 )
		return false;

	return m_Textures.IsValidIndex( id );
}


//-----------------------------------------------------------------------------
// Binds a file to a particular texture
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToFile( int id, const char *pFileName )
{
	if (!IsValidId(id))
	{
		Msg( "BindTextureToFile: Invalid texture id for file %s\n", pFileName );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];

	// Reload from file if the material was never loaded, or if the filename has changed at all
	CRC32_t fileNameCRC = Texture_CRCName( pFileName );
	if ( !texture.GetMaterial() || fileNameCRC != texture.GetCRC() )
	{
		// New texture name
		texture.SetCRC( fileNameCRC );
		texture.SetMaterial( pFileName );
	}
}


//-----------------------------------------------------------------------------
// Binds a material to a texture
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToMaterial( int id, IMaterial *pMaterial )
{
	if (!IsValidId(id))
	{
		Msg( "BindTextureToFile: Invalid texture id %d\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	texture.SetMaterial( pMaterial );
}


//-----------------------------------------------------------------------------
// Binds a material to a texture reference
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial )
{
	if (!IsValidId(id) || !IsValidId(referenceId))
	{
		Msg( "BindTextureToFile: Invalid texture ids %d %d\n", id, referenceId );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	CMatSystemTexture &textureSource = m_Textures[referenceId];
	texture.ReferenceOtherProcedural( &textureSource, pMaterial );
}


//-----------------------------------------------------------------------------
// Returns the material associated with an id
//-----------------------------------------------------------------------------
IMaterial *CTextureDictionary::GetTextureMaterial( int id )
{
	if (!IsValidId(id))
		return NULL;

	return m_Textures[id].GetMaterial();
}


//-----------------------------------------------------------------------------
// Returns the material size associated with an id
//-----------------------------------------------------------------------------
void CTextureDictionary::GetTextureSize(int id, int& iWide, int& iTall )
{
	if (!IsValidId(id))
	{
		iWide = iTall = 0;
		return;
	}

	iWide = m_Textures[id].Width();
	iTall = m_Textures[id].Height();
}

void CTextureDictionary::GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 )
{
	if (!IsValidId(id))
	{
		s0 = t0 = 0.0f;
		s1 = t1 = 1.0f;
		return;
	}

	s0 = m_Textures[id].m_s0;
	t0 = m_Textures[id].m_t0;
	s1 = m_Textures[id].m_s1;
	t1 = m_Textures[id].m_t1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
// Output : CMatSystemTexture
//-----------------------------------------------------------------------------
CMatSystemTexture *CTextureDictionary::GetTexture( int id )
{
	if (!IsValidId(id))
		return NULL;

	return &m_Textures[ id ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
int	CTextureDictionary::FindTextureIdForTextureFile( char const *pFileName )
{
	for ( int i = m_Textures.Head(); i != m_Textures.InvalidIndex(); i = m_Textures.Next( i ) )
	{
		CMatSystemTexture *tex = &m_Textures[i];
		if ( !tex )
			continue;

		IMaterial *mat = tex->GetMaterial();
		if ( !mat )
			continue;

		if ( !stricmp( mat->GetName(), pFileName ) )
			return i;
	}

	return -1;
}
