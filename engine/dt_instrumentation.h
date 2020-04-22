//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_INSTRUMENTATION_H
#define DATATABLE_INSTRUMENTATION_H
#ifdef _WIN32
#pragma once
#endif

#include "dt_recv_eng.h"
#include "dt_encode.h"


// Is instrumentation enabled?
extern bool g_bDTIEnabled;


// ------------------------------------------------------------------------------------------ // 
// Instrumentation functions.
// ------------------------------------------------------------------------------------------ // 

// This is called at startup to enable instrumentation.
void DTI_Init();

// Calls DTI_Flush and cleans up.
void DTI_Term();

// This writes out the instrumentation file.
void DTI_Flush();

// Setup instrumentation on a CRecvDecoder.
void DTI_HookRecvDecoder( CRecvDecoder *pDecoder );

// Notify the instrumentation that a delta bit has been read.
void DTI_HookDeltaBits( CRecvDecoder *pDecoder, int iProp, int nDataBits, int nIndexBits );



// ------------------------------------------------------------------------------------------ // 
// Inlines.
// ------------------------------------------------------------------------------------------ // 

inline void DTI_HookDeltaBits( CRecvDecoder *pDecoder, int iProp, int nDataBits, int nIndexBits )
{
	if( g_bDTIEnabled )
	{
		extern void _DTI_HookDeltaBits( CRecvDecoder *pDecoder, int iProp, int nDataBits, int nIndexBits );
		_DTI_HookDeltaBits( pDecoder, iProp, nDataBits, nIndexBits );
	}
}

#endif // DATATABLE_INSTRUMENTATION_H
