//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RENDERUTILS_H
#define RENDERUTILS_H
#ifdef _WIN32
#pragma once
#endif


class CRender2D;

//
// Flags for DrawBoundsText
//
#define DBT_TOP		0x1
#define DBT_BOTTOM	0x2
#define DBT_LEFT	0x4
#define DBT_RIGHT	0x8
#define DBT_BACK    0x10


void DrawBoundsText(CRender2D *pRender, const Vector &Mins, const Vector &Maxs, int nFlags);


#endif // RENDERUTILS_H
