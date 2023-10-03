//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASWCIRCULARPROGRESSBAR_H
#define ASWCIRCULARPROGRESSBAR_H

#ifdef _WIN32
#pragma once
#endif

//#include <vgui_controls/Panel.h>
#include <vgui_controls/CircularProgressBar.h>

namespace vgui
{
	class CircularProgressBar;


//-----------------------------------------------------------------------------
// Purpose: Progress Bar in the shape of a pie graph
//-----------------------------------------------------------------------------
class ASWCircularProgressBar : public CircularProgressBar
{
	DECLARE_CLASS_SIMPLE( ASWCircularProgressBar, CircularProgressBar );

public:
	ASWCircularProgressBar(Panel *parent, const char *panelName);
	~ASWCircularProgressBar();

	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void ApplySchemeSettings(IScheme *pScheme);

	void SetFgImage(const char *imageName) { SetImage( imageName, PROGRESS_TEXTURE_FG ); }
	void SetBgImage(const char *imageName) { SetImage( imageName, PROGRESS_TEXTURE_BG ); }

	void SetIsOnCursor( bool bOnCursor ) { m_bIsOnCursor = bOnCursor; }
	void SetScale( float flScale ) { m_flScale = flScale; }
	float GetScale( void ) { return m_flScale; }

	void SetStartProgress( float fStartProgress ) { m_fStartProgress = fStartProgress; }
	float GetStartProgress() { return m_fStartProgress; }

	static void DrawCircleSegmentAtPosition( int x, int y, int w, int h, float flStartDegrees, float flEndDegrees, bool clockwise, float flRotation, float flUVRotation = 0.0f, float subrect_u = 0.0f, float subrect_v = 0.0f, float subrect_s = 1.0f, float subrect_t = 1.0f );

protected:
	virtual void Paint();
	virtual void PaintBackground();
	
	virtual void SetImage(const char *imageName, progress_textures_t iPos);

	float GetMarineFacingYaw( int x, int y );

private:
	bool m_bIsOnCursor;
	int m_iProgressDirection;
	float m_flScale;
	float m_fStartProgress;

	int m_nTextureId[NUM_PROGRESS_TEXTURES];
	char *m_pszImageName[NUM_PROGRESS_TEXTURES];
	int   m_lenImageName[NUM_PROGRESS_TEXTURES];
};

} // namespace vgui

#endif // ASWCIRCULARPROGRESSBAR_H