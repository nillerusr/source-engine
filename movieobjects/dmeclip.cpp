//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeclip.h"

#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"

#include "movieobjects/dmetimeframe.h"
#include "movieobjects/dmebookmark.h"
#include "movieobjects/dmesound.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmecamera.h"
#include "movieobjects/dmelight.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeinput.h"
#include "movieobjects/dmeoperator.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmematerialoverlayfxclip.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects_interfaces.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// String to clip type + back
//-----------------------------------------------------------------------------
static const char *s_pClipTypeNames[DMECLIP_TYPE_COUNT] = 
{
	"Channel",
	"Audio",
	"Effects",
	"Film",
};

DmeClipType_t ClipTypeFromString( const char *pName )
{
	for ( DmeClipType_t i = DMECLIP_FIRST; i <= DMECLIP_LAST; ++i )
	{
		if ( !Q_stricmp( pName, s_pClipTypeNames[i] ) )
			return i;
	}
	return DMECLIP_UNKNOWN;
}

const char *ClipTypeToString( DmeClipType_t type )
{
	if ( type >= DMECLIP_FIRST && type <= DMECLIP_LAST )
		return s_pClipTypeNames[ type ];
	return "Unknown";
}


//-----------------------------------------------------------------------------
// CDmeClip - common base class for filmclips, soundclips, and channelclips
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeClip, CDmeClip );

void CDmeClip::OnConstruction()
{
	m_TimeFrame.InitAndCreate( this, "timeFrame" );
	m_ClipColor.InitAndSet( this, "color", Color( 0, 0, 0, 0 ) );
	m_ClipText.Init( this, "text" );
	m_bMute.Init( this, "mute" );
	m_TrackGroups.Init( this, "trackGroups", FATTRIB_MUSTCOPY | FATTRIB_HAS_ARRAY_CALLBACK );
}

void CDmeClip::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Inherited from IDmElement
//-----------------------------------------------------------------------------
void CDmeClip::OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	BaseClass::OnAttributeArrayElementAdded( pAttribute, nFirstElem, nLastElem );
	if ( pAttribute == m_TrackGroups.GetAttribute() )
	{
		for ( int i = nFirstElem; i <= nLastElem; ++i )
		{
			CDmeTrackGroup *pTrackGroup = m_TrackGroups[ i ];
			if ( pTrackGroup )
			{
				pTrackGroup->SetOwnerClip( this );
			}
		}
		return;
	}
}

