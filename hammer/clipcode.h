//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CLIPCODE_H
#define CLIPCODE_H
#pragma once


#include "hammer_mathlib.h"


class CMapFace;
class IEditorTexture;


#define VERTEXSIZE		5
#define MAX_CLIPVERT	64


int CreateClippedPoly(CMapFace *pFace, IEditorTexture *pDecalTex, Vector& org, vec5_t *pOutPoints, int nOutSize);

#endif // CLIPCODE_H
