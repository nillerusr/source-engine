//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxattribute.h"
#include "tier1/utlbufferutil.h"
#include "tier1/uniqueid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
CUtlSymbolTableMT CDmxAttribute::s_AttributeNameSymbols;


//-----------------------------------------------------------------------------
// Attribute size
//-----------------------------------------------------------------------------
static size_t s_pAttributeSize[AT_TYPE_COUNT] = 
{
	0,							// AT_UNKNOWN,
	sizeof(CDmxElement*),		// AT_ELEMENT,
	sizeof(int),				// AT_INT,
	sizeof(float),				// AT_FLOAT,
	sizeof(bool),				// AT_BOOL,
	sizeof(CUtlString),			// AT_STRING,
	sizeof(CUtlBinaryBlock),	// AT_VOID,
	sizeof(DmObjectId_t),		// AT_OBJECTID,
	sizeof(Color),				// AT_COLOR,
	sizeof(Vector2D),			// AT_VECTOR2,
	sizeof(Vector),				// AT_VECTOR3,
	sizeof(Vector4D),			// AT_VECTOR4,
	sizeof(QAngle),				// AT_QANGLE,
	sizeof(Quaternion),			// AT_QUATERNION,
	sizeof(VMatrix),			// AT_VMATRIX,
	sizeof(CUtlVector<CDmxElement*>),		// AT_ELEMENT_ARRAY,
	sizeof(CUtlVector<int>),				// AT_INT_ARRAY,
	sizeof(CUtlVector<float>),				// AT_FLOAT_ARRAY,
	sizeof(CUtlVector<bool>),				// AT_BOOL_ARRAY,
	sizeof(CUtlVector<CUtlString>),			// AT_STRING_ARRAY,
	sizeof(CUtlVector<CUtlBinaryBlock>),	// AT_VOID_ARRAY,
	sizeof(CUtlVector<DmObjectId_t>),		// AT_OBJECTID_ARRAY,
	sizeof(CUtlVector<Color>),				// AT_COLOR_ARRAY,
	sizeof(CUtlVector<Vector2D>),			// AT_VECTOR2_ARRAY,
	sizeof(CUtlVector<Vector>),				// AT_VECTOR3_ARRAY,
	sizeof(CUtlVector<Vector4D>),			// AT_VECTOR4_ARRAY,
	sizeof(CUtlVector<QAngle>),				// AT_QANGLE_ARRAY,
	sizeof(CUtlVector<Quaternion>),			// AT_QUATERNION_ARRAY,
	sizeof(CUtlVector<VMatrix>),			// AT_VMATRIX_ARRAY,
};


//-----------------------------------------------------------------------------
// make sure that the attribute data type sizes are what we think they are to choose the right allocator
//-----------------------------------------------------------------------------
struct CSizeTest
{
	CSizeTest()
	{
		// test internal value attribute sizes
		COMPILE_TIME_ASSERT( sizeof( int )			== 4 );
		COMPILE_TIME_ASSERT( sizeof( float )		== 4 );
		COMPILE_TIME_ASSERT( sizeof( bool )			<= 4 );
		COMPILE_TIME_ASSERT( sizeof( Color )		== 4 );
		COMPILE_TIME_ASSERT( sizeof( Vector2D )		== 8 );
		COMPILE_TIME_ASSERT( sizeof( Vector )		== 12 );
		COMPILE_TIME_ASSERT( sizeof( Vector4D )		== 16 );
		COMPILE_TIME_ASSERT( sizeof( QAngle )		== 12 );
		COMPILE_TIME_ASSERT( sizeof( Quaternion )	== 16 );
		COMPILE_TIME_ASSERT( sizeof( VMatrix )		== 64 );
		COMPILE_TIME_ASSERT( sizeof( CUtlString )	== 4 );
		COMPILE_TIME_ASSERT( sizeof( CUtlBinaryBlock ) == 16 );
		COMPILE_TIME_ASSERT( sizeof( DmObjectId_t )	== 16 );
	};
};
static CSizeTest g_sizeTest;


