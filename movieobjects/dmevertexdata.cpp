//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmevertexdata.h"
#include "movieobjects_interfaces.h"
#include <limits.h>
#include "tier3/tier3.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Standard vertex fields
//-----------------------------------------------------------------------------
static char *g_pStandardFieldNames[] =
{
	"positions",
	"normals",
	"tangents",
	"textureCoordinates",
	"colors",
	"jointWeights",
	"jointIndices",
	"balance",
	"speed",
	"wrinkle",
	"weight"
};

static DmAttributeType_t g_pStandardFieldTypes[] =
{
	AT_VECTOR3_ARRAY,
	AT_VECTOR3_ARRAY,
	AT_VECTOR4_ARRAY,
	AT_VECTOR2_ARRAY,
	AT_COLOR_ARRAY,
	AT_FLOAT_ARRAY,
	AT_INT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_FLOAT_ARRAY
};



//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeVertexDataBase, CDmeVertexDataBase );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::OnConstruction()
{
	m_nVertexCount = 0;
	memset( m_pStandardFieldIndex, 0xFF, sizeof(m_pStandardFieldIndex) );
	m_VertexFormat.Init( this, "vertexFormat" );

	m_nJointCount.Init( this, "jointCount" );
	m_bFlipVCoordinates.Init( this, "flipVCoordinates" );
}

void CDmeVertexDataBase::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Updates info for fast lookups for well-known fields
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::UpdateStandardFieldInfo( int nFieldIndex, const char *pFieldName, DmAttributeType_t attrType )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE(g_pStandardFieldNames) == STANDARD_FIELD_COUNT );
	COMPILE_TIME_ASSERT( ARRAYSIZE(g_pStandardFieldTypes) == STANDARD_FIELD_COUNT );

	for ( int i = 0; i < STANDARD_FIELD_COUNT; ++i )
	{
		if ( !Q_stricmp( pFieldName, g_pStandardFieldNames[i] ) )
		{
			if ( attrType != g_pStandardFieldTypes[i] )
			{
				Warning( "Standard field %s has incorrect attribute type!\n", pFieldName );
				return;
			}
			m_pStandardFieldIndex[i] = nFieldIndex;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Computes information about how to find particular fields
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::ComputeFieldInfo()
{
	// Clear existing field info, 
	// but keep the old names around so field indices remain constant
	int nCurrentCount = m_FieldInfo.Count();
	for ( int i = 0; i < nCurrentCount; ++i )
	{
		m_FieldInfo[i].m_pIndexData = NULL;
		m_FieldInfo[i].m_pVertexData = NULL;
	}

	// FIXME: Want to maintain field indices as constants for all time 
	int nFieldCount = m_VertexFormat.Count();
	for ( int i = 0; i < nFieldCount; ++i )
	{
		const char *pFieldName = m_VertexFormat[i];
		int nLen = Q_strlen( pFieldName ) + 21;
		char *pIndicesName = (char*)_alloca( nLen );
		Q_snprintf( pIndicesName, nLen, "%sIndices", pFieldName );

		CDmAttribute *pVerticesArray = GetAttribute( pFieldName );
		if ( !pVerticesArray || !IsArrayType( pVerticesArray->GetType() ) )
			continue;

		CDmAttribute *pIndicesArray = NULL;
		if ( Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_WEIGHTS] ) &&
			Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_INDICES] ) )
		{
			pIndicesArray = GetAttribute( pIndicesName );
			if ( !pIndicesArray || pIndicesArray->GetType() != AT_INT_ARRAY )
				continue;
		}

		FieldIndex_t nFieldIndex = FindFieldIndex( pFieldName );
		if ( nFieldIndex < 0 )
		{
			nFieldIndex = m_FieldInfo.AddToTail();
			m_FieldInfo[nFieldIndex].m_Name = pFieldName;
			m_FieldInfo[nFieldIndex].m_bInverseMapDirty = true;
			UpdateStandardFieldInfo( nFieldIndex, pFieldName, pVerticesArray->GetType() );
		}
		m_FieldInfo[nFieldIndex].m_pVertexData = pVerticesArray;
		m_FieldInfo[nFieldIndex].m_pIndexData = pIndicesArray;
	}
}


