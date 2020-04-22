//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ---------------------------------------------------------------------------------------- //
// This is a datatable test case. 
// It is run in debug mode when the engine starts to catch any bugs in datatable code.
// This can also serve as a simple example of how datatables work separately from the
// intricacies of the entity system.
// ---------------------------------------------------------------------------------------- //
// This is also a good place to test new code since it's run right when the engine
// starts up. It can also be put into standalone apps easily.
// ---------------------------------------------------------------------------------------- //
// Things it tests:
// - Data transmission integrity.
// - Delta calculation.
// - Strings, floats, vectors, recursive datatables, and ints.
// - Fixed-length arrays.
// - Variable-length arrays.
// - Exclude props.
// - Recursive datatables.
// - Datatable proxies returning false.
// - CUtlVectors of regular types (like floats) and data tables.
// ---------------------------------------------------------------------------------------- //
// Things it does not test:
// - Quantization.
// - Clamping.
// - The entity system's usage of data tables.
// - Stress testing - too many properties, maxing out delta bits.
// - Built-in and custom client and server proxies.
// ---------------------------------------------------------------------------------------- //
// At a high level, the test is setup as such:
// - Server structure and datatable.
// - Client structure and datatable. 
// - A function table with a function to modify and compare each element.
// - A function that initializes the server structure and tries random changes to it and 
//   verifies that the client receives the deltas and changes correctly.
// ---------------------------------------------------------------------------------------- //
// Eventually it would be nice to stress-test the entities with tests for:
// - Misordered proxy callbacks and missing data.
// ---------------------------------------------------------------------------------------- //
#include "quakedef.h"
#include "dt.h"
#include "dt_send.h"
#include "dt_recv.h"
#include "tier0/dbg.h"
#include "dt_utlvector_send.h"
#include "dt_utlvector_recv.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// If datatables support these again, then uncomment this to have them tested.
//#define SUPPORT_ARRAYS_OF_DATATABLES


#ifdef _DEBUG



class DTTestSub2Sub
{
public:
	int		m_Int2;
};

class DTTestSub2
{
public:
	int				m_Int;
	DTTestSub2Sub	m_Sub;
};


class CTestStruct
{
public:
	int a,b;
	float f;
};


#define MAX_STRUCTARRAY_ELEMENTS 11
#define MAX_FLOATARRAY_ELEMENTS	18
#define MAX_CHARARRAY_ELEMENTS	22


// ------------------------------------------------------------------------------------------- //
// DTTestServerSub and its DataTable.
// ------------------------------------------------------------------------------------------- //
class DTTestServerSub
{
public:
	float				m_FloatArray[3];
	char				m_Strings[2][64];
	
	CUtlVector<CTestStruct> m_UtlVectorStruct;
	CUtlVector<float> m_UtlVectorFloat;
	CUtlVector<char> m_UtlVectorChar;
};

void SendProxy_DTTestServerSubString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	SendProxy_StringToString( pProp, pStruct, pData, pOut, iElement, objectID);

}

BEGIN_SEND_TABLE_NOBASE( CTestStruct, DT_TestStruct )
	SendPropInt( SENDINFO_NOCHECK( a ) ),
	SendPropInt( SENDINFO_NOCHECK( b ) ),
	SendPropFloat( SENDINFO_NOCHECK( f ) )
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE(DTTestServerSub, DT_DTTestSub)
	// - Auto type conversions (receiving an array of floats into an array of ints).
	SendPropArray(
		SendPropFloat(SENDINFO_NOCHECK(m_FloatArray[0]), 0, SPROP_NOSCALE),
		m_FloatArray),

	SendPropUtlVectorDataTable( m_UtlVectorStruct, MAX_STRUCTARRAY_ELEMENTS, DT_TestStruct ),

	SendPropArray(
		SendPropString(SENDINFO_NOCHECK(m_Strings[0]), 0, SendProxy_DTTestServerSubString),
		m_Strings ),

	SendPropUtlVector(
		SENDINFO_UTLVECTOR( m_UtlVectorChar ),
		MAX_CHARARRAY_ELEMENTS,
		SendPropInt( NULL, 0, sizeof( char ), 0 ) ),

	SendPropUtlVector( 
		SENDINFO_UTLVECTOR( m_UtlVectorFloat ),
		MAX_FLOATARRAY_ELEMENTS,	// max elements
		SendPropFloat( NULL, 0, 0, 0, SPROP_NOSCALE ) )
END_SEND_TABLE()



