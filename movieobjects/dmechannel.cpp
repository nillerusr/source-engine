//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmechannel.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmeclip.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmattribute.h"
#include "tier0/vprof.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// 
// CDmeChannelRecordingMgr
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static CDmeChannelRecordingMgr s_ChannelRecordingMgr;
CDmeChannelRecordingMgr *g_pChannelRecordingMgr = &s_ChannelRecordingMgr;


//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CDmeChannelRecordingMgr::CDmeChannelRecordingMgr()
{
	m_bActive = false;
	m_bSavedUndoState = false;
	m_bUseTimeSelection = false;
	m_nRevealType = PROCEDURAL_PRESET_NOT;
	m_pRevealTarget = NULL;
}


//-----------------------------------------------------------------------------
// Activates, deactivates layer recording.
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::StartLayerRecording( const char *pUndoRedoDesc, const DmeLog_TimeSelection_t *pTimeSelection )
{
	g_pDataModel->StartUndo( pUndoRedoDesc, pUndoRedoDesc );
	m_bSavedUndoState = g_pDataModel->IsUndoEnabled();
	g_pDataModel->SetUndoEnabled( false );

	Assert( !m_bActive );
	Assert( m_LayerChannels.Count() == 0 );
	m_LayerChannels.Purge();
	m_bActive = true;
	m_bUseTimeSelection = ( pTimeSelection != NULL );
	if ( pTimeSelection )
	{
		m_TimeSelection = *pTimeSelection;
	}
	else
	{
		// Slam to default value
		m_TimeSelection = DmeLog_TimeSelection_t();
	}
	m_TimeSelection.ResetTimeAdvancing();
}

void CDmeChannelRecordingMgr::FinishLayerRecording( float flThreshhold, bool bFlattenLayers /*=true*/ )
{
	Assert( m_bActive );

	RemoveAllChannelsFromRecordingLayer();
	m_bUseTimeSelection = false;
	m_TimeSelection.ResetTimeAdvancing();

	g_pDataModel->SetUndoEnabled( m_bSavedUndoState );
	if ( bFlattenLayers )
	{
		FlattenLayers( flThreshhold );
	}
	g_pDataModel->FinishUndo();

	m_bActive = false;
	m_LayerChannels.Purge();
	m_nRevealType = PROCEDURAL_PRESET_NOT;
	m_pRevealTarget = NULL;
	m_PasteTarget.RemoveAll();
}


//-----------------------------------------------------------------------------
// Adds a channel to the recording layer
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::AddChannelToRecordingLayer( CDmeChannel *pChannel, CDmeClip *pRoot, CDmeClip *pShot )
{
	Assert( pChannel->m_nRecordLayerIndex == -1 );

	CDmeLog *pLog = pChannel->GetLog();
	if ( !pLog )
		return;

	int nRecordLayerIndex = m_LayerChannels.AddToTail();
	LayerChannelInfo_t& info = m_LayerChannels[nRecordLayerIndex];
	info.m_Channel = pChannel;
	if ( pRoot )
	{
		if ( !pChannel->BuildClipStack( &info.m_ClipStack, pRoot, pShot ) )
		{
			m_LayerChannels.Remove( nRecordLayerIndex );
			return;
		}
	}

	pChannel->m_nRecordLayerIndex = nRecordLayerIndex;

	// This operation is undoable
	CEnableUndoScopeGuard guard;
	pLog->AddNewLayer();
	pChannel->SetMode( CM_RECORD );
}


//-----------------------------------------------------------------------------
// Removes all channels from the recording layer
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::RemoveAllChannelsFromRecordingLayer( )
{
	int c = m_LayerChannels.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeChannel *pChannel = m_LayerChannels[ i ].m_Channel.Get();
		if ( !pChannel )
			continue;

		CDmeLog *pLog = pChannel->GetLog();
		if ( pLog && IsUsingTimeSelection() )
		{
			// Computes local times for the time selection
			DmeLog_TimeSelection_t timeSelection;
			GetLocalTimeSelection( timeSelection, pChannel->m_nRecordLayerIndex );
			pLog->FinishTimeSelection( pChannel->GetCurrentTime(), timeSelection );
		}
		pChannel->m_nRecordLayerIndex = -1;
		pChannel->SetMode( CM_PLAY );
	}
}


