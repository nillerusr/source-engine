//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DOD_VIEW_SCENE_H
#define DOD_VIEW_SCENE_H
#ifdef _WIN32
#pragma once
#endif

#include "iviewrender.h"
#include "viewrender.h"

#include "colorcorrectionmgr.h"

//-----------------------------------------------------------------------------
// Purpose: Implements the interview to view rendering for the client .dll
//-----------------------------------------------------------------------------
class CDODViewRender : public CViewRender
{
public:
	CDODViewRender();

	void Init( );
	void Shutdown( );

	void RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );
	void RenderPlayerSprites();

	void PerformStunEffect( const CViewSetup &view );

	void InitColorCorrection( );
	void ShutdownColorCorrection( );
	void SetupColorCorrection( );

private:
	ITexture *m_pStunTexture;

	ClientCCHandle_t		m_SpectatorLookupHandle;
	ClientCCHandle_t		m_DeathLookupHandle;
	bool					m_bLookupActive;
};

#endif //DOD_VIEW_SCENE_H