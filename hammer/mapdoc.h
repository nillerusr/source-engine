//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef MAPDOC_H
#define MAPDOC_H
#ifdef _WIN32
#pragma once
#endif

#include "MapClass.h"
#include "Selection.h"
#include "MapEntity.h"
#include "GameConfig.h"
#include "filesystem.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstack.h"

class CToolManager;
class CMapDoc;
class CGameConfig;
class CHistory;
class CMapGroup;
class CMapView;
class CMapView3D;
class CMapView2D;
class IBSPLighting;
class CRender;
class CManifest;

struct FindEntity_t;
struct FindGroup_t;
struct AddNonSelectedInfo_t;


enum SelectionHandleMode_t;
enum MAPFORMAT;
enum ToolID_t;


enum
{
	// hints what recently changed
	MAPVIEW_UPDATE_OBJECTS			= 0x001,	// a world object has changed (pos, size etc)
	MAPVIEW_UPDATE_ANIMATION		= 0x002,	// an object animation has been changed
	MAPVIEW_UPDATE_COLOR			= 0x004,	// an object color has been changed
	MAPVIEW_UPDATE_SELECTION		= 0x008,	// the current selection has changed
	MAPVIEW_UPDATE_TOOL				= 0x010,	// the current tool has been changed

	// what views should be updated
	MAPVIEW_UPDATE_ONLY_2D			= 0x020,	// update only 2D views for some reason
	MAPVIEW_UPDATE_ONLY_3D			= 0x040,	// update only 3D views for some reason
	MAPVIEW_UPDATE_ONLY_LOGICAL		= 0x080,	// update only Logical views for some reason
		
	MAPVIEW_OPTIONS_CHANGED			= 0x100,	// view options has been changed
	MAPVIEW_UPDATE_VISGROUP_STATE	= 0x200,	// a visgroup was hidden or shown.
	MAPVIEW_UPDATE_VISGROUP_ALL		= 0x400,	// a visgroup was created or deleted.

	MAPVIEW_RENDER_NOW				= 0x800,	// bypass the main frame loop and render now
};


// Flags for view2dinfo.wflags
enum
{
	VI_ZOOM = 0x01,
	VI_CENTER = 0x02
};


// Display modes for instances
enum ShowInstance_t
{
	INSTANCES_HIDE = 0,			// instance contents are not displayed
	INSTANCES_SHOW_TINTED,		// instance contents are displayed with a yellowish tint
	INSTANCES_SHOW_NORMAL		// instance contents are displayed normally
};


enum VMFLoadFlags_t
{
	VMF_LOAD_ACTIVATE = 0x01,	// loading map should be activated
	VMF_LOAD_IS_SUBMAP = 0x02,	// loading map is part of an instance / manifest
};

typedef struct
{
	WORD wFlags;
	float fZoom;
	Vector ptCenter;
} VIEW2DINFO;

struct ExportDXFInfo_s
{
	int nObject;
	bool bVisOnly;
	CMapWorld *pWorld;
	FILE *fp;
};


//
// The doc holds a list of objects with dependents that changed since the last render. The list
// must be processed before rendering so that interobject dependencies are resolved.
//
struct NotifyListEntry_t
{
	CSmartPtr< CSafeObject< CMapClass > > pObject;
	Notify_Dependent_t eNotifyType;
};


//
// To pass as hint to UpdateAllViews.
//
class UpdateBox : public CObject
{
	public:

		UpdateBox(void) { Objects = NULL; }

		CMapObjectList *Objects;
		BoundBox Box;
};

// this holds all of the data needed to visualize the level's portal file
struct portalfile_t
{
	CString				fileName;
	int					totalVerts;
	CUtlVector<Vector>	verts;
	CUtlVector<int>		vertCount;
};


struct UpdateVisibilityData_t
{
	CMapDoc *pDoc;
	bool bRadiusCullingEnabled;
	Vector vecRadiusCullCenter;
	float flRadiusCullDistSq;
};


// this is a structure to manage clipboards
class IHammerClipboard
{
protected:
	IHammerClipboard(){}
	virtual ~IHammerClipboard() {}

public:
	static IHammerClipboard *CreateInstance();

public:
	virtual void Destroy() = 0;
};


class CMapDoc : public CDocument
{
	friend class CManifest;

	protected:
		DECLARE_DYNCREATE(CMapDoc)

	public:

		// attribs:
		bool m_bSnapToGrid;
		bool m_bShowGrid;
		bool m_bShowLogicalGrid;

		// pointfile stuff:
		enum
		{
			PFPNext = 1,
			PFPPrev = -1
		};

		int m_iCurPFPoint;
		CUtlVector<Vector> m_PFPoints;
		CString m_strLastPointFile;
		portalfile_t *m_pPortalFile;

		static inline CMapDoc *GetActiveMapDoc(void);
		static void SetActiveMapDoc(CMapDoc *pDoc);
		static void ActivateMapDoc( CMapDoc *pDoc );
		static inline CManifest *GetManifest(void);
		static inline int GetInLevelLoad( );

