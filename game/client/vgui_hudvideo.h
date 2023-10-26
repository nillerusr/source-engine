//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#ifndef VGUI_HUDVIDEO_H
#define VGUI_HUDVIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_video.h"


class HUDVideoPanel : public VideoPanel
{
	DECLARE_CLASS_SIMPLE( HUDVideoPanel, VideoPanel );
public:

	HUDVideoPanel( vgui::Panel *parent, const char *name );

	virtual void Paint( void );
	virtual void Activate( void );
	virtual void DoModal( void );

	virtual void OnVideoOver();

	void ReturnToLoopVideo( void );
	void PlayTempVideo( const char *pFilename, const char *pTransitionFilename = NULL );
	void SetLoopVideo( const char *pFilename, int nNumLoopAlternatives = 0, float fAlternateChance = 1.0f );

	const char* GetCurrentVideo( void ) const;
	const char* GetLoopVideo( void ) const { return m_szLoopVideo; }
	const char* GetLastTempVideo( void ) const { return m_szLastTempVideo; }

private:
	char m_szLoopVideo[ FILENAME_MAX ];
	char m_szLastTempVideo[ FILENAME_MAX ];
	int m_nNumLoopAlternatives;
	float m_fAlternateChance;
	bool m_bIsLoopVideo;
	bool m_bIsTransition;
};

#endif // VGUI_HUDVIDEO_H
