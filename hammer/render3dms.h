//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RENDER3DMS_H
#define RENDER3DMS_H
#pragma once


#include "Render.h"
#include "mathlib/Vector4D.h"
#include "utlpriorityqueue.h"
#include "mapclass.h"
#include "lpreview_thread.h"
#include "shaderapi/ishaderapi.h"

//
// Size of the buffer used for picking. See glSelectBuffer for documention on
// the contents of the selection buffer.
//
#define SELECTION_BUFFER_SIZE	50

//
// Size of the texture cache. THis is the maximum number of unique textures that
// a map can refer to and still render properly in the editor.
//
#define TEXTURE_CACHE_SIZE		2048

//
// Maximum number of objects that can be kept in the list of objects to render last.
//
#define MAX_RENDER_LAST_OBJECTS	256

//
// Maximum number of hits that can be returned by ObjectsAt.
//
#define MAX_PICK_HITS			512


class BoundBox;
class CCamera;
class CCullTreeNode;
class CMapClass;
class CMapDoc;
class CMapFace;
class CMapInstance;
class CMapWorld;
class IMaterial;
class IMaterialVar;
template< class T, class A >
class CUtlVector;

enum Visibility_t;
enum SelectionState_t;


typedef struct TranslucentObjects_s {
	float			depth;
	CMapAtom		*object;

	bool			m_bInstanceSelected;
	TInstanceState	m_InstanceState;
} TranslucentObjects_t;


enum RenderState_t
{
	RENDER_CENTER_CROSSHAIR,		// Whether to draw the crosshair in the center of the view.
	RENDER_GRID,					// Whether to draw a projected grid onto solid faces.
	RENDER_FILTER_TEXTURES,			// Whether to filter textures.
	RENDER_POLYGON_OFFSET_FILL,		// Whether to offset filled polygons (for decals)
	RENDER_POLYGON_OFFSET_LINE,		// Whether to offset line polygons (for wireframe selections)
	RENDER_REVERSE_SELECTION,		// Driver issue fix - whether to return the largest (rather than smallest) Z value when picking
};


//
// Render state information set via RenderEnable:
//
typedef struct
{
	bool bCenterCrosshair;	// Whether to render the center crosshair.
	bool bDrawGrid;			// Whether to render the grid.
	float fGridSpacing;		// Grid spacing in world units.
	float fGridDistance;	// Maximum distance from camera to draw grid.
	bool bFilterTextures;	// Whether to filter textures.
	bool bReverseSelection;	// Driver issue fix - whether to return the largest (rather than smallest) Z value when picking
} RenderStateInfo_t;

static inline bool RenderingModeIsTextured(EditorRenderMode_t mode)
{
	return (
		(mode==RENDER_MODE_TEXTURED) || 
		(mode==RENDER_MODE_TEXTURED_SHADED) || 
		(mode==RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) || 
		(mode==RENDER_MODE_LIGHT_PREVIEW2) );
}

//
// Picking state information used when called from ObjectsAt.
//
typedef struct
{
	bool bPicking;							// Whether we are rendering in pick mode or not.
	unsigned int m_nFlags;					// flags

	float fX;								// Leftmost coordinate of pick rectangle, passed in by caller.
	float fY;								// Topmost coordinate of pick rectangle, passed in by caller.
	float fWidth;							// Width of pick rectangle, passed in by caller.
	float fHeight;							// Height of pick rectangle, passed in by caller.

	HitInfo_t *pHitsDest;				// Final array in which to place pick hits, passed in by caller.
	int nMaxHits;							// Maximum number of hits to place in the 'pHits' array, passed in by caller, must be <= MAX_PICK_HITS.

	HitInfo_t Hits[MAX_PICK_HITS];	// Temporary array in which to place unsorted pick hits.
	int nNumHits;							// Number of hits so far in this pick (number of hits in 'Hits' array).

	unsigned int uSelectionBuffer[SELECTION_BUFFER_SIZE];
	unsigned int uLastZ;
} PickInfo_t;


typedef struct
{
	IEditorTexture *pTexture;		// Pointer to the texture object that implements this texture.
	int nTextureID;			// Unique ID of this texture across all renderers.
	unsigned int uTexture;	// The texture name as returned by OpenGL when the texture was uploaded in this renderer.
} TextureCache_t;


typedef struct
{
    HINSTANCE        hInstance;
    int              iCmdShow;
    HWND             hWnd;
	HDC				 hDC;
    bool             bActive;
    bool             bFullScreen;
    ATOM             wndclass;
    WNDPROC          wndproc;
    bool             bChangeBPP;
    bool             bAllowSoft;
    char            *szCmdLine;
    int              argc;
    char           **argv;
    int              iResCount;
    int              iVidMode;
} MatWinData_t;


class CRender3D : public CRender
{
public:

	// Constructor / Destructor.
	CRender3D(void);
	virtual ~CRender3D(void);

	// Initialization & shutdown functions.
	void ShutDown(void);

	float GetElapsedTime(void);
	float GetGridDistance(void);
	float GetGridSize(void);
	
	bool DeferRendering() const { return m_DeferRendering; }
	bool IsEnabled(RenderState_t eRenderState);
	bool IsPicking(void);

	virtual bool IsInLightingPreview();
	virtual void SetInLightingPreview( bool bLightingPreview );
	
