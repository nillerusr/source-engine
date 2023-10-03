//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_VIEW_SCENE_H
#define ASW_VIEW_SCENE_H
#ifdef _WIN32
#pragma once
#endif

#include "viewrender.h"

//-----------------------------------------------------------------------------
// Purpose: Implements the interview to view rendering for the client .dll
//-----------------------------------------------------------------------------
class CASWViewRender : public CViewRender
{
public:
	CASWViewRender();

	virtual void OnRenderStart();
	virtual void Render2DEffectsPreHUD( const CViewSetup &view );

	virtual bool	AllowScreenspaceFade( void ) { return false; }

private:
	void DoMotionBlur( const CViewSetup &view );
	void PerformNightVisionEffect( const CViewSetup &view );
};

#endif //ASW_VIEW_SCENE_H