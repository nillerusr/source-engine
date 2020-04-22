//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmetrack.h"

#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmetrackgroup.h"

#include "movieobjects_interfaces.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// The solo track
//-----------------------------------------------------------------------------
DmElementHandle_t CDmeTrack::m_hSoloTrack[ DMECLIP_TYPE_COUNT ] = 
{
	DMELEMENT_HANDLE_INVALID,
	DMELEMENT_HANDLE_INVALID,
	DMELEMENT_HANDLE_INVALID,
	DMELEMENT_HANDLE_INVALID,
};


//-----------------------------------------------------------------------------
// CDmeTrack - common container class for clip objects
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTrack, CDmeTrack );

void CDmeTrack::OnConstruction()
{
	m_hOwner = DMELEMENT_HANDLE_INVALID;

	m_Flags.ClearAllFlags();
	m_Clips.Init( this, "children" );
	m_Collapsed.InitAndSet( this, "collapsed", true );
	m_Mute.InitAndSet( this, "mute", false );
	m_Synched.InitAndSet( this, "synched", true );
	m_ClipType.InitAndSet( this, "clipType", DMECLIP_UNKNOWN, FATTRIB_HAS_CALLBACK | FATTRIB_HAS_PRE_CALLBACK );

	m_Volume.InitAndSet( this, "volume", 1.0 );

}