//-----------------------------------------------------------------------------
// Flattens recorded layers into the base layer
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::FlattenLayers( float flThreshhold )
{
	int nFlags = 0;
	if ( IsUsingDetachedTimeSelection() && IsTimeAdvancing() )
	{
		nFlags |= CDmeLog::FLATTEN_NODISCONTINUITY_FIXUP;
	}

	int c = m_LayerChannels.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeChannel *pChannel = m_LayerChannels[ i ].m_Channel.Get();
		if ( !pChannel )
			continue;

		CDmeLog *pLog = pChannel->GetLog();
		Assert( pLog );
		if ( !pLog )
			continue;

		pLog->FlattenLayers( flThreshhold, nFlags );
	}
}


//-----------------------------------------------------------------------------
// Used to iterate over all channels currently being recorded
//-----------------------------------------------------------------------------
int CDmeChannelRecordingMgr::GetLayerRecordingChannelCount()
{
	return m_LayerChannels.Count();
}

CDmeChannel* CDmeChannelRecordingMgr::GetLayerRecordingChannel( int nIndex )
{
	return m_LayerChannels[nIndex].m_Channel.Get();
}


//-----------------------------------------------------------------------------
// Computes time selection info in log time for a particular recorded channel
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::GetLocalTimeSelection( DmeLog_TimeSelection_t& selection, int nIndex )
{
	Assert( m_bUseTimeSelection );
	LayerChannelInfo_t& info = m_LayerChannels[nIndex];
	selection = m_TimeSelection;
	for ( int i = 0; i < TS_TIME_COUNT; ++i )
	{
		selection.m_nTimes[i] = CDmeClip::ToChildMediaTime( info.m_ClipStack, selection.m_nTimes[i], false );
	}
	selection.m_pPresetValue = info.m_pPresetValue;
}


//-----------------------------------------------------------------------------
// Methods which control various aspects of recording
//-----------------------------------------------------------------------------
void CDmeChannelRecordingMgr::UpdateTimeAdvancing( bool bPaused, DmeTime_t tCurTime )
{
	Assert( m_bActive && m_bUseTimeSelection );
	if ( !bPaused && !m_TimeSelection.IsTimeAdvancing() )
	{
		m_TimeSelection.StartTimeAdvancing();

		// blow away logs after curtime
		int nCount = m_LayerChannels.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			LayerChannelInfo_t& info = m_LayerChannels[i];
			DmeTime_t t = CDmeClip::ToChildMediaTime( info.m_ClipStack, tCurTime, false );
			info.m_Channel->GetLog()->RemoveKeys( t, DMETIME_MAXTIME );
		}
	}
}

void CDmeChannelRecordingMgr::UpdateRecordingTimeSelectionTimes( const DmeLog_TimeSelection_t& timeSelection )
{
	Assert( m_bActive );
	for ( int i = 0; i < TS_TIME_COUNT; ++i )
	{
		m_TimeSelection.m_nTimes[i] = timeSelection.m_nTimes[i];
	}
	m_TimeSelection.m_nResampleInterval = timeSelection.m_nResampleInterval;
}

void CDmeChannelRecordingMgr::SetIntensityOnAllLayers( float flIntensity )
{
	m_TimeSelection.m_flIntensity = flIntensity;
}

void CDmeChannelRecordingMgr::SetRecordingMode( RecordingMode_t mode )
{
	m_TimeSelection.SetRecordingMode( mode );
}

void CDmeChannelRecordingMgr::SetPresetValue( CDmeChannel* pChannel, CDmAttribute *pPresetValue )
{
	Assert( pChannel->m_nRecordLayerIndex != -1 );
	m_LayerChannels[ pChannel->m_nRecordLayerIndex ].m_pPresetValue = pPresetValue;
}


//-----------------------------------------------------------------------------
// Methods to query aspects of recording
//-----------------------------------------------------------------------------
bool CDmeChannelRecordingMgr::IsUsingDetachedTimeSelection() const
{
	Assert( m_bActive );
	return !m_TimeSelection.m_bAttachedMode;
}

bool CDmeChannelRecordingMgr::IsTimeAdvancing() const
{
	Assert( m_bActive );
	return m_TimeSelection.IsTimeAdvancing();
}

bool CDmeChannelRecordingMgr::IsUsingTimeSelection() const
{
	return m_bUseTimeSelection;
}

bool CDmeChannelRecordingMgr::ShouldRecordUsingTimeSelection() const
{
	return m_bUseTimeSelection && m_bActive;
}