void CDmeClip::OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	BaseClass::OnAttributeArrayElementRemoved( pAttribute, nFirstElem, nLastElem );
	if ( pAttribute == m_TrackGroups.GetAttribute() )
	{
		for ( int i = nFirstElem; i <= nLastElem; ++i )
		{
			CDmeTrackGroup *pTrackGroup = m_TrackGroups[ i ];
			if ( pTrackGroup )
			{
				pTrackGroup->SetOwnerClip( NULL );
			}
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Clip color
//-----------------------------------------------------------------------------
void CDmeClip::SetClipColor( const Color& clr )
{
	m_ClipColor.Set( clr );
}

Color CDmeClip::GetClipColor() const
{
	return m_ClipColor.Get();
}


//-----------------------------------------------------------------------------
// Clip text
//-----------------------------------------------------------------------------
void CDmeClip::SetClipText( const char *pText )
{
	m_ClipText = pText;
}

const char*	CDmeClip::GetClipText() const
{
	return m_ClipText;
}


//-----------------------------------------------------------------------------
// Returns the time frame
//-----------------------------------------------------------------------------
CDmeTimeFrame *CDmeClip::GetTimeFrame() const
{
	return m_TimeFrame.GetElement();
}

DmeTime_t CDmeClip::ToChildMediaTime( DmeTime_t t, bool bClamp ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->ToChildMediaTime( t, bClamp ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::FromChildMediaTime( DmeTime_t t, bool bClamp ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->FromChildMediaTime( t, bClamp ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::ToChildMediaDuration( DmeTime_t dt ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->ToChildMediaDuration( dt ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::FromChildMediaDuration( DmeTime_t dt ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->FromChildMediaDuration( dt ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetTimeOffset() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetTimeOffset() : DmeTime_t( 0 );
}

float CDmeClip::GetTimeScale() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetTimeScale() : 0.0f;
}

DmeTime_t CDmeClip::GetStartTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartTime() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetEndTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartTime() + tf->GetDuration() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetDuration() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetDuration() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetStartInChildMediaTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartInChildMediaTime() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetEndInChildMediaTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetEndInChildMediaTime() : DmeTime_t( 0 );
}

void CDmeClip::SetTimeOffset( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetTimeOffset( t );
	}
}

void CDmeClip::SetTimeScale( float s )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetTimeScale( s );
	}
}

void CDmeClip::SetStartTime( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetStartTime( t );
	}
}

void CDmeClip::SetDuration( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetDuration( t );
	}
}


//-----------------------------------------------------------------------------
// Track iteration
//-----------------------------------------------------------------------------
const CUtlVector< DmElementHandle_t > &CDmeClip::GetTrackGroups( ) const
{
	return m_TrackGroups.Get();
}

int CDmeClip::GetTrackGroupCount( ) const
{
	// Make sure no invalid clip types have snuck in
	return m_TrackGroups.Count();
}

CDmeTrackGroup *CDmeClip::GetTrackGroup( int nIndex ) const
{
	if ( ( nIndex >= 0 ) && ( nIndex < m_TrackGroups.Count() ) )
		return m_TrackGroups[ nIndex ];
	return NULL;
}
	
//-----------------------------------------------------------------------------
// Is a track group valid to add?
//-----------------------------------------------------------------------------
bool CDmeClip::IsTrackGroupValid( CDmeTrackGroup *pTrackGroup )
{
	// FIXME: If track groups have allowed types, we can check for validity
	for ( DmeClipType_t i = DMECLIP_FIRST; i <= DMECLIP_LAST; ++i )
	{
		if ( !IsSubClipTypeAllowed( i ) && pTrackGroup->IsSubClipTypeAllowed( i ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Track group addition/removal
//-----------------------------------------------------------------------------
void CDmeClip::AddTrackGroup( CDmeTrackGroup *pTrackGroup )
{
	if ( !IsTrackGroupValid( pTrackGroup ) )
		return;

	// FIXME:  Should check if track with same name already exists???
	if ( GetTrackGroupIndex( pTrackGroup ) < 0 )
	{
		m_TrackGroups.AddToTail( pTrackGroup );
	}
}

void CDmeClip::AddTrackGroupBefore( CDmeTrackGroup *pTrackGroup, CDmeTrackGroup *pBefore )
{
	if ( !IsTrackGroupValid( pTrackGroup ) )
		return;

	// FIXME:  Should check if track with same name already exists???
	if ( GetTrackGroupIndex( pTrackGroup ) < 0 )
	{
		int nBeforeIndex = pBefore ? GetTrackGroupIndex( pBefore ) : GetTrackGroupCount();
		if ( nBeforeIndex >= 0 )
		{
			m_TrackGroups.InsertBefore( nBeforeIndex, pTrackGroup );
		}
	}
}


CDmeTrackGroup *CDmeClip::AddTrackGroup( const char *pTrackGroupName )
{
	CDmeTrackGroup *pTrackGroup = CreateElement< CDmeTrackGroup >( pTrackGroupName, GetFileId() );
	pTrackGroup->SetMinimized( false );
	m_TrackGroups.AddToTail( pTrackGroup );
	return pTrackGroup;
}

void CDmeClip::RemoveTrackGroup( int nIndex )
{
	Assert( nIndex >= 0 && nIndex < m_TrackGroups.Count() );

	m_TrackGroups.Remove( nIndex );
}

void CDmeClip::RemoveTrackGroup( CDmeTrackGroup *pTrackGroup )
{
	int i = GetTrackGroupIndex( pTrackGroup );
	if ( i < 0 )
		return;

	m_TrackGroups.Remove( i );
}

void CDmeClip::RemoveTrackGroup( const char *pTrackGroupName )
{	
	if ( !pTrackGroupName )
	{
		pTrackGroupName = DMETRACKGROUP_DEFAULT_NAME;
	}

	int c = m_TrackGroups.Count();
	for ( int i = c; --i >= 0; )
	{
		if ( !Q_strcmp( m_TrackGroups[i]->GetName(), pTrackGroupName ) )
		{
			m_TrackGroups.Remove( i );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Swap track groups
//-----------------------------------------------------------------------------
void CDmeClip::SwapOrder( CDmeTrackGroup *pTrackGroup1, CDmeTrackGroup *pTrackGroup2 )
{
	if ( pTrackGroup1 == pTrackGroup2 )
		return;
	if ( pTrackGroup1->IsFilmTrackGroup() || pTrackGroup2->IsFilmTrackGroup() )
		return;

	int nIndex1 = -1, nIndex2 = -1;
	int c = m_TrackGroups.Count();
	for ( int i = c; --i >= 0; )
	{
		if ( m_TrackGroups[i] == pTrackGroup1 )
		{
			nIndex1 = i;
		}
		if ( m_TrackGroups[i] == pTrackGroup2 )
		{
			nIndex2 = i;
		}
	}
	if ( ( nIndex1 < 0 ) || ( nIndex2 < 0 ) )
		return;

	m_TrackGroups.Swap( nIndex1, nIndex2 );
}

	
//-----------------------------------------------------------------------------
// Track group finding
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeClip::FindTrackGroup( const char *pTrackGroupName ) const
{
	if ( !pTrackGroupName )
	{
		pTrackGroupName = DMETRACKGROUP_DEFAULT_NAME;
	}

	int c = m_TrackGroups.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeTrackGroup *pTrackGroup = m_TrackGroups[i];
		if ( !pTrackGroup )
			continue;

		if ( !Q_strcmp( pTrackGroup->GetName(), pTrackGroupName ) )
			return pTrackGroup;
	}
	return NULL;
}

int CDmeClip::GetTrackGroupIndex( CDmeTrackGroup *pTrackGroup ) const
{
	int nTrackGroups = m_TrackGroups.Count();
	for ( int i = 0 ; i < nTrackGroups; ++i )
	{
		if ( pTrackGroup == m_TrackGroups[i] )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Find or create a track group
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeClip::FindOrAddTrackGroup( const char *pTrackGroupName )
{
	CDmeTrackGroup *pTrackGroup = FindTrackGroup( pTrackGroupName );
	if ( !pTrackGroup )
	{
		pTrackGroup = AddTrackGroup( pTrackGroupName );
	}
	return pTrackGroup;
}


//-----------------------------------------------------------------------------
// Finding clips in track groups
//-----------------------------------------------------------------------------
CDmeTrack *CDmeClip::FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup /*= NULL*/ ) const
{
//	DmeClipType_t type = pClip->GetClipType();
	int c = m_TrackGroups.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		// FIXME: If trackgroups have valid types, can early out here
		CDmeTrack *pTrack = m_TrackGroups[i]->FindTrackForClip( pClip );
		if ( pTrack )
		{
			if ( ppTrackGroup )
			{
				*ppTrackGroup = m_TrackGroups[i];
			}
			return pTrack;
		}
	}

	return NULL;
}

bool CDmeClip::FindMultiTrackGroupForClip( CDmeClip *pClip, int *pTrackGroupIndex, int *pTrackIndex, int *pClipIndex ) const
{
	int nTrackGroups = m_TrackGroups.Count();
	for ( int gi = 0 ; gi < nTrackGroups; ++gi )
	{
		CDmeTrackGroup *pTrackGroup = m_TrackGroups[ gi ];
		if ( !pTrackGroup )
			continue;

		if ( !pTrackGroup->FindTrackForClip( pClip, pTrackIndex, pClipIndex ) )
			continue;

		if ( pTrackGroupIndex )
		{
			*pTrackGroupIndex = gi;
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Finding clips in tracks by time
//-----------------------------------------------------------------------------
void CDmeClip::FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( clipType == DMECLIP_FILM )
		return;

	int gc = GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		if ( !pTrackGroup )
			continue;

		pTrackGroup->FindClipsAtTime( clipType, time, flags, clips );
	}
}

void CDmeClip::FindClipsWithinTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( clipType == DMECLIP_FILM )
		return;

	int gc = GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		if ( !pTrackGroup )
			continue;

		pTrackGroup->FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
	}
}


//-----------------------------------------------------------------------------
// Build a list of all referring clips
//-----------------------------------------------------------------------------
static int BuildReferringClipList( CDmeClip *pClip, CDmeClip** ppParents, int nMaxCount )
{
	int nCount = 0;

	DmAttributeReferenceIterator_t it, it2, it3;
	for ( it = g_pDataModel->FirstAttributeReferencingElement( pClip->GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( it );
		const char *pName = pAttribute->GetName();
		CDmElement *pElement = pAttribute->GetOwner();
		CDmeTrack *pTrack = CastElement< CDmeTrack >( pElement );
		if ( !pTrack )
			continue;

		for ( it2 = g_pDataModel->FirstAttributeReferencingElement( pTrack->GetHandle() );
			it2 != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
			it2 = g_pDataModel->NextAttributeReferencingElement( it2 ) )
		{
			pAttribute = g_pDataModel->GetAttribute( it2 );
			pName = pAttribute->GetName();
			pElement = pAttribute->GetOwner();
			CDmeTrackGroup *pTrackGroup = CastElement< CDmeTrackGroup >( pElement );
			if ( !pTrackGroup )
				continue;

			for ( it3 = g_pDataModel->FirstAttributeReferencingElement( pTrackGroup->GetHandle() );
				it3 != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
				it3 = g_pDataModel->NextAttributeReferencingElement( it3 ) )
			{
				pAttribute = g_pDataModel->GetAttribute( it3 );
				pName = pAttribute->GetName();
				pElement = pAttribute->GetOwner();
				CDmeClip *pParent = CastElement< CDmeClip >( pElement );
				if ( !pParent )
					continue;

				Assert( nCount < nMaxCount );
				if ( nCount >= nMaxCount )
					return nCount;
				ppParents[nCount++] = pParent;
			}
		}
	}
	return nCount;
}


//-----------------------------------------------------------------------------
// Clip stack
//-----------------------------------------------------------------------------
static bool BuildClipStack_R( DmeClipStack_t* pStack, CDmeClip *pMovie, CDmeClip *pShot, CDmeClip *pCurrent )
{
	// Add this clip to the stack
	int nIndex = pStack->AddToHead( CDmeHandle< CDmeClip >( pCurrent ) );

	// Is this clip the shot? We don't need to look for it any more.
	if ( pCurrent == pShot )
	{
		pShot = NULL;
	}

	// Is this clip the movie? We succeeded if we already found the shot!
	if ( pCurrent == pMovie )
	{
		if ( !pShot )
			return true;
	}
	else
	{
		// NOTE: This algorithm assumes a clip can never appear twice under another clip
		// at a single level of hierarchy.
		CDmeClip* ppParents[1024];
		int nCount = BuildReferringClipList( pCurrent, ppParents, 1024 );
		for ( int i = 0; i < nCount; ++i )
		{
			// Can we find a path to the root through the shot? We succeeded!
			if ( BuildClipStack_R( pStack, pMovie, pShot, ppParents[i] ) )
				return true;
		}
	}

	// This clip didn't work out for us. Remove it.
	pStack->Remove( nIndex );

	return false;
}

bool CDmeClip::BuildClipStack( DmeClipStack_t* pStack, CDmeClip *pMovie, CDmeClip *pShot )
{
	// Walk through each shot in the movie and look for the subClip, if don't find it recurse into each shot
	return BuildClipStack_R( pStack, pMovie, pShot, this );
}

DmeTime_t CDmeClip::ToChildMediaTime( const DmeClipStack_t& stack, DmeTime_t globalTime, bool bClamp /* = true */ )
{
	DmeTime_t time = globalTime;

	int nClips = stack.Count();
	for ( int i = 0; i < nClips; ++i )
	{
		time = stack[ i ]->ToChildMediaTime( time, bClamp );
	}

	return time;
}

DmeTime_t CDmeClip::FromChildMediaTime( const DmeClipStack_t& stack, DmeTime_t localTime, bool bClamp /* = true */ )
{
	DmeTime_t time = localTime;

	int nClips = stack.Count();
	for ( int i = nClips-1; i >= 0; --i )
	{
		time = stack[ i ]->FromChildMediaTime( time, bClamp );
	}

	return time;
}

DmeTime_t CDmeClip::ToChildMediaDuration( const DmeClipStack_t& stack, DmeTime_t globalDuration )
{
	DmeTime_t duration = globalDuration;

	int nClips = stack.Count();
	for ( int i = 0; i < nClips; ++i )
	{
		duration = stack[ i ]->ToChildMediaDuration( duration );
	}

	return duration;
}

DmeTime_t CDmeClip::FromChildMediaDuration( const DmeClipStack_t& stack, DmeTime_t localDuration )
{
	DmeTime_t duration = localDuration;

	int nClips = stack.Count();
	for ( int i = nClips-1; i >= 0; --i )
	{
		duration = stack[ i ]->FromChildMediaDuration( duration );
	}

	return duration;
}


void CDmeClip::ToChildMediaTime( DmeLog_TimeSelection_t &params, const DmeClipStack_t& stack )
{
	for ( int i = 0; i < TS_TIME_COUNT; ++i )
	{
		params.m_nTimes[i] = ToChildMediaTime( stack, params.m_nTimes[i], false );
	}
}


//-----------------------------------------------------------------------------
//
// CDmeSoundClip - timeframe view into a dmesound
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSoundClip, CDmeSoundClip );

void CDmeSoundClip::OnConstruction()
{
	m_Sound.Init( this, "sound" );
	m_bShowWave.InitAndSet( this, "showwave", false );
}

void CDmeSoundClip::OnDestruction()
{
}

void CDmeSoundClip::SetShowWave( bool state )
{
	m_bShowWave = state;
}

bool CDmeSoundClip::ShouldShowWave( ) const
{
	return m_bShowWave;
}

//-----------------------------------------------------------------------------
// CDmeChannelsClip - timeframe view into a set of channels
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeChannelsClip, CDmeChannelsClip );

void CDmeChannelsClip::OnConstruction()
{
	m_Channels.Init( this, "channels" );
}

void CDmeChannelsClip::OnDestruction()
{
}

CDmeChannel *CDmeChannelsClip::CreatePassThruConnection( char const *passThruName,
														CDmElement *pFrom, char const *pFromAttribute,
														CDmElement *pTo, char const *pToAttribute, int index /*= 0*/ )
{
	CDmeChannel *helper = CreateElement< CDmeChannel >( passThruName, GetFileId() );
	Assert( helper );

	helper->SetMode( CM_PASS );
	helper->SetInput( pFrom, pFromAttribute );
	helper->SetOutput( pTo, pToAttribute, index );

	m_Channels.AddToTail( helper );

	return helper;
}

void CDmeChannelsClip::RemoveChannel( CDmeChannel *pChannel )
{
	int nCount = m_Channels.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pChannel == m_Channels[i] )
		{
			m_Channels.Remove( i );
			break;
		}
	}
}



//-----------------------------------------------------------------------------
// CDmeFXClip - timeframe view into an effect
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFXClip, CDmeFXClip );

void CDmeFXClip::OnConstruction()
{
}

void CDmeFXClip::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Global list of FX clip types
//-----------------------------------------------------------------------------
const char *CDmeFXClip::s_pFXClipTypes[MAX_FXCLIP_TYPES];
const char *CDmeFXClip::s_pFXClipDescriptions[MAX_FXCLIP_TYPES];
int CDmeFXClip::s_nFXClipTypeCount = 0;

void CDmeFXClip::InstallFXClipType( const char *pElementType, const char *pDescription )
{
	s_pFXClipTypes[s_nFXClipTypeCount] = pElementType;
	s_pFXClipDescriptions[s_nFXClipTypeCount] = pDescription;
	++s_nFXClipTypeCount;
}

int CDmeFXClip::FXClipTypeCount()
{
	return s_nFXClipTypeCount;
}

const char *CDmeFXClip::FXClipType( int nIndex )
{
	Assert( s_nFXClipTypeCount > nIndex );
	return s_pFXClipTypes[nIndex];
}

const char *CDmeFXClip::FXClipDescription( int nIndex )
{
	Assert( s_nFXClipTypeCount > nIndex );
	return s_pFXClipDescriptions[nIndex];
}


//-----------------------------------------------------------------------------
// CDmeFilmClip - hierarchical clip (movie, sequence or shot) w/ scene info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFilmClip, CDmeFilmClip );

void CDmeFilmClip::OnConstruction()
{
	m_MaterialOverlayEffect.Init( this, "materialOverlay" );

	m_MapName.Init( this, "mapname" );
	m_Camera.Init( this, "camera" );
	m_MonitorCameras.Init( this, "monitorCameras" );
	m_nActiveMonitor.InitAndSet( this, "activeMonitor", -1 );
	m_Lights.Init( this, "lights" );
	m_Scene.Init( this, "scene" );
	m_AVIFile.Init( this, "aviFile" );
	m_fadeInDuration .InitAndSet( this, "fadeIn", 0 );
	m_fadeOutDuration.InitAndSet( this, "fadeOut", 0 );

	m_Inputs.Init( this, "inputs" );
	m_Operators.Init( this, "operators" );
	m_bIsUsingCachedVersion.Init( this, "useAviFile" );
	m_AnimationSets.Init( this, "animationSets" );
	m_Bookmarks.Init( this, "bookmarks" );
	m_FilmTrackGroup.Init( this, "subClipTrackGroup", FATTRIB_HAS_CALLBACK | FATTRIB_HAS_PRE_CALLBACK );
	m_Volume.InitAndSet( this, "volume", 1.0);

	m_pCachedVersion = NULL;
	m_bIsUsingCachedVersion = false;
	m_bReloadCachedVersion = false;
}

void CDmeFilmClip::OnDestruction()
{
	if ( g_pVideo != NULL && m_pCachedVersion != NULL )
	{
		g_pVideo->DestroyVideoMaterial( m_pCachedVersion );
		m_pCachedVersion = NULL;
	}
}


//-----------------------------------------------------------------------------
// Returns the special film track group
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeFilmClip::GetFilmTrackGroup() const
{
	return m_FilmTrackGroup;
}

//-----------------------------------------------------------------------------
// Returns the special film track
//-----------------------------------------------------------------------------
CDmeTrack *CDmeFilmClip::GetFilmTrack() const
{
	CDmeTrackGroup *pTrackGroup = m_FilmTrackGroup.GetElement();
	if ( pTrackGroup )
		return pTrackGroup->GetFilmTrack();
	return NULL;
}

CDmeTrackGroup *CDmeFilmClip::FindOrCreateFilmTrackGroup()
{
	if ( !m_FilmTrackGroup )
	{
		m_FilmTrackGroup = CreateElement< CDmeTrackGroup >( "subClipTrackGroup", GetFileId() );
		m_FilmTrackGroup->SetMinimized( false );
		m_FilmTrackGroup->SetMaxTrackCount( 1 );
		m_FilmTrackGroup->SetOwnerClip( this );
	}
	return m_FilmTrackGroup;
}

CDmeTrack *CDmeFilmClip::FindOrCreateFilmTrack()
{
	CDmeTrackGroup *pTrackGroup = FindOrCreateFilmTrackGroup();
	CDmeTrack *pTrack = pTrackGroup->GetFilmTrack();
	return pTrack ? pTrack : m_FilmTrackGroup->CreateFilmTrack();
}


//-----------------------------------------------------------------------------
// Finding clips in track groups
//-----------------------------------------------------------------------------
CDmeTrack *CDmeFilmClip::FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup /*= NULL*/ ) const
{
	if ( m_FilmTrackGroup.GetElement() )
	{
		CDmeTrack *pTrack = m_FilmTrackGroup->FindTrackForClip( pClip );
		if ( pTrack )
		{
			if ( ppTrackGroup )
			{
				*ppTrackGroup = m_FilmTrackGroup;
			}
			return pTrack;
		}
	}

	return CDmeClip::FindTrackForClip( pClip, ppTrackGroup );
}

//-----------------------------------------------------------------------------
// Finding clips in tracks by time
//-----------------------------------------------------------------------------
void CDmeFilmClip::FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( clipType == DMECLIP_FILM ) || ( clipType == DMECLIP_UNKNOWN ) )
	{
		if ( m_FilmTrackGroup )
		{
			m_FilmTrackGroup->FindClipsAtTime( clipType, time, flags, clips );
		}
	}

	CDmeClip::FindClipsAtTime( clipType, time, flags, clips );
}