//-----------------------------------------------------------------------------
// Returns attribute name for type
//-----------------------------------------------------------------------------
const char *g_pAttributeTypeName[AT_TYPE_COUNT] = 
{
	"unknown", // AT_UNKNOWN
	CDmAttributeInfo<DmElementHandle_t>::AttributeTypeName(), // AT_ELEMENT,
	CDmAttributeInfo<int>::AttributeTypeName(), // AT_INT,
	CDmAttributeInfo<float>::AttributeTypeName(), // AT_FLOAT,
	CDmAttributeInfo<bool>::AttributeTypeName(), // AT_BOOL,
	CDmAttributeInfo<CUtlString>::AttributeTypeName(), // AT_STRING,
	CDmAttributeInfo<CUtlBinaryBlock>::AttributeTypeName(), // AT_VOID,
	CDmAttributeInfo<DmObjectId_t>::AttributeTypeName(), // AT_OBJECTID,
	CDmAttributeInfo<Color>::AttributeTypeName(), // AT_COLOR,
	CDmAttributeInfo<Vector2D>::AttributeTypeName(), // AT_VECTOR2,
	CDmAttributeInfo<Vector>::AttributeTypeName(), // AT_VECTOR3,
	CDmAttributeInfo<Vector4D>::AttributeTypeName(), // AT_VECTOR4,
	CDmAttributeInfo<QAngle>::AttributeTypeName(), // AT_QANGLE,
	CDmAttributeInfo<Quaternion>::AttributeTypeName(), // AT_QUATERNION,
	CDmAttributeInfo<VMatrix>::AttributeTypeName(), // AT_VMATRIX,
	CDmAttributeInfo< CUtlVector< DmElementHandle_t > >::AttributeTypeName(), // AT_ELEMENT_ARRAY,
	CDmAttributeInfo< CUtlVector< int > >::AttributeTypeName(), // AT_INT_ARRAY,
	CDmAttributeInfo< CUtlVector< float > >::AttributeTypeName(), // AT_FLOAT_ARRAY,
	CDmAttributeInfo< CUtlVector< bool > >::AttributeTypeName(), // AT_BOOL_ARRAY,
	CDmAttributeInfo< CUtlVector< CUtlString > >::AttributeTypeName(), // AT_STRING_ARRAY,
	CDmAttributeInfo< CUtlVector< CUtlBinaryBlock > >::AttributeTypeName(), // AT_VOID_ARRAY,
	CDmAttributeInfo< CUtlVector< DmObjectId_t > >::AttributeTypeName(), // AT_OBJECTID_ARRAY,
	CDmAttributeInfo< CUtlVector< Color > >::AttributeTypeName(), // AT_COLOR_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector2D > >::AttributeTypeName(), // AT_VECTOR2_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector > >::AttributeTypeName(), // AT_VECTOR3_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector4D > >::AttributeTypeName(), // AT_VECTOR4_ARRAY,
	CDmAttributeInfo< CUtlVector< QAngle > >::AttributeTypeName(), // AT_QANGLE_ARRAY,
	CDmAttributeInfo< CUtlVector< Quaternion > >::AttributeTypeName(), // AT_QUATERNION_ARRAY,
	CDmAttributeInfo< CUtlVector< VMatrix > >::AttributeTypeName(), // AT_VMATRIX_ARRAY,
};


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDmxAttribute::CDmxAttribute( const char *pAttributeName )
{
	m_Name = s_AttributeNameSymbols.AddString( pAttributeName );
	m_Type = AT_UNKNOWN;
	m_pData = NULL;
}

CDmxAttribute::CDmxAttribute( CUtlSymbol attributeName )
{
	m_Name = attributeName;
	m_Type = AT_UNKNOWN;
	m_pData = NULL;
}

CDmxAttribute::~CDmxAttribute()
{
	FreeDataMemory();
}


