//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATSYSTEMSURFACE_H
#define MATSYSTEMSURFACE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>
#include <vgui/IClientPanel.h>
#include <vgui_controls/Panel.h>
#include <vgui/IInput.h>
#include <vgui_controls/Controls.h>
#include <vgui/Point.h>
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier1/utldict.h"
#include "tier3/tier3.h"

using namespace vgui;

class IImage;

extern class IMaterialSystem *g_pMaterialSystem;
//-----------------------------------------------------------------------------
// The default material system embedded panel
//-----------------------------------------------------------------------------
class CMatEmbeddedPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	CMatEmbeddedPanel();
	virtual void OnThink();

	VPANEL IsWithinTraverse(int x, int y, bool traversePopups);
};

//-----------------------------------------------------------------------------
//
// Implementation of the VGUI surface on top of the material system
//
//-----------------------------------------------------------------------------
class CMatSystemSurface : public CTier3AppSystem< IMatSystemSurface >
{
	typedef CTier3AppSystem< IMatSystemSurface > BaseClass;

public:
	CMatSystemSurface();
	virtual ~CMatSystemSurface();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// initialization
	virtual void SetEmbeddedPanel(vgui::VPANEL pEmbeddedPanel);

	// returns true if a panel is minimzed
	bool IsMinimized(vgui::VPANEL panel);

	// Sets the only panel to draw.  Set to NULL to clear.
	void RestrictPaintToSinglePanel(vgui::VPANEL panel);

	// frame
	virtual void RunFrame();

	// implementation of vgui::ISurface
	virtual vgui::VPANEL GetEmbeddedPanel();
	
	// drawing context
	virtual void PushMakeCurrent(vgui::VPANEL panel,bool useInSets);
	virtual void PopMakeCurrent(vgui::VPANEL panel);

	// rendering functions
	virtual void DrawSetColor(int r,int g,int b,int a);
	virtual void DrawSetColor(Color col);
	
	virtual void DrawLine( int x0, int y0, int x1, int y1 );
	virtual void DrawTexturedLine( const vgui::Vertex_t &a, const vgui::Vertex_t &b );
	virtual void DrawPolyLine(int *px, int *py, int numPoints);
	virtual void DrawTexturedPolyLine( const vgui::Vertex_t *p, int n );

	virtual void DrawFilledRect(int x0, int y0, int x1, int y1);
	virtual void DrawFilledRectArray( IntRect *pRects, int numRects );
	virtual void DrawFilledRectFastFade( int x0, int y0, int x1, int y1, int fadeStartPt, int fadeEndPt, unsigned int alpha0, unsigned int alpha1, bool bHorizontal );
	virtual void DrawFilledRectFade( int x0, int y0, int x1, int y1, unsigned int alpha0, unsigned int alpha1, bool bHorizontal );
	virtual void DrawOutlinedRect(int x0, int y0, int x1, int y1);
	virtual void DrawOutlinedCircle(int x, int y, int radius, int segments);

	// textured rendering functions
	virtual int  CreateNewTextureID( bool procedural = false );
	virtual bool IsTextureIDValid(int id);

	virtual bool DrawGetTextureFile(int id, char *filename, int maxlen );
	virtual int	 DrawGetTextureId( char const *filename );
	virtual int  DrawGetTextureId( ITexture *pTexture );
	virtual void DrawSetTextureFile(int id, const char *filename, int hardwareFilter, bool forceReload);
	virtual void DrawSetTexture(int id);
	virtual void DrawGetTextureSize(int id, int &wide, int &tall);
	virtual bool DeleteTextureByID(int id);

	virtual IVguiMatInfo *DrawGetTextureMatInfoFactory(int id);

