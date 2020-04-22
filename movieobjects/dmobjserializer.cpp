//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Serialize and Unserialize Wavefront OBJ <-> DME Data
//
//=============================================================================


// Valve includes
#include "tier1/characterset.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmobjserializer.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmemodel.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier1/UtlStringMap.h"
#include "mathlib/mathlib.h"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CFaceSetData
{
public:
	void Clear();

	inline CUtlVector< int > *GetFaceSetIndices( const char *pFaceSetName )
	{
		return &m_faceSetIndices[ pFaceSetName ];
	}

	void AddToMesh( CDmeMesh *pMesh );
protected:

	CUtlStringMap< CUtlVector< int > > m_faceSetIndices;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFaceSetData::Clear()
{
	m_faceSetIndices.Clear();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFaceSetData::AddToMesh( CDmeMesh *pMesh )
{
	const int nFaceSets( m_faceSetIndices.GetNumStrings() );
	for ( int i( 0 ); i < nFaceSets; ++i )
	{
		const char *pName( m_faceSetIndices.String( i ) );
		CUtlVector< int > &faceSetIndices( m_faceSetIndices[ pName ] );

		if ( faceSetIndices.Count() )
		{
			CDmeFaceSet *pFaceSet = CreateElement< CDmeFaceSet >( pName, pMesh->GetFileId() );

			CDmeMaterial *pMaterial = CreateElement< CDmeMaterial >( pName, pMesh->GetFileId() );
			pMaterial->SetMaterial( pName );

			pFaceSet->AddIndices( faceSetIndices.Count() );
			pFaceSet->SetIndices( 0, faceSetIndices.Count(), faceSetIndices.Base() );

			pFaceSet->SetMaterial( pMaterial );

			pMesh->AddFaceSet( pFaceSet );
		}
	}

	Clear();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CVertexData
{
public:
	void Clear();

	inline void AddPosition( const Vector &p ) { m_positions.AddToTail( p ); }

	inline void AddPositionIndex( int i ) { m_pIndices.AddToTail( i ); }

	inline void AddNormal( const Vector &n ) { m_normals.AddToTail( n ); }

	inline void AddNormalIndex( int i ) { m_nIndices.AddToTail( i ); }

	inline void AddUV( const Vector2D &uv ) { AddUniqueValue( uv, m_uvs, m_uvIndexMap, FLT_EPSILON * 0.1f ); }

	inline void AddUVIndex( int i ) { Assert( i < m_uvIndexMap.Count() ); m_uvIndices.AddToTail( m_uvIndexMap[ i ] ); }

	inline int VertexCount() const { return m_pIndices.Count(); }

	CDmeVertexDataBase *AddToMesh( CDmeMesh *pMesh, bool bAbsolute, const char *pName, bool bDelta );

protected:
	template < class T_t > void AddUniqueValue(
		const T_t &v,
		CUtlVector< T_t > &vs,
		CUtlVector< int > &map,
		float flThresh = FLT_EPSILON )
	{
		const int nVs( vs.Count() );
		for ( int i( 0 ); i < nVs; ++i )
		{
			if ( v.DistToSqr( vs[ i ] ) < flThresh )
			{
				map.AddToTail( i );
				return;
			}
		}

		map.AddToTail( vs.Count() );
		vs.AddToTail( v );
	}

	CDmeVertexDataBase *Add( CDmeMesh *pMesh, const char *pName = "bind" );

	CDmeVertexDeltaData *AddDelta( CDmeMesh *pMesh, bool bAbsolute, const char *pName = "bind" );

	CUtlVector< Vector > m_positions;
	CUtlVector< int > m_pIndices;

	CUtlVector< Vector > m_normals;
	CUtlVector< int > m_nIndices;

	CUtlVector< Vector2D > m_uvs;
	CUtlVector< int > m_uvIndexMap;
	CUtlVector< int > m_uvIndices;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVertexData::Clear()
{
	m_positions.RemoveAll();
	m_pIndices.RemoveAll();

	m_normals.RemoveAll();
	m_nIndices.RemoveAll();

	m_uvs.RemoveAll();
	m_uvIndexMap.RemoveAll();
	m_uvIndices.RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDataBase *CVertexData::AddToMesh( CDmeMesh *pMesh, bool bAbsolute, const char *pName, bool delta )
{
	if ( delta )
		return AddDelta( pMesh, bAbsolute, pName );

	return Add( pMesh, pName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDataBase *CVertexData::Add( CDmeMesh *pMesh, const char *pName )
{
	CDmeVertexDataBase *pVertexData( NULL );

	if ( m_positions.Count() && m_pIndices.Count() )
	{
		{
			CDmeVertexData *pBaseVertexData = pMesh->FindOrCreateBaseState( pName );
			pMesh->SetCurrentBaseState( pName );
			pBaseVertexData->AddVertexIndices( m_pIndices.Count() );
			pVertexData = pBaseVertexData;
		}
		
		pVertexData->FlipVCoordinate( true );

		const FieldIndex_t pIndex( pVertexData->CreateField( CDmeVertexData::FIELD_POSITION ) );
		pVertexData->AddVertexData( pIndex, m_positions.Count() );
		pVertexData->SetVertexData( pIndex, 0, m_positions.Count(), AT_VECTOR3, m_positions.Base() );
		pVertexData->SetVertexIndices( pIndex, 0, m_pIndices.Count(), m_pIndices.Base() );

		if ( pVertexData && m_normals.Count() && m_nIndices.Count() )
		{
			Assert( m_pIndices.Count() == m_nIndices.Count() );

			const FieldIndex_t nIndex( pVertexData->CreateField( CDmeVertexData::FIELD_NORMAL ) );
			pVertexData->AddVertexData( nIndex, m_normals.Count() );
			pVertexData->SetVertexData( nIndex, 0, m_normals.Count(), AT_VECTOR3, m_normals.Base() );
			pVertexData->SetVertexIndices( nIndex, 0, m_nIndices.Count(), m_nIndices.Base() );
		}

		if ( pVertexData && m_uvs.Count() && m_uvIndices.Count() )
		{
			Assert( m_pIndices.Count() == m_uvIndices.Count() );

			const FieldIndex_t uvIndex( pVertexData->CreateField( CDmeVertexData::FIELD_TEXCOORD ) );
			pVertexData->AddVertexData( uvIndex, m_uvs.Count() );
			pVertexData->SetVertexData( uvIndex, 0, m_uvs.Count(), AT_VECTOR2, m_uvs.Base() );
			pVertexData->SetVertexIndices( uvIndex, 0, m_uvIndices.Count(), m_uvIndices.Base() );
		}
	}

	return pVertexData;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CVertexData::AddDelta( CDmeMesh *pMesh, bool bAbsolute, const char *pName )
{
	CDmeVertexDeltaData *pDelta( NULL );

	if ( m_positions.Count() )
	{
		CDmeVertexData *pBind = pMesh->FindBaseState( "bind" );
		if ( pBind == NULL )
			return NULL;

		const FieldIndex_t pBindIndex( pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );

		if ( pBindIndex < 0 )
			return NULL;

		CDmrArrayConst< Vector > pBindData( pBind->GetVertexData( pBindIndex ) );

		const int pCount( m_positions.Count() );
		if ( pBindData.Count() != pCount )
			return NULL;

		for ( int i( 0 ); i < pCount; ++i )
		{
			m_positions[ i ] -= pBindData[ i ];
		}

		int *pIndices = reinterpret_cast< int * >( alloca( pCount * sizeof( int ) ) );
		int nNonZero( 0 );
		for ( int i( 0 ); i < pCount; ++i )
		{
			const Vector &v( m_positions[ i ] );
			// Kind of a magic number but it's because of 16 bit compression of the delta values
			if ( fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) || fabs( v.z ) >= ( 1 / 4096.0f ) )
			{
				m_positions[ nNonZero ] = v;
				pIndices[ nNonZero ] = i;
				++nNonZero;
			}
		}

		pDelta = pMesh->FindOrCreateDeltaState( pName );
		pDelta->FlipVCoordinate( true );
		pDelta->SetValue( "corrected", !bAbsolute );

		const FieldIndex_t pIndex( pDelta->CreateField( CDmeVertexData::FIELD_POSITION ) );
		pDelta->AddVertexData( pIndex, nNonZero );
		pDelta->SetVertexData( pIndex, 0, nNonZero, AT_VECTOR3, m_positions.Base() );
		pDelta->SetVertexIndices( pIndex, 0, nNonZero, pIndices );

		const FieldIndex_t nBindNormalIndex = pBind->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
		if ( nBindNormalIndex >= 0 )
		{
			CDmrArrayConst< Vector > bindNormalData( pBind->GetVertexData( nBindNormalIndex ) );
			const int nNormalCount = m_normals.Count();
			if ( bindNormalData.Count() == nNormalCount )
			{
				for ( int i = 0; i < nNormalCount; ++i )
				{
					m_normals[ i ] -= bindNormalData[ i ];
				}

				int *pNormalIndices = reinterpret_cast< int * >( stackalloc( nNormalCount * sizeof( int ) ) );
				int nNormalDeltaCount = 0;
				for ( int i = 0; i < nNormalCount; ++i )
				{
					const Vector &n = m_normals[ i ];
					// Kind of a magic number but it's because of 16 bit compression of the delta values
					if ( fabs( n.x ) >= ( 1 / 4096.0f ) || fabs( n.y ) >= ( 1 / 4096.0f ) || fabs( n.z ) >= ( 1 / 4096.0f ) )
					{
						m_normals[ nNormalDeltaCount ] = n;
						pNormalIndices[ nNormalDeltaCount ] = i;
						++nNormalDeltaCount;
					}
				}
			}
		}
	}

	return pDelta;
}


//-----------------------------------------------------------------------------
// Convert from DME -> OBJ
//-----------------------------------------------------------------------------
bool CDmObjSerializer::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	return false; // For now
}


//-----------------------------------------------------------------------------
// Convert from OBJ -> DME
//-----------------------------------------------------------------------------
bool CDmObjSerializer::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
									const char *pSourceFormatName, int nSourceFormatVersion,
									DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = ReadOBJ( buf, fileid, "bind" );
	return *ppRoot != NULL;
}


//-----------------------------------------------------------------------------
// Convert from OBJ -> DME
// If mesh is not NULL, the OBJ is added as a delta state to the mesh
//-----------------------------------------------------------------------------
CDmElement *CDmObjSerializer::ReadOBJ(
	const char *pFilename,
	CDmeMesh **ppCreatedMesh,
	bool bLoadAllDeltas /* = true */,
	bool bAbsolute /* = true */ )
{
	char filename[ MAX_PATH ];
	Q_strncpy( filename, pFilename, sizeof( filename ) );
	Q_FixSlashes( filename );

	CUtlBuffer utlBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( filename, NULL, utlBuf ) )
		return NULL;

	char baseFile[ MAX_PATH ];
	Q_FileBase( filename, baseFile, sizeof( baseFile ) );

	CDmeMesh *pMesh( NULL );

	DmFileId_t nFileId = g_pDataModel->FindOrCreateFileId( pFilename );

	CDmElement *pRoot = ReadOBJ( utlBuf, nFileId, baseFile, filename, NULL, &pMesh, bAbsolute );
	if ( pRoot && pMesh )
	{
		if ( ppCreatedMesh )
		{
			*ppCreatedMesh = pMesh;
		}

		CDmeCombinationOperator *pCombo( NULL );

		// Check if there are deltas in the directory with the same prefix
		// But only if the rest of the file is <prefix>=<suffix>.obj or is <prefix>_zero.obj

		char *pSuffix = Q_strrchr( baseFile, '=' );
		if ( !pSuffix || !*pSuffix )
		{
			pSuffix = Q_strrchr( baseFile, '_' );
			if ( !pSuffix || !*pSuffix )
				return pRoot;

			if ( Q_stricmp( pSuffix, "_zero" ) )
				return pRoot;
		}

		char findGlob[ MAX_PATH ];
		Q_strncpy( findGlob, baseFile, sizeof( findGlob ) );
		pSuffix = findGlob + ( pSuffix - baseFile );
		*( pSuffix + 0 ) = '_';	// Just in case it was <prefix>=<suffix>.obj
		*( pSuffix + 1 ) = '*';
		*( pSuffix + 2 ) = '.';
		Q_strncpy( pSuffix + 3, Q_GetFileExtension( filename ), sizeof( findGlob ) - ( pSuffix - findGlob + 3 ) );

		char path[ MAX_PATH ];
		Q_ExtractFilePath( filename, path, sizeof( path ) );

		m_objDirectory = path;

		char findPath[ MAX_PATH ];
		Q_ComposeFileName( path, findGlob, findPath, sizeof( findPath ) );

		FileFindHandle_t hFind;

		char deltaFile[ MAX_PATH ];
		char deltaPath[ MAX_PATH ];

		for ( const char *pFindFile( g_pFullFileSystem->FindFirst( findPath, &hFind ) ); pFindFile && *pFindFile; pFindFile = g_pFullFileSystem->FindNext( hFind ) )
		{
			Q_FileBase( pFindFile, deltaFile, sizeof( deltaFile ) );

			if ( Q_stricmp( baseFile, deltaFile ) )
			{
				Q_ComposeFileName( path, pFindFile, deltaPath, sizeof( deltaPath ) );

				if ( !g_pFullFileSystem->FileExists( deltaPath ) )
					continue;

				char *pControlName = strchr( deltaFile, '_' );
				if ( pControlName && *( pControlName + 1 ) )
				{
					++pControlName;
					char *pDeltaName( pControlName );
					for ( char *pPlus( strchr( pDeltaName, '+' ) ); pPlus; pPlus = strchr( pPlus, '+' ) )
					{
						*pPlus = '_';
					}
				}

				if ( !strchr( pControlName, '_' ) )
				{
					if ( pCombo == NULL )
					{
						pCombo = CreateElement< CDmeCombinationOperator >( "combinationOperator", pRoot->GetFileId() );
						pRoot->SetValue( "combinationOperator", pCombo );
					}
				}

				DeltaInfo_t &deltaInfo = m_deltas[ pControlName ];
				deltaInfo.m_filename = pFindFile;
				deltaInfo.m_pMesh = pMesh;
				deltaInfo.m_pComboOp = pCombo;

				if ( bLoadAllDeltas )
				{
					GetDelta( pControlName, bAbsolute );
				}
			}
		}

		g_pFullFileSystem->FindClose( hFind );

		if ( pCombo )
		{
			pCombo->AddTarget( pMesh );
			pMesh->ComputeAllCorrectedPositionsFromActualPositions();
		}
	}

	return pRoot;
}


//-----------------------------------------------------------------------------
// Common function both ReadOBJ & Unserialize can call
//-----------------------------------------------------------------------------
CDmElement *CDmObjSerializer::ReadOBJ( CUtlBuffer &buf,
	DmFileId_t dmFileId,
	const char *pName,
	const char *pFilename /* = NULL */,
	CDmeMesh *pBaseMesh /* = NULL */,
	CDmeMesh **ppCreatedMesh /* = NULL */,
	bool bAbsolute /* = true */ )
{
	CDmElement *pRoot( NULL );
	CDmeModel *pModel( NULL );

	if ( !pBaseMesh )
	{
		pRoot = CreateElement< CDmElement >( "root", dmFileId );
		pModel = CreateElement< CDmeModel >( "model", dmFileId );

		pRoot->SetValue( "skeleton", pModel );
		pRoot->SetValue( "model", pModel );
	}

	m_mtlLib.RemoveAll();

	char tmpBuf0[ 4096 ];
	char tmpBuf1[ 4096 ];

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "/\\" );

	const char *pBuf;

	Vector p;
	Vector2D uv;

	CVertexData vertexData;
	CFaceSetData faceSetData;

	CUtlString groupName;

	CDmeDag *pDmeDag( NULL );
	CDmeMesh *pDmeMesh( NULL );
	CUtlVector< int > *pFaceIndices( NULL );

	while ( buf.IsValid() )
	{
		buf.GetLine( tmpBuf0, sizeof( tmpBuf0 ) );

		pBuf = SkipSpace( tmpBuf0 );

		if ( sscanf( tmpBuf0, "v %f %f %f", &p.x, &p.y, &p.z ) == 3 )
		{
			if ( pDmeDag )
			{
				vertexData.AddToMesh( pDmeMesh, bAbsolute, "bind", false );
				faceSetData.AddToMesh( pDmeMesh );

				pDmeDag = NULL;
				pDmeMesh = NULL;
				pFaceIndices = NULL;
			}

			vertexData.AddPosition( p );

			continue;
		}

		if ( sscanf( pBuf, "vn %f %f %f", &p.x, &p.y, &p.z ) == 3 )
		{
			vertexData.AddNormal( p );
			continue;
		}

		if ( !pBaseMesh )
		{
			if ( sscanf( pBuf, "vt %f %f", &uv.x, &uv.y ) == 2 )
			{
				vertexData.AddUV( uv );
				continue;
			}

			if ( pFilename && sscanf( pBuf, "mtllib %4096s", tmpBuf1 ) == 1 )
			{
				CUtlString mtlLib( tmpBuf1 );

				Q_strncpy( tmpBuf0, pFilename, sizeof( tmpBuf0 ) );
				Q_FixSlashes( tmpBuf0 );
				Q_StripFilename( tmpBuf0 );

				char mtlLibPath[ MAX_PATH ];

				Q_ComposeFileName( tmpBuf0, tmpBuf1, mtlLibPath, sizeof( mtlLibPath ) );
				CUtlBuffer utlBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );

				if ( g_pFullFileSystem->ReadFile( mtlLibPath, NULL, utlBuf ) )
				{
					ParseMtlLib( utlBuf );
				}

				continue;
			}

			if ( sscanf( pBuf, "usemtl %4096s", tmpBuf1 ) == 1 )
			{
				// Remove any 'SG' suffix from the material
				const uint sLen = Q_strlen( tmpBuf1 );
				if ( sLen && !Q_strcmp( tmpBuf1 + sLen - 2, "SG" ) )
				{
					tmpBuf1[ sLen - 2 ] = '\0';
				}

				const char *pTexture( FindMtlEntry( tmpBuf1 ) );
				if ( pTexture )
				{
					pFaceIndices = faceSetData.GetFaceSetIndices( pTexture );
				}
				else
				{
					pFaceIndices = faceSetData.GetFaceSetIndices( tmpBuf1 );
				}

				continue;
			}

			if ( sscanf( pBuf, "g %4096s", tmpBuf1 ) == 1 )
			{
				groupName = tmpBuf1;
				if ( pFaceIndices == NULL )
				{
					pFaceIndices = faceSetData.GetFaceSetIndices( tmpBuf1 );
				}
				continue;
			}
			if ( *pBuf == 'f' && ( *( pBuf + 1 ) == ' ' || *( pBuf + 1 ) == '\t' ) )
			{
				if ( pDmeDag == NULL )
				{
					pDmeDag = CreateElement< CDmeDag >( pName ? pName : ( groupName.IsEmpty() ? "obj" : groupName.Get() ), pRoot->GetFileId() );
					Assert( pDmeDag );
					pDmeMesh = CreateElement< CDmeMesh >( pName ? pName : ( groupName.IsEmpty() ? "obj" : groupName.Get() ), pRoot->GetFileId() );
					if ( ppCreatedMesh && *ppCreatedMesh == NULL )
					{
						// Only the first mesh created...
						*ppCreatedMesh = pDmeMesh;
					}
					pDmeDag->SetShape( pDmeMesh );
					if ( pModel )
					{
						pModel->AddJoint( pDmeDag );
						pModel->AddChild( pDmeDag );
					}
				}

				if ( pFaceIndices == NULL )
				{
					pFaceIndices = faceSetData.GetFaceSetIndices( "facetSet" );
				}

				int v;
				int t;
				int n;

				pBuf = SkipSpace( pBuf + 1 );
				int nLen = Q_strlen( pBuf );

				CUtlBuffer bufParse( pBuf, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );

				while ( bufParse.IsValid() )
				{
					if ( !ParseVertex( bufParse, breakSet, v, t, n ) )
						break;

					pFaceIndices->AddToTail( vertexData.VertexCount() );
					if ( v > 0 )
					{
						vertexData.AddPositionIndex( v - 1 );
					}

					if ( n > 0 )
					{
						vertexData.AddNormalIndex( n - 1 );
					}

					if ( t > 0 )
					{
						vertexData.AddUVIndex( t - 1 );
					}
				}

				pFaceIndices->AddToTail( -1 );
				continue;
			}
		}
	}

	CDmeVertexDataBase *pVertexData( NULL );

	if ( pBaseMesh )
	{
		pVertexData = vertexData.AddToMesh( pBaseMesh, bAbsolute, pName, true );
	}
	else
	{
		pVertexData = vertexData.AddToMesh( pDmeMesh, bAbsolute, "bind", false );
		faceSetData.AddToMesh( pDmeMesh );
	}

	if ( pModel )
	{
		pModel->CaptureJointsToBaseState( "bind" );
	}

	if ( pBaseMesh )
		return pVertexData;

	return pRoot;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmObjSerializer::OutputVectors(
	CUtlBuffer &b,
	const char *pPrefix,
	const CUtlVector< Vector > &vData,
	const matrix3x4_t &matrix )
{
	Vector v;

	const int nv( vData.Count() );

	for ( int i( 0 ); i < nv; ++i )
	{
		VectorTransform( vData[ i ], matrix, v );
		b << pPrefix << v << "\n";
	}

	return nv;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmObjSerializer::OutputVectors(
	CUtlBuffer &b,
	const char *pPrefix,
	const CUtlVector< Vector2D > &vData )
{
	Vector v;

	const int nv( vData.Count() );

	for ( int i( 0 ); i < nv; ++i )
	{
		b << pPrefix << vData[ i ] << "\n";
	}

	return nv;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmObjSerializer::MeshToObj(
	CUtlBuffer &b,
	const matrix3x4_t &parentWorldMatrix,
	CDmeMesh *pMesh,
	const char *pDeltaName,
	bool absolute )
{
	CUtlVector< CDmeMesh::DeltaComputation_t > compList;

	if ( pDeltaName )
	{
		pMesh->ComputeDependentDeltaStateList( compList );
	}

	const int nCompList( compList.Count() );

	CDmeVertexData *pBase( pMesh->FindBaseState( "bind" ) );
	if ( !pBase )
		return;

	const FieldIndex_t nPosIndex( pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );
	if ( nPosIndex < 0 )
		return;

	int nPositionCount = 0;
	int nTextureCount = 0;
	int nNormalCount = 0;

	b << "g " << pMesh->GetName() << "\n";

	CDmrArrayConst< Vector > pArray( pBase->GetVertexData( nPosIndex ) );
	const CUtlVector< int > *ppIndices( &pBase->GetVertexIndexData( nPosIndex ) );
	const CUtlVector< Vector > &pConstData( pArray.Get() );

	if ( nCompList )
	{
		CUtlVector< Vector > pData;
		pData.CopyArray( pConstData.Base(), pConstData.Count() );

		if ( absolute )
		{
			for ( int i ( 0 ); i < nCompList; ++i )
			{
				CDmeVertexDeltaData *pTmpDeltaState( pMesh->GetDeltaState( compList[ i ].m_nDeltaIndex ) );
				if ( Q_strcmp( pTmpDeltaState->GetName(), pDeltaName ) )
					continue;

				b << "# Delta: " << pTmpDeltaState->GetName() << "\n";
				pMesh->AddDelta( pTmpDeltaState, pData.Base(), pData.Count(), CDmeVertexData::FIELD_POSITION, 1.0f );

				const CUtlVector< int > &depDeltas( compList[ i ].m_DependentDeltas );
				const int nDepDeltas( depDeltas.Count() );
				for ( int j( 0 ); j < nDepDeltas; ++j )
				{
					pTmpDeltaState = pMesh->GetDeltaState( depDeltas[ j ] );
					b << "# Dependent Delta: " << pTmpDeltaState->GetName() << "\n";
					pMesh->AddDelta( pTmpDeltaState, pData.Base(), pData.Count(), CDmeVertexData::FIELD_POSITION, 1.0f );
				}
			}
		}
		else
		{
			for ( int i ( 0 ); i < nCompList; ++i )
			{
				CDmeVertexDeltaData *pTmpDeltaState( pMesh->GetDeltaState( compList[ i ].m_nDeltaIndex ) );
				if ( Q_strcmp( pTmpDeltaState->GetName(), pDeltaName ) )
					continue;

				b << "# Delta: " << pTmpDeltaState->GetName() << "\n";
				pMesh->AddDelta( pTmpDeltaState, pData.Base(), pData.Count(), CDmeVertexData::FIELD_POSITION, 1.0f );
			}
		}

		nPositionCount = OutputVectors( b, "v ", pData, parentWorldMatrix );
	}
	else
	{
		nPositionCount = OutputVectors( b, "v ", pConstData, parentWorldMatrix );
	}

	const CUtlVector< int > *puvIndices( NULL );
	const FieldIndex_t uvIndex( pBase->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD ) );
	if ( uvIndex >= 0 )
	{
		CDmrArrayConst< Vector2D > uvArray( pBase->GetVertexData( uvIndex ) );
		const CUtlVector< Vector2D > &uvData( uvArray.Get() );
		puvIndices = &pBase->GetVertexIndexData( uvIndex );

		nTextureCount = OutputVectors( b, "vt ", uvData );
	}

	const CUtlVector< int > *pnIndices( NULL );
	const FieldIndex_t nIndex( pBase->FindFieldIndex( CDmeVertexData::FIELD_NORMAL ) );
	if ( nIndex >= 0 )
	{
		matrix3x4_t normalMatrix;
		MatrixInverseTranspose( parentWorldMatrix, normalMatrix );

		CDmrArrayConst< Vector > nArray( pBase->GetVertexData( nIndex ) );
		const CUtlVector< Vector > &nConstData( nArray.Get() );
		pnIndices = &pBase->GetVertexIndexData( nIndex );

		if ( nCompList )
		{
			CUtlVector< Vector > nData;
			nData.CopyArray( nConstData.Base(), nConstData.Count() );

			if ( absolute )
			{
				for ( int i ( 0 ); i < nCompList; ++i )
				{
					CDmeVertexDeltaData *pTmpDeltaState( pMesh->GetDeltaState( compList[ i ].m_nDeltaIndex ) );
					if ( Q_strcmp( pTmpDeltaState->GetName(), pDeltaName ) )
						continue;

					b << "# Delta: " << pTmpDeltaState->GetName() << "\n";
					pMesh->AddDelta( pTmpDeltaState, nData.Base(), nData.Count(), CDmeVertexData::FIELD_NORMAL, 1.0f );

					const CUtlVector< int > &depDeltas( compList[ i ].m_DependentDeltas );
					const int nDepDeltas( depDeltas.Count() );
					for ( int j( 0 ); j < nDepDeltas; ++j )
					{
						pTmpDeltaState = pMesh->GetDeltaState( depDeltas[ j ] );
						b << "# Dependent Delta: " << pTmpDeltaState->GetName() << "\n";
						pMesh->AddDelta( pTmpDeltaState, nData.Base(), nData.Count(), CDmeVertexData::FIELD_NORMAL, 1.0f );
					}
				}
			}
			else
			{
				for ( int i ( 0 ); i < nCompList; ++i )
				{
					CDmeVertexDeltaData *pTmpDeltaState( pMesh->GetDeltaState( compList[ i ].m_nDeltaIndex ) );
					if ( Q_strcmp( pTmpDeltaState->GetName(), pDeltaName ) )
						continue;

					b << "# Delta: " << pTmpDeltaState->GetName() << "\n";
					pMesh->AddDelta( pTmpDeltaState, nData.Base(), nData.Count(), CDmeVertexData::FIELD_NORMAL, 1.0f );
				}
			}

			nNormalCount = OutputVectors( b, "vn ", nData, normalMatrix );
		}
		else
		{
			nNormalCount = OutputVectors( b, "vn ", nConstData, normalMatrix );
		}
	}

	const int pCount( ppIndices->Count() );
	const int uvCount( puvIndices ? puvIndices->Count() : 0 );
	const int nCount( pnIndices ? pnIndices->Count() : 0 );

	const int nFaceSets( pMesh->FaceSetCount() );
	for ( int i= 0 ; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet( pMesh->GetFaceSet( i ) );
		CDmeMaterial *pMaterial( pFaceSet->GetMaterial() );
		if ( pMaterial )
		{
			b << "usemtl " << pMaterial->GetMaterialName() << "\n";
		}

		const int nIndices( pFaceSet->NumIndices() );
		const int *pnFaceSetIndex( pFaceSet->GetIndices() );
		const int *const pEnd( pnFaceSetIndex + nIndices );
		int fIndex;

		const char *const pFaceStart( "f " );
		const char *const pFaceNext( " " );
		const char *pFaceSep( pFaceStart );

		if ( pCount == uvCount && pCount == nCount )
		{
			const CUtlVector< int > &pIndices( *ppIndices );
			const CUtlVector< int > &uvIndices( *puvIndices );
			const CUtlVector< int > &nvIndices( *pnIndices );

			while ( pnFaceSetIndex < pEnd )
			{
				fIndex = *pnFaceSetIndex++;
				if ( fIndex < 0 )
				{
					b << "\n";
					pFaceSep = pFaceStart;
					continue;
				}

				b << pFaceSep << ( pIndices[ fIndex ] + m_nPositionOffset ) << '/' << ( uvIndices[ fIndex ] + m_nTextureOffset ) << '/' << ( nvIndices[ fIndex ] + m_nNormalOffset );
				pFaceSep = pFaceNext;
			}
		}
		else if ( pCount == uvCount )
		{
			const CUtlVector< int > &pIndices( *ppIndices );
			const CUtlVector< int > &uvIndices( *puvIndices );

			while ( pnFaceSetIndex < pEnd )
			{
				fIndex = *pnFaceSetIndex++;
				if ( fIndex < 0 )
				{
					b << "\n";
					pFaceSep = pFaceStart;
					continue;
				}

				b << pFaceSep << ( pIndices[ fIndex ] + m_nPositionOffset ) << '/' << ( uvIndices[ fIndex ] + m_nTextureOffset ) << '/';
				pFaceSep = pFaceNext;
			}
		}
		else if ( pCount == nCount )
		{
			const CUtlVector< int > &pIndices( *ppIndices );
			const CUtlVector< int > &nvIndices( *pnIndices );

			while ( pnFaceSetIndex < pEnd )
			{
				fIndex = *pnFaceSetIndex++;
				if ( fIndex < 0 )
				{
					b << "\n";
					pFaceSep = pFaceStart;
					continue;
				}

				b << pFaceSep << ( pIndices[ fIndex ] + m_nPositionOffset ) << "//" << ( nvIndices[ fIndex ] + m_nNormalOffset );
				pFaceSep = pFaceNext;
			}
		}
		else
		{
			const CUtlVector< int > &pIndices( *ppIndices );

			while ( pnFaceSetIndex < pEnd )
			{
				fIndex = *pnFaceSetIndex++;
				if ( fIndex < 0 )
				{
					b << "\n";
					pFaceSep = pFaceStart;
					continue;
				}

				b << pFaceSep << ( pIndices[ fIndex ] + m_nPositionOffset ) << "//";
				pFaceSep = pFaceNext;
			}
		}
	}

	m_nPositionOffset += nPositionCount;
	m_nTextureOffset += nTextureCount;
	m_nNormalOffset += nNormalCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmObjSerializer::DagToObj(
	CUtlBuffer &b,
	const matrix3x4_t &parentWorldMatrix,
	CDmeDag *pDag,
	const char *pDeltaName,
	bool absolute )
{
	matrix3x4_t inclusiveMatrix;
	pDag->GetTransform()->GetTransform( inclusiveMatrix );

	ConcatTransforms( parentWorldMatrix, inclusiveMatrix, inclusiveMatrix );

	CDmeMesh *pMesh( CastElement< CDmeMesh >( pDag->GetShape() ) );
	if ( pMesh )
	{
		MeshToObj( b, inclusiveMatrix, pMesh, pDeltaName, absolute );
	}

	const int nChildren( pDag->GetChildCount() );
	for ( int i( 0 ); i < nChildren; ++i )
	{
		DagToObj( b, inclusiveMatrix, pDag->GetChild( i ), pDeltaName, absolute );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmObjSerializer::FindDeltaMeshes( CDmeDag *pDag, CUtlVector< CDmeMesh * > &meshes )
{
	CDmeMesh *pMesh( CastElement< CDmeMesh >( pDag->GetShape() ) );
	if ( pMesh && pMesh->DeltaStateCount() )
	{
		meshes.AddToTail( pMesh );
	}

	const int nChildren( pDag->GetChildCount() );
	for ( int i( 0 ); i < nChildren; ++i )
	{
		FindDeltaMeshes( pDag->GetChild( i ), meshes );
	}
}


//-----------------------------------------------------------------------------
// Convert from OBJ -> DME
//-----------------------------------------------------------------------------
bool CDmObjSerializer::WriteOBJ( const char *pFilename, CDmElement *pRoot, bool bWriteOBJs, const char *pDeltaName, bool absolute )
{
	CDmeDag *pModel = pRoot->GetValueElement< CDmeDag >( "model" );

	if ( !pModel )
		return false;

	matrix3x4_t identityMatrix;
	SetIdentityMatrix( identityMatrix );

	if ( !pDeltaName )
	{
		CUtlBuffer b( 0, 0, CUtlBuffer::TEXT_BUFFER );

		b << "# OBJ\n";
		b << "#\n";

		m_nPositionOffset = 1;	// OBJs start indexing at 1
		m_nTextureOffset = 1;
		m_nNormalOffset = 1;

		DagToObj( b, identityMatrix, pModel, pDeltaName, absolute );

		g_pFullFileSystem->WriteFile( pFilename, NULL, b );

		// Filesystem is silly
		// On WIN32 filesystem changes all of the characters to lowercase grrrr.....
		rename( pFilename, pFilename );
	}

	if ( !bWriteOBJs )
		return true;

	CUtlVector< CDmeMesh * > deltaMeshes;
	FindDeltaMeshes( pModel, deltaMeshes );

	if ( deltaMeshes.Count() )
	{
		char base[ MAX_PATH ];
		Q_FileBase( pFilename, base, sizeof( base ) );

		char path[ MAX_PATH ];
		Q_ExtractFilePath( pFilename, path, sizeof( path ) );

		char *pSuffix = strchr( base, '='	 );
		if ( !pSuffix )
		{
			pSuffix = strchr( base, '_' );
		}

		if ( pSuffix )
		{
			*( pSuffix + 0 ) = '_';
			*( pSuffix + 1 ) = '\0';
		}

		char filename[ MAX_PATH ];

		const int nDeltaMeshes( deltaMeshes.Count() );
		for ( int i( 0 ); i < nDeltaMeshes; ++i )
		{
			CDmeMesh *pDeltaMesh( deltaMeshes[ i ] );
			const int nDeltas( pDeltaMesh->DeltaStateCount() );
			for ( int j( 0 ); j < nDeltas; ++j )
			{
				CDmeVertexDeltaData *pDelta( pDeltaMesh->GetDeltaState( j ) );

				if ( !pDeltaName || !Q_strcmp( pDeltaName, pDelta->GetName() ) )
				{
					CUtlBuffer b( 0, 0, CUtlBuffer::TEXT_BUFFER );

					b << "# Delta OBJ: " << pDelta->GetName() << "\n";
					b << "#\n";

					Q_strncpy( filename, pDelta->GetName(), sizeof( filename ) );
					// Change _ to +
					const char *const pEnd( filename + sizeof( filename ) );
					for ( char *pChar = filename; *pChar && pChar < pEnd; ++pChar )
					{
						if ( *pChar == '_' )
						{
							*pChar = '+';
						}
					}

					CUtlString deltaFile( base );
					deltaFile += filename;
					deltaFile += ".obj";

					m_nPositionOffset = 1;	// OBJs use 1 based indexes
					m_nTextureOffset = 1;
					m_nNormalOffset = 1;

					DagToObj( b, identityMatrix, pModel, pDelta->GetName(), absolute );

					Q_ComposeFileName( path, deltaFile.Get(), filename, sizeof( filename ) );
					g_pFullFileSystem->WriteFile( filename, NULL, b );
					// On WIN32 filesystem changes all of the characters to lowercase grrrr.....
					rename( filename, filename );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmObjSerializer::ParseMtlLib( CUtlBuffer &buf )
{
	char tmpBuf0[ 4096 ];

	int nCurrentMtl = -1;
	while ( buf.IsValid() )
	{
		buf.GetLine( tmpBuf0, sizeof(tmpBuf0) );

		if ( StringHasPrefix( tmpBuf0, "newmtl " ) )
		{
			char mtlName[1024];
			if ( sscanf( tmpBuf0, "newmtl %s", mtlName ) == 1 )
			{
				// Remove any 'SG' suffix from the material
				const uint sLen = Q_strlen( mtlName );
				if ( sLen > 2 && !Q_strcmp( mtlName + sLen - 2, "SG" ) )
				{
					mtlName[ sLen - 2 ] = '\0';
				}

				nCurrentMtl = m_mtlLib.AddToTail( );
				m_mtlLib[nCurrentMtl].m_MtlName = mtlName;
				m_mtlLib[nCurrentMtl].m_TgaName = mtlName;
			}
			continue;
		}

		if ( StringHasPrefix( tmpBuf0, "map_Kd " ) )
		{
			if ( nCurrentMtl < 0 )
				continue;

			char tgaPath[MAX_PATH];
			char tgaName[1024];
			if ( sscanf( tmpBuf0, "map_Kd %s", tgaPath ) == 1 )
			{
				// Try a cheesy hack - look for /materialsrc/ and set the material name off the entire path minus extension
				Q_strncpy( tmpBuf0, tgaPath, sizeof( tmpBuf0 ) );
				Q_FixSlashes( tmpBuf0, '/' );
				const char *pMaterialSrc = Q_strstr( tmpBuf0, "/materialsrc/" );
				if ( pMaterialSrc )
				{
					pMaterialSrc += Q_strlen( "/materialsrc/" );
					Q_StripExtension( pMaterialSrc, tgaName, sizeof( tgaName) );
					m_mtlLib[ nCurrentMtl ].m_TgaName = tgaName;
				}
				else
				{
					Q_FileBase( tgaPath, tgaName, sizeof(tgaName) );
					m_mtlLib[nCurrentMtl].m_TgaName = tgaName;
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmObjSerializer::FindMtlEntry( const char *pTgaName )
{
	int nCount = m_mtlLib.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_mtlLib[i].m_MtlName, pTgaName ) )
			return m_mtlLib[i].m_TgaName;
	}
	return pTgaName;
}									 


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmObjSerializer::ParseVertex( CUtlBuffer& bufParse, characterset_t &breakSet, int &v, int &t, int &n )
{
	char	cmd[1024];
	int nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
	if ( nLen <= 0 )
		return false;

	v = atoi( cmd );
	n = 0;
	t = 0;

	char c = *(char*)bufParse.PeekGet();
	bool bHasTexCoord = IN_CHARACTERSET( breakSet, c ) != 0;
	bool bHasNormal = false;
	if ( bHasTexCoord )
	{
		// Snag the '/'
		nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
		Assert( nLen == 1 );

		c = *(char*)bufParse.PeekGet();
		if ( !IN_CHARACTERSET( breakSet, c ) )
		{
			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen > 0 );
			t = atoi( cmd );

			c = *(char*)bufParse.PeekGet();
			bHasNormal = IN_CHARACTERSET( breakSet, c ) != 0;
		}
		else
		{
			bHasNormal = true;
			bHasTexCoord = false;
		}

		if ( bHasNormal )
		{
			// Snag the '/'
			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen == 1 );

			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen > 0 );
			n = atoi( cmd );
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmObjSerializer::SkipSpace(
	const char *pBuf )
{
	while ( *pBuf == ' ' || *pBuf == '\t' )
		++pBuf;

	return pBuf;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CDmObjSerializer::GetDelta( const char *pDeltaName, bool bAbsolute )
{
	if ( !m_deltas.Defined( pDeltaName ) )
		return NULL;

	DeltaInfo_t &deltaInfo( m_deltas[ pDeltaName ] );

	if ( deltaInfo.m_pDeltaData )
		return deltaInfo.m_pDeltaData;

	if ( !LoadDependentDeltas( pDeltaName ) )
		return NULL;

	CUtlBuffer utlBuf;

	char deltaPath[ MAX_PATH ];
	Q_ComposeFileName( m_objDirectory, deltaInfo.m_filename, deltaPath, sizeof( deltaPath ) );
	Q_FixSlashes( deltaPath );

	if ( !g_pFullFileSystem->ReadFile( deltaPath, NULL, utlBuf ) )
		return NULL;

	if ( deltaInfo.m_pComboOp && !strchr( pDeltaName, '_' ) )
	{
		deltaInfo.m_pComboOp->FindOrCreateControl( pDeltaName, false, true );
	}

	deltaInfo.m_pDeltaData = CastElement< CDmeVertexDeltaData >( ReadOBJ( utlBuf, deltaInfo.m_pMesh->GetFileId(), pDeltaName, deltaPath, deltaInfo.m_pMesh, NULL, bAbsolute ) );

	return deltaInfo.m_pDeltaData;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmObjSerializer::LoadDependentDeltas( const char *pDeltaName )
{
	// TODO: Load Dependent Deltas
	return true;
}


//-----------------------------------------------------------------------------
// Counts the number of _'s in a string
//-----------------------------------------------------------------------------
int ComputeDimensionality( const char *pDeltaName )
{
	const char *pUnderBar = pDeltaName;
	int nDimensions = 0;

	while ( pUnderBar )
	{
		++nDimensions;
		pUnderBar = strchr( pUnderBar, '_' );
		if ( pUnderBar )
		{
			++pUnderBar;
		}
	}

	return nDimensions;
}

/*
//-----------------------------------------------------------------------------
// Generates a sorted list in order of dimensionality of the delta states
// NOTE: This assumes a naming scheme where delta state names have _ that separate control names
//-----------------------------------------------------------------------------
void CDmObjSerializer::ComputeDeltaStateComputationList( CUtlVector< CUtlVector< int > > &dependentDeltaList )
{
	// Do all combinations in order of dimensionality, lowest dimension first

	for ( int i = 0; i < nDeltas; ++i )
	{
		compList[i].m_nDeltaIndex = i;
		compList[i].m_nDimensionality = ComputeDeltaStateDimensionality( i );
	}
	qsort( compList.Base(), nCount, sizeof(DeltaComputation_t), DeltaStateLessFunc );
}



{
	CUtlVector< CUtlString > atomicControls;
	deltaStateUsage.SetCount( nCount );

	// Build a list of atomic controls
	int nCurrentDelta;
	for ( nCurrentDelta = 0; nCurrentDelta < nCount; ++nCurrentDelta ) 
	{
		if ( pInfo[nCurrentDelta].m_nDimensionality != 1 )
			break;
		int j = atomicControls.AddToTail( GetDeltaState( pInfo[nCurrentDelta].m_nDeltaIndex )->GetName() );
		deltaStateUsage[ nCurrentDelta ].AddToTail( j );
	}

	for ( ; nCurrentDelta < nCount; ++nCurrentDelta )
	{
		CDmeVertexDeltaData *pDeltaState = GetDeltaState( pInfo[nCurrentDelta].m_nDeltaIndex );
		int nLen = Q_strlen( pDeltaState->GetName() );
		char *pTempBuf = (char*)_alloca( nLen + 1 );
		memcpy( pTempBuf, pDeltaState->GetName(), nLen+1 );
		char *pNext;
		for ( char *pUnderBar = pTempBuf; pUnderBar; pUnderBar = pNext )
		{
			pNext = strchr( pUnderBar, '_' );
			if ( pNext )
			{
				*pNext = 0;
				++pNext;
			}

			// Find this name in the list of strings
			int j;
			int nControlCount = atomicControls.Count();
			for ( j = 0; j < nControlCount; ++j )
			{
				if ( !Q_stricmp( pUnderBar, atomicControls[j] ) )
					break;
			}
			if ( j == nControlCount )
			{
				j = atomicControls.AddToTail( pUnderBar );
			}
			deltaStateUsage[ nCurrentDelta ].AddToTail( j );
		}
		deltaStateUsage[ nCurrentDelta ].Sort( DeltaStateUsageLessFunc );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmObjSerializer::ComputeDependentUsage( CUtlVector< CUtlVector< int > > &deltaUsage )
{
	const int nDeltas = m_deltas.GetNumStrings();
	compList.EnsureCount( nDeltas );

	CUtlVector< CUtlVector< int > > deltaStateUsage;
	const int nCount( compList.Count() );
	BuildAtomicControlLists( nCount, compList.Base(), deltaStateUsage );

	// Now build up a list of dependent delta states based on usage
	// NOTE: Usage is sorted in ascending order.
	for ( int i = 1; i < nCount; ++i )
	{
		int nUsageCount1 = deltaStateUsage[i].Count();
		for ( int j = 0; j < i; ++j )
		{
			// At the point they have the same dimensionality, no more need to check
			if ( compList[j].m_nDimensionality == compList[i].m_nDimensionality )
				break;

			int ii = 0;
			bool bSubsetFound = true;
			int nUsageCount2 = deltaStateUsage[j].Count();
			for ( int ji = 0; ji < nUsageCount2; ++ji )
			{
				for ( bSubsetFound = false; ii < nUsageCount1; ++ii )
				{
					if ( deltaStateUsage[j][ji] == deltaStateUsage[i][ii] )
					{
						++ii;
						bSubsetFound = true;
						break;
					}

					if ( deltaStateUsage[j][ji] < deltaStateUsage[i][ii] )
						break;
				}

				if ( !bSubsetFound )
					break;
			}

			if ( bSubsetFound )
			{
				compList[i].m_DependentDeltas.AddToTail( compList[j].m_nDeltaIndex );
			}
		}
	}
}
*/