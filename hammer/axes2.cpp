//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Axes2.h"
#include "mathlib/vector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Axes2::SetAxes(int h, bool bInvertH, int v, bool bInvertV)
{
	bInvertHorz = bInvertH;
	bInvertVert = bInvertV;

	axHorz = h;
	axVert = v;

	if(h != AXIS_X && v != AXIS_X)
		axThird = AXIS_X;
	if(h != AXIS_Y && v != AXIS_Y)
		axThird = AXIS_Y;
	if(h != AXIS_Z && v != AXIS_Z)
		axThird = AXIS_Z;
}

void Axes2::SetAxes(Axes2 &axes)
{
	*this = axes;
}