void CDmeFilmClip::FindClipsWithinTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( clipType == DMECLIP_FILM ) || ( clipType == DMECLIP_UNKNOWN ) )
	{
		if ( m_FilmTrackGroup )
		{
			m_FilmTrackGroup->FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
		}
	}

	CDmeClip::FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
}


//-----------------------------------------------------------------------------
// Volume
//-----------------------------------------------------------------------------
void CDmeFilmClip::SetVolume( float state )
{
	m_Volume = state;
}
float CDmeFilmClip::GetVolume() const
{
	return m_Volume.Get();
}

//-----------------------------------------------------------------------------
// mapname helper methods
//-----------------------------------------------------------------------------
const char *CDmeFilmClip::GetMapName()
{
	return m_MapName.Get();
}

void CDmeFilmClip::SetMapName( const char *pMapName )
{
	m_MapName.Set( pMapName );
}


//-----------------------------------------------------------------------------
// Attribute changed
//-----------------------------------------------------------------------------
void CDmeFilmClip::PreAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::PreAttributeChanged( pAttribute );
	if ( pAttribute == m_FilmTrackGroup.GetAttribute() )
	{
		if ( m_FilmTrackGroup.GetElement() )
		{
			m_FilmTrackGroup->SetOwnerClip( NULL );
		}
		return;
	}
}

