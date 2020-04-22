//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmetrackgroup.h"

#include <limits.h>
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmeclip.h"

#include "movieobjects_interfaces.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// CDmeTrackGroup - contains a list of tracks
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTrackGroup, CDmeTrackGroup );

void CDmeTrackGroup::OnConstruction()
{
	m_hOwner = DMELEMENT_HANDLE_INVALID;

	m_Tracks.Init( this, "tracks", FATTRIB_MUSTCOPY | FATTRIB_HAS_ARRAY_CALLBACK );
	m_bIsVisible.InitAndSet( this, "visible", true );
	m_bMute.Init( this, "mute" );
	m_nDisplaySize.InitAndSet( this, "displaySize", 110 );
	m_bMinimized.InitAndSet( this, "minimized", true );
	m_nMaxTrackCount = INT_MAX;
	m_Volume.InitAndSet( this, "volume", 1.0 );

}

void CDmeTrackGroup::OnDestruction()
{
	// NOTE: The track owner handles may still be pointing to us when we get destructed,
	// but their handles will be invalid, so GetTrackGroup on a track
	// will correctly return NULL. 
}


//-----------------------------------------------------------------------------
// Max track count
//-----------------------------------------------------------------------------
void CDmeTrackGroup::SetMaxTrackCount( int nCount )
{
	m_nMaxTrackCount = nCount;
}


//-----------------------------------------------------------------------------
// Mute
//-----------------------------------------------------------------------------
void CDmeTrackGroup::SetMute( bool state )
{
	m_bMute = state;
}

bool CDmeTrackGroup::IsMute( ) const
{
	return m_bMute;
}

//-----------------------------------------------------------------------------
// Volume
//-----------------------------------------------------------------------------
void CDmeTrackGroup::SetVolume( float state )
{
	m_Volume = state;
}
float CDmeTrackGroup::GetVolume() const
{
	return m_Volume.Get();
}

//-----------------------------------------------------------------------------
// Owning clip
//-----------------------------------------------------------------------------
CDmeClip *CDmeTrackGroup::GetOwnerClip()
{
	return GetElement< CDmeClip >( m_hOwner );
}

