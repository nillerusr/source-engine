//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rendering and mouse handling in the logical view.
//
//===========================================================================//

#include "stdafx.h"
#include "MapViewLogical.h"
#include "Render2D.h"
#include "MapWorld.h"
#include "TitleWnd.h"
#include "MapDoc.h"
#include "ToolManager.h"
#include "history.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_DYNCREATE(CMapViewLogical, CMapView2DBase)


BEGIN_MESSAGE_MAP(CMapViewLogical, CMapView2DBase)
	//{{AFX_MSG_MAP(CMapViewLogical)
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Logical View Look and feel constants
//-----------------------------------------------------------------------------

#define LOGICAL_CONN_VERT_SPACING 100
#define LOGICAL_CONN_HORIZ_SPACING 20
#define LOGICAL_CONN_SPREAD_DIST 50
#define LOGICAL_CONN_TEXT_LENGTH 450
#define LOGICAL_CONN_CROSS_SIZE 30
#define LOGICAL_CONN_MULTI_CIRCLE_RADIUS 15

// Broken connection blinking interval (in ms)
#define TIMER_BLINK_INTERVAL	512

// Unselected and selected color values for the connections palette against the background color

#define DARK	112
#define	MID		144

#define BACKGROUND	168

#define LITE	224
#define BRITE	255

#define LOGICAL_CONN_COLOR_COUNT		7
#define LOGICAL_CONN_SELECTION_STATES	2

static color32 s_pWireColors[LOGICAL_CONN_COLOR_COUNT][LOGICAL_CONN_SELECTION_STATES] = 
{
	{ { MID, MID, 0,	255 },		/* Mid Yellow */	{ BRITE, BRITE, 0,	255 },		/* Bright Yellow  */	},
	{ { MID, DARK, 0,	255 },		/* Dark Orange */	{ BRITE, MID, 0,	255 },		/* Bright Orange */		},
	{ { 0, DARK, 0,		255 },		/* Dark Green */	{ 0, BRITE, 0,		255 },		/* Bright Green */		},
	{ { 0, MID, MID,	255 },		/* Mid Cyan */		{ 0, BRITE, BRITE,	255 },		/* Bright Cyan */		},
	{ { 0, 0, MID,		255 },		/* Mid Blue */		{ 0, MID, BRITE,	255 },		/* Bright Baby Blue */	},
	{ { MID, 0, MID,	255 },		/* Mid Magenta */	{ BRITE, 0, BRITE,	255 },		/* Bright Magenta */	},
	{ { DARK, DARK, DARK, 255 },	/* Dark Gray */		{ BRITE, BRITE, BRITE, 255 },	/* Bright White */		},
};

static color32 s_pBrokenWireColor[LOGICAL_CONN_SELECTION_STATES] =
{
	{ DARK, 0, 0, 255 },			/* Dark Red */		{ BRITE, 0, 0, 255 },			/* Bright Red */	
};