void CDmeChannelRecordingMgr::SetProceduralTarget( int nProceduralMode, const CDmAttribute *pTarget )
{
	m_nRevealType = nProceduralMode;
	m_pRevealTarget = pTarget;
	m_PasteTarget.RemoveAll();
}

void CDmeChannelRecordingMgr::SetProceduralTarget( int nProceduralMode, const CUtlVector< KeyValues * >& list )
{
	m_nRevealType = nProceduralMode;
	m_pRevealTarget = NULL;
	m_PasteTarget.RemoveAll();
	for ( int i = 0; i < list.Count(); ++i )
	{
		m_PasteTarget.AddToTail( list[ i ] );
	}
}

int CDmeChannelRecordingMgr::GetProceduralType() const
{
	return m_nRevealType;
}

const CDmAttribute *CDmeChannelRecordingMgr::GetProceduralTarget() const
{
	Assert( m_pRevealTarget );
	return m_pRevealTarget;
}

const CUtlVector< KeyValues * > &CDmeChannelRecordingMgr::GetPasteTarget() const
{
	return m_PasteTarget;
}

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeChannel, CDmeChannel );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeChannel::OnConstruction()
{
	m_nRecordLayerIndex		= -1;
	m_nNextCurveType		= CURVE_DEFAULT;
	m_tCurrentTime			= DMETIME_INVALID;
	m_tPreviousTime			= DMETIME_INVALID;
	m_timeOutsideTimeframe	= DMETIME_INVALID;

	m_fromElement	.Init( this, "fromElement",		FATTRIB_HAS_CALLBACK | FATTRIB_NEVERCOPY );
	m_fromAttribute	.Init( this, "fromAttribute",	FATTRIB_TOPOLOGICAL | FATTRIB_HAS_CALLBACK );
	m_fromIndex		.Init( this, "fromIndex",		FATTRIB_TOPOLOGICAL );
	m_toElement		.Init( this, "toElement",		FATTRIB_HAS_CALLBACK | FATTRIB_NEVERCOPY );
	m_toAttribute	.Init( this, "toAttribute",		FATTRIB_TOPOLOGICAL | FATTRIB_HAS_CALLBACK );
	m_toIndex		.Init( this, "toIndex",			FATTRIB_TOPOLOGICAL );
	m_mode			.InitAndSet( this, "mode", (int)CM_PASS );
	m_log			.Init( this, "log" );
	m_FromAttributeHandle	= DMATTRIBUTE_HANDLE_INVALID;
	m_ToAttributeHandle		= DMATTRIBUTE_HANDLE_INVALID;
}

void CDmeChannel::OnDestruction()
{
}

int CDmeChannel::GetFromArrayIndex() const
{
	return m_fromIndex;
}

int CDmeChannel::GetToArrayIndex() const
{
	return m_toIndex;
}

void CDmeChannel::Play()
{
	CDmAttribute *pToAttr = GetToAttribute();

	if ( pToAttr == NULL )
		return;

	CDmeLog *pLog = GetLog();
	if ( !pLog )
		return;

	DmeTime_t time = GetCurrentTime();

	DmeTime_t t0 = pLog->GetBeginTime();
	DmeTime_t tn = pLog->GetEndTime();

	PlayMode_t pmode = PM_HOLD;
	switch ( pmode )
	{
	case PM_HOLD:
		time = clamp( time, t0, tn );
		break;

	case PM_LOOP:
		if ( tn == t0 )
		{
			time = t0;
		}
		else
		{
			time -= t0;
			time = time % ( tn - t0 );
			time += t0;
		}
		break;
	}

	// We might not want to do it this way, but this makes empty logs not get in the way if there is a valid pFromAttr
#if 1
	if ( pLog->IsEmpty() && !pLog->HasDefaultValue() &&
		GetFromAttribute() != NULL )
	{
		Pass();
		return;
	}
#endif

	pLog->GetValue( time, pToAttr, m_toIndex.Get() );
}

