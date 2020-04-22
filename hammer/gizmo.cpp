//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "BoundBox.h"
#include "Gizmo.h"
#include "Render3D.h"
#include "Resource.h"
#include "materialsystem/imesh.h"
#include "TextureSystem.h"
#include "camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static IEditorTexture* g_pAxisTexture = 0;
static IEditorTexture* g_pRotateHandleTexture = 0;
static IEditorTexture* g_pScaleHandleTexture = 0;
static IEditorTexture* g_pTranslateHandleTexture = 0;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGizmo::CGizmo(void)
{
	static bool bFirst = true;

	if (bFirst)
	{
		g_pAxisTexture = g_Textures.FindActiveTexture("editor/gizmoAxis");
		g_pRotateHandleTexture = g_Textures.FindActiveTexture("editor/gizmoRotateHandle");
		g_pScaleHandleTexture = g_Textures.FindActiveTexture("editor/gizmoScaleHandle");
		g_pTranslateHandleTexture = g_Textures.FindActiveTexture("editor/gizmoTranslateHandle");
		bFirst = false;
	}

	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			z - 
//-----------------------------------------------------------------------------
CGizmo::CGizmo(float x, float y, float z)
{
	Initialize();

	m_Position[0] = x;
	m_Position[1] = y;
	m_Position[2] = z;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGizmo::Initialize(void)
{
	m_Position[0] = m_Position[1] = m_Position[2] = 0;

	m_fAxisLength = 100;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ViewPoint - 
//			Origin - 
//			EndPoint - 
//			red - 
//			green - 
//			blue - 
// Output : 
//-----------------------------------------------------------------------------
#define GIZMO_AXIS_WIDTH	2
#define GIZMO_HANDLE_WIDTH	4

void CGizmo::DrawGizmoAxis(CRender3D *pRender, Vector& Origin, 
						   Vector& EndPoint, int red, int green, int blue, 
						   unsigned int uAxisHandle)
{
	CCamera *pCamera = pRender->GetCamera();

	Vector ViewUp;
	pCamera->GetViewUp(ViewUp);

	Vector ViewPoint;
	Vector ViewForward;
	pCamera->GetViewPoint(ViewPoint);
	VectorSubtract(Origin, ViewPoint, ViewForward);

	Vector Axis;
	VectorSubtract(EndPoint, Origin, Axis);

	CrossProduct(ViewForward, Axis, ViewUp);
	VectorNormalize(ViewUp);

	Vector Start;
	Vector End;

	//
	// Draw the first segment of the gizmo axis.
	//
	VectorMA(Origin, 0.1, Axis, Start);
	VectorMA(Origin, 0.25, Axis, End);
	
	pRender->BindTexture( g_pAxisTexture );
	
	CMeshBuilder meshBuilder;

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.TexCoord2f(0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f(0, 2, 0);
	meshBuilder.Position3f(End[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f(0, 2, 1);
	meshBuilder.Position3f(End[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f(0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	//
	// Draw the scale handle.
	//
	Start = End;
	VectorMA(Origin, 0.35, Axis, End);
	
	pRender->BeginRenderHitTarget(this, uAxisHandle + GIZMO_HANDLE_SCALE);

	pRender->BindTexture( g_pScaleHandleTexture );

	pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.TexCoord2f( 0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] - ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] - ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 0);
	meshBuilder.Position3f(End[0] - ViewUp[0] * GIZMO_HANDLE_WIDTH, End[1] - ViewUp[1] * GIZMO_HANDLE_WIDTH, End[2] - ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 1);
	meshBuilder.Position3f(End[0] + ViewUp[0] * GIZMO_HANDLE_WIDTH, End[1] + ViewUp[1] * GIZMO_HANDLE_WIDTH, End[2] + ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] + ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] + ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->EndRenderHitTarget();

	//
	// Draw the second segment of the gizmo axis.
	//
	Start = End;
	VectorMA(Origin, 0.5, Axis, End);
	
	pRender->BindTexture( g_pAxisTexture );

	pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.TexCoord2f( 0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 2, 0);
	meshBuilder.Position3f(End[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.AdvanceVertex();
	meshBuilder.Color3ub(red, green, blue);

	meshBuilder.TexCoord2f( 0, 2, 1);
	meshBuilder.Position3f(End[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	//
	// Draw the rotate handle.
	//
	Start = End;
	VectorMA(Origin, 0.6, Axis, End);
	
	pRender->BeginRenderHitTarget(this, uAxisHandle + GIZMO_HANDLE_ROTATE);

	pRender->BindTexture( g_pRotateHandleTexture );
	pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.TexCoord2f( 0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] - ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] - ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 0);
	meshBuilder.Position3f(End[0] - ViewUp[0] * GIZMO_HANDLE_WIDTH, End[1] - ViewUp[1] * GIZMO_HANDLE_WIDTH, End[2] - ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 1);
	meshBuilder.Position3f(End[0] + ViewUp[0] * GIZMO_HANDLE_WIDTH, End[1] + ViewUp[1] * GIZMO_HANDLE_WIDTH, End[2] + ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] + ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] + ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->EndRenderHitTarget();

	//
	// Draw the third segment of the gizmo axis.
	//
	Start = End;
	VectorMA(Origin, 0.75, Axis, End);
	
	pRender->BindTexture( g_pAxisTexture );
	pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.TexCoord2f( 0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 2, 0);
	meshBuilder.Position3f(End[0] - ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] - ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] - ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 2, 1);
	meshBuilder.Position3f(End[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, End[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, End[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_AXIS_WIDTH, Start[1] + ViewUp[1] * GIZMO_AXIS_WIDTH, Start[2] + ViewUp[2] * GIZMO_AXIS_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	//
	// Draw the translate handle (arrowhead).
	//
	Start = End;
	
	pRender->BeginRenderHitTarget(this, uAxisHandle + GIZMO_HANDLE_TRANSLATE);

	pRender->BindTexture( g_pTranslateHandleTexture );
	pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 1 );

	meshBuilder.TexCoord2f( 0, 0, 0);
	meshBuilder.Position3f(Start[0] - ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] - ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] - ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 0.5);
	meshBuilder.Position3f(EndPoint[0], EndPoint[1], EndPoint[2]);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1);
	meshBuilder.Position3f(Start[0] + ViewUp[0] * GIZMO_HANDLE_WIDTH, Start[1] + ViewUp[1] * GIZMO_HANDLE_WIDTH, Start[2] + ViewUp[2] * GIZMO_HANDLE_WIDTH);
	meshBuilder.Color3ub(red, green, blue);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->EndRenderHitTarget();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//-----------------------------------------------------------------------------
void CGizmo::Render(CRender3D *pRender)
{
	Vector XAxis( m_Position[0] + m_fAxisLength, m_Position[1], m_Position[2] );
	Vector YAxis( m_Position[0], m_Position[1] + m_fAxisLength, m_Position[2] );
	Vector ZAxis( m_Position[0], m_Position[1], m_Position[2] + m_fAxisLength );

	static BoundBox UniformScaleBox;
	Vector Mins;
	Vector Maxs;
	Mins[0] = m_Position[0] - m_fAxisLength * 0.1;
	Mins[1] = m_Position[1] - m_fAxisLength * 0.1;
	Mins[2] = m_Position[2] - m_fAxisLength * 0.1;
	Maxs[0] = m_Position[0] + m_fAxisLength * 0.1;
	Maxs[1] = m_Position[1] + m_fAxisLength * 0.1;
	Maxs[2] = m_Position[2] + m_fAxisLength * 0.1;
	UniformScaleBox.ResetBounds();
	UniformScaleBox.UpdateBounds(Mins, Maxs);

	pRender->BeginRenderHitTarget(this, GIZMO_HANDLE_UNIFORM_SCALE);
	//pRender->RenderBox(Mins, Maxs, BoxType_Solid, 200, 200, 200);
	pRender->EndRenderHitTarget();

	pRender->SetRenderMode( RENDER_MODE_TEXTURED );

	DrawGizmoAxis(pRender, m_Position, XAxis, 255, 0, 0, GIZMO_AXIS_X);
	DrawGizmoAxis(pRender, m_Position, YAxis, 0, 255, 0, GIZMO_AXIS_Y);
	DrawGizmoAxis(pRender, m_Position, ZAxis, 0, 0, 255, GIZMO_AXIS_Z);

	pRender->SetRenderMode( RENDER_MODE_DEFAULT );
}