void CDmeTrack::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Methods of IDmElement
//-----------------------------------------------------------------------------
void CDmeTrack::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	// Attach callbacks to detected sorted conditions if we're a film clip
	if ( pAttribute == m_ClipType.GetAttribute() )
	{
		if ( m_ClipType == DMECLIP_FILM )
		{
			m_Flags.ClearFlag( IS_SORTED );
		}
		return;
	}

	// This gets called when start/end time of children change, or if the array changes
	// This is a hack, since any OnAttributeChanged call that gets chained here from another element will trigger this
	// At some point, we'll probably have to start sending more data through OnAttributeChanged, (like an event string or chain path)
	// or perhaps add a new callback OnElementChanged() with this data
	if ( pAttribute == m_Clips.GetAttribute() || ( pAttribute->GetOwner() != this ) )
	{
		if ( !m_Flags.IsFlagSet( SUPPRESS_DIRTY_ORDERING ) )
		{
			m_Flags.ClearFlag( IS_SORTED );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Clip type
//-----------------------------------------------------------------------------
DmeClipType_t CDmeTrack::GetClipType() const
{
	return (DmeClipType_t)m_ClipType.Get();
}

void CDmeTrack::SetClipType( DmeClipType_t type )
{
	m_ClipType = type;
}
		
void CDmeTrack::SetCollapsed( bool state )
{
	m_Collapsed = state;

}

bool CDmeTrack::IsCollapsed() const
{
	return m_Collapsed.Get();
}

void CDmeTrack::SetMute( bool state )
{
	m_Mute = state;
}

//-----------------------------------------------------------------------------
// Volume
//-----------------------------------------------------------------------------
void CDmeTrack::SetVolume( float state )
{
	m_Volume = state;
}
float CDmeTrack::GetVolume() const
{
	return m_Volume.Get();
}

// Is this track synched to the film track?
void CDmeTrack::SetSynched( bool bState )
{
	m_Synched = bState;
}

bool CDmeTrack::IsSynched() const
{
	return m_Synched;
}

bool CDmeTrack::IsMute( bool bCheckSoloing ) const
{
	// if we're muted, don't play regardless of whether we're solo
	CDmeTrack *pSoloTrack = bCheckSoloing ? GetSoloTrack() : NULL;
	return m_Mute.Get() || ( pSoloTrack != this && pSoloTrack != NULL );
}

int CDmeTrack::GetClipCount() const
{
	return m_Clips.Count();
}

CDmeClip *CDmeTrack::GetClip( int i ) const
{
	return m_Clips[ i ];
}

const CUtlVector< DmElementHandle_t > &CDmeTrack::GetClips( ) const
{
	return m_Clips.Get();
}

void CDmeTrack::AddClip( CDmeClip *clip )
{
	if ( clip->GetClipType() == GetClipType() )
	{
		// FIXME: In the case of a non-overlapped track,
		// we could optimize this to insert the clip in sorted order,
		// then fix overlaps (fixing overlaps requires a sorted list)
		Assert( FindClip( clip ) < 0 );
		m_Clips.AddToTail( clip );
	}
}

void CDmeTrack::RemoveClip( int i )
{
	// NOTE: Removal shouldn't cause sort order or fixup to become invalid
	CSuppressAutoFixup suppress( this, SUPPRESS_OVERLAP_FIXUP | SUPPRESS_DIRTY_ORDERING );
	m_Clips.Remove( i );
}

bool CDmeTrack::RemoveClip( CDmeClip *clip )
{
	Assert( clip->GetClipType() == GetClipType() );
	int i = FindClip( clip );
	if ( i != -1 )
	{
		RemoveClip( i );
		return true;
	}
	return false;
}

void CDmeTrack::RemoveAllClips()
{
	CSuppressAutoFixup suppress( this, SUPPRESS_OVERLAP_FIXUP | SUPPRESS_DIRTY_ORDERING );
	m_Clips.RemoveAll();
}


//-----------------------------------------------------------------------------
// Returns the solo track, if any
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrack::GetSoloTrack( DmeClipType_t clipType )
{
	return GetElement< CDmeTrack >( m_hSoloTrack[ clipType ] );
}

void CDmeTrack::SetSoloTrack( DmeClipType_t clipType, CDmeTrack *pTrack )
{
	m_hSoloTrack[ clipType ] = pTrack->GetHandle();
}

bool CDmeTrack::IsSoloTrack() const
{
	return m_hSoloTrack[ GetClipType() ] == GetHandle();
}

CDmeTrack *CDmeTrack::GetSoloTrack() const
{
	return GetSoloTrack( GetClipType() );
}

void CDmeTrack::SetSoloTrack( )
{
	m_hSoloTrack[ GetClipType() ] = GetHandle();
}


//-----------------------------------------------------------------------------
// Methods related to finding clips
//-----------------------------------------------------------------------------
int CDmeTrack::FindClip( CDmeClip *clip )
{
	Assert( clip->GetClipType() == GetClipType() );
	int c = m_Clips.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		if ( m_Clips[ i ] == clip )
			return i;
	}
	return -1;
}

CDmeClip *CDmeTrack::FindNamedClip( const char *name )
{
	int c = m_Clips.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		CDmeClip *child = m_Clips[ i ];
		if ( child && !Q_stricmp( child->GetName(), name ) )
			return child;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Find clips at, intersecting or within a particular time interval
//-----------------------------------------------------------------------------
void CDmeTrack::FindClipsAtTime( DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && IsCollapsed() )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int nClipCount = GetClipCount();
	for ( int j = 0; j < nClipCount; ++j )
	{
		CDmeClip *pSubClip = GetClip( j );
		if ( !pSubClip )
			continue;
		if ( ( flags & DMESKIP_MUTED ) && pSubClip->IsMute() )
			continue;

		if ( time.IsInRange( pSubClip->GetStartTime(), pSubClip->GetEndTime() ) )
		{
			clips.AddToTail( pSubClip );
		}
	}
}

void CDmeTrack::FindClipsIntersectingTime( DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && IsCollapsed() )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int nClipCount = GetClipCount();
	for ( int j = 0; j < nClipCount; ++j )
	{
		CDmeClip *pSubClip = GetClip( j );
		if ( !pSubClip )
			continue;
		if ( ( flags & DMESKIP_MUTED ) && pSubClip->IsMute() )
			continue;

		DmeTime_t clipStart = pSubClip->GetStartTime();
		DmeTime_t clipEnd = pSubClip->GetEndTime();
		if ( clipEnd >= startTime && clipStart < endTime )
		{
			clips.AddToTail( pSubClip );
		}
	}
}

