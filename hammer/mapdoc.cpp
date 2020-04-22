//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The document. Exposes functions for object creation, deletion, and
//			manipulation. Holds the current tool. Handles GUI messages that are
//			view-independent.
//
//=============================================================================//

#include "stdafx.h"
#include <direct.h>
#include <io.h>
#include <mmsystem.h>
#include <process.h>
#include <direct.h>
#include "BuildNum.h"
#include "CustomMessages.h"
#include "EditPrefabDlg.h"
#include "EntityReportDlg.h"
#include "FaceEditSheet.h"
#include "GlobalFunctions.h"
#include "GotoBrushDlg.h"
#include "History.h"
#include "MainFrm.h"
#include "MapAnimator.h"
#include "MapCheckDlg.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapDisp.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapGroup.h"
#include "MapInfoDlg.h"
#include "MapInstance.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapViewLogical.h"
#include "MapView3D.h"
#include "MapWorld.h"
#include "NewVisGroupDlg.h"
#include "ObjectProperties.h"
#include "OptionProperties.h"
#include "Options.h"
#include "ProcessWnd.h"
#include "PasteSpecialDlg.h"
#include "Prefabs.h"
#include "Prefab3D.h"
#include "ReplaceTexDlg.h"
#include "RunMap.h"
#include "RunMapExpertDlg.h"
#include "SaveInfo.h"
#include "Manifest.h"
#include "ManifestDialog.h"

#include "ToolManager.h"
#include "ToolCamera.h"
#include "ToolEntity.h"

#include "SelectEntityDlg.h"
#include "Shell.h"
#include "StatusBarIDs.h"
#include "StrDlg.h"
#include "TextureSystem.h"
#include "TextureConverter.h"
#include "TransformDlg.h"
#include "VisGroup.h"
#include "hammer.h"
#include "ibsplighting.h"
#include "camera.h"
#include "MapDiffDlg.h"
#include "StockSolids.h"
#include "ToolMorph.h"
#include "ToolBlock.h"
#include "p4lib/ip4.h"

#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define KeyInt( key, dest ) \
	if (stricmp(szKey, key) != 0) \
		; \
	else \
	{ \
		CChunkFile::ReadKeyValueInt(szValue, dest); \
	}

#define KeyBool( key, dest ) \
	if (stricmp(szKey, key) != 0) \
		; \
	else \
	{ \
		CChunkFile::ReadKeyValueBool(szValue, dest); \
	}


#define MAX_REPLACE_LINE_LENGTH 256
#define LOGICAL_SPACING 500

#define HALF_LIFE_2_EYE_HEIGHT 64

#define VMF_FORMAT_VERSION	100
static int g_nFileFormatVersion = 0;


extern BOOL bSaveVisiblesOnly;
extern CShell g_Shell;


IMPLEMENT_DYNCREATE(CMapDoc, CDocument)

BEGIN_MESSAGE_MAP(CMapDoc, CDocument)
	//{{AFX_MSG_MAP(CMapDoc)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_MAP_SNAPTOGRID, OnMapSnaptogrid)
	ON_UPDATE_COMMAND_UI(ID_MAP_SNAPTOGRID, OnUpdateMapSnaptogrid)
	ON_COMMAND(ID_MAP_ENTITY_GALLERY, OnMapEntityGallery)
	ON_COMMAND(ID_EDIT_APPLYTEXTURE, OnEditApplytexture)
	ON_COMMAND(ID_TOOLS_SUBTRACTSELECTION, OnToolsSubtractselection)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SUBTRACTSELECTION, OnUpdateEditSelection)
	ON_COMMAND(ID_MAP_ENABLELIGHTPREVIEW, OnEnableLightPreview)
	ON_COMMAND(ID_ENABLE_LIGHT_PREVIEW_CUSTOM_FILENAME, OnEnableLightPreviewCustomFilename)
	ON_COMMAND(ID_MAP_DISABLELIGHTPREVIEW, OnDisableLightPreview)
	ON_COMMAND(ID_MAP_UPDATELIGHTPREVIEW, OnUpdateLightPreview)
	ON_COMMAND(ID_MAP_TOGGLELIGHTPREVIEW, OnToggleLightPreview)
	ON_COMMAND(ID_MAP_ABORTLIGHTCALCULATION, OnAbortLightCalculation)
	ON_COMMAND(ID_EDIT_COPYWC, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYWC, OnUpdateEditSelection)
	ON_COMMAND(ID_EDIT_PASTEWC, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEWC, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_CUTWC, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUTWC, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_GROUP, OnToolsGroup)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_GROUP, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_UNGROUP, OnToolsUngroup)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_UNGROUP, OnUpdateEditSelection)
	ON_COMMAND(ID_VIEW_GRID, OnViewGrid)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GRID, OnUpdateViewGrid)
	ON_COMMAND(ID_VIEW_LOGICAL_GRID, OnViewLogicalGrid)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOGICAL_GRID, OnUpdateViewLogicalGrid)
	ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectall)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, OnUpdateEditFunction)
	ON_COMMAND(ID_EDIT_REPLACE, OnEditReplace)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REPLACE, OnUpdateEditFunction)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_MAP_GRIDLOWER, OnMapGridlower)
	ON_COMMAND(ID_MAP_GRIDHIGHER, OnMapGridhigher)
	ON_COMMAND(ID_EDIT_TOWORLD, OnEditToWorld)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TOWORLD, OnUpdateEditSelection)
	ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
	ON_COMMAND(ID_FILE_EXPORTAGAIN, OnFileExportAgain)
	ON_COMMAND(ID_EDIT_MAPPROPERTIES, OnEditMapproperties)
	ON_UPDATE_COMMAND_UI(ID_EDIT_MAPPROPERTIES, OnUpdateEditFunction)
	ON_COMMAND(ID_FILE_CONVERT_WAD, OnFileConvertWAD)
	ON_UPDATE_COMMAND_UI(ID_FILE_CONVERT_WAD, OnUpdateFileConvertWAD)
	ON_COMMAND(ID_FILE_RUNMAP, OnFileRunmap)
	ON_COMMAND(ID_TOOLS_HIDEITEMS, OnToolsHideitems)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_HIDEITEMS, OnUpdateToolsHideitems)
	ON_COMMAND(ID_VIEW_HIDEUNCONNECTED, OnViewHideUnconnectedEntities)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HIDEUNCONNECTED, OnUpdateViewHideUnconnectedEntities)
	ON_COMMAND(ID_TOOLS_HIDE_ENTITY_NAMES, OnToolsHideEntityNames)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_HIDE_ENTITY_NAMES, OnUpdateToolsHideEntityNames)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditSelection)
	ON_COMMAND(ID_MAP_INFORMATION, OnMapInformation)
	ON_COMMAND(ID_VIEW_CENTERONSELECTION, OnViewCenterOnSelection)
	ON_COMMAND(ID_VIEW_CENTER3DVIEWSONSELECTION, OnViewCenter3DViewsOnSelection)
	ON_COMMAND(ID_EDIT_PASTESPECIAL, OnEditPastespecial)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTESPECIAL, OnUpdateEditPastespecial)
	ON_COMMAND(ID_EDIT_SELNEXT, OnEditSelnext)
	ON_COMMAND(ID_EDIT_SELPREV, OnEditSelprev)
	ON_COMMAND(ID_EDIT_SELNEXT_CASCADING, OnEditSelnextCascading)
	ON_COMMAND(ID_EDIT_SELPREV_CASCADING, OnEditSelprevCascading)
	ON_COMMAND(ID_LOGICALOBJECT_MOVETOGETHER, OnLogicalMoveBlock)
	ON_COMMAND(ID_LOGICALOBJECT_SELECTALLCASCADING, OnLogicalSelectAllCascading)	
	ON_COMMAND(ID_LOGICALOBJECT_SELECTALLCONNECTED, OnLogicalSelectAllConnected)	
	ON_COMMAND_EX(ID_VIEW_HIDESELECTEDOBJECTS, OnViewHideObjects)
	ON_COMMAND(ID_MAP_CHECK, OnMapCheck)
	ON_COMMAND(ID_VIEW_SHOWCONNECTIONS, OnViewShowconnections)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWCONNECTIONS, OnUpdateViewShowconnections)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_TOOLS_CREATEPREFAB, OnToolsCreateprefab)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_CREATEPREFAB, OnUpdateEditSelection)
	ON_COMMAND(ID_INSERTPREFAB_ORIGINAL, OnInsertprefabOriginal)
	ON_COMMAND(ID_EDIT_REPLACETEX, OnEditReplacetex)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REPLACETEX, OnUpdateEditFunction)
	ON_COMMAND(ID_TOOLS_HOLLOW, OnToolsHollow)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_HOLLOW, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_SNAPSELECTEDTOGRID, OnToolsSnapselectedtogrid)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SNAPSELECTEDTOGRID, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_SNAP_SELECTED_TO_GRID_INDIVIDUALLY, OnToolsSnapSelectedToGridIndividually)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SNAP_SELECTED_TO_GRID_INDIVIDUALLY, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_SPLITFACE, OnToolsSplitface)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SPLITFACE, OnUpdateToolsSplitface)
	ON_COMMAND(ID_TOOLS_TRANSFORM, OnToolsTransform)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TRANSFORM, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_TOGGLETEXLOCK, OnToolsToggletexlock)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TOGGLETEXLOCK, OnUpdateToolsToggletexlock)
	ON_COMMAND(ID_TOOLS_TOGGLETEXLOCKSCALE, OnToolsToggletexlockScale)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TOGGLETEXLOCKSCALE, OnUpdateToolsToggletexlockScale)
	ON_COMMAND(ID_TOOLS_TEXTUREALIGN, OnToolsTextureAlignment)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TEXTUREALIGN, OnUpdateToolsTextureAlignment)
	ON_COMMAND(ID_TOGGLE_CORDON, OnToggleCordon)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_CORDON, OnUpdateToggleCordon)
	ON_COMMAND_EX(ID_VIEW_HIDENONSELECTEDOBJECTS, OnViewHideObjects)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HIDENONSELECTEDOBJECTS, OnUpdateViewHideUnselectedObjects)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOW_HELPERS, OnUpdateViewShowHelpers)
	ON_COMMAND(ID_VIEW_SHOW_HELPERS, OnViewShowHelpers)
	ON_COMMAND(ID_VIEW_SHOWMODELSIN2D, OnViewShowModelsIn2D)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMODELSIN2D, OnUpdateViewShowModelsIn2D)
	ON_COMMAND(ID_VIEW_PREVIEW_MODEL_FADE, OnViewPreviewModelFade)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PREVIEW_MODEL_FADE, OnUpdateViewPreviewModelFade)
	ON_COMMAND(ID_COLLISION_WIREFRAME, OnCollisionWireframe)
	ON_UPDATE_COMMAND_UI(ID_COLLISION_WIREFRAME, OnUpdateCollisionWireframe)
	ON_COMMAND(ID_SHOW_DETAIL_OBJECTS, OnShowDetailObjects)
	ON_UPDATE_COMMAND_UI(ID_SHOW_DETAIL_OBJECTS, OnUpdateShowDetailObjects)
	ON_COMMAND(ID_SHOW_NODRAW_BRUSHES, OnShowNoDrawBrushes)
	ON_UPDATE_COMMAND_UI(ID_SHOW_NODRAW_BRUSHES, OnUpdateShowNoDrawBrushes)
	ON_COMMAND(ID_TOGGLE_GROUPIGNORE, OnToggleGroupignore)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_GROUPIGNORE, OnUpdateToggleGroupignore)
	ON_COMMAND(ID_VSCALE_TOGGLE, OnVscaleToggle)
	ON_COMMAND(ID_MAP_ENTITYREPORT, OnMapEntityreport)
	ON_COMMAND(ID_TOGGLE_SELECTBYHANDLE, OnToggleSelectbyhandle)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_SELECTBYHANDLE, OnUpdateToggleSelectbyhandle)
	ON_COMMAND(ID_TOGGLE_INFINITESELECT, OnToggleInfiniteselect)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_INFINITESELECT, OnUpdateToggleInfiniteselect)
	ON_COMMAND(ID_FILE_EXPORTTODXF, OnFileExporttodxf)
	ON_UPDATE_COMMAND_UI(ID_EDIT_APPLYTEXTURE, OnUpdateEditApplytexture)
	ON_COMMAND(ID_EDIT_CLEARSELECTION, OnEditClearselection)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARSELECTION, OnUpdateEditSelection)
	ON_COMMAND(ID_TOOLS_CENTER_ORIGINS, OnToolsCenterOrigins)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_CENTER_ORIGINS, OnUpdateEditSelection)
	ON_COMMAND(ID_MAP_LOADPOINTFILE, OnMapLoadpointfile)
	ON_COMMAND(ID_MAP_UNLOADPOINTFILE, OnMapUnloadpointfile)
	ON_COMMAND(ID_MAP_LOADPORTALFILE, OnMapLoadportalfile)
	ON_COMMAND(ID_MAP_UNLOADPORTALFILE, OnMapUnloadportalfile)
	ON_COMMAND(ID_TOGGLE_3D_GRID, OnToggle3DGrid)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_3D_GRID, OnUpdateToggle3DGrid)
	ON_COMMAND(ID_EDIT_TOENTITY, OnEditToEntity)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TOENTITY, OnUpdateEditSelection)
	ON_COMMAND_EX(ID_EDIT_UNDO, OnUndoRedo)
	ON_COMMAND_EX(ID_EDIT_REDO, OnUndoRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateUndoRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateUndoRedo)
	ON_COMMAND(ID_VSCALE_CHANGED, OnChangeVertexscale)
	ON_COMMAND(ID_GOTO_BRUSH, OnViewGotoBrush)
	ON_COMMAND(ID_GOTO_COORDS, OnViewGotoCoords)
	ON_COMMAND(ID_SHOW_SELECTED_BRUSH_NUMBER, OnMapShowSelectedBrushNumber)
	ON_COMMAND(ID_EDIT_FINDENTITIES, OnEditFindEntities)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FINDENTITIES, OnUpdateEditFunction)
	ON_COMMAND( ID_TOOLS_DISP_SOLIDDRAW, OnToggleDispSolidMask )
	ON_UPDATE_COMMAND_UI( ID_TOOLS_DISP_SOLIDDRAW, OnUpdateToggleSolidMask )	
	ON_COMMAND( ID_TOOLS_DISP_DRAWWALKABLE, OnToggleDispDrawWalkable )
	ON_UPDATE_COMMAND_UI( ID_TOOLS_DISP_DRAWWALKABLE, OnUpdateToggleDispDrawWalkable )	
	ON_COMMAND( ID_TOOLS_DISP_DRAW3D, OnToggleDispDraw3D )
	ON_UPDATE_COMMAND_UI( ID_TOOLS_DISP_DRAW3D, OnUpdateToggleDispDraw3D )	
	ON_COMMAND( ID_TOOLS_DISP_DRAWBUILDABLE, OnToggleDispDrawBuildable )
	ON_UPDATE_COMMAND_UI( ID_TOOLS_DISP_DRAWBUILDABLE, OnUpdateToggleDispDrawBuildable )	
	ON_COMMAND( ID_TOOLS_DISP_DRAWREMOVEDVERTS, OnToggleDispDrawRemovedVerts )
	ON_UPDATE_COMMAND_UI( ID_TOOLS_DISP_DRAWREMOVEDVERTS, OnUpdateToggleDispDrawRemovedVerts )	
    ON_COMMAND(ID_MAP_DIFFMAPFILE, OnMapDiff)
	ON_COMMAND(ID_LOGICALOBJECT_LAYOUTGEOMETRIC, OnLogicalobjectLayoutgeometric)
	ON_COMMAND(ID_LOGICALOBJECT_LAYOUTDEFAULT, OnLogicalobjectLayoutdefault)
	ON_COMMAND(ID_LOGICALOBJECT_LAYOUTLOGICAL, OnLogicalobjectLayoutlogical)
	ON_COMMAND(ID_TOOLS_INSTANCES_HIDE, OnToolsInstancesHide)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_INSTANCES_HIDE, OnUpdateToolsInstancesHide)
	ON_COMMAND(ID_TOOLS_INSTANCES_SHOWTINTED, OnToolsInstancesShowTinted)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_INSTANCES_SHOWTINTED, OnUpdateToolsInstancesShowTinted)
	ON_COMMAND(ID_TOOLS_INSTANCES_SHOWNORMAL, OnToolsInstancesShowNormal)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_INSTANCES_SHOWNORMAL, OnUpdateToolsInstancesShowNormal)
	ON_COMMAND(ID_INSTANCES_HIDEALL, OnInstancesHideAll)
	ON_COMMAND(ID_INSTANCES_SHOWALL, OnInstancesShowAll)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_INSTANCING_CREATEMANIFEST, &CMapDoc::OnInstancingCreatemanifest)
	ON_UPDATE_COMMAND_UI(ID_INSTANCING_CREATEMANIFEST, &CMapDoc::OnUpdateInstancingCreatemanifest)
	ON_COMMAND(ID_VERSIONCONTROL_CHECKINALL, &CMapDoc::OnInstancingCheckinAll)
	ON_UPDATE_COMMAND_UI(ID_VERSIONCONTROL_CHECKINALL, &CMapDoc::OnUpdateInstancingCheckinAll)
	ON_COMMAND(ID_VERSIONCONTROL_CHECKOUTMANIFEST, &CMapDoc::OnInstancingCheckOutManifest)
	ON_UPDATE_COMMAND_UI(ID_VERSIONCONTROL_CHECKOUTMANIFEST, &CMapDoc::OnUpdateInstancingCheckOutManifest)
	ON_COMMAND(ID_VERSIONCONTROL_ADDMANIFEST, &CMapDoc::OnInstancingAddManifest)
	ON_UPDATE_COMMAND_UI(ID_VERSIONCONTROL_ADDMANIFEST, &CMapDoc::OnUpdateInstancingAddManifest)
	ON_COMMAND(ID_INSTANCES_COLLAPSEALL, &CMapDoc::OnInstancesCollapseAll)
	ON_COMMAND(ID_INSTANCES_COLLAPSESELECTION, &CMapDoc::OnInstancesCollapseSelection)
	ON_COMMAND( ID_VIEW_QUICKHIDE, OnQuickHide_HideObjects )
	ON_UPDATE_COMMAND_UI(ID_VIEW_QUICKHIDE, OnUpdateEditSelection)
	ON_COMMAND( ID_VIEW_QUICKHIDEUNSELECTED, OnQuickHide_HideUnselectedObjects )
	ON_UPDATE_COMMAND_UI(ID_VIEW_QUICKHIDEUNSELECTED, OnUpdateEditSelection)
	ON_COMMAND( ID_VIEW_QUICKUNHIDE, OnQuickHide_Unhide )
	ON_UPDATE_COMMAND_UI(ID_VIEW_QUICKUNHIDE, OnQuickHide_UpdateUnHide)
	ON_COMMAND( ID_TOGGLE_RADIUSCULLING, OnRadiusCulling )
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_RADIUSCULLING, OnUpdateRadiusCulling )	
	ON_COMMAND( ID_VIEW_QUICKHIDEVISGROUP, OnQuickHide_CreateVisGroupFromHidden )
	ON_UPDATE_COMMAND_UI(ID_VIEW_QUICKHIDEVISGROUP, OnQuickHide_UpdateCreateVisGroupFromHidden)
	END_MESSAGE_MAP()


static CUtlVector<CMapDoc*> s_ActiveDocs;
CMapDoc		*CMapDoc::m_pMapDoc = NULL;
CManifest	*CMapDoc::m_pManifest = NULL;
int			CMapDoc::m_nInLevelLoad = 0;
static CProgressDlg *pProgDlg;

//
// Clipboard. Global to all documents to allow copying from one document and
// pasting in another.
//

class CHammerClipboard : public IHammerClipboard
{
public:
	CHammerClipboard(){}

public:
	virtual void Destroy() { delete this; }

public:
	CMapObjectList Objects;
	CMapWorld *pSourceWorld;
	BoundBox Bounds;
	Vector vecOriginalCenter;
};


IHammerClipboard * IHammerClipboard::CreateInstance()
{
	return new CHammerClipboard;
}

// Get the global Hammer clipboard
CHammerClipboard * GetHammerClipboard()
{
	static class CGlobalHammerClipboard :
	public CHammerClipboard
	{
		virtual void Destroy() { Assert( 0 ); }
	}
	s_Clipboard;
	return &s_Clipboard;
}

// Convert a clipboard interface
CHammerClipboard * GetHammerClipboard( IHammerClipboard *pInterface )
{
	return static_cast< CHammerClipboard * >( pInterface );
}


struct BatchReplaceTextures_t
{
	char szFindTexName[MAX_REPLACE_LINE_LENGTH];		// Texture to find.
	char szReplaceTexName[MAX_REPLACE_LINE_LENGTH];	// Texture to replace the found texture with.
};


struct AddNonSelectedInfo_t
{
	CVisGroup *pGroup;
	int nCount;
	CMapObjectList *pList;
	BoundBox *pBox;
	CMapWorld *pWorld;
};


struct ReplaceTexInfo_t
{
	char szFind[128];
	char szReplace[128];
	int iAction;
	int nReplaced;
	int iFindLen;	// strlen(szFind) - for speed
	CMapWorld *pWorld;
	CMapDoc *pDoc;
	BOOL bMarkOnly;
	BOOL bHidden;
	bool m_bRescaleTextureCoordinates;
};


struct FindEntity_t
{
	char szClassName[MAX_PATH];	// 
	Vector Pos;					// 
	CMapEntity *pEntityFound;	// Points to object found, NULL if unsuccessful.
};


struct SelectBoxInfo_t
{
	CMapDoc *pDoc;
	BoundBox *pBox;
	BOOL bInside;
	SelectMode_t eSelectMode;
};

