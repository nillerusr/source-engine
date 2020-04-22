//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_CHECK_PROCESS_H
#define CL_CHECK_PROCESS_H
#ifdef _WIN32
#pragma once
#endif

#include "platform.h"

#define CHECK_PROCESS_UNSUPPORTED		-1


int CheckOtherInstancesRunning( void );


#endif // CL_CHECK_PROCESS_H