//-----------------------------------------------------------------------------
// Returns the size of the variables storing the various attribute types
//-----------------------------------------------------------------------------
int CDmxAttribute::AttributeDataSize( DmAttributeType_t type )
{
	return s_pAttributeSize[type];
}


//-----------------------------------------------------------------------------
// Allocate, free memory for data
//-----------------------------------------------------------------------------
void CDmxAttribute::AllocateDataMemory( DmAttributeType_t type )
{
	FreeDataMemory();

	m_Type = type;
	m_pData = DMXAlloc( s_pAttributeSize[m_Type] );
}

#define DESTRUCT_ARRAY( _dataType )									\
case CDmAttributeInfo< CUtlVector< _dataType > >::ATTRIBUTE_TYPE:	\
	Destruct( ( CUtlVector< _dataType >* )m_pData );				\
	break;

void CDmxAttribute::FreeDataMemory()
{
	if ( m_Type != AT_UNKNOWN )
	{
		Assert( m_pData != NULL );
		if ( !IsArrayType( m_Type ) )
		{
			if ( m_Type == AT_STRING )
			{
				Destruct( (CUtlString*)m_pData );
			}
			else if ( m_Type == AT_VOID	)
			{
				Destruct( (CUtlBinaryBlock*)m_pData );
			}
		}
		else
		{
			switch ( m_Type )
			{
				DESTRUCT_ARRAY( int );
				DESTRUCT_ARRAY( float );
				DESTRUCT_ARRAY( bool );
				DESTRUCT_ARRAY( CUtlString );
				DESTRUCT_ARRAY( CUtlBinaryBlock );
				DESTRUCT_ARRAY( DmObjectId_t );
				DESTRUCT_ARRAY( Color );
				DESTRUCT_ARRAY( Vector2D );
				DESTRUCT_ARRAY( Vector );
				DESTRUCT_ARRAY( Vector4D );
				DESTRUCT_ARRAY( QAngle );
				DESTRUCT_ARRAY( Quaternion );
				DESTRUCT_ARRAY( VMatrix );
				DESTRUCT_ARRAY( CDmxElement* );
			}
		}
		m_Type = AT_UNKNOWN;
	}
}


//-----------------------------------------------------------------------------
// Returns attribute type string
//-----------------------------------------------------------------------------
inline const char* CDmxAttribute::GetTypeString() const
{
	return g_pAttributeTypeName[ m_Type ];
}


//-----------------------------------------------------------------------------
// Returns attribute name
//-----------------------------------------------------------------------------
const char *CDmxAttribute::GetName() const
{
	return s_AttributeNameSymbols.String( m_Name );
}


//-----------------------------------------------------------------------------
// Gets the size of an array, returns 0 if it's not an array type
//-----------------------------------------------------------------------------
#define GET_ARRAY_COUNT( _dataType )			\
	case CDmAttributeInfo< CUtlVector< _dataType > >::ATTRIBUTE_TYPE:		\
	{															\
		const CUtlVector< _dataType > &array = *( CUtlVector< _dataType >* )m_pData; \
		return array.Count();									\
	}

