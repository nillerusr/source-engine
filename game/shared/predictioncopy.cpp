//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#if !defined( NO_ENTITY_PREDICTION )

#include "IGameSystem.h"
#include <typeinfo.h>
#include "cdll_int.h"
#include <memory.h>
#include <stdarg.h>
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "predictioncopy.h"
#include "engine/ivmodelinfo.h"
#include "tier1/fmtstr.h"
#include "utlvector.h"
#include "tier0/vprof.h"
#include "tier1/tokenset.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CClassMemoryPool< optimized_datamap_t >	g_OptimizedDataMapPool( 20, CUtlMemoryPool::GROW_SLOW );

// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static const char *g_FieldTypes[ FIELD_TYPECOUNT ] = 
{
	"FIELD_VOID",			// FIELD_VOID
	"FIELD_FLOAT",			// FIELD_FLOAT
	"FIELD_STRING",			// FIELD_STRING
	"FIELD_VECTOR",			// FIELD_VECTOR
	"FIELD_QUATERNION",		// FIELD_QUATERNION
	"FIELD_INTEGER",		// FIELD_INTEGER
	"FIELD_BOOLEAN",		// FIELD_BOOLEAN
	"FIELD_SHORT",			// FIELD_SHORT
	"FIELD_CHARACTER",		// FIELD_CHARACTER
	"FIELD_COLOR32",		// FIELD_COLOR32
	"FIELD_EMBEDDED",		// FIELD_EMBEDDED	(handled specially)
	"FIELD_CUSTOM",			// FIELD_CUSTOM		(handled specially)
	"FIELD_CLASSPTR",		// FIELD_CLASSPTR
	"FIELD_EHANDLE",		// FIELD_EHANDLE
	"FIELD_EDICT",			// FIELD_EDICT
	"FIELD_POSITION_VECTOR",// FIELD_POSITION_VECTOR
	"FIELD_TIME",			// FIELD_TIME
	"FIELD_TICK",			// FIELD_TICK
	"FIELD_MODELNAME",		// FIELD_MODELNAME
	"FIELD_SOUNDNAME",		// FIELD_SOUNDNAME
	"FIELD_INPUT",			// FIELD_INPUT		(uses custom type)
	"FIELD_FUNCTION",		// FIELD_FUNCTION
	"FIELD_VMATRIX",			
	"FIELD_VMATRIX_WORLDSPACE",
	"FIELD_MATRIX3X4_WORLDSPACE",
	"FIELD_INTERVAL"		// FIELD_INTERVAL
	"FIELD_MODELINDEX"		// FIELD_MODELINDEX
};

static int g_FieldSizes[FIELD_TYPECOUNT] = 
{
	0,					// FIELD_VOID
	sizeof(float),		// FIELD_FLOAT
	sizeof(int),		// FIELD_STRING
	sizeof(Vector),		// FIELD_VECTOR
	sizeof(Quaternion),	// FIELD_QUATERNION
	sizeof(int),		// FIELD_INTEGER
	sizeof(char),		// FIELD_BOOLEAN
	sizeof(short),		// FIELD_SHORT
	sizeof(char),		// FIELD_CHARACTER
	sizeof(color32),	// FIELD_COLOR32
	sizeof(int),		// FIELD_EMBEDDED	(handled specially)
	sizeof(int),		// FIELD_CUSTOM		(handled specially)

	//---------------------------------

	sizeof(int),		// FIELD_CLASSPTR
	sizeof(EHANDLE),	// FIELD_EHANDLE
	sizeof(int),		// FIELD_EDICT

	sizeof(Vector),		// FIELD_POSITION_VECTOR
	sizeof(float),		// FIELD_TIME
	sizeof(int),		// FIELD_TICK
	sizeof(int),		// FIELD_MODELNAME
	sizeof(int),		// FIELD_SOUNDNAME

	sizeof(int),		// FIELD_INPUT		(uses custom type)
	sizeof(int *),		// FIELD_FUNCTION
	sizeof(VMatrix),	// FIELD_VMATRIX
	sizeof(VMatrix),	// FIELD_VMATRIX_WORLDSPACE
	sizeof(matrix3x4_t),// FIELD_MATRIX3X4_WORLDSPACE	// NOTE: Use array(FIELD_FLOAT, 12) for matrix3x4_t NOT in worldspace
	sizeof(interval_t), // FIELD_INTERVAL
	sizeof(int),		// FIELD_MODELINDEX
};