struct SelectLogicalBoxInfo_t
{
	CMapDoc *pDoc;
	Vector2D vecMins;
	Vector2D vecMaxs;
	bool bInside;
	SelectMode_t eSelectMode;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor. Attaches all tools members to this document. Adds this
//			document to the list of active documents.
//-----------------------------------------------------------------------------
CMapDoc::CMapDoc(void)
{
	m_nLogicalPositionCount = 0;
	int nSize = sizeof(CMapFace);
	nSize = sizeof(CMapSolid);

	m_bHasInitialUpdate = false;
	m_bLoading = false;
	m_pWorld = NULL;

	//
	// Set up undo/redo system.
	//
	m_pUndo = new CHistory;
	m_pRedo = new CHistory;

	m_pUndo->SetDocument(this);
	m_pUndo->SetOpposite(TRUE, m_pRedo);

	m_pRedo->SetDocument(this);
	m_pRedo->SetOpposite(FALSE, m_pUndo);

	// init object selection 
	m_pSelection = new CSelection;
	m_pSelection->Init( this );

	m_VisGroups = new CUtlVector<CVisGroup *>();
	m_RootVisGroups = new CUtlVector<CVisGroup *>();

	// init tool manager
    m_pToolManager = new CToolManager;
	m_pToolManager->Init( this );

	Assert(GetMainWnd());

	m_bHideItems = false;
	m_bSnapToGrid = true;
	m_bShowGrid = true;
	m_bShowLogicalGrid = false;
	m_nGridSpacing = Options.view2d.iDefaultGrid;
	m_bShow3DGrid = false;

	m_tShowInstance = INSTANCES_SHOW_TINTED;

	m_nExternalReferenceCount = 0;

	m_nDocVersion = 0;

	m_nNextMapObjectID = 1;
	m_nNextNodeID = 1;

	m_pGame = NULL;

	m_flAnimationTime = 0.0f;

	m_bEditingPrefab = false;
	m_bPrefab = false;

	m_bIsAnimating = false;

	m_strLastExportFileName = "";

	m_bDispSolidDrawMask = true;
	m_bDispDrawWalkable = false;
	m_bDispDrawBuildable = false;
	m_bDispDraw3D = true;
	m_bDispDrawRemovedVerts = false;
	m_bVisGroupUpdatesLocked = false;

	s_ActiveDocs.AddToTail(this);

	m_pBSPLighting = 0;
	m_pPortalFile = NULL;

	m_SmoothingGroupVisual = 0;

	UpdateStatusBarSnap();

	//setup autosave members
	m_bNeedsAutosave = false;
	m_bIsAutosave = false;
	m_strAutosavedFrom = "";

	m_bIsCordoning = false;
	m_vCordonMins = Vector(-1024,-1024,-1024);
	m_vCordonMaxs = Vector( 1024,1024,1024);

	m_bIsEditable = true;
	m_pManifestOwner = NULL;
	m_bCollapsingInstances = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapDoc::~CMapDoc(void)
{
	GetMainWnd()->pObjectProperties->MarkDataDirty();

	//
	// Remove this doc from the list of active docs.
	//
	s_ActiveDocs.FindAndRemove(this);
	
	if (this == GetActiveMapDoc())
	{
		SetActiveMapDoc(NULL);
	}

	DeleteContents();

	delete m_pUndo;
	delete m_pRedo;

	delete m_pToolManager;

	if ( m_VisGroups )
	{
		delete m_VisGroups;
		m_VisGroups = NULL;
	}

	if ( m_RootVisGroups )
	{
		delete m_RootVisGroups;
		m_RootVisGroups = NULL;
	}

	if ( m_pManifestOwner == NULL && m_pSelection )
	{
		delete m_pSelection;
		m_pSelection = NULL;
	}

	OnDisableLightPreview();
}


//-----------------------------------------------------------------------------
// Default logical placement for new entities
//-----------------------------------------------------------------------------
void CMapDoc::GetDefaultNewLogicalPosition( Vector2D &vecPosition )
{
	int nMaxDim = ( g_MAX_MAP_COORD - g_MIN_MAP_COORD ) / LOGICAL_SPACING;
	int x = m_nLogicalPositionCount / nMaxDim;
	int y = m_nLogicalPositionCount - x * nMaxDim;
	x = x % nMaxDim;
	vecPosition.x = x * LOGICAL_SPACING;
	if ( vecPosition.x > g_MAX_MAP_COORD )
	{
		vecPosition.x += g_MIN_MAP_COORD - g_MAX_MAP_COORD;
	}
	vecPosition.y = y * LOGICAL_SPACING;
	if ( vecPosition.y > g_MAX_MAP_COORD )
	{
		vecPosition.y += g_MIN_MAP_COORD - g_MAX_MAP_COORD;
	}
	++m_nLogicalPositionCount;
}


//-----------------------------------------------------------------------------
// Purpose: Removes any object groups with no members or only one member.
//-----------------------------------------------------------------------------
void CMapDoc::RemoveEmptyGroups(void)
{
	int nEmptyGroupCount = 0;

 	CUtlVector<CMapGroup *> GroupList;
	m_pWorld->GetGroupList(GroupList);

	int nGroupCount = GroupList.Count();
	for (int i = nGroupCount - 1; i >= 0; i--)
	{
		CMapGroup *pGroup = GroupList.Element(i);

		if (!pGroup->GetChildCount())
		{
			// We found an empty group. Remove it.
			nEmptyGroupCount++;
			RemoveObjectFromWorld(pGroup, false);

			GroupList.FastRemove(i);
			delete pGroup;
		}
	}

	if (nEmptyGroupCount)
	{
		Msg(mwWarning, "Removed %d object group(s) with no children.\n", nEmptyGroupCount);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::AssignToGroups()
{
	//
	// Get a list of all the groups.
	//
 	CUtlVector<CMapGroup *> GroupList;
	int nGroupCount = m_pWorld->GetGroupList(GroupList);

	//
	// Assign all loaded objects to their proper groups.
	//
	CUtlVector<MapObjectPair_t> GroupedObjects;

	const CMapObjectList *pChildren = m_pWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		// Assign the object to its group, if any.

		CMapClass *pChild = pChildren->Element(pos);
		
		const char *pszGroupID = pChild->GetEditorKeyValue("groupid");
		if (pszGroupID != NULL)
		{
			int nID;
			CChunkFile::ReadKeyValueInt(pszGroupID, nID);

			MapObjectPair_t pair;
			for (int i = 0; i < nGroupCount; i++)
			{
				CMapGroup *pGroup = GroupList.Element(i);
				if (pGroup->GetID() == nID)
				{
					// Add the object to a list for removal from the world.
					pair.pObject1 = pChild;
					pair.pObject2 = pGroup;

					GroupedObjects.AddToTail(pair);
					break;
				}
			}
		}
	}

	//
	// Remove all the objects that were added to groups from the world, since they are
	// now children of CMapGroup objects that are already in the world.
	//
	int nPairCount = GroupedObjects.Count();
	for (int i = 0; i < nPairCount; i++)
	{
		m_pWorld->RemoveChild(GroupedObjects.Element(i).pObject1);
		GroupedObjects.Element(i).pObject2->AddChild(GroupedObjects.Element(i).pObject1);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after loading a VMF file. Assigns all objects to their proper
//			visgroups. It also finds the next unique ID to use when creating new objects.
//-----------------------------------------------------------------------------
void CMapDoc::AssignToVisGroups(void)
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		//
		// HACK: If this entity needs a node ID and doesn't have one, set it now. 
		//		 Would be better implemented more generically.
		//
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pChild);
		if (pEntity != NULL)
		{
			if (pEntity->IsNodeClass() && (pEntity->GetNodeID() == 0))
			{
				int nID = GetNextNodeID();
				char szID[80];
				itoa(nID, szID, 10);
				pEntity->SetKeyValue("nodeid", szID);
			}
		}

		//
		// Assign the object to its visgroups, if any. Visgroup IDs are held
		// in a temporary keyvalue list that was loaded from the VMF.
		//
		int nKeyCount = pChild->GetEditorKeyCount();
		for (int i = 0; i < nKeyCount; i++)
		{
			const char *pszKey = pChild->GetEditorKey(i);
			if (!stricmp(pszKey, "visgroupid"))
			{
				const char *pszVisGroupID = pChild->GetEditorKeyValue(i);
				Assert(pszVisGroupID != NULL);
				if (pszVisGroupID != NULL)
				{
					unsigned int nID = (unsigned int)atoi(pszVisGroupID);
					CVisGroup *pVisGroup = VisGroups_GroupForID(nID);

					// If an object is assigned to a nonexistent visgroup, we do nothing here, 
					// which effectively discards the bogus visgroup assignment.  
					if (pVisGroup)
					{
						pChild->AddVisGroup(pVisGroup);
						if (CVisGroup::IsConvertingOldVisGroups() && (pVisGroup->GetVisible() != VISGROUP_SHOWN))
						{
							pChild->VisGroupShow(false);
						}
					}
				}
			}
			else if (!stricmp(pszKey, "colorvisgroupid"))
			{
				const char *pszVisGroupID = pChild->GetEditorKeyValue(i);
				Assert(pszVisGroupID != NULL);
				if (pszVisGroupID != NULL)
				{
					unsigned int nID = (unsigned int)atoi(pszVisGroupID);
					CVisGroup *pVisGroup = VisGroups_GroupForID(nID);
					Assert(pVisGroup != NULL);
					pChild->SetColorVisGroup(pVisGroup);
				}
			}
		}

		//
		// Free the temporary keyvalue list.
		//
		pChild->RemoveEditorKeys();

		pChild = m_pWorld->GetNextDescendent(pos);
	}

	VisGroups_Validate();
	AssignAllToAutoVisGroups();

	VisGroups_UpdateAll();
}


//-----------------------------------------------------------------------------
// Purpose: Makes sure that the visgroup assignments are valid.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_Validate()
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild)
	{
		CMapClass *pParent = pChild->GetParent();

		// Old versions of Hammer had wacky visgroup assignments -- solid children of groups or entities
		// could belong to visgroups, even if those visgroups no longer existed.

		// For new versions of Hammer, just make sure that the object is allowed to belong to a visgroup.
		if (((g_nFileFormatVersion < 100) && (!IsWorldObject(pParent))) ||
			!VisGroups_ObjectCanBelongToVisGroup(pChild))
		{
			int nCount = pChild->GetVisGroupCount();
			if (nCount != 0)
			{
				Msg(mwWarning, "'%s', child of '%s', was in visgroups illegally. Removed from visgroups.", pChild->GetDescription(), pParent->GetDescription());
				pChild->RemoveAllVisGroups();
			}
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Begins a remote shell editing session. This causes the GUI to be
//			disabled to avoid version mismatches between the editor's map and the
//			shell client's map.
//-----------------------------------------------------------------------------
void CMapDoc::BeginShellSession(void)
{
	//
	// Disable all our views.
	//
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView *pView = GetNextView(pos);
		pView->EnableWindow(FALSE);
	}

	//
	// Set the modified flag to update our version number. This marks the working
	// version of the map in memory as different from the saved version on disk.
	//
	SetModifiedFlag();

	GetMainWnd()->BeginShellSession();
}

void CMapDoc::GetSelectedCenter(Vector &vCenter)
{
	Morph3D *pMorphTool = dynamic_cast<Morph3D*>( m_pToolManager->GetActiveTool() );
	CToolBlock *pBlockTool = dynamic_cast<CToolBlock*>( m_pToolManager->GetActiveTool() );

	if ( pMorphTool )
	{
		pMorphTool->GetSelectedCenter(vCenter);
	}
	else if ( pBlockTool )
	{
		pBlockTool->GetBoundsCenter(vCenter);
	}
	else if (!m_pSelection->IsEmpty())
	{
		m_pSelection->GetBoundsCenter(vCenter);
	}
	else if ( m_pWorld )
	{
		m_pWorld->GetBoundsCenter(vCenter);
	}
	else
	{
		vCenter.Init();
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::CenterViewsOnSelection()
{
	Vector vecCenter;
	GetSelectedCenter( vecCenter );

	Center2DViewsOn(vecCenter);
	Center3DViewsOn(vecCenter);
	CenterLogicalViewsOnSelection();
}


//-----------------------------------------------------------------------------
// Purpose: Handles the View | Center On Selection menu item.
//-----------------------------------------------------------------------------
void CMapDoc::Center2DViewsOnSelection(void)
{
	Vector vecCenter;
	GetSelectedCenter( vecCenter );

	Center2DViewsOn( vecCenter );
}


//-----------------------------------------------------------------------------
// Purpose: Handles the View | Center 3D Views On Selection menu item.
//-----------------------------------------------------------------------------
void CMapDoc::Center3DViewsOnSelection()
{
	Vector vecCenter; 
	GetSelectedCenter( vecCenter );
	
	Center3DViewsOn( vecCenter );
}


void CMapDoc::CenterLogicalViewsOnSelection()
{
	Vector2D vecLogicalCenter;
	if ( m_pSelection->GetLogicalBoundsCenter( vecLogicalCenter ) )
	{
		CenterLogicalViewsOn(vecLogicalCenter);
	}
}

		
//-----------------------------------------------------------------------------
// Purpose: Called after loading a VMF file. Finds the next unique IDs to use
//			when creating new objects, nodes, and faces.
//-----------------------------------------------------------------------------
void CMapDoc::CountGUIDs(void)
{
	CTypedPtrList<CPtrList, MapObjectPair_t *> GroupedObjects;

	// This increments the CMapWorld face ID but it doesn't matter since we're setting it below.
	int nNextFaceID = m_pWorld->FaceID_GetNext();

	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		//
		// Keep track of the highest numbered object ID in this document.
		//
		if (pChild->GetID() >= m_nNextMapObjectID)
		{
			m_nNextMapObjectID = pChild->GetID() + 1;
		}

		//
		// Keep track of the highest numbered node ID in this document.
		//
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pChild);
		if (pEntity != NULL)
		{
			//
			// Blah. Classes aren't assigned until PostLoadWorld, so we have to
			// look at our classname keyvalue to determine whether we are a node class.
			//
			const char *pszClass = pEntity->GetKeyValue("classname");
			if (pEntity->IsNodeClass(pszClass))
			{
				int nID = pEntity->GetNodeID();
				if (nID >= m_nNextNodeID)
				{
					m_nNextNodeID = nID + 1;
				}
			}
		}
		else
		{
			//
			// Keep track of the highest numbered face ID in this document.
			//
			CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pChild);
			if (pSolid != NULL)
			{
				for (int nFace = 0; nFace < pSolid->GetFaceCount(); nFace++)
				{
					CMapFace *pFace = pSolid->GetFace(nFace);
					int nFaceID = pFace->GetFaceID();
					if (nFaceID >= nNextFaceID)
					{
						nNextFaceID = nFaceID + 1;
					}
				}
			}
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}

	m_pWorld->FaceID_SetNext(nNextFaceID);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszClassName - 
//			x - 
//			y - 
//			z - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
CMapEntity *CMapDoc::CreateEntity(const char *pszClassName, float x, float y, float z)
{
	CMapEntity *pEntity = new CMapEntity;
	if (pEntity != NULL)
	{
		GetHistory()->MarkUndoPosition(NULL, "New Entity");

		pEntity->SetPlaceholder(TRUE);

		Vector Pos;
		Pos[0] = x;
		Pos[1] = y;
		Pos[2] = z;
		pEntity->SetOrigin(Pos);

		pEntity->SetClass(pszClassName);

		AddObjectToWorld(pEntity);
		GetHistory()->KeepNew(pEntity);

		//
		// Update all the views.
		//
		/* UpdateBox ub;
		CMapObjectList ObjectList;
		ObjectList.AddTail(pEntity);
		ub.Objects = &ObjectList;
		ub.Box = *((BoundBox *)pEntity); */

		SetModifiedFlag();
	}

	return(pEntity);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszClassName - 
//			x - 
//			y - 
//			z - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
// dvs: Seems better to move some of this functionality into DeleteObject and replace calls to this with FindEntity/DeleteObject calls.
//      Probably need to replicate all functions in the doc as follows: DeleteObject, DeleteObjectList to optimize updates. Either that
//		or we need a way to lock and unlock updates.
bool CMapDoc::DeleteEntity(const char *pszClassName, float x, float y, float z)
{
	CMapEntity *pEntity = FindEntity(pszClassName, x, y, z);
	if (pEntity != NULL)
	{
		GetHistory()->MarkUndoPosition(NULL, "Delete");

		DeleteObject(pEntity);

		SetModifiedFlag();

		return(true);
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Ends a remote shell editing session. This enables the GUI that was
//			disabled by BeginShellSession.
//-----------------------------------------------------------------------------
void CMapDoc::EndShellSession(void)
{
	//
	// Enable all our views.
	//
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView *pView = GetNextView(pos);
		pView->EnableWindow(TRUE);
	}

	GetMainWnd()->EndShellSession();
}


//-----------------------------------------------------------------------------
// Purpose: Finds an object in the map by number. The number corresponds to the
//			order in which the object is written to the map file. Thus, through
//			this function, brushes and entities can be located by ordinal, as
//			reported by the MAP compile tools.
// Input  : pObject - Object being checked for a match.
//			pFindInfo - Structure containing the search criterea.
// Output : Returns FALSE if this is the object that we are looking for, TRUE
//			to continue iterating.
//-----------------------------------------------------------------------------
BOOL CMapDoc::FindEntityCallback(CMapClass *pObject, FindEntity_t *pFindInfo)
{
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);

	if (pEntity != NULL)
	{
		Vector Pos;
		pEntity->GetOrigin(Pos);

		// HACK: Round to origin integers since entity origins are rounded when
		//       saving to MAP file. This makes finding entities from the engine
		//       in the editor work.
		Pos[0] = V_rint(Pos[0]);
		Pos[1] = V_rint(Pos[1]);
		Pos[2] = V_rint(Pos[2]);

		if (VectorCompare(Pos, pFindInfo->Pos))
		{
			if (stricmp(pEntity->GetClassName(), pFindInfo->szClassName) == 0)
			{
				pFindInfo->pEntityFound = pEntity;
				return(FALSE);
			}
		}
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Finds an entity by classname and position. Some ambiguity if two
//			entities of the same name are at the same position.
// Input  : pszClassName - Class name of entity to find, ie "info_node".
//			x, y, z - Position of entity in world coordinates.
// Output : Returns a pointer to the entity, NULL if none was found.
//-----------------------------------------------------------------------------
CMapEntity *CMapDoc::FindEntity(const char *pszClassName, float x, float y, float z)
{
	CMapEntity *pEntity = NULL;

	if (pszClassName != NULL)
	{
		FindEntity_t FindInfo;

		memset(&FindInfo, 0, sizeof(FindInfo));
		strcpy(FindInfo.szClassName, pszClassName);

		// dvs: HACK - only find by integer coordinates because the editor rounds
		//		entity origins when saving the MAP file.
		FindInfo.Pos[0] = V_rint(x);
		FindInfo.Pos[1] = V_rint(y);
		FindInfo.Pos[2] = V_rint(z);

		m_pWorld->EnumChildren((ENUMMAPCHILDRENPROC)FindEntityCallback, (DWORD)&FindInfo, MAPCLASS_TYPE(CMapEntity));

		if (FindInfo.pEntityFound != NULL)
		{
			Assert(FindInfo.pEntityFound->IsMapClass(MAPCLASS_TYPE(CMapEntity)));
		}

		pEntity = FindInfo.pEntityFound;
	}

	return(pEntity);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapEntity *CMapDoc::FindEntityByName( const char *pszName, bool bVisiblesOnly )
{
	return m_pWorld->FindEntityByName( pszName, bVisiblesOnly );
}


//-----------------------------------------------------------------------------
// Purpose: Finds all entities in the map with a given class name.
// Input  : pFound - List of entities with the class name.
//			pszClassName - Class name to match, case insensitive.
// Output : Returns true if any matches were found, false if not.
//-----------------------------------------------------------------------------
bool CMapDoc::FindEntitiesByClassName(CMapEntityList &Found, const char *pszClassName, bool bVisiblesOnly)
{
	return m_pWorld->FindEntitiesByClassName( Found, pszClassName, bVisiblesOnly );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMapDoc::FindEntitiesByKeyValue(CMapEntityList &Found, const char *pszKey, const char *pszValue, bool bVisiblesOnly)
{
	return m_pWorld->FindEntitiesByKeyValue( Found, pszKey, pszValue, bVisiblesOnly );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMapDoc::FindEntitiesByName( CMapEntityList &Found, const char *pszName, bool bVisiblesOnly )
{
	return m_pWorld->FindEntitiesByName( Found, pszName, bVisiblesOnly );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMapDoc::FindEntitiesByNameOrClassName(CMapEntityList &Found, const char *pszName, bool bVisiblesOnly)
{
	return m_pWorld->FindEntitiesByNameOrClassName(Found, pszName, bVisiblesOnly);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapDoc::GetDocumentCount(void)
{
	return s_ActiveDocs.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapDoc *CMapDoc::GetDocument(int index)
{
	return s_ActiveDocs.Element(index);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnViewGotoCoords()
{
	CStrDlg dlg(0, "", "Coordinates to go to (x y z), ex: 200 -4096 1154\nex: setpos -1 2 3; setang 4 5 6", "Go to coordinates");
	while (dlg.DoModal() == IDOK)
	{
		Vector posVec;
		Vector angVec;
		char setposString[255];
		char setangString[255];
		char semicolonString[2];
		
		if (sscanf(dlg.m_string, "%f %f %f", &posVec.x, &posVec.y, &posVec.z) == 3)
		{
			// SILLY:
			if ((posVec.x == 200) && (posVec.y == -4096) && (posVec.z == 1154))
			{
				if (AfxMessageBox("Seriously?\n\n(I mean, that was just an example, you were supposed to type in your own numbers)", MB_YESNO | MB_ICONQUESTION) != IDYES)
					continue;
			}
			CenterViewsOn(posVec);
			return;
		}
		if ( sscanf( dlg.m_string, "%s %f %f %f%s %s %f %f %f", setposString, &posVec.x, &posVec.y, &posVec.z, semicolonString, setangString, &angVec.x, &angVec.y, &angVec.z ) == 9 )
		{
			posVec.z += HALF_LIFE_2_EYE_HEIGHT;			
			CenterViewsOn( posVec );
			Set3DViewsPosAng( posVec, angVec );
			return;
		}
		

		AfxMessageBox("Please enter 3 coordinates, space-delimited or use\nsetpos x y z; setang u v w format.", MB_OK | MB_ICONEXCLAMATION);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Invokes a dialog for finding entities by targetname.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditFindEntities(void)
{
	CStrDlg dlg(0, "", "Targetname to find:", "Find Entities");
	if (dlg.DoModal() == IDOK)
	{
		CMapEntityList Found;
		FindEntitiesByName(Found, dlg.m_string, false);
		if (Found.Count() != 0)
		{
			CMapObjectList Select;
			FOR_EACH_OBJ( Found, pos )
			{
				CMapEntity *pEntity = Found.Element(pos);
				Select.AddToTail(pEntity);
			}

			SelectObjectList(&Select);
			CenterViewsOnSelection();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Brings up the 'Go to Brush ID' dialog and selects the indicated
//			brush, if it can be found.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewGotoBrush(void)
{
	CGotoBrushDlg dlg;

	if (dlg.DoModal() == IDOK)
	{
		CMapSolid *pFoundSolid = NULL;

		EnumChildrenPos_t pos;
		CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
		while (pChild)
		{
			CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pChild);
			if (pSolid && (pSolid->GetID() == dlg.m_nBrushID))
			{
				pFoundSolid = pSolid;
				break;
			}
			pChild = m_pWorld->GetNextDescendent(pos);
		}

		//
		// If we found the brush, select it and center the 2D views on it.
		//
		if (pFoundSolid != NULL)
		{
			SelectObject(pFoundSolid, scSelect|scClear|scSaveChanges );
			CenterViewsOnSelection();
		}
		else
		{
			AfxMessageBox("That brush ID does not exist.");
		}
	}
}

class CFindBrushInfo
{
public:
	CMapSolid *m_pBrush; // The brush to find.
	int m_nObjectCount;
	bool m_bFound;
};


BOOL CMapDoc::GetBrushNumberCallback(CMapClass *pObject, void *pFindInfoVoid)
{
	CFindBrushInfo *pFindInfo = (CFindBrushInfo*)pFindInfoVoid;
	if ( pObject->IsVisible() )
	{
		if ( pObject == pFindInfo->m_pBrush )
		{
			pFindInfo->m_bFound = true;
			return FALSE; // found it!
		}
		else
		{
			pFindInfo->m_nObjectCount++;
			return TRUE;
		}
	}
	else
	{
		return TRUE;
	}
}


void CMapDoc::OnMapShowSelectedBrushNumber()
{
	CFindBrushInfo info;

	info.m_nObjectCount = 0;
	info.m_bFound = false;

	if ( m_pSelection->IsEmpty() )
	{
		AfxMessageBox( ID_NO_BRUSH_SELECTED, MB_OK );
		return;
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	info.m_pBrush = dynamic_cast< CMapSolid* >( pSelList->Element( 0 ) );
	if ( !info.m_pBrush )
	{
		AfxMessageBox( ID_NO_BRUSH_SELECTED, MB_OK );
		return;
	}

	// Enumerate the visible brushes..
	m_pWorld->EnumChildrenRecurseGroupsOnly(
		(ENUMMAPCHILDRENPROC)&CMapDoc::GetBrushNumberCallback, (DWORD)&info, MAPCLASS_TYPE(CMapSolid));

	CString str;
	if ( info.m_bFound )
	{
		str.FormatMessage( ID_BRUSH_NUMBER, info.m_nObjectCount );
	}
	else
	{
		str.FormatMessage( ID_BRUSH_NOT_FOUND );
	}
	AfxMessageBox( str, MB_OK );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : TRUE if the document was successfully initialized; otherwise FALSE.
//-----------------------------------------------------------------------------
BOOL CMapDoc::OnNewDocument(void)
{
	if (!CDocument::OnNewDocument())
	{
		return(FALSE);
	}

	if (!SelectDocType())
	{
		return(FALSE);
	}

	Initialize();

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Creates an empty world and initializes the path list.
//-----------------------------------------------------------------------------
void CMapDoc::Initialize(void)
{
	Assert(!m_pWorld);

	m_pWorld = new CMapWorld( this );
	m_pWorld->CullTree_Build();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadEntityCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	CMapEntity *pEntity = new CMapEntity;

	ChunkFileResult_t eResult = pEntity->LoadVMF(pFile);

	if (eResult == ChunkFile_Ok)
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();
		pWorld->AddChild(pEntity);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadHiddenCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("entity", (ChunkHandler_t)CMapDoc::LoadEntityCallback, pDoc);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk();
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			szChunkName - 
//			eError - 
// Output : Returns true to continue loading, false to stop loading.
//-----------------------------------------------------------------------------
bool CMapDoc::HandleLoadError(CChunkFile *pFile, const char *szChunkName, ChunkFileResult_t eError, CMapDoc *pDoc)
{
	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Loads this document from a VMF file.
// Input  : pszFileName - Full path of file to load.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapDoc::LoadVMF( const char *pszFileName, int LoadFlags )
{
	bool			CreateProgressDlg = m_nInLevelLoad == 0;

	m_nInLevelLoad++;

	//
	// Create a new world to hold the loaded objects.
	//
	if (m_pWorld == NULL)
	{
		m_pWorld = new CMapWorld( this );
	}

	// Show our progress dialog.
	if ( CreateProgressDlg )
	{
		pProgDlg = new CProgressDlg;
		pProgDlg->Create();

		pProgDlg->SetRange(0,18000);
		pProgDlg->SetStep(1000);
	}

	// Set the progress dialog title
	CString caption;
	caption.LoadString(IDS_LOADINGFILE);
	pProgDlg->SetWindowText(caption);

	g_nFileFormatVersion = 0;

	bool bLocked = VisGroups_LockUpdates( true );

	//
	// Open the file.
	//
	CChunkFile File;
	ChunkFileResult_t eResult = File.Open(pszFileName, ChunkFile_Read);
	pProgDlg->StepIt();

	//
	// Read the file.
	//
	if (eResult == ChunkFile_Ok)
	{
		DetailObjects::EnableBuildDetailObjects( false );
		
		//
		// Set up handlers for the subchunks that we are interested in.
		//
		CChunkHandlerMap Handlers;
		Handlers.AddHandler("world", (ChunkHandler_t)CMapDoc::LoadWorldCallback, this);
		Handlers.AddHandler("hidden", (ChunkHandler_t)CMapDoc::LoadHiddenCallback, this);
		Handlers.AddHandler("entity", (ChunkHandler_t)CMapDoc::LoadEntityCallback, this);
		Handlers.AddHandler("versioninfo", (ChunkHandler_t)CMapDoc::LoadVersionInfoCallback, this);
		Handlers.AddHandler("autosave", (ChunkHandler_t)CMapDoc::LoadAutosaveCallback, this);
		Handlers.AddHandler("visgroups", (ChunkHandler_t)CVisGroup::LoadVisGroupsCallback, this);
		Handlers.AddHandler("viewsettings", (ChunkHandler_t)CMapDoc::LoadViewSettingsCallback, this);
		Handlers.AddHandler("cordon", (ChunkHandler_t)CMapDoc::LoadCordonCallback, this);

		m_pToolManager->AddToolHandlers( &Handlers );

		Handlers.SetErrorHandler((ChunkErrorHandler_t)CMapDoc::HandleLoadError, this);

		File.PushHandlers(&Handlers);

		if ( ( LoadFlags & VMF_LOAD_ACTIVATE ) )
		{
			SetActiveMapDoc( this );
		}
		m_bLoading = true;
		
		//
		// Read the sub-chunks. We ignore keys in the root of the file, so we don't pass a
		// key value callback to ReadChunk.
		//

		pProgDlg->SetWindowText( "Reading Chunks..." );
		while (eResult == ChunkFile_Ok)
		{
			eResult = File.ReadChunk();
		}
		pProgDlg->SetStep(5000);
		pProgDlg->StepIt();
		
		if (eResult == ChunkFile_EOF)
		{
			eResult = ChunkFile_Ok;
		}

		File.PopHandlers();
	}


	if (eResult == ChunkFile_Ok)
	{
		pProgDlg->SetWindowText( "Postload Processing..." );
		Postload( pszFileName );

		pProgDlg->StepIt();
		m_bLoading = false;
	}
	else
	{
		GetMainWnd()->MessageBox(File.GetErrorText(eResult), "Error loading file", MB_OK | MB_ICONEXCLAMATION);
	}

	if ( bLocked )
		VisGroups_LockUpdates( false );

	if ( pProgDlg && CreateProgressDlg )
	{
		pProgDlg->DestroyWindow();
		delete pProgDlg;
		pProgDlg = NULL;
	}

	if ( ( LoadFlags & VMF_LOAD_ACTIVATE ) )
	{	// force rendering even if application is not active
		UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
		APP()->SetForceRenderNextFrame();
	}

	m_nInLevelLoad--;

	return(eResult == ChunkFile_Ok);
}


void CMapDoc::BuildAllDetailObjects()
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pChild);
		if (pSolid != NULL)
		{
			int nFaces = pSolid->GetFaceCount();
			for(int i = 0; i < nFaces; i++)
			{
				CMapFace *pFace = pSolid->GetFace( i );
				if ( pFace )
					DetailObjects::BuildAnyDetailObjects( pFace );
			}
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pLoadInfo - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadVersionInfoCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	return(pFile->ReadChunk((KeyHandler_t)LoadVersionInfoKeyCallback, pDoc));
}


//-----------------------------------------------------------------------------
// Purpose: Handles keyvalues when loading the version info chunk of VMF files.
// Input  : szKey - Key to handle.
//			szValue - Value of key.
//			pDoc - Document being loaded.
// Output : Returns ChunkFile_Ok if all is well.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadVersionInfoKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc)
{
	KeyInt("mapversion", pDoc->m_nDocVersion);
	KeyInt("formatversion", g_nFileFormatVersion);
	KeyBool("prefab", pDoc->m_bPrefab);

	return(ChunkFile_Ok);
}

ChunkFileResult_t CMapDoc::LoadAutosaveCallback( CChunkFile *pFile, CMapDoc *pDoc)
{
	return(pFile->ReadChunk((KeyHandler_t)LoadAutosaveKeyCallback, pDoc));
}

ChunkFileResult_t CMapDoc::LoadAutosaveKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc)
{
	if (!stricmp(szKey, "originalname"))
	{
		pDoc->m_bIsAutosave = true;
		char szTempName[MAX_PATH];
		Q_strcpy( szTempName, szValue );
		Q_FixSlashes( szTempName, '\\' );
		pDoc->m_strAutosavedFrom = szTempName;
		
	}

	return(ChunkFile_Ok);
}

ChunkFileResult_t CMapDoc::LoadCordonCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	return pFile->ReadChunk((KeyHandler_t)LoadCordonKeyCallback, pDoc);
}

ChunkFileResult_t CMapDoc::LoadCordonKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc)
{
	if (!stricmp(szKey, "mins"))
	{
		CChunkFile::ReadKeyValuePoint(szValue, pDoc->m_vCordonMins);
	}
	else if (!stricmp(szKey, "maxs"))
	{
		CChunkFile::ReadKeyValuePoint(szValue, pDoc->m_vCordonMaxs);
	}
	else if (!stricmp(szKey, "active"))
	{
		bool bActive;
		CChunkFile::ReadKeyValueBool(szValue, bActive );
		pDoc->SetCordoning( bActive );
		
	}

	return(ChunkFile_Ok);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadViewSettingsKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc)
{
	KeyBool( "bSnapToGrid", pDoc->m_bSnapToGrid);
	KeyBool( "bShowGrid", pDoc->m_bShowGrid);
	KeyBool( "bShowLogicalGrid", pDoc->m_bShowLogicalGrid);
	KeyInt( "nGridSpacing", pDoc->m_nGridSpacing);
	KeyBool( "bShow3DGrid", pDoc->m_bShow3DGrid);

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Loads the view settings chunk, where per-map view settings are kept.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadViewSettingsCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadViewSettingsKeyCallback, pDoc);
	if (eResult == ChunkFile_Ok)
	{	
		pDoc->UpdateStatusBarSnap();
	}

	return eResult;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::LoadWorldCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	CMapWorld *pWorld = pDoc->GetMapWorld();
	ChunkFileResult_t eResult = pWorld->LoadVMF(pFile);
	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Called after loading a map file.
//-----------------------------------------------------------------------------
void CMapDoc::Postload(const char *pszFileName)
{
	if ( pszFileName[ 0 ] ) 
	{	// this path needs to be set early so that instances may properly find their base path
		SetPathName( pszFileName, FALSE );
	}

	//
	// Report any noncritical loading errors here.
	//
	if (CMapSolid::GetBadSolidCount() > 0)
	{
		char szError[ 1024 ];

		V_sprintf_safe( szError, "For your information, %d solid(s) were not loaded due to errors in the file. Would you like to Re-Save your map with the invalid solids removed?", CMapSolid::GetBadSolidCount() );
		if ( GetMainWnd()->MessageBox(szError, "Warning", MB_YESNO | MB_ICONQUESTION) == IDYES )
		{
			OnFileSave();
		}
	}

	//
	// Count GUIDs before calling PostLoadWorld because objects that need to generate GUIDs
	// may do so in PostLoadWorld.
	//
	CountGUIDs();
	m_pWorld->PostloadWorld();
	if ( pProgDlg )
	{
		pProgDlg->StepIt();
		pProgDlg->SetStep(1000);
	}

	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Assigning to groups..." );
	}
	AssignToGroups();
	AssignToVisGroups();
	if ( pProgDlg )
	{
		pProgDlg->StepIt();
	}

	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Postprocessing VisGroups..." );
	}
	m_pWorld->PostloadVisGroups();
	if ( pProgDlg )
	{
		pProgDlg->StepIt();
	}

	// Do this after AssignToVisGroups, because deleting objects causes empty visgroups to be purged,
	// and until AssignToVisGroups is called all the visgroups are empty!
	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Updating Visibility..." );
	}
	RemoveEmptyGroups();
	UpdateVisibilityAll();
	if ( pProgDlg )
	{
	    pProgDlg->StepIt();
	}

	// update displacement neighbors
	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Updating Displacements..." );
	}
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		int count = pDispMgr->WorldCount();
		for( int ndx = 0; ndx < count; ndx++ )
		{
			CMapDisp *pDisp = pDispMgr->GetFromWorld( ndx );
			if( pDisp )
			{
				CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
				pDispMgr->FindWorldNeighbors( pFace->GetDisp() );
			}
		}
	}
	if ( pProgDlg )
	{
	    pProgDlg->StepIt();
	}

	//
	// Do batch search and replace of textures from trans.txt if it exists.
	//
	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Updating Texture Names..." );
	}
	char translationFilename[MAX_PATH];
	Q_snprintf( translationFilename, sizeof( translationFilename ), "materials/trans.txt" );
	FileHandle_t searchReplaceFP = g_pFileSystem->Open( translationFilename, "r" );
	if( searchReplaceFP )
	{
		BatchReplaceTextures( searchReplaceFP );
		g_pFileSystem->Close( searchReplaceFP );
	}
	if ( pProgDlg )
	{
	    pProgDlg->StepIt();
	}

	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Building Cull Tree..." );
	}
	m_pWorld->CullTree_Build();
	if ( pProgDlg )
	{
	    pProgDlg->StepIt();
	}

	// We disabled building detail objects above to prevent it from generating them extra times.
	// Now generate the ones that need to be generated.
	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Building Detail Objects..." );
	}
	DetailObjects::EnableBuildDetailObjects( true );
	BuildAllDetailObjects();

	if ( pProgDlg )
	{
		pProgDlg->SetWindowText( "Finished Loading!" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will hook up the manifest document to this map document
// Input  : pManifest - the manifest document
//-----------------------------------------------------------------------------
void CMapDoc::SetManifest( CManifest *pManifest ) 
{ 
	m_pManifest = pManifest; 
	m_pManifestOwner = pManifest; 

	if ( m_pSelection )
	{
		delete m_pSelection;
	}

	m_pSelection = pManifest->GetSelection();
}


//-----------------------------------------------------------------------------
// Purpose: this routine will check to see if all objects are editable within 
//			the selection.  If one is not editable, the entire selection is not
//			editable.
// Output : the editable state of the selection
//-----------------------------------------------------------------------------
bool CMapDoc::IsSelectionEditable( void )
{
	bool bResult = true;

	int nCount = m_pSelection->GetCount();
	for( int i = 0; i < nCount; i++ )
	{
		CMapClass *pObj = m_pSelection->GetList()->Element( i );
		if ( !pObj->IsEditable() )
		{
			bResult = false;
			break;
		}
	}

	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose: this routine will take a single map document and turn it into a
//			manifest document containing just this one map document.
// Output : true if the conversion worked
//-----------------------------------------------------------------------------
bool CMapDoc::CreateNewManifest( void )
{
	if ( IsModified() )
	{
		OnFileSave();
	}

	if ( IsModified() )
	{
		AfxMessageBox( "Manifest was NOT created as map was not able to be saved!", MB_OK );
		return false;
	}

	CManifest	*pManifest = dynamic_cast< CManifest * >( APP()->pManifestDocTemplate->OpenDocumentFile( NULL ) );
	if(Options.general.bLoadwinpos && Options.general.bIndependentwin)
	{
		::GetMainWnd()->LoadWindowStates();
	}

	if ( pManifest->AddExistingMap( GetPathName(), false ) )
	{
		OnCloseDocument();
		pManifest->SetActiveMapDoc( pManifest );
		pManifest->ActivateMapDoc( pManifest );
		ToolManager()->SetTool(	TOOL_POINTER );
		GetMainWnd()->GlobalNotify( WM_MAPDOC_CHANGED );
		pManifest->UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
		AfxMessageBox( "Manifest was successfully created and has automatically been saved.", MB_OK );
	}
	else
	{
		pManifest->OnCloseDocument();
		AfxMessageBox( "Manifest was NOT created!", MB_OK );
		return false;
	}

	return true;
}


int CMapDoc::GetClipboardCount( void )
{
	return GetHammerClipboard()->Objects.Count();
}


//-----------------------------------------------------------------------------
// Purpose: this does a special paste for manifest maps
// Input  : pDestWorld
//			vecOffset
//			vecRotate
// 			pParent
//			bMakeEntityNamesUnique
//			pszEntityNamePrefix
//-----------------------------------------------------------------------------
void CMapDoc::ManifestPaste( CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix )
{
	Paste( GetHammerClipboard()->Objects, GetHammerClipboard()->pSourceWorld, pDestWorld, vecOffset, vecRotate, pParent, bMakeEntityNamesUnique, pszEntityNamePrefix );
	GetHammerClipboard()->Objects.PurgeAndDeleteElements();
	SetModifiedFlag( true );
}


//-----------------------------------------------------------------------------
// Purpose: this handles the routing of letting instances know their map may have been updated
// Input  : pInstanceMapDoc - the map that was updated
//-----------------------------------------------------------------------------
void CMapDoc::UpdateInstanceMap( CMapDoc *pInstanceMapDoc )
{
	bool		bUpdated = false;

	if ( m_bCollapsingInstances )
	{
		return;
	}

	const CMapObjectList *pChildren = m_pWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass	*pChild = pChildren->Element( pos );
		CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pChild );

		if ( pEntity && stricmp( pEntity->GetClassName(), "func_instance" ) == 0 )
		{
			CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
			if ( pMapInstance )
			{
				if ( pMapInstance->GetInstancedMap() == pInstanceMapDoc )
				{
					pMapInstance->UpdateInstanceMap();
					bUpdated = true;
				}
			}
		}
	}

	if ( bUpdated )
	{
		APP()->pMapDocTemplate->UpdateInstanceMap( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function will collapse instances into the map ( and any children instances )
//-----------------------------------------------------------------------------
void CMapDoc::CollapseInstances( bool bOnlySelected )
{
	int		InstanceCount = 0;

	if ( AfxMessageBox( "Collapsing does not perform all of the operations that the BSP process does for instancing.  There may be some issues or differences between results.  Are you sure you want to do this?", MB_YESNO | MB_ICONQUESTION ) == IDNO )
	{
		return;
	}

	m_bCollapsingInstances = true;

	const CMapObjectList *pChildren = m_pWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass	*pChild = pChildren->Element( pos );
		CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pChild );

		if ( pEntity && stricmp( pEntity->GetClassName(), "func_instance" ) == 0 && ( !bOnlySelected || pEntity->GetSelectionState() != SELECT_NONE ) )
		{
			CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
			if ( pMapInstance )
			{
				if ( pMapInstance->GetInstancedMap() )
				{
					char	temp[ 256 ];
					Vector	origin;
					QAngle	angles;

					pMapInstance->GetInstancedMap()->OnEditSelectall();
					pMapInstance->GetInstancedMap()->Copy();
					pMapInstance->GetInstancedMap()->OnEditClearselection();
					SetActiveMapDoc( this ); // just in case the last instance copy forces the map to close, we need to make ourselves active again

					InstanceCount++;
					sprintf( temp, "AutoInstance%d-", InstanceCount );

					pEntity->GetOrigin( origin );
					pEntity->GetAngles( angles );

					PasteInstance( GetHammerClipboard()->Objects, pMapInstance->GetInstancedMap()->GetMapWorld(), GetMapWorld(), origin, angles, NULL, true, temp );
					Update();
				}

				DeleteObject( pEntity );
				m_UpdateList.RemoveAll();
				SetActiveMapDoc( this ); // just in case the last instance copy forces the map to close, we need to make ourselves active again

				pos = -1;		// start over so that we find any instances within instances
			}
		}
	}

	m_bCollapsingInstances = false;
	APP()->pMapDocTemplate->UpdateInstanceMap( this );
	APP()->pManifestDocTemplate->UpdateInstanceMap( this );

	SetModifiedFlag( true );
	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );

	char temp[ 256 ];
	sprintf( temp, "A total of %d instances were collapsed into the main map.", InstanceCount );

	AfxMessageBox( temp, MB_OK | MB_ICONEXCLAMATION );
}


//-----------------------------------------------------------------------------
// Purpose: this function will allow you to iterate through a string looking for $variables
// Input  : Text - the string to search through
//			StartPos - the starting index in the string
// Output : Result - returns the $variable if it exists
//			returns the string index after the variable if one is found, otherwise -1 if not found
//-----------------------------------------------------------------------------
int FindInstanceParm( char *Text, int StartPos, CString &Result )
{
	char	*found;

	found = strchr( Text + StartPos, '$' );
	if ( found == NULL )
	{
		return -1;
	}

	StartPos = found - Text;

	// Grab the $
	Result += Text[ StartPos ];
	StartPos++;

	while( Text[ StartPos ] )
	{
		if ( V_isalnum ( Text[ StartPos ] ) == 0 && Text[ StartPos ] != '_' )
		{
			break;
		}

		Result += Text[ StartPos ];
		StartPos++;
	}

	return StartPos;
}


void CMapDoc::PopulateInstanceParms_r( CMapEntity *pEntity, const CMapObjectList *pChildren, CUtlVector< CString > &ParmList )
{
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass *pChild = pChildren->Element( pos );
		CMapEntity *pInstanceEntity = dynamic_cast< CMapEntity * >( pChild );

		if ( pInstanceEntity && pInstanceEntity != pEntity )
		{
			for ( int i = pInstanceEntity->GetFirstKeyValue(); i != pInstanceEntity->GetInvalidKeyValue(); i = pInstanceEntity->GetNextKeyValue( i ) )
			{
				LPCTSTR	pValue = pInstanceEntity->GetKeyValue( i );

				int			StartPos = 0;
				CString		Result;

				while( ( StartPos = FindInstanceParm( (char * )pValue, StartPos, Result ) ) != -1 )
				{
					if ( ParmList.Find( Result ) == -1 )
					{
						ParmList.AddToTail( Result );
					}
				}
			}

			int nCount = pInstanceEntity->Connections_GetCount();
			for ( int j = 0; j < nCount; ++j )
			{
				CEntityConnection *pConn = pInstanceEntity->Connections_Get( j );

				const char *pValue = pConn->GetTargetName();

				int			StartPos = 0;
				CString		Result;

				while( ( StartPos = FindInstanceParm( (char * )pValue, StartPos, Result ) ) != -1 )
				{
					if ( ParmList.Find( Result ) == -1 )
					{
						ParmList.AddToTail( Result );
					}
				}
			}
		}

		if ( pInstanceEntity != pEntity )
		{
			PopulateInstanceParms_r( pEntity, pChild->GetChildren(), ParmList );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: this function will iterate through all entities of the current map looking for $variables.
//			any ones that are found will be populated as Parm key values.
// Input  : pEntity - the entity to populate
//-----------------------------------------------------------------------------
void CMapDoc::PopulateInstanceParms( CMapEntity *pEntity )
{
	CUtlVector< CString > ParmList;

	PopulateInstanceParms_r( pEntity, m_pWorld->GetChildren(), ParmList );

	for( int i = 0; i < ParmList.Count(); i++ )
	{
		bool	bFound = false;
		for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
		{
			LPCTSTR	pValue = pEntity->GetKeyValue( j );
			if ( strnicmp( pValue, ParmList[ i ], strlen( ParmList[ i ] ) ) == 0 )
			{
				bFound = true;
				break;
			}
		}

		if ( bFound == false )
		{
			int j = 1;
			while( 1 )
			{
				char tempKey[ 128 ];

				sprintf( tempKey, "parm%d", j );
				if ( pEntity->GetKeyValue( tempKey ) == NULL )
				{
					char	tempValue[ MAX_KEYVALUE_LEN ];

					sprintf( tempValue, "%s string", (const char*)ParmList[ i ] );
					pEntity->SetKeyValue( tempKey, tempValue );
					break;
				}

				j++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will look through the instance for a func_instance_parms.  If one is 
//			found, then it will populate the replace fields with the parm fields.
// Input  : pEntity - the func_instance
//-----------------------------------------------------------------------------
void CMapDoc::PopulateInstance( CMapEntity *pEntity )
{
	CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );

	if ( pMapInstance == NULL || pMapInstance->GetInstancedMap() == NULL )
	{
		return;
	}

	CMapEntityList entityList;

	pMapInstance->GetInstancedMap()->FindEntitiesByClassName( entityList, "func_instance_parms", false );

	if ( entityList.Count() != 1 )
	{
		return;
	}

	CMapEntity *pInstanceParmsEntity = entityList.Element( 0 );

	for ( int i = pInstanceParmsEntity->GetFirstKeyValue(); i != pInstanceParmsEntity->GetInvalidKeyValue(); i = pInstanceParmsEntity->GetNextKeyValue( i ) )
	{
		LPCTSTR	pKey = pInstanceParmsEntity->GetKey( i );
		LPCTSTR	pValue = pInstanceParmsEntity->GetKeyValue( i );

		if ( strnicmp( pKey, "parm", strlen( "parm" ) ) == 0 )
		{
			const char *pos = strchr( pValue, ' ' );
			if ( pos == NULL )
			{
				continue;
			}

			int		len = pos - pValue;
			bool	bFound = false;

			for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
			{
				LPCTSTR	pInstanceKey = pEntity->GetKey( j );
				LPCTSTR	pInstanceValue = pEntity->GetKeyValue( j );
				if ( strnicmp( pInstanceKey, "replace", strlen( "replace" ) ) == 0 &&
					 strnicmp( pInstanceValue, pValue, len ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			if ( bFound == false )
			{
				int j = 1;
				while( 1 )
				{
					char tempKey[ MAX_KEYVALUE_LEN ];

					sprintf( tempKey, "replace%02d", j );
					if ( pEntity->GetKeyValue( tempKey ) == NULL )
					{
						char	tempValue[ MAX_KEYVALUE_LEN ];

						strcpy( tempValue, pValue );
						strcpy( &tempValue[ len ], " ???" );

						pEntity->SetKeyValue( tempKey, tempValue );
						break;
					}

					j++;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will notify all children that their owning instance has been moved.
//			currently not used.
//-----------------------------------------------------------------------------
void CMapDoc::InstanceMoved( void )
{
#if 0
	const CMapObjectList *pChildren = m_pWorld->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass *pChild = pChildren->Element( pos );

		pChild->InstanceMoved();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: this function will return the current primary map document of the
//			manifest.  Generally this is the map layer that is editable where all
//			entity / brush operations will take place.
// Output : the primary map document
//-----------------------------------------------------------------------------
CMapWorld *CMapDoc::GetCurrentWorld( void )
{
	if ( m_pManifest )
	{
		return m_pManifest->GetActiveMapDoc()->m_pWorld;
	}
	else
	{
		return m_pWorld;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapDoc::SelectDocType(void)
{
	// dvs: Disabled for single-config running.
	// if no game configs are set up, we must set them up
	//if (Options.configs.nConfigs == 0)
	//{
	//	if (AfxMessageBox(IDS_NO_CONFIGS_AVAILABLE, MB_YESNO) == IDYES)
	//	{
	//		APP()->OpenURL(ID_HELP_FIRST_TIME_SETUP, GetMainWnd()->GetSafeHwnd());
	//	}
	//
	//	COptionProperties dlg("Configure Hammer", NULL, 0);
	//	dlg.DoModal();
	//	if (Options.configs.nConfigs == 0)
	//	{
	//		return FALSE;
	//	}
	//}
	//
	//
	// Prompt the user to select a game configuration.
	//
	//CGameConfig *pGame = APP()->PromptForGameConfig();
	//if (!pGame)
	//{
	//	return FALSE;
	//}

	CGameConfig *pGame = g_pGameConfig;

	//
	// Try to find some textures that this game can use.
	//
	if (!g_Textures.HasTexturesForConfig(pGame))
	{
		AfxMessageBox(IDS_NO_TEXTURES_AVAILABLE);

		COptionProperties dlg("Configure Hammer", NULL, 0);
		dlg.DoModal();

		if (!g_Textures.HasTexturesForConfig(pGame))
		{
			return FALSE;
		}
	}

	m_pGame = pGame;

	if (GetActiveMapDoc() != this)
	{
		SetActiveMapDoc(this);
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: set up this document to edit a prefab data .. when the object is saved,
//			save it back to the library instead of to a file.
// Input  : dwPrefabID - 
//-----------------------------------------------------------------------------
void CMapDoc::EditPrefab3D(DWORD dwPrefabID)
{
	CPrefab3D *pPrefab = (CPrefab3D *)CPrefab::FindID(dwPrefabID);
	Assert(pPrefab);

	// set up local variables
	m_dwPrefabID = dwPrefabID;
	m_dwPrefabLibraryID = pPrefab->GetLibraryID();
	m_bEditingPrefab = TRUE;

	SetPathName(pPrefab->GetName(), FALSE);
	SetTitle(pPrefab->GetName());
	
	// copy prefab data to world
	if (!pPrefab->IsLoaded())
	{
		pPrefab->Load();
	}

	//
	// Copying into world, so we update the object dependencies to insure
	// that any object references in the prefab get resolved.
	//
	m_pWorld->CopyFrom(pPrefab->GetWorld(), false);
	m_pWorld->CopyChildrenFrom(pPrefab->GetWorld(), false);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
//			bRMF - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapDoc::Serialize(std::fstream& file, BOOL fIsStoring, BOOL bRMF)
{
	SetActiveMapDoc(this);

	// check for editing prefab
	if(m_bEditingPrefab)
	{
		// save prefab in library
		CPrefabLibrary *pLibrary = CPrefabLibrary::FindID(m_dwPrefabLibraryID);
		if(!pLibrary)
		{
			static int id = 1;

			AfxMessageBox("The library this prefab object belongs to has been\n"
				"deleted. This document will now behave as a regular file\n"
				"document.");
			m_bEditingPrefab = FALSE;
			
			CString str;
			str.Format("Prefab%d.rmf", id++);
			SetPathName(str);
			return 1;
		}

		CPrefab3D *pPrefab = (CPrefab3D *)CPrefab::FindID(m_dwPrefabID);
		if (!pPrefab)
		{
			// Not found, create a new prefab.
			pPrefab = new CPrefabRMF;
		}

		pPrefab->SetWorld(m_pWorld);
		m_pWorld = NULL;

		pLibrary->Add(pPrefab);
		pLibrary->Save();

		return 1;
	}

	GetHistory()->Pause();

	if(bRMF)
	{
		if (m_pWorld->SerializeRMF(file, fIsStoring) < 0)
		{
			AfxMessageBox("There was a file error.", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}

		Camera3D *pCamTool = dynamic_cast<Camera3D*>(m_pToolManager->GetToolForID(TOOL_CAMERA));

		if ( pCamTool )
		{
			char sig[8] = "DOCINFO";

			if(fIsStoring)
			{
				file.write(sig, sizeof sig);
				
				pCamTool->SerializeRMF(file, fIsStoring);
			}
			else
			{
				char buf[sizeof sig];
				memset(buf, 0, sizeof buf);
				file.read(buf, sizeof buf);
				if(memcmp(buf, sig, sizeof sig))
					goto Done;

				pCamTool->SerializeRMF(file, fIsStoring);
			}
		}

Done:;
	}
	else
	{
		CMapObjectList CordonList;
		CMapWorld *pCordonWorld = NULL;
		BoundBox CordonBox(m_vCordonMins, m_vCordonMaxs);

		if ( m_bIsCordoning )
		{
			//
			// Create "cordon world", add its objects to our real world, create a list in
			// CordonList so we can remove them again.
			//
			pCordonWorld = CordonCreateWorld();
			
			const CMapObjectList *pChildren = pCordonWorld->GetChildren();
			FOR_EACH_OBJ( *pChildren, pos )
			{
				CMapClass *pChild = pChildren->Element(pos);
				pChild->SetTemporary(TRUE);
				m_pWorld->AddObjectToWorld(pChild);

				CordonList.AddToTail(pChild);
			}

			//
			// HACK: (not mine) - make the cordon bounds bigger so that the cordon brushes
			// overlap the cordon bounds during serialization.
			CordonBox.bmins -= Vector(1,1,1);
			CordonBox.bmaxs += Vector(1,1,1);
		}

		if (fIsStoring)
		{
			void SetMapFormat(MAPFORMAT mf);
			SetMapFormat(m_pGame->mapformat);
		}

		if (m_pWorld->SerializeMAP(file, fIsStoring, m_bIsCordoning? &CordonBox : NULL) < 0)
		{
			AfxMessageBox("There was a file error.", MB_OK | MB_ICONEXCLAMATION);
			return(FALSE);
		}

		//
		// Remove cordon objects.
		//
		if ( m_bIsCordoning )
		{
			FOR_EACH_OBJ( CordonList, pos )
			{
				CMapClass *pobj = CordonList.Element(pos);
				m_pWorld->RemoveChild(pobj);
			}
			delete pCordonWorld;
		}
	}

	GetHistory()->Resume();

	if (!fIsStoring)
	{
		UpdateVisibilityAll();
		GetMainWnd()->GlobalNotify(WM_MAPDOC_CHANGED);
	}

	return TRUE;
}


#ifdef _DEBUG
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::AssertValid(void) const
{
	CDocument::AssertValid();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dc - 
//-----------------------------------------------------------------------------
void CMapDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG



//-----------------------------------------------------------------------------
// Purpose: Frees all dynamically allocated memory from this document.
//-----------------------------------------------------------------------------
void CMapDoc::DeleteContents(void)
{
	m_NotifyList.RemoveAll();

	//
	// Don't leave pointers to deleted worlds lying around!
	//
	if (GetHammerClipboard()->pSourceWorld == m_pWorld)
	{
		GetHammerClipboard()->pSourceWorld = NULL;
	}

	if ( m_VisGroups )
	{
		m_VisGroups->PurgeAndDeleteElements();
//		delete m_VisGroups;
//		m_VisGroups = NULL;
	}

	if ( m_RootVisGroups )
	{
		m_RootVisGroups->RemoveAll();
//		delete m_RootVisGroups;
//		m_RootVisGroups = NULL;
	}

	if ( m_pManifestOwner == NULL && m_pSelection )
	{
		m_pSelection->RemoveAll();
//		delete m_pSelection;
//		m_pSelection = NULL;
	}

	if ( m_pWorld )
	{
		delete m_pWorld;
		m_pWorld = NULL;
	}
	
	GetMainWnd()->m_pFaceEditSheet->ClearFaceListByMapDoc( this );

	CDocument::DeleteContents();

	CMainFrame *pwndMain = GetMainWnd();
	if (pwndMain != NULL)
	{
		pwndMain->OnDeleteActiveDocument();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
// Output : Returns a pointer to the visgroup with the given ID, NULL if none.
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::VisGroups_GroupForID(DWORD id)
{
	int nCount = m_VisGroups->Count();
	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pGroup = m_VisGroups->Element(i);
		if (pGroup->GetID() == id)
		{
			return(pGroup);
		}
	}
	return(NULL);
}

CVisGroup *CMapDoc::VisGroups_GroupForName( const char *pszName, bool bIsAuto )
{
	int nCount = m_VisGroups->Count();
	for ( int i = 0; i < nCount; i++ )
	{
		CVisGroup *pGroup = m_VisGroups->Element(i);
		if ( !Q_stricmp( pGroup->GetName(), pszName ) && ( pGroup->IsAutoVisGroup() == bIsAuto )  )
		{
			return pGroup;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Prompts the user through the process of hiding a set of objects.
// Returns true if the objects were hidden.
//-----------------------------------------------------------------------------
void CMapDoc::ShowNewVisGroupsDialog(CMapObjectList &Objects, bool bUnselectObjects)
{
	int nCount = Objects.Count();
	if (!nCount)
	{
		return;
	}

	//
	// Let the user input a name for the new visgroup.
	//
	CString str;
	str.Format("%d object%s", nCount, nCount == 1 ? "" : "s");
	CNewVisGroupDlg dlg(str);
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}

	//
	// Create the visgroup (or use the one they picked).
	//
	CVisGroup *pVisGroup = dlg.GetPickedVisGroup();
	if (!pVisGroup)
	{
		dlg.GetName(str);
		pVisGroup = VisGroups_AddGroup(str);
	}

	VisGroups_AddObjectsToVisGroup(Objects, pVisGroup, dlg.GetHideObjectsOption(), dlg.GetRemoveFromOtherGroups());

	if ( bUnselectObjects && dlg.GetHideObjectsOption() )
	{
		// We don't want hidden objects still selected, so clear the selection.
		m_pSelection->SelectObject( NULL, scClear );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new visgroup with the given objects in it.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_CreateNamedVisGroup(CMapObjectList &Objects, const char *szName, bool bHide, bool bRemoveFromOtherVisGroups)
{
	CVisGroup *pVisGroup = VisGroups_AddGroup(szName);
	VisGroups_AddObjectsToVisGroup(Objects, pVisGroup, bHide, bRemoveFromOtherVisGroups);
}


//-----------------------------------------------------------------------------
// Purpose: Adds the objects to the given visgroup and does hiding as specified.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_AddObjectsToVisGroup(CMapObjectList &Objects, CVisGroup *pVisGroup, bool bHide, bool bRemoveFromOtherVisGroups)
{
	//
	// Assign the objects to it.
	//

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pObject = Objects.Element(pos);
		if (VisGroups_ObjectCanBelongToVisGroup(pObject))
		{
			if (bRemoveFromOtherVisGroups)
			{
				pObject->RemoveAllVisGroups();
			}
			
			pObject->AddVisGroup(pVisGroup);
		}
	}

	if ( m_bVisGroupUpdatesLocked )
		return;

	//
	// Clean up any visgroups with no members.
	//
	VisGroups_PurgeGroups();

	//
	// Update object visiblity and refresh views.
	//
	if ( bHide )
	{
		VisGroups_ShowVisGroup(pVisGroup, !bHide);
	}
	else
	{
		//this is currently inside of ShowVisGroup
		//needs called even if we are not showing a group
		VisGroups_UpdateAll();
		UpdateVisibilityAll();
		SetModifiedFlag();
	}

	CMainFrame *pwndMain = GetMainWnd();
	if (pwndMain)
	{
		pwndMain->UpdateAllDocViews(MAPVIEW_UPDATE_VISGROUP_ALL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not this object is eligible for inclusion in a visgroup.
//-----------------------------------------------------------------------------
bool CMapDoc::VisGroups_ObjectCanBelongToVisGroup(CMapClass *pObject)
{
	if (!pObject)
		return false;

	CMapClass *pParent = pObject->GetParent();
	if (pParent)
	{
		if ( IsWorldObject(pParent) )
			return true;

		if (pParent->IsGroup())
			return true;

		// Children of entities cannot belong to visgroups independent of their parent.
		Assert(dynamic_cast <CMapEntity *>(pParent));
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pVisGroup - 
//			pParent - 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_SetParent(CVisGroup *pVisGroup, CVisGroup *pNewParent)
{
	// Can't make a group a child of one of its descendents.
	Assert(!pVisGroup->FindDescendent(pNewParent));

	CVisGroup *pOldParent = pVisGroup->GetParent();
	if (pOldParent != pNewParent)
	{
		if (pOldParent)
		{
			pOldParent->RemoveChild(pVisGroup);
		}
		else
		{
			int nIndex = m_RootVisGroups->Find(pVisGroup);
			if (nIndex != -1)
			{
				 m_RootVisGroups->Remove(nIndex);
			}
		}

		if (pNewParent)
		{
			pNewParent->AddChild(pVisGroup);
		}
		else
		{
			m_RootVisGroups->AddToTail(pVisGroup);
		}

		pVisGroup->SetParent(pNewParent);
	}
}		


//-----------------------------------------------------------------------------
// Purpose: Update the visgroup visibility state for all groups that
//			this child belongs to.
//
//			NOTE: Assumes that all visgroups were initialized to VISGROUP_UNDEFINED
//				  before calling this with the first object.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_UpdateForObject(CMapClass *pObject)
{
	//Msg("Object: 0x%X is ", pObject);
	int nVisGroupCount = pObject->GetVisGroupCount();
	for (int i = 0; i < nVisGroupCount; i++)
	{
		CVisGroup *pGroup = pObject->GetVisGroup(i);
		VisGroupState_t eVisState = pGroup->GetVisible();

		if (eVisState != VISGROUP_PARTIAL)
		{
			if (pObject->IsVisGroupShown())
			{
				//if (i == 0)
				//	Msg("shown\n");

				if (eVisState == VISGROUP_HIDDEN)
				{
					//Msg("    Visgroup %s was hidden, now partial\n", pGroup->GetName());
					pGroup->SetVisible(VISGROUP_PARTIAL);
				}
				else
				{
					//Msg("    Visgroup %s is shown\n", pGroup->GetName());
					pGroup->SetVisible(VISGROUP_SHOWN);
				}
			}
			else
			{
				//if (i == 0)
				//	Msg("hidden\n");

				if (eVisState == VISGROUP_SHOWN)
				{
					//Msg("    Visgroup %s was shown, now partial\n", pGroup->GetName());
					pGroup->SetVisible(VISGROUP_PARTIAL);
				}
				else
				{
					//Msg("    Visgroup %s is hidden\n", pGroup->GetName());
					pGroup->SetVisible(VISGROUP_HIDDEN);
				}
			}
		}
		//else
		//{
		//	Msg("    Visgroup %s is partial\n", pGroup->GetName());
		//}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_UpdateParents(void)
{
	int nVisGroupCount = VisGroups_GetCount();
	for (int i = 0; i < nVisGroupCount; i++)
	{
		CVisGroup *pTempGroup = VisGroups_GetVisGroup(i);
		if ( pTempGroup->GetVisible() != VISGROUP_UNDEFINED && pTempGroup->GetParent() != NULL )
		{
			pTempGroup->VisGroups_UpdateParent( pTempGroup->GetVisible() );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_UpdateAll(void)
{
	//Msg("=======  Visgroups_UpdateAll ========\n");

	//
	// Mark all visgroups as having an undefined state so we
	// can update the visibility state of all visgroups while we
	// hide and show the member objects.
	//
	int nVisGroupCount = VisGroups_GetCount();
	for (int i = 0; i < nVisGroupCount; i++)
	{
		CVisGroup *pTempGroup = VisGroups_GetVisGroup(i);
		pTempGroup->SetVisible(VISGROUP_UNDEFINED);
	}

	//
	// Show or hide all the objects that belong to the given visgroup.
	//
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild)
	{
		//
		// Update the visgroup visibility state for all groups that
		// this child belongs to.
		//
		VisGroups_UpdateForObject(pChild);
		pChild = m_pWorld->GetNextDescendent(pos);
	}

	// Update parent state
	VisGroups_UpdateParents();

	//
	// Look for visgroups still set as undefined -- these are empty.
	//
	for (int i = nVisGroupCount - 1; i >= 0; i--)
	{
		CVisGroup *pTempGroup = VisGroups_GetVisGroup(i);
		Assert(pTempGroup->GetVisible() != VISGROUP_UNDEFINED);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether the object belongs to the given visgroup or any
//			of that visgroup's children.
//-----------------------------------------------------------------------------
static bool IsInVisGroupRecursive(CMapClass *pObject, CVisGroup *pGroup)
{
	if (pObject->IsInVisGroup(pGroup))
	{
		return true;
	}

	int nChildCount = pGroup->GetChildCount();
	if (nChildCount > 0)
	{
		for (int i = 0; i < nChildCount; i++)
		{
			CVisGroup *pChild = pGroup->GetChild(i);
			if (IsInVisGroupRecursive(pObject, pChild))
			{
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Hides or shows the given visgroup.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_ShowVisGroup(CVisGroup *pGroup, bool bShow)
{
	//Msg("--------  Visgroups_ShowVisGroup --------\n");

	if (pGroup == NULL)
		return;

	VisGroupSelection eVisGroupType = USER;
	if ( pGroup->IsAutoVisGroup() )
	{
		eVisGroupType = AUTO;
	}

	//
	// Show or hide all the objects that belong to the given visgroup.
	//
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild)
	{
		if (IsInVisGroupRecursive(pChild, pGroup))
		{
			pChild->VisGroupShow(bShow, eVisGroupType);
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}

	VisGroups_UpdateAll();
	
	UpdateVisibilityAll();
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapDoc::SaveModified(void)
{
	if (!IsModified())
		return TRUE;        // ok to continue

	// editing prefab and modified - update data?
	if(m_bEditingPrefab)
	{
		switch(AfxMessageBox("Do you want to save the changes to this prefab object?", MB_YESNOCANCEL))
		{
		case IDYES:
			{
			std::fstream file;
			Serialize(file, 0, 0);
			return TRUE;
			}
		case IDNO:
			return TRUE;	// no save
		case IDCANCEL:
			return FALSE;	// forget this cmd
		}
	}

	return CDocument::SaveModified();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpszPathName - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	Initialize();

	if (!SelectDocType())
	{
		return FALSE;
	}

	//
	// Look for either the RMF or MAP extension to indicate an old file format.
	//
	BOOL bRMF = FALSE;
	BOOL bMAP = FALSE;

	if (!stricmp(lpszPathName + strlen(lpszPathName) - 3, "rmf"))
	{
		bRMF = TRUE;
	}
	else if (!stricmp(lpszPathName + strlen(lpszPathName) - 3, "map"))
	{
		bMAP = TRUE;
	}

	//
	// Call any per-class PreloadWorld functions here.
	//
	CMapSolid::PreloadWorld();

	if ((bRMF) || (bMAP))
	{
		std::fstream file(lpszPathName, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			return(FALSE);
		}

		if (!Serialize(file, FALSE, bRMF))
		{
			return(FALSE);
		}
	}
	else
	{
		if (!LoadVMF(lpszPathName))
		{
			return(FALSE);
		}
	}

	SetModifiedFlag(FALSE);
	Msg(mwStatus, "Opened %s", lpszPathName);
	SetActiveMapDoc(this);

	//
	// We set the active doc before loading for displacements (and maybe other
	// things), but visgroups aren't available until after map load. We have to refresh
	// the visgroups here or they won't be correct.
	//
	GetMainWnd()->GlobalNotify(WM_MAPDOC_CHANGED);

	m_pToolManager->SetTool( TOOL_POINTER );

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Called when the document is closed.
//-----------------------------------------------------------------------------
void CMapDoc::OnCloseDocument(void)
{
	if ( m_nExternalReferenceCount > 0 )
	{	// this is an instance, so hide the window
		ShowWindow( false );

		if ( IsModified() )
		{
			DeleteContents();
			OnOpenDocument( m_strPathName );
		}

		return;
	}

	//
	// Deactivate the current tool now because doing it later can cause side-effects.
	//
	m_pToolManager->Shutdown();

	//
	// Call DeleteContents ourselves because in the framework implementation
	// of OnCloseDocument the doc window is closed first, which activates the
	// document beneath us. This is bad because we must be the active document
	// during the close process for things like displacements to clean themselves
	// up properly.
	//
	SetActiveMapDoc(this);

	CDocument::OnCloseDocument();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpszPathName - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	if( m_pBSPLighting )
		m_pBSPLighting->Serialize();

	// UNDONE: prefab serialization must be redone
	if (m_bEditingPrefab)
	{
		std::fstream file;
		Serialize(file, 0, 0);
		SetModifiedFlag(FALSE);
		OnCloseDocument();
		return(TRUE);
	}

	//
	// If a file with the same name exists, back it up before saving the new one.
	//
	char szFile[MAX_PATH];
	strcpy(szFile, lpszPathName);
	szFile[strlen(szFile) - 1] = 'x';

	if (access(lpszPathName, 0) != -1)
	{
		if (!CopyFile(lpszPathName, szFile, FALSE))
		{
			DWORD dwError = GetLastError();

			char szError[_MAX_PATH];
			wsprintf(szError, "Hammer was unable to backup the existing file \"%s\" (Error: 0x%lX). Please verify that the there is space on the hard drive and that the path still exists.", lpszPathName, dwError);
			AfxMessageBox(szError);
			return(FALSE);
		}
	}

	//
	// Use the file extension to determine how to save the file.
	//
	BOOL bRMF = FALSE;
	BOOL bMAP = FALSE;
	if (!stricmp(lpszPathName + strlen(lpszPathName) - 3, "rmf"))
	{
		bRMF = TRUE;
	}
	else if (!stricmp(lpszPathName + strlen(lpszPathName) - 3, "map"))
	{
		bMAP = TRUE;
	}

	//
	// HalfLife 2 and beyond use heirarchical chunk files.
	//
	if ((m_pGame->mapformat == mfHalfLife2) && (!bRMF) && (!bMAP))
	{
		BOOL bSaved = FALSE;

		BeginWaitCursor();
		if (SaveVMF(lpszPathName, 0))
		{
			bSaved = TRUE;
			SetModifiedFlag(FALSE);
		}
		EndWaitCursor();

		return(bSaved);
	}

	//
	// Half-Life used RMFs and MAPs.
	//
	std::fstream file(lpszPathName, std::ios::out | std::ios::binary);
	if (!file.is_open())
	{
		char szError[_MAX_PATH];
		wsprintf(szError, "Hammer was unable to open the file \"%s\" for writing. Please verify that the file is writable and that the path exists.", lpszPathName);
		AfxMessageBox(szError);
		return(FALSE);
	}

	BeginWaitCursor();
	if (!Serialize(file, TRUE, bRMF))
	{
		EndWaitCursor();
		return(FALSE);
	}
	EndWaitCursor();

	SetModifiedFlag(FALSE);
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : st2 - 
//			st1 - 
// Output : DWORD
//-----------------------------------------------------------------------------
DWORD SubTime(SYSTEMTIME& st2, SYSTEMTIME& st1)
{
	DWORD dwMil = 0;

	if(st2.wMinute != st1.wMinute)
	{
		dwMil += (59 - st1.wSecond) * 1000;
	}
	if(st2.wSecond != st1.wSecond)
	{
		dwMil += 1000 - st1.wMilliseconds;
	}

	if(!dwMil)
	{
		dwMil = st2.wMilliseconds - st1.wMilliseconds;
	}
	else
		dwMil += st2.wMilliseconds;

	return dwMil;
}

void CMapDoc::RenderDocument(CRender *pRender)
{
	if ( m_bIsCordoning )
	{
		pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
		pRender->SetDrawColor( Color(255,0,0,255) );
		pRender->DrawBox( m_vCordonMins, m_vCordonMaxs, false );
		pRender->PopRenderMode();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces a render of all the 3D views. Called from OnIdle to render
//			the 3D views.
//-----------------------------------------------------------------------------
void CMapDoc::RenderAllViews(void)
{
	//
	// Make sure the document is up to date.
	//
	Update();

	bool bViewRendered = false;

	POSITION p = GetFirstViewPosition();
	while (p)
	{
		CMapView *pView = dynamic_cast<CMapView*>(GetNextView(p));

		if ( !pView )
			continue;

		if ( pView->IsActive() )
		{
			pView->ProcessInput();
		}

		if ( pView->ShouldRender() )
		{
			pView->RenderView();
			bViewRendered = true;
		}
	} 

	if ( !bViewRendered )
	{
		// not a single view did update this frame
		// so the application seems to be idle
		Sleep( 1 ); 
	}
	else
	{
		UpdateStatusbar();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::UpdateAllCameras(const Vector *vecViewPos, const Vector *vecLookAt, const float *fZoom)
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CMapView *pView = dynamic_cast<CMapView*>(GetNextView(pos));
		CCamera *pCamera = pView->GetCamera();

		if ( vecViewPos )
		{
			pCamera->SetViewPoint( *vecViewPos );
		}

		if ( vecLookAt && !pCamera->IsOrthographic() )
		{
			pCamera->SetViewTarget( *vecLookAt );
		}

		if ( fZoom && pCamera->IsOrthographic() )
		{
			pCamera->SetZoom( *fZoom );
		}

		pView->UpdateView( MAPVIEW_OPTIONS_CHANGED );
	}
}

// walk through all views
void CMapDoc::UpdateAllViews(int nFlags, UpdateBox *ub )
{
	POSITION p = GetFirstViewPosition();

	while (p)
	{
		CMapView *pView = dynamic_cast<CMapView*>(GetNextView(p));

		if ( pView )
		{
			pView->UpdateView( nFlags );
		}
	}	
}

//-----------------------------------------------------------------------------
// Purpose: used during iteration, tells an map entity to 
//-----------------------------------------------------------------------------
static BOOL _UpdateAnimation( CMapClass *mapClass, float animTime )
{
	mapClass->UpdateAnimation( animTime );
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Sets up for drawing animated objects
//			Needs to be called each frame before any animating object are rendered
//-----------------------------------------------------------------------------
void CMapDoc::UpdateAnimation( void )
{
	//GetMainWnd()->m_AnimationDlg.RunFrame();

	// check to see if the animation needs to be updated
	if ( !IsAnimating() )
		return;

	// if the animation time is 0, turn it off
	if ( GetAnimationTime() == 0.0f )
	{
		m_bIsAnimating = false;
	}

	// get current animation time from animation toolbar
	union {
		float fl;
		DWORD dw;
	} animTime;
	
	animTime.fl = GetAnimationTime();

	// iterate through all CMapEntity object and update their animation frame matrix
	m_pWorld->EnumChildren( ENUMMAPCHILDRENPROC(_UpdateAnimation), animTime.dw, MAPCLASS_TYPE(CMapAnimator) );

}


//-----------------------------------------------------------------------------
// Purpose: Sets the current time in the animation
// Input  : time - a time, from 0 to 1
//-----------------------------------------------------------------------------
void CMapDoc::SetAnimationTime( float time )
{
	m_flAnimationTime = time;

	if ( m_flAnimationTime != 0.0f )
	{
		m_bIsAnimating = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the current time and stores it in the doc, for use during the frame
//-----------------------------------------------------------------------------
void CMapDoc::UpdateCurrentTime( void )
{
	m_flCurrentTime = (float)timeGetTime() / 1000;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static BOOL SelectInBox(CMapClass *pObject, SelectBoxInfo_t *pInfo)
{
	//
	// Skip hidden objects.
	//
	if (!pObject->IsVisible())
	{
		return TRUE;
	}

	//
	// Skip anything with children. We only are interested in leaf objects because
	// PrepareSelection will call up to tree to get the proper ancestor.
	//
	if (pObject->GetChildCount())
	{
		return TRUE;
	}

	//
	// Skip groups. Groups are selected via their members through PrepareSelection.
	//
	if (pObject->IsGroup())
	{
		// Shouldn't ever have empty groups lying around!
		Assert(false);
		return TRUE;
	}

	//
	// Skip clutter helpers.
	//
	if (pObject->IsClutter())
	{
		return TRUE;
	}

	// FIXME: We're calling PrepareSelection on nearly everything in the world,
	// then doing the box test against the object that we get back from that!
	// We should use the octree to cull out most of the world up front.
	CMapClass *pSelObject = pObject->PrepareSelection(pInfo->eSelectMode);
	if (pSelObject)
	{
		if (Options.view2d.bSelectbyhandles)
		{
			Vector ptCenter;
			pObject->GetBoundsCenter(ptCenter);
			if (pInfo->pBox->ContainsPoint(ptCenter))
			{
				pInfo->pDoc->SelectObject(pSelObject, scSelect);
			}

			return TRUE;
		}

		bool bSelect;
		if (pInfo->bInside)
		{
			bSelect = pObject->IsInsideBox(pInfo->pBox->bmins, pInfo->pBox->bmaxs);
		}
		else
		{
			bSelect = pObject->IsIntersectingBox(pInfo->pBox->bmins, pInfo->pBox->bmaxs);
		}

		if (bSelect)
		{
			pInfo->pDoc->SelectObject(pSelObject, scSelect);
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static BOOL SelectInLogicalBox( CMapClass *pObject, SelectLogicalBoxInfo_t *pInfo)
{
	// Skip hidden objects.
	if ( !pObject->IsVisible() || !pObject->IsLogical() || !pObject->IsVisibleLogical() )
		return TRUE;

	// FIXME: Box selection doesn't work when this is uncommented. Why?
	// Skip anything with children. We only are interested in leaf objects because
	// PrepareSelection will call up to tree to get the proper ancestor.
//	if ( pObject->GetChildCount() )
//		return TRUE;

	// Skip groups. Groups are selected via their members through PrepareSelection.
	if ( pObject->IsGroup() )
	{
		// Shouldn't ever have empty groups lying around! 
		// Except if you drag select an empty area!
//		Assert(false);
		return TRUE;
	}

	// Skip clutter helpers.
	if ( pObject->IsClutter() )
		return TRUE;

	// FIXME: We're calling PrepareSelection on nearly everything in the world,
	// then doing the box test against the object that we get back from that!
	// We should use the octree to cull out most of the world up front.
	CMapClass *pSelObject = pObject->PrepareSelection(pInfo->eSelectMode);
	if ( pSelObject )
	{
		Vector2D mins, maxs;
		pObject->GetRenderLogicalBox( mins, maxs );

		bool bSelect;
		if ( pInfo->bInside )
		{
			bSelect = IsBoxInside( mins, maxs, pInfo->vecMins, pInfo->vecMaxs );
		}
		else
		{
			bSelect = IsBoxIntersecting( mins, maxs, pInfo->vecMins, pInfo->vecMaxs );
		}

		if (bSelect)
		{
			pInfo->pDoc->SelectObject( pSelObject, scSelect );
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::SelectRegion( BoundBox *pBox, bool bInsideOnly, bool ResetSelection )
{
	SelectBoxInfo_t info;
	info.pDoc = this;
	info.pBox = pBox;	
	info.bInside = bInsideOnly;
	info.eSelectMode = m_pSelection->GetMode();

	if ( ResetSelection )
	{
		SelectObject(NULL, scSaveChanges);
	}

	m_pWorld->EnumChildren((ENUMMAPCHILDRENPROC)SelectInBox, (DWORD)&info);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::SelectLogicalRegion( const Vector2D &vecMins, const Vector2D &vecMaxs, bool bInsideOnly)
{
	SelectLogicalBoxInfo_t info;
	info.pDoc = this;
	info.vecMins = vecMins;	
	info.vecMaxs = vecMaxs;	
	info.bInside = bInsideOnly;
	info.eSelectMode = m_pSelection->GetMode();

	SelectObject(NULL, scSaveChanges);

	m_pWorld->EnumChildren((ENUMMAPCHILDRENPROC)SelectInLogicalBox, (DWORD)&info);
}

bool CMapDoc::SelectObject(CMapClass *pObj, int cmd)
{
	return m_pSelection->SelectObject( pObj, cmd );
}

void CMapDoc::SelectObjectList(const CMapObjectList *pList, int cmd)
{
	m_pSelection->SelectObjectList( pList, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::UpdateStatusbar(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_FACEEDIT_MATERIAL)
	{
		CString str;
		str.Format("%d faces selected", GetMainWnd()->m_pFaceEditSheet->GetFaceListCount() );
		SetStatusText(SBI_SELECTION, str);
		SetStatusText(SBI_SIZE, "");
		return;
	}

	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		pTool->UpdateStatusBar();
	}

	CString str;
	if ( m_pSelection )
	{
		int nCount = m_pSelection->GetCount();
		switch (nCount)
		{
		case 0:
			{
				str = "no selection.";
				break;
			}

		case 1:
			{
				CMapClass *pobj = m_pSelection->GetList()->Element(0);
				str = pobj->GetDescription();

				// Look for the 3D view so we can also add the distance to the object.
				POSITION p = GetFirstViewPosition();
				while (p)
				{
					CMapView3D *pView = dynamic_cast<CMapView3D*>(GetNextView(p));
					if (pView)
					{
						// Get the position of the 3D camera.
						Vector vViewPoint( 0, 0, 0 );
						CCamera *pCam = pView->GetCamera();
						pCam->GetViewPoint(vViewPoint);

						// Get the position of the object.
						Vector vObjOrigin;
						pobj->GetOrigin( vObjOrigin );
						float flDist = vViewPoint.DistTo( vObjOrigin );

						// Add the distance to the status bar string.
						char strDist[512];
						V_snprintf( strDist, sizeof( strDist ), "   [dist: %.1f]", flDist );
						str += strDist;
						break;
					}
				}

				break;
			}

		default:
			{
				str.Format("%d objects selected.", nCount);
				break;
			}
		}
	}

	SetStatusText(SBI_SELECTION, str);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::BuildCascadingSelectionList( CMapClass *pObj, CUtlRBTree< CMapClass*, unsigned short > &list, bool bRecursive )
{
	// Also add all entities connected to outputs of this selection
	CEditGameClass *pClass = dynamic_cast< CEditGameClass * >( pObj );
	if ( !pClass )
		return;

	int nCount = pClass->Connections_GetCount();
	for ( int j = 0; j < nCount; ++j )
	{
		CEntityConnection *pConn = pClass->Connections_Get( j );

		CMapEntityList entityList;
		FindEntitiesByName( entityList, pConn->GetTargetName(), true );
		
		int nOutputCount = entityList.Count();
		for ( int k = 0; k < nOutputCount; ++k )
		{
			CMapEntity *pEntity = entityList.Element(k);
			if ( pEntity == pObj )
				continue;

			if ( list.InsertIfNotFound( pEntity ) != list.InvalidIndex() )
			{
				if ( bRecursive )
				{
					BuildCascadingSelectionList( pEntity, list, bRecursive );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDoc - 
//-----------------------------------------------------------------------------
void CMapDoc::SetActiveMapDoc(CMapDoc *pDoc)
{
	// Only do the work when the doc actually changes.
	if (pDoc == m_pMapDoc)
	{
		return;
	}

	if ( m_pMapDoc != NULL )
	{
		// disable active views in all map doc
		m_pMapDoc->SetActiveView(NULL);
	}

	m_pMapDoc = pDoc;
	m_pManifest = dynamic_cast< CManifest * >( m_pMapDoc );
	
	//
	// Set the new document in the shell.
	//
	g_Shell.SetDocument(m_pMapDoc);

	//
	// Set the history to the document's history.
	//
	if (m_pMapDoc != NULL)
	{
		// attach document selection to property box
		GetMainWnd()->pObjectProperties->SetObjectList( m_pMapDoc->GetSelection()->GetList() );

		if ( m_pManifest && m_pManifest->GetPrimaryMap() )
		{
			CHistory::SetHistory( m_pManifest->GetPrimaryMap()->m_Map->GetDocHistory() );
		}
		else
		{
			CHistory::SetHistory(m_pMapDoc->GetDocHistory());
		}
		m_pMapDoc->SetUndoActive(GetMainWnd()->IsUndoActive() == TRUE);
        m_pMapDoc->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}
	else
	{
		CHistory::SetHistory(NULL);
		GetMainWnd()->pObjectProperties->SetObjectList( NULL );
	}

	//
	// Notify that the active document has changed.
	//
	GetMainWnd()->GlobalNotify(WM_MAPDOC_CHANGED);

	// dvs: don't do this anymore because we run single-config only
	// Set global game config to type found in doc.
	//
	//CGameConfig *pOldGame = CGameConfig::GetActiveGame();
	//if (pDoc != NULL)
	//{
	//	CGameConfig::SetActiveGame(pDoc->GetGame());
	//}
	//else
	//{
	//	CGameConfig::SetActiveGame(NULL);
	//}
	//
	//
	// Update everything the first time we create a document or when the
	// game configuration changes between documents.
	//
	//static bool bFirst = true;
	//if ((pOldGame != CGameConfig::GetActiveGame()) || (bFirst))
	//{
	//	bFirst = false;
	//	GetMainWnd()->GlobalNotify(WM_GAME_CHANGED);
	//}

	// Update everything the first time we create a document.
	static bool bFirst = true;
	if (bFirst)
	{
		bFirst = false;
		GetMainWnd()->GlobalNotify(WM_GAME_CHANGED);
	}
}


//-----------------------------------------------------------------------------
// Purpose: this will activate and bring to front the supplied map document
// Input  : pDoc - the document to activate
// Output : none
//-----------------------------------------------------------------------------
void CMapDoc::ActivateMapDoc( CMapDoc *pDoc )
{
	POSITION posView = pDoc->GetFirstViewPosition( );
	if( posView )
	{
		CView* pView = pDoc->GetNextView( posView );
		((CMDIChildWnd*) pView->GetParentFrame( ))->MDIActivate( );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapWorld *GetActiveWorld(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		return(pDoc->GetMapWorld());
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
IWorldEditDispMgr *GetActiveWorldEditDispManager( void )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( pDoc  )
	{
		CMapWorld *pWorld = pDoc->GetMapWorld();
		if( pWorld )
		{
			return pWorld->GetWorldEditDispManager();
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the object by removing it from its parent. The object is
//			kept in the Undo history. If the object being deleted is the only
//			child of a solid entity, that entity is also deleted.
// Input  : pObject - The object to delete.
//-----------------------------------------------------------------------------
void CMapDoc::DeleteObject(CMapClass *pObject)
{
	GetHistory()->KeepForDestruction(pObject);

	CMapClass *pParent = pObject->GetParent();

	RemoveObjectFromWorld(pObject, true);

	// If we are deleting the last child of a solid entity, or the last member of 
	// a group, delete the parent object also. This avoids ghost objects at the origin.

	pObject->SignalChanged();
	if (pParent)
	{
		if (pParent->IsGroup())
		{
			if (pParent->GetChildCount() == 0)
			{
				DeleteObject(pParent);
			}
		}
		else
		{
			CMapEntity *pParentEntity = dynamic_cast <CMapEntity *>(pParent);
			if (pParentEntity != NULL)
			{
				if (!pParentEntity->IsPlaceholder() && !pParentEntity->HasSolidChildren())
				{
					DeleteObject(pParentEntity);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Call this to delete multiple objects in a single operation.
// Input  : List - 
//-----------------------------------------------------------------------------
void CMapDoc::DeleteObjectList(CMapObjectList &List)
{
	FOR_EACH_OBJ( List, pos )
	{
		CMapClass *pObject = List.Element(pos);
		Assert(pObject != NULL);

		DeleteObject(pObject);
	}

	m_pSelection->RemoveAll();

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Delete selected objects.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditDelete(void)
{
	Delete();
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the search/replace dialog.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditReplace(void)
{
	GetMainWnd()->ShowSearchReplaceDialog();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapSnaptogrid(void)
{
	m_bSnapToGrid = !m_bSnapToGrid;
	UpdateStatusBarSnap();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::UpdateStatusBarSnap(void)
{
	CString strSnap;
	strSnap.Format(" Snap: %s Grid: %d ", m_bSnapToGrid ? "On" : "Off", m_nGridSpacing);
	SetStatusText(SBI_SNAP, strSnap);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateMapSnaptogrid(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bSnapToGrid);
}


//-----------------------------------------------------------------------------
// Purpose: Deselects everything.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditClearselection(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		// clear morph
		m_pToolManager->GetActiveTool()->SetEmpty();
		UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}
	else if (m_pToolManager->GetActiveToolID() == TOOL_FACEEDIT_MATERIAL)
	{
		SelectFace(NULL, 0, scClear|scSaveChanges);
	}
	else
	{
		if ( m_pSelection->GetCount() > 2)
		{
			GetHistory()->MarkUndoPosition( m_pSelection->GetList(), "Clear Selection");
		}

		SelectObject(NULL, scClear|scSaveChanges);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSolid - 
//			pszTexture - 
// Output : Returns TRUE to continue iterating.
//-----------------------------------------------------------------------------
static BOOL ApplyTextureToSolid(CMapSolid *pSolid, LPCTSTR pszTexture)
{
	pSolid->SetTexture(pszTexture);
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Apply Current Texture toolbar button.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateEditApplytexture(CCmdUI* pCmdUI) 
{
	if ( IsSelectionEditable() == false )
	{
		pCmdUI->Enable( FALSE );

	}
	else
	{
		pCmdUI->Enable( ( m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL ) && !GetMainWnd()->IsShellSessionActive() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Applies the current default texture to all faces of all selected solids.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditApplytexture(void)
{
	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition( pSelList, "Apply Texture");

	// texturebar.cpp:
	LPCTSTR GetDefaultTextureName();

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pobj = pSelList->Element(i);
		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
		{
			GetHistory()->Keep(pobj);
			((CMapSolid*)pobj)->SetTexture(GetDefaultTextureName());
		}

		pobj->EnumChildren((ENUMMAPCHILDRENPROC)ApplyTextureToSolid, (DWORD)GetDefaultTextureName(), MAPCLASS_TYPE(CMapSolid));
	}

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Callback for EnumChildren. Adds the object to the given list.
// Input  : pObject - Object to add to the list.
//			pList - List to add the object to.
// Output : Returns TRUE to continue iterating.
//-----------------------------------------------------------------------------
static BOOL CopyObjectsToList(CMapClass *pObject, CMapObjectList *pList)
{
	pList->AddToTail(pObject);
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Makes all selected brushes the children of a solid entity. The
//			class will be the default solid class from the game configuration.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditToEntity(void)
{
	extern GameData *pGD;

	CMapEntity *pNewEntity = NULL;
	BOOL bMadeEntity = TRUE;
	BOOL bUseSelectionDialog = FALSE;

	//
	// Build a list of every solid in the selection, whether part of a solid entity or not.
	//
	CMapObjectList newobjects;
	const CMapObjectList *pSelList = m_pSelection->GetList();
	
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		//
		// If the object is a solid, add it to our list.
		//
		if (pObject->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
		{
			newobjects.AddToTail(pObject);
		}
		//
		// If the object is a group, add any solids in the group to our list.
		//
		else if (pObject->IsGroup())
		{
			pObject->EnumChildren(ENUMMAPCHILDRENPROC(CopyObjectsToList), DWORD(&newobjects), MAPCLASS_TYPE(CMapSolid));
		}
		//
		// If the object is an entity, add any solid children of the entity to our list.
		//
		else if (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
		{
			pObject->EnumChildren(ENUMMAPCHILDRENPROC(CopyObjectsToList), DWORD(&newobjects), MAPCLASS_TYPE(CMapSolid));

			//
			// See if there is more than one solid entity selected. If so, we'll need to prompt the user
			// to pick one.
			//
			CMapEntity *pEntity = (CMapEntity *)pObject;
			if (!pEntity->IsPlaceholder())
			{
				//
				// Already found an eligible entity, so we want
				// to call up the entity selection dialog. 
				//
				if (pNewEntity != NULL)
				{
					bUseSelectionDialog = TRUE;
				}

				pNewEntity = pEntity;
			}
		}
	}

	//
	// If the list is empty, we have nothing to do.
	//
	if ( newobjects.Count() == 0)
	{
		AfxMessageBox("There are no eligible selected objects.");
		return;
	}

	//
	// If already have an entity selected, ask if they want to 
	// add solids to it.
	//
	if (pNewEntity && !bUseSelectionDialog)
	{
		CString str;
		str.Format("You have selected an existing entity (a '%s'.)\n"
			"Would you like to add the selected solids to the existing entity?\n"
			"If you select 'No', a new entity will be created.",
			pNewEntity->GetClassName());
	
		if (AfxMessageBox(str, MB_YESNO) == IDNO)
		{
			pNewEntity = NULL;	// it'll be made down there
		}
		else
		{
			bMadeEntity = FALSE;
		}
	}
	//
	// If there were multiple solid entities selected, bring up the selection dialog.
	//
	else if (bUseSelectionDialog)
	{
		CSelectEntityDlg dlg(m_pSelection->GetList());
		GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
		if (dlg.DoModal() == IDCANCEL)
		{
			return;	// forget about it
		}
		pNewEntity = dlg.m_pFinalEntity;
		bMadeEntity = FALSE;
	}

	GetHistory()->MarkUndoPosition(m_pSelection->GetList(), "To Entity");
	GetHistory()->Keep(m_pSelection->GetList());

	//
	// If they haven't already picked an entity to add the solids to, create a new
	// solid entity.
	//
	if (!pNewEntity)
	{
		pNewEntity = new CMapEntity;
		bMadeEntity = TRUE;
	}

	//
	// Add all the solids in our list to the solid entity.
	//
	FOR_EACH_OBJ( newobjects, pos )
	{
		CMapClass *pObject = newobjects.Element(pos);
		CMapClass *pOldParent = pObject->GetParent();

		//
		// If the solid is changing parents...
		//
		if (pOldParent != pNewEntity)
		{
			Assert(pOldParent != NULL);
			if (pOldParent != NULL)
			{
				//
				// Remove the solid from its current parent.
				//
				pOldParent->RemoveChild(pObject);

				//
				// If this solid was the child of a solid entity, check to see if the entity has
				// any children left - if not, we remove it from the world because it's useless without
				// solid children.
				//
				CMapEntity *pOldParentEnt = dynamic_cast<CMapEntity *>(pOldParent);
				if (pOldParentEnt && (!pOldParentEnt->IsPlaceholder()) && (!pOldParentEnt->HasSolidChildren()))
				{
					DeleteObject(pOldParentEnt);
				}
			}

			//
			// Add visgroups from the solid to the new entity.
			//
			int nVisGroupCount = pObject->GetVisGroupCount();
			for (int nVisGroup = 0; nVisGroup < nVisGroupCount; nVisGroup++)
			{
				CVisGroup *pVisGroup = pObject->GetVisGroup(nVisGroup);

				// Don't transfer autovisgroups
				if ( pVisGroup->IsAutoVisGroup() )
					continue;

				pNewEntity->AddVisGroup(pVisGroup);
			}

			//
			// Remove the child from all visgroups
			//
			pObject->RemoveAllVisGroups();

			//
			// Add the solid as a child of the new parent entity.
			//
			pNewEntity->AddChild(pObject);
		}
	}

	//
	// If we created a new entity, add it to the world.
	//
	if (bMadeEntity)
	{
		pNewEntity->SetPlaceholder(FALSE);
		pNewEntity->SetClass(g_pGameConfig->szDefaultSolid);
		AddObjectToWorld(pNewEntity);

		//
		// Don't keep our children because they are not new to the world.
		//
		GetHistory()->KeepNew(pNewEntity, false);
	}

	SelectObject(pNewEntity, scClear|scSelect|scSaveChanges );

	m_pToolManager->SetTool(TOOL_POINTER);

	if (bMadeEntity)
	{
		GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
		GetMainWnd()->pObjectProperties->SetActiveWindow();
	}
	
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Moves all solid children of selected entities to the world.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditToWorld(void)
{
	CMapObjectList SelList;
	SelList.AddVectorToTail( *m_pSelection->GetList());

	if ( SelList.Count()>0 )
	{
		GetHistory()->MarkUndoPosition(m_pSelection->GetList(), "To World");
		GetHistory()->Keep(m_pSelection->GetList());

		//
		// Remove selection rect from screen & clear selection list.
		//
		SelectObject(NULL, scClear|scSaveChanges );

		FOR_EACH_OBJ( SelList, pos )
		{
			CMapClass *pObject = SelList.Element(pos);
			CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);

			//
			// If this is a solid entity, move all its children to the world.
			//
			if ((pEntity != NULL) && (!pEntity->IsPlaceholder()))
			{
				//
				// Build a list of the entity's solid children.
				//
				CMapObjectList ChildList;
				
				const CMapObjectList *pChildren = pEntity->GetChildren();
				FOR_EACH_OBJ( *pChildren, pos2 )
				{
					CMapClass *pChild = pChildren->Element(pos2);

					if ((dynamic_cast<CMapSolid *>(pChild)) != NULL)
					{
						ChildList.AddToTail(pChild);
					}
				}

				//
				// Detach all the children from the entity. This throws out
				// all non-solid children, since they aren't in our list.
				//
				pEntity->RemoveAllChildren();

				//
				// Move the entity's former solid children to the world.
				//
				int nChildCount = ChildList.Count();
				for (int i = 0; i < nChildCount; i++)
				{
					CMapClass *pChild = ChildList.Element(i);

					m_pWorld->AddChild(pChild);
					int nVisGroupCount = pEntity->GetVisGroupCount();
					for (int nVisGroup = 0; nVisGroup < nVisGroupCount; nVisGroup++)
					{
						CVisGroup *pVisGroup = pEntity->GetVisGroup(nVisGroup);

						// Don't add autovisgroups when moving back
						if ( pVisGroup->IsAutoVisGroup() )
							continue;

						pChild->AddVisGroup(pVisGroup);
					}

					// Add autovisgroups for the child
					RemoveFromAutoVisGroups( pChild );
					AddToAutoVisGroup( pChild );

					pChild->SetRenderColor(0, 100 + (random() % 156), 100 + (random() % 156));
					SelectObject(pChild, scSelect);

				}

				//
				// The entity is empty; delete it.
				//
				DeleteObject(pEntity);
			}
		}
	}

	m_pToolManager->SetTool(TOOL_POINTER);
		
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Subtracts the first object in the selection set (by index) from
//			all solids in the world.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsSubtractselection(void)
{
	if ( m_pSelection->IsEmpty())
	{
		return;
	}

	//
	// Subtract with the first object in the selection list.
	//

	const CMapObjectList *pSelList = m_pSelection->GetList();

	CMapClass *pSubtractWith = pSelList->Element(0);

	Assert( pSubtractWith != NULL );
	

	GetHistory()->MarkUndoPosition(pSelList, "Carve");
	GetHistory()->Keep(pSelList);

	//
	// Build a list of every solid in the world.
	//
	CMapObjectList WorldSolids;
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pChild);
		if (pSolid != NULL)
		{
			WorldSolids.AddToTail(pSolid);
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}

	if (WorldSolids.Count() == 0)
	{
		return;
	}

	bool bLocked = VisGroups_LockUpdates( true );

	//
	// Subtract the 'subtract with' object from every solid in the world.
	//
	FOR_EACH_OBJ( WorldSolids, p )
	{
		CMapSolid *pSubtractFrom = (CMapSolid *)WorldSolids.Element(p);
		CMapClass *pDestParent = pSubtractFrom->GetParent();

		//
		// Perform the subtraction. If the two objects intersected...
		//
		CMapObjectList Outside;
		if (pSubtractFrom->Subtract(NULL, &Outside, pSubtractWith))
		{
			if (Outside.Count() > 0)
			{
				CMapClass *pResult = NULL;

				//
				// If the subtraction resulted in more than one object, create a group
				// to place the results in.
				//
				if (Outside.Count() > 1)
				{
					pResult = (CMapClass *)(new CMapGroup);
					FOR_EACH_OBJ( Outside, pos2 )
					{
						CMapClass *pTemp = Outside.Element(pos2);
						pResult->AddChild(pTemp);
					}
				}
				//
				// Otherwise, the results are the single object.
				//
				else if (Outside.Count() == 1)
				{
					pResult = Outside[0];
				}

				//
				// Replace the 'subtract from' object with the subtraction results.
				//
				DeleteObject(pSubtractFrom);
				AddObjectToWorld(pResult, pDestParent);
				GetHistory()->KeepNew(pResult);
			}
		}
	}

	if ( bLocked )
		VisGroups_LockUpdates( false );


	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Copies the selected objects to the clipboard.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditCopy(void)
{
	if ( !m_pSelection->IsCopyable() )
	{
		return;
	}

	Copy();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ptOrg - 
//-----------------------------------------------------------------------------
void CMapDoc::GetBestVisiblePoint(Vector& ptOrg)
{
	// create paste position at center of 1st 2d view
	
	FOR_EACH_OBJ( MRU2DViews, pos )
	{
		CMapView2D *pView = MRU2DViews.Element(pos);
		pView->GetCenterPoint(ptOrg);
	}

	for(int i = 0; i < 3; i++)
	{
		if(ptOrg[i] == COORD_NOTINIT)
			ptOrg[i] = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets a point on the screen to paste to. Functionalized because it
//			is called from OnEditPaste and OnEditPasteSpecial.
//-----------------------------------------------------------------------------
void CMapDoc::GetBestPastePoint(Vector &vecPasteOrigin)
{
	//
	// Start with a visible grid point near the center of the screen.
	//
	vecPasteOrigin = Vector(COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT);

	CMapView *pView =  GetActiveMapView();
	CView *pMFCView = dynamic_cast<CView*>(pView);

	if ( pView )
	{
		// zoom in on cursor position
		POINT ptClient;
		GetCursorPos(&ptClient);
		pMFCView->ScreenToClient(&ptClient);

		int width, height;
		pView->GetCamera()->GetViewPort( width, height );

		if ( ptClient.x >= 0 && ptClient.x < width &&
			ptClient.y >= 0 && ptClient.y < height )
		{
			// if mouse pos is inside a view, figure out whic one

			CMapView3D *p3DView = dynamic_cast<CMapView3D*>(pView);
			CMapView2D *p2DView = dynamic_cast<CMapView2D*>(pView);
			Vector2D vPoint(ptClient.x,ptClient.y);

			if ( p3DView )
			{
				HitInfo_t Hits;

				if ( p3DView->ObjectsAt( vPoint, &Hits, 1, FLAG_OBJECTS_AT_RESOLVE_INSTANCES ) )
				{
					VMatrix LocalMatrixNeg;

					Hits.m_LocalMatrix.InverseTR( LocalMatrixNeg );

					// If they clicked on a solid, the index of the face they clicked on is stored
					// in array index [1].
					CMapClass *pObject = Hits.pObject;
					CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pObject);

					Vector HitPos,HitNormal;
					Vector start,end,mins,maxs,delta;
					float dist;
					int face;
					bool bOk = false;

					p3DView->GetCamera()->BuildRay( vPoint, start, end);
					Vector vFinalStart, vFinalEnd;
					LocalMatrixNeg.V3Mul( start, vFinalStart );
					LocalMatrixNeg.V3Mul( end, vFinalEnd );

					if (pSolid != NULL)
					{
						// Build a ray to trace against the face that they clicked on to
						// find the point of intersection.
						CMapFace *pFace = pSolid->GetFace(Hits.uData);
						bOk = pFace->TraceLine(HitPos, HitNormal, vFinalStart, vFinalEnd);
					}
					else if ( pObject != NULL )
					{
						// we hit something, just trace against bound box
						pObject->GetRender2DBox( mins, maxs );
						dist = IntersectionLineAABBox( mins, maxs, vFinalStart, vFinalEnd, face );

						if ( dist > 0 )
						{
							delta = vFinalEnd-vFinalStart;
							VectorNormalize( delta );
							HitPos = vFinalStart + dist * delta;
							HitNormal = GetNormalFromFace( face );
							bOk = true;
						}		
					}

					if ( bOk )
					{
						Vector vFinalHitPos, vFinalHitNormal;
						Hits.m_LocalMatrix.V3Mul( HitPos, vFinalHitPos );
						vFinalHitNormal = Hits.m_LocalMatrix.ApplyRotation( HitNormal );

						mins = GetHammerClipboard()->Bounds.bmins;
						maxs = GetHammerClipboard()->Bounds.bmaxs;
						delta = vFinalHitPos - (mins+maxs)/2;
						mins += delta;
						maxs += delta;
						start = vFinalHitPos;
						end = vFinalHitPos + vFinalHitNormal*4096;
						dist = IntersectionLineAABBox( mins, maxs, start, end, face );

						if ( dist > 0 )
						{
							vecPasteOrigin = vFinalHitPos + vFinalHitNormal * dist;
						}
						else
						{
							vecPasteOrigin = vFinalHitPos;
						}
					}
				}
			}
			else if ( p2DView )
			{
				p2DView->ClientToWorld( vecPasteOrigin, vPoint );
				vecPasteOrigin[p2DView->axThird] = COORD_NOTINIT;
				GetBestVisiblePoint(vecPasteOrigin);
				Snap(vecPasteOrigin);
			}
		}
	}

	// ok, no mouse over any active view, use center of last used 2D views
	if ( vecPasteOrigin.x == COORD_NOTINIT )
	{
		GetBestVisiblePoint(vecPasteOrigin);
		Snap(vecPasteOrigin);

		// Offset the center relative to the grid the same as it was originally.
		Vector vecSnappedOriginalCenter = GetHammerClipboard()->vecOriginalCenter;
		Snap(vecSnappedOriginalCenter);
		vecPasteOrigin += GetHammerClipboard()->vecOriginalCenter - vecSnappedOriginalCenter;
	}
}


void CMapDoc::Paste( IHammerClipboard *pClipboard, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix )
{
	CHammerClipboard *pSrc = GetHammerClipboard( pClipboard );

	Paste( pSrc->Objects, pSrc->pSourceWorld, pDestWorld, vecOffset, vecRotate, pParent, bMakeEntityNamesUnique, pszEntityNamePrefix );
}


//-----------------------------------------------------------------------------
// Purpose: This function will peform a 'cut' operation on the selected entities.
//-----------------------------------------------------------------------------
void CMapDoc::Cut( IHammerClipboard *pClipboard )
{
	Copy( pClipboard );
	Delete();
}


//-----------------------------------------------------------------------------
// Purpose: This function will perform a 'copy' operation on the selected entities.
//-----------------------------------------------------------------------------
void CMapDoc::Copy( IHammerClipboard *pClipboard )
{
	if ( m_pSelection->IsEmpty() )
	{
		return;
	}

	if ( pClipboard == NULL )
	{
		pClipboard = GetHammerClipboard();
	}

	BeginWaitCursor();

	// Delete the contents of the clipboard.
	GetHammerClipboard( pClipboard )->Objects.PurgeAndDeleteElements();

	m_pSelection->GetBoundsCenter(GetHammerClipboard( pClipboard )->vecOriginalCenter);
	m_pSelection->GetBounds(GetHammerClipboard( pClipboard )->Bounds.bmins, GetHammerClipboard( pClipboard )->Bounds.bmaxs);

	GetHistory()->Pause();

	GetHammerClipboard( pClipboard )->pSourceWorld = m_pWorld;

	// Copy the selected objects to the clipboard.
	const CMapObjectList *pSelList = m_pSelection->GetList();

	for (int i = 0; i < pSelList->Count()	; i++)
	{
		CMapClass *pobj = pSelList->Element(i);
		CMapClass *pNewobj = pobj->Copy(false);

		//
		// Prune the object from the world tree without calling RemoveObjectFromWorld.
		// This prevents CopyChildrenFrom from updating the culling tree.
		//
		//pNewobj->SetObjParent(NULL);

		//
		// Copy all the children from the original object into the copied object.
		//
		pNewobj->CopyChildrenFrom(pobj, false);

		//
		// Remove the copied object from the world.
		//
		RemoveObjectFromWorld(pNewobj, true);
		pNewobj->RemoveAllVisGroups();

		GetHammerClipboard( pClipboard )->Objects.AddToTail(pNewobj);
	}

	GetHistory()->Resume();

	EndWaitCursor();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Objects - 
//			pSourceWorld - 
//			pDestWorld - 
//			vecOffset - 
//			vecRotate - 
//			pParent - 
//-----------------------------------------------------------------------------
void CMapDoc::Paste(CMapObjectList &Objects, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix)
{
	//
	// Copy the objects in the clipboard and build a list of objects to paste
	// into the world.
	//
	CMapObjectList PasteList;

	bool bLocked = VisGroups_LockUpdates( true );

	if ( GetManifest() && pSourceWorld != pDestWorld )
	{
		bMakeEntityNamesUnique = true;
	}

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = pOriginal->Copy(false);

		pCopy->CopyChildrenFrom(pOriginal, false);
		PasteList.AddToTail(pCopy);
	}

	if ( bMakeEntityNamesUnique || ( pszEntityNamePrefix && ( pszEntityNamePrefix[0] != '\0' ) ) )
	{
		//
		// Make all the pasted objects with names have new, unique names within the destination world.
		//

		// Stick the objects into a temporary world object for renaming the entities.
		CMapWorld temp( this );
		
		FOR_EACH_OBJ( PasteList, pos )
		{
			CMapClass *pObject = PasteList.Element( pos );
			temp.AddChild( pObject );
		}

		RenameEntities( &temp, pDestWorld, bMakeEntityNamesUnique, pszEntityNamePrefix );

		// So they don't get deleted when temp goes out of scope.
		temp.RemoveAllChildren();
	}

	//
	// Notification happens in two-passes. The first pass lets objects generate new unique
	// IDs in the destination world, the second pass lets objects fixup references to other
	// objects in the clipboard.
	//

	Assert( Objects.Count() == PasteList.Count() );

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = PasteList.Element(pos);

		pOriginal->OnPrePaste(pCopy, pSourceWorld, pDestWorld, Objects, PasteList);
	}

	//
	// Add the objects to the world.
	//

    FOR_EACH_OBJ( PasteList, pos )
	{
		CMapClass *pCopy = PasteList.Element(pos);

		if (vecOffset != vec3_origin)
		{
			pCopy->TransMove(vecOffset);
		}

		if (vecRotate != vec3_angle)
		{
			Vector ptCenter;
			pCopy->GetBoundsCenter(ptCenter);
			pCopy->TransRotate(ptCenter, vecRotate);
		}

		AddObjectToWorld(pCopy, pParent);
	}

	//
	// Do the second pass of notification. The second pass of notification lets objects
	// fixup references to other objects that were pasted. We don't do it in the loop above
	// because then not all the pasted objects would be in the world yet.
	//
	Assert( Objects.Count() == PasteList.Count() );

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = PasteList.Element(pos);

		pOriginal->OnPaste(pCopy, pSourceWorld, pDestWorld, Objects, PasteList);

		//
		// Semi-HACK: If we aren't pasting into a group, keep the new object in the Undo stack.
		// Otherwise, we'll keep the group in OnEditPasteSpecial.
		//
		if ((pParent == NULL) || (pParent == pDestWorld))
		{
			GetHistory()->KeepNew(pCopy);
	 		SelectObject(pCopy, scSelect);
		}
	}

	if ( bLocked )
	{
		VisGroups_LockUpdates( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Objects - 
//			pSourceWorld - 
//			pDestWorld - 
//			vecOffset - 
//			vecRotate - 
//			pParent - 
//-----------------------------------------------------------------------------
void CMapDoc::PasteInstance(CMapObjectList &Objects, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix)
{
	QAngle	vecRotateFix;

	vecRotateFix.x = vecRotate[ PITCH ];
	vecRotateFix.y = vecRotate[ ROLL ];
	vecRotateFix.z = vecRotate[ YAW ];

	//
	// Copy the objects in the clipboard and build a list of objects to paste
	// into the world.
	//
	CMapObjectList PasteList;

	bool bLocked = VisGroups_LockUpdates( true );

	if ( GetManifest() && pSourceWorld != pDestWorld )
	{
		bMakeEntityNamesUnique = true;
	}

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = pOriginal->Copy(false);

		pCopy->CopyChildrenFrom(pOriginal, false);
		PasteList.AddToTail(pCopy);
	}

	if ( bMakeEntityNamesUnique || ( pszEntityNamePrefix && ( pszEntityNamePrefix[0] != '\0' ) ) )
	{
		//
		// Make all the pasted objects with names have new, unique names within the destination world.
		//

		// Stick the objects into a temporary world object for renaming the entities.
		CMapWorld temp( this );

		FOR_EACH_OBJ( PasteList, pos )
		{
			CMapClass *pObject = PasteList.Element( pos );
			temp.AddChild( pObject );
		}

		RenameEntities( &temp, pDestWorld, bMakeEntityNamesUnique, pszEntityNamePrefix );

		// So they don't get deleted when temp goes out of scope.
		temp.RemoveAllChildren();
	}

	//
	// Notification happens in two-passes. The first pass lets objects generate new unique
	// IDs in the destination world, the second pass lets objects fixup references to other
	// objects in the clipboard.
	//

	Assert( Objects.Count() == PasteList.Count() );

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = PasteList.Element(pos);

		pOriginal->OnPrePaste(pCopy, pSourceWorld, pDestWorld, Objects, PasteList);
	}

	//
	// Add the objects to the world.
	//

	FOR_EACH_OBJ( PasteList, pos )
	{
		CMapClass *pCopy = PasteList.Element(pos);

		if (vecRotateFix != vec3_angle)
		{
			pCopy->TransRotate( vec3_origin, vecRotateFix );
		}

		if (vecOffset != vec3_origin)
		{
			pCopy->TransMove(vecOffset);
		}

		AddObjectToWorld(pCopy, pParent);
	}

	//
	// Do the second pass of notification. The second pass of notification lets objects
	// fixup references to other objects that were pasted. We don't do it in the loop above
	// because then not all the pasted objects would be in the world yet.
	//
	Assert( Objects.Count() == PasteList.Count() );

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pOriginal = Objects.Element(pos);
		CMapClass *pCopy = PasteList.Element(pos);

		pOriginal->OnPaste(pCopy, pSourceWorld, pDestWorld, Objects, PasteList);

		//
		// Semi-HACK: If we aren't pasting into a group, keep the new object in the Undo stack.
		// Otherwise, we'll keep the group in OnEditPasteSpecial.
		//
		if ((pParent == NULL) || (pParent == pDestWorld))
		{
			GetHistory()->KeepNew(pCopy);
			SelectObject(pCopy, scSelect);
		}
	}

	if ( bLocked )
	{
		VisGroups_LockUpdates( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function will perform a 'delete' operation on the selected entities.
//-----------------------------------------------------------------------------
void CMapDoc::Delete( void )
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		// Can't delete stuff while morphing.
		return;
	}

	if ( m_pSelection->IsEmpty() || !m_pSelection->IsCopyable() )
	{
		return;
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition(pSelList, "Delete");

	// Delete objects in selection.
	while ( !m_pSelection->IsEmpty() )
	{
		CMapClass *pobj = pSelList->Element(0);

		DeleteObject(pobj);
	}

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Pastes the clipboard contents into the active world.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditPaste(void)
{
	BeginWaitCursor();
	GetHistory()->MarkUndoPosition( m_pSelection->GetList(), "Paste");

	// first, clear selection so we can select all pasted objects
	SelectObject(NULL, scClear|scSaveChanges );

	//
	// Build a translation that will put the pasted objects in the center of the view.
	//
	Vector vecPasteOffset;
	GetBestPastePoint(vecPasteOffset);
	vecPasteOffset -= GetHammerClipboard()->vecOriginalCenter;

	//
	// Paste the objects into the active world.
	//
	CMapWorld *pWorld = GetActiveWorld();
	Paste(GetHammerClipboard()->Objects, GetHammerClipboard()->pSourceWorld, pWorld, vecPasteOffset, QAngle(0, 0, 0), NULL, false, NULL);

	m_pToolManager->SetTool(TOOL_POINTER);
	
	SetModifiedFlag();
	EndWaitCursor();
}

//-----------------------------------------------------------------------------
// Disable the unhide option when the quickhide list is empty.
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_UpdateUnHide(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable( m_QuickHideGroup.Count() > 0 );
}

//-----------------------------------------------------------------------------
// Disable the Create VisGroup menu option when QuickHide is empty.
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_UpdateCreateVisGroupFromHidden(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable( m_QuickHideGroup.Count() > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Copy menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateEditSelection(CCmdUI *pCmdUI) 
{
	bool bMorphSnap = m_pToolManager->GetActiveToolID() == TOOL_MORPH && pCmdUI->m_nID == ID_TOOLS_SNAPSELECTEDTOGRID;
	bool bStandardCommand = ( m_pSelection->GetCount() != 0 ) && m_pSelection->IsCopyable() &&
							  ( m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL ) &&
							  !GetMainWnd()->IsShellSessionActive();
	pCmdUI->Enable( bMorphSnap || bStandardCommand );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Paste menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateEditPaste(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable( GetHammerClipboard()->Objects.Count() &&
		            ( m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL ) &&
					!GetMainWnd()->IsShellSessionActive() );
}


//-----------------------------------------------------------------------------
// Purpose: Handles the Edit | Cut command. Copies the selection to the clipboard,
//			then deletes it from the document.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditCut(void)
{
	if ( !m_pSelection->IsCopyable() )
	{
		return;
	}

	Cut( GetHammerClipboard() );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the various Edit menu items.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateGroupEditFunction(CCmdUI* pCmdUI) 
{
	//
	// Edit functions are disabled when we're applying textures or editing via a shell session.
	//
	pCmdUI->Enable( ( m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL ) &&
		            !GetMainWnd()->IsShellSessionActive() );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new group and adds all selected items to it.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsGroup(void)
{
	if ( m_pSelection->IsEmpty() )
	{
		AfxMessageBox("No objects are selected.");
		return;
	}

	if (( m_pSelection->GetMode() == selectSolids) && !Options.general.bGroupWhileIgnore)
	{
		return;
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	// First see if grouping these objects will remove them from an existing entity or group.
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pobj = pSelList->Element(i);
		if ((pobj->GetParent() != NULL) && (!IsWorldObject(pobj->GetParent())))
		{
			if (GetMainWnd()->MessageBox("Some selected objects are part of an entity or belong to a group. Grouping them now will remove them from the entity or group! Continue?", "Warning", MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
			{
				return;
			}
			break;
		}
	}

	GetHistory()->MarkUndoPosition(m_pSelection->GetList(), "Group Objects");
	GetHistory()->Keep(m_pSelection->GetList());

	//
	// Create a new group containing the selected objects.
	//
	CMapGroup *pGroup = new CMapGroup;
	AddObjectToWorld(pGroup);

	pGroup->SetRenderColor(100 + (random() % 156), 100 + (random() % 156), 0);

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pobj = pSelList->Element(i);
		if (pobj->GetParent() != NULL)
		{
			pobj->GetParent()->RemoveChild(pobj);
		}
		pGroup->AddChild(pobj);
	}

	//
	// Keep the group as a new object. Don't keep its children here,
	// because they are not new.
	//
	GetHistory()->KeepNew(pGroup, false);

	// Clear selection and add the new group to it.
	SelectObject(pGroup, scClear|scSelect|scSaveChanges );
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Ungroups all selected objects.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsUngroup(void)
{
	if (( m_pSelection->GetMode() == selectSolids) && !Options.general.bGroupWhileIgnore)
	{
		return;
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition(pSelList, "Ungroup");
	GetHistory()->Keep(pSelList);
	
	// create new selected list
	CMapObjectList NewSelList;
	NewSelList.AddVectorToTail( *pSelList );

    m_pSelection->SelectObject( NULL, scClear );

	FOR_EACH_OBJ( NewSelList, pos )
	{
		CMapClass *pobj = NewSelList.Element(pos);
		if(!pobj->IsMapClass(MAPCLASS_TYPE(CMapGroup)))
		{
			// make sure it is selected in the map
			SelectObject(pobj, scSelect);
			continue;
		}

		//
		// Build a list of the group's children.
		//
		CMapObjectList ChildList;
		
		const CMapObjectList *pChildren = pobj->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos2 )
		{
			ChildList.AddToTail(pChildren->Element(pos2));
		}

		//
		// Detach the children from the group.
		//
		pobj->RemoveAllChildren();

		//
		// Move the group's former children to the group's parent.
		//
		int nChildCount = ChildList.Count();
		for (int i = 0; i < nChildCount; i++)
		{		
			CMapClass *pChild = ChildList.Element(i);

			pobj->GetParent()->AddChild(pChild);
			
			int nVisGroupCount = pobj->GetVisGroupCount();
			for (int nVisGroup = 0; nVisGroup < nVisGroupCount; nVisGroup++)
			{
				CVisGroup *pVisGroup = pobj->GetVisGroup(nVisGroup);
				pChild->AddVisGroup(pVisGroup);
			}

			pChild->SetRenderColor(0, 100 + (random() % 156), 100 + (random() % 156));
			SelectObject(pChild, scSelect);
		}

		//
		// The group is empty; delete it.
		//
		DeleteObject(pobj);
	}
	
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the visibility of the grid in the 2D views.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewGrid(void)
{
	m_bShowGrid = !m_bShowGrid;
	UpdateAllViews( MAPVIEW_OPTIONS_CHANGED );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the check state of the Show Grid toolbar button and menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewGrid(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowGrid);
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the visibility of the grid in the 2D views.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewLogicalGrid(void)
{
	m_bShowLogicalGrid = !m_bShowLogicalGrid;
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_LOGICAL );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the check state of the Show Grid toolbar button and menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewLogicalGrid(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowLogicalGrid);
}


//-----------------------------------------------------------------------------
// Purpose: Selects all objects that are not hidden.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditSelectall(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		// select all vertices
		Morph3D *pMorph = (Morph3D*) m_pToolManager->GetActiveTool();
		pMorph->SelectHandle(NULL, scSelectAll);
		return;
	}

	SelectObject(NULL, scClear|scSaveChanges );

	const CMapObjectList *pChildren = m_pWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass *pChild = pChildren->Element(pos);

		if (pChild->IsVisible())
		{
			SelectObject(pChild, scSelect);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnFileSaveAs(void)
{
	static char szBaseDir[MAX_PATH] = "";

	bool bSave = true;
	CString str;

	do
	{
		//
		// The default directory for the Save As dialog is either:
		// 1. The directory from which the document was loaded.
		// 2. The last directory they saved any document into.
		// 3. The maps directory as set up in Options | Game Configurations.
		//
		str = GetPathName();
		if (str.ReverseFind('.') != -1)
		{
			str = str.Left(str.ReverseFind('.'));
			strcpy(szBaseDir, str);
			Q_StripFilename(szBaseDir);
		}
		else if (szBaseDir[0] =='\0')
		{
			strcpy(szBaseDir, m_pGame->szMapDir);
		}

		char *pszFilter;
		if ( m_pManifest )
		{
			pszFilter = "Valve Manifest Map Files (*.vmm)|*.vmm||";
		}
		else if (m_pGame->mapformat == mfHalfLife2)
		{
			pszFilter = "Valve Map Files (*.vmf)|*.vmf||";
		}
		else
		{
			pszFilter = "Worldcraft Maps (*.rmf)|*.rmf|Game Maps (*.map)|*.map||";
		}

		CFileDialog dlg(FALSE, NULL, str, OFN_LONGNAMES | OFN_NOCHANGEDIR |	OFN_HIDEREADONLY, pszFilter);
		dlg.m_ofn.lpstrInitialDir = szBaseDir;
		int rvl = dlg.DoModal();

		if (rvl == IDCANCEL)
		{
			return;
		}

		str = dlg.GetPathName();

		// Make sure we've got a .vmt extension, or else compile tools won't work.
		CString wantedExtension = ".vmf";
		if ( m_pManifest )
		{
			wantedExtension = ".vmm";
		}

		if ( m_pGame->mapformat != mfHalfLife2 )
		{
			if ( dlg.m_ofn.nFilterIndex == 1 )
			{
				wantedExtension = ".rmf";
			}
			else if ( dlg.m_ofn.nFilterIndex == 2 )
			{
				wantedExtension = ".map";
			}
		}

		int pos = str.ReverseFind( '.' );
		if ( pos != -1 )
		{
			CString extension = str.Right( str.GetLength() - pos );
			extension.MakeLower();
			if ( extension != wantedExtension )
				str = str.Left( pos ) + wantedExtension;
		}
		else
		{	// no extension
			str += wantedExtension;
		}

		//
		// Save the default directory for next time.
		//
		strcpy(szBaseDir, str);
		Q_StripFilename(szBaseDir);

		bSave = true;

		if (access(str, 0) != -1)
		{
			// The file exists.
			char szConfirm[_MAX_PATH];

			if (access(str, 2) == -1)
			{
				// The file is read-only
				wsprintf(szConfirm, "The file %s is read-only. You must change the file's attributes to overwrite it.", (const char*)str);
				AfxMessageBox(szConfirm, MB_OK | MB_ICONEXCLAMATION);
				bSave = false;
			}
			else
			{
				wsprintf(szConfirm, "Overwrite existing file %s?", (const char*)str);
				if (AfxMessageBox(szConfirm, MB_YESNO | MB_ICONQUESTION) != IDYES)
				{
					bSave = false;
				}
			}
		}
	} while (!bSave);

	OnSaveDocument(str);
	SetPathName(str);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnFileSave(void)
{
	DWORD dwAttrib = GetFileAttributes(GetPathName());
	if (dwAttrib & FILE_ATTRIBUTE_READONLY)
	{
		// we do not have read-write access or the file does not (now) exist
		OnFileSaveAs();
	}
	else
	{
		if(m_strPathName.IsEmpty())
		{
			OnFileSaveAs();
		}
		else
		{
			OnSaveDocument(GetPathName());
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the File | Save menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateFileSave(CCmdUI *pCmdUI)
{
	pCmdUI->SetText(m_bEditingPrefab ? "Update Prefab\tCtrl+S" : "&Save\tCtrl+S");
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapGridlower(void)
{
	if (m_nGridSpacing <= 1)
	{
		return;
	}

	m_nGridSpacing = m_nGridSpacing / 2;
	UpdateAllViews( MAPVIEW_OPTIONS_CHANGED );
	UpdateStatusBarSnap();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapGridhigher(void)
{
	if(m_nGridSpacing >= 512)
		return;

	m_nGridSpacing = m_nGridSpacing * 2;
	UpdateAllViews( MAPVIEW_OPTIONS_CHANGED );
	UpdateStatusBarSnap();
}


class CExportDlg : public CFileDialog
{
public:
	CExportDlg(CString& strFile, LPCTSTR pszExt, LPCTSTR pszDesc) : 
	  CFileDialog(FALSE, pszExt, strFile,
		OFN_NOCHANGEDIR | OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_EXPLORER | 
		OFN_ENABLETEMPLATE,	pszDesc) 
	{
		m_ofn.lpTemplateName = MAKEINTRESOURCE(IDD_MAPEXPORT);
		bVisibles = FALSE;
	}

	afx_msg BOOL OnInitDialog()
	{
		m_Visibles.SubclassDlgItem(IDC_SAVEVISIBLES, this);
		m_Visibles.SetCheck(bVisibles);

		return TRUE;
	}

	afx_msg void OnToggleVisibles()
	{ 
		bVisibles = m_Visibles.GetCheck();
	}

	CButton m_Visibles;
	BOOL bVisibles;

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CExportDlg, CFileDialog)
	ON_BN_CLICKED(IDC_SAVEVISIBLES, OnToggleVisibles)
END_MESSAGE_MAP()

static BOOL bLastVis;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnFileExport(void)
{
	//
	// If we haven't saved the file yet, save it now.
	//
	CString strFile = GetPathName();
	if (strFile.IsEmpty())
	{
		OnFileSave();
		strFile = GetPathName();
		if (strFile.IsEmpty())
		{
			return;
		}
	}

	//
	// Build a name for the exported file.
	//
	int iIndex = strFile.Find('.');

	char *pszFilter;
	char *pszExtension;
	if (m_pGame->mapformat == mfHalfLife2)
	{
		strFile.SetAt(iIndex, '\0');
		
		pszFilter = "Valve Map Files (*.vmf)|*.vmf||";
		pszExtension = "vmf";
	}
	else
	{
		//
		// Use the same filename with a .map extension.
		//
		strcpy(strFile.GetBuffer(1) + iIndex, ".map");
		strFile.ReleaseBuffer();

		pszFilter = "Game Maps (*.map)|*.map||";
		pszExtension = "map";
	}

	//
	// Bring up a dialog to allow them to name the exported file.
	//
	CExportDlg dlg(strFile, pszExtension, pszFilter);

	dlg.m_ofn.lpstrTitle = "Export As";
	dlg.bVisibles = bLastVis;

	if (dlg.DoModal() == IDOK)
	{
		BOOL bModified = IsModified();

		if (strFile.CompareNoCase(dlg.GetPathName()) == 0)
		{
			if (GetMainWnd()->MessageBox("You are about to export over your current work file. Some data loss will occur if you have any objects hidden. Continue?", "Export Warning", MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
			{
				return;
			}
		}
	
		bSaveVisiblesOnly = dlg.bVisibles;
		m_strLastExportFileName = dlg.GetPathName();

		OnSaveDocument(dlg.GetPathName());
		
		bSaveVisiblesOnly = FALSE;

		SetModifiedFlag(bModified);

		bLastVis = dlg.bVisibles;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Exports using the last exported pathname. Brings up the Export dialog
//			only if this document has never been exported.
//-----------------------------------------------------------------------------
void CMapDoc::OnFileExportAgain(void)
{
	CString strFile = m_strLastExportFileName;

	//	
	// If we have never exported this map, bring up the Export dialog.
	//
	if (strFile.IsEmpty())
	{
		OnFileExport();
		return;
	}

	BOOL bModified = IsModified();

	bSaveVisiblesOnly = bLastVis;
	OnSaveDocument(strFile);
	bSaveVisiblesOnly = FALSE;

	SetModifiedFlag(bModified);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnEditMapproperties(void)
{
	m_pSelection->SelectObject( m_pWorld, scClear|scSelect );
	
	GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
}


//-----------------------------------------------------------------------------
// Purpose: Converts a map's textures from WAD3 to VMT.
//-----------------------------------------------------------------------------
void CMapDoc::OnFileConvertWAD( void )
{
	CTextureConverter::ConvertWorldTextures( m_pWorld );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the File | Convert WAD -> VMT menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateFileConvertWAD( CCmdUI * pCmdUI ) 
{
	pCmdUI->Enable( ( m_pWorld != NULL ) && ( g_pGameConfig->GetTextureFormat() == tfVMT ) );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the relevant file extensions for the given map format.
// Input  : mf - 
//			strEditExtension - The extension of the edit file (eg. .VMF, .RMF)
//			strCompileExtension - The extension of the file to compile (eg. .VMF, .MAP)
//-----------------------------------------------------------------------------
void GetFileExtensions(MAPFORMAT mf, CString &strEditExtension, CString &strCompileExtension)
{
	if (mf == mfHalfLife2)
	{
		strEditExtension = ".vmf";
		strCompileExtension = ".vmf";
	}
	else
	{
		strEditExtension = ".rmf";
		strCompileExtension = ".map";
	}
}


//-----------------------------------------------------------------------------
// Purpose: Does a normal map compile.
//-----------------------------------------------------------------------------
void CMapDoc::OnFileRunmap(void)
{
	//
	// Check for texture wads first if the current game config uses them.
	//
	if ((g_pGameConfig->GetTextureFormat() == tfWAD) && !Options.textures.nTextureFiles)
	{
		AfxMessageBox("There are no texture files defined yet. Add some texture files before you run the map.");
		GetMainWnd()->Configure();
		return;
	}

	CString strEditExtension;
	CString strCompileExtension;
	GetFileExtensions(g_pGameConfig->GetMapFormat(), strEditExtension, strCompileExtension);

	CRunMap dlg;
	CRunMapExpertDlg dlgExpert;

	CString strFile = GetPathName();

	bool bWasModified = (IsModified() == TRUE);

	bSaveVisiblesOnly = FALSE;

	// Make sure the .VMF or .RMF is saved first.
	if (strFile.IsEmpty() || bWasModified)
	{
		OnFileSave();
		strFile = GetPathName();
		if (strFile.IsEmpty() || IsModified())
		{
			return;
		}
	}
	strFile.MakeLower();

	// Make sure it has the correct extension for compilation (.VMF or .MAP).
	int iPos = strFile.Find(strEditExtension);
	Assert(iPos != -1);
	if ((iPos != -1) && (strEditExtension.CompareNoCase(strCompileExtension) != 0))
	{
		strcpy(strFile.GetBuffer(0) + iPos, strCompileExtension);
		strFile.ReleaseBuffer();

		// Make sure the .MAP version is up to date.
		if (bWasModified)
		{
			OnSaveDocument(strFile);
		}
	}
		
	// make "bsp" string
	CString strBspFile(strFile);
	iPos = strBspFile.Find(strCompileExtension);
	strcpy(strBspFile.GetBuffer(0) + iPos, ".bsp");
	strBspFile.ReleaseBuffer();
	
	// if no bsp file, make sure it's checked
	if (GetFileAttributes(strBspFile) == 0xFFFFFFFF)
	{
		dlg.m_iQBSP = 1;
	}

	while (1)
	{
		if (AfxGetApp()->GetProfileInt("Run Map", "Mode", 0) == 0)
		{
			// run normal dialog
			if(dlg.DoModal() == IDCANCEL)
				return;

			// switching mode?
			if(dlg.m_bSwitchMode)
			{
				dlg.m_bSwitchMode = FALSE;
				AfxGetApp()->WriteProfileInt("Run Map", "Mode", 1);
			}
			else 
			{
				dlg.SaveToIni();
				break;	// clicked OK
			}
		}
		else
		{
			// run expert dialog
			if (dlgExpert.DoModal() == IDCANCEL)
				return;

			// switching mode?
			if (dlgExpert.m_bSwitchMode)
			{
				AfxGetApp()->WriteProfileInt("Run Map", "Mode", 0);
				dlgExpert.m_bSwitchMode = FALSE;
			}
			else if (dlgExpert.m_pActiveSequence) // clicked ok
			{
				// run the commands in the active sequence
				RunCommands(dlgExpert.m_pActiveSequence->m_Commands, strFile);
				return;
			}
			else
			{
				return;
			}
		}
	}

	if (GetFileAttributes(strBspFile) == 0xFFFFFFFF)
	{
		if (!dlg.m_iQBSP)
		{
			dlg.m_iQBSP = 1;
		}
	}

	CCOMMAND cmd;
	memset(&cmd, 0, sizeof cmd);
	cmd.bEnable = TRUE;
	cmd.bUseProcessWnd = TRUE;
	cmd.bLongFilenames = TRUE;

	CCommandArray cmds;

	// Change to the game drive and directory.
	//cmd.iSpecialCmd = CCChangeDir;
	//Q_snprintf( cmd.szParms, sizeof(cmd.szParms), "\"%s\"", m_pGame->m_szGameExeDir);
	//strcpy(cmd.szRun, "Change Directory");
	//cmds.Add(cmd);
	//cmd.iSpecialCmd = 0;

	// bsp
	if ((dlg.m_iQBSP) && (m_pGame->szBSP[0] != '\0'))
	{
		strcpy(cmd.szRun, "$bsp_exe");
		sprintf(cmd.szParms, "-game $gamedir %s$path\\$file.$ext", dlg.m_iQBSP == 2 ? "-onlyents " : "");

		// check for bsp existence only in quake maps, because
		// we're using the editor's utilities
		if (g_pGameConfig->mapformat == mfQuake)
		{
			cmd.bEnsureCheck = TRUE;
			strcpy(cmd.szEnsureFn, "$path\\$file.bsp");
		}

		cmds.Add(cmd);
 
		cmd.bEnsureCheck = FALSE;
	}

	// vis
	if ((dlg.m_iVis) && (m_pGame->szVIS[0] != '\0'))
	{
		strcpy(cmd.szRun, "$vis_exe");
		sprintf(cmd.szParms, "-game $gamedir %s$path\\$file", dlg.m_iVis == 2 ? "-fast " : "");
		cmds.Add(cmd);
	}

	// rad
	if ((dlg.m_iLight) && (m_pGame->szLIGHT[0] != '\0'))
	{
		strcpy(cmd.szRun, "$light_exe");
		sprintf(cmd.szParms, "%s -game $gamedir %s$path\\$file", dlg.m_bHDRLight ? "-both" : "", dlg.m_iLight == 2 ? "-noextra " : "" );
		cmds.Add(cmd);
	}

	// Copy BSP file to BSP directory for running
	cmd.iSpecialCmd = CCCopyFile;
	strcpy(cmd.szRun, "Copy File");
	sprintf(cmd.szParms, "$path\\$file.bsp $bspdir\\$file.bsp");
	cmds.Add(cmd);
	cmd.iSpecialCmd = 0;

	// Run the game.
	if (dlg.m_bNoQuake == FALSE)
	{
		cmd.bUseProcessWnd = FALSE;
		cmd.bNoWait = TRUE;

		//When running under steam, use applaunch always because if a user forgets to run the game they're trying to launch, it will fail
		//with an obscure error they won't understand.
		{
			strcpy(cmd.szRun, "$game_exe");
			sprintf(cmd.szParms, "-game $gamedir %s +map $file -steam", (const char*)dlg.m_strQuakeParms);
		}

		cmds.Add(cmd);
	}

	RunCommands(cmds, GetPathName());
}


//-----------------------------------------------------------------------------
// Purpose: Updates the title of the doc based on the filename and the
//			active view type.
// Input  : pView - 
//-----------------------------------------------------------------------------
void CMapDoc::UpdateTitle(CView *pView)
{
	CString strFile = GetPathName();
	LPCTSTR pszFilename = strFile;
	
	// Leaving the partial-filename code in here in case we want to make it an option someday.
	bool bShowPartialFilename = false;
	if ( bShowPartialFilename )
	{
		int iPos = strFile.ReverseFind('\\');
		if (iPos != -1)
		{
			pszFilename = strFile.GetBuffer(0) + iPos + 1;
		}
		else
		{
			pszFilename = "Untitled";
		}
	}

	char *pViewType = NULL;
	CMapView2D *pView2D = dynamic_cast <CMapView2D *> (pView);
	if (pView2D != NULL)
	{
		switch (pView2D->GetDrawType())
		{
			case VIEW2D_XY:
			{
				pViewType = "Top";
				break;
			}
			
			case VIEW2D_XZ:
			{
				pViewType = "Right";
				break;
			}
			
			case VIEW2D_YZ:
			{
				pViewType = "Front";
				break;
			}
		}
	}

	CMapViewLogical *pViewLogical = dynamic_cast <CMapViewLogical *> (pView);
	if (pViewLogical != NULL)
	{
		pViewType = "Logical";
	}

	CMapView3D *pView3D = dynamic_cast <CMapView3D *> (pView);
	if (pView3D != NULL)
	{
		switch (pView3D->GetDrawType())
		{
			case VIEW3D_WIREFRAME:
			{
				pViewType = "Wireframe";
				break;
			}

			case VIEW3D_POLYGON:
			{
				pViewType = "Polygon";
				break;
			}

			case VIEW3D_TEXTURED:
			{
				pViewType = "Textured";
				break;
			}

			case VIEW3D_TEXTURED_SHADED:
			{
				pViewType = "Textured Shaded";
				break;
			}

			case VIEW3D_LIGHTMAP_GRID:
			{
				pViewType = "Lightmap grid";
				break;
			}

			case VIEW3D_SMOOTHING_GROUP:
			{
				pViewType = "Smoothing Group";
				break;
			}

			//case VIEW3D_ENGINE:
			//{
			//	pViewType = "Engine";
			//	break;
			//}
		}
	}

	CString str;
	if (pViewType)
	{
		str.Format("%s - %s", pszFilename, pViewType);
	}
	else
	{
		str.Format("%s", pszFilename);
	}

	SetTitle(str);
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the state of Hide Items. When enabled, entities are hidden.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsHideitems(void)
{
	m_bHideItems = !m_bHideItems;
	UpdateVisibilityAll();
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Hide Items menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsHideitems(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck(m_bHideItems ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Hides and shows entity names in the 2D views.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsHideEntityNames(void)
{
	bool bShowEntityNames = !CMapEntity::GetShowEntityNames();
	CMapEntity::ShowEntityNames(bShowEntityNames);
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_2D );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Hide Entity Names menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsHideEntityNames(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck(CMapEntity::GetShowEntityNames() ? FALSE : TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Hides and shows entity names in the 2D views.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewHideUnconnectedEntities(void)
{
	bool bHideUnconnectedEntities = !CMapEntity::GetShowUnconnectedEntities();
	CMapEntity::ShowUnconnectedEntities(bHideUnconnectedEntities);
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_LOGICAL );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Hide Entity Names menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewHideUnconnectedEntities(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck(CMapEntity::GetShowUnconnectedEntities() ? FALSE : TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//-----------------------------------------------------------------------------
void CMapDoc::SetMRU(CMapView2D *pView)
{
	RemoveMRU(pView);
	MRU2DViews.AddToHead(pView);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//-----------------------------------------------------------------------------
void CMapDoc::RemoveMRU(CMapView2D *pView)
{
	MRU2DViews.FindAndRemove(pView);
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of all Edit menu items and toolbar buttons.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateEditFunction(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable( ( m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL ) &&
		            !GetMainWnd()->IsShellSessionActive() );
}


//-----------------------------------------------------------------------------
// Purpose: This is called for each doc when the texture application mode changes.
// Input  : bApplicator - TRUE if entering texture applicator mode, FALSE if
//			leaving texture applicator mode.
//-----------------------------------------------------------------------------
void CMapDoc::UpdateForApplicator(BOOL bApplicator)
{
	if (bApplicator)
	{
		//
		// Build a list of all selected solids.
		//
		CMapObjectList Solids;
		const CMapObjectList *pSelList = m_pSelection->GetList();
		for (int i = 0; i < pSelList->Count(); i++)
		{
			CMapClass *pObject = pSelList->Element(i);
						
			CMapSolid *pSolid = dynamic_cast<CMapSolid*>(pObject);

			if (pSolid != NULL)
			{
				Solids.AddToTail(pSolid);
			}

			pObject->EnumChildren((ENUMMAPCHILDRENPROC)AddLeavesToListCallback, (DWORD)&Solids, MAPCLASS_TYPE(CMapSolid));
		}

		//
		// Clear the object selection.
		//
		SelectObject(NULL, scClear);

		//
		// Select all faces of all solids that were selected originally. Disable updates
		// in the face properties dialog beforehand or this could take a LONG time.
		//
		HCURSOR hCursorOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
		GetMainWnd()->m_pFaceEditSheet->EnableUpdate( false );

		bool bFirst = true;
		
		FOR_EACH_OBJ( Solids, pos )
		{
			CMapClass *pObject = Solids.Element(pos);
			CMapSolid *pSolid = dynamic_cast<CMapSolid*>(pObject);
			Assert(pSolid != NULL);

			if (pSolid != NULL)
			{
				SelectFace(pSolid, -1, scSelect | (bFirst ? scClear : 0));
				bFirst = false;
			}
		}

		GetMainWnd()->m_pFaceEditSheet->EnableUpdate( true );
		SetCursor(hCursorOld);
	}
	else
	{
		//
		// Remove all faces from the dialog's list and update their selection state to be
		// not selected, then update the display.
		//
		GetMainWnd()->m_pFaceEditSheet->ClickFace( NULL, -1, CFaceEditSheet::cfClear );
		UpdateStatusbar();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSolid - 
//			iFace - 
//			cmd - 
//-----------------------------------------------------------------------------
void CMapDoc::SelectFace(CMapSolid *pSolid, int iFace, int cmd)
{
	bool bFirst = true;
	if(iFace == -1 && pSolid)
	{
		// Get draw solid/disp mask.
		bool bDispSolidMask = CMapDoc::GetActiveMapDoc()->IsDispSolidDrawMask() && pSolid->HasDisp();

		// select entire object
		int nFaces = pSolid->GetFaceCount();
		for(int i = 0; i < nFaces; i++)
		{
			if ( bDispSolidMask )
			{
				CMapFace *pFace = pSolid->GetFace( i );
				if( pFace && pFace->HasDisp() )
				{
					SelectFace(pSolid, i, cmd);
					if ( bFirst )
					{
						cmd &= ~scClear;
						bFirst = false;
					}
				}
			}
			else
			{
				SelectFace(pSolid, i, cmd);
				if ( bFirst )
				{	
					cmd &= ~scClear;
					bFirst = false;
				}
			}
		}

		return;
	}

	CFaceEditSheet *pSheet = GetMainWnd()->m_pFaceEditSheet;
	UINT uFaceCmd = 0;
	
	if(cmd & scClear)
	{
		uFaceCmd |= CFaceEditSheet::cfClear;
	}
	if(cmd & scToggle)
	{
		uFaceCmd |= CFaceEditSheet::cfToggle;
	}
	if(cmd & scSelect)
	{
		uFaceCmd |= CFaceEditSheet::cfSelect;
	}
	if(cmd & scUnselect)
	{
		uFaceCmd |= CFaceEditSheet::cfUnselect;
	}

	//
	// Change the click mode to ModeSelect if the scNoLift flag is set.
	//
	int nClickMode;

	if (cmd & scNoLift) // dvs: this is lame
	{
		nClickMode = CFaceEditSheet::ModeSelect;
	}
	else
	{
		nClickMode = -1;
	}

	//
	// Check the current click mode and perform the texture application if appropriate.
	//
	BOOL bApply = FALSE;

	if (!(cmd & scNoApply))
	{
		int iFaceMode = pSheet->GetClickMode(); 
		bApply = ( ( iFaceMode == CFaceEditSheet::ModeApply ) || ( iFaceMode == CFaceEditSheet::ModeApplyAll ) );

		if (bApply && pSolid)
		{
			GetHistory()->MarkUndoPosition(NULL, "Apply texture");
			GetHistory()->Keep(pSolid);
		}
	}

	pSheet->ClickFace( pSolid, iFace, uFaceCmd, nClickMode );

	// update display?
	if (bApply)
	{
		UpdateStatusbar();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapInformation(void)
{
	CMapInfoDlg dlg(m_pWorld);
	dlg.DoModal();
}


//-----------------------------------------------------------------------------
// Purpose: Forces a render of all the 3D views. Called from OnIdle to render
//			the 3D views.
//-----------------------------------------------------------------------------
void CMapDoc::SetActiveView(CMapView *pViewActivate)
{
	POSITION p = GetFirstViewPosition();

	while (p)
	{
		CMapView *pView = dynamic_cast<CMapView*>(GetNextView(p));

		if ( pView )
		{
			pView->ActivateView(pView == pViewActivate);
		}
	}
}


//-----------------------------------------------------------------------------
// releases video memory
//-----------------------------------------------------------------------------
void CMapDoc::ReleaseVideoMemory( )
{
	POSITION p = GetFirstViewPosition();

	while (p)
	{
		CMapView3D *pView = dynamic_cast<CMapView3D*>(GetNextView(p));
		if (pView)
		{
			pView->ReleaseVideoMemory();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vi - 
//-----------------------------------------------------------------------------
void CMapDoc::SetView2dInfo(VIEW2DINFO& vi)
{
	POSITION p = GetFirstViewPosition();
	while(p)
	{
		CView *pView = GetNextView(p);
		if(!pView->IsKindOf(RUNTIME_CLASS(CMapView2D)))
			continue;

		CMapView2D *pView2D = (CMapView2D*) pView;

		// set zoom value
		if(vi.wFlags & VI_ZOOM)
		{
			pView2D->SetZoom(vi.fZoom);
		}

		// center on point
		if(vi.wFlags & VI_CENTER)
		{
			pView2D->CenterView(&vi.ptCenter);
		}

		pView2D->UpdateView( MAPVIEW_UPDATE_OBJECTS );
	}
}

void CMapDoc::SetViewLogicalInfo(VIEW2DINFO& vi)
{
	POSITION p = GetFirstViewPosition();
	while(p)
	{
		CView *pView = GetNextView(p);
		if(!pView->IsKindOf(RUNTIME_CLASS(CMapViewLogical)))
			continue;

		CMapViewLogical *pViewLogical = (CMapViewLogical*) pView;

		// set zoom value
		if(vi.wFlags & VI_ZOOM)
		{
			pViewLogical->SetZoom(vi.fZoom);
		}

		// center on point
		if(vi.wFlags & VI_CENTER)
		{
			pViewLogical->CenterView(&vi.ptCenter);
		}

		pViewLogical->UpdateView( MAPVIEW_UPDATE_OBJECTS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vec - 
//-----------------------------------------------------------------------------
void CMapDoc::CenterViewsOn(const Vector &vec)
{
	Center2DViewsOn(vec);
	Center3DViewsOn(vec);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::Center2DViewsOn(const Vector &vec)
{
	VIEW2DINFO vi;
	vi.wFlags = VI_CENTER;
	vi.ptCenter = vec;

	SetView2dInfo(vi);
}


//-----------------------------------------------------------------------------
// Purpose: Centers the 3D views on the given point.
//-----------------------------------------------------------------------------
void CMapDoc::Center3DViewsOn( const Vector &vPos )
{
	POSITION p = GetFirstViewPosition();
	while(p)
	{
		CView *pView = GetNextView(p);
		if(!pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
			continue;

		CMapView3D *pView3D = (CMapView3D*) pView;
	
		//what's happening here?
		Vector vForward;
		pView3D->GetCamera()->GetViewForward( vForward );

		pView3D->SetCamera( vPos - Vector( 0, 100, 0 ), vPos );

		pView3D->UpdateView( MAPVIEW_UPDATE_OBJECTS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets 3D views with the input position and angle vectors.
//-----------------------------------------------------------------------------
void CMapDoc::Set3DViewsPosAng( const Vector &vPos, const Vector &vAng )
{
	POSITION p = GetFirstViewPosition();
	while(p)
	{
		CView *pView = GetNextView(p);
		if(!pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
			continue;

		CMapView3D *pView3D = (CMapView3D*) pView;
		QAngle angles;
		angles.x = vAng.x;
		angles.y = vAng.y;
		angles.z = vAng.z;
		Vector forward;
		AngleVectors( angles, &forward );		
		pView3D->SetCamera( vPos, forward + vPos );	
		

		pView3D->UpdateView( MAPVIEW_UPDATE_OBJECTS );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::CenterLogicalViewsOn(const Vector2D &vecLogical)
{
	VIEW2DINFO vi;
	vi.wFlags = VI_CENTER;
	vi.ptCenter = Vector( vecLogical.x, vecLogical.y, 0.0f );

	SetViewLogicalInfo(vi);
}


//-----------------------------------------------------------------------------
// Purpose: Centers the 2D views on selected objects.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewCenterOnSelection(void)
{
	Center2DViewsOnSelection();
	CenterLogicalViewsOnSelection();
}


//-----------------------------------------------------------------------------
// Purpose: Centers the 3D views on selected objects.
//-----------------------------------------------------------------------------
void CMapDoc::OnViewCenter3DViewsOnSelection(void)
{
	Center3DViewsOnSelection();
}


//-----------------------------------------------------------------------------
// Purpose: Hollows selected solids by carving them with a scaled version of
//			themselves.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsHollow(void)
{
	//
	// Confirm the operation if there is more than one object selected.
	//
	if ( m_pSelection->GetCount() > 1)
	{
		if (AfxMessageBox("Do you want to turn each of the selected solids into a hollow room?", MB_YESNO) == IDNO)
		{
			return;
		}
	}

	//
	// Prompt for the wall thickness, which is remembered from one hollow to another.
	//
	static int iWallWidth = 32;
	char szBuf[128];
	itoa(iWallWidth, szBuf, 10);
	CStrDlg dlg(CStrDlg::Spin, szBuf, "How thick do you want the walls? Use a negative number to hollow outward.", "Hammer");
	dlg.SetRange(-1024, 1024, 4);
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}
	iWallWidth = atoi(dlg.m_string);

	if (abs(iWallWidth) < 2)
	{
		AfxMessageBox("The width of the walls must be less than -1 or greater than 1.");
		return;
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition(pSelList, "Hollow");
	
	//
	// Build a list of all solids in the selection.
	//
	CMapObjectList SelectedSolids;
	
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pObject);
		if (pSolid != NULL)
		{
			SelectedSolids.AddToTail(pSolid);
		}

		EnumChildrenPos_t pos2;
		CMapClass *pChild = pObject->GetFirstDescendent(pos2);
		while (pChild != NULL)
		{
			pSolid = dynamic_cast <CMapSolid *> (pChild);
			if (pSolid != NULL)
			{
				SelectedSolids.AddToTail(pSolid);
			}
			pChild = pObject->GetNextDescendent(pos2);
		}
	}

	//
	// Carve every solid in the selection with a scaled copy of itself. This accomplishes
	// the goal of hollowing them.
	//
	CMapSolid ScaledCopy;
	
	FOR_EACH_OBJ( SelectedSolids, pos )
	{
		CMapSolid *pSelectedSolid = (CMapSolid *)SelectedSolids.Element(pos);
		CMapClass *pDestParent = pSelectedSolid->GetParent();

		GetHistory()->Keep(pSelectedSolid);

		ScaledCopy.CopyFrom(pSelectedSolid, false);
		ScaledCopy.SetParent(NULL);

		//
		// Get bounds of the solid to be hollowed and calculate scaling required to
		// reduce by iWallWidth.
		//
		BoundBox box;
		Vector ptCenter;
		Vector vecScale;

		pSelectedSolid->GetRender2DBox(box.bmins, box.bmaxs);
		for (int i = 0; i < 3; i++)
		{
			float fHalf = (box.bmaxs[i] - box.bmins[i]) / 2;
			vecScale[i] = (fHalf - iWallWidth) / fHalf;
		}

		ScaledCopy.GetBoundsCenter(ptCenter);
		ScaledCopy.TransScale(ptCenter, vecScale);
		
		//
		// Set up the operands for the subtraction operation.
		//
		CMapSolid *pSubtractWith;
		CMapSolid *pSubtractFrom;

		if (iWallWidth > 0)
		{
			pSubtractFrom = pSelectedSolid;
			pSubtractWith = &ScaledCopy;
		}
		//
		// Negative wall widths reverse the subtraction.
		//
		else
		{
			pSubtractFrom = &ScaledCopy;
			pSubtractWith = pSelectedSolid;
		}

		//
		// Perform the subtraction. If the two objects intersected...
		//
		CMapObjectList Outside;
		if (pSubtractFrom->Subtract(NULL, &Outside, pSubtractWith))
		{
			//
			// If there were pieces outside the 'subtract with' object...
			//
			if (Outside.Count() > 0)
			{
				CMapClass *pResult = NULL;

				//
				// If the subtraction resulted in more than one object, create a group
				// to place the results in.
				//
				if (Outside.Count() > 1)
				{
					pResult = (CMapClass *)(new CMapGroup);
					
					FOR_EACH_OBJ( Outside, pos2 )
					{
						CMapClass *pTemp = Outside.Element(pos2);
						pResult->AddChild(pTemp);
					}
				}
				//
				// Otherwise, the results are the single object.
				//
				else if (Outside.Count() == 1)
				{
					pResult = Outside[0];
				}

				//
				// Replace the current solid with the subtraction results.
				//
				DeleteObject(pSelectedSolid);
				AddObjectToWorld(pResult, pDestParent);
				GetHistory()->KeepNew(pResult);
			}
		}
	}

	// Objects in selection no longer exist.
	m_pSelection->SelectObject( NULL, scClear );

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnEditPastespecial(void)
{
	CPasteSpecialDlg dlg(GetMainWnd(), &GetHammerClipboard()->Bounds);
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}
	dlg.SaveToIni();

	BeginWaitCursor();
	GetHistory()->MarkUndoPosition( m_pSelection->GetList(), "Paste");

	// first, clear selection so we can select all pasted objects
	SelectObject(NULL, scClear|scSaveChanges );

	//
	// Build a paste translation.
	//
	Vector vecPasteOffset( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );

	if (!dlg.m_bCenterOriginal)
	{
		GetBestPastePoint(vecPasteOffset);
		vecPasteOffset -= GetHammerClipboard()->vecOriginalCenter;
	}
	else
	{
		vecPasteOffset[0] = dlg.m_iOffsetX;
		vecPasteOffset[1] = dlg.m_iOffsetY;
		vecPasteOffset[2] = dlg.m_iOffsetZ;
	}

	//
	// Build the paste rotation angles.
	//
	QAngle vecPasteAngles;
	vecPasteAngles[0] = dlg.m_fRotateX;
	vecPasteAngles[1] = dlg.m_fRotateY;
	vecPasteAngles[2] = dlg.m_fRotateZ;

	CMapWorld *pWorld = GetActiveWorld();

	CMapClass *pParent = NULL;
	CMapGroup *pGroup = NULL;

	if (dlg.m_bGroup)
	{
		pGroup = new CMapGroup;
		pParent = (CMapClass *)pGroup;
		AddObjectToWorld(pGroup, pWorld);
	}
    	
	Options.SetLockingTextures(TRUE);

	bool bMakeNamesUnique = (dlg.m_bMakeEntityNamesUnique == TRUE);
	const char *pszPrefix = (dlg.m_bAddPrefix == TRUE) ? dlg.m_strPrefix : "";

	for (int i = 0; i < dlg.m_iCopies; i++)
	{
		//
		// Paste the objects with the current offset and rotation.
		//
		Paste(GetHammerClipboard()->Objects, GetHammerClipboard()->pSourceWorld, pWorld, vecPasteOffset, vecPasteAngles, pParent, bMakeNamesUnique, pszPrefix );

		//
		// Increment the paste offset.
		//
		vecPasteOffset[0] += dlg.m_iOffsetX;
		vecPasteOffset[1] += dlg.m_iOffsetY;
		vecPasteOffset[2] += dlg.m_iOffsetZ;

		//
		// Increment the paste angles.
		//
		vecPasteAngles[0] += dlg.m_fRotateX;
		vecPasteAngles[1] += dlg.m_fRotateY;
		vecPasteAngles[2] += dlg.m_fRotateZ;
	}

	//
	// If we pasted into a group, keep the group now.
	//
	if (pGroup != NULL)
	{
		GetHistory()->KeepNew(pGroup);
		SelectObject(pGroup, scSelect);
	}

	m_pToolManager->SetTool(TOOL_POINTER);

	SetModifiedFlag();
	EndWaitCursor();
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Edit | Paste Special menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateEditPastespecial(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable((GetHammerClipboard()->Objects.Count() != 0) && !GetMainWnd()->IsShellSessionActive());
}


//-----------------------------------------------------------------------------
// Purpose: Does the undo or redo and restores the selection to the state at
//			which it was marked in the Undo system.
// Input  : nID - ID_EDIT_UNDO or ID_EDIT_REDO.
// Output : Always returns TRUE.
//-----------------------------------------------------------------------------
BOOL CMapDoc::OnUndoRedo(UINT nID)
{
	//
	// Morph operations are not undo-friendly because they use a non-CMapClass
	// derived object (SSolid) to store intermediate object state.
	//
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		AfxMessageBox("You must exit morph mode to undo changes you've made.");
		return(TRUE);
	}

	// Do the undo/redo.
	
	CMapObjectList NewSelection;
	if (nID == ID_EDIT_UNDO)
	{
		m_pUndo->Undo(&NewSelection);
	}
	else
	{
		m_pRedo->Undo(&NewSelection);
	}

	// Change the selection to the objects that the undo system says
	// should be selected now. Don't save changes and create new undo entrys
	SelectObjectList( &NewSelection, scClear|scSelect );
	
	m_pSelection->RemoveDead();
	CMapClass::UpdateAllDependencies(NULL);

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Undo/Redo menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateUndoRedo(CCmdUI *pCmdUI) 
{
	CHistory *pHistory = (pCmdUI->m_nID == ID_EDIT_UNDO) ? m_pUndo : m_pRedo;
	char *pszAction = (pCmdUI->m_nID == ID_EDIT_UNDO) ? "Undo" : "Redo";
	char *pszHotkey = (pCmdUI->m_nID == ID_EDIT_UNDO) ? "Ctrl+Z" : "Ctrl+Y";

	if (pHistory->IsUndoable())
	{
		pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());

		CString str;
		str.Format("%s %s\t%s", pszAction, pHistory->GetCurTrackName(), pszHotkey);
		pCmdUI->SetText(str);
	}
	else
	{
		CString str;
		str.Format("Can't %s\t%s", pszAction, pszHotkey);
		pCmdUI->SetText(str);
		pCmdUI->Enable(FALSE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			pWorld - 
//-----------------------------------------------------------------------------
bool CMapDoc::ExpandTargetNameKeywords(char *szNewTargetName, const char *szOldTargetName, CMapWorld *pWorld)
{
	const char *pszKeyword = strstr(szOldTargetName, "&i");
	if (pszKeyword != NULL)
	{
		char szPrefix[100];
		char szSuffix[100];

		strncpy(szPrefix, szOldTargetName, pszKeyword - szOldTargetName);
		szPrefix[pszKeyword - szOldTargetName] = '\0';

		strcpy(szSuffix, pszKeyword + 2);

		int nHighestIndex = 0;

		const CMapEntityList *pEntityList = pWorld->EntityList_GetList();
		
		FOR_EACH_OBJ( *pEntityList, pos )
		{
			CMapEntity *pEntity = pEntityList->Element( pos );
			const char *pszTargetName = pEntity->GetKeyValue("targetname");

			//
			// If this entity has a targetname, check to see if it is of the
			// form <prefix><number><suffix>. If so, it must be counted as
			// we search for the highest instance number. 
			//
			if (pszTargetName != NULL)
			{
				char szTemp[MAX_PATH];
				strcpy(szTemp, pszTargetName);

				int nPrefixLen = strlen(szPrefix);
				int nSuffixLen = strlen(szSuffix);

				int nFullLen = strlen(szTemp);

				//
				// It must be longer than the prefix and the suffix combined to be
				// of the form <prefix><number><suffix>.
				//
				if (nFullLen > nPrefixLen + nSuffixLen)
				{
					char *pszTempSuffix = szTemp + nFullLen - nSuffixLen;

					//
					// If the prefix and the suffix match ours, extract the instance number
					// from between them and check it against our highest instance number.
					//
					if ((strnicmp(szTemp, szPrefix, nPrefixLen) == 0) && (stricmp(pszTempSuffix, szSuffix) == 0))
					{
						*pszTempSuffix = '\0';

						bool bAllDigits = true;
						for (int i = 0; i < (int)strlen(&szTemp[nPrefixLen]); i++)
						{
							if (!V_isdigit(szTemp[nPrefixLen + i]))
							{
								bAllDigits = false;
								break;
							}
						}

						if (bAllDigits)
						{
							int nIndex = atoi(&szTemp[nPrefixLen]);
							if (nIndex > nHighestIndex)
							{
								nHighestIndex = nIndex;
							}
						}
					}
				}
			}
		}

		sprintf(szNewTargetName, "%s%d%s", szPrefix, nHighestIndex + 1, szSuffix);
	
		return(true);
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Expands keywords in pObject, if there are any. Returns whether
//			keyword expansion was performed.
// Input  : pObject - 
//			pWorld - 
//-----------------------------------------------------------------------------
bool CMapDoc::DoExpandKeywords(CMapClass *pObject, CMapWorld *pWorld, char *szOldKeyword, char *szNewKeyword)
{
	CEditGameClass *pEditGameClass = dynamic_cast <CEditGameClass *>(pObject);
	if (pEditGameClass != NULL)
	{
		const char *pszOldTargetName = pEditGameClass->GetKeyValue("targetname");
		if (pszOldTargetName != NULL)
		{
			char szNewTargetName[MAX_PATH];
			if (ExpandTargetNameKeywords(szNewTargetName, pszOldTargetName, pWorld))
			{
				strcpy(szOldKeyword, pszOldTargetName);
				strcpy(szNewKeyword, szNewTargetName);
				pEditGameClass->SetKeyValue("targetname", szNewTargetName);
				return(true);
			}
		}
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Gets this object's name if it has one. Returns false if it has none.
//-----------------------------------------------------------------------------
static bool GetName( CMapClass *pObject, char *szName )
{
	CEditGameClass *pEditGameClass = dynamic_cast <CEditGameClass *>( pObject );
	if ( pEditGameClass == NULL )
		return false;

	const char *pszName = pEditGameClass->GetKeyValue( "targetname" );
	if ( !pszName )
		return false;

	Q_strcpy( szName, pszName );

	return true;
}

static const char *CopyName( const char * pszString )
{
	int length = Q_strlen(pszString)+1;
	char *pNewString = new char[length];
	Q_memcpy( pNewString, pszString, length );
	return pNewString;
}

static bool FindName( CUtlVector<const char*>*pList, const char * pszString )
{
	for ( int i=0; i<pList->Count(); i++ )
	{
		if ( Q_stricmp( pszString, pList->Element(i)) == 0 )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Renames all named entities in the tree pointed to by pRoot.
//		pRoot - Points to a tree of objects.
//		pWorld - If making the names unique, the world that they should be unique within.
//		bMakeUnique - Whether to guarantee that the names are unique in the world.
//			If necessary, numbers will be appended to make the names unique.
//		szPrefix - A string to prepend to all named entities in the tree.
//-----------------------------------------------------------------------------
void CMapDoc::RenameEntities( CMapClass *pRoot, CMapWorld *pWorld, bool bMakeUnique, const char *szAddPrefix )
{
	if ( !bMakeUnique && ( !szAddPrefix || ( szAddPrefix[0] == '\0' ) ) )
		return;

	CUtlVector<const char*>  oldNames;
	char szName[MAX_PATH];

	pRoot->FindTargetNames( oldNames );
	// find all names we have to replace
	if ( GetName( pRoot, szName ) )
	{
		oldNames.AddToTail( CopyName(szName) );
	}

	// Expand keywords in this object's children as well.

	EnumChildrenPos_t pos;
	CMapClass *pChild = pRoot->GetFirstDescendent( pos );
	while ( pChild != NULL )
	{
		pChild->FindTargetNames( oldNames );
		if ( GetName(pChild, szName ) )
		{
			if ( !FindName( &oldNames, szName) )
			{
				oldNames.AddToTail( CopyName(szName) );
			}
		}
		pChild = pRoot->GetNextDescendent( pos );
	}

	for ( int i=0; i<oldNames.Count(); i++ )
	{
		if ( pWorld->GenerateNewTargetname( oldNames[i], szName, sizeof( szName ), bMakeUnique, szAddPrefix, pRoot ) )
		{
			pRoot->ReplaceTargetname( oldNames[i], szName );
		}
	}

	oldNames.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the children of the given object and expands any keywords
//			in the object's properties.
// Input  : pObject - 
//			pWorld - 
//-----------------------------------------------------------------------------
void CMapDoc::ExpandObjectKeywords(CMapClass *pObject, CMapWorld *pWorld)
{
	char szOldName[MAX_PATH];
	char szNewName[MAX_PATH];

	if (DoExpandKeywords(pObject, pWorld, szOldName, szNewName))
	{
		pObject->ReplaceTargetname(szOldName, szNewName);
	}

	//
	// Expand keywords in this object's children as well.
	//
	EnumChildrenPos_t pos;
	CMapClass *pChild = pObject->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		if (DoExpandKeywords(pChild, pWorld, szOldName, szNewName))
		{
			pObject->ReplaceTargetname(szOldName, szNewName);
		}

		pChild = pObject->GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Selects the next object by depth in the 3D view.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditSelnext(void)
{
	m_pSelection->SetCurrentHit(hitNext);
	
}


//-----------------------------------------------------------------------------
// Purpose: Selects the previous object by depth in the 3D view.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditSelprev(void)
{
	m_pSelection->SetCurrentHit(hitPrev);
}


//-----------------------------------------------------------------------------
// Purpose: Selects the next object by depth
//-----------------------------------------------------------------------------
void CMapDoc::OnEditSelnextCascading(void)
{
	m_pSelection->SetCurrentHit(hitNext, true );
}


//-----------------------------------------------------------------------------
// Purpose: Selects the previous object by depth
//-----------------------------------------------------------------------------
void CMapDoc::OnEditSelprevCascading(void)
{
	m_pSelection->SetCurrentHit(hitPrev, true );
}


//-----------------------------------------------------------------------------
// Moves selected objects close to each other in logical space 
//----------------------------------------------------------------------------- 
void CMapDoc::OnLogicalMoveBlock(void)
{
	Vector2D vecLogicalCenter;
	if ( !m_pSelection->GetLogicalBoundsCenter( vecLogicalCenter ) )
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	if ( pSelList->Count() <= 1 )
		return;

	GetHistory()->MarkUndoPosition( pSelList, "Move Block" );
	GetHistory()->Keep( pSelList );

	// Lay out in a squarish region that has the same center as the current one
	int nCount = pSelList->Count();
	int nDim = sqrt( (float)nCount );
	if ( nDim * nDim < nCount )
	{
		++nDim;
	}

	CMapViewLogical *pCurrentView = NULL;
	if ( GetMainWnd()->GetActiveFrame() )
	{
		pCurrentView = dynamic_cast<CMapViewLogical*>( GetMainWnd()->GetActiveFrame()->GetActiveView() );
	}

	bool bCenterView;
	Vector2D vecPositionCenter;
	if ( pCurrentView )
	{
		Vector vecCenter( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
		pCurrentView->GetCenterPoint( vecCenter );
		vecPositionCenter = vecCenter.AsVector2D();
		bCenterView = false;
	}
	else
	{
		vecPositionCenter = vecLogicalCenter;
		bCenterView = true;
	}
	vecPositionCenter.x -= (nDim / 2.0f) * LOGICAL_SPACING;
	vecPositionCenter.y -= (nDim / 2.0f) * LOGICAL_SPACING;
	  
 	for ( int i = 0; i < pSelList->Count(); ++i )
	{
		CMapClass *pClass = pSelList->Element( i );
		if ( !pClass->IsLogical() )
			continue;

		int x = i % nDim;
		int y = ( i / nDim );

		Vector2D newLogicalCenter;
		newLogicalCenter.x = vecPositionCenter.x + x * LOGICAL_SPACING;
		newLogicalCenter.y = vecPositionCenter.y + y * LOGICAL_SPACING;
		pClass->SetLogicalPosition( newLogicalCenter );
	}

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS | MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_ONLY_LOGICAL );

	m_pSelection->SetBoundsDirty();

	if ( bCenterView )
	{
		CenterLogicalViewsOnSelection();
	}
}


//-----------------------------------------------------------------------------
// Select all entities connected to outputs of all entities in the selection list recursively
//----------------------------------------------------------------------------- 
void CMapDoc::OnLogicalSelectAllCascading(void)
{
	if ( m_pSelection->IsEmpty() )
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition( pSelList, "Select All Cascading" );

	CUtlRBTree< CMapClass*, unsigned short > list( 0, 0, DefLessFunc( CMapClass* ) );
	for ( int i = 0; i < pSelList->Count(); ++i )
	{
		CMapClass *pMapClass = pSelList->Element(i);
		list.InsertIfNotFound( pMapClass );
		BuildCascadingSelectionList( pMapClass, list, true );
	}

	for ( unsigned short h = list.FirstInorder(); h != list.InvalidIndex(); h = list.NextInorder(h) )
	{
		SelectObject( list[h], scSelect );
	}
}

//-----------------------------------------------------------------------------
// Add all entities connected to inputs of all entities in the selection list recursively
//----------------------------------------------------------------------------- 

void CMapDoc::AddConnectedNodes( CMapClass *pObject, CUtlRBTree< CMapClass*, unsigned short >& visited )
{
	// Make sure we havent visited this entity before
	if ( visited.Find( pObject ) == visited.InvalidIndex() )
	{
		// Is this node actually a visible entity?
		if ( pObject->IsLogical() && pObject->IsVisibleLogical() )
		{
			// Is this node NOT a group entity
			if ( !pObject->IsGroup() )
			{
				// Mark this entity visited
				visited.Insert( pObject );

				// See if this class has any connections
				CEditGameClass *pClass = dynamic_cast< CEditGameClass * >( pObject );
				if ( pClass )
				{
					// Iterate through each of the upstream connections
					int nCount = pClass->Upstream_GetCount();
					for ( int i = 0; i < nCount; ++i )
					{
						CEntityConnection *pConn = pClass->Upstream_Get( i );

						// Iterate through the source entities on this connection
						FOR_EACH_OBJ( *pConn->GetSourceEntityList(), pos )
						{	
							CMapEntity *pEntity = pConn->GetSourceEntityList()->Element( pos );
							// Don't bother recursing back into this current node
							if ( pEntity != pObject )
							{
								// Recurse to the adjacent source entity
								AddConnectedNodes( pEntity, visited );
							}
						}
					}

					// Iterate through each of the downstream connections
					nCount = pClass->Connections_GetCount();
					for ( int i = 0; i < nCount; ++i )
					{
						CEntityConnection *pConn = pClass->Connections_Get( i );

						// Iterate through the target entities on this connection
						FOR_EACH_OBJ( *pConn->GetTargetEntityList(), pos )
						{	
							CMapEntity *pEntity = pConn->GetTargetEntityList()->Element( pos );
							// Don't bother recursing back into this current node
							if ( pEntity != pObject )
							{
								// Recurse to the adjacent target entity
								AddConnectedNodes( pEntity, visited );
							}
						}
					}
				}
			}

       		// Recurse into any children and add them as well
			const CMapObjectList *pChildren = pObject->GetChildren();
			FOR_EACH_OBJ( *pChildren, pos )
			{
				AddConnectedNodes( pChildren->Element(pos), visited );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Select all entities connected to outputs of all entities in the selection list recursively
//----------------------------------------------------------------------------- 

void CMapDoc::OnLogicalSelectAllConnected(void)
{
	const CMapObjectList *pSelList = m_pSelection->GetList();
	int nSelected = pSelList->Count();
	if ( nSelected )
	{
		GetHistory()->MarkUndoPosition( pSelList, "Select All Connected" );

		CUtlRBTree< CMapClass*, unsigned short > visited( 0, 0, DefLessFunc( CMapClass* ) );

		for ( int i = 0; i < nSelected; ++i )
		{
			CMapClass *pMapClass = pSelList->Element(i);
			AddConnectedNodes( pMapClass, visited );
		}

		for ( unsigned short h = visited.FirstInorder(); h != visited.InvalidIndex(); h = visited.NextInorder(h) )
		{
			SelectObject( visited[h], scSelect );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds all selected or deselected objects to a new VisGroup and hides
//			the group.
//-----------------------------------------------------------------------------
BOOL CMapDoc::OnViewHideObjects(UINT nID)
{
	bool bSelected = (nID == ID_VIEW_HIDESELECTEDOBJECTS);

	if ( m_pSelection->IsEmpty() )
	{
		return TRUE;
	}

	//
	// Build a list of eligible selected objects.
	//
	CMapObjectList Objects;
	GetChildrenToHide(m_pWorld, bSelected, Objects);
	
	int nOriginalCount = Objects.Count();
	for (int pos = Objects.Count()-1; pos>=0; pos --)
	{
		CMapClass *pObject = Objects.Element(pos);
		if (!VisGroups_ObjectCanBelongToVisGroup(pObject))
		{
			Objects.Remove(pos);
		}
	}
	int nFinalCount = Objects.Count();

	//
	// If no eligible selected objects were found, exit.
	//
	if (!nFinalCount)
	{
		CString str;
		str.Format("There are no eligible %sselected objects. The only objects\n"
					"that can be put in a Visible Group are objects that are not\n"
					"part of an entity.", bSelected ? "" : "un");
	
		AfxMessageBox(str);
		return TRUE;
	}
	else if (nFinalCount < nOriginalCount)
	{
		AfxMessageBox("Some objects could not put in the new Visible Group because\n"
						"they are part of an entity.");
	}
	
	ShowNewVisGroupsDialog(Objects, bSelected);
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Fills out a list with all the children of this object that:
//
//			A) Are visible
//			B) Match the bSelected criteria
//			C) Whose children all match the bSelected criteria
//			D) Have no hidden children
//
// Output : Returns true if all top-level children satisfied the above criteria, false if not.
//-----------------------------------------------------------------------------
bool CMapDoc::GetChildrenToHide(CMapClass *pObject, bool bSelected, CMapObjectList &List)
{
	int nAddedCount = 0;

	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass *pChild = pChildren->Element(pos);
		CMapGroup *pGroup = dynamic_cast<CMapGroup *>(pChild);
		CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pChild);
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pChild);
	
		if (pGroup || pSolid || pEntity)
		{
			if (pChild->IsVisible())
			{
				pEntity = dynamic_cast<CMapEntity *>(pChild);
				if (pGroup || (pEntity && pEntity->IsSolidClass()))
				{
					// This child is a group or a solid entity -- check all its children.
					CMapObjectList ChildList;
					if (GetChildrenToHide(pChild, bSelected, ChildList))
					{
						// All this child's children match the criteria, so add the child to the list.
						List.AddToTail(pChild);
						nAddedCount++;
					}
					else if (ChildList.Count())
					{
						// Some of this child's children satisfy the criteria, or have descendents that do.
						// Add the children to the list.
						List.AddVectorToTail(ChildList);
					}
					else
					{
						// None of this child's children satisfy the criteria, so skip this child.
					}
				}
				else if (bSelected == pChild->IsSelected())
				{
					// Unselected point entity or solid, add it to the list.
					List.AddToTail(pChild);
					nAddedCount++;
				}
			}
		}
		else
		{
			// Don't add helpers, but count them anyway.
			nAddedCount++;
		}
	}

	return nAddedCount == pObject->GetChildCount();
}


//-----------------------------------------------------------------------------
// Purpose: Reflects the current handle mode of the selection tool.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewShowHelpers(CCmdUI *pCmdUI) 
{
//	if (pCmdUI->m_nID == ID_VIEW_SELECTION_ONLY)
//	{
//		pCmdUI->SetCheck(m_pToolSelection->GetHandleMode() == HandleMode_SelectionOnly);
//	}
//	else if (pCmdUI->m_nID == ID_VIEW_HELPERS_ONLY)
//	{
		pCmdUI->SetCheck( Options.GetShowHelpers() );
//	}
//	else if (pCmdUI->m_nID == ID_VIEW_SELECTION_AND_HELPERS)
//	{
//		pCmdUI->SetCheck(m_pToolSelection->GetHandleMode() == HandleMode_Both);
//	}

	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}

//-----------------------------------------------------------------------------
// Purpose: Reflects the current 2D model drawing
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewShowModelsIn2D(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck( Options.view2d.bDrawModels?1:0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}

void CMapDoc::OnViewShowModelsIn2D(void)
{
	Options.view2d.bDrawModels = !Options.view2d.bDrawModels;
	UpdateVisibilityAll();
}

void CMapDoc::OnViewShowHelpers(void)
{
	// FIXME: this only sets the handle mode for the active document's selection tool!
	Options.SetShowHelpers(!Options.GetShowHelpers());
	UpdateVisibilityAll();
	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of 3D model fade preview.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewPreviewModelFade(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck( Options.view3d.bPreviewModelFade ? 1 : 0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}

void CMapDoc::OnViewPreviewModelFade(void)
{
	Options.view3d.bPreviewModelFade = !Options.view3d.bPreviewModelFade;
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
}

//-----------------------------------------------------------------------------
// Purpose: Manages the state of 3D model fade preview.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateCollisionWireframe(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck( Options.general.bShowCollisionModels ? 1 : 0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}

void CMapDoc::OnCollisionWireframe(void)
{
	Options.general.bShowCollisionModels = !Options.general.bShowCollisionModels;
	UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}

//-----------------------------------------------------------------------------
// Purpose: Manages the state of 3D model fade preview.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateShowDetailObjects(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck( Options.general.bShowDetailObjects ? 1 : 0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}

void CMapDoc::OnShowDetailObjects(void)
{
	Options.general.bShowDetailObjects = !Options.general.bShowDetailObjects;
	UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}

void CMapDoc::OnShowNoDrawBrushes(void)
{
	Options.general.bShowNoDrawBrushes = !Options.general.bShowNoDrawBrushes;
	UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}

void CMapDoc::OnUpdateShowNoDrawBrushes(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck( Options.general.bShowNoDrawBrushes ? 1 : 0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the View | Hide Unselected menu item and toolbar button.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewHideUnselectedObjects(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapCheck(void)
{
	CMapCheckDlg::CheckForProblems(GetMainWnd());
}

void CMapDoc::OnMapDiff(void)
{
	CMapDiffDlg::MapDiff(GetMainWnd(), this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnViewShowconnections(void)
{
	bool bShow = CMapEntity::GetShowEntityConnections();
	CMapEntity::ShowEntityConnections(!bShow);

	UpdateAllViews( MAPVIEW_UPDATE_ONLY_2D );
}


//-----------------------------------------------------------------------------
// Purpose: Puts one of every point entity in the current FGD set in the current
//			map on a 128 grid.
//-----------------------------------------------------------------------------
void CMapDoc::OnMapEntityGallery(void)
{
	if (GetMainWnd()->MessageBox("This will place one of every possible point entity in the current map! Performing this operation in an empty map is recommended. Continue?", "Create Entity Gallery", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
	{
		int x = -1024;
		int y = -1024;

		CString str;

		int nCount = pGD->GetClassCount();
		for (int i = 0; i < nCount; i++)
		{
			GDclass *pc = pGD->GetClass(i);
			if (!pc->IsBaseClass())
			{
				if (!pc->IsSolidClass())
				{
					if (!pc->IsClass("worldspawn"))
					{
						CreateEntity(pc->GetName(), x, y, 0);
						x += 128;
						if (x > 1024)
						{
							x = -1024;
							y += 128;
						}
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateViewShowconnections(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(CMapEntity::GetShowEntityConnections());
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszFileName - 
//			nSize - 
//-----------------------------------------------------------------------------
bool GetSaveAsFilename(const char *pszBaseDir, char *pszFileName, int nSize)
{
	CString str;
	CFileDialog dlg(FALSE, NULL, str, OFN_LONGNAMES | OFN_NOCHANGEDIR |	OFN_HIDEREADONLY, "Valve Map Files (*.vmf)|*.vmf||");
	dlg.m_ofn.lpstrInitialDir = pszBaseDir;
	int nRet = dlg.DoModal();

	if (nRet != IDCANCEL)
	{
		str = dlg.GetPathName();
		
		if (str.Find('.') == -1)
		{
			str += ".vmf";
		}

		lstrcpyn(pszFileName, str, nSize);
		return(true);
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Takes the current selection and saves it as a prefab. The user is
//			prompted for a folder under the prefabs folder in which to place
//			the prefab.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsCreateprefab(void)
{
	if ( m_pSelection->IsEmpty() )
	{
		AfxMessageBox("This feature creates a prefab with the selected objects. You must select some objects before you can use it.", MB_ICONINFORMATION | MB_OK);
		return;
	}

	//
	// Get a file to save the prefab into. The first time through the default folder
	// is the prefabs folder.
	//
	static char szBaseDir[MAX_PATH] = "";
	if (szBaseDir[0] == '\0')
	{
		APP()->GetDirectory(DIR_PREFABS, szBaseDir);
	}

	char szFilename[MAX_PATH];
	if (!GetSaveAsFilename(szBaseDir, szFilename, sizeof(szFilename)))
	{
		return;
	}

	//
	// Save the default folder for next time.
	//
	strcpy(szBaseDir, szFilename);
	char *pch = strrchr(szBaseDir, '\\');
	if (pch != NULL)
	{
		*pch = '\0';
	}

	//
	// Create a prefab world to contain the selected items. Add the selected
	// items to the new world.
	//
	CMapWorld *pNewWorld = new CMapWorld( NULL );
	pNewWorld->SetTemporary(TRUE);
	const CMapObjectList *pSelList = m_pSelection->GetList();
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		CMapClass *pNew = pObject->Copy(false);

		// HACK: prune the object from the tree without doing any notification
		//       this prevents CopyChildrenFrom from updating the current world's culling tree
		pNew->SetParent(NULL);

		pNew->CopyChildrenFrom(pObject, false);
		pNewWorld->AddObjectToWorld(pNew);
	}
	pNewWorld->CalcBounds(TRUE);

	//
	// Create a prefab object and attach the world to it.
	//
	CPrefabVMF *pPrefab = new CPrefabVMF;
	pPrefab->SetWorld(pNewWorld);
	pPrefab->SetFilename(szFilename);

	//
	// Save the world to the chosen filename.
	//
	CChunkFile File;
	ChunkFileResult_t eResult = File.Open(szFilename, ChunkFile_Write);

	if (eResult == ChunkFile_Ok)
	{
		CSaveInfo SaveInfo;
		SaveInfo.SetVisiblesOnly(false);

		//
		// Write the map file version.
		//
		if (eResult == ChunkFile_Ok)
		{
			// HACK: make sure we save it as a prefab, not as a normal map
			bool bPrefab = m_bPrefab;
			m_bPrefab = true;
			eResult = SaveVersionInfoVMF(&File);
			m_bPrefab = bPrefab;
		}

		//
		// Save the world.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = pNewWorld->SaveVMF(&File, &SaveInfo, false);
		}
	}

	//
	// Try to locate the prefab library that corresponds to the folder where
	// we saved the prefab. If it doesn't exist, try refreshing the prefab library
	// list. Maybe the user just created the folder during this save.
	//
	CPrefabLibrary *pLibrary = CPrefabLibrary::FindOpenLibrary(szBaseDir);
	if (pLibrary == NULL)
	{
		delete pPrefab;

		//
		// This will take care of finding the prefab and adding it to the list.
		//
		CPrefabLibrary::LoadAllLibraries();
	}
	else
	{
		pLibrary->Add(pPrefab);
	}

	//
	// Update the object bar so the new prefab shows up.
	//
	GetMainWnd()->m_ObjectBar.UpdateListForTool(m_pToolManager->GetActiveToolID());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnInsertprefabOriginal(void)
{
	int iCurTool = m_pToolManager->GetActiveToolID();
	if ((iCurTool != TOOL_POINTER) && (iCurTool != TOOL_BLOCK) && (iCurTool != TOOL_ENTITY))
	{
		return;
	}

	BoundBox box;
	if (GetMainWnd()->m_ObjectBar.GetPrefabBounds(&box) == FALSE)
	{
		return;	// not a prefab listing
	}
		
	Vector pt( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );

	if (iCurTool != TOOL_ENTITY)
	{
		GetBestVisiblePoint(pt);
	}
	else
	{
		CToolEntity *pTool = dynamic_cast<CToolEntity*>(m_pToolManager->GetActiveTool()	);
		pTool->GetPos(pt);
	}

	Vector ptCenter;
	box.GetBoundsCenter(ptCenter);

	for(int i = 0; i < 3; i++)
	{
		box.bmins[i] += pt[i] - ptCenter[i];
		box.bmaxs[i] += pt[i] - ptCenter[i];
	}

	// create object
	box.SnapToGrid(m_nGridSpacing);	// snap to grid first
	CMapClass *pObject = GetMainWnd()->m_ObjectBar.CreateInBox(&box);
	if (pObject == NULL)
	{
		return;
	}

	ExpandObjectKeywords(pObject, m_pWorld);

	GetHistory()->MarkUndoPosition(NULL, "Insert Prefab");
	AddObjectToWorld(pObject);
	GetHistory()->KeepNew(pObject);
	
	SelectObject(pObject, scClear|scSelect|scSaveChanges );
	
	// set modified
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Find a substring within a string:
// Input  : *pszSub - 
//			*pszMain - 
// Output : static char *
//-----------------------------------------------------------------------------
static char * FindInString(char *pszSub, char *pszMain)
{
	char *p = pszMain;
	int nSub = strlen(pszSub);
	
	char ch1 = toupper(pszSub[0]);

	while(p[0])
	{
		if(ch1 == toupper(p[0]))
		{
			if(!strnicmp(pszSub, p, nSub))
				return p;
		}
		++p;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSolid - 
//			*pInfo - 
// Output : static BOOL
//-----------------------------------------------------------------------------
static BOOL ReplaceTexFunc(CMapSolid *pSolid, ReplaceTexInfo_t *pInfo)
{
	// make sure it's visible
	if (!pInfo->bHidden && !pSolid->IsVisible())
	{
		return TRUE;
	}

	int nFaces = pSolid->GetFaceCount();
	char *p;
	BOOL bSaved = FALSE;
	BOOL bMarkOnly = pInfo->bMarkOnly;
	for(int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);
		char *pszFaceTex = pFace->texture.texture;

		BOOL bDoMarkSolid = FALSE;

		switch(pInfo->iAction)
		{
			case 0:	// replace exact matches only:
			{
				if(!strcmpi(pszFaceTex, pInfo->szFind))
				{
					if(bMarkOnly)
					{
						bDoMarkSolid = TRUE;
						break;
					}

					if(!bSaved)
					{
						bSaved = TRUE;
						GetHistory()->Keep(pSolid);
					}
					pFace->SetTexture(pInfo->szReplace, pInfo->m_bRescaleTextureCoordinates);
					++pInfo->nReplaced;
				}
				break;
			}
			case 1:	// find partials, replace entire string:
			{
				p = FindInString(pInfo->szFind, pszFaceTex);
				if(p)
				{
					if(bMarkOnly)
					{
						bDoMarkSolid = TRUE;
						break;
					}

					if(!bSaved)
					{
						bSaved = TRUE;
						GetHistory()->Keep(pSolid);
					}
					pFace->SetTexture(pInfo->szReplace, pInfo->m_bRescaleTextureCoordinates);
					++pInfo->nReplaced;
				}
				break;
			}
			case 2:	// find partials, substitute replacement:
			{
				p = FindInString(pInfo->szFind, pszFaceTex);
				if(p)
				{
					if(bMarkOnly)
					{
						bDoMarkSolid = TRUE;
						break;
					}

					if(!bSaved)
					{
						bSaved = TRUE;
						GetHistory()->Keep(pSolid);
					}
					// create a new string
					char szNewTex[128];
					strcpy(szNewTex, pszFaceTex);
					strcpy(szNewTex + int(p - pszFaceTex), pInfo->szReplace);
					strcat(szNewTex, pszFaceTex + int(p - pszFaceTex) + pInfo->iFindLen);
					pFace->SetTexture(szNewTex, pInfo->m_bRescaleTextureCoordinates);
					++pInfo->nReplaced;
				}
				break;
			}
		}

		if (bDoMarkSolid)
		{
			if( pInfo->pDoc->GetTools()->GetActiveToolID() == TOOL_FACEEDIT_MATERIAL )
			{
				pInfo->pDoc->SelectFace(pSolid, i, scSelect);
				pInfo->nReplaced++;
			}
			else
			{
				if (!pSolid->IsSelected())
				{
					pInfo->pDoc->SelectObject(pSolid, scSelect);
					pInfo->nReplaced++;
				}
			}
		}
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFind - 
//			pszReplace - 
//			bEverything - 
//			iAction - 
//			bHidden - 
//-----------------------------------------------------------------------------
void CMapDoc::ReplaceTextures(LPCTSTR pszFind, LPCTSTR pszReplace, BOOL bEverything, int iAction, BOOL bHidden, bool bRescaleTextureCoordinates)
{
	CFaceEditSheet *pSheet = GetMainWnd()->m_pFaceEditSheet;
	HCURSOR hCursorOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
	pSheet->EnableUpdate(false);

	// set up info struct to pass to callback
	ReplaceTexInfo_t info;
	strcpy(info.szFind, pszFind);
	strcpy(info.szReplace, pszReplace);
	info.pDoc = this;
	info.bHidden = bHidden;
	if (iAction & 0x100)
	{
		iAction &= ~0x100;
		info.bMarkOnly = TRUE;
		info.bHidden = FALSE;	// do not mark hidden objects
	}
	else
	{
		info.bMarkOnly = FALSE;
	}

	info.iAction = iAction;
	info.nReplaced = 0;
	info.iFindLen = strlen(pszFind);
	info.pWorld = m_pWorld;
	info.m_bRescaleTextureCoordinates = bRescaleTextureCoordinates;

	if (bEverything)
	{
		// Mark/Replace textures in entire map.

		if (info.bMarkOnly)
		{
			// About to mark solids, set solids mode and clear the selection.
			m_pSelection->SetMode(selectSolids);
			SelectObject(NULL, scClear);
		}

		m_pWorld->EnumChildren((ENUMMAPCHILDRENPROC)ReplaceTexFunc, (DWORD)&info, MAPCLASS_TYPE(CMapSolid));
	}
	else
	{
		// Mark/Replace textures in the selection only.

		// Copy the selection into another list since we might be changing the selection
		// during this process.
		CMapObjectList tempSelection;
		tempSelection.AddVectorToTail( *m_pSelection->GetList() );
		
		if (info.bMarkOnly)
		{
			// About to mark solids, set solids mode and clear the selection.
			m_pSelection->SetMode(selectSolids);
			SelectObject(NULL, scClear);
		}

		FOR_EACH_OBJ( tempSelection, pos )
		{
			CMapClass *pobj = tempSelection.Element(pos);

			//
			// Call the texture replacement callback for this object (if it is a solid) and
			// all of its children (no matter what).
			//
			if (pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
			{
				ReplaceTexFunc((CMapSolid *)pobj, &info);
			}
			pobj->EnumChildren((ENUMMAPCHILDRENPROC)ReplaceTexFunc, (DWORD)&info, MAPCLASS_TYPE(CMapSolid));
		}
	}

	CString str;
	if (!info.bMarkOnly)
	{
		str.Format("%d textures replaced.", info.nReplaced);
		if (info.nReplaced > 0)
		{
			SetModifiedFlag();
		}
	}
	else
	{
		str.Format("%d %s marked.", info.nReplaced, (m_pToolManager->GetActiveToolID() == TOOL_FACEEDIT_MATERIAL) ? "faces" : "solids");
	}

	pSheet->EnableUpdate(true);
	SetCursor(hCursorOld);

	AfxMessageBox(str);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			pInfo - Pointer to the structure with info about how to do the find/replace.
// Output : 
//-----------------------------------------------------------------------------
static BOOL BatchReplaceTextureCallback( CMapClass *pObject, BatchReplaceTextures_t *pInfo )
{ 
	CMapSolid *solid;
	int numFaces, i;
	CMapFace *face;
	char szCurrentTexture[MAX_PATH];

	solid = ( CMapSolid * )pObject;
	numFaces = solid->GetFaceCount();
	for( i = 0; i < numFaces; i++ )
	{
		face = solid->GetFace( i );
		face->GetTextureName( szCurrentTexture );
		if( stricmp( szCurrentTexture, pInfo->szFindTexName ) == 0 )
		{
			face->SetTexture( pInfo->szReplaceTexName );
		}
	}
	return TRUE; // return TRUE to continue enumerating, FALSE to stop. 
}

  
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fp - 
//-----------------------------------------------------------------------------
void CMapDoc::BatchReplaceTextures( FileHandle_t fp )
{
	char *scan, *keyStart, *valStart;
	char buf[MAX_REPLACE_LINE_LENGTH];
	BatchReplaceTextures_t Info;

	while( g_pFullFileSystem->ReadLine( buf, sizeof( buf ), fp ) )
	{
		scan = buf;

		// skip whitespace.
		while( *scan == ' ' || *scan == '\t' )
		{
			scan++;
		}

		// get the key.
		keyStart = scan;
		while( *scan != ' ' && *scan != '\t' )
		{
			if( *scan == '\0' || *scan == '\n' )
			{
				goto next_line;
			}
			scan++;
		}
		memcpy( Info.szFindTexName, keyStart, scan - keyStart );
		Info.szFindTexName[scan - keyStart] = '\0';

		// skip whitespace.
		while( *scan == ' ' || *scan == '\t' )
		{
			scan++;
		}

		// get the value
		valStart = scan;
		while( *scan != ' ' && *scan != '\t' && *scan != '\0' && *scan != '\n' )
		{
			scan++;
		}
		memcpy( Info.szReplaceTexName, valStart, scan - valStart );
		Info.szReplaceTexName[scan - valStart] = '\0';

		// Get rid of the file extension in val if there is one.
		char *period;
		period = Info.szReplaceTexName + strlen( Info.szReplaceTexName ) - 4;
		if( period > Info.szReplaceTexName && *period == '.' )
		{
			*period = '\0';
		}

		// Get of backslashes in both key and val.
		for( scan = Info.szFindTexName; *scan; scan++ )
		{
			if( *scan == '\\' )
			{
				*scan = '/';
			}
		}
		for( scan = Info.szReplaceTexName; *scan; scan++ )
		{
			if( *scan == '\\' )
			{
				*scan = '/';
			}
		}

		// Search and replace all key textures with val.
		m_pWorld->EnumChildren( ( ENUMMAPCHILDRENPROC )BatchReplaceTextureCallback, ( DWORD )&Info, MAPCLASS_TYPE( CMapSolid ) ); 
next_line:;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the replace textures dialog.
//-----------------------------------------------------------------------------
void CMapDoc::OnEditReplacetex(void)
{
	CReplaceTexDlg dlg( m_pSelection->GetCount());

	dlg.m_strFind = GetDefaultTextureName();

	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	GetHistory()->MarkUndoPosition( m_pSelection->GetList(), "Replace Textures");

	if (dlg.m_bMarkOnly)
	{
		SelectObject(NULL, scClear|scSaveChanges);	// clear selection first
	}

	dlg.DoReplaceTextures();
}


//-----------------------------------------------------------------------------
// Purpose: Snaps the selected objects to the grid. This uses the selection
//			bounds as a reference point.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsSnapselectedtogrid(void)
{
	if ( m_pToolManager->GetActiveToolID() == TOOL_MORPH )
	{
		Morph3D *pMorphTool = assert_cast< Morph3D* >( m_pToolManager->GetActiveTool() );
		pMorphTool->SnapSelectedToGrid( m_nGridSpacing );
	}
	if (m_pSelection->IsEmpty())
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	BoundBox NewObjectBox;
	m_pSelection->GetBounds(NewObjectBox.bmins, NewObjectBox.bmaxs);

	Vector vecMove(0, 0, 0);

	// If we have a single point entity selected, just snap its origin.
	bool bOnePointEntity = false;
	if (pSelList->Count() == 1)
	{
		CMapClass *pObject = pSelList->Element(0);
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
		if (pEntity && pEntity->IsPlaceholder())
		{
			Vector vecOrigin;
			pEntity->GetOrigin(vecOrigin);
			Vector vecOriginSnap = vecOrigin;
			Snap(vecOriginSnap);

			vecMove = vecOriginSnap - vecOrigin;
			bOnePointEntity = true;
		}
	}
	
	if (!bOnePointEntity)
	{
		// Something other than a single point entity is selected.
		// Just snap the bmins of the selection bounding box.
		Vector vOldMins = NewObjectBox.bmins;
		NewObjectBox.SnapToGrid(m_nGridSpacing);

		// Calculate the amount to move.
		vecMove = NewObjectBox.bmins - vOldMins;
	}

	GetHistory()->MarkUndoPosition(pSelList, "Snap Objects");
	GetHistory()->Keep(pSelList);

	// do move
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		pObject->TransMove(vecMove);
	}

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CMapDoc::SnapObjectsRecursive(CMapClass *pObject)
{
	if (!pObject->IsGroup())
	{
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
		Vector vecRefPoint;
		if (pEntity && pEntity->IsPlaceholder())
		{
			// Point entities snap based on their origin.
			pEntity->GetOrigin(vecRefPoint);
		}
		else
		{
			// Everthing else snaps based on the mins of it's bounding box.
			Vector maxs;
			pObject->GetRender2DBox(vecRefPoint, maxs);
		}

		Vector vecRefPointSnap = vecRefPoint;
		Snap(vecRefPointSnap);

		Vector vecMove = vecRefPointSnap - vecRefPoint;
		pObject->TransMove(vecMove);
	}
	else
	{
		// Recurse into children of non-entities (since entities can't have
		// entity children).
		const CMapObjectList *pChildren = pObject->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			SnapObjectsRecursive(pChildren->Element(pos));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Snaps the selected objects to the grid. This uses the selection
//			bounds as a reference point.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsSnapSelectedToGridIndividually()
{
	if ( m_pSelection->IsEmpty() )
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	Vector vecMove(0, 0, 0);

	GetHistory()->MarkUndoPosition(pSelList, "Snap Objects Individually");
	GetHistory()->Keep(pSelList);

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		SnapObjectsRecursive(pObject);
	}

	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsSplitface(CCmdUI* pCmdUI) 
{
	if ( m_pToolManager->GetActiveToolID() != TOOL_MORPH )
	{
		pCmdUI->SetCheck( false );
	}
	else
	{
		Morph3D *pMorph = (Morph3D*) m_pToolManager->GetActiveTool();
		pCmdUI->SetCheck( pMorph->CanSplitFace());
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsSplitface(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		Morph3D *pMorph = (Morph3D*) m_pToolManager->GetActiveTool();
		pMorph->SplitFace();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Centers the origin of any entities in the given object tree.
// Input  : pObject - root of the object tree.
//-----------------------------------------------------------------------------
void CMapDoc::CenterOriginsRecursive(CMapClass *pObject)
{
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity != NULL)
	{
		const char *pszOrigin = pEntity->GetKeyValue("origin");
		if (pszOrigin)
		{
			// This entity has an origin key.
			GetHistory()->Keep(pEntity);

			Vector vecCenter;
			pEntity->GetBoundsCenter(vecCenter);

			// dvs: make key parse/unparse code common to Hammer and the engine
			char szOrigin[50];
			sprintf(szOrigin, "%g %g %g", vecCenter.x, vecCenter.y, vecCenter.z);
			pEntity->SetKeyValue("origin", szOrigin);
		}
	}
	else
	{
		// Recurse into children of non-entities (since entities can't have
		// entity children).

		const CMapObjectList *pChildren = pObject->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			CenterOriginsRecursive(pChildren->Element(pos));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Centers the origins of all selected entities.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsCenterOrigins()
{
	const CMapObjectList *pSelList = m_pSelection->GetList();

	GetHistory()->MarkUndoPosition(pSelList, "Center Origins");

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		CenterOriginsRecursive(pObject);
	}

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
}

//-----------------------------------------------------------------------------
// Purpose: Snaps a point to the grid, or to integer values if snap is disabled.
// Input  : pt - Point in world coordinates to snap.
//-----------------------------------------------------------------------------
void CMapDoc::Snap(Vector &pt, int nFlags)
{
	if ( m_bSnapToGrid )
		nFlags |= constrainSnap;
	else
		nFlags |= constrainIntSnap;

	if ( nFlags & constrainIntSnap )
	{
		for (int i = 0; i < 3; i++)
		{
			pt[i] = V_rint(pt[i]);
		}
	}
	else if (nFlags & constrainSnap )
	{
		float flGridSpacing = m_nGridSpacing;

		if ( nFlags & constrainHalfSnap )
			flGridSpacing *= 0.5f;

		for (int i = 0; i < 3; i++)
		{
			pt[i] = V_rint(pt[i] / flGridSpacing) * flGridSpacing;
		}
	}

	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsTransform(void)
{
	if(m_pSelection->IsEmpty())
	{
		AfxMessageBox("You must select some objects before you can\n"
			"transform them.");
		return;
	}

	CTransformDlg dlg;
	dlg.m_iMode = 0;

	if(dlg.DoModal() != IDOK)
		return;

	Vector vDelta( dlg.m_X, dlg.m_Y, dlg.m_Z );

	if (dlg.m_iMode == 1)
	{
		// make sure no 0.0 values
		for (int i = 0; i < 3; i++)
		{
			if (vDelta[i] == 0.0f)
			{
				vDelta[i] = 1.0f;
			}
		}
	}

	const CMapObjectList *pSelList = m_pSelection->GetList();

	// find origin
	Vector vOrigin;
	m_pSelection->GetBoundsCenter( vOrigin );

	GetHistory()->MarkUndoPosition(pSelList, "Transformation");
	GetHistory()->Keep(pSelList);

	//
	// Save any properties that may have been changed in the entity properties dialog.
	// This prevents the LoadData below from losing any changes that were made in the
	// object properties dialog.
	//
	GetMainWnd()->pObjectProperties->SaveData();

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		if ( dlg.m_iMode == 0 ) 
		{
			pObject->TransRotate( vOrigin, (QAngle&)vDelta );
		}
		else if ( dlg.m_iMode == 1 )
		{
			pObject->TransScale( vOrigin, vDelta );
		}
		else if ( dlg.m_iMode == 2 )
		{
			pObject->TransMove( vDelta );
		}
	}
	
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleDispSolidMask( void )
{
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	m_bDispSolidDrawMask = !m_bDispSolidDrawMask;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleSolidMask(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( m_bDispSolidDrawMask );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleDispDrawWalkable( void )
{
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	m_bDispDrawWalkable = !m_bDispDrawWalkable;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleDispDrawWalkable( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck( m_bDispDrawWalkable );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleDispDrawBuildable( void )
{
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	m_bDispDrawBuildable = !m_bDispDrawBuildable;
}


//-----------------------------------------------------------------------------
// Toggles between rendering disps in 3D.
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleDispDraw3D( void )
{
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	m_bDispDraw3D = !m_bDispDraw3D;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleDispDraw3D( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck( m_bDispDraw3D );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleDispDrawBuildable( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck( m_bDispDrawBuildable );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleDispDrawRemovedVerts( void )
{
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	m_bDispDrawRemovedVerts = !m_bDispDrawRemovedVerts;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleDispDrawRemovedVerts( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck( m_bDispDrawRemovedVerts );
}

void CMapDoc::OnToolsToggletexlock(void)
{
	Options.SetLockingTextures(!Options.IsLockingTextures());
	SetStatusText(SBI_PROMPT, Options.IsLockingTextures() ? "Texture locking on" : "Texture locking off");
}

void CMapDoc::OnUpdateToolsToggletexlock(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(Options.IsLockingTextures());
}

void CMapDoc::OnToolsToggletexlockScale(void)
{
	Options.SetScaleLockingTextures(!Options.IsScaleLockingTextures());
	SetStatusText(SBI_PROMPT, Options.IsScaleLockingTextures() ? "Scale texture locking on" : "Scale texture locking off");
}

void CMapDoc::OnUpdateToolsToggletexlockScale(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(Options.IsScaleLockingTextures());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsTextureAlignment(void)
{
	TextureAlignment_t eTextureAlignment;

	eTextureAlignment = Options.GetTextureAlignment();

	if (eTextureAlignment == TEXTURE_ALIGN_WORLD)
	{
		Options.SetTextureAlignment(TEXTURE_ALIGN_FACE);
		SetStatusText(SBI_PROMPT, "Face aligned textures");
	}
	else
	{
		Options.SetTextureAlignment(TEXTURE_ALIGN_WORLD);
		SetStatusText(SBI_PROMPT, "World aligned textures");
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsTextureAlignment(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(Options.GetTextureAlignment() == TEXTURE_ALIGN_FACE);
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether a remote shell editing session has been initiated
//			through a "session_begin" shell command.
//-----------------------------------------------------------------------------
bool CMapDoc::IsShellSessionActive(void)
{
	return(GetMainWnd()->IsShellSessionActive());
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current active state of the cordon tool.
//-----------------------------------------------------------------------------
bool CMapDoc::IsCordoning(void)
{
	return m_bIsCordoning;
}

bool CMapDoc::SetCordoning( bool bState)
{
	if ( m_bIsCordoning != bState )
	{
		m_bIsCordoning = bState;
		UpdateVisibilityAll();
		SetModifiedFlag( true );
		return true;
	}

	return false;
}

void CMapDoc::GetCordon( Vector &mins, Vector &maxs)
{
	mins = m_vCordonMins;
	maxs = m_vCordonMaxs;
}

void CMapDoc::SetCordon( const Vector &mins, const Vector &maxs)
{
	m_vCordonMins = mins;
	m_vCordonMaxs = maxs;
	UpdateVisibilityAll();
	SetModifiedFlag( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleCordon(void)
{
	SetCordoning( !m_bIsCordoning ); 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleCordon(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( m_bIsCordoning?1:0 );
}


//-----------------------------------------------------------------------------
// Purpose: Toggles between Groups selection mode and Solids selection mode.
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleGroupignore(void)
{
	SelectMode_t eSelectMode = m_pSelection->GetMode();
	if (eSelectMode == selectSolids)
	{
		eSelectMode = selectGroups;
	}
	else
	{
		eSelectMode = selectSolids;
	}

	m_pSelection->SetMode(eSelectMode);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleGroupignore(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(m_pSelection->GetMode() == selectSolids);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnChangeVertexscale(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		Morph3D *pMorph = (Morph3D*) m_pToolManager->GetActiveTool();
		pMorph->UpdateScale();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnVscaleToggle(void)
{
	if (m_pToolManager->GetActiveToolID() == TOOL_MORPH)
	{
		Morph3D *pMorph = (Morph3D*) m_pToolManager->GetActiveTool();
		pMorph->OnScaleCmd();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapEntityreport(void) 
{
	CEntityReportDlg::ShowEntityReport(this, GetMainWnd());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleSelectbyhandle(void)
{
	Options.view2d.bSelectbyhandles = !Options.view2d.bSelectbyhandles;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleSelectbyhandle(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(Options.view2d.bSelectbyhandles);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleInfiniteselect() 
{
	Options.view2d.bAutoSelect = !Options.view2d.bAutoSelect;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggleInfiniteselect(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(Options.view2d.bAutoSelect);
}

static BOOL SaveDXF(CMapSolid *pSolid, ExportDXFInfo_s *pInfo)
{
	return pSolid->SaveDXF( pInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnFileExporttodxf(void)
{
	static CString str;

	if (str.IsEmpty())
	{
		int nDot;

		// Replace the extension with DXF.
		str = GetPathName();
		if ((nDot = str.ReverseFind('.')) != -1)
		{
			str = str.Left(nDot);
		}
		str += ".dxf";
	}

	CExportDlg dlg(str, "dxf", "DXF files (*.dxf)|*.dxf||");
	if(dlg.DoModal() == IDCANCEL)
		return;

	str = dlg.GetPathName();
	if(str.ReverseFind('.') == -1)
		str += ".dxf";
	
	FILE *fp = fopen(str, "wb");

	m_pWorld->CalcBounds(TRUE);

	BoundBox box;
	m_pWorld->GetRender2DBox(box.bmins, box.bmaxs);

	fprintf(fp,"0\nSECTION\n2\nHEADER\n");
	fprintf(fp,"9\n$ACADVER\n1\nAC1008\n");
	fprintf(fp,"9\n$UCSORG\n10\n0.0\n20\n0.0\n30\n0.0\n");
	fprintf(fp,"9\n$UCSXDIR\n10\n1.0\n20\n0.0\n30\n0.0\n");
	fprintf(fp,"9\n$TILEMODE\n70\n1\n");
	fprintf(fp,"9\n$UCSYDIR\n10\n0.0\n20\n1.0\n30\n0.0\n");
	fprintf(fp,"9\n$EXTMIN\n10\n%f\n20\n%f\n30\n%f\n",
		box.bmins[0], box.bmins[1], box.bmins[2]);
	fprintf(fp,"9\n$EXTMAX\n10\n%f\n20\n%f\n30\n%f\n",
		box.bmaxs[0], box.bmaxs[1], box.bmaxs[2]);
	fprintf(fp,"0\nENDSEC\n");
	
	/* Tables section */
	fprintf(fp,"0\nSECTION\n2\nTABLES\n");
	/* Continuous line type */
	fprintf(fp,"0\nTABLE\n2\nLTYPE\n70\n1\n0\nLTYPE\n2\nCONTINUOUS"
		"\n70\n64\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n");
	fprintf(fp,"0\nENDTAB\n");
	
	/* Object names for layers */
	fprintf(fp,"0\nTABLE\n2\nLAYER\n70\n%d\n",1);
	fprintf(fp,"0\nLAYER\n2\n0\n70\n0\n62\n7\n6\nCONTINUOUS\n");
	fprintf(fp,"0\nENDTAB\n");
	fprintf(fp,"0\nTABLE\n2\nSTYLE\n70\n1\n0\nSTYLE\n2\nSTANDARD\n70\n0\n"
		"40\n0.0\n41\n1.0\n50\n0.0\n71\n0\n42\n0.2\n3\ntxt\n4\n\n0\nENDTAB\n");
	
	/* Default View? */
	
	/* UCS */ 
	fprintf(fp,"0\nTABLE\n2\nUCS\n70\n0\n0\nENDTAB\n");
	fprintf(fp,"0\nENDSEC\n");
	
	/* Entities section */
	fprintf(fp,"0\nSECTION\n2\nENTITIES\n");

	// export solids
	BeginWaitCursor();

	ExportDXFInfo_s info;
	info.bVisOnly = dlg.bVisibles!=0;
	info.nObject = 0;
	info.pWorld = m_pWorld;
	info.fp = fp;

	m_pWorld->EnumChildren(ENUMMAPCHILDRENPROC(SaveDXF), DWORD(&info), MAPCLASS_TYPE(CMapSolid));

	EndWaitCursor();

	fprintf(fp,"0\nENDSEC\n0\nEOF\n");
	fclose(fp);
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the 3D grid.
//-----------------------------------------------------------------------------
void CMapDoc::OnToggle3DGrid(void)
{
	m_bShow3DGrid = !m_bShow3DGrid;
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D|MAPVIEW_OPTIONS_CHANGED );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the check state of the 3D grid toggle button.
// Input  : *pCmdUI - 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToggle3DGrid(CCmdUI *pCmdUI) 
{
	pCmdUI->SetCheck(m_bShow3DGrid);
}


static void SetFilenameExtension( CString &fileName, const char *pExt )
{
	char *p = fileName.GetBuffer(MAX_PATH);
	p = strrchr(p, '.');
	if (p)
	{
		strcpy(p, pExt);
	}
	fileName.ReleaseBuffer();
}

//-----------------------------------------------------------------------------
// Purpose: Load data from the map's portal file for visualization
//-----------------------------------------------------------------------------
void CMapDoc::OnMapLoadportalfile(void)
{
	delete m_pPortalFile;
	m_pPortalFile = NULL;

	m_pPortalFile = new portalfile_t;
	m_pPortalFile->fileName = GetPathName();
	m_pPortalFile->totalVerts = 0;
	SetFilenameExtension( m_pPortalFile->fileName, ".prt" );

	CString str;
	str.Format("Load default portal file?\n(%s)", m_pPortalFile->fileName.GetBuffer());
	if(GetFileAttributes(m_pPortalFile->fileName) == 0xFFFFFFFF ||
		AfxMessageBox(str, MB_ICONQUESTION | MB_YESNO) == IDNO)
	{
		CFileDialog dlg(TRUE, ".prt", m_pPortalFile->fileName, OFN_HIDEREADONLY | 
			OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR, 
			"Portal files (*.prt)|*.prt|");

		if(dlg.DoModal() != IDOK)
			return;

		m_pPortalFile->fileName = dlg.GetPathName();	
	}
// load the file
	if(GetFileAttributes(m_pPortalFile->fileName) == 0xFFFFFFFF)
	{
		AfxMessageBox("Couldn't find portal file.");
		return;
	}
	FILE *fp = fopen(m_pPortalFile->fileName, "r");
	char szLine[256];

	int clusterCount;
	int portalCount;
	if (fscanf (fp,"%79s\n%i\n%i\n",szLine, &clusterCount, &portalCount) == 3)
	{
		if ( !Q_stricmp( szLine, "PRT1") )
		{
			for ( int iPortal = 0; iPortal < portalCount; iPortal++ )
			{
				int pointCount, leaf0, leaf1;
				if (fscanf (fp, "%i %i %i ", &pointCount, &leaf0, &leaf1 ) == 3 )
				{
					m_pPortalFile->vertCount.AddToTail( pointCount );
					m_pPortalFile->totalVerts += pointCount;
					for ( int i = 0; i < pointCount; i++ )
					{
						Vector v;
						if ( fscanf (fp, "(%f %f %f ) ", &v[0], &v[1], &v[2]) == 3 )
						{
							m_pPortalFile->verts.AddToTail(v);
						}
						else
						{
							break;
						}
					}
					fscanf (fp, "\n");
				}
				else
				{
					break;
				}
			}
		}
		// if this is not true we parsed incorrectly
		Assert( m_pPortalFile->vertCount.Count() == portalCount );
	}

	fclose(fp);
	if ( m_pPortalFile )
	{
		if ( m_pPortalFile->vertCount.Count() != portalCount || portalCount <= 0 )
		{
			delete m_pPortalFile;
			m_pPortalFile = NULL;
		}
	}

	UpdateAllViews( MAPVIEW_UPDATE_ONLY_2D );
}

//-----------------------------------------------------------------------------
// Purpose: Free the memory associate with the portal file
//-----------------------------------------------------------------------------
void CMapDoc::OnMapUnloadportalfile(void)
{
	delete m_pPortalFile;
	m_pPortalFile = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapLoadpointfile(void)
{
	if(m_strLastPointFile.IsEmpty())
	{
		m_strLastPointFile = GetPathName();
		const char *pExt = (m_pGame->mapformat == mfHalfLife2) ? ".lin" : ".pts";
		SetFilenameExtension( m_strLastPointFile, pExt );
	}

	CString str;
	str.Format("Load default pointfile?\n(%s)", m_strLastPointFile.GetBuffer());
	if(GetFileAttributes(m_strLastPointFile) == 0xFFFFFFFF ||
		AfxMessageBox(str, MB_ICONQUESTION | MB_YESNO) == IDNO)
	{
		CFileDialog dlg(TRUE, ".pts", m_strLastPointFile, OFN_HIDEREADONLY | 
			OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR, 
			"Pointfiles (*.pts;*.lin)|*.pts; *.lin||");
		
		if(dlg.DoModal() != IDOK)
			return;

		m_strLastPointFile = dlg.GetPathName();	
	}

	// load the file
	if(GetFileAttributes(m_strLastPointFile) == 0xFFFFFFFF)
	{
		AfxMessageBox("Couldn't load pointfile.");
		return;
	}
	
	std::ifstream file(m_strLastPointFile);

	m_PFPoints.Purge();
	while(!file.eof())
	{
		char szLine[256];
		file.getline(szLine, 256);
		
		Vector v;
		if(sscanf(szLine, "%f %f %f", &v.x, &v.y, &v.z) == 3)
		{
			m_PFPoints.AddToTail( v );
		}
		else
		{
			break;
		}
	}

	file.close();
	m_iCurPFPoint = -1;

	UpdateAllViews( MAPVIEW_UPDATE_ONLY_2D );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnMapUnloadpointfile(void)
{
	m_PFPoints.Purge();
	UpdateAllViews( MAPVIEW_UPDATE_ONLY_2D );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDirection - 
//-----------------------------------------------------------------------------
void CMapDoc::GotoPFPoint(int iDirection)
{
	if (m_PFPoints.Count() == 0)
		return;

	m_iCurPFPoint = m_iCurPFPoint + iDirection;

	if (m_iCurPFPoint == m_PFPoints.Count())
	{
		m_iCurPFPoint = 0;
	}
	else if (m_iCurPFPoint < 0)
	{
		m_iCurPFPoint = m_PFPoints.Count() - 1;
	}

	CenterViewsOn(m_PFPoints[m_iCurPFPoint]);
}


//-----------------------------------------------------------------------------
// Determine visgroup
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::GetRootAutoVisGroup()
{
	// Find the 'auto' visgroup
	CVisGroup *pFoundVisGroup = NULL;
	int nVisGroupCount = VisGroups_GetRootCount();
	for ( int i = 0; i < nVisGroupCount; ++i )
	{
		CVisGroup *pVisGroup = VisGroups_GetRootVisGroup(i);
		if ( !Q_stricmp( "Auto", pVisGroup->GetName() ) )
		{
			pFoundVisGroup = pVisGroup;
			break;
		}
	}

	if ( !pFoundVisGroup )
	{
		pFoundVisGroup = VisGroups_AddGroup( "Auto", true );		
	}

	return pFoundVisGroup;
}


//-----------------------------------------------------------------------------
// Determine visgroup and add it to that.
//-----------------------------------------------------------------------------
void CMapDoc::AddToAutoVisGroup( CMapClass *pObject )
{
	if ( !pObject || !VisGroups_ObjectCanBelongToVisGroup( pObject ) )
		return;

	if ( pObject->IsMapClass( MAPCLASS_TYPE(CMapEntity) ) )
	{
		CMapEntity *pMapEntity = assert_cast<CMapEntity*>( pObject );

		if ( pMapEntity->IsPointClass() )
		{
			// Point entities.
			if ( !pMapEntity->IsNodeClass() )
			{
				AddChildGroupToAutoVisGroup( pObject, "Point Entities", "Entities" );	
			}
			else
			{
				AddChildGroupToAutoVisGroup( pObject, "Nodes", "Entities" );	
			}

			if ( pMapEntity->IsNPCClass() )
			{
				AddChildGroupToAutoVisGroup( pObject, "NPCs", "Entities" );	
			}
			
			if ( !Q_strnicmp( pMapEntity->GetClassName(), "light_", strlen("light_") ) )
			{
				AddChildGroupToAutoVisGroup( pObject, "Lights", "Entities" );	
			}

			if ( !Q_strnicmp( pMapEntity->GetClassName(), "prop_", strlen("prop_") ) )
			{
				AddChildGroupToAutoVisGroup( pObject, "Props", "World Details" );					 			
			}
		}
		else
		{
			// Solid entities.
			if ( !pMapEntity->IsClass("func_detail") )
			{
				AddChildGroupToAutoVisGroup( pObject, "Brush Entities", "Entities" );					
			}
			else
			{
				AddChildGroupToAutoVisGroup( pObject, "Func Detail", "World Details" );					 			
			}

			if ( !Q_strnicmp( pMapEntity->GetClassName(), "trigger_", strlen("trigger_") ) )
			{
				AddChildGroupToAutoVisGroup( pObject, "Triggers", "Entities" );					 			
			}

			// Area portals and area portal windows.
			if ( !Q_strnicmp( pMapEntity->GetClassName(), "func_areaportal", strlen("func_areaportal") ) )
			{
				AddChildGroupToAutoVisGroup( pObject, "Areaportals", "Tool Brushes" );					 			
			}

			if ( pMapEntity->IsClass("func_occluder") )
			{
				AddChildGroupToAutoVisGroup( pObject, "Occluders", "Tool Brushes" );					 			
			}
		}
	}
	else if ( pObject->IsMapClass( MAPCLASS_TYPE(CMapSolid) ) )
	{
		CMapSolid *pMapSolid = assert_cast<CMapSolid*>( pObject );

		if ( pMapSolid->HasDisp() )
		{
			AddChildGroupToAutoVisGroup( pObject, "Displacements", "World Geometry" );					 			
		}
		else
		{
			CMapClass *pParent = pObject->GetParent();
			CMapEntity *pParentEntity = dynamic_cast<CMapEntity*>( pParent );
			if ( !pParentEntity )
			{
				AddToAutoVisGroup( pObject, "World Geometry" );
			}
		}
		char buf[1024];

		bool bWaterAdded = false;
	
		for ( int i = 0; i < pMapSolid->GetFaceCount(); i++ )
		{
			//this check will ensure that an object with a water material on multiple faces will only be added once
			if ( !bWaterAdded )
			{
				IMaterial *pMaterial = pMapSolid->GetFace( i )->GetTexture()->GetMaterial();
				if ( pMaterial )
				{
					pMaterial->FindVar( "%compileWater", &bWaterAdded, false );
					if ( bWaterAdded )
					{
						AddChildGroupToAutoVisGroup( pObject, "Water", "World Geometry" );					 			
					}
				}
			}
			//same check for tool objects
			pMapSolid->GetFace( i )->GetTextureName( buf );

			if ( strstr( buf, "tools/tools" ) )
			{
				if ( strstr( buf, "tools/toolsnodraw" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Nodraw", "World Geometry" );					 								
					continue;
				}
				if ( strstr( buf, "tools/toolssky" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Sky", "World Geometry" );					 								
					continue;
				}	
				if ( strstr( buf, "tools/toolsblock" ) )					
				{					
					AddChildGroupToAutoVisGroup( NULL, "Block", "Tool Brushes" );					 
					if ( strstr( buf, "block_los" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "LOS", "Block" );
						continue;
					}
					else if ( strstr( buf, "blockbullets" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "Bullets", "Block" );	
						continue;
					}
					else if ( strstr( buf, "blocklight" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "Light", "Block" );	
						continue;
					}					 
				}				
				if ( strstr( buf, "tools/tools" ) && strstr( buf, "clip" ) )
				{
					AddChildGroupToAutoVisGroup( NULL, "Clips", "Tool Brushes" );					 					
					if ( strstr( buf, "npcclip" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "NPC", "Clips" );
						continue;
					}
					else if ( strstr( buf, "playerclip" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "Player", "Clips" );
						continue;
					}
					else if ( strstr( buf, "controlclip" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "Control", "Clips" );
						continue;
					}
					else if ( strstr( buf, "clip" ) )
					{
						AddChildGroupToAutoVisGroup( pObject, "Clip", "Clips" );
						continue;
					}					 
				}	
				if ( strstr( buf, "occluder" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Occluder", "Tool Brushes");
					continue;
				}
				if ( strstr( buf, "areaportal") )
				{
					AddChildGroupToAutoVisGroup( pObject, "Area Portal", "Tool Brushes");
					continue;
				}
				if ( strstr( buf, "invisible") )
				{
					AddChildGroupToAutoVisGroup( NULL, "Invisible", "Tool Brushes");
					if ( strstr( buf, "invisibleladder") )
					{
						AddChildGroupToAutoVisGroup( pObject, "Ladder", "Invisible");
						continue;
					}
					else if ( strstr( buf, "invisible") )
					{
						AddChildGroupToAutoVisGroup( pObject, "Invisible", "Invisible");
						continue;
					}					
				}
				if ( strstr( buf, "skip" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Skip", "Tool Brushes" );					 			
					continue;					
				}
				if ( strstr( buf, "trigger" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Trigger", "Tool Brushes" );
					continue;
				}
				if ( strstr( buf, "origin" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Origin", "Tool Brushes" );
					continue;
				}				
				if ( strstr( buf, "hint" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Hint", "Tool Brushes" );
					continue;
				}	
				if ( strstr( buf, "fog" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Fog", "Tool Brushes" );
					continue;
				}	
				if ( strstr( buf, "black" ) )
				{
					AddChildGroupToAutoVisGroup( pObject, "Black", "World Geometry" );					 								
					continue;
				}	
			}
		}
	}
	else if (pObject->IsGroup())
	{
		// Recurse into the children of groups.
		const CMapObjectList *pChildren = pObject->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			AddToAutoVisGroup(pChildren->Element(pos));
			AddToFGDAutoVisGroups( pChildren->Element( pos ) );
		}
	}

	// See if this Object type exists in FGD AutoVisGroups
	if ( pGD->m_FGDAutoVisGroups.Count() > 0 && !( pObject->IsGroup() ) )
	{
		AddToFGDAutoVisGroups( pObject );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CMapDoc::AddToAutoVisGroup( CMapClass *pObject, const char *pAutoVisGroup )
{
	CVisGroup *pRootVisGroup = GetRootAutoVisGroup();

	// Find the desired visgroup
	CVisGroup *pFoundVisGroup = NULL;
	int nVisGroupCount = pRootVisGroup->GetChildCount();
	for ( int i = 0; i < nVisGroupCount; ++i )
	{
		CVisGroup *pVisGroup = pRootVisGroup->GetChild(i);
		if ( !Q_stricmp( pAutoVisGroup, pVisGroup->GetName() ) )
		{
			pFoundVisGroup = pVisGroup;
			break;
		}
	}

	if ( !pFoundVisGroup )
	{
		pFoundVisGroup = VisGroups_AddGroup( pAutoVisGroup, true );
		VisGroups_SetParent( pFoundVisGroup, pRootVisGroup );		
	}

	CMapObjectList Objects;
	Objects.AddToTail( pObject );
	VisGroups_AddObjectsToVisGroup( Objects, pFoundVisGroup, false, false );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CMapDoc::AddChildGroupToAutoVisGroup( CMapClass *pObject, const char *pAutoVisGroup, const char *pParentName )
{
	CVisGroup *pRootVisGroup = VisGroups_GroupForName( pParentName, true );
	if ( !pRootVisGroup )
	{
		CVisGroup *pRootAutoVisGroup = GetRootAutoVisGroup();
		pRootVisGroup = VisGroups_AddGroup( pParentName, true );
		VisGroups_SetParent( pRootVisGroup, pRootAutoVisGroup );
	}

	// Find the desired visgroup
	CVisGroup *pFoundVisGroup = NULL;
	int nVisGroupCount = pRootVisGroup->GetChildCount();
	for ( int i = 0; i < nVisGroupCount; ++i )
	{
		CVisGroup *pVisGroup = pRootVisGroup->GetChild(i);
		if ( !Q_stricmp( pAutoVisGroup, pVisGroup->GetName() ) )//&& pVisGroup->IsAutoVisGroup() )
		{
			pFoundVisGroup = pVisGroup;
			break;
		}
	}

	if ( !pFoundVisGroup )
	{
		pFoundVisGroup = VisGroups_AddGroup( pAutoVisGroup, true );
		VisGroups_SetParent( pFoundVisGroup, pRootVisGroup );	
	}

	CMapObjectList Objects;
	Objects.AddToTail( pObject );
	VisGroups_AddObjectsToVisGroup( Objects, pFoundVisGroup, false, false );
}

// Remove from all auto visgroup
//-----------------------------------------------------------------------------
void CMapDoc::RemoveFromAutoVisGroups( CMapClass *pObject )
{
	if ( !pObject )
		return;

	bool bChanged = false;
	int nVisGroupCount = pObject->GetVisGroupCount();
	for (int nVisGroup = nVisGroupCount - 1; nVisGroup >= 0; nVisGroup--)
	{
		CVisGroup *pVisGroup = pObject->GetVisGroup(nVisGroup);

		// Don't add autovisgroups when moving back
		if ( pVisGroup->IsAutoVisGroup() )
		{
			pObject->RemoveVisGroup(pVisGroup);
			bChanged = true;
		}
	}

	if ( bChanged )
	{
		UpdateVisibility(pObject);
	}
}


//-----------------------------------------------------------------------------
// Determine visgroup
//-----------------------------------------------------------------------------
void CMapDoc::AssignAllToAutoVisGroups()
{
	bool bLocked = VisGroups_LockUpdates( true );

	EnumChildrenPos_t pos;
    CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild)
	{
		AddToAutoVisGroup( pChild );
		pChild = m_pWorld->GetNextDescendent(pos);
	}

	if ( bLocked )
	{
		VisGroups_LockUpdates( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds an object to the world. This is the ONLY correct way to add an
//			object to the world. Calling directly through AddChild skips a bunch
//			of necessary bookkeeping.
// Input  : pObject - object being added to the world.
//-----------------------------------------------------------------------------
void CMapDoc::AddObjectToWorld(CMapClass *pObject, CMapClass *pParent)
{
	Assert(pObject != NULL);
    
	if (pObject != NULL)
	{
		m_pWorld->AddObjectToWorld(pObject, pParent);

		// Auto visgroups!
		AddToAutoVisGroup( pObject );
	
		//
		// Give the renderer a chance to precache data. 
		//
		RenderPreloadObject(pObject);

		//
		// Update the object's visibility. This will recurse into solid children of entities as well.
		//
		UpdateVisibility(pObject);
		 
		// Set a reasonable default 
		Vector2D vecLogicalPos = pObject->GetLogicalPosition();
		if ( vecLogicalPos.x == COORD_NOTINIT )
		{
			GetDefaultNewLogicalPosition( vecLogicalPos );
			pObject->SetLogicalPosition( vecLogicalPos );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes an object from the world object tree.
// Input  : pObject - object being removed from the world.
//-----------------------------------------------------------------------------
void CMapDoc::RemoveObjectFromWorld(CMapClass *pObject, bool bRemoveChildren)
{
	Assert(pObject != NULL);

	if (pObject != NULL)
	{
		m_pWorld->RemoveObjectFromWorld(pObject, bRemoveChildren);

		//
		// Clean up any visgroups with no members.
		//
		VisGroups_PurgeGroups();

		// Remove the object from the update list.
		m_UpdateList.FindAndRemove(pObject);
		// remove object from selection list
		m_pSelection->SelectObject(pObject, scUnselect );
		pObject->SignalChanged();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CMapDoc::RenderPreloadObject(CMapClass *pObject)
{
	POSITION p = GetFirstViewPosition();

	while (p)
	{
		CView *pView = GetNextView(p);
		if (pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
		{
			CMapView3D *pView3D = (CMapView3D *)pView;
			pView3D->RenderPreloadObject(pObject);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: CreateTempWorld creates a world with the brushes that make up the
//			cordoned area. it does this by creating a solid of the size of the
//			box, and another solid 1200 units bigger. subtract A from B, and
//			keep those brushes.
// Output : Returns a pointer to the newly-created world.
//-----------------------------------------------------------------------------
CMapWorld *CMapDoc::CordonCreateWorld()
{
	CMapWorld *pWorld = new CMapWorld( NULL );

	GetHistory()->Pause();

	// create solids
	CMapSolid *pBigSolid = new CMapSolid;
	pBigSolid->SetCordonBrush( true );

	CMapSolid *pSmallSolid = new CMapSolid;
	pSmallSolid->SetCordonBrush( true );

	BoundBox bigbounds( m_vCordonMins, m_vCordonMaxs );
		
	StockBlock box;
	box.SetFromBox( &bigbounds );
    box.CreateMapSolid(pSmallSolid, Options.GetTextureAlignment());

	// make bigger box
	for (int i = 0; i < 3; i++)
	{
		// dvs: FIXME: vbsp barfs if I go all the way out to the full mins & maxs
		bigbounds.bmins[i] = g_MIN_MAP_COORD + 8;
		if (bigbounds.bmins[i] > m_vCordonMins[i])
		{
			bigbounds.bmins[i] = g_MIN_MAP_COORD;
		}

		bigbounds.bmaxs[i] = g_MAX_MAP_COORD - 8;
		if (bigbounds.bmaxs[i] < m_vCordonMaxs[i])
		{
			bigbounds.bmaxs[i] = g_MAX_MAP_COORD;
		}
	}

	box.SetFromBox(&bigbounds);
	box.CreateMapSolid(pBigSolid, Options.GetTextureAlignment());

	//
	// Subtract the small box from the large box and add the outside pieces to the
	// cordon world. This gives a hollow box.
	//
	CMapObjectList Outside;
	pBigSolid->Subtract(NULL, &Outside, pSmallSolid);

	for (int p=0;p<Outside.Count();p++)
	{
		CMapClass *pObject = Outside.Element(p);
		pWorld->AddObjectToWorld(pObject);
	}

	delete pBigSolid;
	delete pSmallSolid;

	//
	// Set all the brush textures to the texture specified in the options.
	//
	const char *pszTexture = g_pGameConfig->GetCordonTexture();
	EnumChildrenPos_t pos;
	CMapClass *pChild = pWorld->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pChild);
		if (pSolid != NULL)
		{
			pSolid->SetCordonBrush(true);
			pSolid->SetTexture(pszTexture);
		}

		pChild = pWorld->GetNextDescendent(pos);
	}

	GetHistory()->Resume();

	return(pWorld);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::CordonSaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t eResult = pFile->BeginChunk("cordon");

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValuePoint("mins", m_vCordonMins);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValuePoint("maxs", m_vCordonMaxs);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueBool("active", m_bIsCordoning );
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszFileName - 
//-----------------------------------------------------------------------------
bool CMapDoc::SaveVMF(const char *pszFileName, int saveFlags )
{
	CChunkFile File;	

	ChunkFileResult_t eResult = File.Open(pszFileName, ChunkFile_Write);
	BeginWaitCursor();
	
	// Change the main title bar.
	GetMainWnd()->SetWindowText( "Saving..." );
	// We can optionally use these calls if we want the document name to go away so the title bar only says "Saving...".
	//GetMainWnd()->ModifyStyle( FWS_ADDTOTITLE, 0 );  //GetMainWnd()->ModifyStyle( 0, FWS_ADDTOTITLE );

	if (eResult == ChunkFile_Ok)
	{
		CSaveInfo SaveInfo;
		CMapObjectList CordonList;
		CMapWorld *pCordonWorld = NULL;

		if (!m_bPrefab && !(saveFlags & SAVEFLAGS_LIGHTSONLY))
		{
			SaveInfo.SetVisiblesOnly(bSaveVisiblesOnly == TRUE);

			//
			// Add cordon objects.
			//
			if ( m_bIsCordoning )
			{
				//
				// Create "cordon world", add its objects to our real world, create a list in
				// CordonList so we can remove them again.
				//
				pCordonWorld = CordonCreateWorld();
				
				const CMapObjectList *pChildren = pCordonWorld->GetChildren();
				FOR_EACH_OBJ( *pChildren, pos )
				{
					CMapClass *pChild = pChildren->Element(pos);
					pChild->SetTemporary(TRUE);
					m_pWorld->AddObjectToWorld(pChild);
					CordonList.AddToTail(pChild);
				}
			}
		}

		//
		// Write the map file version.
		//
		if (eResult == ChunkFile_Ok)
		{
			bool bIsAutosave = false;
			if ( SAVEFLAGS_AUTOSAVE & saveFlags )
			{
				bIsAutosave = true;
			}
			eResult = SaveVersionInfoVMF(&File, bIsAutosave);
		}

		//
		// Save VisGroups information. Save this first so that we can assign visgroups while loading objects.
		//
		if (!m_bPrefab && !(saveFlags & SAVEFLAGS_LIGHTSONLY))
		{
			eResult = VisGroups_SaveVMF(&File, &SaveInfo);
		}

		//
		// Save view related settings (grid setting, splitter proportions, etc)
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = SaveViewSettingsVMF(&File, &SaveInfo);
		}

		// Save the world.
		if (eResult == ChunkFile_Ok)
		{
			eResult = m_pWorld->SaveVMF(&File, &SaveInfo, saveFlags & SAVEFLAGS_LIGHTSONLY);
		}

		if (!m_bPrefab && !(saveFlags & SAVEFLAGS_LIGHTSONLY))
		{
			//
			// Remove cordon objects from the real world.
			//
			if ( m_bIsCordoning )
			{
				FOR_EACH_OBJ( CordonList, pos )
				{
					CMapClass *pobj = CordonList.Element(pos);
					m_pWorld->RemoveObjectFromWorld(pobj, true);
				}

				//
				// The cordon objects will be deleted in the cordon world's destructor.
				//
				delete pCordonWorld;
			}

			// Save tool information.
			if (eResult == ChunkFile_Ok)
			{
				eResult = m_pToolManager->SaveVMF(&File, &SaveInfo);
			}

			if (eResult == ChunkFile_Ok)
			{
				eResult = CordonSaveVMF(&File, &SaveInfo);
			}

			// We use this to flag VMFs checked-in to P4 with QuickHide active
			if ( eResult == ChunkFile_Ok )
			{
				eResult = QuickHide_SaveVMF( &File, &SaveInfo );
			}
		}

		File.Close();
	}

	// Restore the main window's title.
	GetMainWnd()->OnUpdateFrameTitle( true );

	if (eResult != ChunkFile_Ok)
	{
		GetMainWnd()->MessageBox(File.GetErrorText(eResult), "Error Saving File", MB_OK);
	}
	else
	{
		//save filename into registry for last known good file for crash recovery purposes.
		AfxGetApp()->WriteProfileString("General", "Last Good Save", pszFileName);		
	}
	
	EndWaitCursor();
	return( eResult == ChunkFile_Ok );
}


//-----------------------------------------------------------------------------
// Purpose: Saves the version information chunk.
// Input  : *pFile - 
// Output : Returns ChunkFile_Ok on success, an error code on failure.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::SaveVersionInfoVMF(CChunkFile *pFile, bool bIsAutoSave)
{
	ChunkFileResult_t eResult = pFile->BeginChunk("versioninfo");

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("editorversion", 400);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("editorbuild", build_number());
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("mapversion", GetDocVersion());
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("formatversion", VMF_FORMAT_VERSION);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueBool("prefab", m_bPrefab);
	}

	if ( eResult == ChunkFile_Ok && bIsAutoSave )
	{		
		eResult = pFile->BeginChunk("autosave");

		if ( eResult == ChunkFile_Ok )
		{
			char szOriginalName[MAX_PATH];
			V_strcpy_safe( szOriginalName, GetPathName() );
			if ( strlen( szOriginalName ) == 0 )
			{
				strcpy(szOriginalName, g_pGameConfig->szMapDir);
				strcat(szOriginalName, "\\untitled.vmf");	
				//put in the default map path + untitled.vmf
			}
			Q_FixSlashes( szOriginalName, '/' );
			eResult = pFile->WriteKeyValue( "originalname", szOriginalName );
			if ( eResult == ChunkFile_Ok )
			{
				eResult = pFile->EndChunk();
			}
		}				
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Call this function after you have made any modifications to the document.
//			By calling this function consistently, you ensure that the framework 
//			prompts the user to save changes before closing a document.
// Input  : bModified - TRUE to mark the doc as modified, FALSE to mark it as clean.
//-----------------------------------------------------------------------------
void CMapDoc::SetModifiedFlag(BOOL bModified)
{
	//
	// Increment internal version number when the doc changes from clean to dirty.
	// CShell::BeginSession checks this version number, enabling us to detect
	// out-of-sync problems when editing the map in the engine.
	//
	if ((bModified) && !IsModified())
	{
		m_nDocVersion++;
		
	}

	if ( bModified )
	{
		SetAutosaveFlag( bModified );

		m_pSelection->SetBoundsDirty();

		GetMainWnd()->pObjectProperties->MarkDataDirty();

		UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}
	
	CDocument::SetModifiedFlag(bModified);
}

void CMapDoc::SetAutosaveFlag( BOOL bNeedsAutosave )
{
	m_bNeedsAutosave = bNeedsAutosave;
}

BOOL CMapDoc::NeedsAutosave()
{
	return m_bNeedsAutosave;
}

BOOL CMapDoc::IsAutosave()
{
	return m_bIsAutosave;
}

CString *CMapDoc::AutosavedFrom()
{
	return &m_strAutosavedFrom;
}


//-----------------------------------------------------------------------------
// Purpose: this function adds an external reference count to a map
//-----------------------------------------------------------------------------
void CMapDoc::AddReference( void ) 
{ 
	m_nExternalReferenceCount++; 
}


//-----------------------------------------------------------------------------
// Purpose: this function removes an external reference count from a map.  If it is hidden
//			and modified, the document will be made visible.
//-----------------------------------------------------------------------------
void CMapDoc::RemoveReference( void ) 
{ 
	m_nExternalReferenceCount--; 

	if ( m_nExternalReferenceCount <= 0 )
	{
		if ( !IsVisible() )
		{
			if ( IsModified() )
			{
				ShowWindow( true );
			}
			else
			{
				OnCloseDocument();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CMapDoc::SetUndoActive(bool bActive)
{
	m_pUndo->SetActive(bActive);
	m_pRedo->SetActive(bActive);
}


bool CMapDoc::IsCulledBy3DCameraDistance( CMapClass *pObject, UpdateVisibilityData_t *pData )
{
	if ( pObject->IsWorld() || pObject->IsGroup() )
		return false;

	if ( pObject->IsSelected() )
		return false;

	Vector objectPos;
	pObject->GetOrigin( objectPos );

	float flDistance = ( pData->vecRadiusCullCenter - objectPos ).LengthSqr();	

	if ( flDistance >= pData->flRadiusCullDistSq )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the hidden/shown state of the given object based on the current
//			settings including selection mode, visgroups, and cordon bounds.
// Input  : pObject - 
//-----------------------------------------------------------------------------
bool CMapDoc::ShouldObjectBeVisible(CMapClass *pObject, UpdateVisibilityData_t *pData)
{
#if defined(_DEBUG) && 0
	CMapEntity	*pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity)
	{
		LPCTSTR	pszTargetName = pEntity->GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "relay_cancelVCDs") )
		{
			// Set breakpoint here for debugging this entity's visiblity
			int foo = 0;
		}
	}
#endif

	if ( QuickHide_IsObjectHidden( pObject ) )
	{
		return false;
	}

	if ( pData->bRadiusCullingEnabled )
	{
		if ( IsCulledBy3DCameraDistance( pObject, pData ) )
		{
			return false;
		}
	}

	//
	// If hide entities is enabled and the object is an entity, hide the object.
	//
	if (m_bHideItems)
	{
		CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
		if ((pEntity != NULL) && (pEntity->IsPlaceholder()))
		{
			return(false);
		}
	}

	//
	// If the object hides as clutter and clutter is currently hidden, hide it.
	//
	if (pObject->IsClutter() && !Options.GetShowHelpers() )
	{
		return false;
	}

	//
	// If the object was hidden by visgroups, hide it unless visgroup hiding is disabled.
	//
	if (!CVisGroup::IsShowAllActive() && !pObject->IsVisGroupShown())
	{
		return false;
	}

	//
	// If the cordon tool is active and the object is not within the cordon bounds,
	// hide the object. The exception to this is some helpers, which only hide if their
	// parent entity is culled by the cordon.
	//
	if ( m_bIsCordoning && !IsManifest() )
	{
		if (pObject->IsCulledByCordon(m_vCordonMins, m_vCordonMaxs))
		{
			return(false);
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies the document when an object has changed. The object is
//			added to a list which will be processed before the next view is rendered.
// Input  : pObject - Object that has changed.
//-----------------------------------------------------------------------------
void CMapDoc::UpdateObject(CMapClass *pObject)
{
	Assert(!pObject->IsTemporary());

	if ( m_UpdateList.Find(pObject) == -1)
	{
		m_UpdateList.AddToTail(pObject);
	}

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
}


//-----------------------------------------------------------------------------
// Purpose: Processes any objects that have changed since the last call.
//			Updates each object's visiblity and makes sure that there are no
//			invisible objects in the selection set.
//
//			This must be called from the outer loop, as selection.RemoveInvisibles
//			may change the contents of the selection. Therefore, this should never
//			be called from low-level code that might be inside an iteration of the
//			selection.
//-----------------------------------------------------------------------------
void CMapDoc::Update(void)
{
	ProcessNotifyList();

	if (m_UpdateList.Count()>0)
	{
		//
		// Process every object in the update list.
		//
		FOR_EACH_OBJ( m_UpdateList, pos )
		{
			CMapClass *pObject = m_UpdateList.Element(pos);
			UpdateVisibility(pObject);
		}

		//
		// Make sure there aren't any invisible objects in the selection.
		//
		m_pSelection->RemoveInvisibles();

		//
		// Empty the update list now that it has been processed.
		//
		m_UpdateList.RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Callback for updating the visibility status of a single object and
//			its children.
// Input  : pObject - 
//			pDoc - 
//-----------------------------------------------------------------------------
BOOL CMapDoc::UpdateVisibilityCallback(CMapClass *pObject, UpdateVisibilityData_t *pData )
{
	Assert(!pObject->IsTemporary());

	bool bVisible = pData->pDoc->ShouldObjectBeVisible(pObject, pData);
	pObject->SetVisible(bVisible);
	if (bVisible)
	{
		//
		// If this is an entity and it is visible, recurse into any children.
		//
		if ( ( dynamic_cast< CMapEntity * >( pObject ) ) != NULL || ( dynamic_cast< CMapWorld * >( pObject ) ) != NULL )
		{
			pObject->EnumChildren((ENUMMAPCHILDRENPROC)UpdateVisibilityCallback, (DWORD)pData);
		}
	}

	return(TRUE);
}


void CMapDoc::InitUpdateVisibilityData( UpdateVisibilityData_t &data )
{
	memset( &data, 0, sizeof( data ) );

	data.pDoc = this;
	data.bRadiusCullingEnabled = ( Options.general.bRadiusCulling == TRUE );

	if ( data.bRadiusCullingEnabled )
	{
		POSITION viewpos = GetFirstViewPosition();
		while ( viewpos )
		{
			CMapView3D *pView = dynamic_cast<CMapView3D*>( GetNextView( viewpos ) );
			if ( pView )
			{
				CCamera *pCamera = pView->GetCamera();
				if ( pCamera )
				{
					pCamera->GetViewPoint( data.vecRadiusCullCenter );

					data.flRadiusCullDistSq = pCamera->GetFarClip();
					data.flRadiusCullDistSq *= data.flRadiusCullDistSq;

					return;
				}
			}
		}

		// Can't use radius culling without a 3D view.
		data.bRadiusCullingEnabled = false;
	}
};



//-----------------------------------------------------------------------------
// Purpose: Updates the visibility of the given object and its children.
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CMapDoc::UpdateVisibility(CMapClass *pObject)
{
	UpdateVisibilityData_t data;
	InitUpdateVisibilityData( data );

	UpdateVisibilityCallback(pObject, &data);
	if (pObject->IsGroup())
	{
		pObject->EnumChildrenRecurseGroupsOnly((ENUMMAPCHILDRENPROC)UpdateVisibilityCallback, (DWORD)&data);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the visibility of all objects in the world.
//-----------------------------------------------------------------------------
void CMapDoc::UpdateVisibilityAll(void)
{
	UpdateVisibilityData_t data;
	InitUpdateVisibilityData( data );

	//
	// Two stage recursion: first we recurse groups only, then from the callback we recurse
	// solid children of entities.
	//
	m_pWorld->EnumChildrenRecurseGroupsOnly((ENUMMAPCHILDRENPROC)UpdateVisibilityCallback, (DWORD)&data);
	m_pSelection->RemoveInvisibles();

	CMainFrame *pwndMain = GetMainWnd();
	if (pwndMain)
	{
		pwndMain->UpdateAllDocViews( MAPVIEW_UPDATE_VISGROUP_STATE );

		// Some entity I/O connections may have become valid/invalid after this hide/show.
		pwndMain->pObjectProperties->MarkDataDirty();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a visgroup to this document.
// Input  : pszName - The name to assign the visgroup
//			bAuto	- should this be an auto visgroup (default: false)
// Output : Returns a pointer to the newly created visgroup.
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::VisGroups_AddGroup(LPCTSTR pszName, bool bAuto)
{
	CVisGroup *pGroup = new CVisGroup;
	pGroup->SetName(pszName);
	pGroup->SetAuto( bAuto );

	//
	// Generate a random color for the group.
	//
	pGroup->SetColor(80 + (random() % 176), 80 + (random() % 176), 80 + (random() % 176));
	
	//
	// Generate a unique id for this visgroup.
	//
	int id = 0;
	while (id++ < 2000)
	{
		if (!VisGroups_GroupForID(id))
		{
			break;
		}
	}

	pGroup->SetID(id);

	return VisGroups_AddGroup(pGroup);
}


//-----------------------------------------------------------------------------
// Purpose: Adds a visgroup to this document.
// Input  : pGroup - Visgroup to add.
// Output : Returns a pointer to the given visgroup.
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::VisGroups_AddGroup(CVisGroup *pGroup)
{
	Assert( pGroup != NULL );

	m_VisGroups->AddToTail(pGroup);

	if (!pGroup->GetParent())
	{
		m_RootVisGroups->AddToTail(pGroup);
	}

	return pGroup;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMapDoc::VisGroups_CanMoveUp(CVisGroup *pGroup)
{
	CVisGroup *pParent = pGroup->GetParent();
	if (pParent)
	{
		return pParent->CanMoveUp(pGroup);
	}
	else
	{
		return (m_RootVisGroups->Find(pGroup) > 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMapDoc::VisGroups_CanMoveDown(CVisGroup *pGroup)
{
	CVisGroup *pParent = pGroup->GetParent();
	if (pParent)
	{
		return pParent->CanMoveDown(pGroup);
	}
	else
	{
		int nIndex = m_RootVisGroups->Find(pGroup);
		return (nIndex >= 0) && (nIndex < m_RootVisGroups->Count() - 1);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			pGroup - 
// Output : Returns FALSE to stop enumerating if the object belonged to the given
//			visgroup. Otherwise, returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
BOOL CMapDoc::VisGroups_CheckForGroupCallback(CMapClass *pObject, CVisGroup *pGroup)
{
	if (pObject->IsInVisGroup(pGroup))
	{
		return(FALSE);
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of visgroups in this document.
//-----------------------------------------------------------------------------
int CMapDoc::VisGroups_GetCount(void)
{
	return(m_VisGroups->Count());
}


//-----------------------------------------------------------------------------
// Purpose: Returns a visgroup by index.
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::VisGroups_GetVisGroup(int nIndex)
{
	return(m_VisGroups->Element(nIndex));
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of root-level visgroups in this document.
//-----------------------------------------------------------------------------
int CMapDoc::VisGroups_GetRootCount(void)
{
	if ( m_RootVisGroups )
	{
		return( m_RootVisGroups->Count() );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a root-level visgroup by index.
//-----------------------------------------------------------------------------
CVisGroup *CMapDoc::VisGroups_GetRootVisGroup(int nIndex)
{
	return(m_RootVisGroups->Element(nIndex));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pGroup - 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_MoveUp(CVisGroup *pGroup)
{
	CVisGroup *pParent = pGroup->GetParent();
	if (pParent)
	{
		pParent->MoveUp(pGroup);
	}
	else
	{
		int nIndex = m_RootVisGroups->Find(pGroup);
		if (nIndex > 0)
		{
			m_RootVisGroups->Remove(nIndex);
			m_RootVisGroups->InsertBefore(nIndex - 1, pGroup);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pGroup - 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_MoveDown(CVisGroup *pGroup)
{
	CVisGroup *pParent = pGroup->GetParent();
	if (pParent)
	{
		pParent->MoveDown(pGroup);
	}
	else
	{
		int nIndex = m_RootVisGroups->Find(pGroup);
		if (nIndex < (m_RootVisGroups->Count() - 1))
		{
			m_RootVisGroups->Remove(nIndex);
			m_RootVisGroups->InsertAfter(nIndex, pGroup);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Deletes any visgroups that no longer have any members.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_PurgeGroups(void)
{
	bool bUpdate = false;

	int nCount = VisGroups_GetCount();
	for (int i = nCount - 1; i >= 0; i--)
	{
		CVisGroup *pGroup = VisGroups_GetVisGroup(i);

		// Don't purge groups with children
		if ( pGroup->GetChildCount() )
			continue;

		bool bKill = true;
		EnumChildrenPos_t pos2;
		CMapClass *pObject = m_pWorld->GetFirstDescendent(pos2);
		while (pObject != NULL)
		{
			if (pObject->IsInVisGroup(pGroup))
			{
				bKill = false;
				break;
			}

			pObject = m_pWorld->GetNextDescendent(pos2);
		}
		
		if (bKill)
		{
			bUpdate = true;
			VisGroups_UnlinkGroup(pGroup);
			delete pGroup;
		}
	}

	if (bUpdate)
	{
		GetMainWnd()->m_FilterControl.UpdateGroupList();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the given visgroup from the visgroup hierarchy.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_RemoveGroup(CVisGroup *pGroup)
{
	VisGroups_DoRemoveOrCombine(pGroup, NULL);
	VisGroups_UnlinkGroup(pGroup);
	delete pGroup;

    VisGroups_UpdateAll();
	UpdateVisibilityAll();
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Combines two visgroups, moving member objects and child visgroups
//			into the 'to' group, and deletes the 'from' group.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_CombineGroups(CVisGroup *pFrom, CVisGroup *pTo)
{
	// Can't combine a group into one of it's descendents.
	Assert(!pFrom->FindDescendent(pTo));

	VisGroups_DoRemoveOrCombine(pFrom, pTo);
	VisGroups_UnlinkGroup(pFrom);
	delete pFrom;
    
	VisGroups_UpdateAll();
	UpdateVisibilityAll();
	SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Removes the visgroup from the visgroup lists.
// Input  : *pGroup - 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_UnlinkGroup(CVisGroup *pGroup)
{
	m_VisGroups->FindAndRemove(pGroup);
	m_RootVisGroups->FindAndRemove(pGroup);
	
	CVisGroup *pParent = pGroup->GetParent();
	if (pParent)
	{
		pParent->RemoveChild(pGroup);
	}

	GetHistory()->OnRemoveVisGroup(pGroup);
}


//-----------------------------------------------------------------------------
// Purpose: Combines two visgroups, moving member objects and child visgroups
//			into the 'to' group.
// Input  : pFrom - 
//			pTo - 
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_DoRemoveOrCombine(CVisGroup *pFrom, CVisGroup *pTo)
{
	if (!pFrom)
		return;

	//
	// Replace membership in pFrom with membership in pTo.
	//
	CMapWorld *pWorld = GetMapWorld();	
	EnumChildrenPos_t pos;
	CMapClass *pObject = pWorld->GetFirstDescendent(pos);
	while (pObject != NULL)
	{
		if (pObject->IsInVisGroup(pFrom))
		{
			//must add the object to the new visgroup before removing from the old
			//this solves the problem of incorrectly forcing visibilty when an object becomes
			//temporarily orphaned.
			if (pTo)
			{
				pObject->AddVisGroup(pTo);
			}
			pObject->RemoveVisGroup(pFrom);
		}

		pObject = pWorld->GetNextDescendent(pos);
	}

	if (pTo)
	{
		//
		// Move child visgroups into pTo.
		//
		for (int i = 0; i < pFrom->GetChildCount(); i++)
		{
			CVisGroup *pChild = pFrom->GetChild(i);
			pTo->AddChild(pChild);
		}

		//
		// Make sure pTo has the proper visibility state.
		//
		if (pFrom->GetVisible() != pTo->GetVisible())
		{
			pTo->SetVisible(VISGROUP_PARTIAL);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::VisGroups_SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	if (VisGroups_GetCount() == 0)
	{
		return(ChunkFile_Ok);
	}

	ChunkFileResult_t eResult = pFile->BeginChunk("visgroups");
	
	if (eResult == ChunkFile_Ok)
	{
		// Save the root level visgroups; children are saved recursively.
		int nCount = VisGroups_GetRootCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pVisGroup = this->VisGroups_GetRootVisGroup(i);
			if ( pVisGroup != NULL && !pVisGroup->IsAutoVisGroup() && strcmp( pVisGroup->GetName(), "Auto" ) )
			{
				eResult = pVisGroup->SaveVMF(pFile, pSaveInfo);

				if (eResult != ChunkFile_Ok)
				{
					break;
				}
			}
		}
	}
			
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::SaveViewSettingsVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t eResult = pFile->BeginChunk("viewsettings");

	eResult = pFile->WriteKeyValueBool("bSnapToGrid", m_bSnapToGrid);
	if (eResult != ChunkFile_Ok)
		return eResult;

	eResult = pFile->WriteKeyValueBool("bShowGrid", m_bShowGrid);
	if (eResult != ChunkFile_Ok)
		return eResult;

#ifndef SDK_BUILD
	eResult = pFile->WriteKeyValueBool("bShowLogicalGrid", m_bShowLogicalGrid);
	if (eResult != ChunkFile_Ok)
		return eResult;
#endif // SDK_BUILD

	eResult = pFile->WriteKeyValueInt("nGridSpacing", m_nGridSpacing);
	if (eResult != ChunkFile_Ok)
		return eResult;

	eResult = pFile->WriteKeyValueBool("bShow3DGrid", m_bShow3DGrid);
	if (eResult != ChunkFile_Ok)
		return eResult;

	return(pFile->EndChunk());
}


//-----------------------------------------------------------------------------
// Purpose: Turns on lighting preview mode by loading the BSP file with the
//			same named as the VMF being edited.
//-----------------------------------------------------------------------------
void CMapDoc::InternalEnableLightPreview( bool bCustomFilename )
{
#if 0
	OnDisableLightPreview();

	m_pBSPLighting = CreateBSPLighting();
	
	// Either use the VMF filename or the last-exported VMF name.
	CString strFile;
	if( m_strLastExportFileName.GetLength() == 0 )
		strFile = GetPathName();
	else
		strFile = m_strLastExportFileName;

	// Convert the extension to .bsp
	char *p = strFile.GetBuffer(MAX_PATH);
	char *ext = strrchr(p, '.');
	if( ext )
	{
		strcpy( ext, ".bsp" );
	}


	// Strip out the directory.
	char *cur = p;
	while ((cur = strstr(cur, "/")) != NULL)
	{
		*cur = '\\';
	}

	char fileName[MAX_PATH];

	char *pLastSlash = p;
	char *pTest;
	while ((pTest = strstr(pLastSlash, "\\")) != NULL)
	{
		pLastSlash = pTest + 1;
	}
	
	if( pLastSlash )
		strcpy( fileName, pLastSlash );
	else
		strcpy( fileName, p );

	strFile.ReleaseBuffer();


	// Use <mod directory> + "/maps/" + <filename>
	char fullPath[MAX_PATH*2];
	sprintf( fullPath, "%s\\maps\\%s", g_pGameConfig->m_szModDir, fileName );

	
	// Only do the dialog if they said to or if the default BSP file doesn't exist.
	if( !bCustomFilename )
	{
		FILE *fp = fopen( fullPath, "rb" );
		if( fp )
			fclose( fp );
		else
			bCustomFilename = true;
	}


	CString finalPath;
	if( bCustomFilename )
	{
		CFileDialog dlg(
			TRUE,		// bOpenFile
			"bsp",		// default extension
			fullPath,	// default filename,
			OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,	// flags
			"BSP Files (*.bsp)|*.bsp|All Files (*.*)|*.*||",
			NULL 		// filter
			);

		if( dlg.DoModal() != IDOK )
			return;
	
		finalPath = dlg.GetPathName();
	}
	else
	{
		finalPath = fullPath;
	}

	if( !m_pBSPLighting->Load( finalPath ) )
	{
		char str[256];
		Q_snprintf( str, sizeof(str), "Can't load lighting from '%s'.", finalPath );
		AfxMessageBox( str );
	}


	// Switch the first mapview we find into 3D lighting preview.
	POSITION viewPos = GetFirstViewPosition();
	while( viewPos )
	{
		CView *pView = GetNextView( viewPos );
		if (pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
		{
			CMapView3D *pView3D = (CMapView3D *)pView;
			pView3D->SetDrawType( VIEW3D_LIGHTING_PREVIEW );
			break;
		}
	}
#endif
}


void CMapDoc::OnEnableLightPreview()
{
	InternalEnableLightPreview( false );
}


void CMapDoc::OnEnableLightPreviewCustomFilename()
{
	InternalEnableLightPreview( true );
}


//-----------------------------------------------------------------------------
// Purpose: Turns off lighting preview mode.
//-----------------------------------------------------------------------------
void CMapDoc::OnDisableLightPreview()
{
#if 0
	// Change any light preview views back to regular 3D.
	POSITION p = GetFirstViewPosition();
	while (p)
	{
		CView *pView = GetNextView(p);
		if (pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
		{
			CMapView3D *pView3D = (CMapView3D *)pView;

			if( pView3D->GetDrawType() == VIEW3D_LIGHTING_PREVIEW )
				pView3D->SetDrawType( VIEW3D_TEXTURED );
		}
	}

	if( m_pBSPLighting )
	{
		m_pBSPLighting->Release();
		m_pBSPLighting = 0;
	}
#endif
}


void CMapDoc::OnUpdateLightPreview()
{
	if( !m_pBSPLighting )
		return;

	// Save out a file with just the ents.
	char szFile[MAX_PATH];
	V_strcpy_safe( szFile, GetPathName() );
	szFile[strlen(szFile) - 1] = 'e';
	
	if( !SaveVMF( szFile, SAVEFLAGS_LIGHTSONLY ) )
	{
		CString str;
		str.FormatMessage( IDS_CANT_SAVE_ENTS_FILE, szFile );
		return;
	}

	// Get it in memory.
	CUtlVector<char> fileData;
	FILE *fp = fopen( szFile, "rb" );
	if( !fp )
	{
		CString str;
		str.FormatMessage( IDS_CANT_OPEN_ENTS_FILE, szFile );
		AfxMessageBox( str, MB_OK );
		return;
	}

	fseek( fp, 0, SEEK_END );
	fileData.SetSize( ftell( fp ) + 1 );
	fseek( fp, 0, SEEK_SET );
	fread( fileData.Base(), 1, fileData.Count(), fp );
	fclose( fp );

	// Null-terminate it.
	fileData[ fileData.Count() - 1 ] = 0;

	// Tell the incremental lighting manager to relight.
	m_pBSPLighting->StartLighting( fileData.Base() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::OnToggleLightPreview()
{
#if 0
	if( m_pBSPLighting )
	{
		POSITION p = GetFirstViewPosition();
		while (p)
		{
			CView *pView = GetNextView(p);
			if (pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
			{
				CMapView3D *pView3D = (CMapView3D *)pView;

				if( pView3D->GetDrawType() == VIEW3D_LIGHTING_PREVIEW )
					pView3D->SetDrawType( VIEW3D_TEXTURED );
				else if( pView3D->GetDrawType() == VIEW3D_TEXTURED )
					pView3D->SetDrawType( VIEW3D_LIGHTING_PREVIEW );
			}
		}
	}
	else
	{
		// If no lighting is loaded, then load it.
		OnEnableLightPreview();
	}
#endif
}


void CMapDoc::OnAbortLightCalculation()
{
	if( !m_pBSPLighting )
		return;

	m_pBSPLighting->Interrupt();
}


//-----------------------------------------------------------------------------
// Used to avoid adding redundant notifications to the list.
//-----------------------------------------------------------------------------
bool CMapDoc::FindNotification(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	int nCount = m_NotifyList.Count();
	for (int i = 0; i < nCount; i++)
	{
		if ((m_NotifyList.Element(i).pObject->m_pObject == pObject) &&
			(m_NotifyList.Element(i).eNotifyType == eNotifyType))
		{
			return true;
		}
	}

	return false;
}


bool CMapDoc::AnyNotificationsForObject(CMapClass *pObject)
{
	int nCount = m_NotifyList.Count();
	for (int i = 0; i < nCount; i++)
	{
		if ( m_NotifyList.Element(i).pObject->m_pObject == pObject )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Adds the notification event to the list for processing.
// Input  : pObject - 
//			eNotifyType - 
//-----------------------------------------------------------------------------
static bool s_bDispatchingNotifications = false;
void CMapDoc::NotifyDependents(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	Assert( !s_bDispatchingNotifications );
	if ( s_bDispatchingNotifications )
		return;

	if (pObject->IsTemporary())
	{
		return;
	}

	if (eNotifyType != Notify_Removed)
	{
		if (!FindNotification(pObject, eNotifyType))
		{
			NotifyListEntry_t entry;
			entry.pObject = pObject->GetSafeObjectSmartPtr();
			entry.eNotifyType = eNotifyType;
			m_NotifyList.AddToTail(entry);
		}
	}
	else
	{
		DispatchNotifyDependents(pObject, eNotifyType);
	}
}


//-----------------------------------------------------------------------------
// Dispatches notifications to dependent objects. Called once per frame, this
// allows objects to update themselves based on changes made to other objects
// before they are rendered.
//-----------------------------------------------------------------------------
void CMapDoc::ProcessNotifyList()
{
	s_bDispatchingNotifications = true;

	int nCount = m_NotifyList.Count();
	if (nCount)
	{
		for (int i = 0; i < nCount; i++)
		{
			NotifyListEntry_t entry = m_NotifyList.Element(i);
			if ( entry.pObject->m_pObject )
			{
				DispatchNotifyDependents(entry.pObject->m_pObject, entry.eNotifyType);
			}
			else
			{
				static bool bShowedWarning = false;
				if ( !bShowedWarning )
				{
					bShowedWarning = true;
					AfxMessageBox( "ProcessNotifyList: encountered a deleted object. Tell a programmer.", MB_OK );
				}
			}
		}

		m_NotifyList.RemoveAll();
	}

	s_bDispatchingNotifications = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			eNotifyType - 
//-----------------------------------------------------------------------------
void CMapDoc::DispatchNotifyDependents(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	const CMapObjectList *pDependents = pObject->GetDependents();

	if ( pDependents->Count() == 0 )
		return;
	
	// Get a copy of the dependecies list because it may change during iteration.
	CMapObjectList TempDependents;

	TempDependents.AddVectorToTail( *pDependents );
	
	for (int i = 0; i < TempDependents.Count(); i++)
	{
		CMapClass *pDependent = TempDependents.Element(i);
		
		//
		// Maybe we should give our dependents the opportunity to unlink themselves here?
		// Returning false from OnNotify could indicate a desire to sever the dependency.
		// Currently this is accomplished by calling UpdateDependency from OnNotifyDependent.
		//
		pDependent->OnNotifyDependent(pObject, eNotifyType);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Replicates the given list of objects, then selects the duplicates.
// Input  : Objects - 
//-----------------------------------------------------------------------------
void CMapDoc::CloneObjects(const CMapObjectList &Objects)
{
	CMapObjectList NewObjects;

	bool bLocked = VisGroups_LockUpdates( true );
	
	//
	// Run through list of objects and copy each to build a list of cloned
	// objects.
	//
	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pobj = Objects.Element(pos);
		CMapClass *pNewobj = pobj->Copy(false);
		pNewobj->CopyChildrenFrom(pobj, false);

		if (!Options.view2d.bKeepclonegroup)
		{
			pNewobj->RemoveAllVisGroups();
		}

		NewObjects.AddToTail(pNewobj);
	}

	// Notification happens in two-passes. The first pass lets objects generate new unique
	// IDs, the second pass lets objects fixup references to other cloned objects.

	Assert( Objects.Count() == NewObjects.Count() );

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pobj = Objects.Element(pos);
		CMapClass *pNewobj = NewObjects.Element(pos);
		pobj->OnPreClone(pNewobj, m_pWorld, Objects, NewObjects);
	}

	// Do the second pass of notification and add the objects to the world. The second pass
	// of notification lets objects fixup references to other cloned objects.

	FOR_EACH_OBJ( Objects, pos )
	{
		CMapClass *pobj = Objects.Element(pos);
		CMapClass *pNewobj = NewObjects.Element(pos);

		pobj->OnClone(pNewobj, m_pWorld, Objects, NewObjects);

		AddObjectToWorld(pNewobj);
	}

	SelectObjectList( &NewObjects );

	if ( bLocked )
	{
		VisGroups_LockUpdates( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &axes - 
//			vecNudge - 
//-----------------------------------------------------------------------------
void CMapDoc::GetNudgeVector(const Vector& vHorz, const Vector& vVert, int nChar, bool bSnap, Vector &vecNudge)
{
	vecNudge.Init();

	float fUnit = ((bSnap && IsSnapEnabled()) ? GetGridSpacing() : 1);

	switch ( nChar )
	{
		case VK_RIGHT	: vecNudge += fUnit*vHorz; break;
		case VK_LEFT	: vecNudge -= fUnit*vHorz; break;
		case VK_UP		: vecNudge += fUnit*vVert; break;
		case VK_DOWN	: vecNudge -= fUnit*vVert; break;

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDoc::NudgeObjects(const Vector &Delta, bool bClone)
{
	const CMapObjectList *pSelList = m_pSelection->GetList();


	// If they held down shift, clone the selection then nudge the clones.
	if (bClone)
	{
		GetHistory()->MarkUndoPosition(pSelList, "Clone Objects");
		CloneObjects(*pSelList);
		GetHistory()->KeepNew(pSelList);
	}
	else
	{
		GetHistory()->MarkUndoPosition(pSelList, "Nudge objects");
		GetHistory()->Keep(pSelList);
	}

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		pObject->TransMove(Delta);
	}

	SetModifiedFlag();
}

//-----------------------------------------------------------------------------
// Purpose: deals with update problems when changing multiple visgroup settings
//-----------------------------------------------------------------------------
bool CMapDoc::VisGroups_LockUpdates( bool bLock )
{
	// check if already locked/unlocked
	if ( m_bVisGroupUpdatesLocked == bLock )
		return false;

	m_bVisGroupUpdatesLocked = bLock;
	if ( !bLock )
	{
		//
		// Clean up any visgroups with no members.
		//
		VisGroups_PurgeGroups();

		//
		// Update object visiblity and refresh views.
		//
		VisGroups_UpdateAll();

		CMainFrame *pwndMain = GetMainWnd();
		if (pwndMain)
		{
			pwndMain->UpdateAllDocViews(MAPVIEW_UPDATE_VISGROUP_ALL);
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks visgroup-membership for all objects in a visgroup.
//-----------------------------------------------------------------------------
void CMapDoc::VisGroups_CheckMemberVisibility(CVisGroup *pGroup)
{
	//Msg("--------  Visgroups_ShowVisGroup --------\n");

	if (pGroup == NULL)
		return;

	//
	// Show or hide all the objects that belong to the given visgroup.
	//
	EnumChildrenPos_t pos;
	CMapClass *pChild = m_pWorld->GetFirstDescendent(pos);
	while (pChild)
	{
		if (IsInVisGroupRecursive(pChild, pGroup))
		{
			pChild->CheckVisibility();
		}

		pChild = m_pWorld->GetNextDescendent(pos);
	}

	VisGroups_UpdateAll();	
	UpdateVisibilityAll();
	SetModifiedFlag();
}

CMapView3D *CMapDoc::GetFirst3DView()
{
	POSITION pos = this->GetFirstViewPosition();
	while ( pos )
	{
		CView *pView = GetNextView(pos);
		if (pView->IsKindOf(RUNTIME_CLASS(CMapView3D)))
		{
			CMapView3D *pView3D = (CMapView3D *)pView;
			return pView3D;
		}
	}
	return NULL;
}

CMapView *CMapDoc::GetActiveMapView()
{
	POSITION p = GetFirstViewPosition();
	while (p)
	{
		CMapView *pView = dynamic_cast<CMapView*>(GetNextView(p));

		if ( !pView )
			continue;

		if ( pView->IsActive() )
			return pView;
	
	}
	return NULL;
}


void CMapDoc::OnLogicalobjectLayoutgeometric()
{
	Vector2D vecLogicalCenter;
	if ( !m_pSelection->GetLogicalBoundsCenter( vecLogicalCenter ) )
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	if ( pSelList->Count() <= 1 )
		return;

	GetHistory()->MarkUndoPosition( pSelList, "Layout Geometric" );
	GetHistory()->Keep( pSelList );

	bool bCenterView = false;
	CMapViewLogical *pCurrentView = NULL;
	if ( GetMainWnd()->GetActiveFrame() )
	{
		pCurrentView = dynamic_cast<CMapViewLogical*>( GetMainWnd()->GetActiveFrame()->GetActiveView() );
		if ( pCurrentView )
			bCenterView = true;
	}

 	for ( int i = 0; i < pSelList->Count(); ++i )
	{
		CMapClass *pClass = pSelList->Element( i );
		if ( !pClass->IsLogical() )
			continue;

		Vector oldCenter;
		pClass->GetBoundsCenter( oldCenter );

		Vector2D newLogicalCenter;
		newLogicalCenter.x = oldCenter.x + oldCenter.z/4;
		newLogicalCenter.y = oldCenter.y + oldCenter.z/4;
		pClass->SetLogicalPosition( newLogicalCenter );
	}

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS | MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_ONLY_LOGICAL );

	m_pSelection->SetBoundsDirty();

	if ( bCenterView )
	{
		CenterLogicalViewsOnSelection();
	}
}


void CMapDoc::OnLogicalobjectLayoutdefault()
{
	Vector2D vecLogicalCenter;
	if ( !m_pSelection->GetLogicalBoundsCenter( vecLogicalCenter ) )
		return;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	if ( pSelList->Count() <= 1 )
		return;

	GetHistory()->MarkUndoPosition( pSelList, "Layout Default" );
	GetHistory()->Keep( pSelList );

	bool bCenterView = false;
	CMapViewLogical *pCurrentView = NULL;
	if ( GetMainWnd()->GetActiveFrame() )
	{
		pCurrentView = dynamic_cast<CMapViewLogical*>( GetMainWnd()->GetActiveFrame()->GetActiveView() );
		if ( pCurrentView )
			bCenterView = true;
	}

	m_nLogicalPositionCount = 0;
 	for ( int i = 0; i < pSelList->Count(); ++i )
	{
		CMapClass *pClass = pSelList->Element( i );
		if ( !pClass->IsLogical() )
			continue;

		Vector2D newLogicalCenter;
		GetDefaultNewLogicalPosition( newLogicalCenter );
		pClass->SetLogicalPosition( newLogicalCenter );
	}

	UpdateAllViews( MAPVIEW_UPDATE_OBJECTS | MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_ONLY_LOGICAL );

	m_pSelection->SetBoundsDirty();

	if ( bCenterView )
	{
		CenterLogicalViewsOnSelection();
	}
}

void CMapDoc::OnLogicalobjectLayoutlogical()
{
	// TODO: Add your command handler code here
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the state of Tools | Instances | Hide. When enabled, instances are hidden.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsInstancesHide(void)
{
	m_tShowInstance = INSTANCES_HIDE;
	UpdateAllViews( MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Instances | Hide menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsInstancesHide(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck( ( m_tShowInstance == INSTANCES_HIDE ) );
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the state of Tools | Instances | Show Tinted. When enabled, instances are tinted.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsInstancesShowTinted(void)
{
	m_tShowInstance = INSTANCES_SHOW_TINTED;
	UpdateAllViews( MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Instances | Show Tinted menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsInstancesShowTinted(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck( ( m_tShowInstance == INSTANCES_SHOW_TINTED ) );
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the state of Tools | Instances | Show Normal. When enabled, instances are shown just like the rest of the world.
//-----------------------------------------------------------------------------
void CMapDoc::OnToolsInstancesShowNormal(void)
{
	m_tShowInstance = INSTANCES_SHOW_NORMAL;
	UpdateAllViews( MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Tools | Instances | Show Normal menu item.
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateToolsInstancesShowNormal(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
	pCmdUI->SetCheck( ( m_tShowInstance == INSTANCES_SHOW_NORMAL ) );
}


//-----------------------------------------------------------------------------
// Purpose: this function will show or hide the map doc
// Input  : bIsVisible - flag to indicate the visibility status
//-----------------------------------------------------------------------------
void CMapDoc::ShowWindow( bool bIsVisible )
{
	bool	bNeedsInitialUpdate = !HasInitialUpdate();

	POSITION posOpenDoc = GetFirstViewPosition();
	while ( posOpenDoc != NULL )
	{
		CView		*pView = GetNextView( posOpenDoc );
		CFrameWnd	*pFrame = pView->GetParentFrame();

		if ( pFrame )
		{
			if ( bIsVisible && bNeedsInitialUpdate )
			{
				APP()->pMapDocTemplate->InitialUpdateFrame( pFrame, this, true );
			}

			pFrame->ActivateFrame( bIsVisible ? SW_SHOW : SW_HIDE );
			pFrame->ShowWindow( bIsVisible ? SW_SHOW : SW_HIDE );

			CMainFrame *pwndMain = GetMainWnd();
			::SendMessage( pwndMain->m_hWndMDIClient, WM_MDIREFRESHMENU, 0, 0 );
			pwndMain->DrawMenuBar();
		}
	}

	if ( bIsVisible && !HasInitialUpdate() )
	{
		SetInitialUpdate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function checks to see if the map doc is visible
// Output : returns true if the document is visible
//-----------------------------------------------------------------------------
bool CMapDoc::IsVisible( void )
{
	POSITION posOpenDoc = GetFirstViewPosition();
	while ( posOpenDoc != NULL )
	{
		CView		*pView = GetNextView( posOpenDoc );
		CFrameWnd	*pFrame = pView->GetParentFrame();

		if ( pFrame )
		{
			return ( pFrame->IsWindowVisible() == TRUE );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: this function will hide all instance map windows
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancesHideAll( void )
{
	POSITION	pos = APP()->pMapDocTemplate->GetFirstDocPosition();

	while( pos != NULL )
	{
		CDocument *pDoc = APP()->pMapDocTemplate->GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc && pMapDoc->GetReferenceCount() != 0 && !pMapDoc->IsModified() )
		{
			pMapDoc->ShowWindow( false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will show all instance map windows
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancesShowAll( void )
{
	POSITION	pos = APP()->pMapDocTemplate->GetFirstDocPosition();

	while( pos != NULL )
	{
		CDocument *pDoc = APP()->pMapDocTemplate->GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc && pMapDoc->GetReferenceCount() != 0 )
		{
			pMapDoc->ShowWindow( true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: This routine will open up the manifest dialog.  If the map is not 
//			a manifest, it will being the prompted to ask the user if he wants
//			to convert it to one.
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancingCreatemanifest()
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		return;
	}

	CManifest *pManifest = CMapDoc::GetManifest();
	if ( !pManifest )
	{
		if ( AfxMessageBox( "Do you want to create a new manifest from this map?", MB_YESNO ) == IDNO )
		{
			return;
		}
		if ( !CreateNewManifest() )
		{
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancingCheckinAll( )
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	pManifest->m_bDefaultCheckin = true;
	for( int i = 0; i < pManifest->GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = pManifest->GetMap( i );
		pManifestMap->m_bDefaultCheckin = true;
	}

	CManifestCheckin	ManifestCheckin;
	if ( ManifestCheckin.DoModal() == IDOK )
	{
		pManifest->CheckFileStatus();
		GetMainWnd()->m_ManifestFilterControl.Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancingCheckOutManifest( )
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	if ( pManifest->CheckOut() == false && p4 )
	{
		char temp[ 2048 ];

		sprintf( temp, "Could not check out manifest: %s", p4->GetLastError() );
		AfxMessageBox( temp, MB_ICONHAND | MB_OK );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancingAddManifest( )
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	if ( pManifest->AddToVersionControl() == false && p4 )
	{
		char temp[ 2048 ];

		sprintf( temp, "Could not check out manifest: %s", p4->GetLastError() );
		AfxMessageBox( temp, MB_ICONHAND | MB_OK );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function checks to see if it should enable the Create Manifest menu item
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateInstancingCreatemanifest(CCmdUI *pCmdUI)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		pCmdUI->Enable( false );
		return;
	}

	CManifest *pManifest = CMapDoc::GetManifest();

	pCmdUI->Enable( ( pManifest == NULL ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateInstancingCheckinAll( CCmdUI *pCmdUI )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc || !p4 )
	{
		pCmdUI->Enable( false );
		return;
	}

	CManifest *pManifest = CMapDoc::GetManifest();
	if ( pManifest == NULL )
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( pManifest->m_bCheckedOut == true )
	{
		pCmdUI->Enable( true );
		return;
	}

	for( int i = 0; i < pManifest->GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = pManifest->GetMap( i );
		if ( pManifestMap->m_bCheckedOut == true )
		{
			pCmdUI->Enable( true );
			return;
		}
	}

	pCmdUI->Enable( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateInstancingCheckOutManifest( CCmdUI *pCmdUI )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc || !p4 )
	{
		pCmdUI->Enable( false );
		return;
	}

	CManifest *pManifest = CMapDoc::GetManifest();
	if ( pManifest == NULL )
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( pManifest->m_bIsVersionControlled == true && pManifest->m_bCheckedOut == false )
	{
		pCmdUI->Enable( true );
	}
	else
	{
		pCmdUI->Enable( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateInstancingAddManifest( CCmdUI *pCmdUI )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc || !p4 )
	{
		pCmdUI->Enable( false );
		return;
	}

	CManifest *pManifest = CMapDoc::GetManifest();
	if ( pManifest == NULL )
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( pManifest->m_bIsVersionControlled == false )
	{
		pCmdUI->Enable( true );
	}
	else
	{
		pCmdUI->Enable( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function will collapse all instances into the map ( and any children instances )
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancesCollapseAll()
{
	CollapseInstances( false );
}


//-----------------------------------------------------------------------------
// Purpose: This function will collapse all instances that are selected into the map ( and any children instances )
//-----------------------------------------------------------------------------
void CMapDoc::OnInstancesCollapseSelection()
{
	CollapseInstances( true );
}

//-----------------------------------------------------------------------------
// Purpose: Finds if this object/entity belongs to any FGD-defined AutoVisGroups
//-----------------------------------------------------------------------------
void CMapDoc::AddToFGDAutoVisGroups( CMapClass *pObject )
{
	if ( pGD != NULL )
	{
		CMapEntity *pEntity = dynamic_cast< CMapEntity * >( pObject );

		if ( pEntity != NULL )
		{
			int gindex = 0; // Index of FGD-defined AutoVisGroups
			int cindex = 0;	// Index of Classes
			int eindex = 0;	// Index of Entities			
			
			// Parent
			for( gindex = 0; gindex < pGD->m_FGDAutoVisGroups.Count(); gindex++ )
			{			
				// Class
				for ( cindex = 0; cindex < pGD->m_FGDAutoVisGroups[gindex].m_Classes.Count(); cindex++ )
				{
					// Object/Entity
					for ( eindex = 0; eindex < pGD->m_FGDAutoVisGroups[gindex].m_Classes[cindex].szEntities.Count(); eindex++ )
					{
						// 
						if ( !V_stricmp( pEntity->GetClassName(), pGD->m_FGDAutoVisGroups[gindex].m_Classes[cindex].szEntities[eindex] ) )
						{			
							AddChildGroupToAutoVisGroup( pEntity, pGD->m_FGDAutoVisGroups[gindex].m_Classes[cindex].szClass, pGD->m_FGDAutoVisGroups[gindex].szParent );
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add objects/entities to QuickHide
//-----------------------------------------------------------------------------
void CMapDoc::QuickHide_HideObjects( void )
{
	const CMapObjectList *pSelList = m_pSelection->GetList();

	for ( int index = 0; index < pSelList->Count(); index++ )
	{
		CMapClass *pObject = pSelList->Element( index );

		// Hide the object
		m_QuickHideGroup.AddToTail( pObject );
		
		// If it's a Group or Entity, add each child - otherwise we get ghost group selection (driller:  FIXME)
		if ( pObject->IsGroup() || pObject->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) )
		{
			EnumChildrenPos_t pos;
			CMapClass *pChild = pObject->GetFirstDescendent( pos );

			while ( pChild )
			{
				m_QuickHideGroup.AddToTail( pChild );

				pChild = pObject->GetNextDescendent( pos );
			}
		}

		// Does this object have a parent?
		if ( pObject->GetParent() )
		{
			CMapClass *pParent = pObject->GetParent();

			// Track all parents, so we can check if all children (eventually) end up hidden
			while ( pParent )
			{
				if ( pParent->IsGroup() || pParent->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) )
				{
					if ( m_QuickHideGroupedParents.Find( pParent ) == m_QuickHideGroupedParents.InvalidIndex() )
					{
						m_QuickHideGroupedParents.AddToTail( pParent );
					}
				}
				pParent = pParent->GetParent();
			}
		}
	}

	// Now check any parents
	if ( m_QuickHideGroupedParents.Count() > 0 )
	{
		int nHiddenChildCount = 0;

		for ( int index = 0; index < m_QuickHideGroupedParents.Count(); index++ )
		{
			EnumChildrenPos_t posParent;
			CMapClass *pParent = m_QuickHideGroupedParents[ index ];
			int nChildCount = pParent->GetChildCount();

			if ( pParent )
			{
				CMapClass *pChild = pParent->GetFirstDescendent( posParent );

				// Child visible?
				while ( pChild )
				{
					// With the way GetChildCount() works, we need to count nested children, too
					nChildCount += pChild->GetChildCount();
					
					if (  m_QuickHideGroup.Find( pChild ) != m_QuickHideGroup.InvalidIndex() )
					{
						nHiddenChildCount++;
					}

					pChild = pParent->GetNextDescendent( posParent );
				}
			}

			// If none of the children are visible...
			if ( nHiddenChildCount == nChildCount )
			{
				// ...also add the parent
				m_QuickHideGroup.AddToTail( pParent );
			}

			nHiddenChildCount = 0;
		}
	}

	m_pToolManager->GetActiveTool()->SetEmpty();
	UpdateVisibilityAll();
}

//-----------------------------------------------------------------------------
// Purpose: Add unselected objects/entities to QuickHide
//-----------------------------------------------------------------------------
void CMapDoc::QuickHide_HideUnselectedObjects( void )
{
	EnumChildrenPos_t pos;
	CMapWorld *pWorld = GetMapWorld();
	CMapClass *pObject = pWorld->GetFirstDescendent( pos );

	while ( pObject )
	{
		if ( !pObject->IsSelected() )
		{
			m_QuickHideGroup.AddToTail( pObject );
		}

		pObject = pWorld->GetNextDescendent( pos );
	}

	// Check each selected object for a parent...
	const CMapObjectList *pSelList = m_pSelection->GetList();
	for ( int index = 0; index < pSelList->Count(); index++ )
	{
		CMapClass *pSelectedObject = pSelList->Element( index );
		CMapClass *pSelectedParent = pSelectedObject->GetParent();

		while ( pSelectedParent )
		{
			// ...and nuke them from QuickHide
			m_QuickHideGroup.FindAndRemove( pSelectedParent );
			pSelectedParent = pSelectedParent->GetParent();
		}
	}

	UpdateVisibilityAll();
}

//-----------------------------------------------------------------------------
// Purpose: Restore any hidden objects and purge data
//-----------------------------------------------------------------------------
void CMapDoc::QuickHide_Unhide( void )
{
	if ( m_QuickHideGroup.Count() > 0 )
	{
		m_QuickHideGroup.RemoveAll();
		m_QuickHideGroupedParents.RemoveAll();

		UpdateVisibilityAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks the QuickHideGroup for pObject
//-----------------------------------------------------------------------------
bool CMapDoc::QuickHide_IsObjectHidden( CMapClass *pObject )
{
	for ( int index = 0; index < m_QuickHideGroup.Count(); index++ )
	{
		// FIXME: use references to detect objects being deleted
		if ( pObject == m_QuickHideGroup[index] )
		{			
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a visgroup with currently hidden objects
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_CreateVisGroupFromHidden( void )
{
	int iQuickHideObjects = m_QuickHideGroup.Count();

	if ( iQuickHideObjects > 0 )
	{
		CString str;
		str.Format( "_FromQuickHide(%d)", iQuickHideObjects );
		VisGroups_CreateNamedVisGroup( m_QuickHideGroup, str, true, false );
	}

	QuickHide_Unhide();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a token in the VMF, which we use for P4 check-in testing
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapDoc::QuickHide_SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo )
{
	if ( m_QuickHideGroup.Count() == 0)
	{
		return( ChunkFile_Ok );
	}

	ChunkFileResult_t eResult = pFile->BeginChunk( "quickhide" );

	if ( eResult == ChunkFile_Ok )
	{
		int nCount = m_QuickHideGroup.Count();
		eResult = pFile->WriteKeyValueInt( "count", nCount );
	}

	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->EndChunk();
	}

	return( eResult );
}

//-----------------------------------------------------------------------------
// Purpose: Menu/Button version of QuickHide_HideObjects
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_HideObjects( void )
{
	QuickHide_HideObjects();
}

//-----------------------------------------------------------------------------
// Purpose: Menu/Button version of QuickHide_HideUnselectedObjects
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_HideUnselectedObjects( void )
{
	QuickHide_HideUnselectedObjects();
}

//-----------------------------------------------------------------------------
// Purpose: Menu/Button version of OnQuickHide_Unhide
//-----------------------------------------------------------------------------
void CMapDoc::OnQuickHide_Unhide( void )
{
	QuickHide_Unhide();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapDoc::OnRadiusCulling( void )
{
	Options.general.bRadiusCulling = !Options.general.bRadiusCulling;
	UpdateVisibilityAll();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapDoc::OnUpdateRadiusCulling( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck( Options.general.bRadiusCulling ? 1 : 0 );
	pCmdUI->Enable(!GetMainWnd()->IsShellSessionActive());
}


#include <tier0/memdbgoff.h>