void CDmeTrack::FindClipsWithinTime( DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && IsCollapsed() )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int nClipCount = GetClipCount();
	for ( int j = 0; j < nClipCount; ++j )
	{
		CDmeClip *pSubClip = GetClip( j );
		if ( !pSubClip )
			continue;
		if ( ( flags & DMESKIP_MUTED ) && pSubClip->IsMute() )
			continue;

		DmeTime_t clipStart = pSubClip->GetStartTime();
		DmeTime_t clipEnd = pSubClip->GetEndTime();
		if ( clipStart >= startTime && clipEnd <= endTime )
		{
			clips.AddToTail( pSubClip );
		}
	}
}


//-----------------------------------------------------------------------------
// Methods related to shifting clips
//-----------------------------------------------------------------------------
void CDmeTrack::ShiftAllClips( DmeTime_t dt )
{
	if ( dt == DmeTime_t( 0 ) )
		return;

	CSuppressAutoFixup suppress( this, SUPPRESS_OVERLAP_FIXUP | SUPPRESS_DIRTY_ORDERING );

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = m_Clips[ i ];
		pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
	}
}

void CDmeTrack::ShiftAllClipsAfter( DmeTime_t startTime, DmeTime_t dt, bool bTestStartingTime )
{
	if ( dt == DmeTime_t( 0 ) )
		return;

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		DmeTime_t testTime = bTestStartingTime ? pSubClip->GetStartTime() : pSubClip->GetEndTime();
		if ( startTime < testTime )
		{
			pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
		}
	}
}

void CDmeTrack::ShiftAllClipsBefore( DmeTime_t endTime, DmeTime_t dt, bool bTestEndingTime )
{
	if ( dt == DmeTime_t( 0 ) )
		return;

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		DmeTime_t testTime = bTestEndingTime ? pSubClip->GetEndTime() : pSubClip->GetStartTime();
		if ( endTime > testTime )
		{
			DmeTime_t startTime = pSubClip->GetStartTime();
			pSubClip->SetStartTime( startTime + dt );
		}
	}
}


//-----------------------------------------------------------------------------
// A version that works only on film clips
//-----------------------------------------------------------------------------
void CDmeTrack::ShiftAllFilmClipsAfter( CDmeClip *pClip, DmeTime_t dt, bool bShiftClip )
{
	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) || ( dt == DmeTime_t( 0 ) ) )
		return;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = c; --i >= 0; )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( pSubClip == pClip )
		{
			if ( bShiftClip )
			{
				pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
			}
			return;
		}
		pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
	}

	// Clip wasn't found!
	Assert( 0 );
}

void CDmeTrack::ShiftAllFilmClipsBefore( CDmeClip *pClip, DmeTime_t dt, bool bShiftClip )
{
	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) || ( dt == DmeTime_t( 0 ) ) )
		return;
	 
	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );

		if ( pSubClip == pClip )
		{
			if ( bShiftClip )
			{
				pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
			}
			return;
		}
		pSubClip->SetStartTime( pSubClip->GetStartTime() + dt );
	}

	// Clip wasn't found!
	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Method to sort clips	by start time
//-----------------------------------------------------------------------------
struct SortInfo_t
{
	DmeTime_t m_startTime;
	CDmeClip *m_pClip;
};

static int ClipStartLessFunc( const void * lhs, const void * rhs )
{
	SortInfo_t *pInfo1 = (SortInfo_t*)lhs;
	SortInfo_t *pInfo2 = (SortInfo_t*)rhs;
	if ( pInfo1->m_startTime == pInfo2->m_startTime )
		return 0;

	return pInfo1->m_startTime < pInfo2->m_startTime ? -1 : 1;
}