void CDmeChannel::Pass()
{
	CDmAttribute *pFromAttr = GetFromAttribute();
	CDmAttribute *pToAttr = GetToAttribute();
	if ( !pFromAttr || !pToAttr )
		return;

	if ( pFromAttr == pToAttr )
		return;

	DmAttributeType_t type = pFromAttr->GetType();
	const void *pValue = NULL;
	if ( IsArrayType( type ) )
	{
		CDmrGenericArray array( pFromAttr );
		pValue = array.GetUntyped( m_fromIndex.Get() );
		type = ArrayTypeToValueType( type );
	}
	else
	{
		pValue = pFromAttr->GetValueUntyped();
	}

	if ( IsArrayType( pToAttr->GetType() ) )
	{
		CDmrGenericArray array( pToAttr );
		array.Set( m_toIndex.Get(), type, pValue );
	}
	else
	{
		pToAttr->SetValue( type, pValue );
	}
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeChannel::IsDirty()
{
	if ( BaseClass::IsDirty() )
		return true;

	switch( GetMode() )
	{
	case CM_PLAY:
		return true;

	case CM_RECORD:
		if ( m_nRecordLayerIndex != -1 )
			return true;

		// NOTE: Fall through!
	case CM_PASS:
		{
			CDmAttribute *pFromAttr = GetFromAttribute();
			if ( pFromAttr && pFromAttr->IsFlagSet( FATTRIB_OPERATOR_DIRTY ) )
				return true;
		}
		break;

	default:
		break;
	}
	return false;
}


void CDmeChannel::Operate()
{
	VPROF( "CDmeChannel::Operate" );

	switch ( GetMode() )
	{
	case CM_OFF:
		return;

	case CM_PLAY:
		Play();
		return;

	case CM_RECORD:
		Record();
		return;

	case CM_PASS:
		Pass();
		return;
	}
}

void CDmeChannel::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	ChannelMode_t mode = GetMode();
	if ( mode == CM_OFF || mode == CM_PLAY )
		return; // off and play ignore inputs

	CDmAttribute *pAttr = GetFromAttribute();
	if ( pAttr != NULL )
	{
		attrs.AddToTail( pAttr );
	}
}

void CDmeChannel::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	ChannelMode_t mode = GetMode();
	if ( mode == CM_OFF )
		return; // off ignores inputs

	if ( mode == CM_RECORD || mode == CM_PASS )
	{
		if ( GetFromAttribute() == GetToAttribute() )
			return; // record/pass from and to the same attribute doesn't write anything
	}

	CDmAttribute *pAttr = GetToAttribute();
	if ( pAttr != NULL )
	{
		attrs.AddToTail( pAttr );
	}
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
CDmElement *CDmeChannel::GetFromElement() const
{
	return m_fromElement;
}

CDmElement *CDmeChannel::GetToElement() const
{
	return m_toElement;
}

void CDmeChannel::SetLog( CDmeLog *pLog )
{
	m_log = pLog;
}

CDmeLog *CDmeChannel::CreateLog( DmAttributeType_t type )
{
	CDmeLog *log = CDmeLog::CreateLog( type, GetFileId() );
	m_log.Set( log );
	return log;
}

// HACK:  This is an evil hack since the element and attribute change sequentially, but they really need to change in lockstep or else you're looking
//  up an attribute from some other element or vice versa.


void CDmeChannel::SetInput( CDmElement* pElement, const char* pAttribute, int index )
{
	m_fromElement.Set( pElement );
	m_fromAttribute.Set( pAttribute );
	m_fromIndex.Set( index );
	SetupFromAttribute();
}

void CDmeChannel::SetOutput( CDmElement* pElement, const char* pAttribute, int index )
{
	m_toElement.Set( pElement );
	m_toAttribute.Set( pAttribute );
	m_toIndex.Set( index );
	SetupToAttribute();
}

void CDmeChannel::SetInput( CDmAttribute *pAttribute, int index )
{
	if ( pAttribute )
	{
		SetInput( pAttribute->GetOwner(), pAttribute->GetName(), index );
	}
	else
	{
		SetInput( NULL, "", index );
	}
}

void CDmeChannel::SetOutput( CDmAttribute *pAttribute, int index )
{
	if ( pAttribute )
	{
		SetOutput( pAttribute->GetOwner(), pAttribute->GetName(), index );
	}
	else
	{
		SetOutput( NULL, "", index );
	}
}


ChannelMode_t CDmeChannel::GetMode()
{
	return static_cast< ChannelMode_t >( m_mode.Get() );
}

void CDmeChannel::SetMode( ChannelMode_t mode )
{
	if ( mode != m_mode )
	{
		m_mode.Set( static_cast< int >( mode ) );
		m_tPreviousTime = DMETIME_INVALID;
	}
}

