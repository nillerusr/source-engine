//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base rendering utilities for all views
//
// $NoKeywords: $
//===========================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include "mathlib/vmatrix.h"
#include "Render.h"
#include "Camera.h"
#include "Material.h"
#include "materialsystem/imesh.h"
#include "datacache\imdlcache.h"
#include "hammer.h"
#include "hammer_mathlib.h"
#include "vgui_controls/Controls.h"
#include "vgui/IScheme.h"
#include "texturesystem.h"
#include "IStudioRender.h"
#include "builddisp.h"
#include "mapview.h"
#include "material.h"
#include <renderparm.h>
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "vphysics_interface.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiWnd.h"
#include "Box3D.h"
#include "MapInstance.h"

extern IMatSystemSurface *g_pMatSystemSurface;

static float s_fOneUnitLength = 1;

CRender::CRender(void)
{
	m_pView = NULL;
	
	// returns a handle to the default (first loaded) scheme
	vgui::IScheme * pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetDefaultScheme() );
	if ( pScheme )
	{	
		m_DefaultFont = pScheme->GetFont( "Default" );
	}
	else
	{
		m_DefaultFont = vgui::INVALID_FONT;

		static bool s_bOnce = false;
		if ( !s_bOnce )
		{
			s_bOnce = true;
			MessageBox( NULL, "Failed to load the default scheme file. The map views may be missing some visual elements.", "Error", MB_OK | MB_ICONEXCLAMATION );
		}
	}
	
	for (int i = 0; i < 2; ++i)
	{
		m_pFlat[i] = NULL;
		m_pWireframe[i] = NULL;
		m_pTranslucentFlat[i] = NULL;
		m_pLightmapGrid[i] = NULL;
		m_pSelectionOverlay[i] = NULL;
		m_pDotted[i] = NULL;
		m_pFlatNoZ[i] = NULL;
		m_pFlatNoCull[i] = NULL;
	}

	m_pCurrentMaterial = NULL;
	m_pBoundMaterial = NULL;

	m_nDecalMode = 0;
	
	m_bIsRendering = false;
	m_bIsClientSpace = false;
	m_bIsLocalTransform = false;
	m_bIsRenderingIntoVGUI = false;

	VMatrix	IdentityMatrix;

	IdentityMatrix.Identity();
	m_LocalMatrix.AddToHead( IdentityMatrix );

	m_OrthoMatrix.Identity();
	m_eCurrentRenderMode = m_eDefaultRenderMode = RENDER_MODE_FLAT;

	m_bInstanceRendering = false;
	m_nInstanceCount = 0;
	m_InstanceSelectionDepth = 0;

	PushInstanceData( NULL, Vector( 0.0f, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ) ); // always add a default state

	UpdateStudioRenderConfig( false, false );
}