	virtual void DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload);

	virtual void DrawTexturedRect(int x0, int y0, int x1, int y1);
	virtual void DrawTexturedSubRect( int x0, int y0, int x1, int y1, float texs0, float text0, float texs1, float text1 );

	virtual void DrawTexturedPolygon(int n, vgui::Vertex_t *pVertices, bool bClipVertices = true );

	virtual void DrawPrintText(const wchar_t *text, int textLen, FontDrawType_t drawType = FONT_DRAW_DEFAULT);
	virtual void DrawUnicodeChar(wchar_t wch, FontDrawType_t drawType = FONT_DRAW_DEFAULT );
	virtual void DrawUnicodeString( const wchar_t *pwString, FontDrawType_t drawType = FONT_DRAW_DEFAULT );
	virtual void DrawSetTextFont(vgui::HFont font);
	virtual void DrawFlushText();

	virtual void DrawSetTextColor(int r, int g, int b, int a);
	virtual void DrawSetTextColor(Color col);
	virtual void DrawSetTextScale(float sx, float sy);
	virtual void DrawSetTextPos(int x, int y);
	virtual void DrawGetTextPos(int& x,int& y);

	virtual vgui::IHTML *CreateHTMLWindow(vgui::IHTMLEvents *events,vgui::VPANEL context);
	virtual void PaintHTMLWindow(vgui::IHTML *htmlwin);
	virtual void DeleteHTMLWindow(vgui::IHTML *htmlwin);
	virtual bool BHTMLWindowNeedsPaint(IHTML *htmlwin);

	virtual void GetScreenSize(int &wide, int &tall);
	virtual void SetAsTopMost(vgui::VPANEL panel, bool state);
	virtual void BringToFront(vgui::VPANEL panel);
	virtual void SetForegroundWindow (vgui::VPANEL panel);
	virtual void SetPanelVisible(vgui::VPANEL panel, bool state);
	virtual void SetMinimized(vgui::VPANEL panel, bool state);
	virtual void FlashWindow(vgui::VPANEL panel, bool state);
	virtual void SetTitle(vgui::VPANEL panel, const wchar_t *title);
	virtual const wchar_t *GetTitle( vgui::VPANEL panel );

	virtual void SetAsToolBar(vgui::VPANEL panel, bool state);		// removes the window's task bar entry (for context menu's, etc.)

	// windows stuff
	virtual void CreatePopup(VPANEL panel, bool minimized, bool showTaskbarIcon = true, bool disabled = false, bool mouseInput = true , bool kbInput = true);

	virtual void SwapBuffers(vgui::VPANEL panel);
	virtual void Invalidate(vgui::VPANEL panel);

	virtual void SetCursor(vgui::HCursor cursor);
	virtual bool IsCursorVisible();
	virtual void SetCursorAlwaysVisible(bool visible);

	virtual void ApplyChanges();
	virtual bool IsWithin(int x, int y);
	virtual bool HasFocus();

	virtual bool SupportsFeature(SurfaceFeature_e feature);

	// engine-only focus handling (replacing WM_FOCUS windows handling)
	virtual void SetTopLevelFocus(vgui::VPANEL panel);

	// fonts
	virtual vgui::HFont CreateFont();
	virtual bool SetFontGlyphSet(vgui::HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0);
	virtual bool SetBitmapFontGlyphSet(vgui::HFont font, const char *windowsFontName, float scalex, float scaley, int flags);
	virtual int GetFontTall(HFont font);
	virtual int GetFontTallRequested(HFont font);
	virtual int GetFontAscent(HFont font, wchar_t wch);
	virtual bool IsFontAdditive(HFont font);
	virtual void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c);
	virtual void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall);
	virtual int GetCharacterWidth(vgui::HFont font, int ch);
	virtual bool AddCustomFontFile(const char *fontName, const char *fontFileName);
	virtual bool AddBitmapFontFile(const char *fontFileName);
	virtual void SetBitmapFontName( const char *pName, const char *pFontFilename );
	virtual const char *GetBitmapFontName( const char *pName );
	virtual void PrecacheFontCharacters(HFont font, const wchar_t *pCharacters);
	virtual void ClearTemporaryFontCache( void );
	virtual const char *GetFontName( HFont font );
	virtual const char *GetFontFamilyName( HFont font );

	// GameUI-only accessed functions
	// uploads a part of a texture, used for font rendering
	void DrawSetSubTextureRGBA(int textureID, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall);
	void DrawUpdateRegionTextureRGBA( int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall, ImageFormat imageFormat );

	// notify icons?!?
	virtual vgui::VPANEL GetNotifyPanel();
	virtual void SetNotifyIcon(vgui::VPANEL context, vgui::HTexture icon, vgui::VPANEL panelToReceiveMessages, const char *text);

	// plays a sound
	virtual void PlaySound(const char *fileName);

	//!! these functions Should not be accessed directly, but only through other vgui items
	//!! need to move these to seperate interface
	virtual int GetPopupCount();
	virtual vgui::VPANEL GetPopup( int index );
	virtual bool ShouldPaintChildPanel(vgui::VPANEL childPanel);
	virtual bool RecreateContext(vgui::VPANEL panel);
	virtual void AddPanel(vgui::VPANEL panel);
	virtual void ReleasePanel(vgui::VPANEL panel);
	virtual void MovePopupToFront(vgui::VPANEL panel);

	virtual void SolveTraverse(vgui::VPANEL panel, bool forceApplySchemeSettings);
	virtual void PaintTraverse(vgui::VPANEL panel);

	virtual void EnableMouseCapture(vgui::VPANEL panel, bool state);

	virtual void SetWorkspaceInsets( int left, int top, int right, int bottom );
	virtual void GetWorkspaceBounds(int &x, int &y, int &wide, int &tall);

	// Hook needed to Get input to work
	virtual void AttachToWindow( void *hwnd, bool bLetAppDriveInput );
	virtual bool HandleInputEvent( const InputEvent_t &event );

	void		 InitFullScreenBuffer( const char *pszRenderTargetName );
	virtual void Set3DPaintTempRenderTarget( const char *pRenderTargetName );
	virtual void Reset3DPaintTempRenderTarget( void );

	// Begins, ends 3D painting
	virtual void Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom, bool bRenderToTexture = true );
	virtual void End3DPaint();

	virtual void BeginSkinCompositionPainting() OVERRIDE;
	virtual void EndSkinCompositionPainting() OVERRIDE;

	// Disable clipping during rendering
	virtual void DisableClipping( bool bDisable ) OVERRIDE;
	virtual void GetClippingRect( int &left, int &top, int &right, int &bottom, bool &bClippingDisabled ) OVERRIDE;
	virtual void SetClippingRect( int left, int top, int right, int bottom ) OVERRIDE;

	// Prevents vgui from changing the cursor
	virtual bool IsCursorLocked() const;

	// Sets the mouse Get + Set callbacks
	virtual void SetMouseCallbacks( GetMouseCallback_t GetFunc, SetMouseCallback_t SetFunc );

	// Tells the surface to ignore windows messages
	virtual void EnableWindowsMessages( bool bEnable );

	// Installs a function to play sounds
	virtual void InstallPlaySoundFunc( PlaySoundFunc_t soundFunc );

	// Some drawing methods that cannot be accomplished under Win32
	virtual void DrawColoredCircle( int centerx, int centery, float radius, int r, int g, int b, int a );
	virtual int	DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, PRINTF_FORMAT_STRING const char *fmt, ... );
	virtual void DrawColoredTextRect( vgui::HFont font, int x, int y, int w, int h, int r, int g, int b, int a, PRINTF_FORMAT_STRING const char *fmt, ... );
	virtual void DrawTextHeight( vgui::HFont font, int w, int& h, PRINTF_FORMAT_STRING const char *fmt, ... );

	// Returns the length in pixels of the text
	virtual int	DrawTextLen( vgui::HFont font, PRINTF_FORMAT_STRING const char *fmt, ... );

	// Draws a panel in 3D space. 
	virtual void DrawPanelIn3DSpace( vgui::VPANEL pRootPanel, const VMatrix &panelCenterToWorld, int pw, int ph, float sw, float sh ); 

	// Only visible within vguimatsurface
	void DrawSetTextureMaterial(int id, IMaterial *pMaterial);
	void ReferenceProceduralMaterial( int id, int referenceId, IMaterial *pMaterial );

	// new stuff for Alfreds VGUI2 port!!
	virtual bool InEngine() { return true; }
	void GetProportionalBase( int &width, int &height ) { width = BASE_WIDTH; height = BASE_HEIGHT; }
	virtual bool HasCursorPosFunctions() { return true; }

	virtual void SetModalPanel(VPANEL );
	virtual VPANEL GetModalPanel();
	virtual void UnlockCursor();
	virtual void LockCursor();
	virtual void SetTranslateExtendedKeys(bool state);
	virtual VPANEL GetTopmostPopup();
	virtual void GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall);
	virtual void CalculateMouseVisible();
	virtual bool NeedKBInput();
	virtual void SurfaceGetCursorPos(int &x, int &y);
	virtual void SurfaceSetCursorPos(int x, int y);
	virtual void MovePopupToBack(VPANEL panel);

	virtual bool IsInThink( VPANEL panel); 

	virtual bool DrawGetUnicodeCharRenderInfo( wchar_t ch, CharRenderInfo& info );
	virtual void DrawRenderCharFromInfo( const CharRenderInfo& info );

	// global alpha setting functions
	// affect all subsequent draw calls - shouldn't normally be used directly, only in Panel::PaintTraverse()
	virtual void DrawSetAlphaMultiplier( float alpha /* [0..1] */ );
	virtual float DrawGetAlphaMultiplier();

	// web browser
	virtual void SetAllowHTMLJavaScript( bool state );

	// video mode changing
	virtual void OnScreenSizeChanged( int nOldWidth, int nOldHeight );

	virtual vgui::HCursor CreateCursorFromFile( char const *curOrAniFile, char const *pPathID );

	virtual void PaintTraverseEx(VPANEL panel, bool paintPopups = false );

	virtual float GetZPos() const;

	virtual void SetPanelForInput( VPANEL vpanel );

	virtual vgui::IImage *GetIconImageForFullPath( char const *pFullPath );

	virtual void DestroyTextureID( int id );


	virtual const char *GetResolutionKey( void ) const;

	virtual bool ForceScreenSizeOverride( bool bState, int wide, int tall );
	// LocalToScreen, ParentLocalToScreen fixups for explicit PaintTraverse calls on Panels not at 0, 0 position
	virtual bool ForceScreenPosOffset( bool bState, int x, int y );

	virtual void OffsetAbsPos( int &x, int &y );

	virtual void GetKernedCharWidth( HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &flabcA );
	

	virtual const char *GetWebkitHTMLUserAgentString() { return "Valve Client"; }

	virtual void *Deprecated_AccessChromeHTMLController() { return NULL; }

	virtual void SetFullscreenViewport( int x, int y, int w, int h ) OVERRIDE;
	virtual void SetFullscreenViewportAndRenderTarget( int x, int y, int w, int h, ITexture *pRenderTarget ) OVERRIDE;
	virtual void GetFullscreenViewportAndRenderTarget( int & x, int & y, int & w, int & h, ITexture **ppRenderTarget ) OVERRIDE;
	virtual void GetFullscreenViewport( int & x, int & y, int & w, int & h ) OVERRIDE;
	virtual void PushFullscreenViewport() OVERRIDE;
	virtual void PopFullscreenViewport() OVERRIDE;

	// support for software cursors
	virtual void SetSoftwareCursor( bool bUseSoftwareCursor ) OVERRIDE;
	virtual void PaintSoftwareCursor()  OVERRIDE;

	// Methods of ILocalizeTextQuery
