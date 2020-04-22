//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Read SMD and create DMX data
//
//=============================================================================


// Because we use STL
#pragma warning( disable: 4530 )


// Standard includes
#include <io.h>
#include <algorithm>
#include <deque>
#include <fstream>
#include <list>
#include <map>
#include <string>


// Valve includes
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmsmdserializer.h"
#include "filesystem.h"
#include "tier1/characterset.h"
#include "tier1/fmtstr.h"
#include "tier2/tier2.h"
#include "mathlib/mathlib.h"


// Last include
#include "tier0/memdbgon.h"


//=============================================================================
//
// CDmSmdSerializer
//
//=============================================================================
//-----------------------------------------------------------------------------
// Convert from SMD -> DME
//-----------------------------------------------------------------------------
bool CDmSmdSerializer::Unserialize(
	CUtlBuffer &utlBuf,
	const char * /* pszEncodingName */,
	int /* nEncodingVersion */,
	const char * /* pszSourceFormatName */,
	int /* nSourceFormatVersion */,
	DmFileId_t nDmFileId,
	DmConflictResolution_t /* nDmConflictResolution */,
	CDmElement **ppDmRoot )
{
	if ( !ppDmRoot )
		return false;

	const char *pszFilename = g_pDataModel->GetFileName( nDmFileId );

	if ( pszFilename )
	{
		char szFilename[ MAX_PATH ];
		V_strncpy( szFilename, pszFilename, ARRAYSIZE( szFilename ) );
		V_FixSlashes( szFilename );

		*ppDmRoot = ReadSMD( utlBuf, nDmFileId, szFilename, NULL );
	}
	else
	{
		*ppDmRoot = ReadSMD( utlBuf, nDmFileId, "utlBuffer", NULL );
	}

	return *ppDmRoot != NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmElement *CDmSmdSerializer::ReadSMD(
	const char *pszFilename,
	CDmeMesh **ppDmeMeshCreated /* = NULL */ )
{
	char szFilename[ MAX_PATH ];
	V_strncpy( szFilename, pszFilename, ARRAYSIZE( szFilename ) );
	V_FixSlashes( szFilename );

	CUtlBuffer utlBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	if ( !g_pFullFileSystem->ReadFile( szFilename, NULL, utlBuf ) )
		return NULL;

	DmFileId_t nDmFileId = g_pDataModel->FindOrCreateFileId( pszFilename );

	return ReadSMD( utlBuf, nDmFileId, szFilename, ppDmeMeshCreated );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmSmdSerializer::SetUpAxis( CDmSmdSerializer::Axis_t nUpAxis )
{
	m_nUpAxis = clamp( nUpAxis, X_AXIS, Z_AXIS );

	switch ( m_nUpAxis )
	{
	case X_AXIS:	// X Up
		AngleMatrix( RadianEuler( -M_PI / 2.0, M_PI / 2.0, 0.0 ), m_mAdj );
		MatrixInverseTranspose( m_mAdj, m_mAdjNormal );
		break;
	case Y_AXIS:	// Y Up
		SetIdentityMatrix( m_mAdj );
		SetIdentityMatrix( m_mAdjNormal );
		break;
	case Z_AXIS:
	default:
		AngleMatrix( RadianEuler( -M_PI / 2.0, 0.0, 0.0 ), m_mAdj );
		MatrixInverseTranspose( m_mAdj, m_mAdjNormal );
		break;
	}
}


//-----------------------------------------------------------------------------
// Tests whether the passed buffer is an all whitespace line or not
//-----------------------------------------------------------------------------
static bool ParserIsBlankLine( const char *pszBuf )
{
	for ( const char *pChar = pszBuf; *pChar; ++pChar )
	{
		if ( !V_isspace( static_cast< unsigned char >( *pChar ) ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Skips over whitespace
//-----------------------------------------------------------------------------
static const char *ParserSkipSpace( const char *pszBuf )
{
	// Skip to first non-whitespace character
	for ( ;; )
	{
		if ( !V_isspace( static_cast< unsigned char >( *pszBuf ) ) )
			break;

		++pszBuf;
	}

	return pszBuf;
}


//-----------------------------------------------------------------------------
// Returns true the specified buffer is a comment line
// meaning that it starts with // (with optional white space preceding the //)
//-----------------------------------------------------------------------------
static bool IsCommentLine( const char *pszBuf )
{
	pszBuf = ParserSkipSpace( pszBuf );

	if ( *pszBuf && *pszBuf == '/' && *( pszBuf + 1 ) == '/' )
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool ParserHandleVersion( const char *pszBuf, int *pnVersion = NULL )
{
	pszBuf = ParserSkipSpace( pszBuf );

	if ( !( *pszBuf && V_strnicmp( pszBuf, "version", 7 ) == 0 ) )
		return false;

	if ( pnVersion )
	{
		*pnVersion = strtol( pszBuf + 7, NULL, 0 );	// Skip past "version"
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool ParserHandleSectionStart( const char *pszBuf, const char *pszSectionId )
{
	pszBuf = ParserSkipSpace( pszBuf );

	if ( ! *pszBuf )
		return false;
	
	const int nCmd = V_stricmp( pszBuf, pszSectionId );
	if ( !nCmd )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool ParserHandleTime( const char *pszBuf, int &nTime )
{
	if ( sscanf( pszBuf, "time %d", &nTime ) == 1 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Strip trailing CR/NL
//-----------------------------------------------------------------------------
static void Chomp( char *pszBuf )
{
	char *pChar = pszBuf + V_strlen( pszBuf ) - 1;

	while ( pChar >= pszBuf && ( *pChar == '\n' || *pChar == '\r' ) )
	{
		*pChar = '\0';
	}
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void GetLine( CUtlBuffer &utlBuf, char *pszBuf, int nMaxLen )
{
	utlBuf.GetLine( pszBuf, nMaxLen );
	Chomp( pszBuf );
}


//=============================================================================
//
//=============================================================================
class CQcData
{
public:
	CQcData()
	: m_nUpAxis( CDmSmdSerializer::Z_AXIS )
	, m_scale( 1.0f )
	{}

	bool ParseQc( const CUtlString &smdPath, const CUtlString &qcPath );

	bool GetQcData( const CUtlString &smdPath );

	CDmSmdSerializer::Axis_t m_nUpAxis;
	float m_scale;
	std::list< std::string > m_cdmaterials;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool HandleQcHints(
	const char *pBuf,
	CQcData &qcData )
{
	if ( !IsCommentLine( pBuf ) )
		return false;

	char key[ 512 ];
	key[0] = '\0';

	char val[ 512 ];
	val[0] = '\0';

	if ( sscanf( pBuf, "// %511s=%511s", key, val ) == 2 )
	{
		if ( Q_stricmp( key, "UPAXIS" ) == 0 )
		{
			if ( strpbrk( val, "xX" ) )
			{
				qcData.m_nUpAxis = CDmSmdSerializer::X_AXIS;
			}
			else if ( strpbrk( val, "yY" ) )
			{
				qcData.m_nUpAxis = CDmSmdSerializer::Y_AXIS;
			}
			else if ( strpbrk( val, "zZ" ) )
			{
				qcData.m_nUpAxis = CDmSmdSerializer::Z_AXIS;
			}
		}
	}

	key[ ARRAYSIZE(key) - 1 ] = '\0';
	val[ ARRAYSIZE(val) - 1 ] = '\0';
	
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void Tokenize( CUtlVector< CUtlString > &tokens, const char *pszBuf )
{
	tokens.RemoveAll();
	CUtlString strBuf( pszBuf );

	for ( char *pszToken = strtok( (char *)strBuf.Get(), " \t\n" ); pszToken; pszToken = strtok( NULL, " \t\n" ) )
	{
		tokens.AddToTail( pszToken );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmSmdSerializer::ParserGetNodeName( const char *pszBuf, CUtlString &sName ) const
{
	sName = ParserSkipSpace( pszBuf );
	FixNodeName( sName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmSmdSerializer::ParserHandleSkeletonLine(
	const char *pszBuf,
	CUtlString &sName,
	int &nId,
	int &nParentId ) const
{
	const char *pszOrigBuf = pszBuf;

	pszBuf = ParserSkipSpace( pszBuf );

	char szTmpBuf[ 512 ];
	if ( sscanf( pszBuf, "%d \"%[^\"]\" %d", &nId, szTmpBuf, &nParentId ) == 3 )
	{
		ParserGetNodeName( szTmpBuf, sName );
		return true;
	}

	Warning( "Warning! Ignoring malformed skeleton line: %s\n", pszOrigBuf );

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CUtlString PathJoin( const char *pszStr1, const char *pszStr2 )
{
	char szPath[MAX_PATH];
	V_ComposeFileName( pszStr1, pszStr2, szPath, sizeof( szPath ) );
	return CUtlString( szPath );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CQcData::ParseQc(
	const CUtlString &smdPath,
	const CUtlString &qcPath )
{
	bool bRetVal = false;

	if ( _access( qcPath.Get(), 04 ) == 0 )
	{
		try
		{
			std::string buf;
			std::ifstream ifs( qcPath.Get() );

			CUtlVector< CUtlString > tokens;

			while ( std::getline( ifs, buf ) )
			{
				Tokenize( tokens, buf.c_str() );

				if ( tokens.Count() < 1 )
					continue;

				if ( !V_stricmp( tokens[0], "$upaxis" ) )
				{
					if ( strchr( tokens[1].Get(), 'y' ) || strchr( tokens[1].Get(), 'Y' ) )
					{
						m_nUpAxis = CDmSmdSerializer::Y_AXIS;
					}
					else if ( strchr( tokens[1].Get(), 'x' ) || strchr( tokens[1].Get(), 'X' ) )
					{
						m_nUpAxis = CDmSmdSerializer::X_AXIS;
					}
					else
					{
						m_nUpAxis = CDmSmdSerializer::Z_AXIS;
					}
				}
				else if ( !V_stricmp( tokens[0], "$scale" ) )
				{
					m_scale = strtod( tokens[1].Get(), NULL );
				}
				else if ( !V_stricmp( tokens[0], "$cdmaterials" ) )
				{
					m_cdmaterials.push_back( tokens[1].Get() );
				}
			}

			bRetVal = true;
		}
		catch ( ... )
		{
		}
	}

	if ( m_cdmaterials.empty() )
	{
		// If m_cdmaterials is empty, then put the relative smd path onto the cdmaterials path

		char szBuf0[MAX_PATH];
		V_strncpy( szBuf0, smdPath.Get(), ARRAYSIZE( szBuf0 ) );
		V_StripFilename( szBuf0 );
		V_FixSlashes( szBuf0, '/' );

		CUtlVector< char *, CUtlMemory< char *, int > > sPathArray;
		V_SplitString( szBuf0, "/", sPathArray );

		CUtlString sRelSmdPath;

		bool bJoin = false;
		for ( int i = 0; i < sPathArray.Count(); ++i )
		{
			if ( !bJoin && !V_stricmp( sPathArray[i], "models" ) )
			{
				bJoin = true;
			}

			if ( bJoin )
			{
				sRelSmdPath = PathJoin( sRelSmdPath.Get(), sPathArray[i] );
			}
		}
		sRelSmdPath.FixSlashes( '/' );

		m_cdmaterials.push_back( sRelSmdPath.Get() );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CQcData::GetQcData(
	const CUtlString &smdPath )
{
	try
	{
		// Look for same thing named with a .qc extension
		char szBuf0[MAX_PATH];
		char szBuf1[MAX_PATH];

		V_strncpy( szBuf0, smdPath.Get(), ARRAYSIZE( szBuf0 ) );
		V_SetExtension( szBuf0, ".qc", ARRAYSIZE( szBuf0 ) );
		if ( _access( szBuf0, 04 ) == 0 )
			return ParseQc( smdPath, szBuf0 );

		// Remove "_reference" if found
		char *pszRef = V_stristr( szBuf0, "_reference.qc" );
		if ( pszRef )
		{
			*pszRef = '\0';

			V_SetExtension( szBuf0, ".qc", ARRAYSIZE( szBuf0 ) );
			if ( _access( szBuf0, 04 ) == 0 )
				return ParseQc( smdPath, szBuf0 );
		}
		else
		{
			// Add _reference

			V_SetExtension( szBuf0, "", ARRAYSIZE( szBuf0 ) );
			V_strcat( szBuf0, "_reference", ARRAYSIZE( szBuf0 ) );
			V_SetExtension( szBuf0, ".qc", ARRAYSIZE( szBuf0 ) );
			if ( _access( szBuf0, 04 ) == 0 )
				return ParseQc( smdPath, szBuf0 );
		}

		// Look for any *.qc file in the same directory as the smd that contains the smd pathname
		V_strncpy( szBuf0, smdPath.Get(), ARRAYSIZE( szBuf0 ) );
		V_FixSlashes( szBuf0 );

		V_FileBase( szBuf0, szBuf1, ARRAYSIZE( szBuf1 ) );
		CUtlString sFileBase0( "\"" );
		sFileBase0 += szBuf1;
		sFileBase0 += "\"";

		V_SetExtension( szBuf1, ".smd", ARRAYSIZE( szBuf1 ) );
		CUtlString sFileBase1( "\"" );
		sFileBase1 += szBuf1;
		sFileBase1 += "\"";

		V_ExtractFilePath( szBuf0, szBuf1, ARRAYSIZE( szBuf1 ) );
		CUtlString sFilePath = szBuf1;

		if ( sFileBase0.Length() > 0 && sFilePath.Length() > 0 )
		{
			struct _finddata_t qcFile;
			long hFile;

			CUtlVector< CUtlString > tokens;

			/* Find first .qc file in current directory */

			CUtlString sQcGlob = sFilePath;
			sQcGlob += "*.qc";

			if ( ( hFile = _findfirst( sQcGlob.Get(), &qcFile ) ) != -1L )
			{
				/* Find the rest of the .qc files */
				do {
					CUtlString sQcFile = sFilePath;
					sQcFile += qcFile.name;

					std::ifstream ifs( sQcFile.Get() );
					std::string buf;

					while ( std::getline( ifs, buf ) )
					{
						if ( V_stristr( buf.c_str(), sFileBase0.Get() ) || V_stristr( buf.c_str(), sFileBase1.Get() ) )
						{
							_findclose( hFile );
							return ParseQc( smdPath, sQcFile );
						}
					}
				} while( _findnext( hFile, &qcFile ) == 0 );

				_findclose( hFile );
			}
		}
	}
	catch ( const std::exception &e )
	{
		Error( "Exception: %s\n", e.what() );
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define MAX_WEIGHTS_PER_VERTEX 3


//=============================================================================
//
//=============================================================================
class CVertexWeight
{
public:
	int m_nBoneIndex;
	float m_flWeight;

	CVertexWeight()
	: m_nBoneIndex( -1 )
	, m_flWeight( 0.0f )
	{}

	inline void Reset()
	{
		m_nBoneIndex = -1;
		m_flWeight = 0.0f;
	}

	inline bool operator==( const CVertexWeight &rhs ) const
	{
		return m_nBoneIndex == rhs.m_nBoneIndex && m_flWeight == rhs.m_flWeight;
	}

	inline bool operator!=( const CVertexWeight &rhs ) const
	{
		return m_nBoneIndex != rhs.m_nBoneIndex || m_flWeight != rhs.m_flWeight;
	}
};


//=============================================================================
//
//=============================================================================
class CVertex
{
public:
	Vector m_vPosition;
	Vector m_vNormal;
	Vector2D m_vUV;
	uint m_nWeights;
	CVertexWeight m_vertexWeights[ MAX_WEIGHTS_PER_VERTEX ];

	CVertex()
	{
		Reset();
	}

	inline bool operator==( const CVertex &rhs ) const
	{
		if ( m_vPosition != rhs.m_vPosition ||
			m_vNormal != rhs.m_vNormal ||
			m_vUV != rhs.m_vUV ||
			m_nWeights != rhs.m_nWeights )
			return false;

		for ( uint i = 0; i != m_nWeights; ++i )
		{
			if ( m_vertexWeights[ i ] != rhs.m_vertexWeights[ i ] )
				return false;
		}

		return true;
	}

	inline void Reset()
	{
		m_nWeights = 0;
		for ( int i = 0; i < ARRAYSIZE( m_vertexWeights ); ++i )
		{
			m_vertexWeights[i].Reset();
		}
	}
};


//=============================================================================
//
//=============================================================================
class CTriangle
{
public:
	uint m_nVertices;
	CVertex m_vertices[ 3 ];

	CTriangle()
	{
		Reset();
	}

	bool Valid() const;

	inline void Reset()
	{
		m_nVertices = 0;
		m_vertices[ 0U ].Reset();
		m_vertices[ 1U ].Reset();
		m_vertices[ 2U ].Reset();
	}
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTriangle::Valid() const
{
	// A triangle is valid it it has three vertices and all three vertices are unique

	if ( m_nVertices != 3U )
		return false;

	if ( m_vertices[0] == m_vertices[1] )
		return false;

	if ( m_vertices[1] == m_vertices[2] )
		return false;

	if ( m_vertices[2] == m_vertices[0] )
		return false;

	return true;
}


//=============================================================================
//
//=============================================================================
class CShadingGroup
{
public:
	std::string m_materialPath;
	std::map< int, std::deque< int > > m_componentListMap;
};


//=============================================================================
//
//=============================================================================
enum ParserState_t
{
	kUnknown,
	kPreamble,
	kGeneral,
	kNodes,
	kSkeleton,
	kTriangles
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool ParserAddJoint(
	CDmSmdSerializer::SmdJointMap_t &smdJointMap,
	int nId,
	const char *pszName,
	int nParentId,
	const char *pszFilename,
	int nLineNumber )
{
	if ( nId < 0 )
	{
		Error( "Error! %s(%d) : node error : invalid node id %d, must be >= 0: Line '%d \"%s\" %d'\n",
			pszFilename, nLineNumber, nId, nId, pszName, nParentId );
		return false;
	}

	if ( smdJointMap.IsValidIndex( smdJointMap.Find( nId ) ) )
	{
		Error( "Error! %s(%d) : node error : node id %d already defined\n",
			pszFilename, nLineNumber, nId );
		return false;
	}

	CDmSmdSerializer::SmdJoint_t &smdJoint = smdJointMap.Element( smdJointMap.Insert( nId ) );

	smdJoint.m_nId = nId;
	smdJoint.m_sName = pszName;
	smdJoint.m_nParentId = MAX( -1, nParentId );
	smdJoint.m_nLineNumber = nLineNumber;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool ParserCreateJoint(
	const CDmSmdSerializer::SmdJointMap_t &smdJointMap,
	CDmeModel *pDmeModel,
	CDmeChannelsClip *pDmeChannelsClip,
	CDmSmdSerializer::SmdJoint_t *pSmdJoint,
	const char *pszFilename )
{
	const CUtlString &sName = pSmdJoint->m_sName;
	const int nId = pSmdJoint->m_nId;
	const int nParentId = pSmdJoint->m_nParentId;
	const int nLineNumber = pSmdJoint->m_nLineNumber;

	CDmeJoint *pDmeJoint = CreateElement< CDmeJoint >( sName.Get(), pDmeModel->GetFileId() );

	if ( !pDmeJoint )
	{
		Error( "%s(%d) : node error : can't create joint %d(%s)\n",
			pszFilename, nLineNumber, nId, sName.Get() );
		return false;
	}

	if ( nParentId < 0 )
	{
		if ( !pDmeModel )
		{
			Error( "%s(%d) : node error : No DmeModel passed for root joint %d(%s)\n",
				pszFilename, nLineNumber, nId, sName.Get() );
			DestroyElement( pDmeJoint );

			return false;
		}

		pDmeModel->AddChild( pDmeJoint );
		CDmAttribute *pRootJointAttr = pDmeJoint->AddAttribute( "__rootJoint", AT_BOOL );
		pRootJointAttr->AddFlag( FATTRIB_DONTSAVE );
		pRootJointAttr->SetValue( true );
	}
	else
	{
		if ( nParentId == nId )
		{
			Error( "%s(%d) : node error : joint %d(%s) is its own parent\n",
				pszFilename, nLineNumber, nId, sName.Get() );
			DestroyElement( pDmeJoint );

			return false;
		}

		const int nParentIdIndex = smdJointMap.Find( nParentId );

		if ( !smdJointMap.IsValidIndex( nParentIdIndex ) )
		{
			Error( "%s(%d) : node error : joint %d(%s) has invalid parentId (%d)\n",
				pszFilename, nLineNumber, nId, sName.Get(), nParentId );
			DestroyElement( pDmeJoint );

			return false;
		}

		CDmeDag *pDmeDagParent = smdJointMap.Element( nParentIdIndex ).m_pDmeDag;
		if ( pDmeDagParent )
		{
			pDmeDagParent->AddChild( pDmeJoint );
		}
		else
		{
			Error( "%s(%d) : node error : joint %d(%s) has invalid parentId (%d)\n",
				pszFilename, nLineNumber, nId, sName.Get(), nParentId );
		}
	}

	if ( pDmeChannelsClip )
	{
		CDmeTransform *pDmeTransform = pDmeJoint->GetTransform();

		CDmeChannel *pDmePosChannel = CreateElement< CDmeChannel >( CFmtStr( "%s_p", pDmeTransform->GetName() ).Access(), pDmeChannelsClip->GetFileId() );
		pDmePosChannel->SetMode( CM_PLAY );
		pDmePosChannel->SetOutput( pDmeTransform, "position" );
		CDmeVector3Log *pDmePosLog = pDmePosChannel->CreateLog< Vector >();
		pDmePosLog->SetValueThreshold( 1.0e-6 );
		pDmeChannelsClip->m_Channels.AddToTail( pDmePosChannel );

		CDmAttribute *pPosLogAttr = pDmeJoint->AddAttribute( "__posLog", AT_ELEMENT );
		pPosLogAttr->AddFlag( FATTRIB_DONTSAVE );
		pPosLogAttr->SetValue( pDmePosLog );

		CDmeChannel *pDmeRotChannel = CreateElement< CDmeChannel >( CFmtStr( "%s_o", pDmeTransform->GetName() ).Access(), pDmeChannelsClip->GetFileId() );
		pDmeRotChannel->SetMode( CM_PLAY );
		pDmeRotChannel->SetOutput( pDmeTransform, "orientation" );
		CDmeQuaternionLog *pDmeRotLog = pDmeRotChannel->CreateLog< Quaternion >();
		pDmeRotLog->SetValueThreshold( 1.0e-6 );
		pDmeChannelsClip->m_Channels.AddToTail( pDmeRotChannel );

		CDmAttribute *pRotLogAttr = pDmeJoint->AddAttribute( "__rotLog", AT_ELEMENT );
		pRotLogAttr->AddFlag( FATTRIB_DONTSAVE );
		pRotLogAttr->SetValue( pDmeRotLog );
	}

	pSmdJoint->m_pDmeDag = pDmeJoint;
	pDmeModel->AddJoint( pDmeJoint );

	return true;
}


//-----------------------------------------------------------------------------
// Used by ParserAddJoints, sorts joints by parent index
//-----------------------------------------------------------------------------
static bool SmdJointLessFunc( const CDmSmdSerializer::SmdJoint_t *pLhs, const CDmSmdSerializer::SmdJoint_t *pRhs )
{
	// try to preserve joints without parents in their original order as much as possible
	if ( pLhs->m_nParentId < 0 )
		return pLhs->m_nId < pRhs->m_nId;

	return pLhs->m_nParentId < pRhs->m_nParentId;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void ParserCreateJoints(
	CDmSmdSerializer::SmdJointMap_t &smdJointMap,
	CDmeModel *pDmeModel,
	CDmeChannelsClip *pDmeChannelsClip,
	const char *pszFilename )
{
	// Sort the joints in order of parent, pointers only temporary and no elements
	// added/removed from map while pointers exist, so pointers are stable
	CUtlVector< CDmSmdSerializer::SmdJoint_t * > smdJointList;
	FOR_EACH_MAP_FAST( smdJointMap, nSmdJointMapIndex )
	{
		smdJointList.AddToTail( &smdJointMap.Element( nSmdJointMapIndex ) );
	}

	std::stable_sort( smdJointList.Base(), smdJointList.Base() + smdJointList.Count(), SmdJointLessFunc );

	for ( int i = 0; i < smdJointList.Count(); ++i )
	{
		CDmSmdSerializer::SmdJoint_t *pSmdJoint = smdJointList[i];
		pSmdJoint->m_nActualId = i;

		ParserCreateJoint( smdJointMap, pDmeModel, pDmeChannelsClip, pSmdJoint, pszFilename );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmSmdSerializer::ParserSetJoint(
	const SmdJointMap_t &smdJointMap,
	int nFrame,
	int nId,
	const Vector &vPosition,
	const RadianEuler &eRadianEulerXYZ,
	const char *pszFilename,
	int nLineNumber )
{
	const SmdJointMap_t::IndexType_t nIdIndex = smdJointMap.Find( nId );
	if ( !smdJointMap.IsValidIndex( nIdIndex ) )
	{
		Error( "%s(%d) : skeleton error : can't find joint %d\n",
			pszFilename, nLineNumber, nId );
		return;
	}

	CDmeDag *pDmeDag = smdJointMap.Element( nIdIndex ).m_pDmeDag;
	if ( !pDmeDag )
	{
		Error( "%s(%d) : skeleton error : no dmedag created for can't find joint %d (%s)\n",
			pszFilename, nLineNumber, nId, smdJointMap.Element( nIdIndex ).m_sName.Get() );
		return;
	}

	const Quaternion qOrientation = eRadianEulerXYZ;

	if ( pDmeDag->GetValue( "__rootJoint", false ) && GetUpAxis() != CDmSmdSerializer::Y_AXIS )
	{
		matrix3x4_t mPre;
		matrix3x4_t mPost;
		AngleMatrix( qOrientation, vPosition, mPre );
		ConcatTransforms( m_mAdj, mPre, mPost );
		pDmeDag->GetTransform()->SetTransform( mPost );
	}
	else
	{
		pDmeDag->GetTransform()->SetPosition( vPosition );
		pDmeDag->GetTransform()->SetOrientation( qOrientation );
	}

	if ( m_bOptAnimation )
	{
		const DmeTime_t tCurrent( static_cast< float >( nFrame ), DmeFramerate_t( m_flFrameRate ) );

		CDmeTransform *pDmeTransform = pDmeDag->GetTransform();

		CDmeVector3Log *pDmePosLog = pDmeDag->GetValueElement< CDmeVector3Log >( "__posLog" );
		pDmePosLog->SetKey( tCurrent, pDmeTransform->GetPosition() );

		CDmeQuaternionLog *pDmeRotLog = pDmeDag->GetValueElement< CDmeQuaternionLog >( "__rotLog" );
		pDmeRotLog->SetKey( tCurrent, pDmeTransform->GetOrientation() );
	}
}

//-----------------------------------------------------------------------------
// Returns -1 if invalid
//-----------------------------------------------------------------------------
static int GetActualId( const CDmSmdSerializer::SmdJointMap_t &smdJointMap, int nId )
{
	const CDmSmdSerializer::SmdJointMap_t::IndexType_t nIdIndex = smdJointMap.Find( nId );

	if ( !smdJointMap.IsValidIndex( nIdIndex ) )
	{
		Error( "Error! Invalid node id %d looked up\n", nId );
		return -1;
	}

	return smdJointMap.Element( nIdIndex ).m_nActualId;
}


//-----------------------------------------------------------------------------
// Used by HandleVertexWeights, sorts vertex weights by weight
//-----------------------------------------------------------------------------
static int VertexWeightLessFunc( const void *pLhs, const void *pRhs )
{
	const CVertexWeight *pVertexWeightL = reinterpret_cast< const CVertexWeight * >( pLhs );
	const CVertexWeight *pVertexWeightR = reinterpret_cast< const CVertexWeight * >( pRhs );

	if ( pVertexWeightL->m_nBoneIndex < 0 )
	{
		if ( pVertexWeightR->m_nBoneIndex < 0 )
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else if ( pVertexWeightR->m_nBoneIndex < 0 )
	{
		return -1;
	}

	if ( pVertexWeightL->m_flWeight > pVertexWeightR->m_flWeight )
	{
		return -1;
	}
	else if ( pVertexWeightL->m_flWeight < pVertexWeightR->m_flWeight )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void HandleVertexWeights(
	const CDmSmdSerializer::SmdJointMap_t &smdJointMap,
	int nFallbackBoneIndexId,
	CVertex &vertex,
	const char *pszLine,
	const char *pszFilename,
	int nLineNumber )
{
	for ( int i = 0; i < ARRAYSIZE( vertex.m_vertexWeights ); ++i )
	{
		vertex.m_vertexWeights[i].Reset();
	}

	CUtlVector< CVertexWeight > tmpVertexWeights;

	CUtlVector< CUtlString > tokens;
	Tokenize( tokens, pszLine );

	const float flEps = 1.0e-6;

	uint nTokenEnd = tokens.Count();
	if ( nTokenEnd > 10 )
	{
		int nId = -1;
		float flWeight = 0.0f;
		for ( uint i = 10; i < nTokenEnd; ++i )
		{
			nId = strtol( tokens[ i ].Get(), NULL, 0 );
			++i;
			flWeight = strtod( tokens[ i ].Get(), NULL );

			if ( nId < 0 || flWeight < flEps )
				continue;

			const int nActualId = GetActualId( smdJointMap, nId );

			if ( nActualId < 0 )
			{
				Error( "%s(%d) : triangle error : ignoring unknown joint id(%d) with actual id(%d) for vertex weight\n",
					pszFilename, nLineNumber, nId, nActualId );
				continue;
			}

			CVertexWeight &tmpVertexWeight = tmpVertexWeights[ tmpVertexWeights.AddToTail() ];
			tmpVertexWeight.m_nBoneIndex = nActualId;
			tmpVertexWeight.m_flWeight = flWeight;
		}

		// Sort vertex weights inversely by weight
		qsort( tmpVertexWeights.Base(), tmpVertexWeights.Count(), sizeof( CVertexWeight ), VertexWeightLessFunc );

		// Take at most the top ARRAYSIZE( vertex.m_vertexWeights ) from tmpVertexWeights
		// Figure out actual weight count and total weight

		float flTotalWeight = 0.0f;
		vertex.m_nWeights = 0;

		for ( int i = 0; i < tmpVertexWeights.Count() && i < ARRAYSIZE( vertex.m_vertexWeights ); ++i )
		{
			CVertexWeight &vertexWeight( vertex.m_vertexWeights[ i ] );
			vertexWeight = tmpVertexWeights[i];
			flTotalWeight += vertexWeight.m_flWeight;
			vertex.m_nWeights += 1;
		}

		// If there are valid user specified weights then renormalize
		if ( vertex.m_nWeights > 0 && flTotalWeight > flEps )
		{
			for ( uint i = 0; i != ARRAYSIZE( vertex.m_vertexWeights ); ++i )
			{
				CVertexWeight &vertexWeight( vertex.m_vertexWeights[ i ] );
				if ( vertexWeight.m_nBoneIndex >= 0 )
				{
					vertexWeight.m_flWeight /= flTotalWeight;
				}
			}
		}
		else
		{
			// No valid user weights, assign to fallback bone with weight of 1
			vertex.m_vertexWeights[ 0 ].m_nBoneIndex = GetActualId( smdJointMap, nFallbackBoneIndexId );
			if ( vertex.m_vertexWeights[0].m_nBoneIndex < 0 )
			{
				// Vertex is not weighted, can't find fallback bone, this is invalid
				vertex.m_nWeights = 0;
				vertex.m_vertexWeights[0].m_nBoneIndex = -1;
				vertex.m_vertexWeights[0].m_flWeight = 0.0f;
			}
			else
			{
				vertex.m_nWeights = 1;
				vertex.m_vertexWeights[0].m_flWeight = 1.0f;
			}

			// Assign rest of weights to -1, 0.0f
			for ( uint i = 1; i < ARRAYSIZE( vertex.m_vertexWeights ); ++i )
			{
				vertex.m_vertexWeights[i].m_nBoneIndex = -1;
				vertex.m_vertexWeights[i].m_flWeight = 0.0f;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeMesh *CreateDmeMesh(
	CDmeModel *pDmeModel,
	const char *pszFilename,
	int nLineNumber )
{
	char szFileBase[MAX_PATH];
	szFileBase[0] = '\0';

	const char *pszMeshName = "mesh";

	if ( pszFilename )
	{
		V_FileBase( pszFilename, szFileBase, ARRAYSIZE( szFileBase ) );

		if ( V_strlen( szFileBase ) > 0 )
		{
			pszMeshName = szFileBase;
		}
	}

	CDmeDag *pDmeDag = CreateElement< CDmeDag >( pszMeshName, pDmeModel->GetFileId() );
	if ( !pDmeDag )
		return NULL;

	pDmeModel->AddChild( pDmeDag );

	CUtlString sMeshName = pDmeDag->GetName();
	sMeshName += "Shape";

	CDmeMesh *pDmeMesh = CreateElement< CDmeMesh >( sMeshName.Get(), pDmeDag->GetFileId() );
	if ( !pDmeMesh )
		return NULL;

	CDmeVertexData *pDmeVertexData = pDmeMesh->FindOrCreateBaseState( "bind" );
	pDmeMesh->SetCurrentBaseState( "bind" );
	
	pDmeVertexData->FlipVCoordinate( true );
	pDmeVertexData->CreateField( CDmeVertexData::FIELD_POSITION );
	pDmeVertexData->CreateField( CDmeVertexData::FIELD_NORMAL );
	pDmeVertexData->CreateField( CDmeVertexData::FIELD_TEXCOORD );
	FieldIndex_t nJointWeightField;
	FieldIndex_t nJointIndexField;
	pDmeVertexData->CreateJointWeightsAndIndices( MAX_WEIGHTS_PER_VERTEX, &nJointWeightField, &nJointIndexField );

	pDmeDag->SetShape( pDmeMesh );

	pDmeVertexData->Resolve();
	pDmeMesh->Resolve();
	pDmeDag->Resolve();

	return pDmeMesh;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeFaceSet *FindOrCreateFaceSet(
	CDmeMesh *pDmeMesh,
	const CUtlString &sMaterial,
	const char *pszFilename,
	int nLineNumber )
{
	// Could cache in a hashed map or something...
	for ( int i = 0; i < pDmeMesh->FaceSetCount(); ++i )
	{
		CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( i );
		if ( !pDmeFaceSet )
			continue;

		CDmeMaterial *pDmeMaterial = pDmeFaceSet->GetMaterial();
		if ( !pDmeMaterial )
			continue;

		if ( !V_strcmp( pDmeMaterial->GetMaterialName(), sMaterial.Get() ) )
			return pDmeFaceSet;
	}

	char szFaceSetName[ MAX_PATH ];
	V_FileBase( sMaterial.Get(), szFaceSetName, ARRAYSIZE( szFaceSetName ) );

	CDmeFaceSet *pDmeFaceSet = CreateElement< CDmeFaceSet >( szFaceSetName, pDmeMesh->GetFileId() );
	Assert( pDmeFaceSet );

	CDmeMaterial *pDmeMaterial = CreateElement< CDmeMaterial >( szFaceSetName, pDmeMesh->GetFileId() );
	Assert( pDmeMaterial );

	pDmeMaterial->SetMaterial( sMaterial.Get() );
	pDmeFaceSet->SetMaterial( pDmeMaterial );

	pDmeMesh->AddFaceSet( pDmeFaceSet );

	return pDmeFaceSet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void CreatePolygon(
	CDmeMesh *pDmeMesh,
	const CUtlString &sMaterial,
	CTriangle &triangle,
	const char *pszFilename,
	int nLineNumber )
{
	if ( !triangle.Valid() || !pDmeMesh )
		return;

	CDmeVertexData *pDmeVertexData = pDmeMesh->GetBindBaseState();
	Assert( pDmeVertexData );

	FieldIndex_t nPositionField = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	CDmrArray< Vector > positionData = pDmeVertexData->GetVertexData( nPositionField );

	FieldIndex_t nNormalField = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	CDmrArray< Vector > normalData = pDmeVertexData->GetVertexData( nNormalField );

	FieldIndex_t nUvField = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	CDmrArray< Vector2D > uvData = pDmeVertexData->GetVertexData( nUvField );

	FieldIndex_t nJointWeightField = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_JOINT_WEIGHTS );
	CDmrArray< float > jointWeightData = pDmeVertexData->GetVertexData( nJointWeightField );

	FieldIndex_t nJointIndexField = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_JOINT_INDICES );
	CDmrArray< int > jointIndexData = pDmeVertexData->GetVertexData( nJointIndexField );

	CDmeFaceSet *pDmeFaceSet = FindOrCreateFaceSet( pDmeMesh, sMaterial, pszFilename, nLineNumber );
	if ( !pDmeFaceSet )
	{
		Error( "%s(%d) : mesh error : couldn't find or create DmeFace set for material \"%s\" on mesh \"%s\"\n",
			pszFilename, nLineNumber, sMaterial.Get(), pDmeMesh->GetName() );
		return;
	}

	for ( uint i = 0; i < 3; ++i )
	{
		const int nNewVertexIndex = pDmeVertexData->AddVertexIndices( 1 );

		CVertex &cVertex = triangle.m_vertices[i];

		{
			// TODO: Make two positions if they are skinned differently
			int nPositionIndex = positionData.Find( cVertex.m_vPosition );
			if ( !positionData.IsValidIndex( nPositionIndex ) )
			{
				nPositionIndex = positionData.AddToTail( cVertex.m_vPosition );

				for ( int j = 0; j < ARRAYSIZE( cVertex.m_vertexWeights ); ++j )
				{
					jointWeightData.AddToTail( cVertex.m_vertexWeights[j].m_flWeight );
					jointIndexData.AddToTail( cVertex.m_vertexWeights[j].m_nBoneIndex );
				}
			}

			pDmeVertexData->SetVertexIndices( nPositionField, nNewVertexIndex, 1, &nPositionIndex );

			const int nFaceSetIndex = pDmeFaceSet->AddIndices( 1 );
			pDmeFaceSet->SetIndices( nFaceSetIndex, 1, const_cast< int * >( &nNewVertexIndex ) );

		}

		{
			int nNormalIndex = normalData.Find( cVertex.m_vNormal );
			if ( !normalData.IsValidIndex( nNormalIndex ) )
			{
				nNormalIndex = normalData.AddToTail( cVertex.m_vNormal );
			}

			pDmeVertexData->SetVertexIndices( nNormalField, nNewVertexIndex, 1, &nNormalIndex );
		}


		{
			int nUvIndex = uvData.Find( cVertex.m_vUV );
			if ( !uvData.IsValidIndex( nUvIndex ) )
			{
				nUvIndex = uvData.AddToTail( cVertex.m_vUV );
			}

			pDmeVertexData->SetVertexIndices( nUvField, nNewVertexIndex, 1, &nUvIndex );
		}

	}

	static const int nFaceDelimiter = -1;
	const int nFaceSetIndex = pDmeFaceSet->AddIndices( 1 );
	pDmeFaceSet->SetIndices( nFaceSetIndex, 1, const_cast< int * >( &nFaceDelimiter ) );

	triangle.Reset();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeFaceSet *FindOrAddFaceSet(
	CDmeMesh *pDmeMesh,
	CUtlMap< CUtlString, CDmeFaceSet * > &faceListMap,
	const char *pszMaterialPath )
{
	CUtlMap< CUtlString, CDmeFaceSet * >::IndexType_t nIndex = faceListMap.Find( CUtlString( pszMaterialPath ) );

	if ( faceListMap.IsValidIndex( nIndex ) )
		return faceListMap.Element( nIndex );

	CDmeFaceSet *pDmeFaceSet = CreateElement< CDmeFaceSet >( pszMaterialPath, pDmeMesh->GetFileId() );
	CDmeMaterial *pDmeMaterial = CreateElement< CDmeMaterial >( pszMaterialPath, pDmeMesh->GetFileId() );
	Assert( pDmeFaceSet && pDmeMaterial );
	pDmeMaterial->SetMaterial( pszMaterialPath );
	pDmeMesh->AddFaceSet( pDmeFaceSet );

	return faceListMap.Element( faceListMap.Insert( CUtlString( pszMaterialPath ), pDmeFaceSet ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeChannelsClip *FindOrCreateChannelsClip( CDmElement *pDmeRoot, const char *pszAnimationName )
{
	CDmeAnimationList *pDmeAnimationList = pDmeRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	CDmeChannelsClip *pDmeChannelsClip = NULL;

	if ( pDmeAnimationList )
	{
		pDmeChannelsClip = pDmeAnimationList->GetAnimation( 0 );
	}
	else
	{
		CDmeModel *pDmeModel = pDmeRoot->GetValueElement< CDmeModel >( "skeleton" );

		pDmeAnimationList = CreateElement< CDmeAnimationList >( pszAnimationName, pDmeRoot->GetFileId() );
		pDmeChannelsClip = CreateElement< CDmeChannelsClip >( pszAnimationName, pDmeRoot->GetFileId() );
		pDmeAnimationList->AddAnimation( pDmeChannelsClip );
		pDmeRoot->SetValue( "animationList", pDmeAnimationList );
		pDmeModel->SetValue( "animationList", pDmeAnimationList );
	}

	return pDmeChannelsClip;
}

//-----------------------------------------------------------------------------
// Common function both ReadSMD & Unserialize can call
//-----------------------------------------------------------------------------
CDmElement *CDmSmdSerializer::ReadSMD(
	CUtlBuffer &utlBuf,
	DmFileId_t nDmFileId,
	const char *pszFilename,
	CDmeMesh **ppDmeMeshCreated )
{
	CDmElement *pDmeRoot = CreateElement< CDmElement >( "root", nDmFileId );
	CDmeModel *pDmeModel = CreateElement< CDmeModel >( "model", nDmFileId );

	char szAnimationName[ MAX_PATH ] = "";

	if ( pszFilename )
	{
		V_FileBase( pszFilename, szAnimationName, ARRAYSIZE( szAnimationName ) );
	}
	else
	{
		pszFilename = "unknown";
	}

	if ( V_strlen( szAnimationName ) > 0 )
	{
		pDmeModel->SetName( szAnimationName );
	}
	else
	{
		V_strcpy_safe( szAnimationName, "anim" );
	}

	pDmeRoot->SetValue( "skeleton", pDmeModel );

	if ( !m_bOptAnimation )
	{
		// Don't set root.model if animation only, studiomdl can't handle it
		pDmeRoot->SetValue( "model", pDmeModel );
	}

	SmdJointMap_t smdJointMap( CDefOps< int >::LessFunc );

	CUtlString sMaterial;

	CTriangle triangle;

	uint nLineNumber = 0;
	uint nBadPreambleCount = 0;
	ParserState_t nParserState = kPreamble;
	int nFrame = 0;

	CDmeChannelsClip *pDmeChannelsClip = NULL;

	if ( m_bOptAnimation )
	{
		pDmeChannelsClip = FindOrCreateChannelsClip( pDmeRoot, szAnimationName );
	}

	bool bStartTimeSet = false;

	CQcData qcData;

	CDmeMesh *pDmeMesh = NULL;

	char szLine[ 4096 ];

	while ( utlBuf.IsValid() )
	{
		GetLine( utlBuf, szLine, ARRAYSIZE( szLine ) );
		++nLineNumber;

		const char *pszLine = ParserSkipSpace( szLine );

		if ( ParserIsBlankLine( pszLine ) )
			continue;

		if ( nParserState == kPreamble )
		{
			if ( HandleQcHints( pszLine, qcData ) )
				continue;

			if ( ParserHandleVersion( pszLine ) )
			{
				if ( qcData.m_nUpAxis != m_nUpAxis )
				{
					Warning( "Importer UpAxis (%d) is different from UpAxis in SMD data (%d), using value found in SMD/QC\n", m_nUpAxis, qcData.m_nUpAxis );
				}

				SetUpAxis( qcData.m_nUpAxis );
				nParserState = kGeneral;
				continue;
			}

			Error( "%s(%d) : preamble error : expecting comment: \"%s\"\n",
				pszFilename, nLineNumber, szLine );

			++nBadPreambleCount;
			if ( nBadPreambleCount > 10 )
			{
				Error( "%s(%d) : preamble error : too many errors, not an SMD file, aborting\n",
					pszFilename, nLineNumber );
				break;
			}
		}
		else if ( nParserState == kGeneral )
		{
			if ( ParserHandleSectionStart( pszLine, "nodes" ) )
			{
				nParserState = kNodes;
			}
			else if ( ParserHandleSectionStart( pszLine, "skeleton" ) )
			{
				nParserState = kSkeleton;
			}
			else if ( ParserHandleSectionStart( pszLine, "triangles" ) )
			{
				nParserState = kTriangles;
			}
		}
		else if ( nParserState == kNodes )
		{
			if ( ParserHandleSectionStart( pszLine, "end" ) )
			{
				ParserCreateJoints( smdJointMap, pDmeModel, pDmeChannelsClip, pszFilename );

				nParserState = kGeneral;
				continue;
			}

			CUtlString sName;
			int nId;
			int nParentId;

			if ( !m_bOptImportSkeleton || !ParserHandleSkeletonLine( pszLine, sName, nId, nParentId ) )
				continue;

			if ( !ParserAddJoint( smdJointMap, nId, sName, nParentId, pszFilename, nLineNumber ) )
				continue;
		}
		else if ( nParserState == kSkeleton )
		{
			if ( ParserHandleSectionStart( pszLine, "end" ) )
			{
				nParserState = kGeneral;
				continue;
			}

			if ( m_bOptAnimation && ParserHandleTime( pszLine, nFrame ) )
			{
				if ( !bStartTimeSet )
				{
					pDmeChannelsClip->SetStartTime( DmeTime_t( static_cast< float >( nFrame ), DmeFramerate_t( m_flFrameRate ) ) );
					bStartTimeSet = true;
				}
				continue;
			}

			int nId;
			Vector vPosition;
			RadianEuler eRadianEulerXYZ;

			if ( sscanf( pszLine, "%d %f %f %f %f %f %f", &nId, &vPosition.x, &vPosition.y, &vPosition.z, &eRadianEulerXYZ.x, &eRadianEulerXYZ.y, &eRadianEulerXYZ.z ) == 7 )
			{
				if ( !m_bOptImportSkeleton )
					continue;

				ParserSetJoint( smdJointMap, nFrame, nId, vPosition, eRadianEulerXYZ, pszFilename, nLineNumber );
			}
		}
		else if ( nParserState == kTriangles )
		{
			if ( ParserHandleSectionStart( pszLine, "end" ) )
			{
				triangle.Reset();
				nParserState = kGeneral;
				continue;
			}

			int nFallbackSkinJoint;
			Vector vPosition;
			Vector vNormal;
			Vector2D vUV;
			char szShadingGroup[ ARRAYSIZE( szLine ) ];

			if ( sscanf( pszLine, "%d %f %f %f %f %f %f %f %f",
				&nFallbackSkinJoint,
				&vPosition.x, &vPosition.y, &vPosition.z,
				&vNormal.x, &vNormal.y, &vNormal.z, &vUV.x, &vUV.y ) == 9 )
			{
				if ( triangle.m_nVertices >= 3 )
				{
					Error( "%s(%d) : triangle error : Too many vertices in triangle\n",
						pszFilename, nLineNumber );
					continue;
				}

				CVertex &vertex( triangle.m_vertices[ triangle.m_nVertices++ ] );
				vertex.m_vUV = vUV;

				if ( GetUpAxis() != CDmSmdSerializer::Y_AXIS )
				{
					VectorTransform( vPosition, m_mAdj, vertex.m_vPosition );
					VectorTransform( vNormal, m_mAdjNormal, vertex.m_vNormal );
				}
				else
				{
					vertex.m_vPosition = vPosition;
					vertex.m_vNormal = vNormal;
				}

				if ( m_bOptImportSkeleton )
				{
					HandleVertexWeights( smdJointMap, nFallbackSkinJoint, vertex, pszLine, pszFilename, nLineNumber );
				}

				if ( !pDmeMesh )
				{
					pDmeMesh = CreateDmeMesh( pDmeModel, pszFilename, nLineNumber );
					if ( !pDmeMesh )
					{
						Error( "%s(%d) : Couldn't create DmeMesh\n",
							pszFilename, nLineNumber );
						break;
					}
				}

				CreatePolygon( pDmeMesh, sMaterial, triangle, pszFilename, nLineNumber );
			}
			else if ( sscanf( pszLine, "%1024s", szShadingGroup ) == 1 )
			{
				char pszMaterialPath[ ARRAYSIZE( szShadingGroup ) ];
				V_StripExtension( szShadingGroup, pszMaterialPath, ARRAYSIZE( pszMaterialPath ) );

				sMaterial = pszMaterialPath;

				triangle.Reset();
			}
			else
			{
				Error( "%s(%d) : triangle error : unexpected data in triangles\n",
					pszFilename, nLineNumber );
				triangle.Reset();
			}
		}
		else
		{
			Error( "%s(%d) : parser error : unknown parser state\n",
				pszFilename, nLineNumber );
			nParserState = kGeneral;
		}
	}

	pDmeModel->CaptureJointsToBaseState( "bind" );

	if ( pDmeChannelsClip )
	{
		// Reset skeleton values to the first sample
		FOR_EACH_MAP_FAST( smdJointMap, nJointMapIndex )
		{
			CDmeDag *pDmeDag = smdJointMap.Element( nJointMapIndex ).m_pDmeDag;
			if ( pDmeDag )
			{
				CDmeTransform *pDmeTransform = pDmeDag->GetTransform();

				CDmeVector3Log *pDmePosLog = pDmeDag->GetValueElement< CDmeVector3Log >( "__posLog" );
				pDmeTransform->SetPosition( pDmePosLog->GetKeyValue( 0 ) );

				CDmeQuaternionLog *pDmeRotLog = pDmeDag->GetValueElement< CDmeQuaternionLog >( "__rotLog" );
				pDmeTransform->SetOrientation( pDmeRotLog->GetKeyValue( 0 ) );
			}
		}

		pDmeChannelsClip->SetDuration( DmeTime_t( nFrame, DmeFramerate_t( m_flFrameRate ) ) - pDmeChannelsClip->GetStartTime() );
	}

	return pDmeRoot;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmSmdSerializer::FixNodeName( CUtlString &sName ) const
{
	char szTmpBuf0[ MAX_PATH ];
	char szTmpBuf1[ MAX_PATH ];

	V_strncpy( szTmpBuf0, sName.Get(), ARRAYSIZE( szTmpBuf0 ) );

	// Strip trailing quotes
	char *pszName = szTmpBuf0 + sName.Length() - 1;
	while ( pszName >= szTmpBuf0 && *pszName && *pszName == '"' )
	{
		*pszName = '\0';
		--pszName;
	}

	// Strip leading quotes
	pszName = szTmpBuf0;
	while ( *pszName && *pszName == '"')
	{
		++pszName;
	}

	if ( m_bOptAutoStripPrefix )
	{
		// Get the string before the '.'
		V_FileBase( pszName, szTmpBuf1, ARRAYSIZE( szTmpBuf1 ) );
		V_strncpy( szTmpBuf0, szTmpBuf1, ARRAYSIZE( szTmpBuf0 ) );
		pszName = szTmpBuf0;
	}

	if ( !m_sNodeDelPrefix.IsEmpty() && StringHasPrefix( pszName, m_sNodeDelPrefix.Get() ) )
	{
		pszName = const_cast< char * >( StringAfterPrefix( pszName, m_sNodeDelPrefix.Get() ) );
	}

	if ( m_sNodeAddPrefix.IsEmpty() )
	{
		sName = pszName;
	}
	else
	{
		sName = m_sNodeAddPrefix;
		sName += pszName;
	}
}