CRender::~CRender(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: this function will push all of the instance data
// Input  : pInstanceClass - the func_instance entity
//			InstanceOrigin - the translation of the instance
//			InstanceAngles - the rotation of the instance
// Output : none
//-----------------------------------------------------------------------------
void CRender::PushInstanceData( CMapInstance *pInstanceClass, Vector &InstanceOrigin, QAngle &InstanceAngles )
{
	TInstanceState	InstanceState;
	matrix3x4_t		Instance3x4Matrix;

	InstanceState.m_InstanceOrigin = InstanceOrigin;
	InstanceState.m_InstanceAngles = InstanceAngles;
	InstanceState.m_pInstanceClass = pInstanceClass;
	InstanceState.m_pTopInstanceClass = NULL;

	AngleMatrix( InstanceState.m_InstanceAngles, InstanceState.m_InstanceOrigin, Instance3x4Matrix );
	InstanceState.m_InstanceMatrix.Init( Instance3x4Matrix );

	Vector		vecTransformedOrigin;
	TransformInstanceVector( InstanceState.m_InstanceOrigin, vecTransformedOrigin );
	m_CurrentInstanceState.m_InstanceOrigin = vecTransformedOrigin;
	//	RotateInstanceVector( ( Vector )InstanceState.m_InstanceAngles, m_CurrentInstanceState.m_InstanceAngles );		no one uses this right now   make sure to store it in the same fashion as vecTransformedOrigin

	if ( m_InstanceState.Count() > 0 )
	{	// first push is just a default state
		m_bInstanceRendering = true;
		BeginLocalTransfrom( InstanceState.m_InstanceMatrix, true );
		if ( m_CurrentInstanceState.m_pTopInstanceClass == NULL ) 
		{
			if ( pInstanceClass->IsEditable() == false )
			{
				InstanceState.m_pTopInstanceClass = pInstanceClass;
			}
		}
		else
		{
			InstanceState.m_pTopInstanceClass = m_CurrentInstanceState.m_pTopInstanceClass;
		}
		if ( pInstanceClass->IsSelected() || m_InstanceSelectionDepth )
		{
			m_InstanceSelectionDepth++;
		}
		InstanceState.m_InstanceMatrix = m_CurrentInstanceState.m_InstanceMatrix * InstanceState.m_InstanceMatrix;
		InstanceState.m_bIsEditable = pInstanceClass->IsEditable();
	}
	else
	{
		m_bInstanceRendering = false;
		InstanceState.m_bIsEditable = true;
	}

	InstanceState.m_InstanceRenderMatrix = m_LocalMatrix.Head();
	m_InstanceState.AddToHead( InstanceState );
	m_CurrentInstanceState = InstanceState;

	if ( !pInstanceClass )
	{
	}
	else if ( m_InstanceSelectionDepth > 0 )
	{
		PushInstanceRendering( INSTANCE_STACE_SELECTED );
	}
	else if ( pInstanceClass->IsEditable() || CMapDoc::GetActiveMapDoc()->GetShowInstance() == INSTANCES_SHOW_NORMAL )
	{
		PushInstanceRendering( INSTANCE_STATE_OFF );
	}
	else
	{
		PushInstanceRendering( GetInstanceClass()->IsSelected() ? INSTANCE_STACE_SELECTED : INSTANCE_STATE_ON );
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will pop off the top most instance data
//-----------------------------------------------------------------------------
void CRender::PopInstanceData( void )
{
	if ( m_CurrentInstanceState.m_pInstanceClass )
	{
		PopInstanceRendering();
	}

	m_InstanceState.Remove( 0 );
	m_CurrentInstanceState = m_InstanceState.Head();

	if ( m_InstanceState.Count() > 1 )
	{	// first push is just a default state
		m_bInstanceRendering = true;
	}
	else
	{
		m_bInstanceRendering = false;
	}

	if ( m_InstanceSelectionDepth > 0 )
	{
		m_InstanceSelectionDepth--;
	}

	EndLocalTransfrom();

	//	m_CurrentInstanceState.m_InstanceRenderMatrix = m_LocalMatrix.Head();
}


//-----------------------------------------------------------------------------
// Purpose: this function initializes the stencil buffer for instance rendering
//-----------------------------------------------------------------------------
void CRender::PrepareInstanceStencil( void )
{
	CMatRenderContextPtr	pRenderContext( MaterialSystemInterface() );

#if STENCIL_AS_CALLS
	pRenderContext->SetStencilEnable( true );
	pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );
	pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
	pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
	pRenderContext->SetStencilWriteMask( 0xff );
	pRenderContext->SetStencilTestMask( 0xff );
	pRenderContext->SetStencilReferenceValue( 0x01 );
	pRenderContext->SetStencilPassOperation( STENCILOPERATION_ZERO);
#else
	m_ShaderStencilState.m_bEnable = true;
	m_ShaderStencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
	m_ShaderStencilState.m_FailOp = SHADER_STENCILOP_KEEP;
	m_ShaderStencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
	m_ShaderStencilState.m_nWriteMask = 0xff;
	m_ShaderStencilState.m_nTestMask = 0xff;
	m_ShaderStencilState.m_nReferenceValue = 0x01;
	m_ShaderStencilState.m_PassOp = SHADER_STENCILOP_ZERO;
	pRenderContext->SetStencilState( m_ShaderStencilState );
#endif // STENCIL_AS_CALLS
}


//-----------------------------------------------------------------------------
// Purpose: this function will draw the various alpha color 2d rectangles for the instance states
//-----------------------------------------------------------------------------
void CRender::DrawInstanceStencil( void )
{
	CMatRenderContextPtr	pRenderContext( MaterialSystemInterface() );
	CCamera					*pCamera = GetCamera();

	if ( m_nNumInstancesRendered > 0 )
	{
#if STENCIL_AS_CALLS
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
#else
		m_ShaderStencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_ShaderStencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
#endif // STENCIL_AS_CALLS

		Color	InstanceColoring;
		int		width, height;

		pCamera->GetViewPort( width, height );
		PushRenderMode( RENDER_MODE_INSTANCE_OVERLAY );

		BeginClientSpace();
		InstanceColor( InstanceColoring, false );
#if STENCIL_AS_CALLS
		pRenderContext->SetStencilReferenceValue( 0x01 );
#else
		m_ShaderStencilState.m_nReferenceValue = 0x01;
		pRenderContext->SetStencilState( m_ShaderStencilState );
#endif // STENCIL_AS_CALLS
		DrawFilledRect( Vector2D( 0.0f, 0.0f ), Vector2D( width, height ), ( byte * )&InstanceColoring, false );

		InstanceColor( InstanceColoring, true );
#if STENCIL_AS_CALLS
		pRenderContext->SetStencilTestMask( 0xff );
		pRenderContext->SetStencilReferenceValue( 0x02 );
#else
		m_ShaderStencilState.m_nReferenceValue = 0x02;
		pRenderContext->SetStencilState( m_ShaderStencilState );
#endif // STENCIL_AS_CALLS
		DrawFilledRect( Vector2D( 0.0f, 0.0f ), Vector2D( width, height ), ( byte * )&InstanceColoring, false );

		EndClientSpace();
		PopRenderMode();
	}
#if STENCIL_AS_CALLS
	pRenderContext->SetStencilEnable( false );
#else
	m_ShaderStencilState.m_bEnable = false;
	pRenderContext->SetStencilState( m_ShaderStencilState );
#endif // STENCIL_AS_CALLS
}

//-----------------------------------------------------------------------------
// Purpose: this function will push all of the instance data
// Input  : pInstanceClass - the func_instance entity
//			InstanceOrigin - the translation of the instance
//			InstanceAngles - the rotation of the instance
// Output : none
//-----------------------------------------------------------------------------
void CRender::PushInstanceRendering( InstanceRenderingState_t State )
{
	SetInstanceRendering( State );

	m_InstanceRenderingState.AddToHead( State );

	if ( State != INSTANCE_STATE_OFF )
	{
		m_nNumInstancesRendered++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will pop off the top most instance data
//-----------------------------------------------------------------------------
void CRender::PopInstanceRendering( void )
{
	m_InstanceRenderingState.Remove( 0 );
	if ( m_InstanceRenderingState.Count() > 0 )
	{
		SetInstanceRendering( m_InstanceRenderingState.Head() );
	}
	else
	{
		SetInstanceRendering( INSTANCE_STATE_OFF );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the instance rendering state for stencil buffer operations.
//			0 in stencil buffer = normal drawing
//			1 in stencil buffer = instance shaded pass
//			2 in stencil buffer = instance selected shaded pass
// Input  : State - the state at which the next drawing operations should impact
//				the stencil buffer
//-----------------------------------------------------------------------------
void CRender::SetInstanceRendering( InstanceRenderingState_t State )
{
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );

	switch( State )
	{
	case INSTANCE_STATE_OFF:
#ifdef STENCIL_AS_CALLS
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_ZERO );
#else
		m_ShaderStencilState.m_PassOp = SHADER_STENCILOP_ZERO;
#endif // STENCIL_AS_CALLS
		break;

	case INSTANCE_STATE_ON:
#if STENCIL_AS_CALLS
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_REPLACE );
		pRenderContext->SetStencilReferenceValue( 0x01 );
#else
		m_ShaderStencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		m_ShaderStencilState.m_nReferenceValue = 0x01;
#endif // STENCIL_AS_CALLS
		break;

	case INSTANCE_STACE_SELECTED:
#if STENCIL_AS_CALLS
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_REPLACE );
		pRenderContext->SetStencilReferenceValue( 0x02 );
#else
		m_ShaderStencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		m_ShaderStencilState.m_nReferenceValue = 0x02;
#endif // STENCIL_AS_CALLS
		break;
	}

#ifndef STENCIL_AS_CALLS
	pRenderContext->SetStencilState( m_ShaderStencilState );
#endif // STENCIL_AS_CALLS
}


//-----------------------------------------------------------------------------
// Purpose: This function renders a text string at the given position, width,
//          height, and format
//   Input: text - the string to print
//          pos - the screen space position of the text (the is the top-left of
//                a screen space rect)
//          width - the width to render the text into
//          height - the height to render the text into
//          nFlags - define the text format -- see SetTextFormat
//-----------------------------------------------------------------------------
void CRender::DrawText( const char *text, int x, int y, int nFlags )
{
	wchar_t unicode[ 128 ];

	mbstowcs( unicode, text, ARRAYSIZE(unicode) );

	int len = min( 127, Q_strlen( text ) );

	Assert( m_DefaultFont != vgui::INVALID_FONT );
	bool bJustifyText = nFlags & ( TEXT_JUSTIFY_LEFT | TEXT_JUSTIFY_TOP | TEXT_JUSTIFY_HORZ_CENTER | TEXT_JUSTIFY_VERT_CENTER );
	if ( bJustifyText && m_DefaultFont != vgui::INVALID_FONT )
	{
		int wide,tall;
		g_pMatSystemSurface->GetTextSize( m_DefaultFont, unicode, wide, tall );

		if ( nFlags & TEXT_JUSTIFY_LEFT )
			x -= wide;

		if ( nFlags & TEXT_JUSTIFY_TOP )
			y -= tall;

		if ( nFlags & TEXT_JUSTIFY_HORZ_CENTER )
			x -= wide/2;

		if ( nFlags & TEXT_JUSTIFY_VERT_CENTER )
			y -= tall/2;
	}

	PushRenderMode( RENDER_MODE_EXTERN );

	bool bPopMode = BeginClientSpace();

	g_pMatSystemSurface->DrawSetTextPos( x, y );
	g_pMatSystemSurface->DrawPrintText( unicode, len );

	if ( bPopMode )
		EndClientSpace();

	PopRenderMode();
}


//-----------------------------------------------------------------------------
// Uses "world" coordinates
//-----------------------------------------------------------------------------
void CRender::DrawText( const char *text, const Vector2D &vPos, int nOffsetX, int nOffsetY, int nFlags )
{
	Vector vecActualPos( vPos.x, vPos.y, 0.0f );
	if ( IsInLocalTransformMode() )
	{
		VMatrix matrix;
		GetLocalTranform( matrix );
		Vector vWorld = vecActualPos;
		Vector3DMultiplyPosition( matrix, vWorld, vecActualPos );
	}

	Vector2D pt;
	m_pView->WorldToClient( pt, vecActualPos );
	DrawText( text, (int)pt.x + nOffsetX, (int)pt.y + nOffsetY, nFlags );
}

			
//-----------------------------------------------------------------------------
// Purpose: set the text and background (behind text) colors
//   Input: tR, tG, tB - text red, green, and blue values : [0...255]
//          bkR, bkG, bkB - background red, green, blue values : [0...255]
//-----------------------------------------------------------------------------
void CRender::SetTextColor( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	m_TextColor.SetColor( r,g,b,a );
	if ( m_bIsRenderingIntoVGUI )
	{
		g_pMatSystemSurface->DrawSetTextColor( m_TextColor );
	}
}

void CRender::SetHandleColor( unsigned char r, unsigned char g, unsigned char b )
{
	m_HandleColor.SetColor( r, g, b, 255 );
}

void CRender::UpdateStudioRenderConfig( bool bFlat, bool bWireframe )
{
	StudioRenderConfig_t config;
	memset( &config, 0, sizeof( config ) );
	config.fEyeShiftX = 0.0f;
	config.fEyeShiftY = 0.0f;
	config.fEyeShiftZ = 0.0f;
	config.fEyeSize = 0;
	config.drawEntities = 1;
	config.skin = 0;
	config.fullbright = MaterialSystemConfig().nFullbright;
	config.bEyeMove = true;
	config.bSoftwareSkin = bWireframe;
	config.bNoHardware = false;
	config.bNoSoftware = false;
	config.bTeeth = false;
	config.bEyes = true;
	config.bFlex = true;
	config.bSoftwareLighting = true;
	config.bWireframe = bWireframe;
	config.bDrawNormals = false;
	config.bShowEnvCubemapOnly = false;
	g_pStudioRender->UpdateConfig( config );
}

void CRender::StartRenderFrame()
{
	Assert( !m_bIsRendering );

	m_nNumInstancesRendered = 0;

	m_bIsRenderingIntoVGUI = dynamic_cast<CVGuiWnd*>( GetView() );
	if ( m_bIsRenderingIntoVGUI )
	{
		g_pMatSystemSurface->DrawSetTextFont( m_DefaultFont );
		g_pMatSystemSurface->DrawSetTextColor( m_TextColor );
		g_pMatSystemSurface->DrawSetColor( m_DrawColor );
	}

	int width, height;
	VMatrix matrix;	
	CCamera *pCamera = GetCamera();

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );

	// build ortho matrix for client space mode
	pCamera->GetViewPort( width, height );
	pRenderContext->MatrixMode(MATERIAL_PROJECTION);
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho(0, 0, width, height, -99999, 99999 );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &m_OrthoMatrix );
	
	// setup world camera
	pCamera->GetProjMatrix(matrix);
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadMatrix( matrix );

	pCamera->GetViewMatrix(matrix);
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( matrix );

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->LoadIdentity();

	pRenderContext->SetAmbientLight( 1.0, 1.0, 1.0 );

	pCamera->GetViewMatrix( m_CurrentMatrix );

	// Disable all the lights..
	for( int i = 0; i < MaterialSystemHardwareConfig()->MaxNumLights(); ++i)
	{
		LightDesc_t desc;
		desc.m_Type = MATERIAL_LIGHT_DISABLE;
		pRenderContext->SetLight( i, desc );
	}

	m_bIsClientSpace = false;

	// reset render mode
	m_RenderModeStack.Clear();
	SetRenderMode( m_eDefaultRenderMode, true );

	// reset colors
	m_DrawColor.SetColor( 255,255,255,255 );
	m_TextColor.SetColor( 255,255,255,255 );
	m_HandleColor.SetColor( 255,255,255,255 );

    s_fOneUnitLength = 1/pCamera->GetZoom();

	// tell studiorender that we've updated the camera.
	if( g_pStudioRender )
	{
		g_pStudioRender->BeginFrame();

		Vector viewOrigin, viewRight, viewUp, viewForward;
		pCamera->GetViewPoint( viewOrigin );
		pCamera->GetViewRight( viewRight );
		pCamera->GetViewUp( viewUp );
		pCamera->GetViewForward( viewForward );

		g_pStudioRender->SetViewState( viewOrigin, viewRight, viewUp, viewForward );

		static Vector white[6] = 
		{
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
		};

		g_pStudioRender->SetAmbientLightColors( white );
	}

	m_bIsRendering = true;
}