void CDmeTrackGroup::SetOwnerClip( CDmeClip *pClip )
{
	m_hOwner = pClip ? pClip->GetHandle() : DMELEMENT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Are we a film track group?
//-----------------------------------------------------------------------------
bool CDmeTrackGroup::IsFilmTrackGroup()
{
	CDmeClip *pOwnerClip = GetOwnerClip();
	if ( pOwnerClip ) 
		return pOwnerClip->GetFilmTrackGroup() == this;
	return m_nMaxTrackCount == 1;
}


//-----------------------------------------------------------------------------
// Is a particular clip typed able to be added?
//-----------------------------------------------------------------------------
bool CDmeTrackGroup::IsSubClipTypeAllowed( DmeClipType_t type )
{
	if ( IsFilmTrackGroup() )
	{
		if ( type != DMECLIP_FILM )
			return false;
	}
	else
	{
		if ( type == DMECLIP_FILM )
			return false;
	}

	CDmeClip *pOwnerClip = GetOwnerClip();
	Assert( pOwnerClip );
	if ( !pOwnerClip )
		return true;

	return pOwnerClip->IsSubClipTypeAllowed( type );
}


//-----------------------------------------------------------------------------
// Track addition/removal
//-----------------------------------------------------------------------------
void CDmeTrackGroup::AddTrack( CDmeTrack *pTrack )
{
	// FIXME:  Should check if track with same name already exists???
	if ( GetTrackIndex( pTrack ) < 0 )
	{
		// Tracks can only exist in one track group
		Assert( GetTrackIndex( pTrack ) >= 0 );
		m_Tracks.AddToTail( pTrack );
		Assert( m_nMaxTrackCount >= m_Tracks.Count() );
	}
}

CDmeTrack* CDmeTrackGroup::AddTrack( const char *pTrackName, DmeClipType_t trackType )
{
	CDmeTrack *pTrack = CreateElement< CDmeTrack >( pTrackName, GetFileId() );
	pTrack->SetClipType( trackType );
	pTrack->SetCollapsed( false );
	m_Tracks.AddToTail( pTrack );
	Assert( m_nMaxTrackCount >= m_Tracks.Count() );
	return pTrack;
}

CDmeTrack* CDmeTrackGroup::FindOrAddTrack( const char *pTrackName, DmeClipType_t trackType )
{
	CDmeTrack *pTrack = FindTrack( pTrackName );
	if ( pTrack )
	{
		// If we found it, but it's the wrong type, no dice
		if ( pTrack->GetClipType() != trackType )
			return NULL;
	}
	else
	{
		pTrack = AddTrack( pTrackName, trackType );
	}
	return pTrack;
}

void CDmeTrackGroup::RemoveTrack( int nIndex )
{
	m_Tracks.Remove( nIndex );
}

void CDmeTrackGroup::RemoveTrack( CDmeTrack *pTrack )
{
	int i = GetTrackIndex( pTrack );
	if ( i >= 0 )
	{
		m_Tracks.Remove( i );
	}
}

void CDmeTrackGroup::RemoveTrack( const char *pTrackName )
{
	if ( !pTrackName )
	{
		pTrackName = DMETRACK_DEFAULT_NAME;
	}

	int c = m_Tracks.Count();
	for ( int i = c; --i >= 0; )
	{
		if ( !Q_strcmp( m_Tracks[i]->GetName(), pTrackName ) )
		{
			m_Tracks.Remove( i );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Track finding
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::FindTrack( const char *pTrackName ) const
{
	if ( !pTrackName )
	{
		pTrackName = DMETRACK_DEFAULT_NAME;
	}

	int c = m_Tracks.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeTrack *pTrack = m_Tracks[i];
		if ( !pTrack )
			continue;

		if ( !Q_strcmp( pTrack->GetName(), pTrackName ) )
			return pTrack;
	}
	return NULL;
}

int CDmeTrackGroup::GetTrackIndex( CDmeTrack *pTrack ) const
{
	int nTracks = m_Tracks.Count();
	for ( int i = 0 ; i < nTracks; ++i )
	{
		if ( pTrack == m_Tracks[i] )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Creates the film track group [for internal use only]
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::CreateFilmTrack()
{
	Assert( GetTrackCount() == 0 );
	return AddTrack( "Film", DMECLIP_FILM );
}


//-----------------------------------------------------------------------------
// Returns the film track, if any
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::GetFilmTrack()
{
	if ( !IsFilmTrackGroup() )
		return NULL;

	if ( GetTrackCount() > 0 )
	{
		Assert( GetTrackCount() == 1 );
		return m_Tracks[0];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Adding/removing clips from tracks
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::AddClip( CDmeClip *pClip, const char *pTrackName )
{
	DmeClipType_t type = pClip->GetClipType();
	if ( !pTrackName )
	{
		pTrackName = DMETRACK_DEFAULT_NAME;
	}

	CDmeTrack *pTrack = FindOrAddTrack( pTrackName, type );
	if ( pTrack )
	{
		pTrack->AddClip( pClip );
	}
	return pTrack;
}

bool CDmeTrackGroup::RemoveClip( CDmeClip *pClip )
{
	CDmeTrack *pTrack = FindTrackForClip( pClip );
	if ( pTrack )
		return pTrack->RemoveClip( pClip );
	return false;
}


//-----------------------------------------------------------------------------
// Changing clip track
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::ChangeTrack( CDmeClip *pClip, const char *pNewTrack )
{
	// Add, then remove, to avoid refcount problems
	// Don't remove if it wasn't added for some reason.
	CDmeTrack *pOldTrack = FindTrackForClip( pClip );
	CDmeTrack *pTrack = AddClip( pClip, pNewTrack );
	if ( pTrack && pOldTrack )
	{
		pOldTrack->RemoveClip( pClip );
	}
	return pTrack;
}


//-----------------------------------------------------------------------------
// Finding clips in tracks
//-----------------------------------------------------------------------------
CDmeTrack *CDmeTrackGroup::FindTrackForClip( CDmeClip *pClip ) const
{
	int nTrackIndex = -1;
	if ( !FindTrackForClip( pClip, &nTrackIndex, NULL ) )
		return NULL;

	return GetTrack( nTrackIndex );
}


bool CDmeTrackGroup::FindTrackForClip( CDmeClip *pClip, int *pTrackIndex, int *pClipIndex ) const
{
	DmeClipType_t type = pClip->GetClipType();
	int c = GetTrackCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeTrack *pTrack = GetTrack( i );
		if ( !pTrack )
			continue;

		if ( pTrack->GetClipType() != type )
			continue;

		int nClipCount = pTrack->GetClipCount();
		for ( int j = 0; j < nClipCount; ++j )
		{
			if ( pTrack->GetClip( j ) == pClip )
			{
				if ( pTrackIndex )
				{
					*pTrackIndex = i;
				}
				if ( pClipIndex )
				{
					*pClipIndex = j;
				}
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Finding clips in tracks by time
//-----------------------------------------------------------------------------
void CDmeTrackGroup::FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && ( !IsVisible() || IsMinimized() ) )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int c = GetTrackCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeTrack *pTrack = GetTrack( i );
		if ( !pTrack )
			continue;

		if ( ( clipType != DMECLIP_UNKNOWN ) && ( pTrack->GetClipType() != clipType ) )
			continue;

		pTrack->FindClipsAtTime( time, flags, clips );
	}
}


void CDmeTrackGroup::FindClipsIntersectingTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && ( !IsVisible() || IsMinimized() ) )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int c = GetTrackCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeTrack *pTrack = GetTrack( i );
		if ( !pTrack )
			continue;

		if ( ( clipType != DMECLIP_UNKNOWN ) && ( pTrack->GetClipType() != clipType ) )
			continue;

		pTrack->FindClipsIntersectingTime( startTime, endTime, flags, clips );
	}
}


void CDmeTrackGroup::FindClipsWithinTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( flags & DMESKIP_INVISIBLE ) && ( !IsVisible() || IsMinimized() ) )
		return;

	if ( ( flags & DMESKIP_MUTED ) && IsMute() )
		return;

	int c = GetTrackCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeTrack *pTrack = GetTrack( i );
		if ( !pTrack )
			continue;

		if ( ( clipType != DMECLIP_UNKNOWN ) && ( pTrack->GetClipType() != clipType ) )
			continue;

		pTrack->FindClipsWithinTime( startTime, endTime, flags, clips );
	}
}


//-----------------------------------------------------------------------------
// Removes empty tracks
//-----------------------------------------------------------------------------
void CDmeTrackGroup::RemoveEmptyTracks()
{
	int tc = GetTrackCount();
	for ( int i = tc; --i >= 0; )
	{
		CDmeTrack *pTrack = GetTrack( i );
		if ( pTrack->GetClipCount() == 0 )
		{
			RemoveTrack( i );
		}
	}
}

	
//-----------------------------------------------------------------------------
// Sort tracks by track type, then alphabetically
//-----------------------------------------------------------------------------
static int TrackLessFunc( const void * lhs, const void * rhs )
{
	CDmeTrack *pInfo1 = *(CDmeTrack**)lhs;
	CDmeTrack *pInfo2 = *(CDmeTrack**)rhs;
	if ( pInfo1->GetClipType() < pInfo2->GetClipType() )
		return -1;
 	if ( pInfo1->GetClipType() > pInfo2->GetClipType() )
		return 1;
	return Q_strcmp( pInfo1->GetName(), pInfo2->GetName() );
}

void CDmeTrackGroup::SortTracksByType()
{
	int tc = GetTrackCount();
	if ( tc == 0 )
		return;

	CDmeTrack **ppTrack = (CDmeTrack**)_alloca( tc * sizeof(CDmeTrack*) );
	for ( int i = 0; i < tc; ++i )
	{
		ppTrack[i] = GetTrack(i);
	}

	qsort( ppTrack, tc, sizeof(CDmeTrack*), TrackLessFunc );

	m_Tracks.RemoveAll();

	for ( int i = 0; i < tc; ++i )
	{
		m_Tracks.AddToTail( ppTrack[i] );
	}
}


//-----------------------------------------------------------------------------
// Returns the flattened clip count
//-----------------------------------------------------------------------------
int CDmeTrackGroup::GetSubClipCount() const
{
	int nCount = 0;
	DMETRACKGROUP_FOREACH_CLIP_START( this, pTrack, pClip )
		++nCount;
	DMETRACKGROUP_FOREACH_CLIP_END()
	return nCount;
}

void CDmeTrackGroup::GetSubClips( CDmeClip **ppClips )
{
	int nCount = 0;
	DMETRACKGROUP_FOREACH_CLIP_START( this, pTrack, pClip )
		ppClips[nCount++] = pClip;
	DMETRACKGROUP_FOREACH_CLIP_END()
}

//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeFilmClip *GetParentClip( CDmeTrackGroup *pTrackGroup )
{
	DmAttributeReferenceIterator_t hAttr = g_pDataModel->FirstAttributeReferencingElement( pTrackGroup->GetHandle() );
	for ( ; hAttr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; hAttr = g_pDataModel->NextAttributeReferencingElement( hAttr ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( hAttr );
		if ( !pAttr )
			continue;

		CDmeFilmClip *pFilmClip = CastElement< CDmeFilmClip >( pAttr->GetOwner() );
		if ( pFilmClip )
			return pFilmClip;
	}
	return NULL;
}