#define PREDICTIONCOPY_APPLY( func, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize )		\
	switch( nFieldType )																			\
	{																								\
	case FIELD_EMBEDDED:																			\
		{																							\
			Error( "FIELD_EMBEDDED in flat list!!!" );												\
		}																							\
		break;																						\
	case FIELD_FLOAT:																				\
		func( pCurrentMap, pField, (float *)pOutputData, (const float *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_STRING:																				\
		func( pCurrentMap, pField, (char *)pOutputData, (const char *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_VECTOR:																				\
		func( pCurrentMap, pField, (Vector *)pOutputData, (const Vector *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_QUATERNION:																			\
		func( pCurrentMap, pField, (Quaternion *)pOutputData, (const Quaternion *)pInputData, fieldSize );\
		break;																						\
	case FIELD_COLOR32:																				\
		func( pCurrentMap, pField, (color32 *)pOutputData, (const color32 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_BOOLEAN:																				\
		func( pCurrentMap, pField, (bool *)pOutputData, (const bool *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_INTEGER:																				\
		func( pCurrentMap, pField, (int *)pOutputData, (const int *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_SHORT:																				\
		func( pCurrentMap, pField, (short *)pOutputData, (const short *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_CHARACTER:																			\
		func( pCurrentMap, pField, (uint8 *)pOutputData, (const uint8 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_EHANDLE:																				\
		func( pCurrentMap, pField, (EHANDLE *)pOutputData, (const EHANDLE *)pInputData, fieldSize );\
		break;																						\
	default:																						\
		break;																						\
	}

#define PREDICTIONCOPY_APPLY_NOEMBEDDED( func, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize )		\
	switch( nFieldType )																			\
	{																								\
	case FIELD_FLOAT:																				\
		func( pCurrentMap, pField, (float *)pOutputData, (const float *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_STRING:																				\
		func( pCurrentMap, pField, (char *)pOutputData, (const char *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_VECTOR:																				\
		func( pCurrentMap, pField, (Vector *)pOutputData, (const Vector *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_QUATERNION:																			\
		func( pCurrentMap, pField, (Quaternion *)pOutputData, (const Quaternion *)pInputData, fieldSize );\
		break;																						\
	case FIELD_COLOR32:																				\
		func( pCurrentMap, pField, (color32 *)pOutputData, (const color32 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_BOOLEAN:																				\
		func( pCurrentMap, pField, (bool *)pOutputData, (const bool *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_INTEGER:																				\
		func( pCurrentMap, pField, (int *)pOutputData, (const int *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_SHORT:																				\
		func( pCurrentMap, pField, (short *)pOutputData, (const short *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_CHARACTER:																			\
		func( pCurrentMap, pField, (uint8 *)pOutputData, (const uint8 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_EHANDLE:																				\
		func( pCurrentMap, pField, (EHANDLE *)pOutputData, (const EHANDLE *)pInputData, fieldSize );\
		break;																						\
	default:																						\
		break;																						\
	}

CPredictionCopy::CPredictionCopy( int type, byte *dest, bool dest_packed, const byte *src, bool src_packed, 
	optype_t opType, FN_FIELD_COMPARE func /*= NULL*/ )
{
	m_OpType			= opType;
	m_nType				= type;
	m_pDest				= dest;
	m_pSrc				= src;
	m_nDestOffsetIndex	= dest_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL;
	m_nSrcOffsetIndex	= src_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL;

	m_nErrorCount		= 0;
	m_nEntIndex			= -1;

	m_pWatchField		= NULL;
	m_FieldCompareFunc	= func;
}

static ConVar cl_pred_error_verbose( "cl_pred_error_verbose", "0", 0, "Show more field info when spewing prediction errors." );

template< class T >
inline void CPredictionCopy::CopyField( difftype_t difftype, T *outvalue, const T *invalue, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		outvalue[ i ] = invalue[ i ];
	}
}

// specialized for strings
template<>
inline void CPredictionCopy::CopyField( difftype_t difftype, char *outvalue, const char *invalue, int count )
{
	Q_strcpy( outvalue, invalue );
}

template< class T >
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const T *outvalue, int count )
{
	Assert( 0 );
}

// Short
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const short *outvalue, int count )
{
	WatchMsg( pField, "short (%i)", (int)(outvalue[0]) );
}

// Int
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const int *outvalue, int count )
{
	bool described = false;
	if ( pField->flags & FTYPEDESC_MODELINDEX )
	{
		int modelindex = outvalue[0];
		model_t const *m = modelinfo->GetModel( modelindex );
		if ( m )
		{
			described = true;
			char shortfile[ 512 ];
			shortfile[ 0 ] = 0;
			Q_FileBase( modelinfo->GetModelName( m ), shortfile, sizeof( shortfile ) );

			WatchMsg( pField, "integer (%i->%s)", outvalue[0], shortfile );
		}
	}

	if ( !described )
	{
		WatchMsg( pField, "integer (%i)", outvalue[0] );
	}
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const bool *outvalue, int count )
{
	WatchMsg( pField, "bool (%s)", (outvalue[0]) ? "true" : "false" );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const float *outvalue, int count )
{
	WatchMsg( pField, "float (%f)", outvalue[ 0 ] );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const char *outstring, int count )
{
	WatchMsg( pField, "string (%s)", outstring );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Vector* outValue, int count )
{
	WatchMsg( pField, "vector (%f %f %f)", outValue[0].x, outValue[0].y, outValue[0].z );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Quaternion* outValue, int count )
{
	WatchMsg( pField, "quaternion (%f %f %f %f)", outValue[0].x, outValue[0].y, outValue[0].z, outValue[0].w );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const EHANDLE *outvalue, int count )
{
	C_BaseEntity *ent = outvalue[0].Get();
	if ( ent )
	{
		const char *classname = ent->GetClassname();
		if ( !classname[0] )
		{
			classname = typeid( *ent ).name();
		}

		WatchMsg( pField, "EHandle (0x%p->%s)", (void *)outvalue[ 0 ], classname );
	}
	else
	{
		WatchMsg( pField, "EHandle (NULL)" );
	}
}

void CPredictionCopy::DumpWatchField( const typedescription_t *pField, const byte *outvalue, int count )
{
	switch ( pField->fieldType )
	{
	case FIELD_FLOAT:
		WatchField( pField, (const float *)outvalue, count );
		break;
	case FIELD_STRING:
		WatchField( pField, (const char *)outvalue, count );
		break;
	case FIELD_VECTOR:
		WatchField( pField, (const Vector *)outvalue, count );
		break;
	case FIELD_QUATERNION:
		WatchField( pField, (const Quaternion *)outvalue, count );
		break;
	case FIELD_COLOR32:
		WatchField( pField, (const color32 *)outvalue, count );
		break;
	case FIELD_BOOLEAN:
		WatchField( pField, (const bool *)outvalue, count );
		break;
	case FIELD_INTEGER:
		WatchField( pField, (const int *)outvalue, count );
		break;
	case FIELD_SHORT:
		WatchField( pField, (const short *)outvalue, count );
		break;
	case FIELD_CHARACTER:
		WatchField( pField, (const uint8 *)outvalue, count );
		break;
	case FIELD_EHANDLE:
		WatchField( pField, (const EHANDLE *)outvalue, count );
		break;
	default:
		break;		
	}
}

// color32
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const color32 *outvalue, int count )
{
	WatchMsg( pField, "color32 (%d %d %d %d)", outvalue[0].r, outvalue[0].g, outvalue[0].b, outvalue[0].a );
}

inline bool QuaternionCompare( const Quaternion& q1, const Quaternion& q2 )
{
	for ( int i = 0; i < 4; ++i )
	{
		if ( q1[i] != q2[i] )
			return false;
	}
	return true;
}

template< class T >
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const T *outvalue, const T *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ] == invalue[ i ] )
			continue;
		return DIFFERS;
	}
	return IDENTICAL;
}

// float uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const float *outvalue, const float *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	float tolerance = pField->fieldTolerance;
	Assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			float diff = fabs( outvalue[ i ] - invalue[ i ] );
			if ( diff <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( outvalue[ i ] == invalue[ i ] )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// vector uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Vector *outvalue, const Vector *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	float tolerance = pField->fieldTolerance;
	Assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			Vector delta = outvalue[ i ] - invalue[ i ];

			if ( delta.x <= tolerance &&
				delta.y <= tolerance &&
				delta.z <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( outvalue[ i ] == invalue[ i ] )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// quaternion uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Quaternion *outvalue, const Quaternion *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	
	float tolerance = pField->fieldTolerance;
	Assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			Quaternion delta;
			for ( int j = 0; j < 4; j++ )
			{
				delta[i] = outvalue[i][j] - invalue[i][j];
			}

			if ( delta.x <= tolerance &&
				delta.y <= tolerance &&
				delta.z <= tolerance &&
				delta.w <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( QuaternionCompare( outvalue[ i ], invalue[ i ] ) )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// string
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const char *outvalue, const char *invalue, int count )
{
	if ( Q_strcmp( outvalue, invalue ) )
	{
		return DIFFERS;
	}
	return IDENTICAL;
}

// ehandle
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const EHANDLE *outvalue, const EHANDLE *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ].Get() == invalue[ i ].Get() )
			continue;
		return DIFFERS;
	}
	return IDENTICAL;
}

// color32
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const color32 *outvalue, const color32 *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ] != invalue[ i ] )
			return DIFFERS;
	}
	return IDENTICAL;
}

template< class T >
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const T *outvalue, const T *invalue, int count )
{
	Assert( 0 );
}

// short
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const short *outvalue, const short *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "short differs (net %i pred %i) diff(%i)\n", (int)(invalue[i]), (int)(outvalue[i]), (int)(outvalue[i] - invalue[i]) );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "short (%i)\n", (int)(outvalue[0]) );
}

// int
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const int *outvalue, const int *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "int differs (net %i pred %i) diff(%i)\n", invalue[i], outvalue[i], outvalue[i] - invalue[i] );
	}

	bool described = false;
	if ( pField->flags & FTYPEDESC_MODELINDEX )
	{
		int modelindex = outvalue[0];
		model_t const *m = modelinfo->GetModel( modelindex );
		if ( m )
		{
			described = true;
			char shortfile[ 512 ];
			shortfile[ 0 ] = 0;
			Q_FileBase( modelinfo->GetModelName( m ), shortfile, sizeof( shortfile ) );

			OutputFieldDescription( pCurrentMap, pField, difftype, "integer (%i->%s)\n", outvalue[0], shortfile );
		}
	}

	if ( !described )
	{
		OutputFieldDescription( pCurrentMap, pField, difftype, "integer (%i)\n", outvalue[0] );
	}
}

// bool
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const bool *outvalue, const bool *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "bool differs (net %s pred %s)\n", (invalue[i]) ? "true" : "false", (outvalue[i]) ? "true" : "false" );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "bool (%s)\n", (outvalue[0]) ? "true" : "false" );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const float *outvalue, const float *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "float differs (net %f pred %f) diff(%f)\n", invalue[ i ], outvalue[ i ], outvalue[ i ] - invalue[ i ] );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "float (%f)\n", outvalue[ 0 ] );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const char *outstring, const char *instring, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "string differs (net %s pred %s)\n", instring, outstring );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "string (%s)\n", outstring );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Vector *outValue, const Vector *inValue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		Vector delta = outValue[ i ] - inValue[ i ];
		ReportFieldsDiffer( pCurrentMap, pField, "vec[] differs (1st diff) (net %f %f %f - pred %f %f %f) delta(%f %f %f)\n", 
			inValue[i].x, inValue[i].y, inValue[i].z,
			outValue[i].x, outValue[i].y, outValue[i].z,
			delta.x, delta.y, delta.z );
	}
	OutputFieldDescription( pCurrentMap, pField, difftype, "vector (%f %f %f)\n", 
		outValue[0].x, outValue[0].y, outValue[0].z );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Quaternion *outValue, const Quaternion *inValue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		Quaternion delta;
		for ( int j = 0; j < 4; j++ )
		{
			delta[i] = outValue[i][j] - inValue[i][j];
		}

		ReportFieldsDiffer( pCurrentMap, pField, "quaternion[] differs (1st diff) (net %f %f %f %f - pred %f %f %f %f) delta(%f %f %f %f)\n", 
			inValue[i].x, inValue[i].y, inValue[i].z, inValue[i].w,
			outValue[i].x, outValue[i].y, outValue[i].z, outValue[i].w,
			delta[0], delta[1], delta[2], delta[3] );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "quaternion (%f %f %f %f)\n", outValue[0][0], outValue[0][1], outValue[0][2], outValue[3] );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const EHANDLE *outvalue, EHANDLE const *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "EHandles differ (net) 0x%p (pred) 0x%p\n", (void const *)invalue[ i ].Get(), (void *)outvalue[ i ].Get() );
	}

	C_BaseEntity *ent = outvalue[0].Get();
	if ( ent )
	{
		const char *classname = ent->GetClassname();
		if ( !classname[0] )
		{
			classname = typeid( *ent ).name();
		}

		OutputFieldDescription( pCurrentMap, pField, difftype, "EHandle (0x%p->%s)", (void *)outvalue[ 0 ], classname );
	}
	else
	{
		OutputFieldDescription( pCurrentMap, pField, difftype, "EHandle (NULL)" );
	}
}

