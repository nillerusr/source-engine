//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#undef PROTECTED_THINGS_ENABLE
#include "vertexdecl.h" // this includes <windows.h> inside the dx headers
#define PROTECTED_THINGS_ENABLE
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "shaderapidx8_global.h"
#include "tier0/dbg.h"
#include "utlrbtree.h"
#include "recording.h"
#include "tier1/strtools.h"
#include "tier0/vprof.h"
#include "materialsystem/imesh.h"
#include "shaderdevicedx8.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Computes the DX8 vertex specification
//-----------------------------------------------------------------------------
static const char *DeclTypeToString( BYTE type )
{
	switch( type )
	{
	case D3DDECLTYPE_FLOAT1:
		return "D3DDECLTYPE_FLOAT1";
	case D3DDECLTYPE_FLOAT2:
		return "D3DDECLTYPE_FLOAT2";
	case D3DDECLTYPE_FLOAT3:
		return "D3DDECLTYPE_FLOAT3";
	case D3DDECLTYPE_FLOAT4:
		return "D3DDECLTYPE_FLOAT4";
	case D3DDECLTYPE_D3DCOLOR:
		return "D3DDECLTYPE_D3DCOLOR";
	case D3DDECLTYPE_UBYTE4:
		return "D3DDECLTYPE_UBYTE4";
	case D3DDECLTYPE_SHORT2:
		return "D3DDECLTYPE_SHORT2";
	case D3DDECLTYPE_SHORT4:
		return "D3DDECLTYPE_SHORT4";
	case D3DDECLTYPE_UBYTE4N:
		return "D3DDECLTYPE_UBYTE4N";
	case D3DDECLTYPE_SHORT2N:
		return "D3DDECLTYPE_SHORT2N";
	case D3DDECLTYPE_SHORT4N:
		return "D3DDECLTYPE_SHORT4N";
	case D3DDECLTYPE_USHORT2N:
		return "D3DDECLTYPE_USHORT2N";
	case D3DDECLTYPE_USHORT4N:
		return "D3DDECLTYPE_USHORT4N";
	case D3DDECLTYPE_UDEC3:
		return "D3DDECLTYPE_UDEC3";
	case D3DDECLTYPE_DEC3N:
		return "D3DDECLTYPE_DEC3N";
	case D3DDECLTYPE_FLOAT16_2:
		return "D3DDECLTYPE_FLOAT16_2";
	case D3DDECLTYPE_FLOAT16_4:
		return "D3DDECLTYPE_FLOAT16_4";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static const char *DeclMethodToString( BYTE method )
{
	switch( method )
	{
	case D3DDECLMETHOD_DEFAULT:
		return "D3DDECLMETHOD_DEFAULT";
	case D3DDECLMETHOD_PARTIALU:
		return "D3DDECLMETHOD_PARTIALU";
	case D3DDECLMETHOD_PARTIALV:
		return "D3DDECLMETHOD_PARTIALV";
	case D3DDECLMETHOD_CROSSUV:
		return "D3DDECLMETHOD_CROSSUV";
	case D3DDECLMETHOD_UV:
		return "D3DDECLMETHOD_UV";
	case D3DDECLMETHOD_LOOKUP:
		return "D3DDECLMETHOD_LOOKUP";
	case D3DDECLMETHOD_LOOKUPPRESAMPLED:
		return "D3DDECLMETHOD_LOOKUPPRESAMPLED";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static const char *DeclUsageToString( BYTE usage )
{
	switch( usage )
	{
	case D3DDECLUSAGE_POSITION:
		return "D3DDECLUSAGE_POSITION";
	case D3DDECLUSAGE_BLENDWEIGHT:
		return "D3DDECLUSAGE_BLENDWEIGHT";
	case D3DDECLUSAGE_BLENDINDICES:
		return "D3DDECLUSAGE_BLENDINDICES";
	case D3DDECLUSAGE_NORMAL:
		return "D3DDECLUSAGE_NORMAL";
	case D3DDECLUSAGE_PSIZE:
		return "D3DDECLUSAGE_PSIZE";
	case D3DDECLUSAGE_COLOR:
		return "D3DDECLUSAGE_COLOR";
	case D3DDECLUSAGE_TEXCOORD:
		return "D3DDECLUSAGE_TEXCOORD";
	case D3DDECLUSAGE_TANGENT:
		return "D3DDECLUSAGE_TANGENT";
	case D3DDECLUSAGE_BINORMAL:
		return "D3DDECLUSAGE_BINORMAL";
	case D3DDECLUSAGE_TESSFACTOR:
		return "D3DDECLUSAGE_TESSFACTOR";
//	case D3DDECLUSAGE_POSITIONTL:
//		return "D3DDECLUSAGE_POSITIONTL";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static D3DDECLTYPE VertexElementToDeclType( VertexElement_t element, VertexCompressionType_t compressionType )
{
	Detect_VertexElement_t_Changes( element );

	if ( compressionType == VERTEX_COMPRESSION_ON )
	{
		// Compressed-vertex element sizes
		switch ( element )
		{
#if		( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_SEPARATETANGENTS_SHORT2 )
			case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_SHORT2;
			case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_SHORT2;
#else //( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_COMBINEDTANGENTS_UBYTE4 ) 
			case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_UBYTE4;
			case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_UBYTE4;
#endif
			case VERTEX_ELEMENT_BONEWEIGHTS1:	return D3DDECLTYPE_SHORT2;
			case VERTEX_ELEMENT_BONEWEIGHTS2:	return D3DDECLTYPE_SHORT2;
			default:
				break;
		}
	}

	// Uncompressed-vertex element sizes
	switch ( element )
	{
		case VERTEX_ELEMENT_POSITION:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_COLOR:			return D3DDECLTYPE_D3DCOLOR;
		case VERTEX_ELEMENT_SPECULAR:		return D3DDECLTYPE_D3DCOLOR;
		case VERTEX_ELEMENT_TANGENT_S:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TANGENT_T:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_WRINKLE:
			// Wrinkle is packed into Position.W, it is not specified as a separate vertex element
			Assert( 0 );
			return D3DDECLTYPE_UNUSED;
#if !defined( _X360 )
		case VERTEX_ELEMENT_BONEINDEX:		return D3DDECLTYPE_D3DCOLOR;
#else
		// UBYTE4 comes in as [0,255] in the shader, which is ideal for bone indices
		// (unfortunately, UBYTE4 is not universally supported on PC DX8 GPUs)
		case VERTEX_ELEMENT_BONEINDEX:		return D3DDECLTYPE_UBYTE4;
#endif
		case VERTEX_ELEMENT_BONEWEIGHTS1:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_BONEWEIGHTS2:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_BONEWEIGHTS3:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_BONEWEIGHTS4:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_USERDATA1:		return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_USERDATA2:		return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_USERDATA3:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD1D_0:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_1:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_2:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_3:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_4:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_5:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_6:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_7:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD2D_0:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_1:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_2:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_3:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_4:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_5:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_6:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_7:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD3D_0:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_1:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_2:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_3:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_4:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_5:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_6:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_7:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD4D_0:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_1:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_2:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_3:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_4:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_5:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_6:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_7:	return D3DDECLTYPE_FLOAT4;
		default:
			Assert(0);
			return D3DDECLTYPE_UNUSED;
	};
}

void PrintVertexDeclaration( const D3DVERTEXELEMENT9 *pDecl )
{
	int i;
	static D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
	for ( i = 0; ; i++ )
	{
		if ( memcmp( &pDecl[i], &declEnd, sizeof( declEnd ) ) == 0 )
		{
			Warning( "D3DDECL_END\n" );
			break;
		}
		Msg( "%d: Stream: %d, Offset: %d, Type: %s, Method: %s, Usage: %s, UsageIndex: %d\n",
			i, ( int )pDecl[i].Stream, ( int )pDecl[i].Offset,
			DeclTypeToString( pDecl[i].Type ),
			DeclMethodToString( pDecl[i].Method ),
			DeclUsageToString( pDecl[i].Usage ),
			( int )pDecl[i].UsageIndex );
	}
}

//-----------------------------------------------------------------------------
// Converts format to a vertex decl
//-----------------------------------------------------------------------------
void ComputeVertexSpec( VertexFormat_t fmt, D3DVERTEXELEMENT9 *pDecl, bool bStaticLit, bool bUsingFlex, bool bUsingMorph )
{
	int i = 0;
	int offset = 0;

	VertexCompressionType_t compressionType = CompressionType( fmt );

	if ( IsX360() )
	{
		// On 360, there's a performance penalty for reading more than 2 streams in the vertex shader
		// (we don't do this yet, but we should be aware if we start doing it)
#ifdef _DEBUG
		int numStreams = 1 + ( bStaticLit ? 1 : 0 ) + ( bUsingFlex ? 1 : 0 ) + ( bUsingMorph ? 1 : 0 );
		Assert( numStreams <= 2 );
#endif
	}

	if ( fmt & VERTEX_POSITION )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_POSITION, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_POSITION, compressionType );
		++i;
	}

	int numBones = NumBoneWeights(fmt);
	if ( numBones > 0 )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_BLENDWEIGHT;
		pDecl[i].UsageIndex = 0;

		// Always exactly two weights
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_BONEWEIGHTS2, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEWEIGHTS2, compressionType );
		++i;
	}

	if ( fmt & VERTEX_BONE_INDEX )
	{
		// this isn't FVF!!!!!
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_BLENDINDICES;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_BONEINDEX, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEINDEX, compressionType );
		++i;
	}

	int normalOffset = -1;
	if ( fmt & VERTEX_NORMAL )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		normalOffset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_NORMAL;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_NORMAL, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_NORMAL, compressionType );
		++i;
	}

	if ( fmt & VERTEX_COLOR )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_COLOR;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_COLOR, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_COLOR, compressionType );
		++i;
	}

	if ( fmt & VERTEX_SPECULAR )
	{
		Assert( !bStaticLit );
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_COLOR;
		pDecl[i].UsageIndex = 1; // SPECULAR goes in the second COLOR slot
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_SPECULAR, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_SPECULAR, compressionType );
		++i;
	}

	VertexElement_t texCoordDimensions[4] = {	VERTEX_ELEMENT_TEXCOORD1D_0,
												VERTEX_ELEMENT_TEXCOORD2D_0,
												VERTEX_ELEMENT_TEXCOORD3D_0,
												VERTEX_ELEMENT_TEXCOORD4D_0 };
	for ( int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j )
	{
		int nCoordSize = TexCoordSize( j, fmt );
		if ( nCoordSize <= 0 )
			continue;
		Assert( nCoordSize <= 4 );

		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = j;
		VertexElement_t texCoordElement = (VertexElement_t)( texCoordDimensions[ nCoordSize - 1 ] + j );
		pDecl[i].Type = VertexElementToDeclType( texCoordElement, compressionType );
		offset += GetVertexElementSize( texCoordElement, compressionType );
		++i;
	}

	if ( fmt & VERTEX_TANGENT_S )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TANGENT;
		pDecl[i].UsageIndex = 0;
		// NOTE: this is currently *not* compressed
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_TANGENT_S, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_S, compressionType );
		++i;
	}

	if ( fmt & VERTEX_TANGENT_T )
	{
		pDecl[i].Stream = 0;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage =   D3DDECLUSAGE_BINORMAL;
		pDecl[i].UsageIndex = 0;
		// NOTE: this is currently *not* compressed
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_TANGENT_T, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_T, compressionType );
		++i;
	}

	int userDataSize = UserDataSize(fmt);
	if ( userDataSize > 0 )
	{
		Assert( userDataSize == 4 ); // This is actually only ever used for tangents
		pDecl[i].Stream = 0;
		if ( ( compressionType == VERTEX_COMPRESSION_ON ) &&
			 ( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_COMBINEDTANGENTS_UBYTE4 ) )
		{
			// FIXME: Normals and tangents are packed together into a single UBYTE4 element,
			//        so just point this back at the same data while we're testing UBYTE4 out.
			pDecl[i].Offset = normalOffset;
		}
		else
		{
			pDecl[i].Offset = offset;
		}
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TANGENT;
		pDecl[i].UsageIndex = 0;
		VertexElement_t userDataElement = (VertexElement_t)( VERTEX_ELEMENT_USERDATA1 + ( userDataSize - 1 ) );
		pDecl[i].Type = VertexElementToDeclType( userDataElement, compressionType );
		offset += GetVertexElementSize( userDataElement, compressionType );
		++i;
	}

	if ( bStaticLit )
	{
		// force stream 1 to have specular color in it, which is used for baked static lighting
		pDecl[i].Stream = 1;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_COLOR;
		pDecl[i].UsageIndex = 1; // SPECULAR goes into the second COLOR slot
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_SPECULAR, compressionType );
		++i;
	}

	if ( HardwareConfig()->SupportsVertexAndPixelShaders() )
	{
		// FIXME: There needs to be a better way of doing this
		// In 2.0b, assume position is 4d, storing wrinkle in pos.w.
		bool bUseWrinkle = HardwareConfig()->SupportsPixelShaders_2_b();

		// Force stream 2 to have flex deltas in it
		pDecl[i].Stream = 2;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 1;
		// FIXME: unify this with VertexElementToDeclType():
		pDecl[i].Type = bUseWrinkle ? D3DDECLTYPE_FLOAT4 : D3DDECLTYPE_FLOAT3;
		++i;

		int normalOffset = GetVertexElementSize( VERTEX_ELEMENT_POSITION, compressionType );
		if ( bUseWrinkle )
		{
			normalOffset += GetVertexElementSize( VERTEX_ELEMENT_WRINKLE, compressionType );
		}

		// Normal deltas
		pDecl[i].Stream = 2;
		pDecl[i].Offset = normalOffset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_NORMAL;
		pDecl[i].UsageIndex = 1;
		// NOTE: this is currently *not* compressed
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_NORMAL, VERTEX_COMPRESSION_NONE );
		++i;
	}

	if ( bUsingMorph )
	{
		// force stream 3 to have vertex index in it, which is used for doing vertex texture reads
		pDecl[i].Stream = 3;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 2;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_USERDATA1, compressionType );
		++i;
	}

	static D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
	pDecl[i] = declEnd;

	//PrintVertexDeclaration( pDecl );
}

