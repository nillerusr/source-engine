//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "ground_line.h"
#include "mathlib/vplane.h"
#include "beamdraw.h"
#include "bitvec.h"
#include "clientmode_commander.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "clienteffectprecachesystem.h"
#include "tier0/vprof.h"

#define MAX_DOWN_DIST	300
#define MAX_UP_DIST		300
#define XY_PER_SEGMENT	100

CLIENTEFFECT_REGISTER_BEGIN( PrecacheGroundLine )
CLIENTEFFECT_MATERIAL( "player/support/mortarline" )
CLIENTEFFECT_REGISTER_END()

static CUtlLinkedList< CGroundLine*, unsigned short >	s_GroundLines; 

// ---------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------- //

VPlane VPlaneFromCPlane(const cplane_t &plane)
{
	if(plane.signbits)
		return VPlane(-plane.normal, -plane.dist);
	else
		return VPlane(plane.normal, plane.dist);
}


Vector ClipEndPos(const Vector &vStart, const Vector &vEnd, float clipDist, VPlane *pPlane)
{
	trace_t trace;

	UTIL_TraceLine(vStart, vEnd, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);
	if(trace.fraction < 1)
	{
		*pPlane = VPlaneFromCPlane(trace.plane);
		return trace.endpos + pPlane->m_Normal * clipDist;
	}
	else
	{
		pPlane->m_Normal.Init(0,0,1);
		pPlane->m_Dist = DotProduct(pPlane->m_Normal, vEnd);
		return vEnd;
	}
}

// Tries to find the closest surface point to the specified point.
Vector FindBestSurfacePoint(const Vector &vPos)
{
	static float stepDist = 500;
	static float flHeightAboveGround = 20;

	// First, find an inside point.

	// Test upwards.
	trace_t trace;
	UTIL_TraceLine(
		Vector(vPos[0], vPos[1], vPos[2] + stepDist),
		vPos,
		MASK_SOLID_BRUSHONLY,
		NULL,
		COLLISION_GROUP_NONE,
		&trace);
	if(trace.fraction < 1 && trace.fraction != 0)
	{
		return Vector(trace.endpos[0], trace.endpos[1], trace.endpos[2] + flHeightAboveGround );
	}

	// Test down.
	UTIL_TraceLine(
		vPos,
		Vector(vPos[0], vPos[1], vPos[2] - stepDist),
		MASK_SOLID_BRUSHONLY,
		NULL,
		COLLISION_GROUP_NONE,
		&trace);
	if(trace.fraction < 1 && trace.fraction != 0)
	{
		return Vector(trace.endpos[0], trace.endpos[1], trace.endpos[2] + flHeightAboveGround );
	}

	return vPos;
}


// Tries to find the place in the world geometry which blocks vStart from the line segment (vEnd1, vEnd2).
// Uses a binary search so your error is |vEnd2 - vEnd1| ^ (1 / nIterations)
bool BinSearchSegments(const Vector &vStart, const Vector &vEnd1, const Vector &vEnd2, int nIterations, Vector *out)
{
	trace_t trace;
	
	// If what was passed into us already intersects then there's nothing we can do.
	UTIL_TraceLine(vStart, vEnd2, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);
	if(trace.fraction < 1)
		return false;

	Vector vecs[2] = {vEnd1, vEnd2};
	int iIntersect = 0;	// Which vector intersects.
	for(int i=0; i < nIterations; i++)
	{
		// Test the midpoint.
		Vector mid = (vecs[0] + vecs[1]) * 0.5f;
		UTIL_TraceLine(vStart, mid, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);
		if(trace.fraction < 1)
			vecs[iIntersect] = mid;
		else
			vecs[!iIntersect] = mid;
	}

	*out = (vecs[0] + vecs[1]) * 0.5f;
	return true;
}

// Tries to snap the point to its underlying plane's z.
Vector SnapToPlane(const Vector &v)
{
return v;

	trace_t trace;
	UTIL_TraceLine(Vector(v[0], v[1], v[2] + 50), Vector(v[0], v[1], v[2] - 50), 
		MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);
	if(trace.fraction < 1)
		return Vector(v[0], v[1], trace.endpos[2] + 3);
	else
		return v;
}