void CDmeFilmClip::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );
	if ( pAttribute == m_FilmTrackGroup.GetAttribute() )
	{
		if ( m_FilmTrackGroup.GetElement() )
		{
			m_FilmTrackGroup->SetMaxTrackCount( 1 );
			m_FilmTrackGroup->SetOwnerClip( this );
		}																											 
	}
	else if ( pAttribute->GetOwner() == m_TimeFrame.GetElement() )
	{
		InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
	}
}

void CDmeFilmClip::OnElementUnserialized( )
{
	BaseClass::OnElementUnserialized();

	CDmeTrackGroup *pFilmTrackGroup = m_FilmTrackGroup.GetElement();
	if ( pFilmTrackGroup )
	{
		pFilmTrackGroup->SetMaxTrackCount( 1 );
		if ( pFilmTrackGroup->GetTrackCount() == 0 )
		{
			pFilmTrackGroup->CreateFilmTrack();
		}
	}

	// this conversion code went in on 10/31/2005
	// I'm hoping we don't care about any files that old - if we ever hit this, we should move this code into an unserialization converter
	Assert( !HasAttribute( "overlay" ) && !HasAttribute( "overlayalpha" ) );
	if ( HasAttribute( "overlay" ) || HasAttribute( "overlayalpha" ) )
	{
		Warning( "CDmeFilmClip %s is an old version that has overlay and/or overlayalpha attributes!\n", GetName() );

		// Backward compat conversion
		// If this is an older file with an overlay attribute, strip it out into materialoverlay
		CDmAttribute *pOverlayAttribute = GetAttribute( "overlay" );
		if ( !pOverlayAttribute )
			goto cleanUp;

		const char *pName = pOverlayAttribute->GetValueString();
		if ( !pName || !pName[0] )
			goto cleanUp;

		// If we don't yet have a material overlay, create one
		if ( m_MaterialOverlayEffect.GetElement() == NULL )
		{
			m_MaterialOverlayEffect = CreateElement<CDmeMaterialOverlayFXClip>( "materialOverlay", GetFileId() );
		}

		m_MaterialOverlayEffect->SetOverlayEffect( pName );

		// If this is an older file with an overlayalpha attribute, strip it out into materialoverlay
		CDmAttribute *pOverlayAlphaAttribute = GetAttribute( "overlayalpha" );
		if ( pOverlayAlphaAttribute )
		{
			float alpha = pOverlayAlphaAttribute->GetValue<float>();
			m_MaterialOverlayEffect->SetAlpha( alpha );
		}

cleanUp:
		// Always strip out the old overlay attribute
		RemoveAttribute( "overlay" );							
		RemoveAttribute( "overlayalpha" );
	}
}


