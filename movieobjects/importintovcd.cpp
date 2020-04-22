//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/importintovcd.h"
#include "movieobjects/movieobjects.h"
#include "tier3/scenetokenprocessor.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoevent.h"
#include "tier2/p4helpers.h"
#include "tier1/utlbuffer.h"
#include "tier3/tier3.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "studio.h"


//-----------------------------------------------------------------------------
// Helper wrapper class for log layers (necessary to avoid movieobjects dependence)
//-----------------------------------------------------------------------------
class CDmeLogLayerHelper
{
public:
	CDmeLogLayerHelper( CDmElement *pLogLayer, int nDefaultCurveType );

	// Finds a key
	int FindKey( int nTime ) const;

	// Gets a value at a particular time
	float GetValue( int nTime ) const;

	// Inserts keys
	void AddToTail( int nTime, float flValue, int nCurveType );
	void InsertAfter( int nAfter, int nTime, float flValue, int nCurveType );
	int InsertKey( int nTime, float flValue, int nCurveType );

	// Simplifies the curve
	void Simplify( float flThreshhold );

	void SetCurveType( int nKey, int nCurveType );

	// Total simplified points
	static int TotalRemovedPoints();

private:
	void CurveSimplify_R( float flThreshold, int nStartPoint, int nEndPoint, CDmeLogLayerHelper *pDest );

	// Computes the total error
	float ComputeTotalError( CDmeLogLayerHelper *pDest, int nStartPoint, int nEndPoint );

	// Select the best fit curve type
	void ChooseBestCurveType( int nKey, int nStartPoint, int nEndPoint, CDmeLogLayerHelper *pDest );

	// Compute first + second derivatives of data
	void ComputeDerivates( float *pSlope, float *pAccel, int nPoint, CDmeLogLayerHelper *pDest );

	CDmElement *m_pLogLayer;
	CDmrArray<int> m_times;
	CDmrArray<float> m_values;
	CDmrArray<int> m_curvetypes;
	int m_nDefaultCurveType;

	static int s_nTotalRemovedPoints;
};