// color32
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const color32 *outvalue, const color32 *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "color differs (net %d %d %d %d pred %d %d %d %d)\n", 
			outvalue[ 0 ].r,
			outvalue[ 0 ].g,
			outvalue[ 0 ].b,
			outvalue[ 0 ].a,
			invalue[ 0 ].r,
			invalue[ 0 ].g,
			invalue[ 0 ].b,
			invalue[ 0 ].a
			);
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "color (%d %d %d %d)\n", 
		outvalue[ 0 ].r,
		outvalue[ 0 ].g,
		outvalue[ 0 ].b,
		outvalue[ 0 ].a );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const uint8 *outstring, const uint8 *instring, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "byte differs (net %d pred %d)\n", int(instring[0]), int(outstring[0]) );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "byte (%d)\n", int(outstring[0]) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::ReportFieldsDiffer( const datamap_t *pCurrentMap, const typedescription_t *pField, const char *fmt, ... )
{
	++m_nErrorCount;

	if ( m_FieldCompareFunc )
		return;

	const char *fieldname = "empty";
	const char *classname = "empty";
	int flags = 0;

	if ( pField )
	{
		flags		= pField->flags;
		fieldname	= pField->fieldName ? pField->fieldName : "NULL";
		classname   = pCurrentMap->dataClassName;
	}

	va_list argptr;
	char data[ 4096 ];
	int len;
	va_start(argptr, fmt);
	len = Q_vsnprintf(data, sizeof( data ), fmt, argptr);
	va_end(argptr);

	bool bUseLongName = cl_pred_error_verbose.GetBool();
	CUtlString longName;
	for ( int i = 0; i < m_FieldStack.Count(); ++i )
	{
		const typedescription_t *top = m_FieldStack[ i ];
		if ( !top )
			continue;

		if ( top->flags & FTYPEDESC_KEY )
			bUseLongName = true;

		longName += top->fieldName ? top->fieldName : "NULL";
		longName += "/";
	}
	
	if ( bUseLongName )
	{
		Msg( "%2d (%d)%s%s::%s - %s",
			m_nErrorCount,
			m_nEntIndex,
			longName.String(),
			classname,
			fieldname,
			data );
	}
	else
	{
		Msg( "%2d (%d)%s::%s - %s",
			m_nErrorCount,
			m_nEntIndex,
			classname,
			fieldname,
			data );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::OutputFieldDescription( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t dt, const char *fmt, ... )
{
	if ( !m_FieldCompareFunc )
		return;

	const char *fieldname = "empty";
	const char *classname = "empty";
	int flags = 0;

	if ( pField )
	{
		flags		= pField->flags;
		fieldname	= pField->fieldName ? pField->fieldName : "NULL";
		classname   = pCurrentMap->dataClassName;
	}

	va_list argptr;
	char data[ 4096 ];
	int len;
	va_start(argptr, fmt);
	len = Q_vsnprintf(data, sizeof( data ), fmt, argptr);
	va_end(argptr);

	bool isnetworked = ( flags & FTYPEDESC_INSENDTABLE ) ? true : false;
	bool isnoterrorchecked = ( flags & FTYPEDESC_NOERRORCHECK ) ? true : false;

	( *m_FieldCompareFunc )( 
		classname,
		fieldname,
		g_FieldTypes[ pField->fieldType ],
		isnetworked,
		isnoterrorchecked,
		dt != IDENTICAL ? true : false,
		dt == WITHINTOLERANCE ? true : false,
		data 
	);
}

void CPredictionCopy::DescribeFields( const CUtlVector< const datamap_t * > &vecGroups, const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = pCurrentMap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
	int fieldCount = flat.m_Flattened.Count();
	const typedescription_t * RESTRICT pField = &flat.m_Flattened[ 0 ];
	PREFETCH360(pField, 0);

	for ( i = 0; i < fieldCount; ++i, pField++ )
	{
#if _X360
		if ( !(i & 0xF) )
		{
			PREFETCH360(pField, 128);
		}
#endif
		flags = pField->flags;
		int nFieldType = pField->fieldType;

		const byte * RESTRICT pOutputData;
		const byte * RESTRICT pInputData;

		fieldOffsetDest = pField->flatOffset[ m_nSrcOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nDestOffsetIndex ];
		fieldSize = pField->fieldSize;

		pOutputData = (m_pDest + fieldOffsetDest );
		pInputData = (m_pSrc + fieldOffsetSrc );

		const datamap_t *sourceGroup = pCurrentMap;
		if ( pField->flatGroup >= 0 && pField->flatGroup < vecGroups.Count() )
		{
			sourceGroup = vecGroups[ pField->flatGroup ];
		}

		PREDICTIONCOPY_APPLY( ProcessField_Describe, nFieldType, sourceGroup, pField, pOutputData, pInputData, fieldSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: static method
// Input  : *fieldname - 
//			*dmap - 
// Output : typedescription_t
//-----------------------------------------------------------------------------
// static method
const typedescription_t *CPredictionCopy::FindFlatFieldByName( const char *fieldname, const datamap_t *dmap )
{
	PrepareDataMap( const_cast< datamap_t * >( dmap ) );

	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		const flattenedoffsets_t &flat = dmap->m_pOptimizedDataMap->m_Info[ i ].m_Flat;
		int c = flat.m_Flattened.Count();
		for ( int j = 0; j < c; ++j )
		{
			const typedescription_t *td = &flat.m_Flattened[ j ];
			if ( !Q_stricmp( td->fieldName, fieldname ) )
			{
				return td;
			}
		}
	}
	return NULL;
}

static ConVar pwatchent( "pwatchent", "-1", FCVAR_CHEAT, "Entity to watch for prediction system changes." );
static ConVar pwatchvar( "pwatchvar", "", FCVAR_CHEAT, "Entity variable to watch in prediction system for changes." );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::WatchMsg( const typedescription_t *pField, const char *fmt, ... )
{
	Assert( pField );
	Assert( m_pOperation );

	va_list argptr;
	char data[ 4096 ];
	int len;
	va_start(argptr, fmt);
	len = Q_vsnprintf(data, sizeof( data ), fmt, argptr);
	va_end(argptr);

	Msg( "%i %s %s : %s\n", gpGlobals->tickcount, m_pOperation, pField->fieldName, data );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *operation - 
//			entindex - 
//			*dmap - 
//-----------------------------------------------------------------------------
void CPredictionCopy::DetermineWatchField( const char *operation, int entindex, const datamap_t *dmap )
{
	m_pWatchField = NULL;
	m_pOperation = operation;
	if ( !m_pOperation || !m_pOperation[0] )
		return;

	int enttowatch = pwatchent.GetInt();
	if ( enttowatch < 0 )
		return;

	if ( entindex != enttowatch )
		return;

	// See if they specified a field
	if ( pwatchvar.GetString()[0] == 0 )
		return;

	m_pWatchField = CPredictionCopy::FindFlatFieldByName( pwatchvar.GetString(), dmap );
}

static void RemoveFieldsByName( char const *pchFieldName, CUtlVector< typedescription_t > &build )
{
	// Don't start at final field, since it the one we just added with FTYPEDESC_OVERRIDE
	for ( int i = build.Count() - 2; i >= 0 ; --i )
	{
		// Embedded field "offsets" can be the same as their first data field, so don't remove
		if ( build[ i ].fieldType == FIELD_EMBEDDED )
			continue;

		if ( !Q_stricmp( build[ i ].fieldName, pchFieldName ) )
		{
			// Msg( "Removing %s %d due to override\n", build[ i ]->fieldName, build[ i ]->flatOffset[ TD_OFFSET_NORMAL ] );
			build.Remove( i );
		}
	}
}

static void BuildGroupList_R( int nPredictionCopyType, int nGroup, const datamap_t *dmap, CUtlVector< const datamap_t * > &vecGroups )
{
	// Descend to base classes first so FTYPEDESC_OVERRIDE can remove the previous ones
	if ( dmap->baseMap )
	{
		BuildGroupList_R( nPredictionCopyType, nGroup + 1, dmap->baseMap, vecGroups );
	}

	vecGroups.AddToTail( dmap );
}

static void BuildFlattenedChains_R( 
	int nPredictionCopyType, 
	int &nMaxGroupSeen, 
	int nGroup, 
	datamap_t *dmap, 
	CUtlVector< typedescription_t > &build, 
	int nBaseOffset )
{
	// Descend to base classes first so FTYPEDESC_OVERRIDE can remove the previous ones
	if ( dmap->baseMap )
	{
		BuildFlattenedChains_R( nPredictionCopyType, nMaxGroupSeen, nGroup + 1, dmap->baseMap, build, nBaseOffset );
	}

	if ( nGroup > nMaxGroupSeen )
	{
		nMaxGroupSeen = nGroup;
	}

	int c = dmap->dataNumFields;
	for ( int i = 0; i < c; i++ )
	{
		typedescription_t *pField = &dmap->dataDesc[ i ];
		if ( pField->fieldType == FIELD_VOID )
			continue;

		bool bAdd = true;
		if ( pField->fieldType != FIELD_EMBEDDED )
		{
			if ( pField->flags & FTYPEDESC_PRIVATE )
			{
				if ( pField->flags & FTYPEDESC_OVERRIDE )
				{
					// Find previous field targeting same offset
					RemoveFieldsByName( pField->fieldName, build );
				}
				continue;
			}

			// For PC_NON_NETWORKED_ONLYs skip any fields that are present in the network send tables
			if ( nPredictionCopyType == PC_NON_NETWORKED_ONLY && ( pField->flags & FTYPEDESC_INSENDTABLE ) )
			{
				bAdd = false;
			}
			// For PC_NETWORKED_ONLYs skip any fields that are not present in the network send tables
			if ( nPredictionCopyType == PC_NETWORKED_ONLY && !( pField->flags & FTYPEDESC_INSENDTABLE ) )
			{
				bAdd = false;
			}
		}
		else
		{
			bAdd = false;
		}

		pField->flatGroup = nGroup;
		pField->flatOffset[ TD_OFFSET_NORMAL ] = nBaseOffset + pField->fieldOffset;

		if ( bAdd )
		{
			build.AddToTail( *pField );
		}

		//	Msg( "Visit %s offset %d\n", pField->fieldName, pField->flatOffset[ nPackType ] );

		if ( pField->fieldType == FIELD_EMBEDDED )
		{
			AssertFatalMsg( !(pField->flags & FTYPEDESC_PTR ), ( "Prediction copy does not support FTYPEDESC_PTR(%d)", pField->fieldName ) );
			BuildFlattenedChains_R( nPredictionCopyType, nMaxGroupSeen, nGroup, pField->td, build, pField->flatOffset[ TD_OFFSET_NORMAL ] );
		}

		if ( pField->flags & FTYPEDESC_OVERRIDE )
		{
			// Find previous field targeting same offset
			RemoveFieldsByName( pField->fieldName, build );
		}
	}	
}

static int __cdecl CompareFlattenedOffsets( const void *pv1, const void *pv2 )
{
	const typedescription_t *td1 = (const typedescription_t *)pv1;
	const typedescription_t *td2 = (const typedescription_t *)pv2;
	if ( td1->flatOffset[ TD_OFFSET_NORMAL ] < td2->flatOffset[ TD_OFFSET_NORMAL ] )
		return -1;
	else if ( td1->flatOffset[ TD_OFFSET_NORMAL ] > td2->flatOffset[ TD_OFFSET_NORMAL ] )
		return 1;
	if ( td1->flatGroup < td2->flatGroup )
		return -1;
	else if ( td1->flatGroup > td2->flatGroup )
		return 1;

	return 0;
}

static int BuildPackedFlattenedOffsets( int nStartOffset, flattenedoffsets_t &flat )
{
	int current_position = nStartOffset;

	for ( int i = 0; i < flat.m_Flattened.Count(); ++i )
	{
		typedescription_t *field = &flat.m_Flattened[ i ];

		switch ( field->fieldType )
		{
		default:
		case FIELD_MODELINDEX:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_TIME:
		case FIELD_TICK:
		case FIELD_CUSTOM:
		case FIELD_CLASSPTR:
		case FIELD_EDICT:
		case FIELD_POSITION_VECTOR:
		case FIELD_FUNCTION:
			Assert( 0 );
			break;

		case FIELD_EMBEDDED:
			{
				Error( "Not expecting FIELD_EMBEDDED in flattened list (%s)", field->fieldName );
			}
			break;

		case FIELD_FLOAT:
		case FIELD_VECTOR:
		case FIELD_QUATERNION:
		case FIELD_INTEGER:
		case FIELD_EHANDLE:
		case FIELD_COLOR32:
		case FIELD_VMATRIX:
		case FIELD_VECTOR4D:
			{
				current_position = ALIGN_VALUE( current_position, 4 );
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_SHORT:
			{
				current_position = ALIGN_VALUE( current_position, 2 );
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_STRING:
		case FIELD_BOOLEAN:
		case FIELD_CHARACTER:
			{
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_VOID:
			{
				// Special case, just skip it
			}
			break;
		}

		// Msg( "%s packed to %d size %d\n", field->fieldName, field->flatOffset[ TD_OFFSET_PACKED ], field->fieldSizeInBytes );
	}

	flat.m_nPackedStartOffset = nStartOffset;
	flat.m_nPackedSize = current_position - nStartOffset;

	current_position = ALIGN_VALUE( current_position, 4 );

	return current_position;
}

static void BuildDataRuns( datamap_t *dmap )
{
	for ( int pc = 0; pc < PC_COPYTYPE_COUNT; ++pc )
	{
		datamapinfo_t &info = dmap->m_pOptimizedDataMap->m_Info[ pc ];
		datacopyruns_t *runs = &info.m_CopyRuns;

		Assert( !info.m_CopyRuns.m_vecRuns.Count() );

		const flattenedoffsets_t &flat = info.m_Flat;

		int nRunStartField = 0;
		int nCurrentRunStartOffset = 0;
		int	nLastFieldEndOffset = 0;

		CUtlVector< datarun_t > &vecRuns = runs->m_vecRuns;

		int i;
		for ( i = 0; i < flat.m_Flattened.Count(); ++i )
		{
			const typedescription_t *td = &flat.m_Flattened[ i ];
			int offset = td->flatOffset[ TD_OFFSET_NORMAL ];
			if ( i == 0 )
			{
				nRunStartField = i;
				nLastFieldEndOffset = offset;
				nCurrentRunStartOffset = offset;
			}

			if ( td->fieldType == FIELD_EMBEDDED )
			{
				Assert( 0 );
				continue;
			}

			if ( nLastFieldEndOffset != offset )
			{
				datarun_t run;
				run.m_nStartFlatField = nRunStartField;
				run.m_nEndFlatField = i;
				Assert( nCurrentRunStartOffset == flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_NORMAL ] );
				run.m_nStartOffset[ TD_OFFSET_NORMAL ] = nCurrentRunStartOffset;
				run.m_nStartOffset[ TD_OFFSET_PACKED ] = flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_PACKED ];
				run.m_nLength = nLastFieldEndOffset - nCurrentRunStartOffset;

#ifdef _X360 
				if ( vecRuns.Count() > 0 )
				{
					for ( int td = 0; td < TD_OFFSET_COUNT; ++td )
					{
						vecRuns[ vecRuns.Count() - 1 ].m_nPrefetchOffset[ td ] = run.m_nStartOffset[ td ];
					}
				}
#endif
				vecRuns.AddToTail( run );

				nRunStartField = i;
				nCurrentRunStartOffset = offset;
			}
			nLastFieldEndOffset = td->flatOffset[ TD_OFFSET_NORMAL ] + td->fieldSizeInBytes;
		}

		// Close off last run
		if ( nLastFieldEndOffset != nCurrentRunStartOffset )
		{
			datarun_t run;
			run.m_nStartFlatField = nRunStartField;
			run.m_nEndFlatField = i - 1;
			Assert( nCurrentRunStartOffset == flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_NORMAL ] );
			run.m_nStartOffset[ TD_OFFSET_NORMAL ] = nCurrentRunStartOffset;
			run.m_nStartOffset[ TD_OFFSET_PACKED ] = flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_PACKED ];
			run.m_nLength = nLastFieldEndOffset - nCurrentRunStartOffset;

#ifdef _X360 
			if ( vecRuns.Count() > 0 )
			{
				for ( int td = 0; td < TD_OFFSET_COUNT; ++td )
				{
					vecRuns[ vecRuns.Count() - 1 ].m_nPrefetchOffset[ td ] = run.m_nStartOffset[ td ];
				}
			}
#endif
			vecRuns.AddToTail( run );
		}
	}
}

static void BuildFlattenedChains( datamap_t *dmap )
{
	if ( dmap->m_pOptimizedDataMap )
		return;
	dmap->m_pOptimizedDataMap = g_OptimizedDataMapPool.AllocZero();

	int nMaxGroupSeen[ PC_COPYTYPE_COUNT ] = { 0 };

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		CUtlVector< typedescription_t > &build = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat.m_Flattened;

		int nGroupCount = 0;
		int nBaseOffset = 0;
		BuildFlattenedChains_R( 
			nPredictionCopyType, 
			nMaxGroupSeen[ nPredictionCopyType ], 
			nGroupCount, 
			dmap, 
			build, 
			nBaseOffset );
		// Msg( "%d == %d entries\n", nPredictionCopyType, build[ nPredictionCopyType ].Count() );
	}

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		int nMaxGroup = nMaxGroupSeen[ nPredictionCopyType ];

		CUtlVector< typedescription_t > &build = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat.m_Flattened;

		for ( int i = 0; i < build.Count(); ++i )
		{
			typedescription_t *field = &build[ i ];
			field->flatGroup = nMaxGroup - field->flatGroup;
		}
	}

	int nPackedDataStartOffset = 0;
	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		flattenedoffsets_t &flat = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
		qsort( flat.m_Flattened.Base(), flat.m_Flattened.Count(), sizeof( typedescription_t ), (int (__cdecl *)(const void *, const void *))CompareFlattenedOffsets );

		nPackedDataStartOffset = BuildPackedFlattenedOffsets( nPackedDataStartOffset, flat );
		dmap->m_nPackedSize = nPackedDataStartOffset;
	}

	BuildDataRuns( dmap );
}

const tokenset_t< int > s_PredCopyType[] =
{
	{ "Non-Sendtable"	, PC_NON_NETWORKED_ONLY },	
	{ "SendTable"		, PC_NETWORKED_ONLY },											
	{ "Everything"		, PC_EVERYTHING },													
	{ NULL, -1 }
};

const tokenset_t< int > s_PredPackType[] =
{
	{ "Normal"	, TD_OFFSET_NORMAL },	
	{ "Packed"	, TD_OFFSET_PACKED },											
	{ NULL, -1 }
};

static void DescribeRuns( const datamap_t *dmap, int nPredictionCopyType, int packType )
{
	const datacopyruns_t &runs = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_CopyRuns;
	const flattenedoffsets_t &flat = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
	Msg( "   Runs for copy type: %s, packing: %s\n", s_PredCopyType->GetNameByToken( nPredictionCopyType ), s_PredPackType->GetNameByToken( packType ) );
	for ( int i = 0; i < runs.m_vecRuns.Count(); ++i )
	{
		const datarun_t *run = &runs.m_vecRuns[ i ];
		Msg( "     %5d:  %5d -> %5d (%5d bytes): %s to %s\n",
			i, run->m_nStartOffset[ packType ], run->m_nStartOffset[ packType ] + run->m_nLength, run->m_nLength,
			flat.m_Flattened[ run->m_nStartFlatField ].fieldName,
			flat.m_Flattened[ run->m_nEndFlatField ].fieldName );
	}
}

static void DescribeFlattenedList( const datamap_t *dmap, int nPredictionCopyType, int packType )
{
	char const *prefix = dmap->dataClassName;

	Msg( "->Sorted %s for copy type: %s, packing: %s\n", prefix, s_PredCopyType->GetNameByToken( nPredictionCopyType ), s_PredPackType->GetNameByToken( packType ) );
	// Now dump the flattened list
	int nLastFieldEnd = 0;
	int nCurrentRunStartOffset = 0;

	int nRuns = 0;
	int nBytesInRuns = 0;
	int offset = 0;

	const flattenedoffsets_t &list = dmap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
	for ( int i = 0; i < list.m_Flattened.Count(); ++i )
	{
		const typedescription_t *td = &list.m_Flattened[ i ];
		offset = td->flatOffset[ packType ];
		if ( i == 0 )
		{
			nLastFieldEnd = offset;
			nCurrentRunStartOffset = offset;
		}

		if ( td->fieldType != FIELD_EMBEDDED )
		{
			if ( nLastFieldEnd != offset )
			{
				Msg( "  gap of %d bytes [last run %d]\n", offset - nLastFieldEnd, ( nLastFieldEnd - nCurrentRunStartOffset ) );
				++nRuns;
				nBytesInRuns += ( nLastFieldEnd - nCurrentRunStartOffset );

				nCurrentRunStartOffset = offset;
			}
			nLastFieldEnd = td->flatOffset[ packType ] + td->fieldSizeInBytes;
		}

		Msg( "group %s [flat %d] [sort %d] %d bytes\n",
			td->fieldName,
			td->flatOffset[ packType ],
			td->flatOffset[ TD_OFFSET_NORMAL ],
			td->fieldSizeInBytes );
	}

	// Close off last run
	if ( nLastFieldEnd != nCurrentRunStartOffset )
	{
		Msg( "Last run %d\n", nLastFieldEnd - nCurrentRunStartOffset );
		++nRuns;
		nBytesInRuns += ( nLastFieldEnd - nCurrentRunStartOffset );
	}

	if ( nRuns > 0 )
	{
		float flAvgBytesPerRun = (float)nBytesInRuns/(float)nRuns;

		Msg( "%d runs, %d bytes in runs, %f avg bytes per run\n",
			nRuns, nBytesInRuns, flAvgBytesPerRun );
	}
	Msg( "->\n" );

	DescribeRuns( dmap, nPredictionCopyType, packType );
}

void CPredictionCopy::CopyFlatFieldsUsingRuns( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	byte			* RESTRICT pOutputData;
	const byte		* RESTRICT pInputData;

	const datacopyruns_t &runs = pCurrentMap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_CopyRuns;

	byte * RESTRICT pDest = ( byte * RESTRICT )m_pDest;
	const byte * RESTRICT pSrc = (const byte * RESTRICT)m_pSrc;

	PREFETCH360( pSrc, 0 );
	PREFETCH360( pDest, 0 );

	int c = runs.m_vecRuns.Count();
	for ( int i = 0; i < c; ++i )
	{
		const datarun_t * RESTRICT run = &runs.m_vecRuns[ i ];
		fieldOffsetDest = run->m_nStartOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= run->m_nStartOffset[ m_nSrcOffsetIndex ];
		pOutputData = pDest + fieldOffsetDest;
		pInputData = pSrc + fieldOffsetSrc;

#ifdef _X360
		PREFETCH360( pDest + run->m_nPrefetchOffset[ m_nDestOffsetIndex ], 0 );
		PREFETCH360( pSrc + run->m_nPrefetchOffset[ m_nSrcOffsetIndex ], 0 );
#endif

		Q_memcpy( pOutputData, pInputData, run->m_nLength );
	}
}

void CPredictionCopy::CopyFlatFields( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	byte			* RESTRICT pOutputData;
	const byte		* RESTRICT pInputData;

	byte * RESTRICT pDest = (byte * RESTRICT)m_pDest;
	const byte * RESTRICT pSrc = (const byte * RESTRICT)m_pSrc;

	PREFETCH360( pSrc, 0 );
	PREFETCH360( pDest, 0 );

	const flattenedoffsets_t &flat = pCurrentMap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;

	int fieldCount = flat.m_Flattened.Count();
	for ( int i = 0; i < fieldCount; ++i )
	{
		const typedescription_t *pField = &flat.m_Flattened[ i ];

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];
		pOutputData = pDest + fieldOffsetDest;
		PREFETCH360( pOutputData, 0 );
		pInputData = pSrc + fieldOffsetSrc;
		PREFETCH360( pInputData, 0 );

		Q_memcpy( pOutputData, pInputData, pField->fieldSizeInBytes );
	}
}

