//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CAMERA3D_H
#define CAMERA3D_H
#pragma once


#include "Tool3D.h"
#include "ToolInterface.h"
#include "utlvector.h"
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)



class CChunkFile;
class CSaveInfo;


enum ChunkFileResult_t;


//
// Defines a camera position/look pair.
//
struct CAMSTRUCT
{
	// index 0 = camera origin, 1 = pos look to
	Vector position[2];
};


class Camera3D : public Tool3D
{
public:

	Camera3D(void);

	enum SNCTYPE
	{
		sncNext = -1,
		sncFirst = 0,
		sncPrev = 1
	};

	int GetActiveCamera(void) { return m_iActiveCamera; }
	void GetCameraPos(Vector &vViewPos, Vector &vLookAt);
	void UpdateActiveCamera(Vector &vViewPos, Vector &vLookAt);

	//
	// Serialization.
	//
	const char *GetVMFChunkName() { return "cameras"; }
	ChunkFileResult_t LoadVMF(CChunkFile *pFile);
	ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
	void SerializeRMF(std::fstream &file, BOOL fIsStoring);

	//
	// Tool3D implementation.
	//
	virtual bool IsEmpty(void);
	virtual void SetEmpty(void);
	virtual unsigned int GetConstraints(unsigned int nKeyFlags);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_CAMERA; }

	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);

	virtual void RenderTool2D(CRender2D *pRender);

protected:

	//
	// Tool3D implementation.
	//
	virtual int  HitTest(CMapView *pView, const Vector2D &vPoint, bool bTestHandles = false);
	virtual bool UpdateTranslation(const Vector &vUpdate, UINT flags = 0);
	virtual void FinishTranslation(bool bSave);
	
private:

	int GetCameraCount() { return Cameras.Count(); }
	void AddCamera(CAMSTRUCT &pCamPos);
	
	void SetNextCamera(SNCTYPE next);
	void DeleteActiveCamera(void);

	void OnEscape(void);
	void EnsureMaxCameras();

	static ChunkFileResult_t LoadCameraKeyCallback(const char *szKey, const char *szValue, CAMSTRUCT *pCam);
	static ChunkFileResult_t LoadCamerasKeyCallback(const char *szKey, const char *szValue, Camera3D *pCameras);
	static ChunkFileResult_t LoadCameraCallback(CChunkFile *pFile, Camera3D *pCameras);

	

	CUtlVector<CAMSTRUCT> Cameras;		// The cameras that have been created.
	CAMSTRUCT m_MoveCamera;

	enum
	{
		MovePos = 0,
		MoveLook = 1,
	};

	int m_iActiveCamera;
	int m_nMovePositionIndex;
	Vector m_vOrgPos;
};


#endif // CAMERA3D_H

