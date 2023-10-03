//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREDICTIONCOPY_H
#define PREDICTIONCOPY_H
#ifdef _WIN32
#pragma once
#endif

#include <memory.h>
#include "datamap.h"
#include "ehandle.h"
#include "tier1/utlstring.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstack.h"

#if defined( CLIENT_DLL )
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;

// #define COPY_CHECK_STRESSTEST
#if defined( COPY_CHECK_STRESSTEST )
class IGameSystem;
IGameSystem* GetPredictionCopyTester( void );
#endif

#else
class CBaseEntity;
typedef CHandle<CBaseEntity> EHANDLE;
#endif

typedef void ( *FN_FIELD_COMPARE )( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, bool noterrorchecked, bool differs, bool withintolerance, const char *value );

// Each datamap_t is broken down into two flattened arrays of fields, 
//  one for PC_NETWORKED_DATA and one for PC_NON_NETWORKED_ONLY (optimized_datamap_t::datamapinfo_t::flattenedoffsets_t)
// Each flattened array is sorted by offset for better cache performance
// Finally, contiguous "runs" off offsets are precomputed (optimized_datamap_t::datamapinfo_t::datacopyruns_t) for fast copy operations

// A data run is a set of DEFINE_PRED_FIELD fields in a c++ object which are contiguous and can be processing
//  using a single memcpy operation
struct datarun_t
{
	datarun_t() : m_nStartFlatField( 0 ), m_nEndFlatField( 0 ), m_nLength( 0 )
	{
		for ( int i = 0 ; i < TD_OFFSET_COUNT; ++i )
		{
			m_nStartOffset[ i ] = 0;
#ifdef _X360
			// These are the offsets of the next run, for priming the L1 cache
			m_nPrefetchOffset[ i ] = 0;
#endif
		}
	}

	// Indices of start/end fields in the flattened typedescription_t list
	int m_nStartFlatField;
	int m_nEndFlatField;

	// Offsets for run in the packed/unpacked data (I think the run starts need to be properly aligned)
	int m_nStartOffset[ TD_OFFSET_COUNT ];
#ifdef _X360
	// These are the offsets of the next run, for priming the L1 cache
	int m_nPrefetchOffset[ TD_OFFSET_COUNT ];
#endif
	int m_nLength;
};

struct datacopyruns_t 
{
public:
	CUtlVector< datarun_t > m_vecRuns;
};

struct flattenedoffsets_t
{
	CUtlVector< typedescription_t >	m_Flattened;
	int								m_nPackedSize; // Contiguous memory to pack all of these together for TD_OFFSET_PACKED
	int								m_nPackedStartOffset;
};

struct datamapinfo_t
{
	// Flattened list, with FIELD_EMBEDDED, FTYPEDESC_PRIVATE, 
	//  and FTYPEDESC_OVERRIDE (overridden) fields removed
	flattenedoffsets_t	m_Flat;
	datacopyruns_t		m_CopyRuns;
};

struct optimized_datamap_t
{
	// Optimized info for PC_NON_NETWORKED and PC_NETWORKED data
	datamapinfo_t	m_Info[ PC_COPYTYPE_COUNT ];
};

class CPredictionCopy
{
public:
	typedef enum
	{
		DIFFERS = 0,
		IDENTICAL,
		WITHINTOLERANCE,
	} difftype_t;

	typedef enum
	{
		TRANSFERDATA_COPYONLY = 0,  // Data copying only (uses runs)
		TRANSFERDATA_ERRORCHECK_NOSPEW, // Checks for errors, returns after first error found
		TRANSFERDATA_ERRORCHECK_SPEW,   // checks for errors, reports all errors to console
		TRANSFERDATA_ERRORCHECK_DESCRIBE, // used by hud_pdump, dumps values, etc, for all fields
	} optype_t;

	CPredictionCopy( int type, byte *dest, bool dest_packed, const byte *src, bool src_packed,
		optype_t opType, FN_FIELD_COMPARE func = NULL );
	
	int		TransferData( const char *operation, int entindex, datamap_t *dmap );

	static bool PrepareDataMap( datamap_t *dmap );
	static const typedescription_t *FindFlatFieldByName( const char *fieldname, const datamap_t *dmap );
private:

	// Operations:
	void	TransferDataCopyOnly( const datamap_t *dmap );
	void	TransferDataErrorCheckNoSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataErrorCheckSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataDescribe( char const *pchOperation, const datamap_t *dmap );


	// Report function
	void	ReportFieldsDiffer( const datamap_t *pCurrentMap, const typedescription_t *pField, const char *fmt, ... );
	void	OutputFieldDescription( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t dt, const char *fmt, ... );
	
	// Helper for TransferDataCopyOnly
	void	CopyFlatFieldsUsingRuns( const datamap_t *pCurrentMap, int nPredictionCopyType );
	void	CopyFlatFields( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	FORCEINLINE void CopyField( difftype_t difftype, T *outvalue, const T *invalue, int count );

	// Helper for TransferDataErrorCheckNoSpew
	void	ErrorCheckFlatFields_NoSpew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	FORCEINLINE void ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataErrorCheckSpew
	void	ErrorCheckFlatFields_Spew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	FORCEINLINE void ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataDescribe
	void	DescribeFields( const CUtlVector< const datamap_t * > &vecGroups, const datamap_t *pCurrentMap, int nPredictionCopyType );
	// Main entry point
	template< class T >
	FORCEINLINE void ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helpers for entity field watcher
	void	DetermineWatchField( const char *operation, int entindex, const datamap_t *dmap );
	void	WatchMsg( const typedescription_t *pField, const char *fmt, ... );
	void	DumpWatchField( const typedescription_t *pField, const byte *outvalue, int count );
	template< class T >
	FORCEINLINE void WatchField( const typedescription_t *pField, const T *outvalue, int count );

	// Helper for ErrorCheck ops
	template< class T >
	FORCEINLINE difftype_t CompareField( const typedescription_t *pField, const T *outvalue, const T *invalue, int count );
	// Used by TRANSFERDATA_ERRORCHECK_SPEW and by TRANSFERDATA_ERRORCHECK_DESCRIBE
	template< class T >
	FORCEINLINE void DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const T *outvalue, const T *invalue, int count );

private:

	optype_t		m_OpType;
	int				m_nType;
	byte			*m_pDest;
	const byte		*m_pSrc;
	int				m_nDestOffsetIndex;
	int				m_nSrcOffsetIndex;
	int				m_nErrorCount;
	int				m_nEntIndex;

	FN_FIELD_COMPARE	m_FieldCompareFunc;

	const typedescription_t	 *m_pWatchField;
	char const			*m_pOperation;

	CUtlStack< const typedescription_t * > m_FieldStack;
};

typedef void (*FN_FIELD_DESCRIPTION)( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, const char *value );

// 
// Compare methods
// 
// Specializations
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const float *outvalue, const float *invalue, int count );
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Vector *outvalue, const Vector *invalue, int count );
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Quaternion *outvalue, const Quaternion *invalue, int count );
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const char *outvalue, const char *invalue, int count );
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> FORCEINLINE CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const color32 *outvalue, const color32 *invalue, int count );

//
// Describe Methods
//

// Specializations
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const short *outvalue, const short *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const int *outvalue, const int *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const bool *outvalue, const bool *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const float *outvalue, const float *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const char *outvalue, const char *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Vector* outValue, const Vector *inValue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Quaternion* outValue, const Quaternion *inValue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const color32 *outvalue, const color32 *invalue, int count );
template<> FORCEINLINE void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const uint8 *outvalue, const uint8 *invalue, int count );


//
// Watch Methods
//
// Specializations
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const short *outvalue,int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const int *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const bool *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const float *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const Vector *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const Quaternion *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const EHANDLE *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const char *outvalue, int count );
template<> FORCEINLINE void CPredictionCopy::WatchField( const typedescription_t *pField, const color32 *outvalue, int count );
//
// Copy Methods
//
// specializations
template<> FORCEINLINE void CPredictionCopy::CopyField( difftype_t difftype, char *outvalue, const char *invalue, int count );

template< class T >
FORCEINLINE void CPredictionCopy::ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	if ( difftype == DIFFERS )
	{
		++m_nErrorCount;
	}
}


template< class T >
FORCEINLINE void CPredictionCopy::ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}


template< class T >
FORCEINLINE void CPredictionCopy::ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}

#if defined( CLIENT_DLL )
class CValueChangeTracker
{
public:
	CValueChangeTracker();

	void Reset();

	void StartTrack( char const *pchContext );
	void EndTrack();

	bool IsActive() const;

	void SetupTracking( C_BaseEntity *ent, char const *pchFieldName );
	void ClearTracking();

	void Spew();

	C_BaseEntity *GetEntity();

private:

	enum
	{
		eChangeTrackerBufSize = 128,
	};

	// Returns field size
	void				GetValue( char *buf, size_t bufsize );

	bool				m_bActive : 1;
	bool				m_bTracking : 1;
	EHANDLE				m_hEntityToTrack;
	const typedescription_t	*m_pTrackField;
	CUtlString			m_strFieldName;
	CUtlString			m_strContext;
	// First 128 bytes of data is all we will consider
	char				m_OrigValueBuf[ eChangeTrackerBufSize ];
	CUtlVector< CUtlString >	 m_History;
};

extern CValueChangeTracker *g_pChangeTracker;

class CValueChangeTrackerScope
{
public:
	CValueChangeTrackerScope( char const *pchContext )
	{
		m_bCallEndTrack = true;
		g_pChangeTracker->StartTrack( pchContext );
	}

	// Only calls Start/End if passed in entity matches entity to track
	CValueChangeTrackerScope( C_BaseEntity *pEntity, char const *pchContext )
	{
		m_bCallEndTrack = g_pChangeTracker->GetEntity() == pEntity;
		if ( m_bCallEndTrack )
		{
			g_pChangeTracker->StartTrack( pchContext );
		}
	}

	~CValueChangeTrackerScope()
	{
		if ( m_bCallEndTrack )
		{
			g_pChangeTracker->EndTrack();
		}
	}
private:

	bool		m_bCallEndTrack;
};

#if defined( _DEBUG )
#define PREDICTION_TRACKVALUECHANGESCOPE( context )		CValueChangeTrackerScope scope( context );
#define PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( entity, context ) CValueChangeTrackerScope scope( entity, context );
#define PREDICTION_STARTTRACKVALUE( context )			g_pChangeTracker->StartTrack( context );
#define PREDICTION_ENDTRACKVALUE()						g_pChangeTracker->EndTrack();
#define PREDICTION_SPEWVALUECHANGES()					g_pChangeTracker->Spew();
#else
#define PREDICTION_TRACKVALUECHANGESCOPE( context )
#define PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( entity, context )
#define PREDICTION_STARTTRACKVALUE( context )
#define PREDICTION_ENDTRACKVALUE()	
#define PREDICTION_SPEWVALUECHANGES() 
#endif

#endif // !CLIENT_DLL
#endif // PREDICTIONCOPY_H