void CDmeChannel::ClearLog()
{
	GetLog()->ClearKeys();
}

CDmeLog *CDmeChannel::GetLog()
{
	if ( !m_log.GetElement() && ( m_FromAttributeHandle == DMATTRIBUTE_HANDLE_INVALID ) )
	{
		// NOTE: This will generate a new log based on the from attribute
		SetupFromAttribute();
	}
	return m_log.GetElement();
}


//-----------------------------------------------------------------------------
// Used to cache off handles to attributes
//-----------------------------------------------------------------------------
CDmAttribute *CDmeChannel::SetupFromAttribute()
{
	m_FromAttributeHandle = DMATTRIBUTE_HANDLE_INVALID;

	CDmElement *pObject = m_fromElement.GetElement();
	const char *pName = m_fromAttribute.Get();
	if ( pObject == NULL || pName == NULL || !pName[0] )
		return NULL;

	CDmAttribute *pAttr = pObject->GetAttribute( pName );
	if ( !pAttr )
		return NULL;

	m_FromAttributeHandle = pAttr->GetHandle();

	DmAttributeType_t fromType = pAttr->GetType();
	if ( IsArrayType( fromType ) )
	{
		fromType = ArrayTypeToValueType( fromType );
	}

	CDmeLog *pLog = m_log.GetElement();
	if ( pLog == NULL )
	{
		CreateLog( fromType );
		return pAttr;
	}

	DmAttributeType_t logType = pLog->GetDataType();
	if ( IsArrayType( logType ) )
	{
		logType = ArrayTypeToValueType( logType );
	}

	if ( logType != fromType )
	{
		// NOTE: This will release the current log
		CreateLog( fromType );
	}

	return pAttr;
}

CDmAttribute *CDmeChannel::SetupToAttribute()
{
	m_ToAttributeHandle = DMATTRIBUTE_HANDLE_INVALID;

	CDmElement *pObject = m_toElement.GetElement();
	const char *pName = m_toAttribute.Get();
	if ( pObject == NULL || pName == NULL || !pName[0] )
		return NULL;

	CDmAttribute *pAttr = pObject->GetAttribute( pName );
	if ( !pAttr )
		return NULL;

	m_ToAttributeHandle = pAttr->GetHandle();
	return pAttr;
}


//-----------------------------------------------------------------------------
// This function gets called whenever an attribute changes
//-----------------------------------------------------------------------------
void CDmeChannel::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( ( pAttribute == m_fromElement  .GetAttribute() ) ||
		( pAttribute == m_fromAttribute.GetAttribute() ) )
	{
		// NOTE: This will force a recache of the attribute handle
		m_FromAttributeHandle = DMATTRIBUTE_HANDLE_INVALID;
		return;
	}

	if ( ( pAttribute == m_toElement  .GetAttribute() ) ||
		( pAttribute == m_toAttribute.GetAttribute() ) )
	{
		m_ToAttributeHandle = DMATTRIBUTE_HANDLE_INVALID;
		return;
	}

	BaseClass::OnAttributeChanged( pAttribute );
}


DmeTime_t CDmeChannel::GetCurrentTime() const
{
	return m_tCurrentTime;
}

//-----------------------------------------------------------------------------
// Simple version. Only works if multiple active channels clips
// do not reference the same channels
//-----------------------------------------------------------------------------
void CDmeChannel::SetCurrentTime( DmeTime_t time )
{
	m_tPreviousTime = m_tCurrentTime;
	m_tCurrentTime = time;
	m_timeOutsideTimeframe = DMETIME_ZERO;
}

//-----------------------------------------------------------------------------
// SetCurrentTime sets the current time on the clip,
// choosing the time closest to (and after) a timeframe if multiple sets in a frame
//-----------------------------------------------------------------------------
void CDmeChannel::SetCurrentTime( DmeTime_t time, DmeTime_t start, DmeTime_t end )
{ 
	m_tPreviousTime = m_tCurrentTime;

	DmeTime_t dt( 0 );
	if ( time < start )
	{
		dt = time - start;
		time = start;
	}
	else if ( time >= end )
	{
		dt = time - end;
		time = end;
	}
	DmeTime_t totf = m_timeOutsideTimeframe;

	const DmeTime_t t0( 0 );
	if ( ( dt < t0 && totf < t0 && dt < totf ) ||	// both prior to clip, old totf closer
		( dt < t0 && totf >= t0 ) ||				// new dt prior to clip, old totf in or after
		( dt >= t0 && totf >= t0 && dt > totf ) )	// both after clip, old totf closer
		return; // if old todt is a better match, don't update channel time

	m_tCurrentTime = time;
	m_timeOutsideTimeframe = dt;
}

