//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: D3DX command implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef D3DXFXC_H
#define D3DXFXC_H
#ifdef _WIN32
#pragma once
#endif

#include "cmdsink.h"

namespace InterceptFxc
{

	bool TryExecuteCommand( const char *pCommand, CmdSink::IResponse **ppResponse );

}; // namespace InterceptFxc

#endif // #ifndef D3DXFXC_H