//-----------------------------------------------------------------------------
// Computes the vertex count ( min of the index buffers )
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::ComputeVertexCount()
{
	int nCount = m_FieldInfo.Count();
	if ( nCount == 0 )
	{
		m_nVertexCount = 0;
		return;
	}

	m_nVertexCount = INT_MAX;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !m_FieldInfo[i].m_pIndexData )
			continue;

		CDmrGenericArray array( m_FieldInfo[i].m_pIndexData );
		int nFieldCount = array.Count();
		if ( nFieldCount < m_nVertexCount )
		{
			m_nVertexCount = nFieldCount; 
		}
	}
}


//-----------------------------------------------------------------------------
// resolve internal data from changed attributes
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::Resolve()
{
	BaseClass::Resolve();

	if ( m_VertexFormat.IsDirty() )
	{
		ComputeFieldInfo();
	}

	if ( !IsVertexDeltaData() )
	{
		ComputeVertexCount();
	}

	// Mark inverse map dirty if necessary
	int nCount = m_FieldInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_FieldInfo[i].m_pIndexData && m_FieldInfo[i].m_pIndexData->IsFlagSet( FATTRIB_DIRTY ) )
		{
			m_FieldInfo[i].m_bInverseMapDirty = true;
		}
	}
}


//-----------------------------------------------------------------------------
// Returns indices into the various fields
//-----------------------------------------------------------------------------
int CDmeVertexDataBase::GetPositionIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_POSITION );
}

int CDmeVertexDataBase::GetNormalIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_NORMAL );
}

int CDmeVertexDataBase::GetTangentIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_TANGENT );
}

int CDmeVertexDataBase::GetTexCoordIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_TEXCOORD );
}

int CDmeVertexDataBase::GetColorIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_COLOR );
}

int CDmeVertexDataBase::GetBalanceIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_BALANCE );
}

int CDmeVertexDataBase::GetMorphSpeedIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_MORPH_SPEED );
}

int CDmeVertexDataBase::GetWrinkleIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_WRINKLE );
}

int CDmeVertexDataBase::GetWeightIndex( int nVertexIndex ) const
{
	return GetFieldIndex( nVertexIndex, FIELD_WEIGHT );
}


//-----------------------------------------------------------------------------
// Vertex accessors
//-----------------------------------------------------------------------------
const Vector& CDmeVertexDataBase::GetPosition( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_POSITION];
	if ( nFieldIndex < 0 )
		return vec3_origin;

	CDmrArrayConst<int> indices( GetIndexData( nFieldIndex ) );
	CDmrArrayConst<Vector> vertexData( GetVertexData( nFieldIndex ) );
	return vertexData[ indices[nIndex] ];
}

const float *CDmeVertexDataBase::GetJointWeights( int nVertexIndex ) const
{
	Assert( IsVertexDeltaData() || nVertexIndex < m_nVertexCount );
	FieldIndex_t nPosFieldIndex = m_pStandardFieldIndex[FIELD_POSITION];
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_WEIGHTS];
	if ( nPosFieldIndex < 0 || nFieldIndex < 0 )
		return NULL;

	CDmrArrayConst<int> indices = GetIndexData( nPosFieldIndex );
	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return &vertexData[ indices[ nVertexIndex ] * m_nJointCount ];
}


//-----------------------------------------------------------------------------
// Same as GetJointWeights except it uses a direct position index instead of
// the vertex index to access the data
//-----------------------------------------------------------------------------
const float *CDmeVertexDataBase::GetJointPositionWeights( int nPositionIndex ) const
{
	Assert( !IsVertexDeltaData() );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_WEIGHTS];
	if ( nFieldIndex < 0 )
		return NULL;

	CDmrArrayConst< float > jointWeights = GetVertexData( nFieldIndex );
	Assert( nPositionIndex * m_nJointCount < jointWeights.Count() );
	return &jointWeights[ nPositionIndex * m_nJointCount ];
}

const int *CDmeVertexDataBase::GetJointIndices( int nVertexIndex ) const
{
	Assert( IsVertexDeltaData() || nVertexIndex < m_nVertexCount );
	FieldIndex_t nPosFieldIndex = m_pStandardFieldIndex[FIELD_POSITION];
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_INDICES];
	if ( nPosFieldIndex < 0 || nFieldIndex < 0 )
		return NULL;

	CDmrArrayConst<int> indices = GetIndexData( nPosFieldIndex );
	CDmrArrayConst<int> vertexData = GetVertexData( nFieldIndex );
	return &vertexData[ indices[ nVertexIndex ] * m_nJointCount ];
}