public:
	//virtual int ComputeTextWidth( const wchar_t *pString );

	// Causes fonts to get reloaded, etc.
	virtual void ResetFontCaches();

	virtual bool IsScreenSizeOverrideActive( void );
	virtual bool IsScreenPosOverrideActive( void );

	virtual IMaterial *DrawGetTextureMaterial( int id );

	virtual int GetTextureNumFrames( int id );
	virtual void DrawSetTextureFrame( int id, int nFrame, unsigned int *pFrameCache );

private:
	//void DrawRenderCharInternal( const FontCharRenderInfo& info );
	void DrawRenderCharInternal( const CharRenderInfo& info );

private:
	enum { BASE_HEIGHT = 480, BASE_WIDTH = 640 };

	struct PaintState_t
	{
		vgui::VPANEL m_pPanel;
		int	m_iTranslateX;
		int m_iTranslateY;
		int	m_iScissorLeft;
		int	m_iScissorRight;
		int	m_iScissorTop;
		int	m_iScissorBottom;
	};

	// material Setting method 
	void InternalSetMaterial( IMaterial *material = NULL );

	// Draws the fullscreen buffer into the panel
	void DrawFullScreenBuffer( int nLeft, int nTop, int nRight, int nBottom );

	// Helper method to initialize vertices (transforms them into screen space too)
	void InitVertex( vgui::Vertex_t &vertex, int x, int y, float u, float v );

	// Draws a quad + quad array 
	void DrawQuad( const vgui::Vertex_t &ul, const vgui::Vertex_t &lr, unsigned char *pColor );
	void DrawQuadArray( int numQuads, vgui::Vertex_t *pVerts, unsigned char *pColor, bool bShouldClip = true );

	// Necessary to wrap the rendering
	void StartDrawing( void );
	void StartDrawingIn3DSpace( const VMatrix &screenToWorld, int pw, int ph, float sw, float sh );
	void FinishDrawing( void );

	// Sets up a particular painting state...
	void SetupPaintState( const PaintState_t &paintState );

	void ResetPopupList();
	void AddPopup( vgui::VPANEL panel );
	void RemovePopup( vgui::VPANEL panel );
	void AddPopupsToList( vgui::VPANEL panel );

	// Helper for drawing colored text
	int DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const char *fmt, va_list argptr );
	void SearchForWordBreak( vgui::HFont font, char *text, int& chars, int& pixels );

	void InternalThinkTraverse(VPANEL panel);
	void InternalSolveTraverse(VPANEL panel);
	void InternalSchemeSettingsTraverse(VPANEL panel, bool forceApplySchemeSettings);

	// handles mouse movement
	void SetCursorPos(int x, int y);
	void GetCursorPos(int &x, int &y);

	void DrawTexturedLineInternal( const Vertex_t &a, const Vertex_t &b );

	// Gets texture coordinates for drawing the full screen buffer
	void GetFullScreenTexCoords( int x, int y, int w, int h, float *pMinU, float *pMinV, float *pMaxU, float *pMaxV );

	// Is a panel under the restricted panel?
	bool IsPanelUnderRestrictedPanel( VPANEL panel );

	// Point Translation for current panel
	int				m_nTranslateX;
	int				m_nTranslateY;

	// alpha multiplier for current panel [0..1]
	float			m_flAlphaMultiplier;

	// The size of the window to draw into
	int				m_pSurfaceExtents[4];

	// Color for drawing all non-text things
	unsigned char	m_DrawColor[4];

	// Color for drawing text
	unsigned char	m_DrawTextColor[4];

	// Location of text rendering
	int				m_pDrawTextPos[2];

	// Meshbuilder used for drawing
	IMesh* m_pMesh;
	CMeshBuilder meshBuilder;

	// White material used for drawing non-textured things
	CMaterialReference m_pWhite;

	// Used for 3D-rendered images
	CTextureReference m_FullScreenBuffer;
	CMaterialReference m_FullScreenBufferMaterial;
	int m_nFullScreenBufferMaterialId;
	CUtlString		m_FullScreenBufferName;

	bool			m_bUsingTempFullScreenBufferMaterial;

	// Root panel
	vgui::VPANEL m_pEmbeddedPanel;
	vgui::Panel *m_pDefaultEmbeddedPanel;
	vgui::VPANEL m_pRestrictedPanel;

	// List of pop-up panels based on the type enum above (draw order vs last clicked)
	CUtlVector<vgui::HPanel>	m_PopupList;

	// Stack of paint state...
	CUtlVector<	PaintState_t > m_PaintStateStack;

	vgui::HFont				m_hCurrentFont;
	vgui::HCursor			_currentCursor;
	bool					m_cursorAlwaysVisible;

	// The currently bound texture
	int m_iBoundTexture;

	// font drawing batching code
	enum { MAX_BATCHED_CHAR_VERTS = 4096 };
	vgui::Vertex_t m_BatchedCharVerts[ MAX_BATCHED_CHAR_VERTS ];
	int m_nBatchedCharVertCount;

	// What's the rectangle we're drawing in 3D paint mode?
	int m_n3DLeft, m_n3DRight, m_n3DTop, m_n3DBottom;

	// Are we painting in 3D? (namely drawing 3D objects *inside* the vgui panel)
	bool m_bIn3DPaintMode : 1;

	// If we are in 3d Paint mode, are we rendering to a texture?  (Or directly to the screen)
	bool m_b3DPaintRenderToTexture : 1;

	// Are we drawing the vgui panel in the 3D world somewhere?
	bool m_bDrawingIn3DWorld : 1;

	// Is the app gonna call HandleInputEvent?
	bool m_bAppDrivesInput : 1;

	// Are we currently in the think() loop
	bool m_bInThink : 1;

	bool m_bNeedsKeyboard : 1;
	bool m_bNeedsMouse : 1;
	bool m_bAllowJavaScript : 1;

	int m_nLastInputPollCount;

	VPANEL m_CurrentThinkPanel;

	// The attached HWND
	void *m_HWnd;

	// Installed function to play sounds
	PlaySoundFunc_t m_PlaySoundFunc;

	int		m_WorkSpaceInsets[4];

	class TitleEntry
	{
	public:
		TitleEntry()
		{
			panel = NULL;
			title[0] = 0;
		}

		vgui::VPANEL panel;
		wchar_t	title[128];
	};

	CUtlVector< TitleEntry >	m_Titles;
	CUtlVector< CUtlSymbol >	m_CustomFontFileNames;
	CUtlVector< CUtlSymbol >	m_BitmapFontFileNames;
	CUtlDict< int, int >		m_BitmapFontFileMapping;

	float	m_flZPos;
	CUtlDict< vgui::IImage *, unsigned short >	m_FileTypeImages;

	int		GetTitleEntry( vgui::VPANEL panel );

	virtual void DrawSetTextureRGBAEx(int id, const unsigned char *rgba, int wide, int tall, ImageFormat format );

	struct ScreenOverride_t
	{
		ScreenOverride_t() : m_bActive( false ) 
		{
			m_nValue[ 0 ] = m_nValue[ 1 ] = 0;
		}
		bool	m_bActive;
		int		m_nValue[ 2 ];
	};

	ScreenOverride_t m_ScreenSizeOverride;
	ScreenOverride_t m_ScreenPosOverride;

	int m_nFullscreenViewportX;
	int m_nFullscreenViewportY;
	int m_nFullscreenViewportWidth;
	int m_nFullscreenViewportHeight;
	ITexture *m_pFullscreenRenderTarget;

