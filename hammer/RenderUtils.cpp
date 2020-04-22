//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Render2D.h"
#include "RenderUtils.h"
#include "mapview2d.h"
#include "toolinterface.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the measurements of a brush in the 2D view.
// Input  : pRender -
//			Mins - 
//			Maxs -
//			nFlags - 
//-----------------------------------------------------------------------------
void DrawBoundsText(CRender2D *pRender, const Vector &Mins, const Vector &Maxs, int nFlags)
{
	CMapView2D *pView = (CMapView2D*) pRender->GetView();

	// Calculate the solid's extents along our 2D view axes.
	Vector Extents = Maxs - Mins;
	Vector Center = (Mins + Maxs ) * 0.5f;

	for ( int i=0; i<3;i++ )
		Extents[i] = fabs(Extents[i]);
	
	// Transform the solids mins and maxs to 2D view space. These are used
	// for placing the text in the view.
	Vector2D projMins, projMaxs, projCenter;
    pRender->TransformPoint( projMins, Mins );
    pRender->TransformPoint( projMaxs, Maxs );
	pRender->TransformPoint( projCenter, Center );

    if( projMins.x > projMaxs.x )
    {
		V_swap( projMins.x, projMaxs.x );
    }

    if( projMins.y > projMaxs.y )
    {
        V_swap( projMins.y, projMaxs.y );
    }

	//
    // display the extents of this brush
    //
    char extentText[30];
    int nTextX, nTextY;
	int nTextFlags;

	pRender->SetTextColor( 255, 255, 255 );

    // horz
     sprintf( extentText, "%.1f", Extents[pView->axHorz] );
 	nTextFlags = CRender2D::TEXT_JUSTIFY_HORZ_CENTER;
    nTextX = projCenter.x;

	if ( nFlags & DBT_TOP )
	{
		nTextY = projMins.y - (HANDLE_RADIUS*3);
		nTextFlags |= CRender2D::TEXT_JUSTIFY_TOP;
	}
	else
	{
		nTextY = projMaxs.y + (HANDLE_RADIUS*3);
		nTextFlags |= CRender2D::TEXT_JUSTIFY_BOTTOM;
	}

    pRender->DrawText( extentText, nTextX, nTextY, nTextFlags );

    // vert
    sprintf( extentText, "%.1f", Extents[pView->axVert] );
	nTextFlags = CRender2D::TEXT_JUSTIFY_VERT_CENTER;
	nTextY = projCenter.y;
	
	if ( nFlags & DBT_LEFT )
	{
		nTextX = projMins.x - (HANDLE_RADIUS*3);
		nTextFlags |= CRender2D::TEXT_JUSTIFY_LEFT;
	}
	else
	{
		nTextX = projMaxs.x + (HANDLE_RADIUS*3);
		nTextFlags |= CRender2D::TEXT_JUSTIFY_RIGHT;
	}
    
    pRender->DrawText( extentText, nTextX, nTextY, nTextFlags );
}


