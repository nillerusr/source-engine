//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/basereplayserializeable.h"
#include "replay/replayutils.h"
#include "KeyValues.h"
#include "tier1/strtools.h"
#include "replay/shared_defs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CBaseReplaySerializeable::CBaseReplaySerializeable()
:	m_hThis( REPLAY_HANDLE_INVALID ),
	m_bLocked( false )
{
}

void CBaseReplaySerializeable::SetHandle( ReplayHandle_t h )
{
	m_hThis = h;
}

ReplayHandle_t CBaseReplaySerializeable::GetHandle() const
{
	return m_hThis;
}

bool CBaseReplaySerializeable::Read( KeyValues *pIn )
{
	m_hThis = (ReplayHandle_t)pIn->GetInt( "handle" );

	return true;
}

void CBaseReplaySerializeable::Write( KeyValues *pOut )
{
	pOut->SetInt( "handle", (int)m_hThis );
}

const char *CBaseReplaySerializeable::GetFullFilename() const
{
	const char *pPath = GetPath();
	const char *pFilename = GetFilename();

	if ( !pPath || !pPath[0] || !pFilename || !pFilename[0] )
		return NULL;

	return Replay_va( "%s%s", pPath, pFilename );
}

const char *CBaseReplaySerializeable::GetFilename() const
{
	return Replay_va( "%s.%s", GetSubKeyTitle(), GENERIC_FILE_EXTENSION );
}

const char *CBaseReplaySerializeable::GetDebugName() const
{
	return GetSubKeyTitle();
}

void CBaseReplaySerializeable::SetLocked( bool bLocked )
{
	m_bLocked = bLocked;
}

bool CBaseReplaySerializeable::IsLocked() const
{
	return m_bLocked;
}

void CBaseReplaySerializeable::OnDelete()
{
}

void CBaseReplaySerializeable::OnUnload()
{
}

void CBaseReplaySerializeable::OnAddedToDirtyList()
{
}

//----------------------------------------------------------------------------------------