//-----------------------------------------------------------------------------
// Same as GetJointIndices except it uses a direct position index instead of
// the vertex index to access the data
//-----------------------------------------------------------------------------
const int *CDmeVertexDataBase::GetJointPositionIndices( int nPositionIndex ) const
{
	Assert( !IsVertexDeltaData() );
	FieldIndex_t nJointIndicesField = m_pStandardFieldIndex[ FIELD_JOINT_INDICES ];
	if ( nJointIndicesField < 0 )
		return NULL;

	CDmrArrayConst<int> jointIndices = GetVertexData( nJointIndicesField );
	Assert( nPositionIndex * m_nJointCount < jointIndices.Count() );
	return &jointIndices[ nPositionIndex * m_nJointCount ];
}

const Vector& CDmeVertexDataBase::GetNormal( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_NORMAL];
	if ( nFieldIndex < 0 )
		return vec3_origin;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<Vector> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

const Vector4D& CDmeVertexDataBase::GetTangent( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_TANGENT];
	if ( nFieldIndex < 0 )
		return vec4_origin;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<Vector4D> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

const Vector2D& CDmeVertexDataBase::GetTexCoord( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_TEXCOORD];
	if ( nFieldIndex < 0 )
		return vec2_origin;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<Vector2D> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

static Color s_Black( 0, 0, 0, 255 );
const Color& CDmeVertexDataBase::GetColor( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_COLOR];
	if ( nFieldIndex < 0 )
		return s_Black;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<Color> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

float CDmeVertexDataBase::GetBalance( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_BALANCE];
	if ( nFieldIndex < 0 )
		return 0.5f;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

float CDmeVertexDataBase::GetMorphSpeed( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_MORPH_SPEED];
	if ( nFieldIndex < 0 )
		return 1.0f;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}

float CDmeVertexDataBase::GetWrinkle( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_WRINKLE];
	if ( nFieldIndex < 0 )
		return 1.0f;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeVertexDataBase::GetWeight( int nIndex ) const
{
	Assert( IsVertexDeltaData() || nIndex < m_nVertexCount );
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_WEIGHT];
	if ( nFieldIndex < 0 )
		return 1.0f;

	CDmrArrayConst<int> indices = GetIndexData( nFieldIndex );
	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData[ indices[ nIndex ] ];
}


//-----------------------------------------------------------------------------
// Adds a field to the vertex format
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::FindOrAddVertexField( const char *pFieldName )
{
	int i;
	int nFormatCount = m_VertexFormat.Count();
	for ( i = 0; i < nFormatCount; ++i )
	{
		if ( !Q_stricmp( pFieldName, m_VertexFormat[i] ) )
			return;
	}
	m_VertexFormat.AddToTail( pFieldName );
}


//-----------------------------------------------------------------------------
// Returns the field index of a particular field
//-----------------------------------------------------------------------------
FieldIndex_t CDmeVertexDataBase::CreateField( const char *pFieldName, DmAttributeType_t type )
{
	Assert( Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_WEIGHTS] ) );
	Assert( Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_INDICES] ) );
	if ( !Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_WEIGHTS] ) ||
		!Q_stricmp( pFieldName, g_pStandardFieldNames[FIELD_JOINT_INDICES] ) )
	{
		return -1;
	}

	AddAttribute( pFieldName, type );

	int nLen = Q_strlen( pFieldName ) + 21;
	char *pIndicesName = (char*)_alloca( nLen );
	Q_snprintf( pIndicesName, nLen, "%sIndices", pFieldName );
	AddAttribute( pIndicesName, AT_INT_ARRAY );

	FindOrAddVertexField( pFieldName );
	 
	// FIXME: Not hugely efficient, is there a better way of doing this?
	// Necessary to return a field index for the name
	ComputeFieldInfo();
	FieldIndex_t nFieldIndex = FindFieldIndex( pFieldName );
	if ( !IsVertexDeltaData() && m_nVertexCount > 0 )
	{
		CDmrArray<int> indices( GetIndexData( nFieldIndex ) );
		indices.EnsureCount( m_nVertexCount );
	}
	return nFieldIndex;
}