int CDmxAttribute::GetArrayCount() const
{
	if ( !IsArrayType( m_Type ) || !m_pData )
		return 0;

	switch( m_Type )
	{
		GET_ARRAY_COUNT( int );
		GET_ARRAY_COUNT( float );
		GET_ARRAY_COUNT( bool );
		GET_ARRAY_COUNT( CUtlString );
		GET_ARRAY_COUNT( CUtlBinaryBlock );
		GET_ARRAY_COUNT( DmObjectId_t );
		GET_ARRAY_COUNT( Color );
		GET_ARRAY_COUNT( Vector2D );
		GET_ARRAY_COUNT( Vector );
		GET_ARRAY_COUNT( Vector4D );
		GET_ARRAY_COUNT( QAngle );
		GET_ARRAY_COUNT( Quaternion );
		GET_ARRAY_COUNT( VMatrix );
		GET_ARRAY_COUNT( CDmxElement* );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Gets the size of an array, returns 0 if it's not an array type
//-----------------------------------------------------------------------------
#define SERIALIZES_ON_MULTIPLE_LINES( _dataType )	\
	case CDmAttributeInfo< _dataType >::ATTRIBUTE_TYPE: \
		return ::SerializesOnMultipleLines< _dataType >();  \
	case CDmAttributeInfo< CUtlVector< _dataType > >::ATTRIBUTE_TYPE: \
		return ::SerializesOnMultipleLines< CUtlVector< _dataType > >()

bool CDmxAttribute::SerializesOnMultipleLines() const
{
	switch( m_Type )
	{
		SERIALIZES_ON_MULTIPLE_LINES( CDmxElement* );
		SERIALIZES_ON_MULTIPLE_LINES( int );
		SERIALIZES_ON_MULTIPLE_LINES( float );
		SERIALIZES_ON_MULTIPLE_LINES( bool );
		SERIALIZES_ON_MULTIPLE_LINES( CUtlString );
		SERIALIZES_ON_MULTIPLE_LINES( CUtlBinaryBlock );
		SERIALIZES_ON_MULTIPLE_LINES( DmObjectId_t );
		SERIALIZES_ON_MULTIPLE_LINES( Color );
		SERIALIZES_ON_MULTIPLE_LINES( Vector2D );
		SERIALIZES_ON_MULTIPLE_LINES( Vector );
		SERIALIZES_ON_MULTIPLE_LINES( Vector4D );
		SERIALIZES_ON_MULTIPLE_LINES( QAngle );
		SERIALIZES_ON_MULTIPLE_LINES( Quaternion );
		SERIALIZES_ON_MULTIPLE_LINES( VMatrix );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Write to file
//-----------------------------------------------------------------------------
#define SERIALIZE_TYPE( _buf, _attributeType, _dataType )		\
	case _attributeType:										\
	{															\
		if ( m_pData )											\
			return ::Serialize( _buf, *(_dataType*)m_pData );	\
		_dataType temp;											\
		CDmAttributeInfo< _dataType >::SetDefaultValue( temp ); \
		return ::Serialize( _buf, temp );						\
	}

#define SERIALIZE_ARRAY_TYPE( _buf, _attributeType, _dataType )	\
	case _attributeType:										\
	{															\
		if ( m_pData )											\
			return ::Serialize( _buf, *(CUtlVector< _dataType >*)m_pData ); \
		CUtlVector< _dataType > temp;							\
		CDmAttributeInfo< CUtlVector< _dataType > >::SetDefaultValue( temp ); \
		return ::Serialize( _buf, temp );						\
	}

bool CDmxAttribute::Serialize( CUtlBuffer &buf ) const
{
	switch( m_Type )
	{
		SERIALIZE_TYPE( buf, AT_INT, int );
		SERIALIZE_TYPE( buf, AT_FLOAT, float );
		SERIALIZE_TYPE( buf, AT_BOOL, bool );
		SERIALIZE_TYPE( buf, AT_STRING, CUtlString );
		SERIALIZE_TYPE( buf, AT_VOID, CUtlBinaryBlock );
		SERIALIZE_TYPE( buf, AT_OBJECTID, DmObjectId_t );
		SERIALIZE_TYPE( buf, AT_COLOR, Color );
		SERIALIZE_TYPE( buf, AT_VECTOR2, Vector2D );
		SERIALIZE_TYPE( buf, AT_VECTOR3, Vector );
		SERIALIZE_TYPE( buf, AT_VECTOR4, Vector4D );
		SERIALIZE_TYPE( buf, AT_QANGLE, QAngle );
		SERIALIZE_TYPE( buf, AT_QUATERNION, Quaternion );
		SERIALIZE_TYPE( buf, AT_VMATRIX, VMatrix );
		SERIALIZE_ARRAY_TYPE( buf, AT_INT_ARRAY, int );
		SERIALIZE_ARRAY_TYPE( buf, AT_FLOAT_ARRAY, float );
		SERIALIZE_ARRAY_TYPE( buf, AT_BOOL_ARRAY, bool );
		SERIALIZE_ARRAY_TYPE( buf, AT_STRING_ARRAY, CUtlString );
		SERIALIZE_ARRAY_TYPE( buf, AT_VOID_ARRAY, CUtlBinaryBlock );
		SERIALIZE_ARRAY_TYPE( buf, AT_OBJECTID_ARRAY, DmObjectId_t );
		SERIALIZE_ARRAY_TYPE( buf, AT_COLOR_ARRAY, Color );
		SERIALIZE_ARRAY_TYPE( buf, AT_VECTOR2_ARRAY, Vector2D );
		SERIALIZE_ARRAY_TYPE( buf, AT_VECTOR3_ARRAY, Vector );
		SERIALIZE_ARRAY_TYPE( buf, AT_VECTOR4_ARRAY, Vector4D );
		SERIALIZE_ARRAY_TYPE( buf, AT_QANGLE_ARRAY, QAngle );
		SERIALIZE_ARRAY_TYPE( buf, AT_QUATERNION_ARRAY, Quaternion );
		SERIALIZE_ARRAY_TYPE( buf, AT_VMATRIX_ARRAY, VMatrix );
	default:
		AssertMsg( 0, "Cannot serialize elements or element arrays!\n" );
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Serialize a single element in an array attribute
//-----------------------------------------------------------------------------
#define SERIALIZE_TYPED_ELEMENT( _index, _buf, _attributeType, _dataType )			\
	case _attributeType:															\
	{																				\
		if ( m_pData )																\
		{																			\
			const CUtlVector< _dataType > &array = *( CUtlVector< _dataType >* )m_pData;\
			return ::Serialize( buf, array[_index] );								\
		}																			\
		_dataType temp;																\
		CDmAttributeInfo< _dataType >::SetDefaultValue( temp );						\
		return ::Serialize( _buf, temp );											\
	}

bool CDmxAttribute::SerializeElement( int nIndex, CUtlBuffer &buf ) const
{
	if ( !IsArrayType( m_Type ) )
		return false;

	switch( m_Type )
	{
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_INT_ARRAY, int );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_FLOAT_ARRAY, float );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_BOOL_ARRAY, bool );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_STRING_ARRAY, CUtlString );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_VOID_ARRAY, CUtlBinaryBlock );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_OBJECTID_ARRAY, DmObjectId_t );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_COLOR_ARRAY, Color );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_VECTOR2_ARRAY, Vector2D );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_VECTOR3_ARRAY, Vector );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_VECTOR4_ARRAY, Vector4D );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_QANGLE_ARRAY, QAngle );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_QUATERNION_ARRAY, Quaternion );
		SERIALIZE_TYPED_ELEMENT( nIndex, buf, AT_VMATRIX_ARRAY, VMatrix );

	default:
		AssertMsg( 0, "Cannot serialize elements!\n" );
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Read from file
//-----------------------------------------------------------------------------
#define UNSERIALIZE_TYPE( _buf, _attributeType, _dataType )	\
	case _attributeType:									\
		Construct( (_dataType*)m_pData );					\
		return ::Unserialize( _buf, *(_dataType*)m_pData )

