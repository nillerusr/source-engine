//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ASW_AMBIENTGENERIC_H
#define ASW_AMBIENTGENERIC_H
#ifdef _WIN32
#pragma once
#endif

#include "ambientgeneric.h"

class CASW_AmbientGeneric : public CAmbientGeneric
{
public:
	DECLARE_CLASS( CASW_AmbientGeneric, CAmbientGeneric );
	DECLARE_DATADESC();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Activate( void );
	virtual void ToggleSound( void );
	virtual void SendSound( SoundFlags_t flags );

	float m_fPreventStimMusicDuration;
	bool m_bStartedNonLoopingSound;
};

#endif // ASW_AMBIENTGENERIC_H