//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "meshbase.h"
#include "shaderapi_global.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Helpers with VertexDesc_t...
//-----------------------------------------------------------------------------
// FIXME: add compression-agnostic read-accessors (which decompress and return by value, checking desc.m_CompressionType)
inline Vector &Position( VertexDesc_t const &desc, int vert )
{
	return *(Vector*)((unsigned char*)desc.m_pPosition + vert * desc.m_VertexSize_Position );
}

inline float Wrinkle( VertexDesc_t const &desc, int vert )
{
	return *(float*)((unsigned char*)desc.m_pWrinkle + vert * desc.m_VertexSize_Wrinkle );
}

inline float *BoneWeight( VertexDesc_t const &desc, int vert )
{
	Assert( desc.m_CompressionType == VERTEX_COMPRESSION_NONE );
	return (float*)((unsigned char*)desc.m_pBoneWeight + vert * desc.m_VertexSize_BoneWeight );
}

inline unsigned char *BoneIndex( VertexDesc_t const &desc, int vert )
{
	return desc.m_pBoneMatrixIndex + vert * desc.m_VertexSize_BoneMatrixIndex;
}

inline Vector &Normal( VertexDesc_t const &desc, int vert )
{
	Assert( desc.m_CompressionType == VERTEX_COMPRESSION_NONE );
	return *(Vector*)((unsigned char*)desc.m_pNormal + vert * desc.m_VertexSize_Normal );
}

inline unsigned char *Color( VertexDesc_t const &desc, int vert )
{
	return desc.m_pColor + vert * desc.m_VertexSize_Color;
}

inline Vector2D &TexCoord( VertexDesc_t const &desc, int vert, int stage )
{
	return *(Vector2D*)((unsigned char*)desc.m_pTexCoord[stage] + vert * desc.m_VertexSize_TexCoord[stage] );
}

inline Vector &TangentS( VertexDesc_t const &desc, int vert )
{
	return *(Vector*)((unsigned char*)desc.m_pTangentS + vert * desc.m_VertexSize_TangentS );
}

inline Vector &TangentT( VertexDesc_t const &desc, int vert )
{
	return *(Vector*)((unsigned char*)desc.m_pTangentT + vert * desc.m_VertexSize_TangentT );
}


//-----------------------------------------------------------------------------
//
// Vertex Buffer implementations begin here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CVertexBufferBase::CVertexBufferBase( const char *pBudgetGroupName )
{
	m_pBudgetGroupName = pBudgetGroupName;
}

CVertexBufferBase::~CVertexBufferBase()
{
}