void CPredictionCopy::ErrorCheckFlatFields_NoSpew( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = pCurrentMap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
	const typedescription_t *pBase = &flat.m_Flattened[ 0 ];
	PREFETCH360( pBase, 0 );
	int fieldCount = flat.m_Flattened.Count();

	const byte * RESTRICT pDest = (const byte * RESTRICT)m_pDest;
	const byte * RESTRICT pSrc = (const byte * RESTRICT)m_pSrc;
	
	const byte *pOutputData;
	const byte *pInputData;

	PREFETCH360( pSrc, 0 );
	PREFETCH360( pDest, 0 );

	for ( i = 0; i < fieldCount && !m_nErrorCount; ++i )
	{
		const typedescription_t * RESTRICT pField = &pBase[ i ];

		flags = pField->flags;
		if ( flags & FTYPEDESC_NOERRORCHECK )
			continue;

#if _X360
		if ( !(i & 0xF) )
		{
			PREFETCH360(pField, 128);
		}
#endif

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];

		pOutputData = pDest + fieldOffsetDest;
		PREFETCH360( pOutputData, 0 );
		pInputData = pSrc + fieldOffsetSrc;
		PREFETCH360( pInputData, 0 );

		fieldSize = pField->fieldSize;
		int nFieldType = pField->fieldType;

		PREDICTIONCOPY_APPLY( ProcessField_Compare_NoSpew, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize );
	}
}

