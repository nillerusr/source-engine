//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Render2D.h"
#include "gameconfig.h"
#include "vgui_controls/Controls.h"
#include "HammerVGui.h"
#include "material.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include "mapview2d.h"
#include "camera.h"
#include "vgui/IScheme.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: constructor - initialize all the member variables
//-----------------------------------------------------------------------------
CRender2D::CRender2D()
{
	m_vCurLine.Init();
}


//-----------------------------------------------------------------------------
// Purpose: deconstructor - free all info that may need to be freed
//-----------------------------------------------------------------------------
CRender2D::~CRender2D()
{
}


void CRender2D::MoveTo( const Vector	&vPoint	)
{
	m_vCurLine = vPoint;
}

void CRender2D::DrawLineTo( const Vector	&vPoint )
{
	DrawLine( m_vCurLine, vPoint );
	m_vCurLine = vPoint;
}


//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ptCenter - the center point in client coordinates
//			nRadiusX - the x radius in pixels
//			nRadiusY - the y radius in pixels
//			bFill - Whether to fill the ellipse with the fill color or not.
//-----------------------------------------------------------------------------
void CRender2D::DrawCircle( const Vector &vCenter, float fRadius )
{
	Vector vNormal;
	GetCamera()->GetViewForward( vNormal );
	CRender::DrawCircle( vCenter, vNormal, fRadius, 32 ); 
}

void CRender2D::DrawRectangle( const Vector &vMins, const Vector &vMaxs, bool bFill, int extent )
{
	Vector2D ptMin, ptMax;
	TransformPoint( ptMin, vMins );
	TransformPoint( ptMax, vMaxs );

	if ( ptMin.x > ptMax.x )
		V_swap( ptMin.x, ptMax.x );

	if ( ptMin.y > ptMax.y )
		V_swap( ptMin.y, ptMax.y );

	if ( extent != 0 )
	{
		ptMin.x -= extent;
		ptMin.y -= extent;
		ptMax.x += extent;
		ptMax.y += extent;
	}

	bool bPopMode = BeginClientSpace();

	if ( bFill )
	{
		CRender::DrawFilledRect( ptMin, ptMax, (byte*)&m_DrawColor, false );
	}
	else
	{
		CRender::DrawRect( ptMin, ptMax, (byte*)&m_DrawColor );
	}

	if ( bPopMode )
		EndClientSpace();
}

void CRender2D::DrawBox( const Vector &vMins, const Vector &vMaxs, bool bFill)
{
	Vector points[8];
	PointsFromBox( vMins, vMaxs, points );

	meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, 6 );

	meshBuilder.Position3fv( &points[0].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &points[4].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &points[6].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &points[7].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &points[3].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &points[1].x );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}