#define UNSERIALIZE_ARRAY_TYPE( _buf, _attributeType, _dataType )		\
	case _attributeType:												\
		Construct( (CUtlVector< _dataType > *)m_pData );				\
		return ::Unserialize( _buf, *(CUtlVector< _dataType >*)m_pData )

bool CDmxAttribute::Unserialize( DmAttributeType_t type, CUtlBuffer &buf )
{
	AllocateDataMemory( type );
	switch( type )
	{
	UNSERIALIZE_TYPE( buf, AT_INT, int );
	UNSERIALIZE_TYPE( buf, AT_FLOAT, float );
	UNSERIALIZE_TYPE( buf, AT_BOOL, bool );
	UNSERIALIZE_TYPE( buf, AT_STRING, CUtlString );
	UNSERIALIZE_TYPE( buf, AT_VOID, CUtlBinaryBlock );
	UNSERIALIZE_TYPE( buf, AT_OBJECTID, DmObjectId_t );
	UNSERIALIZE_TYPE( buf, AT_COLOR, Color );
	UNSERIALIZE_TYPE( buf, AT_VECTOR2, Vector2D );
	UNSERIALIZE_TYPE( buf, AT_VECTOR3, Vector );
	UNSERIALIZE_TYPE( buf, AT_VECTOR4, Vector4D );
	UNSERIALIZE_TYPE( buf, AT_QANGLE, QAngle );
	UNSERIALIZE_TYPE( buf, AT_QUATERNION, Quaternion );
	UNSERIALIZE_TYPE( buf, AT_VMATRIX, VMatrix );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_INT_ARRAY, int );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_FLOAT_ARRAY, float );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_BOOL_ARRAY, bool );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_STRING_ARRAY, CUtlString );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_VOID_ARRAY, CUtlBinaryBlock );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_OBJECTID_ARRAY, DmObjectId_t );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_COLOR_ARRAY, Color );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_VECTOR2_ARRAY, Vector2D );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_VECTOR3_ARRAY, Vector );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_VECTOR4_ARRAY, Vector4D );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_QANGLE_ARRAY, QAngle );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_QUATERNION_ARRAY, Quaternion );
	UNSERIALIZE_ARRAY_TYPE( buf, AT_VMATRIX_ARRAY, VMatrix );
	default:
		AssertMsg( 0, "Cannot unserialize elements or element arrays!\n" );
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Read element from file
//-----------------------------------------------------------------------------
#define UNSERIALIZE_TYPED_ELEMENT( _buf, _attributeType, _dataType )	\
	case _attributeType:										\
	{															\
		if ( !bIsDataMemoryAllocated )							\
		{														\
			Construct( (CUtlVector< _dataType > *)m_pData );	\
		}														\
		_dataType temp;											\
		bool bReadElement = ::Unserialize( _buf, temp );		\
		if ( bReadElement )										\
		{														\
			GetArrayForEdit< _dataType >().AddToTail( temp );	\
		}														\
		return bReadElement;									\
	}															