void CPredictionCopy::ErrorCheckFlatFields_Spew( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = pCurrentMap->m_pOptimizedDataMap->m_Info[ nPredictionCopyType ].m_Flat;
	const typedescription_t *pBase = &flat.m_Flattened[ 0 ];
	PREFETCH360( pBase, 0 );
	int fieldCount = flat.m_Flattened.Count();

	const byte * RESTRICT pDest = m_pDest;
	const byte * RESTRICT pSrc = m_pSrc;

	const byte *pOutputData;
	const byte *pInputData;

	PREFETCH360( pSrc, 0 );
	PREFETCH360( pDest, 0 );

	for ( i = 0; i < fieldCount ; ++i )
	{
		const typedescription_t * RESTRICT pField = &pBase[ i ];

		flags = pField->flags;
		if ( flags & FTYPEDESC_NOERRORCHECK )
			continue;

#if _X360
		if ( !(i & 0xF) )
		{
			PREFETCH360(pField, 128);
		}
#endif

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];

		pOutputData = pDest + fieldOffsetDest;
		PREFETCH360( pOutputData, 0 );
		pInputData = pSrc + fieldOffsetSrc;
		PREFETCH360( pInputData, 0 );

		fieldSize = pField->fieldSize;
		flags = pField->flags;
		int nFieldType = pField->fieldType;

		PREDICTIONCOPY_APPLY( ProcessField_Compare_Spew, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize );
	}
}

