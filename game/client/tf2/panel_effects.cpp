//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"	
#include "hud.h"
#include "c_tf2rootpanel.h"
#include "paneleffect.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>

#define EFFECT_FLASH_TIME 0.7f

#define EFFECT_R 100
#define EFFECT_G 150
#define EFFECT_B 220
#define EFFECT_A 255

#define ARROW_R 130
#define ARROW_G 190
#define ARROW_B 240
#define ARROW_A 255

#define AXIALLINE_R 220
#define AXIALLINE_G 220
#define AXIALLINE_B 255
#define AXIALLINE_A 255

//-----------------------------------------------------------------------------
// Purpose: Helper for drawing line segments
//-----------------------------------------------------------------------------
class CConnectingLine
{
public:
	int			m_ptStart[ 2 ];
	int			m_ptEnd[ 2 ];
};

//-----------------------------------------------------------------------------
// Purpose: Fill in the intersection between the two rectangles.
// Input  : *pRect1 - 
//			*pRect2 - 
//			*pOut - 
// Output : inline bool
//-----------------------------------------------------------------------------
inline bool GetRectIntersection( wrect_t const *pRect1, wrect_t const *pRect2, wrect_t *pOut )
{
	pOut->left  = MAX( pRect1->left, pRect2->left );
	pOut->right = MIN( pRect1->right, pRect2->right );
	if( pOut->left >= pOut->right )
		return false;

	pOut->bottom = MIN( pRect1->bottom, pRect2->bottom );
	pOut->top    = MAX( pRect1->top, pRect2->top );
	if( pOut->top >= pOut->bottom )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Edge to use
//-----------------------------------------------------------------------------
typedef enum
{
	TOPCENTER = 0,
	RIGHTCENTER,
	BOTTOMCENTER,
	LEFTCENTER
} LINEEDGE_t;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			rect - 
//			edge - 
//			border - 
//-----------------------------------------------------------------------------
static void GetCenterPoint( int& x, int& y, const wrect_t& rect, LINEEDGE_t edge, int border )
{
	int xcenter;
	int ycenter;

	xcenter = ( rect.left + rect.right ) / 2;
	ycenter = ( rect.top + rect.bottom ) / 2;

	switch ( edge )
	{
	default:
	case TOPCENTER:
		x = xcenter;
		y = rect.top - border;
		break;
	case RIGHTCENTER:
		x = rect.right + border;
		y = ycenter;
		break;
	case BOTTOMCENTER:
		x = xcenter;
		y = rect.bottom + border;
		break;
	case LEFTCENTER:
		x = rect.left - border;
		y = ycenter;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given two rectangles, finds a direct line between the two rectangles, unless
//  they overlap, in which case no line is output.
// Input  : x1 - 
//			y1 - 
//			w1 - 
//			h1 - 
//			x2 - 
//			y2 - 
//			w2 - 
//			h2 - 
//			output - 
//-----------------------------------------------------------------------------
static void FindConnectingLines_Straight( 
	int x1, int y1, int w1, int h1, 
	int x2, int y2, int w2, int h2, 
	CUtlVector< CConnectingLine >& output )
{
	// Reset output
	output.RemoveAll();

	// If the rectangles intersect, no line needed
	wrect_t r1;
	r1.left = x1;
	r1.top = y1;
	r1.right = x1 + w1;
	r1.bottom = y1 + h1;

	wrect_t r2;
	r2.left = x2;
	r2.top = y2;
	r2.right = x2 + w2;
	r2.bottom = y2 + h2;

	wrect_t dummy;

	if ( GetRectIntersection( &r1, &r2, &dummy ) )
		return;

	int center1[2];
	int center2[2];

	center1[ 0 ] = x1 + w1/2;
	center1[ 1 ] = y1 + h1/2;

	center2[ 0 ] = x2 + w2/2;
	center2[ 1 ] = y2 + h2/2;

	int gaph;
	int gapv;

	LINEEDGE_t edge1 = TOPCENTER;
	LINEEDGE_t edge2 = BOTTOMCENTER;

	// Top
	gaph = MAX( r2.left - r1.right, r1.left - r2.right );
	gapv = MAX( r1.top - r2.bottom, r2.top - r1.bottom );

	if ( gapv > gaph )
	{
		// vertical
		if ( ( r1.top - r2.bottom ) > ( r2.top - r1.bottom ) )
		{
			edge2 = BOTTOMCENTER;
			edge1 = TOPCENTER;
		}
		else
		{
			edge2 = TOPCENTER;
			edge1 = BOTTOMCENTER;
		}
	}
	else
	{
		if ( ( r1.left - r2.right ) > ( r2.left - r1.right ) )
		{
			// horizontal
			edge2 = RIGHTCENTER;
			edge1 = LEFTCENTER;
		}
		else
		{
			edge2 = LEFTCENTER;
			edge1 = RIGHTCENTER;
		}
	}

	int pt1[ 2 ];
	int pt2[ 2 ];

	GetCenterPoint( pt1[ 0 ], pt1[ 1 ], r1, edge1, 3 );
	GetCenterPoint( pt2[ 0 ], pt2[ 1 ], r2, edge2, 3 );

	CConnectingLine line;
	line.m_ptStart[ 0 ] = pt1[ 0 ];
	line.m_ptStart[ 1 ] = pt1[ 1 ];
	line.m_ptEnd[ 0 ] = pt2[ 0 ];
	line.m_ptEnd[ 1 ] = pt2[ 1 ];

	output.AddToTail( line );
}

//-----------------------------------------------------------------------------
// Purpose: Given two non-intersecting rectangles, finds one, two or three segments
//  which connect the midpoints of two of the sides of the items together with only
//  axial lines.
// Input  : x1 - 
//			y1 - 
//			w1 - 
//			h1 - 
//			x2 - 
//			y2 - 
//			w2 - 
//			h2 - 
//			output - 
//-----------------------------------------------------------------------------
static void FindConnectingLines_Axial( 
	int x1, int y1, int w1, int h1, 
	int x2, int y2, int w2, int h2, 
	CUtlVector< CConnectingLine >& output )
{
	// Reset output
	output.RemoveAll();

	// If the rectangles intersect, no line needed
	wrect_t r1;
	r1.left = x1;
	r1.top = y1;
	r1.right = x1 + w1;
	r1.bottom = y1 + h1;

	wrect_t r2;
	r2.left = x2;
	r2.top = y2;
	r2.right = x2 + w2;
	r2.bottom = y2 + h2;
	wrect_t dummy;

	if ( GetRectIntersection( &r1, &r2, &dummy ) )
		return;

	int center1[2];
	int center2[2];

	center1[ 0 ] = x1 + w1/2;
	center1[ 1 ] = y1 + h1/2;

	center2[ 0 ] = x2 + w2/2;
	center2[ 1 ] = y2 + h2/2;

	int gaph;
	int gapv;

	LINEEDGE_t edge1 = TOPCENTER;
	LINEEDGE_t edge2 = BOTTOMCENTER;

	// Top
	gaph = MAX( r2.left - r1.right, r1.left - r2.right );
	gapv = MAX( r1.top - r2.bottom, r2.top - r1.bottom );

	if ( gapv > gaph )
	{
		// vertical
		if ( ( r1.top - r2.bottom ) > ( r2.top - r1.bottom ) )
		{
			edge2 = BOTTOMCENTER;
			edge1 = TOPCENTER;
		}
		else
		{
			edge2 = TOPCENTER;
			edge1 = BOTTOMCENTER;
		}
	}
	else
	{
		if ( ( r1.left - r2.right ) > ( r2.left - r1.right ) )
		{
			// horizontal
			edge2 = RIGHTCENTER;
			edge1 = LEFTCENTER;
		}
		else
		{
			edge2 = LEFTCENTER;
			edge1 = RIGHTCENTER;
		}
	}

	int pt1[ 2 ];
	int pt2[ 2 ];

	GetCenterPoint( pt1[ 0 ], pt1[ 1 ], r1, edge1, 3 );
	GetCenterPoint( pt2[ 0 ], pt2[ 1 ], r2, edge2, 3 );

	CConnectingLine line;

	int mid[ 2 ];
	int size1[ 2 ];
	int size2[ 2 ];

	mid[ 0 ] = ( pt1[ 0 ] + pt2[ 0 ] ) / 2;
	mid[ 1 ] = ( pt1[ 1 ] + pt2[ 1 ] ) / 2;

	size1[ 0 ] = r1.right - r1.left;
	size1[ 1 ] = r1.bottom - r1.top;

	size2[ 0 ] = r2.right - r2.left;
	size2[ 1 ] = r2.bottom - r2.top;

	float sizefrac = 0.25f;

	if ( edge1 == TOPCENTER || edge1 == BOTTOMCENTER )
	{
		int dx = abs( mid[ 0 ] - pt1[ 0 ] );
		if ( dx < ( sizefrac * size1[ 0 ] ) && 
			 dx < ( sizefrac * size2[ 0 ] ) )
		{
			// Gap is small, just use midpoint to align both
			line.m_ptStart[ 0 ] = mid[ 0 ];
			line.m_ptStart[ 1 ] = pt1[ 1 ];
			line.m_ptEnd[ 0 ] = mid[ 0 ];
			line.m_ptEnd[ 1 ] = pt2[ 1 ];

			output.AddToTail( line );
		}
		else
		{
			// Draw an L
			line.m_ptStart[ 0 ] = pt1[ 0 ];
			line.m_ptStart[ 1 ] = pt1[ 1 ];
			line.m_ptEnd[ 0 ] = pt1[ 0 ];
			line.m_ptEnd[ 1 ] = mid[ 1 ];

			output.AddToTail( line );

			line.m_ptStart[ 0 ] = pt1[ 0 ];
			line.m_ptStart[ 1 ] = mid[ 1 ];
			line.m_ptEnd[ 0 ] = pt2[ 0 ];
			line.m_ptEnd[ 1 ] = mid[ 1 ];

			output.AddToTail( line );

			line.m_ptStart[ 0 ] = pt2[ 0 ];
			line.m_ptStart[ 1 ] = mid[ 1 ];
			line.m_ptEnd[ 0 ] = pt2[ 0 ];
			line.m_ptEnd[ 1 ] = pt2[ 1 ];

			output.AddToTail( line );
		}
	}
	else
	{
		int dy = abs( mid[ 1 ] - pt1[ 1 ] );
		if ( dy < ( sizefrac * size1[ 1] ) && 
			 dy < ( sizefrac * size2[ 1 ] ) )
		{
			// Gap is small, just use midpoint to align both
			line.m_ptStart[ 0 ] = pt1[ 0 ];
			line.m_ptStart[ 1 ] = mid[ 1 ];
			line.m_ptEnd[ 0 ] = pt2[ 0 ];
			line.m_ptEnd[ 1 ] = mid[ 1 ];

			output.AddToTail( line );
		}
		else
		{
			// Draw an L
			line.m_ptStart[ 0 ] = pt1[ 0 ];
			line.m_ptStart[ 1 ] = pt1[ 1 ];
			line.m_ptEnd[ 0 ] = mid[ 0 ];
			line.m_ptEnd[ 1 ] = pt1[ 1 ];

			output.AddToTail( line );

			line.m_ptStart[ 0 ] = mid[ 0 ];
			line.m_ptStart[ 1 ] = pt1[ 1 ];
			line.m_ptEnd[ 0 ] = mid[ 0 ];
			line.m_ptEnd[ 1 ] = pt2[ 1 ];

			output.AddToTail( line );

			line.m_ptStart[ 0 ] = mid[ 0 ];
			line.m_ptStart[ 1 ] = pt2[ 1 ];
			line.m_ptEnd[ 0 ] = pt2[ 0 ];
			line.m_ptEnd[ 1 ] = pt2[ 1 ];

			output.AddToTail( line );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: Map input panel rectangle into space of output panel and return extents in xywh
// Input  : x - 
//			y - 
//			w - 
//			h - 
//			*output - 
//			*input - 
//-----------------------------------------------------------------------------
void PanelToPanelRectangle( int& x, int& y, int& w, int& h, vgui::Panel *output, vgui::Panel *input )
{
	input->GetSize( w, h );
	w += 2;
	h += 2;

	x = y = 0;
	input->LocalToScreen( x, y );
	output->ScreenToLocal( x, y );

	x--;
	y--;
}

//-----------------------------------------------------------------------------
// Purpose: Cycle between in/2 and in centered at 3/4in
// Input  : in - 
//			f - ranges from -1 to 1
// Output : static int
//-----------------------------------------------------------------------------
static int EffectResampleColor( int in, float f )
{
	int base = in / 2;
	int midpoint = ( in + base ) / 2;
	float range = (float)( in - midpoint );

	int color = midpoint + (int)( f * range );
	return clamp( color, 0, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: Border flashing effect
//-----------------------------------------------------------------------------
class CFlashBorderPanelEffect : public CPanelEffect
{
	DECLARE_CLASS( CFlashBorderPanelEffect, CPanelEffect );
public:

	CFlashBorderPanelEffect( ITFHintItem *owner );
	virtual void doPaint( vgui::Panel *panel );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//-----------------------------------------------------------------------------
CFlashBorderPanelEffect::CFlashBorderPanelEffect( ITFHintItem *owner )
	: CPanelEffect( owner )
{
	// Mark type field
	SetType( FLASHBORDER );
}

//-----------------------------------------------------------------------------
// Purpose: Paint the effect
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CFlashBorderPanelEffect::doPaint( vgui::Panel *panel )
{
	vgui::Panel *p = m_hPanel;
	if ( !p || !IsVisibleIncludingParent( p ) )
		return;

	int w, h;
	p->GetSize( w, h );

	// Convert top,left to local coordinates
	int x = 0, y = 0;
	p->LocalToScreen( x, y );
	panel->ScreenToLocal( x, y );

	x--;
	y--;
	w+=2;
	h+=2;

	float frac = fmod( gpGlobals->curtime, EFFECT_FLASH_TIME );
	frac *= 2 * M_PI;
	frac = cos( frac );

	int r, g, b;

	r = EffectResampleColor( m_r, frac );
	g = EffectResampleColor( m_g, frac );
	b = EffectResampleColor( m_b, frac );

	vgui::surface()->DrawSetColor( r, g, b, m_a );

	for ( int gap = 0; gap < 3; gap++ )
	{
		vgui::surface()->DrawOutlinedRect( x - gap, y - gap, x + w + gap, y + h + gap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates an arry from m_hPanel to m_hOtherPanel
//-----------------------------------------------------------------------------
class CArrowPanelEffect : public CPanelEffect
{
	DECLARE_CLASS( CArrowPanelEffect, CPanelEffect );
public:

	CArrowPanelEffect( ITFHintItem *owner );

	virtual void doPaint( vgui::Panel *panel );

	void		SetDrawBorder( bool drawborder );
	void		SetFlashing( bool flashing );

protected:
	void	DrawArrow( int startx, int starty, int endx, int endy, int r, int g, int b, int a );
	void	DrawLine( int startx, int starty, int endx, int endy, int r, int g, int b, int a );
	void	ComputeBestPoint( int& px, int &py, vgui::Panel *output, vgui::Panel *from, vgui::Panel *to );

protected:

	bool	m_bDrawBorder;
	bool	m_bFlashing;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//-----------------------------------------------------------------------------
CArrowPanelEffect::CArrowPanelEffect( ITFHintItem *owner )
	: CPanelEffect( owner )
{
	SetType( ARROW );

	m_bDrawBorder = true;
	m_bFlashing = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawborder - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::SetDrawBorder( bool drawborder )
{
	m_bDrawBorder = drawborder;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flashing - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::SetFlashing( bool flashing )
{
	m_bFlashing = flashing;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startx - 
//			starty - 
//			endx - 
//			endy - 
//			r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::DrawArrow( int startx, int starty, int endx, int endy, int r, int g, int b, int a )
{
	vgui::surface()->DrawSetColor( r, g, b, a );
	
	// Draw an arrow

	Vector start( startx, starty, 0.0f );
	Vector end( endx, endy, 0.0f );

	Vector delta = end - start;

	Vector right;

	right.x = delta.y;
	right.y = -delta.x;
	right.z = 0.0f;

	VectorNormalize( right );
	Vector base;

	float length = VectorNormalize( delta );

	float size = MIN( length / 2.0f, 15.0f );

	base = start + ( length - size ) * delta;

	Vector baseLeft = base + size * 0.25f * right;
	Vector baseRight = base - size * 0.25f * right;

	vgui::surface()->DrawLine( end.x, end.y, baseLeft.x, baseLeft.y );
	vgui::surface()->DrawLine( end.x, end.y, baseRight.x, baseRight.y );

	base = start + ( length - size + size * 0.3f ) * delta;

	vgui::surface()->DrawLine( base.x, base.y, baseLeft.x, baseLeft.y );
	vgui::surface()->DrawLine( base.x, base.y, baseRight.x, baseRight.y );

	vgui::surface()->DrawLine( startx, starty, base.x, base.y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startx - 
//			starty - 
//			endx - 
//			endy - 
//			r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::DrawLine( int startx, int starty, int endx, int endy, int r, int g, int b, int a )
{
	vgui::surface()->DrawSetColor( r, g, b, a );
	vgui::surface()->DrawLine( startx, starty, endx, endy );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::doPaint( vgui::Panel *panel )
{
	vgui::Panel *from = m_hPanel;
	if ( !from || !IsVisibleIncludingParent( from ) )
		return;

	int r, g, b;
	// Determine flash amount
	if ( m_bFlashing )
	{
		float frac = fmod( gpGlobals->curtime, EFFECT_FLASH_TIME );
		frac *= 2 * M_PI;
		frac = cos( frac );

		// Resample color

		r = EffectResampleColor( m_r, frac );
		g = EffectResampleColor( m_g, frac );
		b = EffectResampleColor( m_b, frac );
	}
	else
	{
		r = m_r;
		g = m_g;
		b = m_b;
	}

	int startx, starty, startw, starth;
	int endx, endy, endw, endh;

	PanelToPanelRectangle( startx, starty, startw, starth, panel, from );

	if ( !GetTargetRectangle( panel, endx, endy, endw, endh ) )
		return;

	CUtlVector< CConnectingLine > lines;

	FindConnectingLines_Straight( startx, starty, startw, starth, endx, endy, endw, endh, lines );

	int i;

	if ( m_bDrawBorder )
	{
		for ( i = 0; i < lines.Size(); i++ )
		{
			CConnectingLine *l = &lines[ i ];

			// Make it thicker
			int hstep = 0;
			int vstep = 0;

			if ( abs( l->m_ptEnd[ 1 ] - l->m_ptStart[ 1 ] ) > abs( l->m_ptEnd[ 0 ] - l->m_ptStart[ 0 ] ) )
			{
				// Taller so draw horizontally
				hstep = 1;
			}
			else
			{
				vstep = 1;
			}

			// Draw a black border
			for ( int x = -1; x <= 1 + hstep; x ++ )
			{
				for ( int y = -1; y <= 1 + vstep; y ++ )
				{
					if ( !x && !y )
						continue;

					if ( i == lines.Size() - 1 )
					{
						DrawArrow( l->m_ptStart[ 0 ] + x, l->m_ptStart[1] + y, l->m_ptEnd[0] + x, l->m_ptEnd[1] + y, 0, 0, 0, m_a );	
					}
					else
					{
						DrawLine( l->m_ptStart[ 0 ] + x, l->m_ptStart[1] + y, l->m_ptEnd[0] + x, l->m_ptEnd[1] + y, 0, 0, 0, m_a );	
					}
				}
			}
		}
	}

	for ( i = 0; i < lines.Size(); i++ )
	{
		CConnectingLine *l = &lines[ i ];

		// Make it thicker
		int hstep = 0;
		int vstep = 0;

		if ( abs( l->m_ptEnd[ 1 ] - l->m_ptStart[ 1 ] ) > abs( l->m_ptEnd[ 0 ] - l->m_ptStart[ 0 ] ) )
		{
			// Taller so draw horizontally
			hstep = 1;
		}
		else
		{
			vstep = 1;
		}

		if ( i == lines.Size() - 1 )
		{
			// Draw arrow
			DrawArrow( l->m_ptStart[ 0 ], l->m_ptStart[ 1 ], l->m_ptEnd[ 0 ], l->m_ptEnd[ 1 ], r, g, b, m_a );
			// Draw a second time, but thicker
			DrawArrow( l->m_ptStart[ 0 ] + hstep, l->m_ptStart[ 1 ] + vstep, l->m_ptEnd[ 0 ] + hstep, l->m_ptEnd[ 1 ] + vstep, r, g, b, m_a );
		}
		else
		{
			// Draw arrow
			DrawLine( l->m_ptStart[ 0 ], l->m_ptStart[ 1 ], l->m_ptEnd[ 0 ], l->m_ptEnd[ 1 ], r, g, b, m_a );
			// Draw a second time, but thicker
			DrawLine( l->m_ptStart[ 0 ] + hstep, l->m_ptStart[ 1 ] + vstep, l->m_ptEnd[ 0 ] + hstep, l->m_ptEnd[ 1 ] + vstep, r, g, b, m_a );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : px - 
//			&py - 
//			*output - 
//			*from - 
//			*to - 
//-----------------------------------------------------------------------------
void CArrowPanelEffect::ComputeBestPoint( int& px, int &py, vgui::Panel *output, vgui::Panel *from, vgui::Panel *to )
{
	int fw, fh;
	int tw, th;
	from->GetSize( fw, fh );
	to->GetSize( tw, th );

	// Convert top,left to local coordinates
	int fx = 0, fy = 0;
	int tx = 0, ty = 0;
	from->LocalToScreen( fx, fy );
	output->ScreenToLocal( fx, fy );

	to->LocalToScreen( tx, ty );
	output->ScreenToLocal( tx, ty );

	fx--;
	fy--;
	tx--;
	ty--;
	fw+=2;
	fh+=2;
	tw+=2;
	th+=2;

	int type = 0;

	// is to totally below from
	if ( ty > ( fy + fh ) )
	{
		type = 0;
	}
	// is to totally above from
	else if ( ty + th < fy )
	{
		type = 2;
	}
	// is to totally to the left of from
	else if ( tx + tw < fx )
	{
		type = 3;
	}
	// is to totally to the rigth of from
	else if ( tx > fx + fw )
	{
		type = 1;
	}
	else
	{
		type = 2;
	}

	int border = 1;

	switch ( type )
	{
	// unknown, just use object center point
	default:
	case 4:
		//
		px = fx + fw  / 2;
		py = fy + fh / 2;
		break;
	//bottom
	case 0:
		px = fx + fw / 2;
		py = fy + fh + border;
		break;
	// right
	case 1:
		px = fx + fw + border;
		py = fy + fh / 2;
		break;
	// top
	case 2:
		px = fx + fw / 2;
		py = fy - border;
		break;
	// left
	case 3:
		px = fx - border;
		py = fy + fh / 2;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates an axial line effect between the two specified panels
//-----------------------------------------------------------------------------
class CAxialLinePanelEffect : public CArrowPanelEffect
{
DECLARE_CLASS( CAxialLinePanelEffect, CArrowPanelEffect );
public:
	CAxialLinePanelEffect( ITFHintItem *owner );

	virtual void doPaint( vgui::Panel *panel );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//-----------------------------------------------------------------------------
CAxialLinePanelEffect::CAxialLinePanelEffect( ITFHintItem *owner )
: CArrowPanelEffect( owner )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CAxialLinePanelEffect::doPaint( vgui::Panel *panel )
{
	vgui::Panel *from = m_hPanel;
	if ( !from || !IsVisibleIncludingParent( from ) )
		return;

	int r, g, b;

	if ( m_bFlashing )
	{
		// Determine flash amount
		float frac = fmod( gpGlobals->curtime, EFFECT_FLASH_TIME );
		frac *= 2 * M_PI;
		frac = cos( frac );

		// Resample color
		r = EffectResampleColor( m_r, frac );
		g = EffectResampleColor( m_g, frac );
		b = EffectResampleColor( m_b, frac );
	}
	else
	{
		r = m_r;
		g = m_g;
		b = m_b;
	}

	int startx, starty, startw, starth;
	int endx, endy, endw, endh;

	PanelToPanelRectangle( startx, starty, startw, starth, panel, from );

	if ( !GetTargetRectangle( panel, endx, endy, endw, endh ) )
		return;

	CUtlVector< CConnectingLine > lines;

	FindConnectingLines_Axial( startx, starty, startw, starth, endx, endy, endw, endh, lines );

	int i;

	if ( m_bDrawBorder )
	{
		for ( i = 0; i < lines.Size(); i++ )
		{
			CConnectingLine *l = &lines[ i ];

			// Draw a black border
			for ( int x = -1; x <= 1; x ++ )
			{
				for ( int y = -1; y <= 1; y ++ )
				{
					if ( !x && !y )
						continue;

					DrawLine( l->m_ptStart[ 0 ] + x, l->m_ptStart[1] + y, l->m_ptEnd[0] + x, l->m_ptEnd[1] + y, 0, 0, 0, m_a );	
				}
			}
		}
	}

	// Draw actual lines
	for ( i = 0; i < lines.Size(); i++ )
	{
		CConnectingLine *l = &lines[ i ];
		DrawLine( l->m_ptStart[ 0 ], l->m_ptStart[ 1 ], l->m_ptEnd[ 0 ], l->m_ptEnd[ 1 ], r, g, b, m_a );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//			*target - 
// Output : EFFECT_HANDLE
//-----------------------------------------------------------------------------
EFFECT_HANDLE CreateFlashEffect( ITFHintItem *owner, vgui::Panel *target )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CFlashBorderPanelEffect *e = new CFlashBorderPanelEffect( owner );

	e->SetColor( EFFECT_R, EFFECT_G, EFFECT_B, EFFECT_A );
	// e->SetEndTime( gpGlobals->curtime + 15.0f );
	e->SetPanel( target );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//			*from - 
//			*to - 
// Output : EFFECT_HANDLE
//-----------------------------------------------------------------------------
EFFECT_HANDLE CreateArrowEffect( ITFHintItem *owner, vgui::Panel *from, vgui::Panel *to )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CArrowPanelEffect *e = new CArrowPanelEffect( owner );

	e->SetColor( ARROW_R, ARROW_G, ARROW_B, ARROW_A );
	e->SetPanel( from );
	e->SetPanelOther( to );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//			*from - 
//			*to - 
// Output : EFFECT_HANDLE
//-----------------------------------------------------------------------------
EFFECT_HANDLE CreateAxialLineEffect( ITFHintItem *owner, vgui::Panel *from, vgui::Panel *to )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CAxialLinePanelEffect *e = new CAxialLinePanelEffect( owner );

	e->SetColor( AXIALLINE_R, AXIALLINE_G, AXIALLINE_B, AXIALLINE_A );
	e->SetPanel( from );
	e->SetPanelOther( to );

	e->SetFlashing( false );
	e->SetDrawBorder( false );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

EFFECT_HANDLE CreateArrowEffectToPoint( ITFHintItem *owner, vgui::Panel *from, int x, int y )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CArrowPanelEffect *e = new CArrowPanelEffect( owner );

	e->SetColor( ARROW_R, ARROW_G, ARROW_B, ARROW_A );
	e->SetPanel( from );
	e->SetTargetPoint( x, y );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

EFFECT_HANDLE CreateAxialLineEffectToPoint( ITFHintItem *owner, vgui::Panel *from, int x, int y )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CAxialLinePanelEffect *e = new CAxialLinePanelEffect( owner );

	e->SetColor( AXIALLINE_R, AXIALLINE_G, AXIALLINE_B, AXIALLINE_A );
	e->SetPanel( from );
	e->SetTargetPoint( x, y );

	e->SetFlashing( false );
	e->SetDrawBorder( false );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

EFFECT_HANDLE CreateArrowEffectToRect( ITFHintItem *owner, vgui::Panel *from, int x, int y, int w, int h )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CArrowPanelEffect *e = new CArrowPanelEffect( owner );

	e->SetColor( ARROW_R, ARROW_G, ARROW_B, ARROW_A );
	e->SetPanel( from );
	e->SetTargetRect( x, y, w, h );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

EFFECT_HANDLE CreateAxialLineEffectToRect( ITFHintItem *owner, vgui::Panel *from, int x, int y, int w, int h )
{
	if ( !g_pTF2RootPanel )
		return EFFECT_INVALID_HANDLE;

	CAxialLinePanelEffect *e = new CAxialLinePanelEffect( owner );

	e->SetColor( AXIALLINE_R, AXIALLINE_G, AXIALLINE_B, AXIALLINE_A );
	e->SetPanel( from );
	e->SetTargetRect( x, y, w, h );

	e->SetFlashing( false );
	e->SetDrawBorder( false );

	g_pTF2RootPanel->AddEffect( e );

	return e->GetHandle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//-----------------------------------------------------------------------------
void DestroyPanelEffects( ITFHintItem *owner )
{
	if ( !g_pTF2RootPanel )
		return;

	g_pTF2RootPanel->DestroyPanelEffects( owner );
}