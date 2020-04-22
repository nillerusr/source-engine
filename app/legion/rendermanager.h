//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of rendering 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "tier1/mempool.h"
#include "mathlib/mathlib.h"


//-----------------------------------------------------------------------------
// Physics property for entities
//-----------------------------------------------------------------------------
class CCameraProperty
{
	DECLARE_FIXEDSIZE_ALLOCATOR( CCameraProperty );

public:
	CCameraProperty();
	void GetForward( Vector *pForward );

	// Array used for fixed-timestep simulation
	Vector m_Origin;
	QAngle m_Angles;
	Vector m_Velocity;
	QAngle m_AngVelocity;

private:
	friend class CRenderManager;
};


//-----------------------------------------------------------------------------
// Rendering management 
//-----------------------------------------------------------------------------
class CRenderManager : public CGameManager<>
{
public:
	// Inherited from IGameManager
	virtual bool Init();
	virtual LevelRetVal_t LevelInit( bool bFirstCall );
	virtual void Update( );
	virtual LevelRetVal_t LevelShutdown( bool bFirstCall );
	virtual void Shutdown();

	// Property allocation
	CCameraProperty *CreateCameraProperty();
	void DestroyCameraProperty( CCameraProperty *pProperty );

	void RenderWorldInRect( int x, int y, int nWidth, int nHeight );
	void RenderWorldFullscreen();

private:
	// Set up a projection matrix for a 90 degree fov
	void SetupProjectionMatrix( int nWidth, int nHeight, float flFOV );

	// Set up a orthographic projection matrix
	void SetupOrthoMatrix( int nWidth, int nHeight );

	// Sets up the camera
	void SetupCameraRenderState( );

	// Draws the world
	void RenderWorld();

	// Done completely client-side, want total smoothness, so simulate at render interval
	void UpdateLocalPlayerCamera();

	bool m_bRenderWorldFullscreen;
	int m_nRenderX;
	int m_nRenderY;
	int m_nRenderWidth;
	int m_nRenderHeight;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CRenderManager *g_pRenderManager;


#endif // RENDERMANAGER_H