//-----------------------------------------------------------------------------
// Displays the vertex format
//-----------------------------------------------------------------------------
void CVertexBufferBase::PrintVertexFormat( VertexFormat_t vertexFormat )
{
	VertexCompressionType_t compression = CompressionType( vertexFormat );
	if( vertexFormat & VERTEX_POSITION )
	{
		Msg( "VERTEX_POSITION|" );
	}
	if( vertexFormat & VERTEX_NORMAL )
	{
		// FIXME: genericise this stuff using VertexElement_t data tables (so funcs like 'just work' if we make compression changes)
		if ( compression == VERTEX_COMPRESSION_ON )
			Msg( "VERTEX_NORMAL|" );
		else
			Msg( "VERTEX_NORMAL[COMPRESSED]|" );
	}
	if( vertexFormat & VERTEX_COLOR )
	{
		Msg( "VERTEX_COLOR|" );
	}
	if( vertexFormat & VERTEX_SPECULAR )
	{
		Msg( "VERTEX_SPECULAR|" );
	}
	if( vertexFormat & VERTEX_TANGENT_S )
	{
		Msg( "VERTEX_TANGENT_S|" );
	}
	if( vertexFormat & VERTEX_TANGENT_T )
	{
		Msg( "VERTEX_TANGENT_T|" );
	}
	if( vertexFormat & VERTEX_BONE_INDEX )
	{
		Msg( "VERTEX_BONE_INDEX|" );
	}
	if( vertexFormat & VERTEX_FORMAT_VERTEX_SHADER )
	{
		Msg( "VERTEX_FORMAT_VERTEX_SHADER|" );
	}
	if( NumBoneWeights( vertexFormat ) > 0 )
	{
		Msg( "VERTEX_BONEWEIGHT(%d)%s|",
			NumBoneWeights( vertexFormat ), ( compression ? "[COMPRESSED]" : "" ) );
	}
	if( UserDataSize( vertexFormat ) > 0 )
	{
		Msg( "VERTEX_USERDATA_SIZE(%d)|", UserDataSize( vertexFormat ) );
	}
	int i;
	for( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
	{
		int nDim = TexCoordSize( i, vertexFormat );
		if ( nDim == 0 )
			continue;

		Msg( "VERTEX_TEXCOORD_SIZE(%d,%d)", i, nDim );
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
// Used to construct vertex data
//-----------------------------------------------------------------------------
void CVertexBufferBase::ComputeVertexDescription( unsigned char *pBuffer, 
	VertexFormat_t vertexFormat, VertexDesc_t &desc )
{
	ComputeVertexDesc( pBuffer, vertexFormat, desc );
}


//-----------------------------------------------------------------------------
// Returns the vertex format size 
//-----------------------------------------------------------------------------
int CVertexBufferBase::VertexFormatSize( VertexFormat_t vertexFormat )
{
	// FIXME: We could make this much faster
	MeshDesc_t temp;
	ComputeVertexDescription( 0, vertexFormat, temp );
	return temp.m_ActualVertexSize;
}


//-----------------------------------------------------------------------------
// Spews the mesh data
//-----------------------------------------------------------------------------
void CVertexBufferBase::Spew( int nVertexCount, const VertexDesc_t &desc )
{
	LOCK_SHADERAPI();

	char pTempBuf[1024];
	Q_snprintf( pTempBuf, sizeof(pTempBuf), "\nVerts %d (First %d, Offset %d) :\n", nVertexCount, desc.m_nFirstVertex, desc.m_nOffset );
	Warning( "%s", pTempBuf );

	Assert( ( desc.m_NumBoneWeights == 2 ) || ( desc.m_NumBoneWeights == 0 ) );

	int nLen = 0;
	int nBoneWeightCount = desc.m_NumBoneWeights;
	for ( int i = 0; i < nVertexCount; ++i )
	{
		nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "[%4d] ", i + desc.m_nFirstVertex );
		if ( desc.m_VertexSize_Position )
		{
			Vector &pos = Position( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "P %8.2f %8.2f %8.2f ",
				pos[0], pos[1], pos[2]);
		}

		if ( desc.m_VertexSize_Wrinkle )
		{
			float flWrinkle = Wrinkle( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "Wr %8.2f ",flWrinkle );
		}

		if ( nBoneWeightCount )
		{
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "BW ");
			float* pWeight = BoneWeight( desc, i );
			for ( int j = 0; j < nBoneWeightCount; ++j )
			{
				nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "%1.2f ", pWeight[j] );
			}
		}
		if ( desc.m_VertexSize_BoneMatrixIndex )
		{
			unsigned char *pIndex = BoneIndex( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "BI %d %d %d %d ", ( int )pIndex[0], ( int )pIndex[1], ( int )pIndex[2], ( int )pIndex[3] );
			Assert( pIndex[0] >= 0 && pIndex[0] < 16 );
			Assert( pIndex[1] >= 0 && pIndex[1] < 16 );
			Assert( pIndex[2] >= 0 && pIndex[2] < 16 );
			Assert( pIndex[3] >= 0 && pIndex[3] < 16 );
		}

		if ( desc.m_VertexSize_Normal )
		{
			Vector & normal = Normal( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "N %1.2f %1.2f %1.2f ",
				normal[0],	normal[1],	normal[2]);
		}

		if ( desc.m_VertexSize_Color )
		{
			unsigned char* pColor = Color( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "C b %3d g %3d r %3d a %3d ",
				pColor[0], pColor[1], pColor[2], pColor[3]);
		}

		for ( int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j )
		{
			if ( desc.m_VertexSize_TexCoord[j] )
			{
				Vector2D& texcoord = TexCoord( desc, i, j );
				nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "T%d %.2f %.2f ", j,texcoord[0], texcoord[1]);
			}
		}

		if ( desc.m_VertexSize_TangentS )
		{
			Vector& tangentS = TangentS( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "S %1.2f %1.2f %1.2f ",
				tangentS[0], tangentS[1], tangentS[2]);
		}

		if ( desc.m_VertexSize_TangentT )
		{
			Vector& tangentT = TangentT( desc, i );
			nLen += Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "T %1.2f %1.2f %1.2f ",
				tangentT[0], tangentT[1], tangentT[2]);
		}

		Q_snprintf( &pTempBuf[nLen], sizeof(pTempBuf) - nLen, "\n" );
		Warning( "%s", pTempBuf );
		nLen = 0;
	}
}