void CDmeTrack::SortClipsByStartTime( )
{
	// If we're not a film clip, then we haven't installed callbacks to make sorting fast.
	// The IS_SORTED flag is some random state
	if ( (m_ClipType == DMECLIP_FILM) && m_Flags.IsFlagSet( IS_SORTED ) )
		return;
	m_Flags.SetFlag( IS_SORTED );

	int c = GetClipCount();
	if ( c <= 1 )
		return;

	DmeTime_t lastTime;
	SortInfo_t *pSortInfo = (SortInfo_t*)_alloca( c * sizeof(SortInfo_t) );
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip(i);
		pSortInfo[i].m_startTime = pSubClip ? pSubClip->GetStartTime() : DmeTime_t::InvalidTime();
		pSortInfo[i].m_pClip = pSubClip;
		if ( lastTime > pSortInfo[i].m_startTime )
		{
			m_Flags.ClearFlag( IS_SORTED );
		}
		lastTime = pSortInfo[i].m_startTime;
	}
	if ( m_Flags.IsFlagSet( IS_SORTED ) )
		return;

	m_Flags.SetFlag( IS_SORTED );
	qsort( pSortInfo, c, sizeof(SortInfo_t), ClipStartLessFunc );

	CSuppressAutoFixup suppress( this, SUPPRESS_OVERLAP_FIXUP | SUPPRESS_DIRTY_ORDERING );

	m_Clips.RemoveAll();

	for ( int i = 0; i < c; ++i )
	{
		m_Clips.AddToTail( pSortInfo[i].m_pClip );
	}
}


//-----------------------------------------------------------------------------
// Shifts all clips to be non-overlapping
//-----------------------------------------------------------------------------
void CDmeTrack::FixOverlaps()
{
	int c = GetClipCount();
	if ( c <= 1 )
		return;

	SortClipsByStartTime();

	CSuppressAutoFixup suppress( this, SUPPRESS_OVERLAP_FIXUP | SUPPRESS_DIRTY_ORDERING );

	// Cull NULL clips
	int nActualCount = 0;
	CDmeClip **pClips = (CDmeClip**)_alloca( c * sizeof(CDmeClip*) );
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pCurr = GetClip( i );
		if ( pCurr && ((i == 0) || (pClips[i-1] != pCurr)) )
		{
			pClips[nActualCount++] = pCurr;
		}
	}

	if ( nActualCount <= 1 )
		return;

	CDmeClip *pPrev = pClips[0];
	for ( int i = 1; i < nActualCount; ++i )
	{
		CDmeClip *pCurr = pClips[i];

		DmeTime_t prevEndTime = pPrev->GetEndTime();
		DmeTime_t startTime = pCurr->GetStartTime();

		if ( startTime < prevEndTime )
		{
			pCurr->SetStartTime( prevEndTime );
		}

		pPrev = pCurr;
	}
}


//-----------------------------------------------------------------------------
// Finds a clip at a particular time
//-----------------------------------------------------------------------------
CDmeClip* CDmeTrack::FindFilmClipAtTime( DmeTime_t localTime )
{
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return NULL;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( pSubClip && pSubClip->GetStartTime() <= localTime && pSubClip->GetEndTime() > localTime )
			return pSubClip;
	}

	return NULL;
}

	
//-----------------------------------------------------------------------------
// Find first clip in a specific time range
//-----------------------------------------------------------------------------
CDmeClip* CDmeTrack::FindFirstFilmClipIntesectingTime( DmeTime_t localStartTime, DmeTime_t localEndTime )
{
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return NULL;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( !pSubClip )
			continue;
		if ( ( localStartTime < pSubClip->GetEndTime() ) && ( localEndTime >= pSubClip->GetStartTime() ) )
			return static_cast<CDmeFilmClip*>( pSubClip );
		if ( localEndTime <= pSubClip->GetStartTime() )
			break;
	}

	return NULL;
}

		
//-----------------------------------------------------------------------------
// Inserts space in a film track for a film clip
//-----------------------------------------------------------------------------
void CDmeTrack::InsertSpaceInFilmTrack( DmeTime_t localStartTime, DmeTime_t localEndTime )
{
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	CDmeClip *pClip	= FindFirstFilmClipIntesectingTime( localStartTime, localEndTime );
	if ( pClip )
	{
		DmeTime_t filmStart = pClip->GetStartTime();
		DmeTime_t dt = localEndTime - filmStart;
		ShiftAllFilmClipsAfter( pClip, dt, true ); 
	}

	return;
}


