//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
#ifndef TIER_V0PROF_SN_HDR
#define TIER_V0PROF_SN_HDR

// enable this to get detailed SN Tuner markers. PS3 specific
#if defined( SN_TARGET_PS3 ) && !defined(_CERT)
#define VPROF_SN_LEVEL 0

extern "C" void(*g_pfnPushMarker)( const char * pName );
extern "C" void(*g_pfnPopMarker)();

class CVProfSnMarkerScope
{
public:
	CVProfSnMarkerScope( const char * pszName )
	{
		g_pfnPushMarker( pszName );
	}
	~CVProfSnMarkerScope()
	{
		 g_pfnPopMarker( );
	}
};

#else

class CVProfSnMarkerScope  { public: CVProfSnMarkerScope( const char * ) {} };

#endif

#endif