BEGIN_SEND_TABLE_NOBASE(DTTestSub2Sub, DT_DTTestSub2Sub)
	SendPropInt( SENDINFO_NOCHECK( m_Int2 ), 32 ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE(DTTestSub2, DT_DTTestSub2)
	SendPropDataTable(SENDINFO_DT(m_Sub), &REFERENCE_SEND_TABLE(DT_DTTestSub2Sub)),
	SendPropInt( SENDINFO_NOCHECK( m_Int ), 32 ),
END_SEND_TABLE()



// ------------------------------------------------------------------------------------------- //
// DTTestServer and its DataTable.
// ------------------------------------------------------------------------------------------- //
class DTTestServer
{
public:
	DTTestServerSub		m_Sub;
	DTTestSub2			m_Sub2;

	float			m_Float;

#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	DTTestServerSub	m_SubArray[2];
#endif	
	
	Vector			m_Vector;
	char			m_String[64];
	int				m_Int;
	int				m_IntArray[32];		// Note that the server and client array length are different.
	char			m_CharArray[8];

	int				m_VLALength;
	int				m_VLA[16];
};

void SendProxy_DTTestServerFloat( const SendProp *pProp, void *pStruct, void *pData, DVariant *pOut, int iElement, int objectID )
{
	SendProxy_FloatToFloat(pProp, pStruct, pData, pOut, iElement, objectID);
}

void SendProxy_DTTestServerVector( const SendProp *pProp, void *pStruct, void *pData, DVariant *pOut, int iElement, int objectID )
{
	SendProxy_VectorToVector(pProp, pStruct, pData, pOut, iElement, objectID);
}

void SendProxy_DTTestServerString( const SendProp *pProp, void *pStruct, void *pData, DVariant *pOut, int iElement, int objectID )
{
	SendProxy_StringToString(pProp, pStruct, pData, pOut, iElement, objectID);
}

void SendProxy_DTTestServerInt( const SendProp *pProp, void *pStruct, void *pData, DVariant *pOut, int iElement, int objectID )
{
	SendProxy_Int32ToInt32(pProp, pStruct, pData, pOut, iElement, objectID);
}

bool g_bSendSub = true;
void* SendProxy_DTTestServerSub( const SendProp *pProp, const void *pStruct, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
{
	if( !g_bSendSub )
		return NULL;
	
	return SendProxy_DataTableToDataTable( pProp, pStruct, pData, pRecipients, objectID );
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_DTTestServerSub );


int ArrayLengthSendProxy_VLALength( const void *pStruct, int objectID )
{
	DTTestServer *pServer = (DTTestServer*)pStruct;
	return pServer->m_VLALength;
}


BEGIN_SEND_TABLE_NOBASE(DTTestServer, DT_DTTest)
	SendPropVariableLengthArray(
		ArrayLengthSendProxy_VLALength,
		SendPropInt( SENDINFO_NOCHECK( m_VLA[0] ) ),
		m_VLA ),

	// Test exclude props.
	SendPropExclude( "DT_DTTest", "m_Int" ),
	SendPropDataTable(SENDINFO_DT(m_Sub), &REFERENCE_SEND_TABLE(DT_DTTestSub), SendProxy_DTTestServerSub),


	SendPropFloat (SENDINFO_NOCHECK(m_Float), 	32, SPROP_NOSCALE),

	SendPropDataTable(SENDINFO_DT(m_Sub2), &REFERENCE_SEND_TABLE(DT_DTTestSub2)),

	SendPropInt   (SENDINFO_NOCHECK(m_Int),		23, SPROP_UNSIGNED),
	
	SendPropExclude( "DT_DTTestSub", "m_FloatArray" ),
	

	SendPropString(SENDINFO_NOCHECK(m_String)),

	SendPropArray(
		SendPropInt(SENDINFO_NOCHECK(m_CharArray[0]),      8),
		m_CharArray),

	SendPropArray(
		SendPropInt (SENDINFO_NOCHECK(m_IntArray[0]),		23, SPROP_UNSIGNED),
		m_IntArray),

#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	SendPropArray(
		SendPropDataTable(SENDINFO_DT(m_SubArray[0]), &REFERENCE_SEND_TABLE(DT_DTTestSub), SendProxy_DTTestServerSub),
		m_SubArray ),
#endif

	SendPropVector(SENDINFO_NOCHECK(m_Vector), 	32, SPROP_NOSCALE)
END_SEND_TABLE()



// ------------------------------------------------------------------------------------------- //
// DTTestClientSub and its DataTable.
// ------------------------------------------------------------------------------------------- //
class DTTestClientSub
{
public:
	char	m_Strings[2][64];
	float	m_FloatArray[3];

	CUtlVector<CTestStruct> m_UtlVectorStruct;
	CUtlVector<float> m_UtlVectorFloat;
	CUtlVector<char> m_UtlVectorChar;
};

void RecvProxy_DTTestClientSubString( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_StringToString( pData, pStruct, pOut );
}

BEGIN_RECV_TABLE_NOBASE( CTestStruct, DT_TestStruct )
	RecvPropInt( RECVINFO( a ) ),
	RecvPropInt( RECVINFO( b ) ),
	RecvPropFloat( RECVINFO( f ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE(DTTestClientSub, DT_DTTestSub)
	// - Auto type conversions (receiving an array of floats into an array of ints).
	RecvPropArray(
		RecvPropFloat(RECVINFO(m_FloatArray[0])),
		m_FloatArray),

	RecvPropUtlVector( RECVINFO_UTLVECTOR( m_UtlVectorFloat ), MAX_FLOATARRAY_ELEMENTS, RecvPropFloat(NULL,0,0) ),
	RecvPropUtlVectorDataTable( m_UtlVectorStruct, MAX_STRUCTARRAY_ELEMENTS, DT_TestStruct ),
	
	RecvPropUtlVector( 
		RECVINFO_UTLVECTOR( m_UtlVectorChar ),
		MAX_CHARARRAY_ELEMENTS,
		RecvPropInt( NULL, 0, sizeof( char ) ) ),

	RecvPropArray(
		RecvPropString(RECVINFO(m_Strings[0]), 0, RecvProxy_DTTestClientSubString),
		m_Strings),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE(DTTestSub2Sub, DT_DTTestSub2Sub)
	RecvPropInt( RECVINFO( m_Int2 ), 32 ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE(DTTestSub2, DT_DTTestSub2)
	RecvPropDataTable(RECVINFO_DT(m_Sub), 0, &REFERENCE_RECV_TABLE(DT_DTTestSub2Sub)),
	RecvPropInt( RECVINFO( m_Int ) ),
END_RECV_TABLE()




// ------------------------------------------------------------------------------------------- //
// DTTestClient and DataTable.
// ------------------------------------------------------------------------------------------- //
class DTTestClient
{
public:
	DTTestClientSub		m_Sub;
	long				m_Guard1;
	
	DTTestSub2			m_Sub2;
	long				m_Guard2;

#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	DTTestClientSub		m_SubArray[2];
#endif

	long				m_Guard3;

	float				m_Float;
	long				m_Guard4;

	Vector				m_Vector;
	long				m_Guard5;

	char				m_String[64];
	long				m_Guard6;

	int					m_Int;
	long				m_Guard7;

	int					m_IntArray[32];		// Note that the server and client array length are different.
	long				m_Guard8;

	char				m_CharArray[8];
	long				m_Guard9;

	int					m_VLALength;
	int					m_VLA[16];
};


void RecvProxyArrayLength_VLA( void *pStruct, int objectID, int currentArrayLength )
{
	DTTestClient *pClient = (DTTestClient*)pStruct;
	pClient->m_VLALength = currentArrayLength;
}


BEGIN_RECV_TABLE_NOBASE(DTTestClient, DT_DTTest)

#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	RecvPropArray(
		RecvPropDataTable(RECVINFO_DT(m_SubArray[0]), 0, &REFERENCE_RECV_TABLE(DT_DTTestSub), RecvProxy_DTTestClientSub),
		m_SubArray ),
#endif

	RecvPropFloat (RECVINFO(m_Float), 0),

	RecvPropDataTable(RECVINFO_DT(m_Sub), 0, &REFERENCE_RECV_TABLE(DT_DTTestSub)),
	RecvPropDataTable(RECVINFO_DT(m_Sub2), 0, &REFERENCE_RECV_TABLE(DT_DTTestSub2)),

	// -	Arrays with and without the SPROP_ONEBITDELTA flag.
	RecvPropArray(
		RecvPropInt (RECVINFO(m_CharArray[0]),      8),
		m_CharArray),

	RecvPropVector(RECVINFO(m_Vector), 0),
	RecvPropString(RECVINFO_STRING(m_String), 0),
	
	RecvPropInt (RECVINFO(m_Int),	0),

	// -	Arrays with and without the SPROP_ONEBITDELTA flag.
	// -	Array size mismatches between the client and the server.
	RecvPropArray(
		RecvPropInt (RECVINFO(m_IntArray[0]), 0),
		m_IntArray),

	RecvPropInt( RECVINFO( m_VLALength ) ),

	RecvPropVariableLengthArray(
		RecvProxyArrayLength_VLA,
		RecvPropInt( RECVINFO( m_VLA[0] ) ),
		m_VLA )
END_RECV_TABLE()




// ------------------------------------------------------------------------------------------- //
// Functions that act on the data.
// ------------------------------------------------------------------------------------------- //

typedef bool (*CompareElementFn)(DTTestClient *pClient, DTTestServer *pServer);
typedef void (*RandomlyChangeElementFn)(DTTestServer *pServer);

float FRand(double minVal, double maxVal)
{
	return (float)(((double)rand() / VALVE_RAND_MAX) * (maxVal - minVal) + minVal);
}

void RandomlyChangeStringGeneric(char *str, int size)
{
	for(int i=0; i < size-1; i++)
		str[i] = (char)rand();

	str[size-1] = 0;
}

bool CompareTestSubString0(DTTestClient *pClient, DTTestServer *pServer)
{
	return strcmp(pClient->m_Sub.m_Strings[0], pServer->m_Sub.m_Strings[0]) == 0;
}
void RandomlyChangeSubString0(DTTestServer *pServer)
{
	if( g_bSendSub )
		RandomlyChangeStringGeneric(pServer->m_Sub.m_Strings[0], sizeof(pServer->m_Sub.m_Strings[0]));
}

bool CompareTestSubString1(DTTestClient *pClient, DTTestServer *pServer)
{
	return strcmp(pClient->m_Sub.m_Strings[1], pServer->m_Sub.m_Strings[1]) == 0;
}
void RandomlyChangeSubString1(DTTestServer *pServer)
{
	if( g_bSendSub )
		RandomlyChangeStringGeneric(pServer->m_Sub.m_Strings[1], sizeof(pServer->m_Sub.m_Strings[1]));
}

bool CompareFloat(DTTestClient *pClient, DTTestServer *pServer)
{
	return pClient->m_Float == pServer->m_Float;
}
void RandomlyChangeFloat(DTTestServer *pServer)
{
	pServer->m_Float = FRand(-500000, 500000);
}

bool CompareVector(DTTestClient *pClient, DTTestServer *pServer)
{
	return pClient->m_Vector.x == pServer->m_Vector.x && 
		pClient->m_Vector.y == pServer->m_Vector.y && 
		pClient->m_Vector.z == pServer->m_Vector.z;
}
void RandomlyChangeVector(DTTestServer *pServer)
{
	pServer->m_Vector.x = FRand(-500000, 500000);
	pServer->m_Vector.y = FRand(-500000, 500000);
	pServer->m_Vector.z = FRand(-500000, 500000);
}

bool CompareString(DTTestClient *pClient, DTTestServer *pServer)
{
	return strcmp(pClient->m_String, pServer->m_String) == 0;
}
void RandomlyChangeString(DTTestServer *pServer)
{
	//memset( pServer->m_String, , sizeof( pServer->m_String ) );
	 Q_strncpy( pServer->m_String, "a", sizeof( pServer->m_String ) );

	//RandomlyChangeStringGeneric(pServer->m_String, sizeof(pServer->m_String));
}

bool CompareInt(DTTestClient *pClient, DTTestServer *pServer)
{
// (m_Int is the exclude prop we're testing)
//	return pClient->m_Int == pServer->m_Int;
return true;
}
void RandomlyChangeInt(DTTestServer *pServer)
{
	pServer->m_Int = (int)rand();
}

bool CompareIntArray(DTTestClient *pClient, DTTestServer *pServer)
{
	// Just verify however much of the data we can.
	int leastElements = (sizeof(pClient->m_IntArray) < sizeof(pServer->m_IntArray)) ? (sizeof(pClient->m_IntArray)/sizeof(pClient->m_IntArray[0])) : (sizeof(pServer->m_IntArray)/sizeof(pServer->m_IntArray[0]));
	return memcmp(pClient->m_IntArray, pServer->m_IntArray, leastElements*sizeof(int)) == 0;
}
void RandomlyChangeIntArray(DTTestServer *pServer)
{
	// Change a random subset of the array.
	int nElements = sizeof(pServer->m_IntArray) / sizeof(pServer->m_IntArray[0]);
	int nChanges = 4 + rand() % nElements;
	
	for(int i=0; i < nChanges; i++)
	{
		pServer->m_IntArray[rand() % nElements] = (int)rand();
	}
}

bool CompareFloatArray(DTTestClient *pClient, DTTestServer *pServer)
{
// m_FloatArray is an ExcludeProp.
/*
	int leastElements = (sizeof(pClient->m_Sub.m_FloatArray) < sizeof(pServer->m_Sub.m_FloatArray)) ? (sizeof(pClient->m_Sub.m_FloatArray)/sizeof(pClient->m_Sub.m_FloatArray[0])) : (sizeof(pServer->m_Sub.m_FloatArray)/sizeof(pServer->m_Sub.m_FloatArray[0]));
	for(int i=0; i < leastElements; i++)
	{
		if(pClient->m_Sub.m_FloatArray[i] != pServer->m_Sub.m_FloatArray[i])
			return false;
	}
*/	
	return true;
}
void RandomlyChangeFloatArray(DTTestServer *pServer)
{
	// Change a random subset of the array.
	int nElements = sizeof(pServer->m_Sub.m_FloatArray) / sizeof(pServer->m_Sub.m_FloatArray[0]);
	int nChanges = 4 + rand() % nElements;
	
	for(int i=0; i < nChanges; i++)
	{
		pServer->m_Sub.m_FloatArray[rand() % nElements] = (float)rand() * 0.943123f;
	}
}

bool CompareCharArray(DTTestClient *pClient, DTTestServer *pServer)
{
	return memcmp(pClient->m_CharArray, pServer->m_CharArray, sizeof(pClient->m_CharArray)) == 0;
}
void RandomlyChangeCharArray(DTTestServer *pServer)
{
	for(int i=0; i < (sizeof(pServer->m_CharArray) / sizeof(pServer->m_CharArray[0])); i++)
		pServer->m_CharArray[i] = (char)rand();
}


bool CompareSubArray(DTTestClient *pClient, DTTestServer *pServer )
{
#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	for( int i=0; i < 2; i++ )
	{
		for( int z=0; z < sizeof(pServer->m_SubArray[0].m_FloatArray) / sizeof(pServer->m_SubArray[0].m_FloatArray[0]); z++ )
		{
			if( pServer->m_SubArray[i].m_FloatArray[z] != pClient->m_SubArray[i].m_FloatArray[z] )
				return false;
		}
		
		for( int iString=0; iString < sizeof(pServer->m_SubArray[0].m_Strings) / sizeof(pServer->m_SubArray[0].m_Strings[0]); iString++ )
		{
			for( z=0; z < sizeof(pServer->m_SubArray[0].m_Strings[0]) / sizeof(pServer->m_SubArray[0].m_Strings[0][0]); z++ )
			{
				if( pServer->m_SubArray[i].m_Strings[iString][z] != pClient->m_SubArray[i].m_Strings[iString][z] )
					return false;
				
				// Check for null termination.
				if( pServer->m_SubArray[i].m_Strings[iString][z] == 0 )
					break;
			}
		}
	}
#endif

	return true;
}

void RandomlyChangeSubArray(DTTestServer *pServer)
{
#if defined( SUPPORT_ARRAYS_OF_DATATABLES )
	if( !g_bSendSub )
		return;

	for( int i=0; i < 2; i++ )
	{
		int index = rand() & 1;
		
		for( int z=0; z < sizeof(pServer->m_SubArray[0].m_FloatArray) / sizeof(pServer->m_SubArray[0].m_FloatArray[0]); z++ )
			pServer->m_SubArray[index].m_FloatArray[z] = rand() * 0.932f;
		
		for( int iString=0; iString < sizeof(pServer->m_SubArray[0].m_Strings) / sizeof(pServer->m_SubArray[0].m_Strings[0]); iString++ )
		{
			int stringLen = sizeof(pServer->m_SubArray[0].m_Strings[0]) / sizeof(pServer->m_SubArray[0].m_Strings[0][0]);
			for( z=0; z < stringLen; z++ )
				pServer->m_SubArray[index].m_Strings[iString][z] = (char)rand();

			// null-terminate it
			pServer->m_SubArray[index].m_Strings[iString][stringLen-1] = 0;
		}
	}
#endif
}

bool CompareSub2( DTTestClient *pClient, DTTestServer *pServer )
{
	return memcmp( &pClient->m_Sub2, &pServer->m_Sub2, sizeof( pClient->m_Sub2 ) ) == 0;
}

void RandomlyChangeSub2( DTTestServer *pServer )
{
	pServer->m_Sub2.m_Int = rand();
}

bool CompareSub2Sub( DTTestClient *pClient, DTTestServer *pServer )
{
	return pClient->m_Sub2.m_Sub.m_Int2 == pServer->m_Sub2.m_Sub.m_Int2;
}

void RandomlyChangeSub2Sub( DTTestServer *pServer )
{
	pServer->m_Sub2.m_Sub.m_Int2 = rand();
}


bool CompareVLA( DTTestClient *pClient, DTTestServer *pServer )
{
	if ( pClient->m_VLALength != pServer->m_VLALength )
		return false;

	for ( int i=0; i < pClient->m_VLALength; i++ )
	{
		if ( pClient->m_VLA[i] != pServer->m_VLA[i] )
			return false;
	}

	return true;
}

void RandomlyChangeVLA( DTTestServer *pServer )
{
	pServer->m_VLALength = rand() % ARRAYSIZE( pServer->m_VLA );
	for ( int i=0; i < pServer->m_VLALength; i++ )
		pServer->m_VLA[i] = rand() * rand();
}


bool CompareUtlVectorStruct( DTTestClient *pClient, DTTestServer *pServer )
{
	CUtlVector<CTestStruct> &c = pClient->m_Sub.m_UtlVectorStruct;
	CUtlVector<CTestStruct> &s = pServer->m_Sub.m_UtlVectorStruct;

	if ( c.Count() != s.Count() )
		return false;

	for ( int i=0; i < c.Count(); i++ )
	{
		if ( c[i].a != s[i].a || c[i].b != s[i].b || c[i].f != s[i].f )
			return false;
	}
	return true;
}


void RandomlyChangeUtlVectorStruct( DTTestServer *pServer )
{
	if ( !g_bSendSub )
		return;

	int nElements = rand() % MAX_STRUCTARRAY_ELEMENTS;
	pServer->m_Sub.m_UtlVectorStruct.SetSize( nElements );
	for ( int i=0; i < nElements; i++ )
	{
		pServer->m_Sub.m_UtlVectorStruct[i].a = rand();
		pServer->m_Sub.m_UtlVectorStruct[i].b = rand();
		pServer->m_Sub.m_UtlVectorStruct[i].f = rand();
	}
}

bool CompareUtlVectorFloat( DTTestClient *pClient, DTTestServer *pServer )
{
	CUtlVector<float> &c = pClient->m_Sub.m_UtlVectorFloat;
	CUtlVector<float> &s = pServer->m_Sub.m_UtlVectorFloat;

	if ( c.Count() != s.Count() )
		return false;

	for ( int i=0; i < c.Count(); i++ )
	{
		if ( c[i] != s[i] )
			return false;
	}
	return true;
}

void RandomlyChangeUtlVectorChar( DTTestServer *pServer )
{
	if ( !g_bSendSub )
		return;

	int nElements = rand() % MAX_CHARARRAY_ELEMENTS;
	pServer->m_Sub.m_UtlVectorChar.SetSize( nElements );
	for ( int i=0; i < nElements; i++ )
		pServer->m_Sub.m_UtlVectorChar[i] = (char)rand();
}


bool CompareUtlVectorChar( DTTestClient *pClient, DTTestServer *pServer )
{
	CUtlVector<char> &c = pClient->m_Sub.m_UtlVectorChar;
	CUtlVector<char> &s = pServer->m_Sub.m_UtlVectorChar;

	if ( c.Count() != s.Count() )
		return false;

	for ( int i=0; i < c.Count(); i++ )
	{
		if ( c[i] != s[i] )
			return false;
	}
	return true;
}

void RandomlyChangeUtlVectorFloat( DTTestServer *pServer )
{
	if ( !g_bSendSub )
		return;

	int nElements = rand() % MAX_FLOATARRAY_ELEMENTS;
	pServer->m_Sub.m_UtlVectorFloat.SetSize( nElements );
	for ( int i=0; i < nElements; i++ )
		pServer->m_Sub.m_UtlVectorFloat[i] = rand() / 0.93;
}


typedef struct
{
	CompareElementFn		m_CompareFn;
	RandomlyChangeElementFn	m_ChangeFn;
} VarTestInfo;

VarTestInfo g_VarTestInfos[] =
{
	{CompareVLA,			RandomlyChangeVLA},
	{CompareUtlVectorStruct,RandomlyChangeUtlVectorStruct},
	{CompareUtlVectorFloat, RandomlyChangeUtlVectorFloat},
	{CompareUtlVectorChar,	RandomlyChangeUtlVectorChar},
	{CompareFloat,			RandomlyChangeFloat},
	{CompareSub2,			RandomlyChangeSub2},
	{CompareSub2Sub,		RandomlyChangeSub2Sub},
	{CompareInt,			RandomlyChangeInt},
	{CompareFloatArray,		RandomlyChangeFloatArray},
	{CompareTestSubString0,	RandomlyChangeSubString0},
	{CompareTestSubString1,	RandomlyChangeSubString1},
	{CompareCharArray,		RandomlyChangeCharArray},
	{CompareVector,			RandomlyChangeVector},
	{CompareString,			RandomlyChangeString},
	{CompareIntArray,		RandomlyChangeIntArray},
	{CompareSubArray,		RandomlyChangeSubArray}
};
#define NUMVARTESTINFOS	(sizeof(g_VarTestInfos) / sizeof(g_VarTestInfos[0]))


int g_GuardOffsets[] = 
{
	offsetof( DTTestClient, m_Guard1 ),
	offsetof( DTTestClient, m_Guard2 ),
	offsetof( DTTestClient, m_Guard3 ),
	offsetof( DTTestClient, m_Guard4 ),
	offsetof( DTTestClient, m_Guard5 ),
	offsetof( DTTestClient, m_Guard6 ),
	offsetof( DTTestClient, m_Guard7 ),
	offsetof( DTTestClient, m_Guard8 ),
	offsetof( DTTestClient, m_Guard9 )
};
int g_nGuardOffsets = sizeof( g_GuardOffsets ) / sizeof( g_GuardOffsets[0] );


void SetGuardBytes( DTTestClient *pClient )
{
	for( int i=0; i < g_nGuardOffsets; i++ )
	{
		unsigned char *pDest = ((unsigned char *)pClient) + g_GuardOffsets[i];
		*((long*)pDest) = i;
	}
}


void CheckGuardBytes( DTTestClient *pClient )
{
	for( int i=0; i < g_nGuardOffsets; i++ )
	{
		unsigned char *pDest = ((unsigned char *)pClient) + g_GuardOffsets[i];
		Assert( *((long*)pDest) == i );
	}
}


// ------------------------------------------------------------------------------------------- //
// TEST CODE
// ------------------------------------------------------------------------------------------- //

bool CompareDTTest(DTTestClient *pClient, DTTestServer *pServer)
{
	for(int iVar=0; iVar < NUMVARTESTINFOS; iVar++)
	{
		if(!g_VarTestInfos[iVar].m_CompareFn(pClient, pServer))
		{
			Assert( !"CompareDTTest: comparison failed. There is a new datatable bug." );
			return false;
		}
	}
	return true;
}

bool WriteSendTable_R( SendTable *pTable, bf_write &bfWrite, bool bNeedsDecoder )
{
	if( pTable->GetWriteFlag() )
		return true;

	pTable->SetWriteFlag( true );

	// Send the version with the exclude props.
	bfWrite.WriteOneBit( 1 );

	bfWrite.WriteOneBit( bNeedsDecoder?1:0 );
	
	if( !SendTable_WriteInfos( pTable, &bfWrite ) )
		return false;

	for( int i=0; i < pTable->m_nProps; i++ )
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
			if( !WriteSendTable_R( pProp->GetDataTable(), bfWrite, false ) )
				return false;
	}

	return true;
}


void RunDataTableTest()
{
	RecvTable *pRecvTable = &REFERENCE_RECV_TABLE(DT_DTTest);
	SendTable *pSendTable = &REFERENCE_SEND_TABLE(DT_DTTest);


	ALIGN4 unsigned char buf[4096] ALIGN4_POST;
	bf_write x = bf_write(buf, 4096);
	bf_read y = bf_read(buf, 4096);
	x.WriteUBitLong(1, 1);
	x.WriteUBitLong(3, 2);
	x.WriteUBitLong(7, 3);
	x.WriteUBitLong(0x31415926, 32);
	Verify( y.ReadOneBit() == 1 );
	Verify( y.ReadUBitLong(5) == 7*4+3 );
	Verify( y.ReadUBitLong(32) == 0x31415926 );


	// Initialize the send and receive modules.
	SendTable_Init( &pSendTable, 1 );
	RecvTable_Init( &pRecvTable, 1 );

	pSendTable->SetWriteFlag( false );
	
	// Send DataTable info to the client.
	ALIGN4 unsigned char commBuf[8192] ALIGN4_POST;
	bf_write bfWrite( "RunDataTableTest->commBuf", commBuf, sizeof(commBuf) );
	if( !WriteSendTable_R( pSendTable, bfWrite, true ) )
	{
		Assert( !"RunDataTableTest: SendTable_SendInfo failed." );
	}	
	bfWrite.WriteOneBit(0);


	// Receive the SendTable's info.
	bf_read bfRead( "RunDataTableTest->bfRead", commBuf, sizeof(commBuf));
	while( bfRead.ReadOneBit() )
	{
		bool bNeedsDecoder = bfRead.ReadOneBit()!=0;

		if( !RecvTable_RecvClassInfos( &bfRead, bNeedsDecoder ) )
		{
			Assert( !"RunDataTableTest: RecvTable_ReadInfos failed." );
			continue;
		}
	}

	// Register our receive table.
	if( !RecvTable_CreateDecoders( NULL, false ) )
	{
		Assert(false);
	}


	// Setup the data with all zeros.
	DTTestServer dtServer;
	DTTestClient dtClient;

	ALIGN4 unsigned char prevEncoded[4096] ALIGN4_POST;
	ALIGN4 unsigned char fullEncoded[4096] ALIGN4_POST;

	memset(&dtServer, 0, sizeof(dtServer));
	memset(&dtClient, 0, sizeof(dtClient));
	memset(prevEncoded, 0, sizeof(prevEncoded));

	SetGuardBytes( &dtClient );

	// Now loop around, changing the data a little bit each time and send/recv deltas.
	int nIterations = 25;
	for( int iIteration=0; iIteration < nIterations; iIteration++ )
	{
		// Change the server's data.
		g_bSendSub = true;
		if( (iIteration % 5) == 0 )
		{
			g_bSendSub = false; // every 8th time, don't send the subtable
		}
		
		if( (iIteration & 3) == 0 )
		{
			// Every once in a while, change ALL the properties.
			for( int iChange=0; iChange < NUMVARTESTINFOS; iChange++ )
				g_VarTestInfos[iChange].m_ChangeFn( &dtServer );
		}
		else
		{
			int nChanges = 3 + rand() % NUMVARTESTINFOS;
			for( int iChange=0; iChange < nChanges; iChange++ )
			{
				int iInfo = rand() % NUMVARTESTINFOS;
				g_VarTestInfos[iInfo].m_ChangeFn( &dtServer );
			}
		}

		// Fully encode it.
		bf_write bfFullEncoded( "RunDataTableTest->bfFullEncoded", fullEncoded, sizeof(fullEncoded) );
		if( !SendTable_Encode( pSendTable, &dtServer, &bfFullEncoded, -1, NULL ) )
		{
			Assert(false);
		}


		ALIGN4 unsigned char deltaEncoded[4096] ALIGN4_POST;
		bf_write bfDeltaEncoded( "RunDataTableTest->bfDeltaEncoded", deltaEncoded, sizeof(deltaEncoded) );
		
		if ( iIteration == 0 )
		{
			// On the first iteration, just write the whole state.
			if( !SendTable_Encode( pSendTable, &dtServer, &bfDeltaEncoded, -1, NULL ) )
			{
				Assert( false );
			}
		}
		else
		{
			// Figure out the delta between the newly encoded one and the previously encoded one.
			ALIGN4 int deltaProps[MAX_DATATABLE_PROPS] ALIGN4_POST;

			bf_read fullEncodedRead( "RunDataTableTest->fullEncodedRead", fullEncoded, sizeof( fullEncoded ), bfFullEncoded.GetNumBitsWritten() );
			bf_read prevEncodedRead( "RunDataTableTest->prevEncodedRead", prevEncoded, sizeof( prevEncoded ) );

			int nDeltaProps = SendTable_CalcDelta( 
				pSendTable, 
				prevEncoded, sizeof( prevEncoded ) * 8, 
				fullEncoded, bfFullEncoded.GetNumBitsWritten(),
				deltaProps,
				ARRAYSIZE( deltaProps ),
				-1 );
			
			Assert( nDeltaProps != -1 ); // BAD: buffer overflow

			
			// Reencode with just the delta. This is what is actually sent to the client.
			SendTable_WritePropList( 
				pSendTable,
				fullEncoded,
				bfFullEncoded.GetNumBitsWritten(),
				&bfDeltaEncoded,
				-1111, 
				deltaProps,
				nDeltaProps );
		}

		memcpy( prevEncoded, fullEncoded, sizeof( prevEncoded ) );


		// This step isn't necessary to have the client decode the data but it's here to test
		// RecvTable_CopyEncoding (and RecvTable_MergeDeltas). This call should just make an exact
		// copy of the encoded data.
		ALIGN4 unsigned char copyEncoded[4096] ALIGN4_POST;
		bf_read bfReadDeltaEncoded( "RunDataTableTest->bfReadDeltaEncoded", deltaEncoded, sizeof( deltaEncoded ) );
		bf_write bfCopyEncoded( "RunDataTableTest->bfCopyEncoded", copyEncoded, sizeof(copyEncoded) );

		RecvTable_CopyEncoding( pRecvTable, &bfReadDeltaEncoded, &bfCopyEncoded, -1 );
		
		// Decode..
		bf_read bfDecode( "RunDataTableTest->copyEncoded", copyEncoded, sizeof( copyEncoded ) );
		if(!RecvTable_Decode(pRecvTable, &dtClient, &bfDecode, 1111))
		{
			Assert(false);
		}

		
		// Make sure it didn't go into memory it shouldn't have.
		CheckGuardBytes( &dtClient );


		// Verify that only the changed properties were sent and that they were received correctly.
		CompareDTTest( &dtClient, &dtServer );
	}

	SendTable_Term();
	RecvTable_Term();
}


#endif