//-----------------------------------------------------------------------------
// Validates vertex buffer data
//-----------------------------------------------------------------------------
void CVertexBufferBase::ValidateData( int nVertexCount, const VertexDesc_t &spewDesc )
{
	LOCK_SHADERAPI();
#ifdef VALIDATE_DEBUG
	int i;

	// This is needed so buffering can just use this
	VertexFormat_t fmt = m_pMaterial->GetVertexUsage();

	// Set up the vertex descriptor
	VertexDesc_t desc = spewDesc;

	int numBoneWeights = NumBoneWeights( fmt );
	for ( i = 0; i < nVertexCount; ++i )
	{
		if( fmt & VERTEX_POSITION )
		{
			D3DXVECTOR3& pos = Position( desc, i );
			Assert( IsFinite( pos[0] ) && IsFinite( pos[1] ) && IsFinite( pos[2] ) );
		}
		if( fmt & VERTEX_WRINKLE )
		{
			float flWrinkle = Wrinkle( desc, i );
			Assert( IsFinite( flWrinkle ) );
		}
		if (numBoneWeights > 0)
		{
			float* pWeight = BoneWeight( desc, i );
			for (int j = 0; j < numBoneWeights; ++j)
			{
				Assert( pWeight[j] >= 0.0f && pWeight[j] <= 1.0f );
			}
		}
		if( fmt & VERTEX_BONE_INDEX )
		{
			unsigned char *pIndex = BoneIndex( desc, i );
			Assert( pIndex[0] >= 0 && pIndex[0] < 16 );
			Assert( pIndex[1] >= 0 && pIndex[1] < 16 );
			Assert( pIndex[2] >= 0 && pIndex[2] < 16 );
			Assert( pIndex[3] >= 0 && pIndex[3] < 16 );
		}
		if( fmt & VERTEX_NORMAL )
		{
			D3DXVECTOR3& normal = Normal( desc, i );
			Assert( normal[0] >= -1.05f && normal[0] <= 1.05f );
			Assert( normal[1] >= -1.05f && normal[1] <= 1.05f );
			Assert( normal[2] >= -1.05f && normal[2] <= 1.05f );
		}

		if (fmt & VERTEX_COLOR)
		{
			int* pColor = (int*)Color( desc, i );
			Assert( *pColor != FLOAT32_NAN_BITS );
		}

		for (int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j)
		{
			if( TexCoordSize( j, fmt ) > 0)
			{
				D3DXVECTOR2& texcoord = TexCoord( desc, i, j );
				Assert( IsFinite( texcoord[0] ) && IsFinite( texcoord[1] ) );
			}
		}

		if (fmt & VERTEX_TANGENT_S)
		{
			D3DXVECTOR3& tangentS = TangentS( desc, i );
			Assert( IsFinite( tangentS[0] ) && IsFinite( tangentS[1] ) && IsFinite( tangentS[2] ) );
		}

		if (fmt & VERTEX_TANGENT_T)
		{
			D3DXVECTOR3& tangentT = TangentT( desc, i );
			Assert( IsFinite( tangentT[0] ) && IsFinite( tangentT[1] ) && IsFinite( tangentT[2] ) );
		}
	}
#endif // _DEBUG
}


//-----------------------------------------------------------------------------
//
// Index Buffer implementations begin here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CIndexBufferBase::CIndexBufferBase( const char *pBudgetGroupName ) : m_pBudgetGroupName( pBudgetGroupName )
{
}


//-----------------------------------------------------------------------------
// Spews the mesh data
//-----------------------------------------------------------------------------
void CIndexBufferBase::Spew( int nIndexCount, const IndexDesc_t &indexDesc )
{
	LOCK_SHADERAPI();

	char pTempBuf[512];
	int nLen = 0;
	pTempBuf[0] = '\0';
	char *pTemp = pTempBuf;
	Q_snprintf( pTempBuf, sizeof(pTempBuf), "\nIndices: %d (First %d, Offset %d)\n", nIndexCount, indexDesc.m_nFirstIndex, indexDesc.m_nOffset );
	Warning( "%s", pTempBuf );
	for ( int i = 0; i < nIndexCount; ++i )
	{
		nLen += Q_snprintf( pTemp, sizeof(pTempBuf) - nLen - 1, "%d ", ( int )indexDesc.m_pIndices[i] );
		pTemp = pTempBuf + nLen;
		if ( (i & 0x0F) == 0x0F )
		{
			Q_snprintf( pTemp, sizeof(pTempBuf) - nLen - 1, "\n" );
			Warning( "%s", pTempBuf );
			pTempBuf[0] = '\0';
			nLen = 0;
			pTemp = pTempBuf;
		}
	}
	Q_snprintf( pTemp, sizeof(pTempBuf) - nLen - 1, "\n" );
	Warning( "%s", pTempBuf ); 
}


//-----------------------------------------------------------------------------
// Call this in debug mode to make sure our data is good.
//-----------------------------------------------------------------------------
void CIndexBufferBase::ValidateData( int nIndexCount, const IndexDesc_t& desc )
{ 
	/* FIXME */ 
	// NOTE: Is there anything reasonable to do here at all? 
	// Or is this a bogus method altogether?
}



//-----------------------------------------------------------------------------
//
// Base mesh
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CMeshBase::CMeshBase()
{
}

CMeshBase::~CMeshBase()
{			   
}
