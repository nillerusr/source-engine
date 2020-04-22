//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Debugging overlay functions
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef DEBUGOVERLAY_H
#define DEBUGOVERLAY_H

#ifdef _WIN32
#pragma once
#endif

namespace CDebugOverlay
{
#ifndef DEDICATED
	void AddEntityTextOverlay(int ent_index, int line_offset, float flDuration, int r, int g, int b, int a, const char *text);
	void AddBoxOverlay(const Vector& origin, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void AddSphereOverlay(const Vector& vOrigin, float flRadius, int nTheta, int nPhi, int r, int g, int b, int a, float flDuration);
	void AddSweptBoxOverlay(const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void AddGridOverlay(const Vector& vPos );
	void AddCoordFrameOverlay(const matrix3x4_t& frame, float flScale, int ppColorTable[3][3] = NULL);
	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void AddTriangleOverlay(const Vector& p1, const Vector& p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void AddTextOverlay(const Vector& origin, float flDuration, const char *text);
	void AddTextOverlay(const Vector& origin, int line_offset, float flDuration, const char *text);
	void AddTextOverlay(const Vector& origin, int line_offset, float flDuration, float alpha, const char *text);
	void AddTextOverlay(const Vector& origin, int line_offset, float flDuration, float r, float g, float b, float alpha, const char *text);
	void AddScreenTextOverlay(float flXPos, float flYPos, float flDuration, int r, int g, int b, int a, const char *text);
	void AddScreenTextOverlay(float flXPos, float flYPos, int line_offset, float flDuration, int r, int g, int b, int a, const char *text);
	void AddTextOverlay(const Vector& textPos, float duration, float alpha, const char *text) ;
	void Draw3DOverlays(void);
#endif
}

#endif // DEBUGOVERLAY_H