	private:

		static CMapDoc		*m_pMapDoc;
		static CManifest	*m_pManifest;
		static int			m_nInLevelLoad;

	public:

		CMapDoc(void);
		virtual ~CMapDoc();

		virtual void Initialize( void );

		virtual bool IsManifest( void ) { return false; }

		IBSPLighting	*GetBSPLighting()	{ return m_pBSPLighting; }
		CToolManager	*GetTools()			{ return m_pToolManager; } // return tools working on this document
		CSelection		*GetSelection()		{ return m_pSelection;	} // return current selection

		void BeginShellSession(void);
		void EndShellSession(void);
		bool IsShellSessionActive(void);

		inline int GetDocVersion(void);
		inline void DecrementDocVersion(void);

		//
		// Interface to the map document. Functions for object creation, deletion, etc.
		//
		CMapEntity *CreateEntity(const char *pszClassName, float x, float y, float z);
		bool DeleteEntity(const char *pszClassName, float x, float y, float z);

		CMapEntity *FindEntity(const char *pszClassName, float x, float y, float z);

		CMapEntity *FindEntityByName( const char *pszName, bool bVisiblesOnly );
		bool FindEntitiesByKeyValue(CMapEntityList &Found, const char *szKey, const char *szValue, bool bVisiblesOnly);
		bool FindEntitiesByName(CMapEntityList &Found, const char *szName, bool bVisiblesOnly);
		bool FindEntitiesByClassName(CMapEntityList &Found, const char *szClassName, bool bVisiblesOnly);
		bool FindEntitiesByNameOrClassName(CMapEntityList &Found, const char *pszName, bool bVisiblesOnly);

		virtual void Update(void);
		virtual void SetModifiedFlag(BOOL bModified = TRUE);
		void SetAutosaveFlag(BOOL bAutosave = TRUE);

		BOOL NeedsAutosave();
		BOOL IsAutosave();	
		CString *AutosavedFrom();

		void	AddReference( void );
		void	RemoveReference( void );
		int		GetReferenceCount( void ) { return m_nExternalReferenceCount; }

		// Used to track down a potential crash.
		bool AnyNotificationsForObject(CMapClass *pObject);
		
		void SetAnimationTime( float time );
		float GetAnimationTime( void ) { return m_flAnimationTime; }
		bool IsAnimating( void ) { return m_bIsAnimating; }

		// other stuff
		float m_flCurrentTime;
		void UpdateCurrentTime( void );
		float GetTime( void ) { return m_flCurrentTime; }

		void GotoPFPoint(int iDirection);

		// world cordon
		bool IsCordoning(void);
		bool SetCordoning( bool bState);
		void GetCordon( Vector &mins, Vector &maxs);
		void SetCordon( const Vector &mins, const Vector &maxs);
		CMapWorld *CordonCreateWorld();
		ChunkFileResult_t CordonSaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
		Vector		m_vCordonMins;
		Vector		m_vCordonMaxs;
		bool		m_bIsCordoning;

		CMapView *GetActiveMapView();
		CMapView3D *GetFirst3DView();
		void Snap(Vector &pt, int nFlags = 0);
		inline bool IsSnapEnabled(void);

		//
		// Face selection for face editing.
		//
		void SelectFace(CMapSolid *pSolid, int iFace, int cmd);
		void UpdateForApplicator(BOOL bApplicator);

		
		void UpdateAnimation( void );

		void DeleteObject(CMapClass *pObject);
		void DeleteObjectList(CMapObjectList &List);

		void InternalEnableLightPreview( bool bCustomFilename );

		//
		// Object selection:
		//
		bool SelectObject(CMapClass *pobj, int cmd = scSelect);
		void SelectObjectList(const CMapObjectList *pList, int cmd = (scSelect|scClear|scSaveChanges) );
		void SelectRegion( BoundBox *pBox, bool bInsideOnly, bool ResetSelection = true );
		void SelectLogicalRegion( const Vector2D &vecMins, const Vector2D &vecMaxs, bool bInsideOnly);

		// View centering.
		void CenterViewsOnSelection();
		void CenterViewsOn(const Vector &vec);
		void CenterLogicalViewsOnSelection();
		void CenterLogicalViewsOn(const Vector2D &vecLogical);
		void Center2DViewsOnSelection();
		void Center2DViewsOn(const Vector &vec);
		void Center3DViewsOnSelection();
		void Center3DViewsOn(const Vector &vPos);
		void Set3DViewsPosAng( const Vector &vPos, const Vector &vAng );
		void GetSelectedCenter(Vector &vCenter);

		void GetBestVisiblePoint(Vector &ptOrg);
		void Cut( IHammerClipboard *pClipboard );
		void Copy( IHammerClipboard *pClipboard = NULL );
		void Paste(CMapObjectList &Objects, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix);
		void PasteInstance(CMapObjectList &Objects, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix);
		void Paste( IHammerClipboard *pClipboard, CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix );
		void Delete( void );

