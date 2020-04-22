//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Debugging overlay functions
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "render_pch.h"
#include "edict.h"
#include "client.h"
#include "debugoverlay.h"
#include "cdll_int.h"
#include "ivideomode.h"
#include "materialsystem/imesh.h"
#include "gl_matsysiface.h"
#include "server.h"
#include "client_class.h"
#include "icliententitylist.h"
#include "mathlib/vmatrix.h"
#include "icliententity.h"
#include "overlaytext.h"
#include "engine/ivdebugoverlay.h"
#include "cmodel_engine.h"
#include "vphysics_interface.h"
#include "materialsystem/imaterial.h"
#include "tier2/renderutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern edict_t *EDICT_NUM(int n);

ConVar enable_debug_overlays( "enable_debug_overlays", "1", FCVAR_GAMEDLL | FCVAR_CHEAT, "Enable rendering of debug overlays" );

int GetOverlayTick()
{
	if ( sv.IsActive() )
		return sv.m_nTickCount;

	return cl.GetClientTickCount();
}

bool OverlayText_t::IsDead()
{
	if ( IsXbox() && cl.IsPaused() )
		return false;

	if ( m_nServerCount != cl.m_nServerCount )
		return true;

	if ( m_nCreationTick != -1 )
	{
		if ( GetOverlayTick() > m_nCreationTick )
			return true;

		return false;
	}

	if ( m_flEndTime == NDEBUG_PERSIST_TILL_NEXT_SERVER )
		return false;

	return (cl.GetTime() >= m_flEndTime);
}

void OverlayText_t::SetEndTime( float duration )
{
	m_nServerCount = cl.m_nServerCount;

	if ( duration <= 0.0f )
	{
		m_flEndTime = 0.0f;
		m_nCreationTick = GetOverlayTick();
		return;
	}

	if ( duration == NDEBUG_PERSIST_TILL_NEXT_SERVER )
	{
		m_flEndTime = NDEBUG_PERSIST_TILL_NEXT_SERVER;
	}
	else
	{
	m_flEndTime = cl.GetTime() + duration;
	}
}

namespace CDebugOverlay
{

enum OverlayType_t
{
	OVERLAY_BOX = 0,
	OVERLAY_SPHERE,
	OVERLAY_LINE,
	OVERLAY_TRIANGLE,
	OVERLAY_SWEPT_BOX,
	OVERLAY_BOX2
};

struct OverlayBase_t
{
	OverlayBase_t()
	{
		m_Type = OVERLAY_BOX;
		m_nServerCount = -1;
		m_nCreationTick = -1;
		m_flEndTime = 0.0f;
		m_pNextOverlay = NULL;
	}

	bool IsDead()
	{
		if ( IsXbox() && cl.IsPaused() )
			return false;

		if ( m_nServerCount != cl.m_nServerCount )
			return true;

		if ( m_nCreationTick != -1 )
		{
			if ( GetOverlayTick() > m_nCreationTick )
				return true;

			return false;
		}

		if ( m_flEndTime == NDEBUG_PERSIST_TILL_NEXT_SERVER )
			return false;

		return (cl.GetTime() >= m_flEndTime) ;
	}

	void SetEndTime( float duration )
	{
		m_nServerCount = cl.m_nServerCount;

		if ( duration <= 0.0f )
		{
			m_nCreationTick = GetOverlayTick();	// stay alive for only one frame
			return;
		}
		
		if ( duration == NDEBUG_PERSIST_TILL_NEXT_SERVER )
		{
			m_flEndTime = NDEBUG_PERSIST_TILL_NEXT_SERVER;
		}
		else
		{
			m_flEndTime = cl.GetTime() + duration;
		}
	}

	OverlayType_t	m_Type;				// What type of overlay is it?
	int				m_nCreationTick;	// Duration -1 means go away after this frame #
	int				m_nServerCount;		// Latch server count, too
	float			m_flEndTime;		// When does this box go away
	OverlayBase_t	*m_pNextOverlay;
};

struct OverlayBox_t : public OverlayBase_t
{
	OverlayBox_t() { m_Type = OVERLAY_BOX; }

	Vector			origin;
	Vector			mins;
	Vector			maxs;
	QAngle			angles;
	int				r;
	int				g;
	int				b;
	int				a;
};

struct OverlayBox2_t : public OverlayBase_t
{
	OverlayBox2_t() { m_Type = OVERLAY_BOX2; }

	Vector			origin;
	Vector			mins;
	Vector			maxs;
	QAngle			angles;
	Color			edgeColor;
	Color			faceColor;
};

struct OverlaySphere_t : public OverlayBase_t
{
	OverlaySphere_t() { m_Type = OVERLAY_SPHERE; }