bool CPredictionCopy::PrepareDataMap( datamap_t *dmap )
{
	bool bPerformedPrepare = false;
	if ( dmap && !dmap->m_pOptimizedDataMap )
	{
		bPerformedPrepare = true;
		BuildFlattenedChains( dmap );
		dmap = dmap->baseMap;
	}

	return bPerformedPrepare;
}

CON_COMMAND( cl_predictioncopy_describe, "Describe datamap_t for entindex" )
{
	if ( args.ArgC() <= 1 )
	{
		Msg( "Usage:  %s <entindex>", args[ 0 ] );
		return;
	}
	int entindex = Q_atoi( args[ 1 ] );
	C_BaseEntity *ent = C_BaseEntity::Instance( entindex );
	if ( !ent )
	{
		Msg( "cl_predictioncopy_describe:  no such entity %d\n", entindex );
		return;
	}

	datamap_t *dmap = ent->GetPredDescMap();
	if ( !dmap )
	{
		return;
	}
	CPredictionCopy::PrepareDataMap( dmap );

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		//for ( int i = 0; i < TD_OFFSET_COUNT; ++i )
		{
			DescribeFlattenedList( dmap, nPredictionCopyType, 0 );
		}
	}
}

// 0 PC_NON_NETWORKED_ONLY = (1<<0) or (1)
// 1 PC_NETWORKED_ONLY = (1<<1) or (2)
// 2 PC_EVERYTHING = ( PC_NON_NETWORKED_ONLY | PC_NETWORKED_ONLY ) or 3
// So for these three options, we just take the type and add one!!!
FORCEINLINE int ComputeTypeMask( int nType )
{
	return nType + 1;
}