//-----------------------------------------------------------------------------
// Creates a field given a file ID
//-----------------------------------------------------------------------------
FieldIndex_t CDmeVertexDataBase::CreateField( StandardFields_t fieldId )
{
	return CreateField( g_pStandardFieldNames[fieldId], g_pStandardFieldTypes[fieldId] );
}


//-----------------------------------------------------------------------------
// Use this to create vertex fields for joint weights + indices
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::CreateJointWeightsAndIndices( int nJointCount, FieldIndex_t *pJointWeightsField, FieldIndex_t *pJointIndicesField )
{
	m_nJointCount = nJointCount;

	AddAttribute( g_pStandardFieldNames[FIELD_JOINT_WEIGHTS], AT_FLOAT_ARRAY );
	AddAttribute( g_pStandardFieldNames[FIELD_JOINT_INDICES], AT_INT_ARRAY );

	FindOrAddVertexField( g_pStandardFieldNames[FIELD_JOINT_WEIGHTS] );
	FindOrAddVertexField( g_pStandardFieldNames[FIELD_JOINT_INDICES] );


	// FIXME: Not hugely efficient, is there a better way of doing this?
	// Necessary to return a field index for the name
	ComputeFieldInfo();
	*pJointWeightsField = FindFieldIndex( g_pStandardFieldNames[FIELD_JOINT_WEIGHTS] );
	*pJointIndicesField = FindFieldIndex( g_pStandardFieldNames[FIELD_JOINT_INDICES] );
}


//-----------------------------------------------------------------------------
// Adds a new vertex; creates a new entry in all vertex data fields
// Returns the vertex index
//-----------------------------------------------------------------------------
int CDmeVertexDataBase::AddVertexData( FieldIndex_t nFieldIndex, int nCount )
{
	CDmrGenericArray array( m_FieldInfo[nFieldIndex].m_pVertexData );
	int nDataCount = array.Count();
	array.EnsureCount( nDataCount + nCount );

	// DmeMeshDeltaData must have the same number of vertices + indices
	if ( IsVertexDeltaData() )
	{
		CDmrArray<int> indices( GetIndexData( nFieldIndex ) );
		Assert( nDataCount == indices.Count() );
		indices.EnsureCount( nDataCount + nCount );
	}

	return nDataCount;
}


//-----------------------------------------------------------------------------
// Sets vertex data
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::SetVertexData( FieldIndex_t nFieldIndex, int nFirstVertex, int nCount, DmAttributeType_t valueType, const void *pData )
{
	CDmrGenericArray array( m_FieldInfo[nFieldIndex].m_pVertexData );
	Assert(	nFirstVertex + nCount <= array.Count() );
	array.SetMultiple( nFirstVertex, nCount, valueType, pData );
}

void CDmeVertexDataBase::SetVertexIndices( FieldIndex_t nFieldIndex, int nFirstIndex, int nCount, const int *pIndices )
{
	CDmrArray<int> array( GetIndexData( nFieldIndex ) );
	Assert(	nFirstIndex + nCount <= array.Count() );
	array.SetMultiple( nFirstIndex, nCount, pIndices );
}