//-----------------------------------------------------------------------------
// ClearTimeMetric marks m_timeOutsideTimeframe invalid (-inf is the worst possible match)
//-----------------------------------------------------------------------------
void CDmeChannel::ClearTimeMetric()
{
	m_timeOutsideTimeframe = DmeTime_t::MinTime();
}

void CDmeChannel::SetChannelToPlayToSelf( const char *outputAttributeName, float defaultValue, bool force /*= false*/ )
{
	if ( !HasAttribute( outputAttributeName ) )
	{
		AddAttribute( outputAttributeName, AT_FLOAT );
	}

	CDmeTypedLog< bool > *log = static_cast< CDmeTypedLog< bool >* >( GetLog() );
	// Usually we won't put it into playback if it's empty, we'll just read the default value continously
	if ( force || ( log && !log->IsEmpty() && !log->HasDefaultValue() ) )
	{
		SetMode( CM_PLAY );
		SetOutput( this, outputAttributeName );
	}
	SetValue( outputAttributeName, defaultValue );
}

void CDmeChannel::SetNextKeyCurveType( int nCurveType )
{
	m_nNextCurveType = nCurveType;
}

CDmeLogLayer *FindLayerInSnapshot( const CDmrElementArray<CDmElement>& snapshotArray, CDmeLog *origLog )
{
	if ( !snapshotArray.IsValid() )
		return NULL;

	int c = snapshotArray.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer *layer = CastElement< CDmeLogLayer >( snapshotArray[ i ] );
		if ( !layer )
			continue;

		CDmeLog *pLog = layer->GetValueElement< CDmeLog >( "origLog" );
		if ( !pLog )
		{
			Assert( 0 );
			continue;
		}

		if ( pLog == origLog )
			return layer;
	}

	return NULL;
}

KeyValues *FindLayerInPasteData( const CUtlVector< KeyValues * > &list, CDmeLog *log )
{
	int c = list.Count();
	for ( int i = 0; i  < c; ++i )
	{
		CDisableUndoScopeGuard noundo;

		KeyValues *kv = list[ i ];
		Assert( kv );

		if ( Q_stricmp( kv->GetName(), "ControlLayers" ) )
			continue;

		LayerSelectionData_t *data = reinterpret_cast< LayerSelectionData_t * >( kv->GetPtr( "LayerData" ) );
		if ( !data )
			continue;

		CDmeChannel *ch = data->m_hChannel;
		if ( !ch )
			continue;

		CDmeLog *chLog = ch->GetLog();
		if ( chLog == log )
			return kv;
	}

	return NULL;
}

static int FindSpanningLayerAndSetIntensity( DmeLog_TimeSelection_t &ts, LayerSelectionData_t *data )
{
	Assert( data->m_vecData.Count() >= 2 );

	float frac = ts.m_flIntensity;
	int i = 0;
	for ( ; i < data->m_vecData.Count() - 1; ++i )
	{
		LayerSelectionData_t::DataLayer_t *current = &data->m_vecData[ i ];
		LayerSelectionData_t::DataLayer_t *next = &data->m_vecData[ i + 1 ];

		if ( frac >= current->m_flStartFraction && frac <= next->m_flStartFraction )
		{
			frac = RemapVal( frac, current->m_flStartFraction, next->m_flStartFraction, 0.0f, 1.0f );
			ts.m_flIntensity = frac;
			break;
		}
	}

	return i;
}