//-----------------------------------------------------------------------------
// Resolve
//-----------------------------------------------------------------------------
void CDmeFilmClip::Resolve()
{
	BaseClass::Resolve();
	if ( m_AVIFile.IsDirty() )
	{
		m_bReloadCachedVersion = true;
		m_AVIFile.GetAttribute()->RemoveFlag( FATTRIB_DIRTY );
	}
}


//-----------------------------------------------------------------------------
// Helper for overlays
//-----------------------------------------------------------------------------
void CDmeFilmClip::SetOverlay( const char *pMaterialName )
{
	if ( pMaterialName && pMaterialName[0] )
	{
		if ( !m_MaterialOverlayEffect.GetElement() )
		{
			m_MaterialOverlayEffect = CreateElement<CDmeMaterialOverlayFXClip>( "materialOverlay", GetFileId() );
		}

		m_MaterialOverlayEffect->SetOverlayEffect( pMaterialName );
	}
	else
	{
		m_MaterialOverlayEffect.Set( NULL );
	}
}

IMaterial *CDmeFilmClip::GetOverlayMaterial()
{
	return m_MaterialOverlayEffect.GetElement() ? m_MaterialOverlayEffect->GetMaterial() : NULL;
}

float CDmeFilmClip::GetOverlayAlpha()
{
	return m_MaterialOverlayEffect.GetElement() ? m_MaterialOverlayEffect->GetAlpha() : 0.0f;
}

void CDmeFilmClip::SetOverlayAlpha( float alpha )
{
	if ( m_MaterialOverlayEffect.GetElement() )
	{
		m_MaterialOverlayEffect->SetAlpha( alpha );
	}
}

bool CDmeFilmClip::HasOpaqueOverlay( void )
{
	if ( m_MaterialOverlayEffect->GetMaterial()->IsTranslucent() ||
	   ( m_MaterialOverlayEffect->GetAlpha() < 1.0f ) )
	{
		return false;
	}

	return true;
}