//-----------------------------------------------------------------------------
// Removes all vertex data associated with a particular field
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::RemoveAllVertexData( FieldIndex_t nFieldIndex )
{
	CDmrGenericArray array( m_FieldInfo[nFieldIndex].m_pVertexData );
	array.RemoveAll();
	if ( IsVertexDeltaData() )
	{
		CDmrArray<int> arrayDelta( m_FieldInfo[nFieldIndex].m_pIndexData );
		arrayDelta.RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Returns the field index of a particular field
//-----------------------------------------------------------------------------
FieldIndex_t CDmeVertexDataBase::FindFieldIndex( const char *pFieldName ) const
{
	int nCount = m_FieldInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_FieldInfo[i].m_Name, pFieldName ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Returns well-known vertex data
//-----------------------------------------------------------------------------
static CUtlVector<Vector4D> s_EmptyVector4D; 
static CUtlVector<Vector> s_EmptyVector; 
static CUtlVector<Vector2D> s_EmptyVector2D; 
static CUtlVector<Color> s_EmptyColor; 
static CUtlVector<float> s_EmptyFloat; 

const CUtlVector<Vector> &CDmeVertexDataBase::GetPositionData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_POSITION ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyVector;

	CDmrArrayConst<Vector> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<Vector> &CDmeVertexDataBase::GetNormalData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_NORMAL ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyVector;

	CDmrArrayConst<Vector> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<Vector4D> &CDmeVertexDataBase::GetTangentData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_TANGENT ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyVector4D;

	CDmrArrayConst<Vector4D> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<Vector2D> &CDmeVertexDataBase::GetTextureCoordData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_TEXCOORD ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyVector2D;

	CDmrArrayConst<Vector2D> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<Color> &CDmeVertexDataBase::GetColorData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_COLOR ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyColor;

	CDmrArrayConst<Color> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const float *CDmeVertexDataBase::GetJointWeightData( int nDataIndex ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_WEIGHTS];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return NULL;

	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return &vertexData[ nDataIndex * m_nJointCount ];
}

const int *CDmeVertexDataBase::GetJointIndexData( int nDataIndex ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_INDICES];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return NULL;

	CDmrArrayConst<int> vertexData = GetVertexData( nFieldIndex );
	return &vertexData.Element( nDataIndex * m_nJointCount );
}

const CUtlVector<float> &CDmeVertexDataBase::GetBalanceData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_BALANCE ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyFloat;

	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<float> &CDmeVertexDataBase::GetMorphSpeedData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_MORPH_SPEED ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyFloat;

	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<float> &CDmeVertexDataBase::GetWrinkleData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_WRINKLE ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyFloat;

	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}

const CUtlVector<float> &CDmeVertexDataBase::GetWeightData( ) const
{
	FieldIndex_t nFieldIndex = m_pStandardFieldIndex[ FIELD_WEIGHT ];
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyFloat;

	CDmrArrayConst<float> vertexData = GetVertexData( nFieldIndex );
	return vertexData.Get(); 
}


//-----------------------------------------------------------------------------
// Returns well-known index data
//-----------------------------------------------------------------------------
static CUtlVector<int> s_EmptyInt; 
const CUtlVector<int> &CDmeVertexDataBase::GetVertexIndexData( FieldIndex_t nFieldIndex ) const
{
	if ( nFieldIndex < 0 || nFieldIndex >= m_FieldInfo.Count() )
		return s_EmptyInt;

	CDmrArrayConst<int> indexData = GetIndexData( nFieldIndex );
	return indexData.Get(); 
}

const CUtlVector<int> &CDmeVertexDataBase::GetVertexIndexData( StandardFields_t fieldId ) const
{
	return GetVertexIndexData( m_pStandardFieldIndex[fieldId] );
}


//-----------------------------------------------------------------------------
// Returns an inverse map from vertex data index to vertex index
//-----------------------------------------------------------------------------
const CUtlVector< int > &CDmeVertexDataBase::FindVertexIndicesFromDataIndex( FieldIndex_t nFieldIndex, int nDataIndex )
{
	if ( nFieldIndex < 0 )
		return s_EmptyInt;

	FieldInfo_t &info = m_FieldInfo[nFieldIndex]; 
	if ( info.m_bInverseMapDirty )
	{
		CDmrArrayConst<int> array( info.m_pIndexData );
		CDmrGenericArray vertexArray( info.m_pVertexData );

		int nDataCount = vertexArray.Count();
		int nCount = array.Count();

		// Clear out the utlvectors
		info.m_InverseMap.RemoveAll();
		info.m_InverseMap.SetCount( nDataCount );

		for ( int i = 0; i < nCount; ++i )
		{
			int nIndex = array[ i ];
			info.m_InverseMap[nIndex].AddToTail( i );
		}
		info.m_bInverseMapDirty = false;
	}

	return info.m_InverseMap[ nDataIndex ];
}

const CUtlVector< int > &CDmeVertexDataBase::FindVertexIndicesFromDataIndex( StandardFields_t fieldId, int nDataIndex )
{
	// NOTE! Wrinkles don't exist in the base state, therefore we use the index to index
	// into the TEXCOORD base state fields instead of the wrinkle fields
	if ( fieldId == FIELD_WRINKLE )
	{
		fieldId = FIELD_TEXCOORD;
	}

	return FindVertexIndicesFromDataIndex( m_pStandardFieldIndex[fieldId], nDataIndex );
}


