//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef ANIMATED_IMAGE_STRIP_H
#define ANIMATED_IMAGE_STRIP_H

#ifdef _WIN32
#pragma once
#endif

#include "image.h"

namespace panorama
{
	
//-----------------------------------------------------------------------------
// Purpose: Animated Image Strip
//
// Takes an image that has multiple sub-frames and animates displaying them.
// Accepts strips in either horizontal or vertical orientation.
//-----------------------------------------------------------------------------
class CAnimatedImageStrip : public CImagePanel
{
	DECLARE_PANEL2D( CAnimatedImageStrip, CImagePanel );

public:
	CAnimatedImageStrip( CPanel2D *parent, const char * pchPanelID );
	virtual ~CAnimatedImageStrip();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	virtual void Paint() OVERRIDE;

	void StartAnimating();
	
	void StopAnimating();
	void StopAnimatingAtFrame( int nFrame );

	int GetDefaultFrame() const { return m_nDefaultFrame; }
	void SetDefaultFrame( int nFrame ) { m_nDefaultFrame = nFrame; }

	float GetFrameTime() const { return m_flFrameTime; }
	void SetFrameTime( float flFrameTime ) { m_flFrameTime = flFrameTime; }

	void SetCurrentFrame( int nFrame );
	int GetCurrentFrame() const { return m_nCurrentFrameIndex; }

	int GetFrameCount();

private:
	void AdvanceFrame();
	int GetFrameIndex( int nFrame );

	bool EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventAdvanceFrame();

	int m_nDefaultFrame;
	float m_flFrameTime;
	int m_nCurrentFrameIndex;
	int m_nStopAtFrameIndex;
	bool m_bAnimating;
	bool m_bCurrentFramePainted;
};

} // namespace panorama

#endif // PANORAMA_BUTTON_H
