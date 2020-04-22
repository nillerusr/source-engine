//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef FACEPOSER_VGUI_H
#define FACEPOSER_VGUI_H
#ifdef _WIN32
#pragma once
#endif

class IMatSystemSurface;
class CVGuiWnd;

extern IMatSystemSurface *g_pMatSystemSurface;

class CFacePoserVGui
{
public:
	CFacePoserVGui(void);
	~CFacePoserVGui(void);

	bool Init( HWND hWindow );
	void Simulate();
	void Shutdown();
	bool HasFocus( CVGuiWnd *pWnd );
	void SetFocus( CVGuiWnd *pWnd );
	bool IsInitialized() { return m_hMainWindow != NULL; };


protected:

	HWND			m_hMainWindow;
	CVGuiWnd		*m_pActiveWindow;	// the VGUI window that has the focus
};

CFacePoserVGui *FaceposerVGui();

#endif // FACEPOSER_VGUI_H
