//========= Copyright Valve Corporation, All rights reserved. ============//
//=======================================================================================//

#if defined( REPLAY_ENABLED )

#ifndef CS_REPLAY_H
#define CS_REPLAY_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/genericclassbased_replay.h"

//----------------------------------------------------------------------------------------

class CCSReplay : public CGenericClassBasedReplay
{
	typedef CGenericClassBasedReplay BaseClass;
public:
	CCSReplay();
	~CCSReplay();

	virtual void OnBeginRecording();
	virtual void OnEndRecording();
	virtual void OnComplete();
	virtual void FireGameEvent( IGameEvent *pEvent );

	virtual bool Read( KeyValues *pIn );
	virtual void Write( KeyValues *pOut );

	virtual void DumpGameSpecificData() const;

	virtual const char *GetPlayerClass() const { return GetCSClassInfo( m_nPlayerClass )->m_pClassName; }
	virtual const char *GetPlayerTeam() const { return m_nPlayerTeam == TEAM_TERRORIST ? "terrorists" : "counterterrorists"; }
	virtual const char *GetMaterialFriendlyPlayerClass() const;

private:
	virtual void Update();
	float GetSentryKillScreenshotDelay();

	virtual bool IsValidClass( int nClass ) const;
	virtual bool IsValidTeam( int iTeam ) const;
	virtual bool GetCurrentStats( RoundStats_t &out );
	virtual const char *GetStatString( int iStat ) const;
	virtual const char *GetPlayerClass( int iClass ) const;
};

//----------------------------------------------------------------------------------------

inline CCSReplay *ToCSReplay( CReplay *pClientReplay )
{
	return static_cast< CCSReplay * >( pClientReplay );
}

inline const CCSReplay *ToCSReplay( const CReplay *pClientReplay )
{
	return static_cast< const CCSReplay * >( pClientReplay );
}

inline CCSReplay *GetCSReplay( ReplayHandle_t hReplay )
{
	return ToCSReplay( g_pClientReplayContext->GetReplay( hReplay ) );
}

//----------------------------------------------------------------------------------------

#endif	// CS_REPLAY_H

#endif