	Vector			vOrigin;
	float			flRadius;
	int				nTheta;
	int				nPhi;
	int				r;
	int				g;
	int				b;
	int				a;
};

struct OverlayLine_t : public OverlayBase_t 
{
	OverlayLine_t() { m_Type = OVERLAY_LINE; }

	Vector			origin;
	Vector			dest;
	int				r;
	int				g;
	int				b;
	int				a;
	bool			noDepthTest;
};

struct OverlayTriangle_t : public OverlayBase_t 
{
	OverlayTriangle_t() { m_Type = OVERLAY_TRIANGLE; }

	Vector			p1;
	Vector			p2;
	Vector			p3;
	int				r;
	int				g;
	int				b;
	int				a;
	bool			noDepthTest;
};

struct OverlaySweptBox_t : public OverlayBase_t 
{
	OverlaySweptBox_t() { m_Type = OVERLAY_SWEPT_BOX; }

	Vector			start;
	Vector			end;
	Vector			mins;
	Vector			maxs;
	QAngle			angles;
	int				r;
	int				g;
	int				b;
	int				a;
};


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void DrawOverlays();
void DrawGridOverlay();
void ClearAllOverlays();
void ClearDeadOverlays();


//-----------------------------------------------------------------------------
// Init static member variables
//-----------------------------------------------------------------------------
OverlayText_t*	s_pOverlayText = NULL;	// text is handled differently; for backward compatibility reasons
OverlayBase_t*	s_pOverlays = NULL; 
Vector			s_vGridPosition(0,0,0);
bool			s_bDrawGrid = false;
CThreadFastMutex s_OverlayMutex;


//-----------------------------------------------------------------------------
// Purpose: Hack to allow this code to run on a client that's not connected to a server
//  (i.e., demo playback, or multiplayer game )
// Input  : ent_num - 
//			origin - 
//			mins - 
//			maxs - 
// Output : static void
//-----------------------------------------------------------------------------
static bool GetEntityOriginClientOrServer( int ent_num, Vector& origin )
{
	AUTO_LOCK( s_OverlayMutex );
	// Assume failure
	origin.Init();

	if ( sv.IsActive() )
	{
		edict_t *e = EDICT_NUM( ent_num );
		if ( e )
		{
			IServerEntity *serverEntity = e->GetIServerEntity();
			if ( serverEntity )
			{
				CM_WorldSpaceCenter( serverEntity->GetCollideable(), &origin );
			}

			return true;
		}
	}
	else
	{
		IClientEntity *clent = entitylist->GetClientEntity( ent_num );
		if ( clent )
		{
			CM_WorldSpaceCenter( clent->GetCollideable(), &origin );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Given a point, return the screen position
// Input  :
// Output :
//-----------------------------------------------------------------------------
int ScreenPosition(const Vector& point, Vector& screen)
{
	AUTO_LOCK( s_OverlayMutex );
	CMatRenderContextPtr pRenderContext( materials );

	int retval = g_EngineRenderer->ClipTransform(point,&screen);
	
	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );

	screen[0] =  0.5 * screen[0] * w;
	screen[1] = -0.5 * screen[1] * h;
	screen[0] += 0.5 * w;
	screen[1] += 0.5 * h;
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Given an xy screen pos (0-1), return the screen position 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int ScreenPosition(float flXPos, float flYPos, Vector& screen)
{
	if (flXPos > 1.0 || flYPos > 1.0 || flXPos < 0.0 || flYPos < 0.0 )
		return 1; // Fail

	AUTO_LOCK( s_OverlayMutex );
	CMatRenderContextPtr pRenderContext( materials );

	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );

	screen[0] =  flXPos * w;
	screen[1] =  flYPos * h;
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Add new entity positioned overlay text
// Input  : Entity to attach text to
//			How many lines to offset text from entity origin
//			The text to print
// Output :
//-----------------------------------------------------------------------------
void AddEntityTextOverlay(int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *text)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	Vector myPos, myMins, myMaxs;

	GetEntityOriginClientOrServer( ent_index, myPos );

	VectorCopy(myPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= line_offset;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= r;
	new_overlay->g			= g;
	new_overlay->b			= b;
	new_overlay->a			= a;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay text
// Input  : Position of text & text
// Output :
//-----------------------------------------------------------------------------
void AddGridOverlay(const Vector& vPos) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	s_vGridPosition[0]		= vPos[0];
	s_vGridPosition[1]		= vPos[1];
	s_vGridPosition[2]		= vPos[2];
	s_bDrawGrid				= true;
}

void AddCoordFrameOverlay(const matrix3x4_t& frame, float flScale, int vColorTable[3][3]/*=NULL*/)
{
	static int s_defaultColorTable[3][3] =
	{
		{ 255,   0, 0   },
		{ 0  , 255, 0   },
		{ 0  ,   0, 255 }
	};

	AUTO_LOCK( s_OverlayMutex );
	if ( vColorTable == NULL )
		vColorTable = s_defaultColorTable;

	Vector startPt, endPt;
	MatrixGetColumn( frame, 3, startPt );

	for (int k = 0; k < 3; k++)
	{
		endPt.x = frame[0][3] + frame[0][k] * flScale;
		endPt.y = frame[1][3] + frame[1][k] * flScale;
		endPt.z = frame[2][3] + frame[2][k] * flScale;

		AddLineOverlay(
			startPt,
			endPt,
			vColorTable[k][0], vColorTable[k][1], vColorTable[k][2], 255,
			true,
			NDEBUG_PERSIST_TILL_NEXT_SERVER
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay text
// Input  : Position of text & text
// Output :
//-----------------------------------------------------------------------------
void AddTextOverlay(const Vector& textPos, float duration, const char *text) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	VectorCopy(textPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= 0;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= 255;
	new_overlay->g			= 255;
	new_overlay->b			= 255;
	new_overlay->a			= 255;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay text
// Input  : Position of text & text
// Output :
//-----------------------------------------------------------------------------
void AddTextOverlay(const Vector& textPos, float duration, float alpha, const char *text) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	VectorCopy(textPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= 0;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= 255;
	new_overlay->g			= 255;
	new_overlay->b			= 255;
	new_overlay->a			= (int)clamp(alpha * 255.f,0.f,255.f);

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

//------------------------------------------------------------------------------
// Purpose : 
// Input   :
// Output  :
//------------------------------------------------------------------------------
void AddScreenTextOverlay(float flXPos, float flYPos, int line_offset, float duration, int r, int g, int b, int a, const char *text)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->flXPos		= flXPos;
	new_overlay->flYPos		= flYPos;
	new_overlay->bUseOrigin = false;
	new_overlay->lineOffset	= line_offset;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= r;
	new_overlay->g			= g;
	new_overlay->b			= b;
	new_overlay->a			= a;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

void AddScreenTextOverlay( float flXPos, float flYPos, float duration, int r, int g, int b, int a, const char *text )
{
	AddScreenTextOverlay( flXPos, flYPos, 0, duration, r, g, b, a, text );
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay text
// Input  : Position of text 
//			How many lines to offset text from position
//			ext
// Output :
//-----------------------------------------------------------------------------
void AddTextOverlay(const Vector& textPos, int line_offset, float duration, const char *text) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	VectorCopy(textPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= line_offset;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= 255;
	new_overlay->g			= 255;
	new_overlay->b			= 255;
	new_overlay->a			= 255;
	new_overlay->bUseOrigin = true;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

void AddTextOverlay(const Vector& textPos, int line_offset, float duration, float alpha, const char *text)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	VectorCopy(textPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= line_offset;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= 255;
	new_overlay->g			= 255;
	new_overlay->b			= 255;
	new_overlay->a			= (int)clamp(alpha * 255.f,0.f,255.f);
	new_overlay->bUseOrigin = true;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

void AddTextOverlay(const Vector& textPos, int line_offset, float duration, float r, float g, float b, float alpha, const char *text)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t *new_overlay = new OverlayText_t;

	VectorCopy(textPos,new_overlay->origin);
	Q_strncpy(new_overlay->text,text, sizeof( new_overlay->text ) );
	new_overlay->bUseOrigin = true;
	new_overlay->lineOffset	= line_offset;
	new_overlay->SetEndTime( duration );
	new_overlay->r			= (int)clamp(r * 255.f,0.f,255.f);
	new_overlay->g			= (int)clamp(g * 255.f,0.f,255.f);
	new_overlay->b			= (int)clamp(b * 255.f,0.f,255.f);
	new_overlay->a			= (int)clamp(alpha * 255.f,0.f,255.f);
	new_overlay->bUseOrigin = true;

	new_overlay->nextOverlayText = s_pOverlayText;
	s_pOverlayText = new_overlay;
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay box
// Input  : Position of box
//			size of box
//			angles of box
//			color & alpha
//			duration
// Output :
//-----------------------------------------------------------------------------
void AddBoxOverlay(const Vector& origin, const Vector& mins, const Vector& maxs, QAngle const& angles, int r, int g, int b, int a, float flDuration) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayBox_t *new_overlay = new OverlayBox_t;

	new_overlay->origin = origin;

	new_overlay->mins[0] = mins[0];
	new_overlay->mins[1] = mins[1];
	new_overlay->mins[2] = mins[2];

	new_overlay->maxs[0] = maxs[0];
	new_overlay->maxs[1] = maxs[1];
	new_overlay->maxs[2] = maxs[2];

	new_overlay->angles = angles;

	new_overlay->r = r;
	new_overlay->g = g;
	new_overlay->b = b;
	new_overlay->a = a;

	new_overlay->SetEndTime( flDuration );

	new_overlay->m_pNextOverlay = s_pOverlays;
	s_pOverlays = new_overlay;
}

void AddBoxOverlay2( const Vector& origin, const Vector& mins, const Vector& maxs, QAngle const& orientation, const Color& faceColor, const Color& edgeColor, float duration )
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayBox2_t *new_overlay = new OverlayBox2_t;

	new_overlay->origin = origin;

	new_overlay->mins[0] = mins[0];
	new_overlay->mins[1] = mins[1];
	new_overlay->mins[2] = mins[2];

	new_overlay->maxs[0] = maxs[0];
	new_overlay->maxs[1] = maxs[1];
	new_overlay->maxs[2] = maxs[2];

	new_overlay->angles = orientation;

	new_overlay->faceColor = faceColor;
	new_overlay->edgeColor = edgeColor;

	new_overlay->SetEndTime( duration );

	new_overlay->m_pNextOverlay = s_pOverlays;
	s_pOverlays = new_overlay;
}

//-----------------------------------------------------------------------------
// Purpose: Add new overlay sphere
// Input  : Position of sphere
//			radius of sphere
//			color & alpha
//			duration
// Output :
//-----------------------------------------------------------------------------
void AddSphereOverlay(const Vector& vOrigin, float flRadius, int nTheta, int nPhi, int r, int g, int b, int a, float flDuration)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlaySphere_t *new_overlay = new OverlaySphere_t;

	new_overlay->vOrigin = vOrigin;
	new_overlay->flRadius = flRadius;

	new_overlay->nTheta = nTheta;
	new_overlay->nPhi = nPhi;

	new_overlay->r = r;
	new_overlay->g = g;
	new_overlay->b = b;
	new_overlay->a = a;

	new_overlay->SetEndTime( flDuration );

	new_overlay->m_pNextOverlay = s_pOverlays;
	s_pOverlays = new_overlay;
}


void AddSweptBoxOverlay(const Vector& start, const Vector& end, 
	const Vector& mins, const Vector& maxs, QAngle const& angles, int r, int g, int b, int a, float flDuration)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlaySweptBox_t *new_overlay = new OverlaySweptBox_t;

	new_overlay->start = start;
	new_overlay->end = end;

	new_overlay->mins[0] = mins[0];
	new_overlay->mins[1] = mins[1];
	new_overlay->mins[2] = mins[2];

	new_overlay->maxs[0] = maxs[0];
	new_overlay->maxs[1] = maxs[1];
	new_overlay->maxs[2] = maxs[2];

	new_overlay->angles = angles;

	new_overlay->r = r;
	new_overlay->g = g;
	new_overlay->b = b;
	new_overlay->a = a;

	new_overlay->SetEndTime( flDuration );

	new_overlay->m_pNextOverlay = s_pOverlays;
	s_pOverlays = new_overlay;
}


//-----------------------------------------------------------------------------
// Purpose: Add new overlay text
// Input  : Entity to attach text to
//			How many lines to offset text from entity origin
//			The text to print
// Output :
//-----------------------------------------------------------------------------
void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b, int a, bool noDepthTest, float flDuration) 
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayLine_t *new_loverlay = new OverlayLine_t;

	new_loverlay->origin[0] = origin[0];
	new_loverlay->origin[1] = origin[1];
	new_loverlay->origin[2] = origin[2];

	new_loverlay->dest[0] = dest[0];
	new_loverlay->dest[1] = dest[1];
	new_loverlay->dest[2] = dest[2];

	new_loverlay->r = r;
	new_loverlay->g = g;
	new_loverlay->b = b;
	new_loverlay->a = a;

	new_loverlay->noDepthTest = noDepthTest;

	new_loverlay->SetEndTime( flDuration );

	new_loverlay->m_pNextOverlay = s_pOverlays;
	s_pOverlays = new_loverlay;
}


//-----------------------------------------------------------------------------
// Purpose: Add new triangle overlay
//-----------------------------------------------------------------------------
void AddTriangleOverlay(const Vector& p1, const Vector& p2, const Vector &p3, 
				int r, int g, int b, int a, bool noDepthTest, float flDuration)
{
	if ( cl.IsPaused() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayTriangle_t *pTriangle = new OverlayTriangle_t;
	pTriangle->p1 = p1;
	pTriangle->p2 = p2;
	pTriangle->p3 = p3;

	pTriangle->r = r;
	pTriangle->g = g;
	pTriangle->b = b;
	pTriangle->a = a;

	pTriangle->noDepthTest = noDepthTest;
	pTriangle->SetEndTime( flDuration );

	pTriangle->m_pNextOverlay = s_pOverlays;
	s_pOverlays = pTriangle;
}


//------------------------------------------------------------------------------
// Purpose :	Draw a grid around the s_vGridPosition
// Input   :
// Output  :
//------------------------------------------------------------------------------
void DrawGridOverlay(void)
{
	AUTO_LOCK( s_OverlayMutex );
	static int gridSpacing		= 100;
	static int numHorzSpaces	= 16;
	static int numVertSpaces	= 3;

	Vector startGrid;
	startGrid[0] = gridSpacing*((int)s_vGridPosition[0]/gridSpacing);
	startGrid[1] = gridSpacing*((int)s_vGridPosition[1]/gridSpacing);
	startGrid[2] = s_vGridPosition[2];

	// Shift to the left
	startGrid[0] -= (numHorzSpaces/2)*gridSpacing;
	startGrid[1] -= (numHorzSpaces/2)*gridSpacing;

	Vector color( 20, 180, 190 );
	for (int i=1;i<numVertSpaces+1;i++)
	{
		// Draw x axis lines
		Vector startLine;
		VectorCopy(startGrid,startLine);
		for (int j=0;j<numHorzSpaces+1;j++)
		{
			Vector endLine;
			VectorCopy(startLine,endLine);
			endLine[0] += gridSpacing*numHorzSpaces;
			RenderLine( startLine, endLine, Color( color.x, color.y, color.z, 255 ), true );

			Vector bottomStartLine;
			VectorCopy(startLine,bottomStartLine);
			for ( int k=0; k<numHorzSpaces+1; k++ )
			{
				Vector bottomEndLine;
				VectorCopy(bottomStartLine,bottomEndLine);
				bottomEndLine[2] -= gridSpacing;
				RenderLine( bottomStartLine, bottomEndLine, Color( color.x, color.y, color.z, 255 ), true );
				bottomStartLine[0] += gridSpacing;
			}
			startLine[1] += gridSpacing;
		}

		// Draw y axis lines
		VectorCopy(startGrid,startLine);
		for ( int j=0; j<numHorzSpaces+1; j++ )
		{
			Vector endLine;
			VectorCopy(startLine,endLine);

			endLine[1] += gridSpacing*numHorzSpaces;
			RenderLine( startLine, endLine, Color( color.x, color.y, color.z, 255 ), true );
			startLine[0] += gridSpacing;
		}
		VectorScale( color, 0.7, color );
		startGrid[2] -= gridSpacing;
	}
	s_bDrawGrid	= false;
}


//------------------------------------------------------------------------------
// Draws a generic overlay
//------------------------------------------------------------------------------
void DrawOverlay( OverlayBase_t *pOverlay )
{
	AUTO_LOCK( s_OverlayMutex );
	switch( pOverlay->m_Type)
	{
	case OVERLAY_LINE:
		{
			// Draw the line
			OverlayLine_t *pLine = static_cast<OverlayLine_t*>(pOverlay);
			RenderLine( pLine->origin, pLine->dest, Color( pLine->r, pLine->g, pLine->b, pLine->a ), !pLine->noDepthTest);
		}
		break;

	case OVERLAY_BOX:
		{
			// Draw the box
			OverlayBox_t *pCurrBox = static_cast<OverlayBox_t*>(pOverlay);
			if ( pCurrBox->a > 0 ) 
			{
				RenderBox( pCurrBox->origin, pCurrBox->angles, pCurrBox->mins, pCurrBox->maxs, Color( pCurrBox->r, pCurrBox->g, pCurrBox->b, pCurrBox->a ), false );
			}
			RenderWireframeBox( pCurrBox->origin, pCurrBox->angles, pCurrBox->mins, pCurrBox->maxs, Color( pCurrBox->r, pCurrBox->g, pCurrBox->b, 255 ), true );
		}
		break;

	case OVERLAY_BOX2:
		{
			// Draw the box
			OverlayBox2_t *pCurrBox = static_cast<OverlayBox2_t*>(pOverlay);
			if ( pCurrBox->faceColor.a() > 0 ) 
			{
				RenderBox( pCurrBox->origin, pCurrBox->angles, pCurrBox->mins, pCurrBox->maxs, pCurrBox->faceColor, false );
			}
			if ( pCurrBox->edgeColor.a() > 0 )
			{
				RenderWireframeBox( pCurrBox->origin, pCurrBox->angles, pCurrBox->mins, pCurrBox->maxs, pCurrBox->edgeColor, false );
			}
		}
		break;

	case OVERLAY_SPHERE:
		{
			// Draw the sphere
			OverlaySphere_t *pSphere = static_cast<OverlaySphere_t*>(pOverlay);
			RenderSphere( pSphere->vOrigin, pSphere->flRadius, pSphere->nTheta, pSphere->nPhi, Color( pSphere->r, pSphere->g, pSphere->b, pSphere->a ), g_pMaterialAmbientCube );
		}
		break;

	case OVERLAY_SWEPT_BOX:
		{
			OverlaySweptBox_t *pBox = static_cast<OverlaySweptBox_t*>(pOverlay);
			RenderWireframeSweptBox( pBox->start, pBox->end, pBox->angles, pBox->mins, pBox->maxs, Color( pBox->r, pBox->g, pBox->b, pBox->a ), true );
		}
		break;

	case OVERLAY_TRIANGLE:
		{
			OverlayTriangle_t *pTriangle = static_cast<OverlayTriangle_t*>(pOverlay);
			RenderTriangle( pTriangle->p1, pTriangle->p2, pTriangle->p3, 
				Color( pTriangle->r, pTriangle->g, pTriangle->b, pTriangle->a ), !pTriangle->noDepthTest );
		}
		break;

	default:
		Assert(0);
	}
}

void DestroyOverlay( OverlayBase_t *pOverlay )
{
	AUTO_LOCK( s_OverlayMutex );
	switch( pOverlay->m_Type)
	{
	case OVERLAY_LINE:
		{
			OverlayLine_t *pCurrLine = static_cast<OverlayLine_t*>(pOverlay);
			delete pCurrLine;
		}
		break;

	case OVERLAY_BOX:
		{
			OverlayBox_t *pCurrBox = static_cast<OverlayBox_t*>(pOverlay);
			delete pCurrBox;
		}
		break;

	
	case OVERLAY_BOX2:
		{
			OverlayBox2_t *pCurrBox = static_cast<OverlayBox2_t*>(pOverlay);
			delete pCurrBox;
		}
		break;

	case OVERLAY_SPHERE:
		{
			OverlaySphere_t *pCurrSphere = static_cast<OverlaySphere_t*>(pOverlay);
			delete pCurrSphere;
		}
		break;

	case OVERLAY_SWEPT_BOX:
		{
			OverlaySweptBox_t *pCurrBox = static_cast<OverlaySweptBox_t*>(pOverlay);
			delete pCurrBox;
		}
		break;

	case OVERLAY_TRIANGLE:
		{
			OverlayTriangle_t *pTriangle = static_cast<OverlayTriangle_t*>(pOverlay);
			delete pTriangle;
		}
		break;

	default:
		Assert(0);
	}
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void DrawAllOverlays(void)
{
	if ( !enable_debug_overlays.GetBool() )
		return;

	AUTO_LOCK( s_OverlayMutex );
	OverlayBase_t* pCurrOverlay = s_pOverlays;
	OverlayBase_t* pPrevOverlay = NULL;
	OverlayBase_t* pNextOverlay;

	while (pCurrOverlay) 
	{
		// Is it time to kill this overlay?
		if ( pCurrOverlay->IsDead() )
		{
			if (pPrevOverlay)
			{
				// If I had a last overlay reset it's next pointer
				pPrevOverlay->m_pNextOverlay = pCurrOverlay->m_pNextOverlay;
			}
			else
			{
				// If the first line, reset the s_pOverlays pointer
				s_pOverlays = pCurrOverlay->m_pNextOverlay;
			}

			pNextOverlay = pCurrOverlay->m_pNextOverlay;
			DestroyOverlay( pCurrOverlay );
			pCurrOverlay = pNextOverlay;
		}
		else
		{
			DrawOverlay( pCurrOverlay );
			pPrevOverlay = pCurrOverlay;
			pCurrOverlay = pCurrOverlay->m_pNextOverlay;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Remove from the linkedlist overlays that have the special "until next
//			 server tick" frametime
// Input   :
// Output  :
//------------------------------------------------------------------------------
//0.01234f 
void PurgeServerOverlays( void )
{
	AUTO_LOCK( s_OverlayMutex );
	OverlayBase_t* pCurrOverlay = s_pOverlays;

	while (pCurrOverlay)
	{
		if ( pCurrOverlay->m_flEndTime == NDEBUG_PERSIST_TILL_NEXT_SERVER )
		{
			pCurrOverlay->m_flEndTime = cl.GetTime() + host_state.interval_per_tick;
		}

		pCurrOverlay = pCurrOverlay->m_pNextOverlay;
	}

	OverlayText_t* pCurrText = s_pOverlayText;
	while (pCurrText)
	{
		if ( pCurrText->m_flEndTime == NDEBUG_PERSIST_TILL_NEXT_SERVER )
		{
			pCurrText->m_flEndTime = cl.GetTime() + host_state.interval_per_tick;
		}

		pCurrText = pCurrText->nextOverlayText;
	}
}

void PurgeTextOverlays( void )
{
	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t* pCurrOverlay = s_pOverlayText;
	while ( pCurrOverlay ) 
	{
		if ( pCurrOverlay->m_flEndTime == 0.0f &&
			 pCurrOverlay->m_nCreationTick != -1 )
		{
			pCurrOverlay->m_nCreationTick = 0;
		}
		pCurrOverlay = pCurrOverlay->nextOverlayText;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void Draw3DOverlays(void)
{
	// Clear overlays every frame
	AUTO_LOCK( s_OverlayMutex );
	static int previous_servercount = 0;
	if ( previous_servercount != cl.m_nServerCount )
	{
		ClearAllOverlays();
		previous_servercount = cl.m_nServerCount;
	}

	DrawAllOverlays();

	if (s_bDrawGrid)
	{
		DrawGridOverlay();
	}
}


//------------------------------------------------------------------------------
// Purpose : Deletes all overlays
// Input   :
// Output  :
//------------------------------------------------------------------------------
void ClearAllOverlays(void)
{
	AUTO_LOCK( s_OverlayMutex );
	while (s_pOverlays) 
	{
		OverlayBase_t *pOldOverlay = s_pOverlays;
		s_pOverlays = s_pOverlays->m_pNextOverlay;
		DestroyOverlay( pOldOverlay );
	}

	while (s_pOverlayText) 
	{
		OverlayText_t *cur_ol = s_pOverlayText;
		s_pOverlayText = s_pOverlayText->nextOverlayText;
		delete cur_ol;
	}

	s_bDrawGrid = false;
}

void ClearDeadOverlays( void )
{
	AUTO_LOCK( s_OverlayMutex );
	OverlayText_t* pCurrText = s_pOverlayText;
	OverlayText_t* pLastText = NULL;
	OverlayText_t* pNextText = NULL;
	while (pCurrText) 
	{
		// Is it time to kill this Text?
		if ( pCurrText->IsDead() )
		{
			// If I had a last Text reset it's next pointer
			if (pLastText)
			{
				pLastText->nextOverlayText = pCurrText->nextOverlayText;
			}
			// If the first Text, reset the s_pOverlayText pointer
			else
			{
				s_pOverlayText = pCurrText->nextOverlayText;
			}
			pNextText = pCurrText->nextOverlayText;
			delete pCurrText;
			pCurrText = pNextText;
		}
		else
		{
			pLastText = pCurrText;
			pCurrText = pCurrText->nextOverlayText;
		}
	}
}

}	// end namespace CDebugOverlay


//-----------------------------------------------------------------------------
// Purpose:	export debug overlay to client DLL
// Input  :
// Output :
//-----------------------------------------------------------------------------
class CIVDebugOverlay : public IVDebugOverlay, public IVPhysicsDebugOverlay
{
private:
	char m_text[1024];
	va_list	m_argptr;

public:
	void AddEntityTextOverlay(int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *format, ...)
	{
		va_start( m_argptr, format );
		Q_vsnprintf( m_text, sizeof( m_text ), format, m_argptr );
		va_end( m_argptr );

		CDebugOverlay::AddEntityTextOverlay(ent_index, line_offset, duration, r, g, b, a, m_text);
	}

	void AddBoxOverlay(const Vector& origin, const Vector& mins, const Vector& max, QAngle const& angles, int r, int g, int b, int a, float duration)
	{
		CDebugOverlay::AddBoxOverlay(origin, mins, max, angles, r, g, b, a, duration);
	}

	void AddSweptBoxOverlay(const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration)
	{
		CDebugOverlay::AddSweptBoxOverlay(start, end, mins, max, angles, r, g, b, a, flDuration);
	}

	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration)
	{
		CDebugOverlay::AddLineOverlay(origin, dest, r, g, b, 255, noDepthTest, duration);
	}

	void AddTriangleOverlay(const Vector& p1, const Vector& p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration)
	{
		CDebugOverlay::AddTriangleOverlay(p1, p2, p3, r, g, b, a, noDepthTest, duration);
	}

	void AddTextOverlay(const Vector& origin, float duration, const char *format, ...)
	{
		va_start( m_argptr, format );
		Q_vsnprintf( m_text, sizeof( m_text ), format, m_argptr );
		va_end( m_argptr );

		CDebugOverlay::AddTextOverlay(origin, duration, m_text);
	}

	void AddTextOverlay(const Vector& origin, int line_offset, float duration, const char *format, ...)
	{
		va_start( m_argptr, format );
		Q_vsnprintf( m_text, sizeof( m_text ), format, m_argptr );
		va_end( m_argptr );

		CDebugOverlay::AddTextOverlay(origin, line_offset, duration, m_text);
	}

	void AddTextOverlayRGB(const Vector& origin, int line_offset, float duration, float r, float g, float b, float alpha, const char *format, ...)
	{
		va_start( m_argptr, format );
		Q_vsnprintf( m_text, sizeof( m_text ), format, m_argptr );
		va_end( m_argptr );

		CDebugOverlay::AddTextOverlay(origin, line_offset, duration, r, g, b, alpha, m_text);
	}

	void AddTextOverlayRGB(const Vector& origin, int line_offset, float flDuration, int r, int g, int b, int alpha, const char *format, ...)
	{
		va_start( m_argptr, format );
		Q_vsnprintf( m_text, sizeof( m_text ), format, m_argptr );
		va_end( m_argptr );

		CDebugOverlay::AddTextOverlay( origin, line_offset, flDuration, r * 1.0f/255.0f, g * 1.0f/255.0f, b * 1.0f/255.0f, alpha * 1.0f/255.0f, m_text );
	}

	void AddScreenTextOverlay(float flXPos, float flYPos,float flDuration, int r, int g, int b, int a, const char *text)
	{
		CDebugOverlay::AddScreenTextOverlay( flXPos, flYPos, flDuration, r, g, b, a, text );
	}
	
	void AddGridOverlay(const Vector& origin)
	{
		CDebugOverlay::AddGridOverlay( origin );
	}

	void AddCoordFrameOverlay(const matrix3x4_t& frame, float flScale, int vColorTable[3][3] = NULL)
	{
		CDebugOverlay::AddCoordFrameOverlay( frame, flScale, vColorTable );
	}

	int ScreenPosition(const Vector& point, Vector& screen)
	{
		return CDebugOverlay::ScreenPosition( point, screen );
	}

	int ScreenPosition(float flXPos, float flYPos, Vector& screen)
	{
		return CDebugOverlay::ScreenPosition( flXPos, flYPos, screen );
	}

	virtual OverlayText_t *GetFirst( void )
	{
		return CDebugOverlay::s_pOverlayText;
	}

	virtual OverlayText_t *GetNext( OverlayText_t *current )
	{
		return current->nextOverlayText;
	}

	virtual void ClearDeadOverlays( void )
	{
		CDebugOverlay::ClearDeadOverlays();
	}

	virtual void ClearAllOverlays()
	{
		CDebugOverlay::ClearAllOverlays();
	}

	void AddLineOverlayAlpha(const Vector& origin, const Vector& dest, int r, int g, int b, int a, bool noDepthTest, float duration)
	{
		CDebugOverlay::AddLineOverlay(origin, dest, r, g, b, a, noDepthTest, duration);
	}

	void AddBoxOverlay2( const Vector& origin, const Vector& mins, const Vector& maxs, QAngle const& orientation, const Color& faceColor, const Color& edgeColor, float duration )
	{
		CDebugOverlay::AddBoxOverlay2(origin, mins, maxs, orientation, faceColor, edgeColor, duration );
	}

	void PurgeTextOverlays()
	{
		CDebugOverlay::PurgeTextOverlays();
	}
};

static CIVDebugOverlay g_DebugOverlay;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CIVDebugOverlay, IVDebugOverlay, VDEBUG_OVERLAY_INTERFACE_VERSION, g_DebugOverlay );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CIVDebugOverlay, IVPhysicsDebugOverlay, VPHYSICS_DEBUG_OVERLAY_INTERFACE_VERSION, g_DebugOverlay );

