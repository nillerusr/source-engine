//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface for rendering in the 2D views.
//
// $NoKeywords: $
//=============================================================================//

#ifndef RENDER2D_H
#define RENDER2D_H
#ifdef _WIN32
#pragma once
#endif

#include "Render.h"

class CRender2D : public CRender
{
public:

   

    //
    // construction/deconstruction
    //
    CRender2D();
    ~CRender2D();

    //
    // setup (view) data
    //

    void MoveTo( const Vector &vPoint );
	void DrawLineTo( const Vector &vPoint );
	void DrawRectangle( const Vector &vMins, const Vector &vMaxs, bool bFill = false, int extent = 0 );
	void DrawBox( const Vector &vMins, const Vector &vMaxs, bool bFill = false );
	void DrawCircle( const Vector &vCenter, float fRadius );

protected:

	Vector m_vCurLine;
};


#endif // RENDER2D_H