#if 0 // Enable this for perf testing
static ConVar cl_predictioncopy_runs( "cl_predictioncopy_runs", "1" );
static ConVar cl_predictioncopy_repeats( "cl_predictioncopy_repeats", "1" );
void CPredictionCopy::TransferDataCopyOnly( datamap_t *dmap )
{
	int repeat = cl_predictioncopy_repeats.GetInt();
	for ( int k = 0; k < repeat; ++k )
	{
		int types = ComputeTypeMask( m_nType );
		for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
		{
			if ( types & (1<<i) )
			{
				if ( !cl_predictioncopy_runs.GetBool() )
				{
					CopyFlatFields( dmap, i );
				}
				else
				{	
					CopyFlatFieldsUsingRuns( dmap, i );
				}
			}
		}
	}
}
#else
void CPredictionCopy::TransferDataCopyOnly( const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			CopyFlatFieldsUsingRuns( dmap, i );
		}
	}
}
#endif

// Stop at first error
void CPredictionCopy::TransferDataErrorCheckNoSpew( char const *pchOperation, const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT && !m_nErrorCount; ++i )
	{
		if ( types & (1<<i) )
		{
			ErrorCheckFlatFields_NoSpew( dmap, i );
		}
	}
}

void CPredictionCopy::TransferDataErrorCheckSpew( char const *pchOperation, const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			ErrorCheckFlatFields_Spew( dmap, i );
		}
	}
}

