//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef VOICE_COMMON_H
#define VOICE_COMMON_H
#ifdef _WIN32
#pragma once
#endif


#include "bitvec.h"
#include "const.h"


#define VOICE_MAX_PLAYERS		MAX_PLAYERS
#define VOICE_MAX_PLAYERS_DW	((VOICE_MAX_PLAYERS / 32) + !!(VOICE_MAX_PLAYERS & 31))
#define VOICE_DEFAULT_PROXIMITY_RANGE 1200 //100 feet


#endif // VOICE_COMMON_H