bool CDmxAttribute::UnserializeElement( DmAttributeType_t type, CUtlBuffer &buf )
{
	if ( !IsArrayType( type ) )
		return false;

	bool bIsDataMemoryAllocated = ( m_Type == type );
	if ( !bIsDataMemoryAllocated )
	{
		AllocateDataMemory( type );
	}
	switch( type )
	{
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_INT_ARRAY, int );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_FLOAT_ARRAY, float );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_BOOL_ARRAY, bool );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_STRING_ARRAY, CUtlString );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_VOID_ARRAY, CUtlBinaryBlock );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_OBJECTID_ARRAY, DmObjectId_t );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_COLOR_ARRAY, Color );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_VECTOR2_ARRAY, Vector2D );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_VECTOR3_ARRAY, Vector );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_VECTOR4_ARRAY, Vector4D );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_QANGLE_ARRAY, QAngle );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_QUATERNION_ARRAY, Quaternion );
	UNSERIALIZE_TYPED_ELEMENT( buf, AT_VMATRIX_ARRAY, VMatrix );
	default:
		AssertMsg( 0, "Cannot unserialize elements!\n" );
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Sets name
//-----------------------------------------------------------------------------
void CDmxAttribute::SetName( const char *pAttributeName )
{
	m_Name = s_AttributeNameSymbols.Find( pAttributeName );
}