void CDmeFilmClip::DrawOverlay( DmeTime_t time, Rect_t &currentRect, Rect_t &totalRect )
{
	if ( m_MaterialOverlayEffect.GetElement() )
	{
		m_MaterialOverlayEffect->ApplyEffect( ToChildMediaTime( time ), currentRect, totalRect, NULL );
	}

	DmeTime_t fadeIn( m_fadeInDuration );
	DmeTime_t fadeOut( m_fadeOutDuration );

	float fade = 1.0f;
	if ( time < GetStartTime() + fadeIn )
	{
		fade = ( time - GetStartTime() ) / fadeIn;
	}
	if ( time > GetEndTime() - fadeOut )
	{
		fade = min( fade, ( GetEndTime() - time ) / fadeOut );
	}
	if ( fade < 1.0f )
	{
		if ( !m_FadeMaterial.IsValid() )
		{
			m_FadeMaterial.Init( "engine\\singlecolor.vmt", NULL, false );
		}

		float r, g, b;
		m_FadeMaterial->GetColorModulation( &r, &g, &b );
		float a = m_FadeMaterial->GetAlphaModulation();

		m_FadeMaterial->ColorModulate( 0.0f, 0.0f, 0.0f );
		m_FadeMaterial->AlphaModulate( 1.0f - fade );

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( m_FadeMaterial );

		float w = currentRect.width;
		float h = currentRect.height;

		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

		meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( 0.0f, h, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w, h, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();

		m_FadeMaterial->ColorModulate( r, g, b );
		m_FadeMaterial->AlphaModulate( a );
	}
}


//-----------------------------------------------------------------------------
// AVI tape out
//-----------------------------------------------------------------------------
void CDmeFilmClip::UseCachedVersion( bool bUseCachedVersion )
{
	m_bIsUsingCachedVersion = bUseCachedVersion;
}

bool CDmeFilmClip::IsUsingCachedVersion() const
{
	return m_bIsUsingCachedVersion;
}

IVideoMaterial *CDmeFilmClip::GetCachedVideoMaterial()
{
	if ( m_bReloadCachedVersion )
	{
		if ( g_pVideo )
		{
			if ( m_pCachedVersion != NULL )
			{
				g_pVideo->DestroyVideoMaterial( m_pCachedVersion );
				m_pCachedVersion = NULL;
			}
			if ( m_AVIFile[0] )
			{
				m_pCachedVersion = g_pVideo->CreateVideoMaterial( m_AVIFile, m_AVIFile, "MOD" );
			}
		
		}
		m_bReloadCachedVersion = false;
	}
	return m_pCachedVersion;
}
	
void CDmeFilmClip::SetCachedAVI( const char *pAVIFile )
{
	m_AVIFile = pAVIFile;
	m_bReloadCachedVersion = true;
}


//-----------------------------------------------------------------------------
// Camera helper methods
//-----------------------------------------------------------------------------
CDmeCamera *CDmeFilmClip::GetCamera()
{
	return m_Camera;
}

void CDmeFilmClip::SetCamera( CDmeCamera *pCamera )
{
	m_Camera = pCamera;
}


//-----------------------------------------------------------------------------
// Returns the monitor camera associated with the clip (for now, only 1 supported)
//-----------------------------------------------------------------------------
CDmeCamera *CDmeFilmClip::GetMonitorCamera()
{
	if ( m_nActiveMonitor < 0 )
		return NULL;
	return m_MonitorCameras[ m_nActiveMonitor ];
}

void CDmeFilmClip::AddMonitorCamera( CDmeCamera *pCamera )
{
	m_MonitorCameras.AddToTail( pCamera );
}

int CDmeFilmClip::FindMonitorCamera( CDmeCamera *pCamera )
{
	return m_MonitorCameras.Find( pCamera->GetHandle() );
}

void CDmeFilmClip::RemoveMonitorCamera( CDmeCamera *pCamera )
{
	int i = m_MonitorCameras.Find( pCamera->GetHandle() );
	if ( i >= 0 )
	{
		if ( m_nActiveMonitor == i )
		{
			m_nActiveMonitor = -1;
		}
		m_MonitorCameras.FastRemove( i );
	}
}

void CDmeFilmClip::SelectMonitorCamera( CDmeCamera *pCamera )
{
	m_nActiveMonitor = pCamera ? m_MonitorCameras.Find( pCamera->GetHandle() ) : -1;
}


//-----------------------------------------------------------------------------
// Light helper methods
//-----------------------------------------------------------------------------
int CDmeFilmClip::GetLightCount()
{
	return m_Lights.Count();
}

CDmeLight *CDmeFilmClip::GetLight( int nIndex )
{
	if ( ( nIndex < 0 ) || ( nIndex >= m_Lights.Count() ) )
		return NULL;

	return m_Lights[ nIndex ];
}

void CDmeFilmClip::AddLight( CDmeLight *pLight )
{
	m_Lights.AddToTail( pLight );
}


//-----------------------------------------------------------------------------
// Scene / Dag helper methods
//-----------------------------------------------------------------------------
CDmeDag *CDmeFilmClip::GetScene()
{
	return m_Scene.GetElement();
}

void CDmeFilmClip::SetScene( CDmeDag *pDag )
{
	m_Scene.Set( pDag );
}


//-----------------------------------------------------------------------------
// helper for inputs and operators
//-----------------------------------------------------------------------------
int CDmeFilmClip::GetInputCount()
{
	return m_Inputs.Count();
}

CDmeInput *CDmeFilmClip::GetInput( int nIndex )
{
	if ( nIndex < 0 || nIndex >= m_Inputs.Count() )
		return NULL;

	return m_Inputs[ nIndex ];
}

void CDmeFilmClip::AddInput( CDmeInput *pInput )
{
	m_Inputs.AddToTail( pInput );
}

void CDmeFilmClip::RemoveAllInputs()
{
	m_Inputs.RemoveAll();
}

void CDmeFilmClip::AddOperator( CDmeOperator *pOperator )
{
	m_Operators.AddToTail( pOperator );
}

void CDmeFilmClip::CollectOperators( CUtlVector< DmElementHandle_t > &operators )
{
	int numInputs = m_Inputs.Count();
	for ( int i = 0; i < numInputs; ++i )
	{
		operators.AddToTail( m_Inputs[ i ]->GetHandle() );
	}

	int numOperators = m_Operators.Count();
	for ( int i = 0; i < numOperators; ++i )
	{
		operators.AddToTail( m_Operators[ i ]->GetHandle() );
	}
}

int	CDmeFilmClip::GetAnimationSetCount()
{
	// yes, this is nasty perf-wise, but since we only have a dozen or so animation sets,
	// and we're about to rewrite the entire structure of animation sets vs. clips vs. scene
	// it's not worth polluting the leaf code just to save a couple cycles short term

	int nCount = 0;

	int nElements = m_AnimationSets.Count();
	for ( int i = 0; i < nElements; ++i )
	{
		CDmElement *pElement = m_AnimationSets.Get( i );
		if ( !pElement )
			continue;

		CDmeAnimationSet *pAnimSet = CastElement< CDmeAnimationSet >( pElement );
		if ( pAnimSet )
		{
			++nCount;
		}
		else
		{
			const CDmAttribute *pChildren = pElement->GetAttribute( "children", AT_ELEMENT_ARRAY );
			if ( !pChildren )
				continue;

			CDmrElementArrayConst< CDmElement > array( pChildren );
			nCount += array.Count();
		}
	}

	return nCount;
}

CDmeAnimationSet *CDmeFilmClip::GetAnimationSet( int idx )
{
	// yes, this is nasty perf-wise, but since we only have a dozen or so animation sets,
	// and we're about to rewrite the entire structure of animation sets vs. clips vs. scene
	// it's not worth polluting the leaf code just to save a couple cycles short term

	int nElements = m_AnimationSets.Count();
	for ( int i = 0; i < nElements; ++i )
	{
		CDmElement *pElement = m_AnimationSets.Get( i );
		if ( !pElement )
			continue;

		CDmeAnimationSet *pAnimSet = CastElement< CDmeAnimationSet >( pElement );
		if ( pAnimSet )
		{
			if ( idx == 0 )
				return pAnimSet;
			--idx;
		}
		else
		{
			const CDmAttribute *pChildren = pElement->GetAttribute( "children", AT_ELEMENT_ARRAY );
			if ( !pChildren )
				continue;

			CDmrElementArrayConst< CDmElement > array( pChildren );
			int nChildren = array.Count();

			if ( idx < nChildren )
				return CastElement< CDmeAnimationSet >( array[ idx ] );
			idx -= nChildren;
		}
	}

	return NULL;
}

void CDmeFilmClip::AddAnimationSet( CDmeAnimationSet *element )
{
	m_AnimationSets.AddToTail( element );
}

void CDmeFilmClip::RemoveAllAnimationSets()
{
	m_AnimationSets.RemoveAll();
}

CDmaElementArray< CDmElement > &CDmeFilmClip::GetAnimationSets()
{
	return m_AnimationSets;
}

const CDmaElementArray< CDmElement > &CDmeFilmClip::GetAnimationSets() const
{
	return m_AnimationSets;
}


const CDmaElementArray< CDmeBookmark > &CDmeFilmClip::GetBookmarks() const
{
	return m_Bookmarks;
}

CDmaElementArray< CDmeBookmark > &CDmeFilmClip::GetBookmarks()
{
	return m_Bookmarks;
}


//-----------------------------------------------------------------------------
// Used to move clips in non-film track groups with film clips
// Call BuildClipAssociations before modifying the film track,
// then UpdateAssociatedClips after modifying it.
//-----------------------------------------------------------------------------
void CDmeFilmClip::BuildClipAssociations( CUtlVector< ClipAssociation_t > &association, bool bHandleGaps )
{
	association.RemoveAll();

	CDmeTrack *pFilmTrack = GetFilmTrack();
	if ( !pFilmTrack )
		return;

	int c = pFilmTrack->GetClipCount();
	int gc = GetTrackGroupCount();
	if ( c == 0 || gc == 0 )
		return;

	DmeTime_t clipStartTime = GetStartInChildMediaTime();
	DmeTime_t clipEndTime   = GetEndInChildMediaTime();

	// These slugs will be removed in UpdateAssociatedClips
	if ( bHandleGaps )
	{
		pFilmTrack->FillAllGapsWithSlugs( "__tempSlug__", clipStartTime, clipEndTime );
	}

	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		int tc = pTrackGroup->GetTrackCount();
		for ( int j = 0; j < tc; ++j )
		{
			CDmeTrack *pTrack = pTrackGroup->GetTrack( j );
			if ( !pTrack->IsSynched() )
				continue;

			// Only have visible tracks now
			int cc = pTrack->GetClipCount();
			association.EnsureCapacity( association.Count() + cc );
			for ( int k = 0; k < cc; ++k )
			{
				CDmeClip *pClip = pTrack->GetClip( k );
				CDmeClip *pFilmClip = pFilmTrack->FindFilmClipAtTime( pClip->GetStartTime() );
				
				int nIndex = association.AddToTail();
				association[nIndex].m_hClip = pClip;
				association[nIndex].m_hAssociation = pFilmClip;
				if ( pFilmClip )
				{
					association[nIndex].m_offset = pClip->GetStartTime() - pFilmClip->GetStartTime();
					association[nIndex].m_nType = ClipAssociation_t::HAS_CLIP;
					continue;
				}

				// Handle edge cases
				if ( pClip->GetStartTime() <= clipStartTime )
				{
					association[nIndex].m_offset = pClip->GetStartTime() - clipStartTime;
					association[nIndex].m_nType = ClipAssociation_t::BEFORE_START;
					continue;
				}

				if ( pClip->GetStartTime() >= clipEndTime )
				{
					association[nIndex].m_offset = pClip->GetStartTime() - clipEndTime;
					association[nIndex].m_nType = ClipAssociation_t::AFTER_END;
					continue;
				}

				association[nIndex].m_offset = DmeTime_t( 0 );
				association[nIndex].m_nType = ClipAssociation_t::NO_MOVEMENT;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Rolls associated clips so they remain in the same relative time
//-----------------------------------------------------------------------------
void CDmeFilmClip::RollAssociatedClips( CDmeClip *pClip, CUtlVector< ClipAssociation_t > &association, DmeTime_t dt )
{
	int c = association.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( association[i].m_nType != ClipAssociation_t::HAS_CLIP )
			continue;

		if ( association[i].m_hAssociation.Get() != pClip )
			continue;
		  
		DmeTime_t newStartTime = association[i].m_hClip->GetStartTime() - dt;
		association[i].m_hClip->SetStartTime( newStartTime );
	}
}

//-----------------------------------------------------------------------------
// Rolls associated clips so they remain in the same relative time
//-----------------------------------------------------------------------------
void CDmeFilmClip::ScaleAssociatedClips( CDmeClip *pClip, CUtlVector< ClipAssociation_t > &association, float ratio, DmeTime_t oldOffset )
{
	int c = association.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( association[i].m_nType != ClipAssociation_t::HAS_CLIP )
			continue;

		if ( association[i].m_hAssociation.Get() != pClip )
			continue;

		DmeTime_t clipStartTime = pClip->GetStartTime();
		DmeTime_t oldStartTime = association[i].m_hClip->GetStartTime();
		DmeTime_t newStartTime = ( oldStartTime - clipStartTime + oldOffset ) / ratio + clipStartTime - pClip->GetTimeOffset();
		association[i].m_hClip->SetStartTime( newStartTime );
	}
}

void CDmeFilmClip::UpdateAssociatedClips( CUtlVector< ClipAssociation_t > &association )
{
	int i;

	CDmeTrack *pFilmTrack = GetFilmTrack();
	if ( !pFilmTrack )
		return;

	int c = association.Count(); 
	if ( c > 0 )
	{
		DmeTime_t clipStartTime = GetStartInChildMediaTime();
		DmeTime_t clipEndTime   = GetEndInChildMediaTime();

		for ( i = 0; i < c; ++i )
		{
			ClipAssociation_t &curr = association[i];
			if ( !curr.m_hClip.Get() )
				continue;

			switch( curr.m_nType )
			{
			case ClipAssociation_t::HAS_CLIP:
				if ( curr.m_hAssociation.Get() )
				{
					curr.m_hClip->SetStartTime( curr.m_hAssociation->GetStartTime() + curr.m_offset ); 
				}
				break;

			case ClipAssociation_t::BEFORE_START:
				curr.m_hClip->SetStartTime( clipStartTime + curr.m_offset ); 
				break;

			case ClipAssociation_t::AFTER_END:
 				curr.m_hClip->SetStartTime( clipEndTime + curr.m_offset ); 
				break;
			}
		}
	}

	c = pFilmTrack->GetClipCount();
	for ( i = c; --i >= 0; )
	{
		CDmeClip *pClip = pFilmTrack->GetClip(i);
		if ( !Q_strcmp( pClip->GetName(), "__tempSlug__" ) )
		{
			pFilmTrack->RemoveClip( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Creates a slug clip
//-----------------------------------------------------------------------------
CDmeFilmClip *CreateSlugClip( const char *pClipName, DmeTime_t startTime, DmeTime_t endTime, DmFileId_t fileid )
{
	CDmeFilmClip *pSlugClip = CreateElement<CDmeFilmClip>( pClipName, fileid );
	pSlugClip->GetTimeFrame()->SetName( "timeframe" );
	pSlugClip->SetStartTime( startTime );
	pSlugClip->SetDuration( endTime - startTime );
	pSlugClip->SetTimeOffset( DmeTime_t( 0 ) );
	pSlugClip->SetTimeScale( 1.0f );
	pSlugClip->SetClipColor( Color( 0, 0, 0, 128 ) );
	pSlugClip->SetOverlay( "vgui/black" );
	return pSlugClip;
}

//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeTrack *GetParentTrack( CDmeClip *pClip )
{
	DmAttributeReferenceIterator_t hAttr = g_pDataModel->FirstAttributeReferencingElement( pClip->GetHandle() );
	for ( ; hAttr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; hAttr = g_pDataModel->NextAttributeReferencingElement( hAttr ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( hAttr );
		if ( !pAttr )
			continue;

		CDmeTrack *pTrack = CastElement< CDmeTrack >( pAttr->GetOwner() );
		if ( pTrack )
			return pTrack;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds a channel in a channel or film clip targetting a particular element 
//-----------------------------------------------------------------------------
CDmeChannel *FindChannelTargetingElement( CDmeChannelsClip *pChannelsClip, CDmElement *pElement, const char *pAttributeName )
{
	int nChannels = pChannelsClip->m_Channels.Count();
	for ( int i = 0; i < nChannels; ++i )
	{
		CDmeChannel *pChannel = pChannelsClip->m_Channels[ i ];
		CDmElement *toElement = pChannel->GetToElement();
		if ( toElement != pElement )
			continue;

		if ( pAttributeName && ( Q_stricmp( pChannel->GetToAttribute()->GetName(), pAttributeName ) != 0 ) )
			continue;

		return pChannel;
	}

	return NULL;
}

CDmeChannel *FindChannelTargetingElement( CDmeFilmClip *pClip, CDmElement *pElement, const char *pAttributeName, CDmeChannelsClip **ppChannelsClip, CDmeTrack **ppTrack, CDmeTrackGroup **ppTrackGroup )
{
	int gc = pClip->GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = pClip->GetTrackGroup( i );
		DMETRACKGROUP_FOREACH_CLIP_TYPE_START( CDmeChannelsClip, pTrackGroup, pTrack, pChannelsClip )

			CDmeChannel *pChannel = FindChannelTargetingElement( pChannelsClip, pElement, pAttributeName );
			if ( !pChannel )
				continue;

			if ( ppChannelsClip )
			{
				*ppChannelsClip = pChannelsClip;
			}
			if ( ppTrack )
			{
				*ppTrack = pTrack;
			}
			if ( ppTrackGroup )
			{
				*ppTrackGroup = pTrackGroup;
			}
			return pChannel;

		DMETRACKGROUP_FOREACH_CLIP_TYPE_END()
	}

	return NULL;
}