//-----------------------------------------------------------------------------
// Returns the next/previous clip in a film track
//-----------------------------------------------------------------------------
CDmeClip* CDmeTrack::FindPrevFilmClip( CDmeClip *pClip )
{
	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return NULL;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	if ( !pClip )
		return m_Clips[ m_Clips.Count() - 1 ];

	// FIXME: Could use a binary search here based on time.
	// Probably doesn't matter though, since there will usually not be a ton of tracks
	CDmeClip *pPrevClip = NULL;

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( pSubClip == pClip )
			return pPrevClip;

		pPrevClip = pSubClip;
	}

	return NULL;
}

CDmeClip* CDmeTrack::FindNextFilmClip( CDmeClip *pClip )
{
	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return NULL;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	if ( !pClip )
		return m_Clips[ 0 ]; 

	CDmeClip *pNextClip = NULL;

	int c = GetClipCount();
	for ( int i = c; --i >= 0; )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( pSubClip == pClip )
			return pNextClip;

		pNextClip = pSubClip;
	}

	return NULL;
}


void CDmeTrack::FindAdjacentFilmClips( CDmeClip *pClip, CDmeClip *&pPrevClip, CDmeClip *&pNextClip )
{
	pPrevClip = pNextClip = NULL;

	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || !pClip || ( m_Clips.Count() == 0 ) )
		return;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( pSubClip == pClip )
		{
			pNextClip = ( i != c-1 ) ? GetClip( i+1 ) : NULL;
			return;
		}

		pPrevClip = pSubClip;
	}

	pPrevClip = NULL;
}


//-----------------------------------------------------------------------------
// Gets the start/end time of the owning clip in local time
//-----------------------------------------------------------------------------
void CDmeTrack::FindAdjacentFilmClips( DmeTime_t localTime, CDmeClip *&pPrevClip, CDmeClip *&pNextClip )
{
	pPrevClip = pNextClip = NULL;

	Assert( IsFilmTrack() );
	if ( !IsFilmTrack() || ( m_Clips.Count() == 0 ) )
		return;

	// This algorithm requires sorted clips
	SortClipsByStartTime();

	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pSubClip = GetClip( i );
		if ( localTime >= pSubClip->GetEndTime() )
		{
			pPrevClip = pSubClip;
		}
		if ( localTime < pSubClip->GetStartTime() )
		{
			pNextClip = pSubClip;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Fills all gaps in a film track with slugs
//-----------------------------------------------------------------------------
void CDmeTrack::FillAllGapsWithSlugs( const char *pSlugName, DmeTime_t startTime, DmeTime_t endTime )
{
	if ( !IsFilmTrack() )
		return;

	FixOverlaps();

	// Create temporary slugs to fill in the gaps
	bool bSlugAdded = false;
	int c = GetClipCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeClip *pFilmClip = GetClip(i);
		DmeTime_t clipStartTime = pFilmClip->GetStartTime();
		if ( clipStartTime > startTime )
		{
			// There's a gap, create a slug
			CDmeFilmClip *pSlug = CreateSlugClip( pSlugName, startTime, clipStartTime, GetFileId() );

			// This will add the slug to the end; so we don't have to
			// worry about iterating over it (we've cached off the initial count)
			AddClip( pSlug );

			bSlugAdded = true;
		}
		startTime = pFilmClip->GetEndTime();
	}

	if ( endTime > startTime )
	{
		// There's a gap, create a temporary slug
		CDmeFilmClip *pSlug = CreateSlugClip( pSlugName, startTime, endTime, GetFileId() );

		// This will add the slug to the end; so we don't have to
		// worry about iterating over it (we've cached off the initial count)
		AddClip( pSlug );

		bSlugAdded = true;
	}

	if ( bSlugAdded )
	{
		FixOverlaps();
	}
}

//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeTrackGroup *GetParentTrackGroup( CDmeTrack *pTrack )
{
	DmAttributeReferenceIterator_t hAttr = g_pDataModel->FirstAttributeReferencingElement( pTrack->GetHandle() );
	for ( ; hAttr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; hAttr = g_pDataModel->NextAttributeReferencingElement( hAttr ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( hAttr );
		if ( !pAttr )
			continue;

		CDmeTrackGroup *pTrackGroup = CastElement< CDmeTrackGroup >( pAttr->GetOwner() );
		if ( pTrackGroup )
			return pTrackGroup;
	}
	return NULL;
}