// ---------------------------------------------------------------------- //
// CGroundLine implementation.
// ---------------------------------------------------------------------- //

CGroundLine::CGroundLine()
: BaseClass( NULL, "CGroundLine" )
{
	m_pMaterial = NULL;

	m_ListHandle = s_GroundLines.AddToHead( this );
	SetParent( CMinimapPanel::MinimapRootPanel() );

	m_nPoints = 0;
	SetVisible( true );
	SetPaintBackgroundEnabled( false );
}


CGroundLine::~CGroundLine()
{
	s_GroundLines.Remove( m_ListHandle );
	
	m_vStart.Init();
	m_vEnd.Init();
	m_LineWidth = 1;
}


bool CGroundLine::Init(const char *pMaterialName)
{
	m_pMaterial = materials->FindMaterial(pMaterialName, TEXTURE_GROUP_CLIENT_EFFECTS);
	return !!m_pMaterial;
}


void CGroundLine::SetParameters(
	const Vector &vStart, 
	const Vector &vEnd, 
	const Vector &vStartColor,	// Color values 0-1
	const Vector &vEndColor,
	float alpha,
	float lineWidth
	)
{
	m_vStart = vStart;
	m_vEnd = vEnd;
	m_vStartColor = vStartColor;
	m_vEndColor = vEndColor;
	m_Alpha = alpha;
	m_LineWidth = lineWidth;

	Vector vTo( vEnd.x - vStart.x, vEnd.y - vStart.y, 0 );
	float flXYLen = vTo.Length();

	// Recalculate our segment list.
	unsigned int nSteps = (int)flXYLen / XY_PER_SEGMENT;
	nSteps = clamp( nSteps, 8, MAX_GROUNDLINE_SEGMENTS ) & ~1;
	unsigned int nMaxSteps = nSteps / 2;

	// First generate the sequence. We generate every other point here so it can insert fixup points to prevent
	// it from crossing world geometry.
	Vector pt[MAX_GROUNDLINE_SEGMENTS];
	Vector vStep = (Vector(m_vEnd[0], m_vEnd[1], 0) - Vector(m_vStart[0], m_vStart[1], 0)) / (nMaxSteps-1);

	pt[0] = FindBestSurfacePoint(m_vStart);

	unsigned int i;
	for(i=1; i < nMaxSteps; i++)
		pt[i<<1] = FindBestSurfacePoint(pt[(i-1)<<1] + vStep);


	CBitVec<MAX_GROUNDLINE_SEGMENTS> pointsUsed;
	pointsUsed.ClearAll();

	// Now try to make sure they don't intersect the geometry.
	for(i=0; i < nMaxSteps-1; i++)
	{
		Vector &a = pt[i<<1];
		Vector &b = pt[(i+1)<<1];

		trace_t trace;
		UTIL_TraceLine(a, b, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);
		if(trace.fraction < 1)
		{
			int cIndex = (i<<1)+1;
			Vector &c = pt[cIndex];

			// Ok, this line segment intersects the world. Do a binary search to try to find the
			// point of intersection.
			Vector hi, lo;
			if(a.z < b.z)
			{
				hi = b;
				lo = a;
			}
			else
			{
				hi = a;
				lo = b;
			}

			if(BinSearchSegments(lo, hi, Vector(lo[0],lo[1],hi[2]), 15, &c))
			{
				pointsUsed.Set( cIndex );
			}
			else if(BinSearchSegments(lo, hi, Vector(hi[0],hi[1],hi[2]+500), 15, &c))
			{
				pointsUsed.Set( cIndex );
			}
		}
	}

	// Export the points.
	m_nPoints = 0;
	for(i=0; i < nSteps; i++)
	{
		// Every other point is always active.
		if( pointsUsed.Get( i ) || !(i & 1) )
		{
			m_Points[m_nPoints] = pt[i];
			++m_nPoints;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the visibility of the groundline
//-----------------------------------------------------------------------------
void CGroundLine::SetVisible( bool bVisible )
{
	m_bVisible = bVisible;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the groundline's visible
//-----------------------------------------------------------------------------
bool CGroundLine::IsVisible( void )
{
	return m_bVisible;
}

void CGroundLine::DrawAllGroundLines()
{
	VPROF("CGroundLine::DrawAllGroundLines()");
	unsigned short i;
	for( i = s_GroundLines.Head(); i != s_GroundLines.InvalidIndex(); i = s_GroundLines.Next(i) )
	{
		s_GroundLines[i]->Draw();
	}
}

void CGroundLine::Draw()
{
	if ( !m_pMaterial || m_nPoints < 2 )
		return;
	if ( !IsVisible() )
		return;
	
	float flAlpha = m_Alpha;
	if( g_pClientMode == ClientModeCommander() )
	{
		flAlpha = 1; // draw bright..
	}

	CBeamSegDraw beamDraw;
	beamDraw.Start( m_nPoints, m_pMaterial );
	
		for( unsigned int i=0; i < m_nPoints; i++ )
		{
			float t = (float)i / (m_nPoints - 1);

			CBeamSeg seg;
			seg.m_vPos = m_Points[i];
			VectorLerp( m_vStartColor, m_vEndColor, t, seg.m_vColor );
			seg.m_flTexCoord = 0;
			seg.m_flWidth = m_LineWidth;
			seg.m_flAlpha = m_Alpha;

			beamDraw.NextSeg( &seg );
		}
	
	beamDraw.End();
}


static inline bool ClipLine( float &x1, float &y1, float &x2, float &y2, float xClip, float sign )
{
	if( x1*sign < (xClip-0.001f)*sign )
	{
		if( x2*sign > (xClip+0.001f)*sign )
		{
			float t = (xClip-x1) / (x2 - x1);
			x1 = x1 + (x2 - x1) * t;
			y1 = y1 + (y2 - y1) * t;
		}
		else
		{
			return false;
		}
	}
	else if( x2*sign < (xClip-0.001f)*sign )
	{
		if( x1*sign > (xClip+0.001f)*sign )
		{
			float t = (xClip-x1) / (x2 - x1);
			x2 = x1 + (x2 - x1) * t;
			y2 = y1 + (y2 - y1) * t;
		}
		else
		{
			return false;
		}
	}
	
	return true;
}


void CGroundLine::Paint( )
{
	vgui::Panel *pPanel = GetParent();
	int wide, tall;
	pPanel->GetSize( wide, tall );
	
	float tPrev = 0;
	float xPrev, yPrev;
	CMinimapPanel::MinimapPanel()->WorldToMinimap( MINIMAP_NOCLIP, m_vStart, xPrev, yPrev );

	int nSegs = 20;
	for( int iSeg=1; iSeg <= nSegs; iSeg++ )
	{
		float t = (float)iSeg / nSegs;

		Vector v3DPos;
		VectorLerp( m_vStart, m_vEnd, t, v3DPos );

		float x, y;
		CMinimapPanel::MinimapPanel()->WorldToMinimap( MINIMAP_NOCLIP, v3DPos, x, y );

		// Clip the line segment on X, then Y.
		if( ClipLine( xPrev, yPrev, x, y, 0,     1 ) &&
			ClipLine( xPrev, yPrev, x, y, wide, -1 ) &&
			ClipLine( yPrev, xPrev, y, x, 0,     1 ) &&
			ClipLine( yPrev, xPrev, y, x, tall, -1 ) )
		{
			Vector vColor;
			VectorLerp( m_vStartColor, m_vEndColor, t, vColor );
			vColor *= 255.9f;

			vgui::surface()->DrawSetColor( 
				(unsigned char)RoundFloatToInt( vColor.x ), 
				(unsigned char)RoundFloatToInt( vColor.y ), 
				(unsigned char)RoundFloatToInt( vColor.z ), 
				255 );

			vgui::surface()->DrawLine( xPrev, yPrev, x, y );
		}

		tPrev = t;
		xPrev = x;
		yPrev = y;
	}

	// Draw a marker at the endpoint.
	float xEnd, yEnd;
	if( CMinimapPanel::MinimapPanel()->WorldToMinimap( MINIMAP_NOCLIP, m_vEnd, xEnd, yEnd ) )
	{
		int ix = RoundFloatToInt( xEnd );
		int iy = RoundFloatToInt( yEnd );
		int rectSize=1;

		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawOutlinedRect( ix-rectSize, iy-rectSize, ix+rectSize, iy+rectSize );
	}
}