	// Operations.
	
	float LightPlane(Vector& Normal);
	void UncacheAllTextures();

	bool SetView( CMapView *pView );
	void StartRenderFrame(void);
	void EndRenderFrame(void);

	virtual	void						PushInstanceData( CMapInstance *pInstanceClass, Vector &InstanceOrigin, QAngle &InstanceAngles );
	virtual	void						PopInstanceData( void );

	void ResetFocus();

	// Picking functions.
	void BeginRenderHitTarget(CMapAtom *pObject, unsigned int uHandle = 0);
	void EndRenderHitTarget(void);

	void Render(void);
	void RenderEnable(RenderState_t eRenderState, bool bEnable);

	void RenderCrossHair();
	virtual void RenderWireframeBox(const Vector &Mins, const Vector &Maxs, unsigned char chRed, unsigned char chGreen, unsigned char chBlue);
	void RenderBox(const Vector &Mins, const Vector &Maxs, unsigned char chRed, unsigned char chGreen, unsigned char chBlue, SelectionState_t eBoxSelectionState);
	void RenderArrow(Vector const &vStartPt, Vector const &vEndPt, unsigned char chRed, unsigned char chGreen, unsigned char chBlue);
	void RenderCone(Vector const &vBasePt, Vector const &vTipPt, float fRadius, int nSlices,
		            unsigned char chRed, unsigned char chGreen, unsigned char chBlue );
	void RenderSphere(Vector const &vCenter, float flRadius, int nTheta, int nPhi,
							  unsigned char chRed, unsigned char chGreen, unsigned char chBlue );
	void RenderWireframeSphere(Vector const &vCenter, float flRadius, int nTheta, int nPhi,
							            unsigned char chRed, unsigned char chGreen, unsigned char chBlue );
	void RenderInstanceMapClass( CMapInstance *pInstanceClass, CMapClass *pMapClass, Vector &InstanceOrigin, QAngle &InstanceAngles );
	

	int ObjectsAt( float x, float y, float fWidth, float fHeight, HitInfo_t *pObjects, int nMaxObjects, unsigned nFlags = 0 );

	void DebugHook1(void *pData = NULL);
	void DebugHook2(void *pData = NULL);

	// indicates we need to render an overlay pass...
	bool NeedsOverlay() const;

	void BuildLightList( CUtlVector<CLightingPreviewLightDescription> *pList ) const;

	void SendLightList();									// send lighting list to lighting preview thread
	
	void SendShadowTriangles();
	void AddTranslucentDeferredRendering( CMapPoint *pMapPoint );

protected:

	inline void DispatchRender3D(CMapClass *pMapClass);

	// Rendering functions.
	void RenderMapClass(CMapClass *pMapClass);
	void RenderInstanceMapClass_r(CMapClass *pMapClass);
	void RenderNode(CCullTreeNode *pNode, bool bForce);
	void RenderOverlayElements(void);
	void RenderTool(void);
	void RenderTree( CMapWorld *pWorld );
    void RenderPointsAndPortals(void);
	void RenderWorldAxes();
	void RenderTranslucentObjects( void );

	// Utility functions.
	void Preload(CMapClass *pParent);
	Visibility_t IsBoxVisible(Vector const &BoxMins, Vector const &BoxMaxs);

	// Frustum methods
	void ComputeFrustumRenderGeometry(CCamera * pCamera);
	void RenderFrustum();

	float m_fFrameRate;					// Framerate in frames per second, calculated once per second.
	int m_nFramesThisSample;			// Number of frames rendered in the current sample period.
	DWORD m_dwTimeLastSample;			// Time when the framerate was last calculated.

	DWORD m_dwTimeLastFrame;			// The time when the previous frame was rendered.
	float m_fTimeElapsed;				// Milliseconds elapsed since the last frame was rendered.

	// context for the last bitmap we sent to lighting preview for ray tracing. we do not send if
	// nothing happens, even if we end up re-rendering
	Vector m_LastLPreviewCameraPos;
	float m_fLastLPreviewAngles[3];						// y,p,r
	float m_fLastLPreviewZoom;
	int m_nLastLPreviewWidth;
	int m_nLastLPreviewHeight;

	Vector4D m_FrustumPlanes[6];		// Plane normals and constants for the current view frustum.
	
	MatWinData_t m_WinData;				// Defines our render window parameters.
	PickInfo_t m_Pick;					// Contains information used when rendering in pick mode.
	RenderStateInfo_t m_RenderState;	// Render state set via RenderEnable.

	bool m_bDroppedCamera;				// Whether we have dropped the camera for debugging.
	bool m_DeferRendering;				// Used when we want to sort lovely opaque objects
	bool m_TranslucentSortRendering;	// Used when we want to sort translucent objects
	CCamera *m_pDropCamera;				// Dropped camera to use for debugging.

	CUtlPriorityQueue<TranslucentObjects_t> m_TranslucentRenderObjects;				// List of objects to render after all the other objects.

	IMaterial* m_pVertexColor[2];		// for selecting actual textures

	bool m_bLightingPreview;

	// for debugging... render the view frustum
#ifdef _DEBUG
	Vector m_FrustumRenderPoint[8];
	bool m_bRenderFrustum;
	bool m_bRecomputeFrustumRenderGeometry;
#endif
};

#endif // RENDER3DGL_H