//-----------------------------------------------------------------------------
// Total simplified points
//-----------------------------------------------------------------------------
int CDmeLogLayerHelper::s_nTotalRemovedPoints = 0;
int CDmeLogLayerHelper::TotalRemovedPoints()
{
	return s_nTotalRemovedPoints;
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmeLogLayerHelper::CDmeLogLayerHelper( CDmElement *pLogLayer, int nDefaultCurveType )	:
	m_pLogLayer( pLogLayer ), m_times( pLogLayer, "times", true ), 
	m_values( pLogLayer, "values", true ), m_curvetypes( pLogLayer, "curvetypes", true )
{
	m_nDefaultCurveType = nDefaultCurveType;
}


//-----------------------------------------------------------------------------
// Inserts keys
//-----------------------------------------------------------------------------
void CDmeLogLayerHelper::AddToTail( int nTime, float flValue, int nCurveType )
{
	m_times.AddToTail( nTime );
	m_values.AddToTail( flValue );
	m_curvetypes.AddToTail( nCurveType );
}

void CDmeLogLayerHelper::InsertAfter( int nAfter, int nTime, float flValue, int nCurveType )
{
	int nBefore = nAfter + 1;
	m_times.InsertBefore( nBefore, nTime );
	m_values.InsertBefore( nBefore, flValue );
	m_curvetypes.InsertBefore( nBefore, nCurveType );
}

int CDmeLogLayerHelper::InsertKey( int nTime, float flValue, int nCurveType )
{
	int nAfter = FindKey( nTime );
	InsertAfter( nAfter, nTime, flValue, nCurveType );
	return nAfter + 1;
}
	
void CDmeLogLayerHelper::SetCurveType( int nKey, int nCurveType )
{
	m_curvetypes.Set( nKey, nCurveType );
}


//-----------------------------------------------------------------------------
// Finds a key
//-----------------------------------------------------------------------------
int CDmeLogLayerHelper::FindKey( int nTime ) const
{
	int tn = m_times.Count();
	for ( int ti = tn - 1; ti >= 0; --ti )
	{
		if ( nTime >= m_times[ ti ] )
			return ti;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Gets a value at a particular time
//-----------------------------------------------------------------------------
float CDmeLogLayerHelper::GetValue( int nTime ) const
{
	int tc = m_times.Count();

	Assert( m_values.Count() == tc );

	int ti = FindKey( nTime );
	if ( ti < 0 )
	{
		if ( tc > 0 )
			return m_values[ 0 ];
		return 0.0f;
	}

	// Early out if we're at the end
	if ( ti >= tc - 1 )
		return m_values[ ti ];

	// Figure out the lerp factor
	int nDummy, nInterpolationType;
	int nCurveType = m_curvetypes.Count() ? m_curvetypes[ti] : m_nDefaultCurveType;
	Interpolator_CurveInterpolatorsForType( nCurveType, nInterpolationType, nDummy );

	Vector vecOutput;
	Vector vecArg1( 0.0f, m_values[ti], 0.0f );
	Vector vecArg2( 1.0f, m_values[ti+1], 0.0f );
	float t = (float)( nTime - m_times[ti] ) / (float)( m_times[ti+1] - m_times[ti] );
	Interpolator_CurveInterpolate( nInterpolationType, vecArg1, vecArg1, vecArg2, vecArg2, t, vecOutput );
	return vecOutput.y;
}


//-----------------------------------------------------------------------------
// Computes the total error
//-----------------------------------------------------------------------------
float CDmeLogLayerHelper::ComputeTotalError( CDmeLogLayerHelper *pDest, int nStartPoint, int nEndPoint )
{
	float flTotalDistance = 0.0f;

	for ( int i = nStartPoint; i <= nEndPoint; ++i )
	{
		float flCheck = m_values[i];
		float flCheck2 = pDest->GetValue( m_times[i] );
		float flDistance = fabs( flCheck2 - flCheck );
		flTotalDistance += flDistance;
	}

	return flTotalDistance;
}


//-----------------------------------------------------------------------------
// Select the best fit curve type
//-----------------------------------------------------------------------------
static int s_nInterpTypes[] = 
{
	INTERPOLATE_LINEAR_INTERP,
	INTERPOLATE_EASE_INOUT,
//	INTERPOLATE_EASE_IN,
//	INTERPOLATE_EASE_OUT,
//	INTERPOLATE_EXPONENTIAL_DECAY,
//	INTERPOLATE_HOLD,
	-1,
};

void CDmeLogLayerHelper::ChooseBestCurveType( int nKey, int nStartPoint, int nEndPoint, CDmeLogLayerHelper *pDest )
{
	return;

	float flMinError = FLT_MAX;
	int nBestInterpType = -1;
	for ( int i = 0; s_nInterpTypes[i] >= 0; ++i )
	{
		pDest->SetCurveType( nKey, MAKE_CURVE_TYPE( s_nInterpTypes[i], s_nInterpTypes[i] ) );
		float flError = ComputeTotalError( pDest, nStartPoint, nEndPoint );
		if ( flMinError > flError )
		{
			nBestInterpType = s_nInterpTypes[i];
			flMinError = flError;
		}
	}
	Assert( nBestInterpType >= 0 );
	pDest->SetCurveType( nKey, MAKE_CURVE_TYPE( nBestInterpType, nBestInterpType ) );
}


//-----------------------------------------------------------------------------
// Compute first + second derivatives of data
//-----------------------------------------------------------------------------
void CDmeLogLayerHelper::ComputeDerivates( float *pSlope, float *pAccel, int nPoint, CDmeLogLayerHelper *pDest )
{
	// Central difference, assume linear slope between points.
	// Find neighboring point with minimum distance
	bool bLeftEdge = ( nPoint == 0 );
	bool bRightEdge = ( nPoint == m_times.Count() - 1 );

	int nTime = m_times[nPoint];
	int nPrevTime = ( !bLeftEdge ) ? m_times[ nPoint - 1 ] : nTime - 1000;
	int nNextTime = ( !bRightEdge ) ? m_times[ nPoint + 1 ] : nTime + 1000;
	float flPrevPoint, flNextPoint;
	if ( nTime - nPrevTime < nNextTime - nTime )
	{
		// prev point is closer
		flPrevPoint = ( !bLeftEdge ) ? m_values[ nPoint - 1 ] : m_values[ nPoint ];
		nNextTime = nTime + ( nTime - nPrevTime );
		flNextPoint = GetValue( nNextTime );
	}
	else
	{
		// next point is closer
		flNextPoint = ( !bRightEdge ) ? m_values[ nPoint + 1 ] : m_values[ nPoint ];
		nPrevTime = nTime - ( nNextTime - nTime );
		flPrevPoint = GetValue( nPrevTime );
	}

	// Central difference: slope = ( vnext - vprev ) / ( tnext - tprev );
	// accel = ( vnext - 2 * vcurr + vprev ) / ( 0.5 * ( tnext - tprev ) )^2
	float flCurrPoint = m_values[nPoint];
	flPrevPoint -= pDest->GetValue( nPrevTime );
	flCurrPoint -= pDest->GetValue( nTime );
	flNextPoint -= pDest->GetValue( nNextTime );

	float flDeltaTime = DMETIME_TO_SECONDS( nTime - nPrevTime );
	*pSlope = ( flNextPoint - flPrevPoint ) / ( 2.0f * flDeltaTime );
	*pAccel = ( flNextPoint - 2 * flCurrPoint + flPrevPoint ) / ( flDeltaTime * flDeltaTime );
}



//-----------------------------------------------------------------------------
// Implementation of Douglas-Peucker curve simplification routine 
// (hacked to only care about error against original curve (sort of 1D)
//-----------------------------------------------------------------------------
void CDmeLogLayerHelper::CurveSimplify_R( float flThreshold, int nStartPoint, int nEndPoint, CDmeLogLayerHelper *pDest )
{
	if ( nEndPoint <= nStartPoint + 1 )
		return;

	int nMaxPoint = nStartPoint;
	float flMaxDistance = 0.0f;

	for ( int i = nStartPoint + 1 ; i < nEndPoint; ++i )
	{
		float flCheck = m_values[i];
		float flCheck2 = pDest->GetValue( m_times[i] );
		float flDistance = fabs( flCheck2 - flCheck );

		if ( flDistance < flMaxDistance )
			continue;

		nMaxPoint = i;
		flMaxDistance = flDistance;
	}

	/*
	float flMaxAccel = 0.0f;
	for ( int i = nStartPoint + 1 ; i < nEndPoint; ++i )
	{
		float flSlope, flAccel;
		ComputeDerivates( &flSlope, &flAccel, i, pDest );
		flAccel = fabs( flAccel );
		if ( flAccel < flMaxAccel )
			continue;

		nMaxPoint = i;
		flMaxAccel = flAccel;
	}
	*/

	if ( flMaxDistance > flThreshold )
	{
		int nKey = pDest->InsertKey( m_times[ nMaxPoint ], m_values[ nMaxPoint ], m_nDefaultCurveType );
		Assert( nKey != 0 );
		ChooseBestCurveType( nKey-1, nStartPoint, nMaxPoint, pDest );
		ChooseBestCurveType( nKey, nMaxPoint, nEndPoint, pDest );

		CurveSimplify_R( flThreshold, nStartPoint, nMaxPoint, pDest );
		CurveSimplify_R( flThreshold, nMaxPoint, nEndPoint, pDest );
	}
}


//-----------------------------------------------------------------------------
// Simplifies the curve
//-----------------------------------------------------------------------------
void CDmeLogLayerHelper::Simplify( float flThreshhold )
{
	int nFirstKey, nLastKey;
	int nKeys = m_values.Count();
	if ( nKeys <= 1 )
		return;

	for ( nFirstKey = 1; nFirstKey < nKeys; ++nFirstKey )
	{
		// FIXME: Should we use a tolerance check here?
		if ( m_values[ nFirstKey ] != m_values[ nFirstKey - 1 ] )
			break;
	}
	--nFirstKey;

	for ( nLastKey = nKeys; --nLastKey >= 1; )
	{
		// FIXME: Should we use a tolerance check here?
		if ( m_values[ nLastKey ] != m_values[ nLastKey - 1 ] )
			break;
	}

	if ( nLastKey <= nFirstKey )
	{
		m_times.RemoveMultiple( 1, nKeys - 1 );
		m_values.RemoveMultiple( 1, nKeys - 1 );
		s_nTotalRemovedPoints += nKeys - 1;
		return;
	}

	CDmElement *pTemp = CreateElement< CDmElement >( "simplified" );
	CDmeLogLayerHelper destLayer( pTemp, m_nDefaultCurveType );

	destLayer.AddToTail( m_times[nFirstKey], m_values[nFirstKey], m_nDefaultCurveType );
	destLayer.AddToTail( m_times[nLastKey], m_values[nLastKey], m_nDefaultCurveType );

	// Recursively finds the point with the largest error from the "simplified curve" 
	// and subdivides the problem on both sides until the largest delta from the simplified
	// curve is less than the tolerance
	CurveSimplify_R( flThreshhold, nFirstKey, nLastKey, &destLayer );

	m_times.CopyArray( destLayer.m_times.Base(), destLayer.m_times.Count() );
	m_values.CopyArray( destLayer.m_values.Base(), destLayer.m_values.Count() );
	m_curvetypes.CopyArray( destLayer.m_curvetypes.Base(), destLayer.m_curvetypes.Count() );

	DestroyElement( pTemp );

	s_nTotalRemovedPoints += nKeys - m_times.Count();
}


//-----------------------------------------------------------------------------
// Finds or adds actors, channels
//-----------------------------------------------------------------------------
static CChoreoActor* FindOrAddActor( CChoreoScene *pScene, const char *pActorName, const char *pActorModel )
{
	CChoreoActor *a = pScene->FindActor( pActorName );
	if ( !a )
	{
		a = pScene->AllocActor();
		Assert( a );
		a->SetName( pActorName );
		a->SetActive( true );
		a->SetFacePoserModelName( pActorModel );
	}
	return a;
}


//-----------------------------------------------------------------------------
// Finds animation events
//-----------------------------------------------------------------------------
static CChoreoEvent* FindOrAddAnimationEvent( CChoreoScene *pScene, CChoreoActor *pActor )
{
	int nEventCount = pScene->GetNumEvents();
	for ( int i = 0; i < nEventCount; ++i )
	{
		CChoreoEvent* pEvent = pScene->GetEvent(i);
		if ( pEvent->GetActor() != pActor )
			continue;

		if ( pEvent->GetType() != CChoreoEvent::FLEXANIMATION )
			continue;

		return pEvent;
	}

	// Allocate new channel
	CChoreoChannel *pChannel = pScene->AllocChannel();
	pChannel->SetName( "imported_flex" );
	pChannel->SetActor( pActor );
	pChannel->SetActive( true );
	pActor->AddChannel( pChannel );

	// Allocate choreo event
	CChoreoEvent *pEvent = pScene->AllocEvent();
	pEvent->SetName( pActor->GetName() );
	pEvent->SetType( CChoreoEvent::FLEXANIMATION );
	pEvent->SetActor( pActor );
	pEvent->SetChannel( pChannel );
	pEvent->SetActive( true );
	pChannel->AddEvent( pEvent );
	
	return pEvent;
}


//-----------------------------------------------------------------------------
// Finds sound events
//-----------------------------------------------------------------------------
static CChoreoEvent* FindOrAddSoundEvent( CChoreoScene *pScene, CChoreoActor *pActor, const char *pEventName )
{
	int nEventCount = pScene->GetNumEvents();
	for ( int i = 0; i < nEventCount; ++i )
	{
		CChoreoEvent* pEvent = pScene->GetEvent(i);
		if ( pEvent->GetActor() != pActor )
			continue;

		if ( pEvent->GetType() != CChoreoEvent::SPEAK )
			continue;

		if ( Q_stricmp( pEvent->GetName(), pEventName ) )
			continue;

		return pEvent;
	}

	// Allocate new channel
	CChoreoChannel *pChannel = pScene->AllocChannel();
	pChannel->SetName( "imported sounds" );
	pChannel->SetActor( pActor );
	pChannel->SetActive( true );
	pActor->AddChannel( pChannel );

	// Allocate sound event
	CChoreoEvent *pEvent = pScene->AllocEvent();
	pEvent->SetName( pEventName );
	pEvent->SetType( CChoreoEvent::SPEAK );
	pEvent->SetActor( pActor );
	pEvent->SetChannel( pChannel );
	pEvent->SetActive( true );
	pChannel->AddEvent( pEvent );

	return pEvent;
}


static CFlexAnimationTrack *FindOrCreateTrack( CChoreoEvent *pEvent, const char *pFlexControllerName )
{
	CFlexAnimationTrack *pTrack = pEvent->FindTrack( pFlexControllerName );
	if ( pTrack )
	{
		pTrack->Clear();
	}
	else
	{
		pTrack = pEvent->AddTrack( pFlexControllerName );
		pTrack->SetTrackActive( true );
	}

	pTrack->SetMin( 0.0f );
	pTrack->SetMax( 1.0f );
	pTrack->SetInverted( false );

	return pTrack;
}


//-----------------------------------------------------------------------------
// Returns flex controller ranges
//-----------------------------------------------------------------------------
void GetStereoFlexControllerRange( float *pMin, float *pMax, studiohdr_t *pStudioHdr, const char *pFlexName )
{
	char pRightBuf[MAX_PATH];
	char pLeftBuf[MAX_PATH];
	Q_snprintf( pRightBuf, sizeof(pRightBuf), "right_%s", pFlexName );
	Q_snprintf( pLeftBuf, sizeof(pLeftBuf), "left_%s", pFlexName );

	for ( LocalFlexController_t i = LocalFlexController_t(0); i < pStudioHdr->numflexcontrollers; ++i )
	{
		mstudioflexcontroller_t *pFlex = pStudioHdr->pFlexcontroller( i );
		const char *pFlexControllerName = pFlex->pszName();
		if ( !Q_stricmp( pFlexControllerName, pFlexName ) )
		{
			*pMin = pFlex->min;
			*pMax = pFlex->max;
			return;
		}

		// FIXME: Probably want to get the left + right controller + find the min and max of each, but this is unnecessary.
		if ( !Q_stricmp( pFlexControllerName, pRightBuf ) )
		{
			*pMin = pFlex->min;
			*pMax = pFlex->max;
			return;
		}

	}
	*pMin = 0.0f;
	*pMax = 1.0f;
}


void GetFlexControllerRange( float *pMin, float *pMax, studiohdr_t *pStudioHdr, const char *pFlexName )
{
	for ( LocalFlexController_t i = LocalFlexController_t(0); i < pStudioHdr->numflexcontrollers; ++i )
	{
		mstudioflexcontroller_t *pFlex = pStudioHdr->pFlexcontroller( i );
		const char *pFlexControllerName = pFlex->pszName();
		if ( !Q_stricmp( pFlexControllerName, pFlexName ) )
		{
			*pMin = pFlex->min;
			*pMax = pFlex->max;
			return;
		}
	}
	*pMin = 0.0f;
	*pMax = 1.0f;
}


//-----------------------------------------------------------------------------
// Imports samples into a track
//-----------------------------------------------------------------------------
void ImportSamplesIntoTrack( CFlexAnimationTrack *pTrack, CDmElement *pLog, int nSampleType, int nTimeOffset, const ImportVCDInfo_t& info )
{
	CDmrArray<int> times( pLog, "times" );
	CDmrArray<float> values( pLog, "values" );

	// Add the samples
	int nSampleCount = times.Count();
	if ( nSampleCount == 0 )
		return;

	int nDefaultCurveType = MAKE_CURVE_TYPE( info.m_nInterpolationType, info.m_nInterpolationType );
	if ( info.m_flSimplificationThreshhold > 0.0f )
	{
		CDmeLogLayerHelper helper( pLog, nDefaultCurveType );
		helper.Simplify( info.m_flSimplificationThreshhold );
	}

	CDmrArray<int> curveTypes( pLog, "curvetypes" );

	nSampleCount = times.Count();
	bool bHasCurveTypeData = ( curveTypes.Count() > 0 );
	for ( int j = 0; j < nSampleCount; ++j )
	{
		int nCurveType = bHasCurveTypeData ? curveTypes[j] : nDefaultCurveType;
		float flValue = values[j];
		float flTime = DMETIME_TO_SECONDS( times[j] - nTimeOffset );

		CExpressionSample *pSample = pTrack->AddSample( flTime, flValue, nSampleType );
		pSample->SetCurveType( nCurveType );
	}

	if ( nSampleType == 0 )
	{
		pTrack->SetEdgeActive( true, true ); 
		pTrack->SetEdgeActive( false, true ); 

		int nCurveType0, nCurveType1;
		if ( bHasCurveTypeData )
		{
			nCurveType0 = curveTypes[0];
			nCurveType1 = curveTypes[nSampleCount-1];
		}
		else
		{
			nCurveType0 = nCurveType1 = nDefaultCurveType;
		}
		pTrack->SetEdgeInfo( true, nCurveType0, values[ 0 ] );
		pTrack->SetEdgeInfo( false, nCurveType1, values[ nSampleCount-1 ] );
	}
}


//-----------------------------------------------------------------------------
// Imports mono log data into a event, creates a new track if necessary
//-----------------------------------------------------------------------------
void ImportMonoLogDataIntoEvent( studiohdr_t *pStudioHdr, CChoreoEvent *pEvent, const char *pTrackName, CDmElement *pLog, int nTimeOffset, const ImportVCDInfo_t& info )
{
	CDmrArray<int> times( pLog, "times" );
	if ( times.Count() == 0 )
		return;

	float flMin, flMax;
	GetFlexControllerRange( &flMin, &flMax, pStudioHdr, pTrackName );

	CFlexAnimationTrack *pTrack = FindOrCreateTrack( pEvent, pTrackName );
	pTrack->Clear();
	pTrack->SetComboType( false );
	pTrack->SetMin( flMin );
	pTrack->SetMax( flMax );
	ImportSamplesIntoTrack( pTrack, pLog, 0, nTimeOffset, info );
}


//-----------------------------------------------------------------------------
// Imports stereo log data into a event, creates a new track if necessary
//-----------------------------------------------------------------------------
void ImportStereoLogDataIntoEvent( studiohdr_t *pStudioHdr, CChoreoEvent *pEvent, const char *pTrackName, CDmElement *pValueLog, CDmElement *pBalanceLog, int nTimeOffset, const ImportVCDInfo_t& info )
{
	CDmrArray<int> valueTimes( pValueLog, "times" );
	CDmrArray<int> balanceTimes( pBalanceLog, "times" );
	if ( valueTimes.Count() == 0 && balanceTimes.Count() == 0 )
		return;

	float flMin, flMax;
	GetStereoFlexControllerRange( &flMin, &flMax, pStudioHdr, pTrackName );

	CFlexAnimationTrack *pTrack = FindOrCreateTrack( pEvent, pTrackName );
	pTrack->Clear();
	pTrack->SetComboType( true );
	pTrack->SetMin( flMin );
	pTrack->SetMax( flMax );
	ImportSamplesIntoTrack( pTrack, pValueLog, 0, nTimeOffset, info );
	ImportSamplesIntoTrack( pTrack, pBalanceLog, 1, nTimeOffset, info );
}


//-----------------------------------------------------------------------------
// Compute track start, end time
//-----------------------------------------------------------------------------
static int ComputeEventTime( CDmElement *pRoot, CChoreoEvent *pEvent )
{
	int nStartTime = INT_MAX;
	int nEndTime = INT_MIN;

	// Iterate over all elements in the animations attribute; each one refers to a log.
	CDmrElementArray<> animations( pRoot, "animations" );
	if ( !animations.IsValid() )
		return 0;

	int nCount = animations.Count();
	for( int i = 0; i < nCount; ++i )
	{
		CDmElement *pLog = animations[i];
		if ( !pLog )
			continue;

		CDmrArray<int> times( pLog, "times" );
		int nSampleCount = times.Count();
		if ( nSampleCount == 0 )
			continue;

		if ( nStartTime > times[0] )
		{
			nStartTime = times[0];
		}
		if ( nEndTime < times[nSampleCount-1] )
		{
			nEndTime = times[nSampleCount-1];
		}
	}

	pEvent->SetStartTime( DMETIME_TO_SECONDS( nStartTime ) );
	pEvent->SetEndTime( DMETIME_TO_SECONDS( nEndTime ) );
	return nStartTime;
}


//-----------------------------------------------------------------------------
// Main entry point for importing animations
//-----------------------------------------------------------------------------
void ImportAnimations( CDmElement *pRoot, CChoreoScene *pChoreoScene, CChoreoActor *pActor, studiohdr_t *pStudioHdr, const ImportVCDInfo_t& info )
{
	CChoreoEvent *pEvent = FindOrAddAnimationEvent( pChoreoScene, pActor );
	pEvent->SetDefaultCurveType( MAKE_CURVE_TYPE( info.m_nInterpolationType, info.m_nInterpolationType ) );
	int nTimeOffset = ComputeEventTime( pRoot, pEvent );

	// Iterate over all elements in the animations attribute; each one refers to a log.
	CDmrElementArray<> animations( pRoot, "animations" );
	if ( !animations.IsValid() )
		return;
	int nCount = animations.Count();
	for( int i = 0; i < nCount; ++i )
	{
		CDmElement *pLog = animations[i];
		if ( !pLog )
			continue;

		const char *pLogName = pLog->GetName();

		// Balance is done at the same time as value
		if ( StringHasPrefix( pLogName, "balance_" ) )
			continue;

		if ( StringHasPrefix( pLogName, "value_" ) )
		{
			if ( i == nCount - 1 )
				continue;

			char pBalanceName[256];
			Q_snprintf( pBalanceName, sizeof(pBalanceName), "balance_%s", pLogName + 6 );
			CDmElement *pBalanceLog = animations[i+1];
			if ( !Q_stricmp( pBalanceName, pBalanceLog->GetName() ) )
			{
				++i;
			}
			else
			{
				pBalanceLog = NULL;
			}
			if ( pBalanceLog )
			{
				ImportStereoLogDataIntoEvent( pStudioHdr, pEvent, pLogName + 6, pLog, pBalanceLog, nTimeOffset, info );
			}
		}
		else
		{
			ImportMonoLogDataIntoEvent( pStudioHdr, pEvent, pLogName, pLog, nTimeOffset, info );
		}
	}
}


//-----------------------------------------------------------------------------
// Main entry point for importing sounds
//-----------------------------------------------------------------------------
void ImportSounds( CDmElement *pRoot, CChoreoScene *pChoreoScene, CChoreoActor *pActor, const ImportVCDInfo_t& info )
{
	// Iterate over all element in the sound attribute; each one refers to a sound
	CDmrElementArray<> sounds( pRoot, "sounds" );
	if ( !sounds.IsValid() )
		return;
	int nCount = sounds.Count();
	for( int i = 0; i < nCount; ++i )
	{
		CDmElement *pSound = sounds[i];
		if ( !pSound )
			continue;

		const char *pEventName = pSound->GetName();
		CChoreoEvent *pEvent = FindOrAddSoundEvent( pChoreoScene, pActor, pEventName );

		int nStart = pSound->GetValue<int>( "start" );
		int nEnd = pSound->GetValue<int>( "end" );
		const char *pGameSound = pSound->GetValueString( "gamesound" );
		pEvent->SetStartTime( DMETIME_TO_SECONDS( nStart ) );
		pEvent->SetEndTime( DMETIME_TO_SECONDS( nEnd ) );
		pEvent->SetParameters( pGameSound );
		pEvent->SetCloseCaptionType( CChoreoEvent::CC_MASTER );
	}
}


//-----------------------------------------------------------------------------
// Main entry point for importing a .fac file into a .vcd file
//-----------------------------------------------------------------------------
bool ImportLogsIntoVCD( const char *pFacFullPath, CChoreoScene *pChoreoScene, const ImportVCDInfo_t& info )
{
	CDmElement *pRoot;
	DmFileId_t id = g_pDataModel->RestoreFromFile( pFacFullPath, NULL, NULL, &pRoot, CR_FORCE_COPY );
	if ( id == DMFILEID_INVALID )
	{
		Warning( "Unable to load file %s\n", pFacFullPath );
		return false;
	}

	pChoreoScene->IgnorePhonemes( info.m_bIgnorePhonemes );

	// Create the actor in the scene
	const char *pActorName = pRoot->GetName();
	const char *pActorModel = pRoot->GetValueString( "gamemodel" );

	MDLHandle_t hMDL = g_pMDLCache->FindMDL( pActorModel );
	if ( hMDL == MDLHANDLE_INVALID )
	{
		Warning( "vcdimport: Model %s doesn't exist!\n", pActorModel );
		return false;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
	if ( !pStudioHdr || g_pMDLCache->IsErrorModel( hMDL ) )
	{
		Warning( "vcdimport: Model %s doesn't exist!\n", pActorModel );
		return false;
	}

	CChoreoActor *pActor = FindOrAddActor( pChoreoScene, pActorName, pActorModel );

	ImportAnimations( pRoot, pChoreoScene, pActor, pStudioHdr, info );
	ImportSounds( pRoot, pChoreoScene, pActor, info );

	DestroyElement( pRoot, TD_DEEP );
	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for importing a .fac file into a .vcd file
//-----------------------------------------------------------------------------
bool ImportLogsIntoVCD( const char *pFacFullPath, const char *pVCDInFullPath, const char *pVCDOutPath, const ImportVCDInfo_t& info )
{
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pVCDInFullPath, NULL, buf ) )
	{
		Warning( "Unable to load file %s\n", pVCDInFullPath );
		return false;
	}

	SetTokenProcessorBuffer( (char *)buf.Base() );
	CChoreoScene *pScene = ChoreoLoadScene( pVCDInFullPath, NULL, GetTokenProcessor(), NULL );
	if ( !pScene )
	{
		Warning( "Unable to parse file %s\n", pVCDInFullPath );
		return false;
	}

	bool bOk = ImportLogsIntoVCD( pFacFullPath, pScene, info );
	if ( !bOk )
		return false;

	Msg( "Removed %d samples\n", CDmeLogLayerHelper::TotalRemovedPoints() );

	char pTemp[MAX_PATH];
	if ( !Q_IsAbsolutePath( pVCDOutPath ) )
	{
		g_pFullFileSystem->RelativePathToFullPath( pVCDOutPath, NULL, pTemp, sizeof(pTemp) );
		if ( !Q_IsAbsolutePath( pTemp ) )
		{
			char pDir[MAX_PATH];
			if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
			{
				Q_ComposeFileName( pDir, pVCDOutPath, pTemp, sizeof(pTemp) );
				pVCDOutPath = pTemp;
			}
		}
		else
		{
			pVCDOutPath = pTemp;
		}
	}

	CP4AutoEditFile checkout( pVCDOutPath );
	return pScene->SaveToFile( pVCDOutPath );
}