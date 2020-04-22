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

#ifndef GLAPP_H
#define GLAPP_H
#pragma once


class CSysModule;


// Mouse button identifiers.
#define MSA_BUTTON_LEFT		0
#define MSA_BUTTON_RIGHT	1


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
	void			QuitNextFrame();

	// Post a message to shutdown the app.
	void			AppShutdown();

	int				WinMain(void *hInstance, void *hPrevInstance, char *szCmdLine, int iCmdShow);
	long			WndProc(void *hwnd, long iMsg, long wParam, long lParam);

	int				FindNumParameter(const char *s, int defaultVal=-1);
	bool			FindParameter(const char *s);
	const char*		FindParameterArg(const char *s);

	void			SetTitleText(PRINTF_FORMAT_STRING const char *fmt, ...);

	// Make the matsysapp window the top window.
	void			MakeWindowTopmost();

	void			MouseCapture();
	void			MouseRelease();


private:

	bool			InitMaterialSystem();
	void			Clear();

	bool			CreateMainWindow(int width, int height, int bpp, bool fullscreen);

	void			RenderScene();

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
    bool             m_bChangeBPP;
    bool             m_bAllowSoft;
    char            *m_szCmdLine;
    int              m_argc;
    char           **m_argv;
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
// The app needs to define these symbols.
// ---------------------------------------------------------------------------------------- //
// g_szAppName[]			-- char array applicaton name
// void AppInit( void )		-- Called at init time
// void AppRender( void )	-- Called each frame (as often as possible)
// void AppExit( void )		-- Called to shut down
// void AppKey( int key, int down ); -- called on each key up/down
// void AppChar( int key ); -- key was pressed & released
extern "C" char g_szAppName[];
extern "C" void AppInit( void );
extern "C" void AppRender( float frametime, float mouseDeltaX, float mouseDeltaY );
extern "C" void AppExit( void );
extern "C" void AppKey( int key, int down );
extern "C" void AppChar( int key );
extern bool g_bCaptureOnFocus;	// The app needs to define this to control how matsysapp handles mouse cursor hiding.
								// If it's set to true, the mouse is captured and hidden when the app gets focus.
								// If false, the mouse is only captured and hidden while the left button is down.


// ---------------------------------------------------------------------------------------- //
// Global functions (MSA stands for Material System App)..
// ---------------------------------------------------------------------------------------- //

// Show an error dialog and quit.
bool Sys_Error(PRINTF_FORMAT_STRING const char *pMsg, ...);

// Print to the trace window.
void con_Printf(PRINTF_FORMAT_STRING const char *pMsg, ...);

// Returns true if the key is down.
bool MSA_IsKeyDown(char key);

// Sleep for the specified number of milliseconds... passing 0 does nothing.
void MSA_Sleep(unsigned long countMS);

// Returns true if the specified button is down.
// button should be one of the MSA_BUTTON identifiers.
bool MSA_IsMouseButtonDown( int button );


extern MaterialSystemApp	g_MaterialSystemApp;


extern unsigned int g_Time;


#endif // GLAPP_H