		// FIXME: refactor these!
		void CloneObjects(const CMapObjectList &Objects);
		void NudgeObjects(const Vector &Delta, bool bClone);
		void GetNudgeVector(const Vector& vHorz, const Vector& vVert, int nChar, bool bSnap, Vector &vecNudge);

		void GetBestPastePoint(Vector &vecPasteOrigin);
		void UpdateStatusbar();
		void UpdateStatusBarSnap();
		void SetView2dInfo(VIEW2DINFO& vi);
		void SetViewLogicalInfo(VIEW2DINFO& vi);
		void SetActiveView(CMapView *pViewActivate);
		void SetUndoActive(bool bActive);
		void UpdateTitle(CView*);

		void CountSolids();
		void CountSolids2();
		
		void ReplaceTextures(
			LPCTSTR pszFind, 
			LPCTSTR pszReplace, 
			BOOL bEverything, 
			int iAction, 
			BOOL bHidden, 
			bool bRescaleTextureCoordinates);

        void BatchReplaceTextures( FileHandle_t fp );

		bool Is3DGridEnabled(void) { return(m_bShow3DGrid); }
		ShowInstance_t	GetShowInstance( void ) { return m_tShowInstance; }

		void ReleaseVideoMemory( );

		inline MAPFORMAT GetMapFormat(void);
		inline CMapWorld *GetMapWorld(void);
		inline CGameConfig *GetGame(void);
		inline int GetGridSpacing(void) { return(max(m_nGridSpacing, 1)); }

		inline CHistory *GetDocHistory(void);

		inline int GetNextMapObjectID(void);
		inline int GetNextNodeID(void);
		inline void SetNextNodeID(int nID);

		static int GetDocumentCount();
		static CMapDoc *GetDocument(int index);

		void SetMRU(CMapView2D *pView);
		void RemoveMRU(CMapView2D *pView);
		CUtlVector<CMapView2D*> MRU2DViews;

		// if theres anything the document whats to show, do it in RenderDocument()
		void RenderDocument(CRender *pRender);

		void RenderAllViews(void);
		BOOL SelectDocType(void);
		BOOL SaveModified(void);

		// Set edit prefab mode.
		void EditPrefab3D(DWORD dwPrefabID);

		//
		// Call these when modifying the document contents.
		//
		virtual void AddObjectToWorld(CMapClass *pObject, CMapClass *pParent = NULL);
		BOOL FindObject(CMapClass *pObject);
		void RemoveObjectFromWorld(CMapClass *pMapClass, bool bNotifyChildren);
		void RenderPreloadObject(CMapClass *pObject);
		void UpdateObject(CMapClass *pMapClass);
		void UpdateVisibilityAll(void);
		void UpdateVisibility(CMapClass *pObject);
	        void NotifyDependents(CMapClass *pObject, Notify_Dependent_t eNotifyType);

		// Radius culling
		bool IsCulledBy3DCameraDistance( CMapClass *pObject, UpdateVisibilityData_t *pData );

		//
		// Morph tool.
		//
		void Morph_SelectObject(CMapSolid *pSolid, unsigned int uCmd);
		void Morph_GetBounds(Vector &mins, Vector &maxs, bool bReset);
		int Morph_GetObjectCount(void);
		CSSolid *Morph_GetFirstObject(POSITION &pos);
		CSSolid *Morph_GetNextObject(POSITION &pos);

		//
		inline bool IsDispSolidDrawMask( void ) { return m_bDispSolidDrawMask; }
		inline void SetDispDrawWalkable( bool bValue ) { m_bDispDrawWalkable = bValue; }
		inline bool IsDispDrawWalkable( void )  { return m_bDispDrawWalkable; }
		inline bool IsDispDraw3D()  { return m_bDispDraw3D; }
		inline void SetDispDrawBuildable( bool bValue ) { m_bDispDrawBuildable = bValue; }
		inline bool IsDispDrawBuildable( void ) { return m_bDispDrawBuildable; }
		inline void SetDispDrawRemove( bool bValue ) { m_bDispDrawRemove = bValue; }
		inline bool IsDispDrawRemove( void ) { return m_bDispDrawRemove; }
		inline bool IsDispDrawRemovedVerts( void ) { return m_bDispDrawRemovedVerts; }
		inline void SetDispDrawRemovedVerts( bool bValue ) { m_bDispDrawRemovedVerts = bValue; }