//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//	---------------------------------------------------------------------------
CMapViewLogical::CMapViewLogical(void) : m_RenderDict( 0, 1024, DefLessFunc( CMapClass* ) )
{
	m_bUpdateRenderObjects = true;
	SetAxes(AXIS_X, FALSE, AXIS_Y, TRUE);
	SetDrawType( VIEW_LOGICAL );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees dynamically allocated resources.
//-----------------------------------------------------------------------------
CMapViewLogical::~CMapViewLogical(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: First-time initialization of this view.
//-----------------------------------------------------------------------------
void CMapViewLogical::OnInitialUpdate(void)
{
	CreateTitleWindow();
	GetTitleWnd()->SetTitle("Logical");
	SetZoom(0);  // Zoom out as far as possible.
	UpdateClientView();
	CMapView2DBase::OnInitialUpdate();
	// FIXME: Hardcoded light gray background - should be from a new "Logical View" options settings dialog
	m_ClearColor.SetColor( BACKGROUND, BACKGROUND, BACKGROUND, 255 );  
	m_clrGrid.SetColor( MID, MID, MID, 255 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : point - Point in client coordinates.
//			bMakeFirst - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapViewLogical::SelectAtCascading( const Vector2D &ptClient, bool bMakeFirst )
{
	CMapDoc *pDoc = GetMapDoc();
	CSelection *pSelection = pDoc->GetSelection();

	pSelection->ClearHitList();

	GetHistory()->MarkUndoPosition(pSelection->GetList(), "Selection");

	//
	// Check all the objects in the world for a hit at this point.
	//

	HitInfo_t HitData[MAX_PICK_HITS];
	int nHits = ObjectsAt(ptClient, HitData, MAX_PICK_HITS);
	
	// If there were no hits at the given point, clear selection.
	if ( nHits == 0 )
	{
		if (bMakeFirst)
		{
			pDoc->SelectFace(NULL, 0, scClear|scSaveChanges);
			pDoc->SelectObject(NULL, scClear|scSaveChanges);
		}

		return false;
	}

	SelectMode_t eSelectMode = pSelection->GetMode();

	for ( int i=0; i<nHits; ++i )
	{
		CMapClass *pSelObject = HitData[i].pObject->PrepareSelection( eSelectMode );
		if ( !pSelObject )
			continue;
		
		pSelection->AddHit( pSelObject );
	}

	//
	// Select a single object.
	//
	if ( bMakeFirst )
	{
		pDoc->SelectFace( NULL, 0, scClear|scSaveChanges );
		pDoc->SelectObject( NULL, scClear|scSaveChanges );
	}

	pSelection->SetCurrentHit( hitFirst, true );

	return true;
}


//-----------------------------------------------------------------------------
// Base class calls this when render lists need rebuilding
//-----------------------------------------------------------------------------
void CMapViewLogical::OnRenderListDirty()
{
	m_bUpdateRenderObjects = true;
}

	
//-----------------------------------------------------------------------------
// Purpose: Builds up list of mapclasses to render
//-----------------------------------------------------------------------------
void CMapViewLogical::AddToRenderLists( CMapClass *pObject )
{
#if _DEBUG && 0
	CMapEntity	*pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity)
	{
		LPCTSTR	pszTargetName = pEntity->GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "relay_cancelVCDs") )
		{
			// Set breakpoint here for debugging this entity's visiblity
			int foo = 0;
		}
	}
#endif

	if ( !pObject->IsVisibleLogical() )
		return;
	
	// Don't render groups, render their children instead.
	if ( !pObject->IsGroup() && pObject->IsLogical() )
	{
		Vector2D vecMins, vecMaxs;
		pObject->GetRenderLogicalBox( vecMins, vecMaxs );

// Always paint all the entities to ensure that any inter-entity connections are visible
//		if ( !IsValidBox( vecMins, vecMaxs ) || IsInClientView( vecMins, vecMaxs ) )
		{
			// Make sure the object is in the update region.
			m_RenderList.AddToTail( pObject );
			m_ConnectionList.AddToTail( pObject );
			m_RenderDict.Insert( pObject );
		}
	}

	// Recurse into children and add them.
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		AddToRenderLists( pChildren->Element(pos) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Builds up list of mapclasses to render
//-----------------------------------------------------------------------------
void CMapViewLogical::PopulateConnectionList( )
{			  
	while ( m_ConnectionUpdate.Count() )
	{
		CMapClass *pObject;
		m_ConnectionUpdate.Pop( pObject );
		if ( !pObject->IsVisibleLogical() )
			continue;

		// Don't render groups, render their children instead.
		if ( !pObject->IsGroup() && pObject->IsLogical() )
		{
			// Don't add it if it's visible already
			if ( m_RenderDict.Find( pObject ) == m_RenderDict.InvalidIndex() )
			{
				CEditGameClass *pClass = dynamic_cast< CEditGameClass * >( pObject );
				if ( pClass )
				{
					int nCount = pClass->Connections_GetCount();
					for ( int i = 0; i < nCount; ++i )
					{
						CEntityConnection *pConn = pClass->Connections_Get( i );
						 
						// Find the input entity associated with this connection
						CMapEntityList *pEntityList = pConn->GetTargetEntityList();

						int j;
						int nInputCount = pEntityList->Count();
						for ( j = 0; j < nInputCount; ++j )
						{
							CMapEntity *pEntity = pEntityList->Element(j);
							if ( m_RenderDict.Find( pEntity ) != m_RenderDict.InvalidIndex() )
							{ 
								m_ConnectionList.AddToTail( pObject );
								break;
							}
						}
						if ( j != nInputCount )
							break;
					}
				}
			}
		}

		// Recurse into children and add them.
		const CMapObjectList *pChildren = pObject->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			m_ConnectionUpdate.Push( pChildren->Element(pos) );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CMapViewLogical::OnTimer(UINT nIDEvent) 
{
	if ( nIDEvent == TIMER_CONNECTIONUPDATE )
	{
		// Make sure we don't blink too fast
		static unsigned int	nLastUpdate = 0;
		if ( GetTickCount() >= (nLastUpdate+TIMER_BLINK_INTERVAL/2) )
		{
			nLastUpdate = GetTickCount();
            UpdateView( 0 ); // Force the view to redraw for blinking errors
		}
	}
	else
		CView::OnTimer(nIDEvent);
}


const color32 & CMapViewLogical::GetWireColor(const char *pszName, const bool bSelected, const bool bError, const bool bAnySelected)
{
	// Select the connecting color based on the string passed in 
	// (using OutputName for instance, gives varying "wire colors" by entity output type).

	Assert( LOGICAL_CONN_COLOR_COUNT == (sizeof(s_pWireColors) / sizeof(color32) ) / LOGICAL_CONN_SELECTION_STATES );

	if ( !bError )
	{

		int nIndex = 0;

		// Has the named passed in
		while ( *pszName )
			nIndex += *pszName++;

		// Index based on the number of colors available
		nIndex %= LOGICAL_CONN_COLOR_COUNT;

		// Only blink non-errors if the drawing object is selected 
//		bool bBlinkingState = bSelected ? (GetTickCount() / TIMER_BLINK_INTERVAL) & 1 : 0;
		return s_pWireColors[nIndex][bSelected];
	}
	else
	{
		// Only blink errors if nothing is selected, or this is selected
		bool	bBlinkingState =  bSelected || !bAnySelected ? (GetTickCount() / TIMER_BLINK_INTERVAL) & 1 : bSelected;
		return s_pBrokenWireColor[bBlinkingState];
	}
}


//-----------------------------------------------------------------------------
// Draws a wire from a particular point to a target
//-----------------------------------------------------------------------------
#define BACKWARD_WIRE_OVERSHOOT 50
#define BACKWARD_WIRE_Y_DISTANCE 150

void CMapViewLogical::DrawConnectingWire( float x, float y, CMapEntity *pSource, CEntityConnection *pConnection, CMapEntity *pTarget )
{
	CRender2D *pRender = GetRender();
	  
	// FIXME: Deal with bad input type
	Vector2D vecEndPosition, vecConnector;
	pTarget->GetLogicalConnectionPosition( LOGICAL_CONNECTION_INPUT, vecConnector );
	vecEndPosition = vecConnector;

	int		nInputs = pTarget->Upstream_GetCount();

	// Compensate for multiple inputs -- fan-in the connections from the various pSource entities
	BOOL bFound = false;
	if ( nInputs )
	{
		int nInput;
		for ( nInput = 0; nInput < nInputs; nInput++ )
		{
			CEntityConnection *pInputConnection = pTarget->Upstream_Get(nInput);
			if ( pInputConnection )
			{
				if ( pInputConnection == pConnection )
				{
					bFound = true;
					break;
				}
			}
		}
		if (bFound)
		{
			vecEndPosition.x -= LOGICAL_CONN_SPREAD_DIST;
			vecEndPosition.y += ( (nInputs - 1) * LOGICAL_CONN_VERT_SPACING / 2 ) / 2 - (nInput * LOGICAL_CONN_VERT_SPACING / 2);

			pRender->MoveTo( Vector( vecEndPosition.x, vecEndPosition.y, 0.0f) );
			pRender->DrawLineTo( Vector( vecConnector.x, vecConnector.y, 0.0f) );
		}
		else
		{
			Assert(0);
		}
	}

	pRender->MoveTo( Vector( x, y, 0.0f ) );
	if ( x < vecEndPosition.x )
	{
		// Do direct connection
		pRender->DrawLineTo( Vector( x, vecEndPosition.y, 0.0f ) );
		pRender->DrawLineTo( Vector( vecEndPosition.x, vecEndPosition.y, 0.0f ) );
		return;
	}

	Vector2D vecTargetMins, vecTargetMaxs;
	pTarget->GetRenderLogicalBox( vecTargetMins, vecTargetMaxs );
	vecTargetMins.y -= BACKWARD_WIRE_Y_DISTANCE;
	vecTargetMaxs.y += BACKWARD_WIRE_Y_DISTANCE;

	float flHalfY = ( y + vecEndPosition.y ) * 0.5f;
	if ( flHalfY > vecTargetMins.y && flHalfY < vecTargetMaxs.y )
	{
		flHalfY = ( flHalfY < vecEndPosition.y ) ? vecTargetMins.y : vecTargetMaxs.y; 
	}

	pRender->DrawLineTo( Vector( x, flHalfY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecEndPosition.x - BACKWARD_WIRE_OVERSHOOT, flHalfY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecEndPosition.x - BACKWARD_WIRE_OVERSHOOT, vecEndPosition.y, 0.0f ) );
	pRender->DrawLineTo( Vector( vecEndPosition.x, vecEndPosition.y, 0.0f ) );
}

void CMapViewLogical::RenderConnections(const bool bDrawSelected, const bool bAnySelected)
{
	Vector2D pt, pt2;
	WorldToClient( pt, Vector( 0.0f, 0.0f, 0.0f ) );
	WorldToClient( pt2, Vector( 0.0f, LOGICAL_CONN_VERT_SPACING, 0.0f ) );
	
	int nCount = m_ConnectionList.Count();

	for ( int i = 0; i < nCount ; ++i )
	{
		CMapEntity *pMapClass = dynamic_cast<CMapEntity*>( m_ConnectionList[i] );
		if ( !pMapClass )
			continue;

		CEditGameClass *pEditClass = dynamic_cast<CEditGameClass*>( pMapClass );
		if ( !pEditClass )
			continue;

		int nConnectionCount = pEditClass->Connections_GetCount();
		if ( nConnectionCount == 0 )
			continue;

		Vector2D vecStartPosition;
		pMapClass->GetLogicalConnectionPosition( LOGICAL_CONNECTION_OUTPUT, vecStartPosition );

		CRender2D *pRender = GetRender();

		float x = vecStartPosition.x + LOGICAL_CONN_SPREAD_DIST + LOGICAL_CONN_TEXT_LENGTH;
		float y = vecStartPosition.y + ( nConnectionCount - 1 ) * LOGICAL_CONN_VERT_SPACING / 2;

		for ( int j = 0; j < nConnectionCount; ++j )
		{
			CEntityConnection *pConn = pEditClass->Connections_Get( j );

			// Find the input entity associated with this connection
			CMapEntityList *pEntityList = pConn->GetTargetEntityList();
			int nInputCount = pEntityList->Count();
			bool bBadInput = !MapEntityList_HasInput( pEntityList, pConn->GetInputName() );
			bool bBadConnection = (nInputCount == 0);

			// Stop drawing entity connection text once the entity itself gets too small.
			bool bDrawOutput = ( fabs( pt.y - pt2.y ) >= 16 );
			bool bDrawDelay = ( fabs( pt.y - pt2.y ) >= 20 );
			bool bDrawInput = ( fabs( pt.y - pt2.y ) >= 24 );
			bool bDrawTarget = ( fabs( pt.y - pt2.y ) >= 28 );
	  
			bool bEntitySelected = ( pMapClass->GetSelectionState() != SELECT_NONE );
			bool bInputSelected = false;

			for ( int k = 0; k < nInputCount; ++k )
			{
				if ( pEntityList->Element( k )->GetSelectionState() != SELECT_NONE )
					bInputSelected = true;

				// Make sure all the connected entities are all visible
				if ( pEntityList->Element( k )->IsVisibleLogical() == false )
					bBadConnection = true;
			}

			// Make sure we only draw the selected OR unselected connections as requested
			if ( bDrawSelected == (bEntitySelected || bInputSelected) )
			{
				color32 c = GetWireColor( pConn->GetOutputName(), 
										  bEntitySelected || bInputSelected, 
										  bBadConnection || bBadInput, 
										  bAnySelected );

				if ( bDrawDelay || bDrawOutput || bDrawInput || bDrawTarget )
				{
					char pBuf[1024];
					
 					pRender->SetTextColor( c.r, c.g, c.b );

					int nChars = 0;
					if ( bDrawOutput )
						nChars += Q_snprintf( pBuf+nChars, sizeof(pBuf), "%s", pConn->GetOutputName() );
					if ( bDrawDelay )
						nChars += Q_snprintf( pBuf+nChars, sizeof(pBuf), "(%.2f)", pConn->GetDelay() );

					if ( nChars )
						pRender->DrawText( pBuf, Vector2D( vecStartPosition.x + LOGICAL_CONN_SPREAD_DIST, y ), 2, 1, CRender2D::TEXT_JUSTIFY_TOP | CRender2D::TEXT_JUSTIFY_RIGHT );
					
					nChars = 0;
					if ( bDrawInput )
						nChars += Q_snprintf( pBuf+nChars, sizeof(pBuf), "%s", pConn->GetInputName() );
					if ( bDrawTarget )
						nChars += Q_snprintf( pBuf+nChars, sizeof(pBuf), "[%s] ", pConn->GetTargetName() );

					if ( nChars )
						pRender->DrawText( pBuf, Vector2D( vecStartPosition.x + LOGICAL_CONN_SPREAD_DIST, y ), 2, -1, CRender2D::TEXT_JUSTIFY_BOTTOM | CRender2D::TEXT_JUSTIFY_RIGHT );
				}

				pRender->SetDrawColor( c.r, c.g, c.b );
				pRender->MoveTo( Vector( vecStartPosition.x, vecStartPosition.y, 0.0f ) );
				pRender->DrawLineTo( Vector( vecStartPosition.x + LOGICAL_CONN_SPREAD_DIST, y, 0.0f ) );
				pRender->DrawLineTo( Vector( x, y, 0.0f ) );
			    
				if ( bBadConnection )
				{
					// Draw an X for a bogus connection.
 					pRender->MoveTo( Vector( x - LOGICAL_CONN_CROSS_SIZE, y - LOGICAL_CONN_CROSS_SIZE, 0.0f ) );
					pRender->DrawLineTo( Vector( x + LOGICAL_CONN_CROSS_SIZE, y + LOGICAL_CONN_CROSS_SIZE, 0.0f ) );
					pRender->MoveTo( Vector( x - LOGICAL_CONN_CROSS_SIZE, y + LOGICAL_CONN_CROSS_SIZE, 0.0f ) );
					pRender->DrawLineTo( Vector( x + LOGICAL_CONN_CROSS_SIZE, y - LOGICAL_CONN_CROSS_SIZE, 0.0f ) );
				}
				else if ( nInputCount == 1 )
				{
					DrawConnectingWire( x, y, pMapClass, pConn, pEntityList->Element( 0 ) );
				}
				else
				{
					// Draw a circle for multiple connections
					pRender->DrawCircle( Vector( x + LOGICAL_CONN_MULTI_CIRCLE_RADIUS, y, 0.0f ), LOGICAL_CONN_MULTI_CIRCLE_RADIUS );

					float mx = x + LOGICAL_CONN_SPREAD_DIST;
					float my = y + ( nInputCount / 2 ) * LOGICAL_CONN_VERT_SPACING/2;

					Vector vecStart( x + LOGICAL_CONN_MULTI_CIRCLE_RADIUS, y, 0.0f );
					for ( int k = 0; k < nInputCount; ++k )
					{ 
						// bBadInput = false; // This should be based on whether downstream entity has the specificied named input
						bInputSelected = ( pEntityList->Element( k )->GetSelectionState() != SELECT_NONE );
						color32 col = GetWireColor( pConn->GetOutputName(), 
												  bEntitySelected || bInputSelected,
												  bBadInput,
												  bAnySelected );

						pRender->SetDrawColor( col.r, col.g, col.b );

						Vector vecEnd( mx, my, 0.0f );
						Vector vecDelta;
						VectorSubtract( vecEnd, vecStart, vecDelta );
						VectorNormalize( vecDelta );
						vecDelta *= LOGICAL_CONN_MULTI_CIRCLE_RADIUS;
						vecDelta += vecStart;

						pRender->MoveTo( vecDelta );
						pRender->DrawLineTo( Vector( mx, my, 0.0f ) );
						DrawConnectingWire( mx, my, pMapClass, pConn, pEntityList->Element( k ) );

						mx += LOGICAL_CONN_HORIZ_SPACING;
						my -= LOGICAL_CONN_VERT_SPACING/2;
					}
				}
			}

			x += LOGICAL_CONN_HORIZ_SPACING * (nInputCount+1) + 2*LOGICAL_CONN_MULTI_CIRCLE_RADIUS;
			y -= LOGICAL_CONN_VERT_SPACING;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapViewLogical::Render()
{
	CMapDoc *pDoc = GetMapDoc();
	CMapWorld *pWorld = pDoc->GetMapWorld();

	GetRender()->StartRenderFrame();
	
	// Draw grid if enabled.
	if ( pDoc->m_bShowLogicalGrid )
	{
		DrawGridLogical( GetRender() );
	}

	// Draw the world if we have one.
	if (pWorld == NULL)
		return;
			   
	// Traverse the entire world, sorting visible elements into two arrays:
	// Normal objects and selected objects, so that we can render the selected
	// objects last.
	if ( m_bUpdateRenderObjects )
	{
		m_bUpdateRenderObjects = false;

		m_RenderList.RemoveAll();
		m_RenderDict.RemoveAll();
		m_ConnectionList.RemoveAll();
		m_ConnectionUpdate.Clear();

		// fill render lists with visible objects
		AddToRenderLists( pWorld );

		// Add the connections too
		m_ConnectionUpdate.Push( pWorld );
		PopulateConnectionList();

		// Make sure we have a timer running to drive error animations
		SetTimer( TIMER_CONNECTIONUPDATE, TIMER_BLINK_INTERVAL, NULL);
	}

	// Assume we are blinking, unless something is selected.
	bool bAnySelected = FALSE;

	CUtlVector<CMapClass *> selectedObjects;
	CUtlVector<CMapClass *> helperObjects;
	CUtlVector<CMapClass *> unselectedObjects;

	// Render normal (nonselected) objects first
	for (int i = 0; i < m_RenderList.Count(); i++)
	{
		CMapClass *pObject = m_RenderList[i];

		if ( pObject->IsSelected() )
		{
			bAnySelected = TRUE;

			// render later
			if ( pObject->GetToolObject( 0, false ) )
			{
				helperObjects.AddToTail( pObject );
			}
			else
			{
				selectedObjects.AddToTail( pObject );
			}
		}
		else
		{
			unselectedObjects.AddToTail( pObject );
		}
	}
	
	// Render unselected connections first
	RenderConnections(false, bAnySelected);

	// render unselected objects next, on top of the connections
	for ( int j = 0; j < unselectedObjects.Count(); j++ )
	{
		unselectedObjects[j]->RenderLogical( GetRender() );
	}

	if ( bAnySelected )
	{
		// Render selected objects in second batch, so they overdraw normal object
		for (int i = 0; i < selectedObjects.Count(); i++)
		{
			selectedObjects[i]->RenderLogical( GetRender() );
		}

		// Render selected connections on top of everything else
		RenderConnections(true, bAnySelected);
	}

	// render all tools
	CBaseTool *pCurTool = pDoc->GetTools()->GetActiveTool();

	int nToolCount = pDoc->GetTools()->GetToolCount();
	for (int i = 0; i < nToolCount; i++)
	{
		CBaseTool *pTool = pDoc->GetTools()->GetTool(i);
		if ((pTool != NULL) && (pTool != pCurTool))
		{
			pTool->RenderToolLogical( GetRender() );
		}
	}

	// render active tool over all other tools
	if ( pCurTool )
	{
		pCurTool->RenderToolLogical( GetRender() );
	}

	// render map helpers at last
	for (int i = 0; i < helperObjects.Count(); i++)
	{
		helperObjects[i]->RenderLogical( GetRender() );
	}

	GetRender()->EndRenderFrame();
}


//-----------------------------------------------------------------------------
// convert client view space to map world coordinates (2D versions for convenience) 
//-----------------------------------------------------------------------------
void CMapViewLogical::WorldToClient( Vector2D &ptClient, const Vector2D &vWorld )
{
	Vector vWorld3D( vWorld.x, vWorld.y, 0.0f );
	CMapView2DBase::WorldToClient( ptClient, vWorld3D );
}

void CMapViewLogical::ClientToWorld( Vector2D &vWorld, const Vector2D &vClient )
{
	Vector vWorld3D;
	CMapView2DBase::ClientToWorld( vWorld3D, vClient );
	vWorld.x = vWorld3D.x;
	vWorld.y = vWorld3D.y;
}

void CMapViewLogical::WorldToClient( Vector2D &ptClient, const Vector &vWorld )
{
	CMapView2DBase::WorldToClient( ptClient, vWorld );
}

void CMapViewLogical::ClientToWorld( Vector &vWorld, const Vector2D &vClient )
{
	CMapView2DBase::ClientToWorld( vWorld, vClient );
}
