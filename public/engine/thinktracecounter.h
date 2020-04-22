//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Some macros for the raytraces-in-think-function-counter. 
//          They're in a header because they're included in a bunch of 
//          places, but on some cases they need to define files and in 
//          others only extern them. 
//
//=============================================================================//

#ifndef THINK_TRACE_COUNTER_H
#define THINK_TRACE_COUNTER_H
#ifdef _WIN32
#pragma once
#endif

#define THINK_TRACE_COUNTER_COMPILED 1 // without this, all the code is elided.


#ifdef THINK_TRACE_COUNTER_COMPILED
	// create a macro that is true if we are allowed to debug traces during thinks, and compiles out to nothing otherwise.
	#if defined( _GAMECONSOLE ) || defined( NO_STEAM )
		#define DEBUG_THINK_TRACE_COUNTER_ALLOWED()  (false)
	#else
		#ifdef THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_ENGINE
			bool DEBUG_THINK_TRACE_COUNTER_ALLOWED()
			{
				// done as a static var to defer initialization until Steam is ready,
				// but also to have the fastest check at runtime (rather than calling through
				// the API each time)
				static bool bIsPublic = GetSteamUniverse() == k_EUniversePublic;
				return !bIsPublic;
			}
		#elif defined( THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_SERVER )
			bool DEBUG_THINK_TRACE_COUNTER_ALLOWED()
			{
				// done as a static var to defer initialization until Steam is ready,
				// but also to have the fastest check at runtime (rather than calling through
				// the API each time)
				static bool bIsPublic = steamapicontext->SteamUtils() != NULL && steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniversePublic;
				return !bIsPublic;
			}
		#else
			extern bool DEBUG_THINK_TRACE_COUNTER_ALLOWED();
		#endif
	#endif
#endif


#endif // THINK_TRACE_COUNTER_H
