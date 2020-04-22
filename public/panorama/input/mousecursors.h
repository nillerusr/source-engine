//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef MOUSECURSORS_H
#define MOUSECURSORS_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama
{

enum EMouseCursors
{
	eMouseCursor_None = 0,
	eMouseCursor_Arrow,
	eMouseCursor_IBeam,
	eMouseCursor_SizeWE,
	eMouseCursor_SizeNS,
	eMouseCursor_Last,
};

} // namespace panorama

#endif // MOUSECURSORS_H
