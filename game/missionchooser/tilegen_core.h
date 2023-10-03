//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Core definitions required by tilegen code.
//
//===============================================================================

#ifndef TILEGEN_CORE_H
#define TILEGEN_CORE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////
static const int MAX_TILEGEN_IDENTIFIER_LENGTH = 64;

DECLARE_LOGGING_CHANNEL( LOG_TilegenLayoutSystem );
DECLARE_LOGGING_CHANNEL( LOG_TilegenGeneral );

#endif // TILEGEN_CORE_H