//-----------------------------------------------------------------------------
// Sets values
//-----------------------------------------------------------------------------
void CDmxAttribute::SetValue( const char *pString )
{
	AllocateDataMemory( AT_STRING );
	CUtlString* pUtlString = (CUtlString*)m_pData;
	Construct( pUtlString );
	pUtlString->Set( pString );
}

void CDmxAttribute::SetValue( const void *pBuffer, size_t nLen )
{
	AllocateDataMemory( AT_VOID );
	CUtlBinaryBlock* pBlob = (CUtlBinaryBlock*)m_pData;
	Construct( pBlob );
	pBlob->Set( pBuffer, nLen );
}

// Untyped method for setting used by unpack
void CDmxAttribute::SetValue( DmAttributeType_t type, const void *pSrc, int nLen )
{
	if ( m_Type != type )
	{
		AllocateDataMemory( type );
	}
	Assert( nLen == CDmxAttribute::AttributeDataSize( type ) ); 
	if ( nLen > CDmxAttribute::AttributeDataSize( type ) )
	{
		nLen = CDmxAttribute::AttributeDataSize( type );
	}
	memcpy( m_pData, pSrc, nLen );
}


void CDmxAttribute::SetValue( const CDmxAttribute *pAttribute )
{
	DmAttributeType_t type = pAttribute->GetType();
	if ( !IsArrayType( type ) )
	{
		SetValue( type, pAttribute->m_pData, CDmxAttribute::AttributeDataSize( type ) );
	}
	else
	{
		// Copying array attributes not done yet..
		Assert( 0 );
	}
}

// Sets the attribute to its default value based on its type
#define SET_DEFAULT_VALUE( _dataType )	\
	case CDmAttributeInfo< _dataType >::ATTRIBUTE_TYPE:			\
		CDmAttributeInfo< _dataType >::SetDefaultValue( *reinterpret_cast< _dataType* >( m_pData ) );	\
		break;																	\
	case CDmAttributeInfo< CUtlVector< _dataType > >::ATTRIBUTE_TYPE:			\
		CDmAttributeInfo< CUtlVector< _dataType > >::SetDefaultValue( *reinterpret_cast< CUtlVector< _dataType > * >( m_pData ) );	\
		break

void CDmxAttribute::SetToDefaultValue()
{
	switch( GetType() )
	{
	SET_DEFAULT_VALUE( int );
	SET_DEFAULT_VALUE( float );
	SET_DEFAULT_VALUE( bool );
	SET_DEFAULT_VALUE( CUtlString );
	SET_DEFAULT_VALUE( CUtlBinaryBlock );
	SET_DEFAULT_VALUE( DmObjectId_t );
	SET_DEFAULT_VALUE( Color );
	SET_DEFAULT_VALUE( Vector2D );
	SET_DEFAULT_VALUE( Vector );
	SET_DEFAULT_VALUE( Vector4D );
	SET_DEFAULT_VALUE( QAngle );
	SET_DEFAULT_VALUE( Quaternion );
	SET_DEFAULT_VALUE( VMatrix );
	SET_DEFAULT_VALUE( CDmxElement* );

	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Convert to and from string
//-----------------------------------------------------------------------------
void CDmxAttribute::SetValueFromString( const char *pValue )
{
	switch ( GetType() )
	{
	case AT_UNKNOWN:
		break;

	case AT_STRING:
		SetValue( pValue );
		break;

	default:
		{
			int nLen = pValue ? Q_strlen( pValue ) : 0;
			if ( nLen == 0 )
			{
				SetToDefaultValue();
				break;
			}

			CUtlBuffer buf( pValue, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			if ( !Unserialize( GetType(), buf ) )
			{
				SetToDefaultValue();
			}
		}
		break;
	}
}

const char *CDmxAttribute::GetValueAsString( char *pBuffer, size_t nBufLen ) const
{
	Assert( pBuffer );
	CUtlBuffer buf( pBuffer, nBufLen, CUtlBuffer::TEXT_BUFFER );
	Serialize( buf );
	return pBuffer;
}
