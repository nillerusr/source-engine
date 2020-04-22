//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "ToolCamera.h"
#include "SaveInfo.h"
#include "MainFrm.h"			// dvs: remove?
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Options.h"
#include "Render2D.h"
#include "StatusBarIDs.h"		// dvs: remove
#include "ToolManager.h"
#include "hammer_mathlib.h"
#include "vgui/Cursor.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma warning(disable:4244)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Camera3D::Camera3D(void)
{
	Cameras.EnsureCapacity(16);
	SetEmpty();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if we are dragging a camera, false if not. // dvs: rename
//-----------------------------------------------------------------------------
bool Camera3D::IsEmpty(void)
{
	return (Cameras.Count() == 0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Camera3D::SetEmpty(void)
{
	Cameras.RemoveAll();
	m_iActiveCamera = -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			BOOL - 
// Output : int
//-----------------------------------------------------------------------------
int Camera3D::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
	for(int i = 0; i < Cameras.Count(); i++)
	{
		for ( int j=0; j<2; j++ )
		{
			if( HitRect( pView, ptClient, Cameras[i].position[j], HANDLE_RADIUS ) )
			{
				return MAKELONG(i+1, j);
			}
		}
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Get rid of extra cameras if we have too many.
//-----------------------------------------------------------------------------
void Camera3D::EnsureMaxCameras()
{
	int nMax = max( Options.general.nMaxCameras, 1 );
	
	int nToRemove = Cameras.Count() - nMax;
	if ( nToRemove > 0 )
	{
		m_iActiveCamera = max( m_iActiveCamera - nToRemove, 0 );
		
		while ( nToRemove-- )
			Cameras.Remove( 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void Camera3D::FinishTranslation(bool bSave)
{
	if (bSave)
	{
		if ( m_iActiveCamera == Cameras.Count() )
		{
			Cameras.AddToTail();
			EnsureMaxCameras();
		}

		Cameras[m_iActiveCamera] = m_MoveCamera;
	}

	Tool3D::FinishTranslation(bSave);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			uFlags - 
//			CSize& - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
bool Camera3D::UpdateTranslation(const Vector &vUpdate, UINT uFlags)
{
	Vector vCamDelta = m_MoveCamera.position[1] - m_MoveCamera.position[0];

	Vector vNewPos = m_vOrgPos + vUpdate;

	// snap point if need be
	if ( uFlags & constrainSnap )
		m_pDocument->Snap( vNewPos, uFlags );

	m_MoveCamera.position[m_nMovePositionIndex] = vNewPos;

	if(uFlags & constrainMoveAll)
	{
		m_MoveCamera.position[(m_nMovePositionIndex+1)%2] = vNewPos + vCamDelta;
	}

 	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
		
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCamPos - 
//			iCamera - 
//-----------------------------------------------------------------------------
void Camera3D::GetCameraPos(Vector &vViewPos, Vector &vLookAt)
{
	if(!inrange(m_iActiveCamera, 0, Cameras.Count()))
	{
		vViewPos.Init();
		vLookAt.Init();
		return;
	}

	vViewPos = Cameras[m_iActiveCamera].position[0];
	vLookAt = Cameras[m_iActiveCamera].position[1];
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCamPos - 
//			iCamera - 
//-----------------------------------------------------------------------------
void Camera3D::AddCamera(CAMSTRUCT &camera)
{
	Cameras.AddToTail( camera );
	EnsureMaxCameras();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void Camera3D::RenderTool2D(CRender2D *pRender)
{
	for (int i = 0; i < Cameras.Count(); i++)
	{
		CAMSTRUCT *pDrawCam = &Cameras[i];

		if (IsTranslating() && (i == m_iActiveCamera))
		{
			pDrawCam = &m_MoveCamera;
		}
 		
		//
		// Draw the line between.
		//
		if (i == m_iActiveCamera)
		{
			pRender->SetDrawColor( 255, 0, 0 );
		}
		else
		{
			pRender->SetDrawColor( 0, 255, 255 );
		}

		pRender->DrawLine( pDrawCam->position[MovePos], pDrawCam->position[MoveLook] );

		//
		// Draw camera handle.
		//
		pRender->SetHandleStyle(HANDLE_RADIUS, CRender::HANDLE_CIRCLE );
		pRender->SetHandleColor( 0, 255, 255 );
		pRender->DrawHandle( pDrawCam->position[MovePos] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles key values being read from the MAP file.
// Input  : szKey - Key being loaded.
//			szValue - Value of the key being loaded.
//			pCam - Camera structure to place the values into.
// Output : Returns ChunkFile_Ok to indicate success.
//-----------------------------------------------------------------------------
ChunkFileResult_t Camera3D::LoadCameraKeyCallback(const char *szKey, const char *szValue, CAMSTRUCT *pCam)
{
	if (!stricmp(szKey, "look"))
	{
		CChunkFile::ReadKeyValueVector3(szValue, pCam->position[MoveLook]);
	}
	else if (!stricmp(szKey, "position"))
	{
		CChunkFile::ReadKeyValueVector3(szValue, pCam->position[MovePos]);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Handles key values being read from the MAP file.
// Input  : szKey - Key being loaded.
//			szValue - Value of the key being loaded.
//			pCam - Camera structure to place the values into.
// Output : Returns ChunkFile_Ok to indicate success.
//-----------------------------------------------------------------------------
ChunkFileResult_t Camera3D::LoadCamerasKeyCallback(const char *szKey, const char *szValue, Camera3D *pCameras)
{
	if (!stricmp(szKey, "activecamera"))
	{
		pCameras->m_iActiveCamera = atoi(szValue);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pLoadInfo - 
//			*pSolid - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t Camera3D::LoadCameraCallback(CChunkFile *pFile, Camera3D *pCameras)
{
	CAMSTRUCT Cam;
	memset(&Cam, 0, sizeof(Cam));

	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadCameraKeyCallback, &Cam);

	if (eResult == ChunkFile_Ok)
	{
		pCameras->AddCamera( Cam );
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t Camera3D::LoadVMF(CChunkFile *pFile)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("camera", (ChunkHandler_t)LoadCameraCallback, this);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadCamerasKeyCallback, this);
	pFile->PopHandlers();

	if (eResult == ChunkFile_Ok)
	{
		//
		// Make sure the active camera is legal.
		//
		if (Cameras.Count() == 0)
		{
			m_iActiveCamera = -1;
		}
		else if (!inrange(m_iActiveCamera, 0, Cameras.Count()))
		{
			m_iActiveCamera = 0;
		}
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &dir - 
//			&pos - 
//-----------------------------------------------------------------------------
void Camera3D::UpdateActiveCamera(Vector &vViewPos, Vector &vDir)
{
	if(!inrange(m_iActiveCamera, 0, Cameras.Count()))
		return;

	Vector& camPos	= Cameras[m_iActiveCamera].position[MovePos];
	Vector& lookPos	= Cameras[m_iActiveCamera].position[MoveLook];

	// get current length
	Vector delta;
	for(int i = 0; i < 3; i++)
		delta[i] = camPos[i] - lookPos[i];

	float length = VectorLength(delta);

	if ( length < 1 )
		length = 1;

	camPos = vViewPos;

	for(int i = 0; i < 3; i++)
		lookPos[i] = camPos[i] + vDir[i] * length;

	if ( IsActiveTool() )
	{
		if (Options.view2d.bCenteroncamera)
		{
			VIEW2DINFO vi;
			vi.wFlags = VI_CENTER;
			vi.ptCenter = vViewPos;
			m_pDocument->SetView2dInfo(vi);
		}

		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void Camera3D::SetNextCamera(SNCTYPE type)
{
	if(Cameras.Count()==0)
	{
		m_iActiveCamera = -1;
		return;
	}
		
	switch(type)
	{
	case sncNext:
		++m_iActiveCamera;
		if(m_iActiveCamera >= Cameras.Count() )
			m_iActiveCamera = 0;
		break;
	case sncPrev:
		--m_iActiveCamera;
		if(m_iActiveCamera < 0)
			m_iActiveCamera = Cameras.Count()-1;
		break;
	case sncFirst:
		m_iActiveCamera = 0;
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Camera3D::DeleteActiveCamera()
{
	if(!inrange(m_iActiveCamera, 0, Cameras.Count()))
		return;

	Cameras.Remove(m_iActiveCamera);
	
	if(m_iActiveCamera >= Cameras.Count() )
		m_iActiveCamera = Cameras.Count()-1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
//-----------------------------------------------------------------------------
void Camera3D::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	float fVersion = 0.2f, fThisVersion;

	int nCameras = Cameras.Count();

	if(fIsStoring)
	{
		file.write((char*)&fVersion, sizeof(fVersion) );

		file.write((char*)&m_iActiveCamera, sizeof(m_iActiveCamera) );
		file.write((char*)&nCameras, sizeof(nCameras));
		for(int i = 0; i < nCameras; i++)
		{
			file.write((char*)&Cameras[i], sizeof(CAMSTRUCT));
		}
	}
	else
	{
		file.read((char*)&fThisVersion, sizeof(fThisVersion) );

		if(fThisVersion >= 0.2f)
		{
			file.read((char*)&m_iActiveCamera, sizeof(m_iActiveCamera));
		}

		file.read((char*)&nCameras, sizeof (nCameras) );

		Cameras.RemoveAll();
		Cameras.EnsureCapacity(nCameras);

		for(int i = 0; i < nCameras; i++)
		{
			CAMSTRUCT cam;
			file.read((char*)&cam, sizeof(CAMSTRUCT));
			Cameras.AddToTail( cam );
		}
		EnsureMaxCameras();

		Assert( Cameras.Count() == nCameras );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t Camera3D::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t eResult = pFile->BeginChunk( GetVMFChunkName() );
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("activecamera", m_iActiveCamera);
	}
	
	if (eResult == ChunkFile_Ok)
	{
		for (int i = 0; i < Cameras.Count(); i++)
		{
			eResult = pFile->BeginChunk("camera");

			if (eResult == ChunkFile_Ok)
			{
				eResult = pFile->WriteKeyValueVector3("position", Cameras[i].position[MovePos]);
			}
			
			if (eResult == ChunkFile_Ok)
			{
				eResult = pFile->WriteKeyValueVector3("look", Cameras[i].position[MoveLook]);
			}

			if (eResult == ChunkFile_Ok)
			{
				eResult = pFile->EndChunk();
			}

			if (eResult != ChunkFile_Ok)
			{
				break;
			}
		}
	}
			
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the key down event in the 2D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_DELETE || nChar == VK_NEXT || nChar == VK_PRIOR)
	{
		CMapDoc *pDoc = pView->GetMapDoc();

		if (nChar == VK_DELETE)
		{
			DeleteActiveCamera();
		}
		else if (nChar == VK_NEXT)
		{
			SetNextCamera(Camera3D::sncNext);
		}
		else
		{
			SetNextCamera(Camera3D::sncPrev);
		}
			
		Vector viewPos,lookAt;

		GetCameraPos( viewPos, lookAt );
		pDoc->UpdateAllCameras( &viewPos, &lookAt, NULL );

		return true;
	}
	else if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;	
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button down event in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	CMapDoc *pDoc = pView->GetMapDoc();

	pView->SetCapture();

	//
	// If there are no cameras created yet or they are holding down
	// the SHIFT key, create a new camera now.
	//

	Vector vecWorld;
	pView->ClientToWorld( vecWorld, vPoint );

	if ( IsEmpty() || (nFlags & MK_SHIFT))
	{
		//
		// Build a point in world space to place the new camera.
		//
		
		if ( !pDoc->GetSelection()->IsEmpty() )
		{
			Vector vecCenter;
			pDoc->GetSelection()->GetBoundsCenter(vecCenter);
			vecWorld[pView->axThird] = vecCenter[pView->axThird];
		}
		else
		{
			vecWorld[pView->axThird] = COORD_NOTINIT;
			pDoc->GetBestVisiblePoint(vecWorld);
		}

		//
		// Create a new camera.
		//
		m_vOrgPos = vecWorld;
		m_MoveCamera.position[MovePos] = vecWorld;
		m_MoveCamera.position[MoveLook] = vecWorld;
		m_nMovePositionIndex = MoveLook;

		// set as active camera
		m_iActiveCamera = Cameras.AddToTail(m_MoveCamera);;
		EnsureMaxCameras();
		
		StartTranslation(pView, vPoint );
	}
	//
	// Otherwise, try to drag an existing camera handle.
	//
	else
	{
		int dwHit = HitTest( pView, vPoint );

		if ( dwHit )
		{
			m_iActiveCamera = LOWORD(dwHit)-1;
			m_MoveCamera = Cameras[m_iActiveCamera];
			m_nMovePositionIndex = HIWORD(dwHit);
			m_vOrgPos = m_MoveCamera.position[m_nMovePositionIndex];
			StartTranslation( pView, vPoint );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button up event in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	ReleaseCapture();

	if (IsTranslating())
	{
		FinishTranslation(true);

		Vector viewPos, lookAt;
		GetCameraPos( viewPos, lookAt );
		
		m_pDocument->UpdateAllCameras( &viewPos, &lookAt, NULL );
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}

unsigned int Camera3D::GetConstraints(unsigned int nKeyFlags)
{
	unsigned int uConstraints = Tool3D::GetConstraints( nKeyFlags );

	if(nKeyFlags & MK_CONTROL)
	{
		uConstraints |= constrainMoveAll;
	}

	return uConstraints;
}

//-----------------------------------------------------------------------------
// Purpose: Handles the mouse move event in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	CMapDoc *pDoc = pView->GetMapDoc();
	if (!pDoc)
	{
		return true;
	}

	vgui::HCursor hCursor = vgui::dc_arrow;

	unsigned int uConstraints = GetConstraints( nFlags );

	// Make sure the point is visible.
	
	pView->ToolScrollToPoint( vPoint );

	//
	// Convert to world coords.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	//
	// Update status bar position display.
	//
	char szBuf[128];

	m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert] );
	SetStatusText(SBI_COORDS, szBuf);
	
	if (IsTranslating())
	{
		Tool3D::UpdateTranslation(pView, vPoint, uConstraints );

		hCursor = vgui::dc_none;
	}
	else if ( !IsEmpty() )
	{
		//
		// If the cursor is on a handle, set it to a cross.
		//
		if ( HitTest( pView, vPoint, true) )
		{
			hCursor = vgui::dc_crosshair;
		}
	}

	if ( hCursor != vgui::dc_none )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button down event in the 3D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->EnableRotating(true);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse up down event in the 3D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->EnableRotating(false);
	pView->UpdateCameraVariables();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the right mouse button down event in the 3D view.
// Input  : Per CWnd::OnRButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->EnableStrafing(true);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the right mouse button up event in the 3D view.
// Input  : Per CWnd::OnRButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->EnableStrafing(false);
	pView->UpdateCameraVariables();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the key down event in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Camera3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_DELETE || nChar == VK_NEXT || nChar == VK_PRIOR)
	{
		CMapDoc *pDoc = pView->GetMapDoc();

		if (nChar == VK_DELETE)
		{
			DeleteActiveCamera();
		}
		else if (nChar == VK_NEXT)
		{
			SetNextCamera(Camera3D::sncNext);
		}
		else
		{
			SetNextCamera(Camera3D::sncPrev);
		}
		
		Vector viewPos, lookAt;
		GetCameraPos( viewPos, lookAt );

		pDoc->UpdateAllCameras( &viewPos, &lookAt, NULL );
		
		return true;
	}
	else if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;	
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Camera3D::OnEscape(void)
{
	//
	// Stop using the camera tool.
	//
	m_pDocument->GetTools()->SetTool(TOOL_POINTER);
}