void CPredictionCopy::TransferDataDescribe( char const *pchOperation, const datamap_t *dmap )
{
	// Copy from here first, then base classes
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			CUtlVector< const datamap_t * > vecGroups;
			int nGroup = 0;
			BuildGroupList_R( i, nGroup, dmap, vecGroups );
			DescribeFields( vecGroups, dmap, i );
		}
	}
}

int CPredictionCopy::TransferData( const char *operation, int entindex, datamap_t *dmap )
{
	m_nEntIndex = entindex;

	PrepareDataMap( dmap );

	switch ( m_OpType )
	{
	default:
		Assert( 0 );
	case TRANSFERDATA_COPYONLY:  // Data copying only (uses runs)
		{
			// Watch is based on "destination" of any write operation
			DetermineWatchField( operation, entindex, dmap );
			TransferDataCopyOnly( dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_NOSPEW: // Checks for errors, returns after first error found
		{
			TransferDataErrorCheckNoSpew( operation, dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_SPEW:   // checks for errors, reports all errors to console
		{
			TransferDataErrorCheckSpew( operation, dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_DESCRIBE:
		{
			TransferDataDescribe( operation, dmap );
		}
		break;
	}

	if ( m_pWatchField ) 
	{
		// Watch is based on "destination" of any write operation
		DumpWatchField( m_pWatchField, m_pDest + m_pWatchField->flatOffset[ m_nDestOffsetIndex ], m_pWatchField->fieldSize );
	}

	return m_nErrorCount;
}

#endif