		//
		// List of VisGroups.
		//
		void ShowNewVisGroupsDialog(CMapObjectList &Objects, bool bUnselectObjects);
		void VisGroups_CreateNamedVisGroup(CMapObjectList &Objects, const char *szName, bool bHide, bool bRemoveFromOtherVisGroups);
		void VisGroups_AddObjectsToVisGroup(CMapObjectList &Objects, CVisGroup *pVisGroup, bool bHide, bool bRemoveFromOtherVisGroups);
		CVisGroup *VisGroups_AddGroup(CVisGroup *pGroup);
		bool VisGroups_ObjectCanBelongToVisGroup(CMapClass *pObject);
		CVisGroup *VisGroups_AddGroup(LPCTSTR pszName, bool bAuto = false);
		static BOOL VisGroups_CheckForGroupCallback(CMapClass *pObject, CVisGroup *pGroup);
		int VisGroups_GetCount();
		CVisGroup *VisGroups_GetVisGroup(int nIndex);
		int VisGroups_GetRootCount();
		CVisGroup *VisGroups_GetRootVisGroup(int nIndex);
		CVisGroup *VisGroups_GroupForID(DWORD id);
		CVisGroup *VisGroups_GroupForName( const char *pszName, bool bIsAuto );
		void VisGroups_PurgeGroups();
		void VisGroups_MoveUp(CVisGroup *pGroup);
		void VisGroups_MoveDown(CVisGroup *pGroup);
		void VisGroups_ShowVisGroup(CVisGroup *pGroup, bool bShow);
		void VisGroups_UpdateAll();
		void VisGroups_UpdateForObject(CMapClass *pObject);
		void VisGroups_SetParent(CVisGroup *pVisGroup, CVisGroup *pNewParent);
		bool VisGroups_CanMoveUp(CVisGroup *pGroup);
		bool VisGroups_CanMoveDown(CVisGroup *pGroup);
		void VisGroups_RemoveGroup(CVisGroup *pGroup);
		void VisGroups_CombineGroups(CVisGroup *pFrom, CVisGroup *pTo);
		bool VisGroups_LockUpdates( bool bLock );
		void VisGroups_CheckMemberVisibility(CVisGroup *pGroup);

