//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Allows matching of initialization and shutdown function calls
//
// $NoKeywords: $
//=============================================================================//
#if !defined( TRACEINIT_H )
#define TRACEINIT_H
#ifdef _WIN32
#pragma once
#endif

void TraceInit( const char *i, const char *s, int list );
#ifdef _XBOX
void TraceInitFinish( const char *i );
#endif
void TraceShutdown( const char *s, int list );

#ifndef _XBOX
#define TRACEINITNUM( initfunc, shutdownfunc, num )	\
	TraceInit( #initfunc, #shutdownfunc, num );		\
	initfunc;
#else
#define TRACEINITNUM( initfunc, shutdownfunc, num )	\
	TraceInit( #initfunc, #shutdownfunc, num );		\
	initfunc;										\
	TraceInitFinish( #initfunc );
#endif

#define TRACESHUTDOWNNUM( shutdownfunc, num )	\
	TraceShutdown( #shutdownfunc, num );		\
	shutdownfunc;

#define TRACEINIT( initfunc, shutdownfunc )	TRACEINITNUM( initfunc, shutdownfunc, 0 )
#define TRACESHUTDOWN( shutdownfunc ) TRACESHUTDOWNNUM( shutdownfunc, 0 )

#endif // TRACEINIT_H