void CDmeChannel::Record()
{
	VPROF( "CDmeChannel::Record" );

	CDmAttribute *pFromAttr = GetFromAttribute();
	if ( pFromAttr == NULL )
		return; // or clear out the log?

	CDmeLog *pLog = GetLog();
	DmeTime_t time = GetCurrentTime();
	if ( m_tPreviousTime == DMETIME_INVALID )
	{
		m_tPreviousTime = time;
	}

	if ( g_pChannelRecordingMgr->ShouldRecordUsingTimeSelection() )
	{
		Assert( m_nRecordLayerIndex != -1 );

		// Computes local times for the time selection
		DmeLog_TimeSelection_t timeSelection;
		g_pChannelRecordingMgr->GetLocalTimeSelection( timeSelection, m_nRecordLayerIndex );

		int nType = g_pChannelRecordingMgr->GetProceduralType();
		switch ( nType )
		{
		default:
		case PROCEDURAL_PRESET_NOT:
			{
				pLog->StampKeyAtHead( time, m_tPreviousTime, timeSelection, pFromAttr, m_fromIndex.Get() );
			}
			break;
		case PROCEDURAL_PRESET_REVEAL:
			{
				// Find the matching layer in the "target" data array
				const CDmrElementArray<CDmElement> snapshotArray = const_cast< CDmAttribute * >( g_pChannelRecordingMgr->GetProceduralTarget() );
				CDmeLogLayer *snapshotLayer = FindLayerInSnapshot( snapshotArray, pLog );
				if ( snapshotLayer )
				{
					Assert( pLog );
					pLog->RevealUsingTimeSelection( timeSelection, snapshotLayer );
				}
			}
			break;
		case PROCEDURAL_PRESET_JITTER:
		case PROCEDURAL_PRESET_SMOOTH:
		case PROCEDURAL_PRESET_SHARPEN:
		case PROCEDURAL_PRESET_SOFTEN:
		case PROCEDURAL_PRESET_STAGGER:
		case PROCEDURAL_PRESET_PASTE:
			{
				const CUtlVector< KeyValues * > &pasteTarget = g_pChannelRecordingMgr->GetPasteTarget();
				KeyValues *layer = FindLayerInPasteData( pasteTarget, pLog );
				if ( layer )
				{
					LayerSelectionData_t *data = reinterpret_cast< LayerSelectionData_t * >( layer->GetPtr( "LayerData" ) );
					Assert( data );

					int iSourceLayer = FindSpanningLayerAndSetIntensity( timeSelection, data );

					CDmeLogLayer *sourceLayer = data->m_vecData[ iSourceLayer ].m_hData.Get();
					CDmeLogLayer *targetLayer = data->m_vecData[ iSourceLayer + 1 ].m_hData.Get();
					if ( sourceLayer && sourceLayer->GetKeyCount() > 0 &&
						 targetLayer && targetLayer->GetKeyCount() > 0 &&
						 sourceLayer->GetKeyCount() == targetLayer->GetKeyCount() )
					{
						Assert( pLog->GetNumLayers() >= 2 );
						CDmeLogLayer *outputLayer = pLog->GetLayer( pLog->GetTopmostLayer() );
						if ( nType == PROCEDURAL_PRESET_STAGGER )
						{
							pLog->BlendTimesUsingTimeSelection( sourceLayer, targetLayer, outputLayer, timeSelection, data->m_tStartOffset );
						}
						else
						{
							pLog->BlendLayersUsingTimeSelection( sourceLayer, targetLayer, outputLayer, timeSelection, false, data->m_tStartOffset );
						}
					}
				}
			}
			break;
		}
	}
	else
	{
		if ( m_tPreviousTime != time )
		{
			pLog->SetDuplicateKeyAtTime( m_tPreviousTime );
		}
		pLog->SetKey( time, pFromAttr, m_fromIndex.Get(), m_nNextCurveType );
		m_nNextCurveType = CURVE_DEFAULT;
	}

	// Output the data that's in the log
	Play();
}


//-----------------------------------------------------------------------------
// Builds a clip stack that passes through root + shot
// Returns true if it succeeded
//-----------------------------------------------------------------------------
bool CDmeChannel::BuildClipStack( DmeClipStack_t *pClipStack, CDmeClip *pRoot, CDmeClip *pShot )
{
	DmAttributeReferenceIterator_t it;
	for ( it = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( it );
		CDmElement *pElement = pAttribute->GetOwner();
		CDmeChannelsClip *pChannelsClip = CastElement< CDmeChannelsClip >( pElement );
		if ( !pChannelsClip )
			continue;

		if ( pChannelsClip->BuildClipStack( pClipStack, pRoot, pShot ) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Finds the owner clip for a channel which passes through the root
//-----------------------------------------------------------------------------
CDmeClip* CDmeChannel::FindOwnerClipForChannel( CDmeClip *pRoot )
{
	DmeClipStack_t stack;
	if ( BuildClipStack( &stack, pRoot, pRoot ) )
		return stack[ stack.Count() - 1 ];
	return NULL;
}