void CRender::EndRenderFrame()
{
	Assert( m_bIsRendering );
	Assert( !m_bIsClientSpace );
	Assert( m_RenderModeStack.Count() == 0 );
	Assert( !m_bIsLocalTransform );

	if ( g_pStudioRender )
	{
		g_pStudioRender->BeginFrame();
	}

	// turn off lighting preview shader state
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->SetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING,0);

	m_nFrameCount++;
	m_bIsRendering = false;
	m_bIsRenderingIntoVGUI = false;
}

void CRender::DrawLine( const Vector &vStart, const Vector &vEnd )
{
	float scale = VectorLength( vEnd-vStart ) / ( s_fOneUnitLength * 16 );

 	meshBuilder.Begin(m_pMesh, MATERIAL_LINES, 1);

    meshBuilder.Position3fv(vStart.Base());
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.TexCoord2f(0, 0, 0);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv(vEnd.Base());
	meshBuilder.Color4ubv((byte*)&m_DrawColor);
	meshBuilder.TexCoord2f(0, scale, scale);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::BeginLocalTransfrom( const VMatrix &matrix, bool MultiplyCurrent )
{
	Assert( !m_bIsClientSpace );

	VMatrix	LocalCopy = matrix;

	if ( MultiplyCurrent )
	{
		LocalCopy = m_LocalMatrix.Head() * LocalCopy;
	}
	m_LocalMatrix.AddToHead( LocalCopy );

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( m_LocalMatrix.Head() );

	CCamera *pCamera = GetCamera();
	pCamera->GetViewMatrix( m_CurrentMatrix );
	m_CurrentMatrix = m_CurrentMatrix * LocalCopy;

	m_bIsLocalTransform = true;
}

void CRender::EndLocalTransfrom()
{
	Assert( m_bIsLocalTransform );
	Assert( !m_bIsClientSpace );

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();

	m_LocalMatrix.Remove( 0 );

	CCamera *pCamera = GetCamera();
	pCamera->GetViewMatrix( m_CurrentMatrix );
	if ( m_LocalMatrix.Count() > 1 )
	{
		m_CurrentMatrix = m_CurrentMatrix * m_LocalMatrix.Head();
		m_bIsLocalTransform = true;
	}
	else
	{
		m_bIsLocalTransform = false;
	}
}

bool CRender::IsInLocalTransformMode()
{
	return m_bIsLocalTransform;
}

void CRender::GetLocalTranform( VMatrix &matrix )
{
	matrix = m_LocalMatrix.Head();
}

bool CRender::BeginClientSpace(void)
{
	if ( m_bIsClientSpace )
		return false;

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( m_OrthoMatrix );

	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	if ( m_bIsLocalTransform )
	{
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
	}

	m_bIsClientSpace = true;

	return true;
}

void CRender::EndClientSpace(void)
{
	Assert( m_bIsClientSpace );

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode(MATERIAL_PROJECTION);
	pRenderContext->PopMatrix();

	if ( m_bIsLocalTransform )
	{
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->PopMatrix();
	}

	m_bIsClientSpace = false;
}

void CRender::TransformPoint( Vector2D &vClient, const Vector& vWorld )
{ 
	Vector vecActualPos;

	if ( !m_bIsLocalTransform )
	{
		vecActualPos = vWorld;
	}
	else
	{
		m_LocalMatrix.Head().V3Mul( vWorld, vecActualPos );
	}

	m_pView->WorldToClient( vClient, vecActualPos );
}


void CRender::TransformNormal( Vector2D &vClient, const Vector& vWorld )
{
	Vector2D originClient, normalClient;
	TransformPoint( originClient, vec3_origin );
	TransformPoint( normalClient, vWorld );
	vClient = normalClient- originClient;
}

void CRender::DrawCircle( const Vector &vCenter, const Vector &vNormal, float flRadius, int nSegments)
{
	Vector vx,vy;

	if ( !BuildAxesFromNormal( vNormal, vx, vy ) )
		return;

	vx *= flRadius;
	vy *= flRadius;

	meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, nSegments );

	float invDelta = 2.0f * M_PI / nSegments;
	for ( int i = 0; i < nSegments; ++i )
	{
		float flRadians = i * invDelta;
		float ca = cos( flRadians );
		float sa = sin( flRadians );

		// Rotate it around the circle
		Vector vertex = vCenter + (ca*vx) + (sa*vy);
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.Position3fv( &vertex.x  );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawCircle( Vector2D &vCenter, float fRadius, int nSegments, unsigned char *pColor )
{
	meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, nSegments );

	float invDelta = 2.0f * M_PI / nSegments;
	for ( int i = 0; i < nSegments; ++i )
	{
		float flRadians = i * invDelta;
		float ca = cos( flRadians );
		float sa = sin( flRadians );

		// Rotate it around the circle
		float x = vCenter.x + (fRadius * ca);
		float y = vCenter.y + (fRadius * sa);
		
		meshBuilder.Color4ubv( pColor );
		meshBuilder.Position3f( x, y, 0 );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawCross( Vector2D& ul, Vector2D& lr, unsigned char *pColor )
{
	// just sizes
	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, 2 );

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, ul.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, lr.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, lr.y-1, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, ul.y-1, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawRect( Vector2D& ul, Vector2D& lr, unsigned char *pColor )
{
	Vector2D vScale = (lr-ul)/16;
				
	meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, 4 );

	Vector2D tex(0,0);

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, ul.y, 0 );
	meshBuilder.TexCoord2f(0, tex.x, tex.y );
	meshBuilder.AdvanceVertex();

	tex.x += vScale.x;
 	tex.y += vScale.x;

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, ul.y, 0 );
	meshBuilder.TexCoord2f(0, tex.x, tex.y );
	meshBuilder.AdvanceVertex();

	tex.x += vScale.y;
	tex.y -= vScale.y;

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, lr.y, 0 );
	meshBuilder.TexCoord2f(0, tex.x, tex.y );
	meshBuilder.AdvanceVertex();

	tex.x -= vScale.x;
	tex.y -= vScale.x;

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, lr.y, 0 );
	meshBuilder.TexCoord2f(0, tex.x, tex.y );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawPlane( const Vector &p0, const Vector &p1, const Vector &p2, const Vector &p3, bool bFill )
{
	if ( bFill )
	{
		meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );
	}
	else
	{
		meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, 4 );
	}

	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.Position3fv( &p0.x );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.Position3fv( &p1.x );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.Position3fv( &p2.x );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.Position3fv( &p3.x );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawFilledRect( Vector2D& ul, Vector2D& lr, unsigned char *pColor, bool bBorder )
{
	static Color black(0,0,0,255);

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, bBorder?2:1 );

	if ( bBorder )
	{
 		meshBuilder.Color4ubv( (byte*)&black );
		meshBuilder.Position3f( ul.x, ul.y, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ubv( (byte*)&black );
		meshBuilder.Position3f( lr.x, ul.y, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ubv( (byte*)&black );
		meshBuilder.Position3f( lr.x, lr.y, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ubv( (byte*)&black );
		meshBuilder.Position3f( ul.x, lr.y, 0 );
		meshBuilder.AdvanceVertex();


		ul.x+=1;
		ul.y+=1;
		lr.x-=1;
		lr.y-=1;
	}

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, ul.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, ul.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( lr.x, lr.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( pColor );
	meshBuilder.Position3f( ul.x, lr.y, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawHandle( const Vector &vCenter, const Vector2D *vOffset )
{
	if ( !m_CurrentInstanceState.m_bIsEditable )
	{
		return;
	}

	int size = m_nHandleSize;

	bool bPopMode = BeginClientSpace();

	Vector2D vCenter2D;

	TransformPoint( vCenter2D, vCenter );

	if ( vOffset )
	{
		vCenter2D += *vOffset;
	}

	RoundVector( vCenter2D );

	if ( m_nHandleType == HANDLE_CROSS )
		size--;

	Vector2D ul( vCenter2D.x-size, vCenter2D.y-size );
	Vector2D lr( vCenter2D.x+size+1, vCenter2D.y+size+1 );

	switch ( m_nHandleType )
	{
		case HANDLE_SQUARE :	DrawFilledRect( ul, lr, (byte*)&m_HandleColor, true );break;
		case HANDLE_CIRCLE	:	DrawCircle( vCenter2D, size, 16, (byte*)&m_HandleColor ); break;
		case HANDLE_DIAMOND	:	break;
		case HANDLE_CROSS	:	DrawCross( ul, lr, (byte*)&m_HandleColor );break;
	}

	if ( bPopMode )
		EndClientSpace();
}

CCamera *CRender::GetCamera()
{
	return m_pView->GetCamera();
}

bool CRender::SetView( CMapView * pView )
{
	Assert( pView );

	m_pView = pView;

	// Store off the three materials we use most often...
	if ( !GetRequiredMaterial( "editor/wireframe", m_pWireframe[0] ) ||
		!GetRequiredMaterial( "editor/flat", m_pFlat[0] ) ||
		!GetRequiredMaterial( "editor/flatdecal", m_pFlat[1] ) ||
		!GetRequiredMaterial( "editor/translucentflat", m_pTranslucentFlat[0] ) ||
		!GetRequiredMaterial( "editor/translucentflatdecal", m_pTranslucentFlat[1] ) ||
		!GetRequiredMaterial( "editor/lightmapgrid", m_pLightmapGrid[0] ) ||
		!GetRequiredMaterial( "editor/lightmapgriddecal", m_pLightmapGrid[1] ) ||
		!GetRequiredMaterial( "editor/selectionoverlay", m_pSelectionOverlay[0] ) ||
		!GetRequiredMaterial( "editor/flatignorez", m_pFlatNoZ[0] ) ||
		!GetRequiredMaterial( "editor/flatnocull", m_pFlatNoCull[0] ) ||
		!GetRequiredMaterial( "editor/dotted", m_pDotted[0] )
		)
	{
		return false;
	}

	// Some materials don't have a separate decal version.
	m_pFlatNoZ[1] = m_pFlatNoZ[0];
	m_pFlatNoCull[1] = m_pFlatNoCull[0];
	m_pWireframe[1] = m_pWireframe[0];
	m_pDotted[1] = m_pDotted[0];
	m_pSelectionOverlay[1] = m_pSelectionOverlay[0];
	
	return true;
	
}

bool CRender::IsActiveView()
{ 
	return m_pView->IsActive();
}

void CRender::SetDrawColor( const Color &color )
{
  	m_DrawColor = color;
	if ( m_bIsRenderingIntoVGUI )
	{
		g_pMatSystemSurface->DrawSetColor( m_DrawColor );
	}
}

void CRender::GetDrawColor( Color &color )
{
	color = m_DrawColor;
}

void CRender::SetDrawColor( unsigned char r, unsigned char g, unsigned char b )
{
	// current draw color, keep the alpha value
	m_DrawColor.SetColor( r, g, b, m_DrawColor.a() );
	if ( m_bIsRenderingIntoVGUI )
	{
		g_pMatSystemSurface->DrawSetColor( m_DrawColor );
	}
}

void CRender::SetHandleStyle( int size, int type  )
{
	m_nHandleType = type;
	m_nHandleSize = size;
}

void CRender::DrawArrow( Vector const &vStart, Vector const &vEnd )
{
	Assert(0);
}

void CRender::DrawPolyLine( int nPoints, const Vector *Points )
{
	// Draw the box bottom, top, and one corner edge.

	meshBuilder.Begin( m_pMesh, MATERIAL_LINE_LOOP, nPoints );

	for (int i= 0; i<nPoints;i++ )
	{
		meshBuilder.Position3fv( &Points[i].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawDisplacement( CCoreDispInfo *pMapDisp )
{
	int numVerts = pMapDisp->GetSize();
	int numIndices = pMapDisp->GetRenderIndexCount();
	bool bWireFrame = m_eCurrentRenderMode == RENDER_MODE_WIREFRAME;
		
	meshBuilder.Begin( m_pMesh, MATERIAL_TRIANGLES, numVerts,	numIndices );

	Color color = m_DrawColor;

	CoreDispVert_t *pVert = pMapDisp->GetDispVertList();
	for (int i = 0; i < numVerts; ++i )
	{
		if ( bWireFrame )
		{
			meshBuilder.Position3fv( pVert[i].m_Vert.Base() );
			meshBuilder.Color4ubv( (byte*)&color );
			meshBuilder.TexCoord2fv( 0, pVert[i].m_TexCoord.Base() );
			meshBuilder.TexCoord2fv( 1, pVert[i].m_LuxelCoords[0].Base() );
			meshBuilder.AdvanceVertex();
		}
		else
		{
			meshBuilder.Position3fv( pVert[i].m_Vert.Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], 255 - ( unsigned char )( pVert[i].m_Alpha ) );
			meshBuilder.Normal3fv( pVert[i].m_Normal.Base() );
			meshBuilder.TangentS3fv( pVert[i].m_TangentS.Base() );
			meshBuilder.TangentT3fv( pVert[i].m_TangentT.Base() );
			meshBuilder.TexCoord2fv( 0, pVert[i].m_TexCoord.Base() );
			meshBuilder.TexCoord2fv( 1, pVert[i].m_LuxelCoords[0].Base() );
			meshBuilder.AdvanceVertex();
		}
	}

	unsigned short *pIndex = pMapDisp->GetRenderIndexList();
	for ( int i = 0; i < numIndices; ++i )
	{
		meshBuilder.Index( pIndex[i] );
		meshBuilder.AdvanceIndex();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}

void CRender::DrawModel( DrawModelInfo_t* pInfo, matrix3x4_t *pBoneToWorld, const Vector &vOrigin, float fAlpha, bool bWireFrame )
{
	UpdateStudioRenderConfig( true, bWireFrame );
		
	g_pStudioRender->SetAlphaModulation( fAlpha );

	Vector viewOrigin;
	GetCamera()->GetViewPoint( viewOrigin );

	g_pStudioRender->SetEyeViewTarget( pInfo->m_pStudioHdr, pInfo->m_Body, viewOrigin );

	g_pStudioRender->DrawModel( NULL, *pInfo, pBoneToWorld, NULL, NULL, vOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL );

	g_pStudioRender->SetAlphaModulation( 1.0f );

	// force rendermode reset
	SetRenderMode( RENDER_MODE_CURRENT, true );
}

void CRender::DrawCollisionModel( MDLHandle_t mdlHandle, const VMatrix &mViewMatrix )
{
	vcollide_t *pCollide =g_pMDLCache->GetVCollide( mdlHandle );

	if ( !pCollide || pCollide->solidCount <= 0 )
		return;

	Vector *outVerts;
	int vertCount = g_pPhysicsCollision->CreateDebugMesh( pCollide->solids[0], &outVerts );

	if ( vertCount )
	{	
		PushRenderMode( RENDER_MODE_WIREFRAME );
	
		meshBuilder.Begin( m_pMesh, MATERIAL_TRIANGLES, vertCount/3 );

		for ( int j = 0; j < vertCount; j++ )
		{
			Vector out;
			mViewMatrix.V3Mul( outVerts[j], out );
			meshBuilder.Position3fv( out.Base() );
			meshBuilder.Color4ubv( (byte*)&m_DrawColor );
			meshBuilder.TexCoord2f( 0, 0, 0 );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		m_pMesh->Draw();

		PopRenderMode();
	}

	g_pPhysicsCollision->DestroyDebugMesh( vertCount, outVerts );
}

void CRender::DrawSphere( const Vector &vCenter, int nRadius )
{
	Assert(0);
}

void CRender::DrawBoxExt( const Vector &vCenter, float extend, bool bFill)
{
	Vector vExtent( extend, extend, extend );
	Vector vMins = vCenter - vExtent;
	Vector vMax = vCenter + vExtent;

	DrawBox( vMins, vMax, bFill );
}

void CRender::DrawHandles( int nPoints, const Vector *Points )
{
	Assert(0);
}

void CRender::DrawBox( const Vector &vMins, const Vector &vMaxs, bool bFill)
{
	Vector points[8];
	PointsFromBox( vMins, vMaxs, points );

	if ( bFill )
	{
		Assert(0);
	}
	else
	{
		// Draw the box bottom, top, and one corner edge.

		meshBuilder.Begin( m_pMesh, MATERIAL_LINE_STRIP, 9 );

		meshBuilder.Position3fv( &points[0].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[1].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[3].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[2].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[6].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[7].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[5].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[4].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[0].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[2].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		m_pMesh->Draw();

		// Draw the three missing edges.
		
		meshBuilder.Begin( m_pMesh, MATERIAL_LINES, 3 );

		meshBuilder.Position3fv( &points[4].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[6].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[1].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[5].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[3].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &points[7].x );
		meshBuilder.Color4ubv( (byte*)&m_DrawColor );
		meshBuilder.AdvanceVertex(); 

		meshBuilder.End();
		m_pMesh->Draw();
	}
}

void CRender::DrawPoint( const Vector &vPoint )
{
	// HACK HACK, MATERIAL_POINTS doesnt work somehow

	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, 1 );

 	meshBuilder.Position3f( vPoint.x, vPoint.y, vPoint.z );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( vPoint.x+1, vPoint.y+1, vPoint.z );
	meshBuilder.Color4ubv( (byte*)&m_DrawColor );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	m_pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Purpose: Binds a texture for rendering. If the texture has never been bound
//			to this rendering context, it is uploaded to the driver.
// Input  : pTexture - Pointer to the texture object being bound.
//-----------------------------------------------------------------------------
void CRender::BindTexture(IEditorTexture *pTexture)
{ 
	// These textures must be CMaterials....
	BindMaterial( pTexture->GetMaterial() );
}


//-----------------------------------------------------------------------------
// Purpose: Binds a material for rendering. If the material has never been bound
//			to this rendering context, it is uploaded to the driver.
// Input  : pMaterial - Pointer to the material object being bound.
//-----------------------------------------------------------------------------
void CRender::BindMaterial( IMaterial *pMaterial )
{
	if ( m_pBoundMaterial != pMaterial )
	{
		m_pBoundMaterial = pMaterial;
		SetRenderMode( RENDER_MODE_CURRENT, true );
	}
}


bool CRender::GetRequiredMaterial( const char *pName, IMaterial* &pMaterial )
{
	pMaterial = NULL;
	IEditorTexture *pTex = g_Textures.FindActiveTexture( pName );
	if ( pTex )
		pMaterial = pTex->GetMaterial();

	if ( pMaterial )
	{
		return true;
	}
	else
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "Missing material '%s'. Go to Tools | Options | Game Configurations and verify that your game directory is correct.", pName );
		MessageBox( NULL, str, "FATAL ERROR", MB_OK );
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eRenderMode - 
//-----------------------------------------------------------------------------
void CRender::SetRenderMode(EditorRenderMode_t eRenderMode, bool bForce)
{
	if (eRenderMode == RENDER_MODE_DEFAULT)
	{
		eRenderMode = m_eDefaultRenderMode;
	}

	if ( eRenderMode == RENDER_MODE_CURRENT )
	{
		eRenderMode = m_eCurrentRenderMode;
	}

	if (m_eCurrentRenderMode == eRenderMode && !bForce)
		return;

	// Bind the appropriate material based on our mode
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	switch(eRenderMode)
	{
		case RENDER_MODE_DOTTED:
			m_pCurrentMaterial = m_pDotted[m_nDecalMode];
			break;

		case RENDER_MODE_FLAT:
			// Ensures we get red even for decals
			m_pCurrentMaterial = m_pFlat[m_nDecalMode];
			break;

		case RENDER_MODE_TRANSLUCENT_FLAT:
			m_pCurrentMaterial = m_pTranslucentFlat[m_nDecalMode];
			break;

		case RENDER_MODE_FLAT_NOZ:
			// flat mode but not depth check
			m_pCurrentMaterial = m_pFlatNoZ[m_nDecalMode];
			break;
		case RENDER_MODE_FLAT_NOCULL:
			// flat mode but not depth check
			m_pCurrentMaterial = m_pFlatNoCull[m_nDecalMode];
			break;

		case RENDER_MODE_WIREFRAME:
			m_pCurrentMaterial = m_pWireframe[m_nDecalMode];
			break;

		case RENDER_MODE_SMOOTHING_GROUP:
			m_pCurrentMaterial = m_pFlat[m_nDecalMode];
			break;

		case RENDER_MODE_LIGHTMAP_GRID:
			m_pCurrentMaterial = m_pLightmapGrid[m_nDecalMode];
			break;

		case RENDER_MODE_SELECTION_OVERLAY:
		case RENDER_MODE_INSTANCE_OVERLAY:
			// Ensures we get red even for decals
			m_pCurrentMaterial = m_pSelectionOverlay[m_nDecalMode];
			break;

		case RENDER_MODE_TEXTURED:
		case RENDER_MODE_TEXTURED_SHADED:
		case RENDER_MODE_LIGHT_PREVIEW2:
		case RENDER_MODE_LIGHT_PREVIEW_RAYTRACED:
			if (m_pBoundMaterial)
			{
				if( m_pBoundMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS ) )
				{
					pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
				}
				else
				{
					pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
				}

				m_pCurrentMaterial = m_pBoundMaterial;
			}
			else
			{
				m_pCurrentMaterial = m_pFlat[m_nDecalMode];
			}
			break;
	}

	Assert( m_pCurrentMaterial != NULL );

	pRenderContext->Bind( m_pCurrentMaterial );

	pRenderContext->SetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING,0);
	if (eRenderMode==RENDER_MODE_TEXTURED_SHADED)
		pRenderContext->SetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING,1);

	if (
		(eRenderMode==RENDER_MODE_LIGHT_PREVIEW2) || 
		(eRenderMode==RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
		)
		pRenderContext->SetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING,2);

	m_pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_pCurrentMaterial );

	Assert( m_pMesh );
	
	m_eCurrentRenderMode = eRenderMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eRenderMode - 
//-----------------------------------------------------------------------------
void CRender::SetDefaultRenderMode(EditorRenderMode_t eRenderMode)
{
	Assert( eRenderMode != RENDER_MODE_DEFAULT );
	m_eDefaultRenderMode = eRenderMode;
}

void CRender::PushRenderMode( EditorRenderMode_t eRenderMode )
{
	m_RenderModeStack.Push( m_eCurrentRenderMode );
	SetRenderMode( eRenderMode );
}

void CRender::PopRenderMode()
{
	SetRenderMode( m_RenderModeStack.Top() );
	m_RenderModeStack.Pop();
}

#define CAMERA_RIGHT		0
#define CAMERA_UP			1
#define CAMERA_FORWARD		2

//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's forward axis.
//-----------------------------------------------------------------------------
void CRender::GetViewForward( Vector &ViewForward ) const
{
	ViewForward[ 0 ] = -m_CurrentMatrix[ CAMERA_FORWARD ][ 0 ];
	ViewForward[ 1 ] = -m_CurrentMatrix[ CAMERA_FORWARD ][ 1 ];
	ViewForward[ 2 ] = -m_CurrentMatrix[ CAMERA_FORWARD ][ 2 ];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's up axis.
//-----------------------------------------------------------------------------
void CRender::GetViewUp(Vector& ViewUp) const
{
	ViewUp[ 0 ] = m_CurrentMatrix[ CAMERA_UP ][ 0 ];
	ViewUp[ 1 ] = m_CurrentMatrix[ CAMERA_UP ][ 1 ];
	ViewUp[ 2 ] = m_CurrentMatrix[ CAMERA_UP ][ 2 ];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's right axis.
//-----------------------------------------------------------------------------
void CRender::GetViewRight(Vector& ViewRight) const
{
	ViewRight[ 0 ] = m_CurrentMatrix[ CAMERA_RIGHT ][ 0 ];
	ViewRight[ 1 ] = m_CurrentMatrix[ CAMERA_RIGHT ][ 1 ];
	ViewRight[ 2 ] = m_CurrentMatrix[ CAMERA_RIGHT ][ 2 ];
}