//-----------------------------------------------------------------------------
// Do we have skinning data?
//-----------------------------------------------------------------------------
bool CDmeVertexDataBase::HasSkinningData() const
{
	if ( m_nJointCount == 0 )
		return false;
	FieldIndex_t nWeightFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_WEIGHTS];
	if ( nWeightFieldIndex < 0 )
		return false;
	FieldIndex_t nIndexFieldIndex = m_pStandardFieldIndex[FIELD_JOINT_INDICES];
	if ( nIndexFieldIndex < 0 )
		return false;

	CDmrArrayConst<float> weightData = GetVertexData( nWeightFieldIndex );
	CDmrArrayConst<int> indexData = GetVertexData( nIndexFieldIndex );
	return ( weightData.Count() > 0 && indexData.Count() > 0 );
}


//-----------------------------------------------------------------------------
// Do we need tangent data? (Utility method for applications to know if they should call ComputeDefaultTangentData)
//-----------------------------------------------------------------------------
bool CDmeVertexDataBase::NeedsTangentData() const
{
	FieldIndex_t posField = m_pStandardFieldIndex[CDmeVertexDataBase::FIELD_POSITION];
	FieldIndex_t normalField = m_pStandardFieldIndex[CDmeVertexDataBase::FIELD_NORMAL];
	FieldIndex_t uvField = m_pStandardFieldIndex[CDmeVertexDataBase::FIELD_TEXCOORD];
	FieldIndex_t tangentField = m_pStandardFieldIndex[CDmeVertexDataBase::FIELD_TANGENT];
	return ( posField >= 0 && uvField >= 0 && normalField >= 0 && tangentField < 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeVertexDataBase::FieldCount() const
{
	return m_VertexFormat.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmeVertexDataBase::FieldName( int i ) const
{
	if ( i < 0 || i >= m_VertexFormat.Count() )
		return NULL;

	return m_VertexFormat[ i ];
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::CopyFrom( CDmeVertexDataBase *pSrc )
{
	pSrc->CopyTo( this );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeVertexDataBase::CopyTo( CDmeVertexDataBase *pDst ) const
{
	// Preserve the name of the destination
	const CUtlString dstName = pDst->GetName();

	CopyAttributesTo( pDst );

	pDst->SetName( dstName );	// The Copy really copies everything!
	pDst->Resolve();
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeVertexData, CDmeVertexData );
IMPLEMENT_ELEMENT_FACTORY( DmeVertexDeltaData, CDmeVertexDeltaData );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeVertexData::OnConstruction()
{
}

void CDmeVertexData::OnDestruction()
{
}

void CDmeVertexDeltaData::OnConstruction()
{
	m_bCorrected.InitAndSet( this, "corrected", false );
}

void CDmeVertexDeltaData::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Method to add vertex indices for normal vertex data
//-----------------------------------------------------------------------------
int CDmeVertexData::AddVertexIndices( int nIndexCount )
{
	int nFirstVertex = m_nVertexCount;
	m_nVertexCount += nIndexCount;
	int nCount = m_FieldInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_FieldInfo[i].m_pIndexData )
		{
			CDmrArray<int> indices( m_FieldInfo[i].m_pIndexData );
			indices.EnsureCount( m_nVertexCount );
		}
	}
	return nFirstVertex;
}


//-----------------------------------------------------------------------------
// Computes max positional delta length
//-----------------------------------------------------------------------------
float CDmeVertexDeltaData::ComputeMaxDeflection( )
{
	float flMaxDeflection = 0.0f;

	const CUtlVector<Vector> &pos = GetPositionData();
	int nCount = pos.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		float flDeflection = pos[i].Length();
		if ( flMaxDeflection < flDeflection )
		{
			flMaxDeflection = flDeflection;
		}
	}
	return flMaxDeflection;
}


//-----------------------------------------------------------------------------
// Computes wrinkle data from position deltas
//-----------------------------------------------------------------------------
void CDmeVertexDeltaData::GenerateWrinkleDelta( CDmeVertexData *pBindState, float flScale, bool bOverwrite )
{
	FieldIndex_t nPosIndex = FindFieldIndex( FIELD_POSITION );
	if ( nPosIndex < 0 )
		return;

	FieldIndex_t nBaseTexCoordIndex = pBindState->FindFieldIndex( FIELD_TEXCOORD );
	if ( nBaseTexCoordIndex < 0 )
		return;

	FieldIndex_t nWrinkleIndex = FindFieldIndex( FIELD_WRINKLE );
	if ( nWrinkleIndex < 0 )
	{
		nWrinkleIndex = CreateField( FIELD_WRINKLE );
	}
	else if ( !bOverwrite )
		return;

	RemoveAllVertexData( nWrinkleIndex );
	if ( flScale == 0.0f )
		return;

	const float flMaxDeflection( ComputeMaxDeflection() );
	if ( flMaxDeflection == 0.0f )
		return;

	const double scaledInverseMaxDeflection = static_cast< double >( flScale ) / static_cast< double >( flMaxDeflection );

	const CUtlVector<int> &positionIndices = GetVertexIndexData( nPosIndex );
	const CUtlVector<Vector> &pos = GetPositionData();
	const CUtlVector<int> &baseTexCoordIndices = pBindState->GetVertexIndexData( nBaseTexCoordIndex );

	CDmrArrayConst<Vector2D> texData( pBindState->GetVertexData( nBaseTexCoordIndex ) );
	int nBaseTexCoordCount = texData.Count();
	int nBufSize = ( ( nBaseTexCoordCount + 7 ) >> 3 );
	unsigned char *pUsedBits = (unsigned char*)_alloca( nBufSize );
	memset( pUsedBits, 0, nBufSize );

	int nCount = pos.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		float flWrinkleDelta = static_cast< float >( static_cast< double >( pos[i].Length() ) * scaledInverseMaxDeflection );
		Assert( fabs( flWrinkleDelta ) <= fabs( flScale ) );

		// NOTE: This will produce bad behavior in cases where two positions share the
		// same texcoord, which shouldn't theoretically happen.
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( FIELD_POSITION, positionIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int j = 0; j < nBaseVertCount; ++j )
		{
			// See if we have a delta for this texcoord...
			int nTexCoordIndex = baseTexCoordIndices[ baseVerts[j] ];
			if ( pUsedBits[ nTexCoordIndex >> 3 ] & ( 1 << ( nTexCoordIndex & 0x7 ) ) )
				continue;

			pUsedBits[ nTexCoordIndex >> 3 ] |= 1 << ( nTexCoordIndex & 0x7 );

			int nDeltaIndex = AddVertexData( nWrinkleIndex, 1 );
			SetVertexIndices( nWrinkleIndex, nDeltaIndex, 1, &nTexCoordIndex );
			SetVertexData( nWrinkleIndex, nDeltaIndex, 1, AT_FLOAT, &flWrinkleDelta );
		}
	}
}

//-----------------------------------------------------------------------------
// Computes weight data from position deltas
//-----------------------------------------------------------------------------
float CDmeVertexDeltaData::GenerateWeightDelta( CDmeVertexData *pBindState )
{
	FieldIndex_t nPosIndex = FindFieldIndex( FIELD_POSITION );
	if ( nPosIndex < 0 )
		return 0.0;

	FieldIndex_t nFieldIndex = FindFieldIndex( FIELD_WEIGHT );
	if ( nFieldIndex < 0 )
	{
		nFieldIndex = CreateField( FIELD_WEIGHT );
	}

	RemoveAllVertexData( nFieldIndex );

	const float maxDeflection( static_cast< double >( ComputeMaxDeflection() ) );

	if ( maxDeflection == 0.0 )
		return maxDeflection;

	const CUtlVector<Vector> &pos( GetPositionData() );
	const CUtlVector< int > &posIndices( GetVertexIndexData( nPosIndex ) );

	float flDeltaDistance;
	int nDeltaIndex;
	const int nCount = pos.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		flDeltaDistance = pos[ i ].Length();

		nDeltaIndex = AddVertexData( nFieldIndex, 1 );
		SetVertexData( nFieldIndex, nDeltaIndex, 1, AT_FLOAT, &flDeltaDistance );
	}

	SetVertexIndices( nFieldIndex, 0, posIndices.Count(), posIndices.Base() );

	return maxDeflection;
}