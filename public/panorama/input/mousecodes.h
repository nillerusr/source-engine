//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef MOUSECODES_H
#define MOUSECODES_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama
{

enum MouseCode
{
	MOUSE_INVALID = -1,
	MOUSE_LEFT = 0,
	MOUSE_RIGHT,
	MOUSE_MIDDLE,
	MOUSE_4,
	MOUSE_5,
	MOUSE_LAST,
};

} // namespace panorama

#endif // MOUSECODES_H