//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Spew free memory at a fixed rate, for logging
//
//=============================================================================//

#ifndef C_MEMORYLOG_H
#define C_MEMORYLOG_H

#if ( defined( _X360 ) && !defined( _CERT ) )

#include "igamesystem.h"

class C_MemoryLog : public CAutoGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual bool Init( void );
	virtual void Update( float frametime );
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

private:
	void	Spew( void );

	float	m_fLastSpewTime;
	int		m_nRecentFreeMem[ 256 ];	// For inspection in full heap crashdumps
};

#endif // ( defined( _X360 ) && !defined( _CERT ) )

#endif // C_MEMORYLOG_H
