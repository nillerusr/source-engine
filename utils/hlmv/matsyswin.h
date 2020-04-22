//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef MATSYSWIN_H
#define MATSYSWIN_H

#ifdef _WIN32
#pragma once
#endif


#include <mxtk/mxMatSysWindow.h>
#include "materialsystem/imaterialsystem.h"
#include "interface.h"

class ITexture;
class MatSysWindow : public mxMatSysWindow
{
public:

	// CREATORS
	MatSysWindow( mxWindow *parent, int x, int y, int w, int h, const char *label, int style );
	~MatSysWindow( );

	// MANIPULATORS
	void dumpViewport (const char *filename);
	virtual int handleEvent( mxEvent *event );
	virtual void draw( );

    void			*m_hWnd;
	// void			*m_hDC;

	CSysModule *m_hMaterialSystemInst;
	ITexture *m_pCubemapTexture;

};


extern MatSysWindow *g_MatSysWindow;
extern IMaterial *g_materialBackground;
extern IMaterial *g_materialWireframe;
extern IMaterial *g_materialWireframeVertexColor;
extern IMaterial *g_materialWireframeVertexColorNoCull;
extern IMaterial *g_materialDebugCopyBaseTexture;
extern IMaterial *g_materialFlatshaded;
extern IMaterial *g_materialSmoothshaded;
extern IMaterial *g_materialBones;
extern IMaterial *g_materialLines;
extern IMaterial *g_materialFloor;
extern IMaterial *g_materialVertexColor;
extern IMaterial *g_materialShadow;

#if 0

typedef struct
{
    int   width;
    int   height;
    int   bpp;
    int   flags;
    int   frequency;
} screen_res_t;



typedef struct
{
    int  width;
    int  height;
    int  bpp;
} devinfo_t;



class MaterialSystemApp
{
public:

					MaterialSystemApp();
					~MaterialSystemApp();

	void			Term();

	// Post a message to shutdown the app.
	void			AppShutdown();

	int				WinMain(void *hInstance, void *hPrevInstance, char *szCmdLine, int iCmdShow);
	long			WndProc(void *hwnd, long iMsg, long wParam, long lParam);

	int				FindNumParameter(const char *s, int defaultVal=-1);
	bool			FindParameter(const char *s);
	const char*		FindParameterArg(const char *s);

	void			SetTitleText(PRINTF_FORMAT_STRING const char *fmt, ...);


private:

	bool			InitMaterialSystem();
	void			Clear();

	bool			CreateMainWindow(int width, int height, int bpp, bool fullscreen);

	void			RenderScene();

	void			MouseCapture();
	void			MouseRelease();

	void			GetParameters();


public:
    IMaterialSystem	*m_pMaterialSystem;
	void			*m_hMaterialSystemInst;

	devinfo_t		m_DevInfo;

	void			*m_hInstance;
    int				m_iCmdShow;
    void			*m_hWnd;
	void			*m_hDC;
    bool			m_bActive;
    bool             m_bFullScreen;
    int              m_width;
    int              m_height;
	int				 m_centerx;		// for mouse offset calculations
	int				 m_centery;
    int              m_bpp;
    BOOL             m_bChangeBPP;
    BOOL             m_bAllowSoft;
	BOOL			 m_bPaused;
    int              m_glnWidth;
    int              m_glnHeight;
    float            m_gldAspect;
    float            m_NearClip;
    float            m_FarClip;
    float            m_fov;
    
    screen_res_t    *m_pResolutions;
	int              m_iResCount;

    int              m_iVidMode;
};


// ---------------------------------------------------------------------------------------- //
// Global functions
// ---------------------------------------------------------------------------------------- //

// Show an error dialog and quit.
bool Sys_Error(PRINTF_FORMAT_STRING const char *pMsg, ...);

// Print to the trace window.
void con_Printf(PRINTF_FORMAT_STRING const char *pMsg, ...);

// Returns true if the key is down.
bool IsKeyDown(char key);



extern MaterialSystemApp	g_MaterialSystemApp;


extern unsigned int g_Time;


#endif

#endif // GLAPP_H