//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
struct VertexDeclLookup_t
{
	enum LookupFlags_t
	{
		STATIC_LIT = 0x1,
		USING_MORPH = 0x2,
		USING_FLEX = 0x4,
	};

	VertexFormat_t				m_VertexFormat;
	int							m_nFlags;
	IDirect3DVertexDeclaration9 *m_pDecl;

	bool operator==( const VertexDeclLookup_t &src ) const
	{
		return ( m_VertexFormat == src.m_VertexFormat ) && ( m_nFlags == src.m_nFlags );
	}
};


//-----------------------------------------------------------------------------
// Dictionary of vertex decls
// FIXME: stick this in the class?
// FIXME: Does anything cause this to get flushed?
//-----------------------------------------------------------------------------
static bool VertexDeclLessFunc( const VertexDeclLookup_t &src1, const VertexDeclLookup_t &src2 )
{
	if ( src1.m_nFlags == src2.m_nFlags )
		return src1.m_VertexFormat < src2.m_VertexFormat;

	return ( src1.m_nFlags < src2.m_nFlags );
}

static CUtlRBTree<VertexDeclLookup_t, int> s_VertexDeclDict( 0, 256, VertexDeclLessFunc );

//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl( VertexFormat_t fmt, bool bStaticLit, bool bUsingFlex, bool bUsingMorph )
{
	MEM_ALLOC_D3D_CREDIT();

	VertexDeclLookup_t lookup;
	lookup.m_VertexFormat = fmt;
	lookup.m_nFlags = 0;
	if ( bStaticLit )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::STATIC_LIT;
	}
	if ( bUsingMorph )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::USING_MORPH;
	}
	if ( bUsingFlex )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::USING_FLEX;
	}

	int i = s_VertexDeclDict.Find( lookup );
	if ( i != s_VertexDeclDict.InvalidIndex() )
	{
		// found
		return s_VertexDeclDict[i].m_pDecl;
	}

	D3DVERTEXELEMENT9 decl[32];
	ComputeVertexSpec( fmt, decl, bStaticLit, bUsingFlex, bUsingMorph );

	HRESULT hr = 
		Dx9Device()->CreateVertexDeclaration( decl, &lookup.m_pDecl );

	// NOTE: can't record until we have m_pDecl!
	RECORD_COMMAND( DX8_CREATE_VERTEX_DECLARATION, 2 );
	RECORD_INT( ( int )lookup.m_pDecl );
	RECORD_STRUCT( decl, sizeof( decl ) );
	COMPILE_TIME_ASSERT( sizeof( decl ) == sizeof( D3DVERTEXELEMENT9 ) * 32 );

	Assert( hr == D3D_OK );
	if ( hr != D3D_OK )
	{
		Warning( " ERROR: failed to create vertex decl for vertex format 0x%08llX! You'll probably see messed-up mesh rendering - to diagnose, build shaderapidx9.dll in debug.\n", fmt );
	}

	s_VertexDeclDict.Insert( lookup );
	return lookup.m_pDecl;
}


//-----------------------------------------------------------------------------
// Clears out all declspecs
//-----------------------------------------------------------------------------
void ReleaseAllVertexDecl()
{
	int i = s_VertexDeclDict.FirstInorder();
	while ( i != s_VertexDeclDict.InvalidIndex() )
	{
		if ( s_VertexDeclDict[i].m_pDecl )
			s_VertexDeclDict[i].m_pDecl->Release();
		i = s_VertexDeclDict.NextInorder( i );
	}
}

