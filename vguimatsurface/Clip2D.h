//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains 2D clipping routines
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef CLIP2D_H
#define CLIP2D_H

namespace vgui
{
	struct Vertex_t;
}

//-----------------------------------------------------------------------------
// Enable/disable scissoring...
//-----------------------------------------------------------------------------
void EnableScissor( bool enable );

//-----------------------------------------------------------------------------
// For simulated scissor tests...
//-----------------------------------------------------------------------------
void SetScissorRect( int left, int top, int right, int bottom );
void GetScissorRect( int &left, int &top, int &right, int &bottom, bool &enabled );

//-----------------------------------------------------------------------------
// Clips a line segment to the current scissor rectangle
//-----------------------------------------------------------------------------
bool ClipLine( const vgui::Vertex_t *pInVerts, vgui::Vertex_t* pOutVerts );


//-----------------------------------------------------------------------------
// Purpose: Does a scissor clip of the input rectangle.  
// Returns false if it is completely clipped off.
//-----------------------------------------------------------------------------
bool ClipRect( const vgui::Vertex_t &inUL, const vgui::Vertex_t &inLR, 
			   vgui::Vertex_t *pOutUL, vgui::Vertex_t *pOutLR );

//-----------------------------------------------------------------------------
// Clips a polygon to the screen area
//-----------------------------------------------------------------------------
int ClipPolygon( int iCount, vgui::Vertex_t *pVerts, int iTranslateX, int iTranslateY, vgui::Vertex_t ***pppOutVertex ); 

#endif // CLIP2D_H