		// Quick Hide
		void QuickHide_HideObjects( void );
		void QuickHide_HideUnselectedObjects( void );
		void QuickHide_Unhide( void );
		bool QuickHide_IsObjectHidden( CMapClass *pObject );
		ChunkFileResult_t QuickHide_SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo );
		CUtlVector< CMapClass* > m_QuickHideGroup;
		CUtlVector< CMapClass* > m_QuickHideGroupedParents;

		// Default logical placement for new entities
		void GetDefaultNewLogicalPosition( Vector2D &vecPosition );

	private:

		void VisGroups_Validate();
		void VisGroups_UpdateParents();
		void VisGroups_UnlinkGroup(CVisGroup *pGroup);
		void VisGroups_DoRemoveOrCombine(CVisGroup *pFrom, CVisGroup *pTo);

		bool FindNotification(CMapClass *pObject, Notify_Dependent_t eNotifyType);

	protected:

		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CMapDoc)
		public:
		virtual BOOL OnNewDocument();
		virtual void DeleteContents();
		virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
		virtual void OnCloseDocument(void);
		virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
		//}}AFX_VIRTUAL

		BOOL Serialize(std::fstream &file, BOOL fIsStoring, BOOL bRMF);
		
		// Save a VMF file. saveFlags is a combination of SAVEFLAGS_ defines.
		bool SaveVMF(const char *pszFileName, int saveFlags );

		bool LoadVMF( const char *pszFileName, int LoadFlags = VMF_LOAD_ACTIVATE );
		void Postload(const char *pszFileName);
		inline bool IsLoading(void);

		inline void SetInitialUpdate( void ) { m_bHasInitialUpdate = true; }
		inline bool HasInitialUpdate( void ) { return m_bHasInitialUpdate; }

		// manifest routines
		void			SetManifest( CManifest *pManifest );
		void			SetEditable( bool IsEditable ) { m_bIsEditable = IsEditable; }
		bool			IsEditable( void ) { return m_bIsEditable; }
		bool			IsSelectionEditable( void );
		bool			CreateNewManifest( void );
		CMapWorld		*GetCurrentWorld( void );
		int				GetClipboardCount( void );
		void			ManifestPaste( CMapWorld *pDestWorld, Vector vecOffset, QAngle vecRotate, CMapClass *pParent, bool bMakeEntityNamesUnique, const char *pszEntityNamePrefix );
		virtual void	UpdateInstanceMap( CMapDoc *pInstanceMapDoc );
		void			CollapseInstances( bool bOnlySelected );
		void			PopulateInstanceParms_r( CMapEntity *pEntity, const CMapObjectList *pChildren, CUtlVector< CString > &ParmList );
		void			PopulateInstanceParms( CMapEntity *pEntity );
		void			PopulateInstance( CMapEntity *pEntity );
		void			InstanceMoved( void );

		void		ShowWindow( bool bIsVisible );
		bool		IsVisible( void );

		void BuildAllDetailObjects();

		void ExpandObjectKeywords(CMapClass *pObject, CMapWorld *pWorld);

		#ifdef _DEBUG
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext& dc) const;
		#endif

		void UpdateAllCameras(const Vector *vecViewPos, const Vector *vecLookAt, const float *fZoom);
		void UpdateAllViews(int nFlags, UpdateBox *ub = NULL );

		void SetSmoothingGroupVisual( int iGroup )		{ m_SmoothingGroupVisual = iGroup; }
		int GetSmoothingGroupVisual( void )				{ return m_SmoothingGroupVisual; }

		void AddToAutoVisGroup( CMapClass *pObject );
		void AddToAutoVisGroup( CMapClass *pObject, const char *pAutoVisGroup );
		void AddChildGroupToAutoVisGroup( CMapClass *pObject, const char *pAutoVisGroup, const char *pParentName );
		void RemoveFromAutoVisGroups( CMapClass *pObject );
		void AddToFGDAutoVisGroups( CMapClass *pObject );

		// Builds a list of all objects which are connected to outputs of pObj 
		void BuildCascadingSelectionList( CMapClass *pObj, CUtlRBTree< CMapClass*, unsigned short > &list, bool bRecursive );

		void Public_SaveMap() { OnFileSave(); }
	protected:

		void AssignAllToAutoVisGroups();
		CVisGroup *GetRootAutoVisGroup();
			
		// Tools:
		CToolManager *m_pToolManager;

		int m_nGridSpacing;

		bool m_bDispSolidDrawMask;
		bool m_bDispDrawWalkable;
		bool m_bDispDraw3D;
		bool m_bDispDrawBuildable;
		bool m_bDispDrawRemove;
		bool m_bDispDrawRemovedVerts;

		bool m_bHasInitialUpdate;
		bool m_bLoading; // Set to true while we are being loaded from VMF.

		static BOOL GetBrushNumberCallback(CMapClass *pObject, void *pFindInfo);

		//
		// Serialization.
		//
		ChunkFileResult_t SaveVersionInfoVMF(CChunkFile *pFile, bool bIsAutosave = false);
		ChunkFileResult_t VisGroups_SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
		ChunkFileResult_t SaveViewSettingsVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);

		static bool HandleLoadError(CChunkFile *pFile, const char *szChunkName, ChunkFileResult_t eError, CMapDoc *pDoc);
		static ChunkFileResult_t LoadCordonCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadCordonKeyCallback(const char *pszKey, const char *pszValue, CMapDoc *pDoc);
		static ChunkFileResult_t LoadEntityCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadHiddenCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadGroupKeyCallback(const char *szKey, const char *szValue, CMapGroup *pGroup);
		static ChunkFileResult_t LoadVersionInfoCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadVersionInfoKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc);
		static ChunkFileResult_t LoadAutosaveCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadAutosaveKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc);		
		static ChunkFileResult_t LoadWorldCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadViewSettingsCallback(CChunkFile *pFile, CMapDoc *pDoc);
		static ChunkFileResult_t LoadViewSettingsKeyCallback(const char *szKey, const char *szValue, CMapDoc *pDoc);

		//
		// Search functions.
		//
		static BOOL FindEntityCallback(CMapClass *pObject, FindEntity_t *pFindInfo);
		static BOOL FindGroupCallback(CMapGroup *pGroup, FindGroup_t *pFindInfo);

		void AssignToVisGroups(void);
		void AssignToGroups(void);
		void RemoveEmptyGroups(void);
		void CountGUIDs(void);

		void InitUpdateVisibilityData( UpdateVisibilityData_t &data );
		bool ShouldObjectBeVisible( CMapClass *pObject, UpdateVisibilityData_t *pData );
		static BOOL UpdateVisibilityCallback( CMapClass *pObject, UpdateVisibilityData_t *pData );

		bool GetChildrenToHide(CMapClass *pObject, bool bSelected, CMapObjectList &List);

		//
		// Interobject dependency notification.
		//
		void ProcessNotifyList();
		void DispatchNotifyDependents(CMapClass *pObject, Notify_Dependent_t eNotifyType);

		CUtlVector<NotifyListEntry_t > m_NotifyList;

		CMapWorld *m_pWorld;				// The world that this document represents.
		CMapObjectList m_UpdateList;		// List of objects that have changed since the last call to Update.
		CString m_strLastExportFileName;	// The full path that we last exported this document to. 
		int m_nDocVersion;					// A number that increments every time the doc is modified after being saved.
		BOOL m_bNeedsAutosave;				// True if the document has been changed and needs autosaved.
		BOOL m_bIsAutosave;
		CString m_strAutosavedFrom;
			
		// Undo/Redo system.
		CHistory *m_pUndo;
		CHistory *m_pRedo;

		CSelection *m_pSelection;				// object selection list
		
		int m_nNextMapObjectID;			// The ID that will be assigned to the next CMapClass object in this document.
		int m_nNextNodeID;				// The ID that will be assigned to the next "info_node_xxx" object created in this document.

		// Editing prefabs data.
		DWORD m_dwPrefabID;
		DWORD m_dwPrefabLibraryID;
		BOOL m_bEditingPrefab;
		bool m_bPrefab;					// true if this document IS a prefab, false if not.

		// Game configuration.
		CGameConfig *m_pGame;

		bool m_bShow3DGrid;				// Whether to render a grid in the 3D views.
		bool m_bHideItems;				// Whether to render point entities in all views.
		
		int					m_nExternalReferenceCount;	// Indicates how many external references ( instances ) are pointing to this map
		ShowInstance_t		m_tShowInstance;	// Indicates how instance contents should be displayed
		CManifest			*m_pManifestOwner;
		bool				m_bIsEditable;
		bool				m_bCollapsingInstances;

		//
		// Animation.
		//
		float m_flAnimationTime;		// Current time in the animation
		bool m_bIsAnimating;

		IBSPLighting		*m_pBSPLighting;

		//
		// Visgroups.
		//
		CUtlVector<CVisGroup *> *m_VisGroups;
		CUtlVector<CVisGroup *> *m_RootVisGroups;
		bool m_bVisGroupUpdatesLocked;

		int				m_SmoothingGroupVisual;

		int	m_nLogicalPositionCount;

		//
		// Expands %i keyword in prefab targetnames to generate unique targetnames for this map.
		//
		bool ExpandTargetNameKeywords(char *szNewTargetName, const char *szOldTargetName, CMapWorld *pWorld);
		bool DoExpandKeywords(CMapClass *pObject, CMapWorld *pWorld, char *szOldKeyword, char *szNewKeyword);

		// Renames all named entities in pRoot
		void RenameEntities( CMapClass *pRoot, CMapWorld *pWorld, bool bMakeUnique, const char *pszPrefix );

		void CenterOriginsRecursive(CMapClass *pObject);
		void SnapObjectsRecursive(CMapClass *pObject);

		// Add all entities connected to all entities in the selection list recursively
		void AddConnectedNodes( CMapClass *pClass, CUtlRBTree< CMapClass*, unsigned short >& list );

		//{{AFX_MSG(CMapDoc)
		afx_msg void OnEditDelete();
		afx_msg void OnMapSnaptogrid();
		afx_msg void OnMapEntityGallery();
		afx_msg void OnUpdateMapSnaptogrid(CCmdUI* pCmdUI);
		afx_msg void OnEditApplytexture();
		afx_msg void OnToolsSubtractselection();
		afx_msg void OnToolsCenterOrigins();
		afx_msg void OnEnableLightPreview();
		afx_msg void OnEnableLightPreviewCustomFilename();
		afx_msg void OnDisableLightPreview();
		afx_msg void OnToggleLightPreview();
		afx_msg void OnUpdateLightPreview();
		afx_msg void OnAbortLightCalculation();
		afx_msg void OnEditCopy();
		afx_msg void OnEditPaste();
		afx_msg void OnUpdateEditSelection(CCmdUI *pCmdUI);
		afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
		afx_msg void OnEditCut();
		afx_msg void OnEditReplace();
		afx_msg void OnToolsGroup();
		afx_msg void OnToolsUngroup();
		afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);
		afx_msg void OnUpdateViewLogicalGrid(CCmdUI* pCmdUI);
		afx_msg void OnEditSelectall();
		afx_msg void OnFileSaveAs();
		afx_msg void OnFileSave();
		afx_msg void OnMapGridlower();
		afx_msg void OnMapGridhigher();
		afx_msg void OnEditToworld();
		afx_msg void OnEditToWorld();
		afx_msg void OnFileExport();
		afx_msg void OnFileExportAgain();
		afx_msg void OnEditMapproperties();
		afx_msg void OnFileConvertWAD();
		afx_msg void OnUpdateFileConvertWAD(CCmdUI* pCmdUI);
		afx_msg void OnFileRunmap();
		afx_msg void OnToolsHideitems();
		afx_msg void OnViewHideUnconnectedEntities();
		afx_msg void OnUpdateViewHideUnconnectedEntities(CCmdUI* pCmdUI);
		afx_msg void OnUpdateToolsSubtractselection(CCmdUI* pCmdUI);
		afx_msg void OnUpdateToolsHideitems(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditDelete(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditFunction(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditMapproperties(CCmdUI* pCmdUI);
		afx_msg void OnMapInformation();

		// View menu
		afx_msg void OnViewGrid();
		afx_msg void OnViewLogicalGrid();
		afx_msg void OnViewCenterOnSelection();
		afx_msg void OnViewCenter3DViewsOnSelection();
		afx_msg BOOL OnViewHideObjects(UINT nID);
		afx_msg void OnQuickHide_HideObjects();
		afx_msg void OnQuickHide_HideUnselectedObjects();
		afx_msg void OnQuickHide_Unhide();
		afx_msg void OnQuickHide_UpdateUnHide(CCmdUI *pCmdUI);
		afx_msg void OnQuickHide_CreateVisGroupFromHidden();
		afx_msg void OnQuickHide_UpdateCreateVisGroupFromHidden(CCmdUI *pCmdUI);
		afx_msg void OnViewShowconnections();
		afx_msg void OnViewGotoBrush(void);
		afx_msg void OnViewGotoCoords(void);
		afx_msg void OnViewShowHelpers();
		afx_msg void OnViewShowModelsIn2D();
		afx_msg void OnViewPreviewModelFade();
		afx_msg void OnCollisionWireframe();
		afx_msg void OnShowDetailObjects();
		afx_msg void OnShowNoDrawBrushes();
		afx_msg void OnRadiusCulling();
		afx_msg void OnUpdateRadiusCulling(CCmdUI *pCmdUI);

		afx_msg void OnToolsHollow();
		afx_msg void OnEditPastespecial();
		afx_msg void OnUpdateEditPastespecial(CCmdUI* pCmdUI);
		afx_msg void OnEditSelnext();
		afx_msg void OnEditSelprev();
		afx_msg void OnEditSelnextCascading();
		afx_msg void OnEditSelprevCascading();
		afx_msg void OnLogicalMoveBlock();
		afx_msg void OnLogicalSelectAllCascading();
		afx_msg void OnLogicalSelectAllConnected();
		afx_msg void OnLogicalobjectLayoutgeometric();
		afx_msg void OnLogicalobjectLayoutdefault();
		afx_msg void OnLogicalobjectLayoutlogical();
		afx_msg void OnMapCheck();
		afx_msg void OnUpdateViewShowconnections(CCmdUI* pCmdUI);
		afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
		afx_msg void OnToolsCreateprefab();
		afx_msg void OnInsertprefabOriginal();
		afx_msg void OnEditReplacetex();
		afx_msg void OnToolsSnapselectedtogrid();
		afx_msg void OnToolsSnapSelectedToGridIndividually();
		afx_msg void OnUpdateToolsSplitface(CCmdUI* pCmdUI);
		afx_msg void OnToolsSplitface();
		afx_msg void OnToolsTransform();
		afx_msg void OnToolsToggletexlock();
		afx_msg void OnUpdateToolsToggletexlock(CCmdUI* pCmdUI);
		afx_msg void OnToolsToggletexlockScale();
		afx_msg void OnUpdateToolsToggletexlockScale(CCmdUI* pCmdUI);
		afx_msg void OnToolsTextureAlignment(void);
		afx_msg void OnUpdateToolsTextureAlignment(CCmdUI *pCmdUI);
		afx_msg void OnToggleCordon();
		afx_msg void OnUpdateToggleCordon(CCmdUI* pCmdUI);
		afx_msg void OnUpdateShowNoDrawBrushes(CCmdUI* pCmdUI);
		afx_msg void OnEditCordon();
		afx_msg void OnUpdateEditCordon(CCmdUI* pCmdUI);
		afx_msg void OnUpdateViewHideUnselectedObjects(CCmdUI* pCmdUI);
		afx_msg void OnToggleGroupignore();
		afx_msg void OnUpdateToggleGroupignore(CCmdUI* pCmdUI);
		afx_msg void OnVscaleToggle();
		afx_msg void OnMapEntityreport();
		afx_msg void OnToggleSelectbyhandle();
		afx_msg void OnUpdateToggleSelectbyhandle(CCmdUI* pCmdUI);
		afx_msg void OnToggleInfiniteselect();
		afx_msg void OnUpdateToggleInfiniteselect(CCmdUI* pCmdUI);
		afx_msg void OnFileExporttodxf();
		afx_msg void OnUpdateEditApplytexture(CCmdUI* pCmdUI);
		afx_msg void OnMapLoadpointfile();
		afx_msg void OnMapUnloadpointfile();
		afx_msg void OnMapLoadportalfile();
		afx_msg void OnMapUnloadportalfile();
		afx_msg void OnUpdate3dViewUI(CCmdUI* pCmdUI);
		afx_msg BOOL OnChange3dViewType(UINT nID);
		afx_msg void OnEditToEntity();
		afx_msg BOOL OnUndoRedo(UINT nID);
		afx_msg void OnUpdateUndoRedo(CCmdUI* pCmdUI);
		afx_msg void OnChangeVertexscale();
		afx_msg void OnUpdateGroupEditFunction(CCmdUI* pCmdUI);
		afx_msg void OnUpdateFileExport(CCmdUI *pCmdUI);
		afx_msg void OnEditClearselection();
		afx_msg void OnUpdateToggle3DGrid(CCmdUI* pCmdUI);
		afx_msg void OnMapShowSelectedBrushNumber();
		afx_msg void OnEditFindEntities(void);
		afx_msg void OnToolsHideEntityNames(void);
		afx_msg void OnUpdateToolsHideEntityNames(CCmdUI *pCmdUI);
		afx_msg void OnToggleDispSolidMask( void );
		afx_msg void OnUpdateToggleSolidMask(CCmdUI* pCmdUI);
		afx_msg void OnToggleDispDrawWalkable( void );
		afx_msg void OnUpdateToggleDispDrawWalkable(CCmdUI* pCmdUI);
		afx_msg void OnToggleDispDraw3D( void );
		afx_msg void OnUpdateToggleDispDraw3D(CCmdUI* pCmdUI);
		afx_msg void OnToggleDispDrawBuildable( void );
		afx_msg void OnUpdateToggleDispDrawBuildable(CCmdUI* pCmdUI);
		afx_msg void OnToggleDispDrawRemovedVerts( void );
		afx_msg void OnUpdateToggleDispDrawRemovedVerts(CCmdUI* pCmdUI);
		afx_msg void OnUpdateViewShowHelpers(CCmdUI *pCmdUI);
		afx_msg void OnUpdateViewShowModelsIn2D(CCmdUI *pCmdUI);
		afx_msg void OnUpdateViewPreviewModelFade(CCmdUI *pCmdUI);
		afx_msg void OnUpdateCollisionWireframe(CCmdUI *pCmdUI);
		afx_msg void OnUpdateShowDetailObjects(CCmdUI *pCmdUI);
		afx_msg void OnMapDiff();
		afx_msg void OnToolsInstancesHide(void);
		afx_msg void OnUpdateToolsInstancesHide(CCmdUI *pCmdUI);
		afx_msg void OnToolsInstancesShowTinted(void);
		afx_msg void OnUpdateToolsInstancesShowTinted(CCmdUI *pCmdUI);
		afx_msg void OnToolsInstancesShowNormal(void);
		afx_msg void OnUpdateToolsInstancesShowNormal(CCmdUI *pCmdUI);
		afx_msg void OnInstancesHideAll( void );
		afx_msg void OnInstancesShowAll( void );

	public:
		afx_msg void OnToggle3DGrid();
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()
		afx_msg void OnInstancingCreatemanifest();
		afx_msg void OnUpdateInstancingCreatemanifest(CCmdUI *pCmdUI);
		afx_msg void OnInstancingCheckinAll();
		afx_msg void OnUpdateInstancingCheckinAll( CCmdUI *pCmdUI );
		afx_msg void OnInstancingCheckOutManifest();
		afx_msg void OnUpdateInstancingCheckOutManifest( CCmdUI *pCmdUI );
		afx_msg void OnInstancingAddManifest();
		afx_msg void OnUpdateInstancingAddManifest( CCmdUI *pCmdUI );
		afx_msg void OnInstancesCollapseAll();
		afx_msg void OnInstancesCollapseSelection();
};


//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline void
//-----------------------------------------------------------------------------
void CMapDoc::DecrementDocVersion(void)
{
	if (m_nDocVersion > 0)
	{
		m_nDocVersion--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if we are loading, false if not.
//-----------------------------------------------------------------------------
bool CMapDoc::IsLoading(void)
{
	return(m_bLoading);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the document that is currently active.
//-----------------------------------------------------------------------------
CMapDoc *CMapDoc::GetActiveMapDoc(void)
{
	return(m_pMapDoc);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the manifest associated with the active document.
//-----------------------------------------------------------------------------
CManifest *CMapDoc::GetManifest(void)
{
	return m_pManifest;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current level loading depth.  0 = not loading level
//-----------------------------------------------------------------------------
int CMapDoc::GetInLevelLoad( )
{
	return m_nInLevelLoad;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current version of the document that is being worked on.
//-----------------------------------------------------------------------------
int CMapDoc::GetDocVersion(void)
{
	return(m_nDocVersion);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the game configuration for this document.
//-----------------------------------------------------------------------------
CGameConfig *CMapDoc::GetGame(void)
{
	return(m_pGame);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the Undo system for this document.
//-----------------------------------------------------------------------------
CHistory *CMapDoc::GetDocHistory(void)
{
	return(m_pUndo);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the map format of the game configuration for this document.
//-----------------------------------------------------------------------------
MAPFORMAT CMapDoc::GetMapFormat(void)
{
	if (m_pGame != NULL)
	{
		return(m_pGame->mapformat);
	}

	return(mfHalfLife2);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the world that this document edits.
//-----------------------------------------------------------------------------
CMapWorld *CMapDoc::GetMapWorld(void)
{
	return(m_pWorld);
}


//-----------------------------------------------------------------------------
// Purpose: All map objects in a given document are assigned a unique ID.
//-----------------------------------------------------------------------------
int CMapDoc::GetNextMapObjectID(void)
{
	return(m_nNextMapObjectID++);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next unique ID for an AI node. Called when an AI node
//			is created so that each one can have a unique ID.
//
//			We can't use the unique object ID (above) for this because of
//			problems with in-engine editing of nodes and node connections.
//-----------------------------------------------------------------------------
int CMapDoc::GetNextNodeID(void)
{
	return(m_nNextNodeID++);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the next unique ID for a AI node creation. Called when an AI node
//			is created from the shell so that node IDs continue from there.
//-----------------------------------------------------------------------------
void CMapDoc::SetNextNodeID(int nID)
{
	m_nNextNodeID = nID;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not grid snap is enabled. Called by the tools and
//			views to determine snap behavior.
//-----------------------------------------------------------------------------
bool CMapDoc::IsSnapEnabled(void)
{
	return m_bSnapToGrid;
}


#endif // MAPDOC_H