#ifdef LINUX
	struct font_entry
	{
		void *data;
		int size;
	};

	static CUtlDict< font_entry, unsigned short > m_FontData;

	static void *FontDataHelper( const char *pchFontName, int &size, const char *fontFileName );
#endif
};

#if GLMDEBUG
class MatSurfFuncLogger	// rip off of GLMFuncLogger - figure out a way to reunify these soon
{
	public:
	
		// simple function log
		MatSurfFuncLogger( char *funcName )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );

			m_funcName = funcName;
			m_earlyOut = false;
			
			prc->Printf( ">%s", m_funcName );
		};
		
		// more advanced version lets you pass args (i.e. called parameters or anything else of interest)
		// no macro for this one, since no easy way to pass through the args as well as the funcname
		MatSurfFuncLogger( char *funcName, PRINTF_FORMAT_STRING const char *fmt, ... )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );

			m_funcName = funcName;
			m_earlyOut = false;

			// this acts like GLMPrintf here
			// all the indent policy is down in GLMPrintfVA
			// which means we need to inject a ">" at the front of the format string to make this work... sigh.
			
			char modifiedFmt[2000];
			modifiedFmt[0] = '>';
			strcpy( modifiedFmt+1, fmt );
			
			va_list	vargs;
			va_start(vargs, fmt);
			prc->PrintfVA( modifiedFmt, vargs );
			va_end( vargs );
		}
		
		~MatSurfFuncLogger( )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );
			if (m_earlyOut)
			{
				prc->Printf( "<%s (early out)", m_funcName );
			}
			else
			{
				prc->Printf( "<%s", m_funcName );
			}
		};
	
		void EarlyOut( void )
		{
			m_earlyOut = true;
		};

		
		char *m_funcName;				// set at construction time
		bool m_earlyOut;
};

// handy macro to go with the helper class
#define	MAT_FUNC			MatSurfFuncLogger _logger_ ( __FUNCTION__ )

#else

#define	MAT_FUNC

#endif

#endif // MATSYSTEMSURFACE_H
