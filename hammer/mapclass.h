//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a common class for all objects in the world object tree.
//
//			Pointers to other objects in the object tree may be stored, but
//			they should be set via UpdateDependency rather than directly. This
//			insures proper linkage to the other object so that if it moves, is
//			removed from the world, or changes in any other way, the dependent
//			object is properly notified.
//
//=============================================================================//

#ifndef MAPCLASS_H
#define MAPCLASS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"

#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "BoundBox.h"
#include "MapPoint.h"
#include "utlvector.h"
#include "visgroup.h"
#include "fgdlib/wckeyvalues.h"
#include "tier1/smartptr.h"


class Box3D;
class CBaseTool;
class CChunkFile;
class GDclass;
class CMapClass;
class CMapEntity;
class CMapSolid;
class CMapView2D;
class CMapViewLogical;
class CMapWorld;
class CPoint;
class CRender3D;
class CSaveInfo;
class CSSolid;
class CVisGroupList;
class CMapFaceList;

struct MapError;

enum ChunkFileResult_t;

struct MapObjectPair_t
{
	CMapClass *pObject1;
	CMapClass *pObject2;
};

//-----------------------------------------------------------------------------
// Structure used for returning hits when calling ObjectsAt.
//-----------------------------------------------------------------------------
typedef struct HitInfo_s
{
	CMapClass *pObject;		// Pointer to the CMapAtom that was clicked on.
	unsigned int uData;		// Additional data provided by the CMapAtom object.
	unsigned int nDepth;	// Depth value of the object that was clicked on.
	VMatrix m_LocalMatrix;
} HitInfo_t;


//
// Passed into PrepareSelection to control what gets selected.
//
enum SelectMode_t
{
	selectGroups = 0,	// select groups, ungrouped entities, and ungrouped solids
	selectObjects,		// select entities and solids not in entities
	selectSolids,		// select point entities, solids in entities, solids
};

enum VisGroupSelection
{
	AUTO = 0,
	USER
};

// helper macro for linked lists as pointers
#define FOR_EACH_OBJ( listName, iteratorName ) \
	for( int iteratorName=0; iteratorName<(listName).Count(); iteratorName++)



typedef const char * MAPCLASSTYPE;
typedef BOOL (*ENUMMAPCHILDRENPROC)(CMapClass *, unsigned int dwParam);
typedef CUtlVector<CMapClass*> CMapObjectList;


#define MAX_ENUM_CHILD_DEPTH	16


struct EnumChildrenStackEntry_t
{
	CMapClass *pParent;
	int pos;
};


struct EnumChildrenPos_t
{
	EnumChildrenStackEntry_t Stack[MAX_ENUM_CHILD_DEPTH];
	int nDepth;
};


typedef struct
{
	MAPCLASSTYPE Type;
	CMapClass * (*pfnNew)();
} MCMSTRUCT;


// This is a reference-counted class that holds a pointer to an object.
// When the object goes away, it can set the pointer in here to NULL
// and anyone else who holds a reference to this can know that the 
// object has gone away. It's similar to the EHANDLEs in the engine,
// except there's no finite list of objects that's managed anywhere.
template<class T>
class CSafeObject
{
public:
	static CSmartPtr< CSafeObject< T > > Create( T *pObject )
	{
		CSafeObject<T> *pRet = new CSafeObject<T>( pObject );
		return CSmartPtr< CSafeObject< T> >( pRet );
	}
	
	void AddRef()
	{
		++m_RefCount;
	}
	void Release()
	{
		--m_RefCount;
		if ( m_RefCount <= 0 )
			delete this;
	}
	int GetRefCount() const
	{
		return m_RefCount;
	}	

public:	
	T *m_pObject;

private:
	CSafeObject( T *pObject )
	{
		m_RefCount = 0;
		m_pObject = pObject;
	}
		
private:
	int m_RefCount;	// This object goes away when all smart pointers to it go away.
};


class CMapClass : public CMapPoint
{
public:
	//
	// Construction/destruction:
	//
	CMapClass(void);
	virtual ~CMapClass(void);
	
	const CSmartPtr< CSafeObject< CMapClass > >& GetSafeObjectSmartPtr();

	inline int GetID(void);
	inline void SetID(int nID);
	virtual size_t GetSize(void);

	//
	// Can belong to one or more visgroups:
	//
	virtual void AddVisGroup(CVisGroup *pVisGroup);
	int GetVisGroupCount(void);
	CVisGroup *GetVisGroup(int nIndex);
	void RemoveAllVisGroups(void);
	void RemoveVisGroup(CVisGroup *pVisGroup);
	int  IsInVisGroup(CVisGroup *pVisGroup);
	void SetColorVisGroup(CVisGroup *pVisGroup);
	virtual bool UpdateObjectColor();

	//
	// Can be tracked in the Undo/Redo system:
	//
	inline void SetTemporary(bool bTemporary) { m_bTemporary = bTemporary; }
	inline bool IsTemporary(void) const { return m_bTemporary; }
	union
	{
		struct
		{
			unsigned ID : 28;
			unsigned Types : 4;	// 0 - copy, 1 - relationship, 2 - delete
		} Kept;

		unsigned int dwKept;
	};

	//
	// Has children:
	//
	virtual void AddChild(CMapClass *pChild);
	virtual void CopyChildrenFrom(CMapClass *pobj, bool bUpdateDependencies);
	virtual void RemoveAllChildren(void);
	virtual void RemoveChild(CMapClass *pChild, bool bUpdateBounds = true);
	virtual void UpdateChild(CMapClass *pChild);

	inline int GetChildCount(void) { return( m_Children.Count()); }
	inline const CMapObjectList *GetChildren() { return &m_Children; }
		
	CMapClass *GetFirstDescendent(EnumChildrenPos_t &pos);
	CMapClass *GetNextDescendent(EnumChildrenPos_t &pos);

	virtual CMapClass *GetParent(void)
	{
		Assert( (m_pParent == NULL) || (dynamic_cast<CMapClass*>(m_pParent) != NULL) );
		return( (CMapClass*)m_pParent);
	}

	virtual void SetParent(CMapAtom *pParent)
	{
		Assert( (pParent == NULL) || (dynamic_cast<CMapClass*>(pParent) != NULL) );
		UpdateParent((CMapClass*)pParent);
	}

	const CMapObjectList *GetDependents() { return &m_Dependents; }

	virtual void FindTargetNames( CUtlVector< const char * > &Names ) { }
	virtual void ReplaceTargetname(const char *szOldName, const char *szNewName);

	//
	// Notifications.
	//
	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnPreClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnPrePaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
	virtual void OnParentKeyChanged(const char* key, const char* value) {}
	virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);
	virtual void OnUndoRedo(void) {}

	virtual bool OnApply( void ) { return true; }

	//
	// Bounds calculation and intersection functions.
	//
	virtual void CalcBounds(BOOL bFullUpdate = FALSE);
	
	void GetCullBox(Vector &mins, Vector &maxs);
	void SetCullBoxFromFaceList( CMapFaceList *pFaces );
	void GetBoundingBox( Vector &mins, Vector &maxs );
	void SetBoundingBoxFromFaceList( CMapFaceList *pFaces );
	
	void GetRender2DBox(Vector &mins, Vector &maxs);

	// NOTE: Logical position is in global space
	virtual void SetLogicalPosition( const Vector2D &vecPosition ) {}
	virtual const Vector2D& GetLogicalPosition( );

	// NOTE: Logical bounds is in global space
	virtual void GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs );

	// HACK: temp stuff to ease the transition to not inheriting from BoundBox
	void GetBoundsCenter(Vector &vecCenter) { m_Render2DBox.GetBoundsCenter(vecCenter); }
	void GetBoundsSize(Vector &vecSize) { m_Render2DBox.GetBoundsSize(vecSize); }
	inline bool IsInsideBox(Vector const &Mins, Vector const &Maxs) const { return(m_Render2DBox.IsInsideBox(Mins, Maxs)); }
	inline bool IsIntersectingBox(const Vector &vecMins, const Vector& vecMaxs) const { return(m_Render2DBox.IsIntersectingBox(vecMins, vecMaxs)); }

	virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);

	void PostUpdate(Notify_Dependent_t eNotifyType);
	static void UpdateAllDependencies(CMapClass *pObject);

	void SetOrigin(Vector& origin);

	// hierarchy
	virtual void UpdateAnimation( float animTime ) {}
	virtual bool GetTransformMatrix( VMatrix& matrix );
		
	virtual MAPCLASSTYPE GetType(void) = 0;
	virtual BOOL IsMapClass(MAPCLASSTYPE Type) = 0;
	virtual bool IsWorld() { return false; }

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);
	virtual bool HitTestLogical(CMapViewLogical *pView, const Vector2D &point, HitInfo_t &HitData);

	// Objects that can be clicked on and activated as tools implement this and return a CBaseTool-derived object.
	virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject) { return NULL; }

	//
	// Can be serialized:
	//
	virtual ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
	virtual ChunkFileResult_t SaveEditorData(CChunkFile *pFile);

	virtual bool ShouldSerialize(void) { return true; }
	virtual int SerializeRMF(std::fstream &File, BOOL bRMF);
	virtual int SerializeMAP(std::fstream &File, BOOL bRMF);
	virtual void PostloadWorld(CMapWorld *pWorld);		
	virtual void PresaveWorld(void) {}
	bool PostloadVisGroups( bool bIsLoading );

	virtual bool IsGroup(void) { return false; }
	virtual bool IsScaleable(void) { return false; }
	virtual bool IsClutter(void) { return false; }			// Whether this object should be hidden when the user hides helpers.
	virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs);	// Whether this object is hidden based on its own intersection with the cordon, independent of its parent's intersection.
	virtual bool IsEditable( void );
	virtual bool ShouldSnapToHalfGrid() { return false; }
	virtual bool IsSolid( ) { return false; }

	// searching
	virtual CMapEntity *FindChildByKeyValue( const char* key, const char* value, bool *bIsInInstance = NULL, VMatrix *InstanceMatrix = NULL );

	// HACK: get the world that this object is contained within.
	static CMapWorld *GetWorldObject(CMapAtom *pStart);
    
	virtual const char* GetDescription() { return ""; }

	BOOL EnumChildren(ENUMMAPCHILDRENPROC pfn, unsigned int dwParam = 0, MAPCLASSTYPE Type = NULL);
	BOOL EnumChildrenRecurseGroupsOnly(ENUMMAPCHILDRENPROC pfn, unsigned int dwParam, MAPCLASSTYPE Type = NULL);
	BOOL IsChildOf(CMapAtom *pObject);

	virtual bool ShouldAppearInLightingPreview(void)
	{
		return true; //false;
	}

	virtual bool ShouldAppearInRaytracedLightingPreview(void)
	{
		return false;
	}

	inline bool IsVisible(void) { return(m_bVisible); }
	void SetVisible(bool bVisible);

	inline bool IsVisGroupShown(void) { return m_bVisGroupShown && m_bVisGroupAutoShown; }
	void VisGroupShow(bool bShow, VisGroupSelection eVisGroup = USER);
	bool CheckVisibility(bool bIsLoading = false);

	//
	// Visible2D functions are used only for hiding solids being morphed. Remove?
	//
	bool IsVisible2D(void) { return m_bVisible2D; }
	void SetVisible2D(bool bVisible2D) { m_bVisible2D = bVisible2D; }

	// Is this class potentially visible in 2D visio view?
	virtual bool IsLogical(void) { return false; }

	// Is this class actually visible in 2D visio view?
	virtual bool IsVisibleLogical(void) { return false; }

	//
	// Overridden to set the render color of each of our children.
	//
	virtual void SetRenderColor(unsigned char red, unsigned char green, unsigned char blue);
	virtual void SetRenderColor(color32 rgbColor);

	//
	// Can be rendered:
	//
	virtual void Render2D(CRender2D *pRender);
	virtual void Render3D(CRender3D *pRender);
	virtual void RenderLogical( CRender2D *pRender ) {}
	virtual bool RenderPreload(CRender3D *pRender, bool bNewContext);
	inline int GetRenderFrame(void) { return(m_nRenderFrame); }
	inline void SetRenderFrame(int nRenderFrame) { m_nRenderFrame = nRenderFrame; }

	SelectionState_t SetSelectionState(SelectionState_t eSelectionState);

	//
	// Has a set of editor-specific properties that are loaded from the VMF file.
	// The keys are freed after being handled by the map post-load code.
	//
	int GetEditorKeyCount(void);
	const char *GetEditorKey(int nIndex);
	const char *GetEditorKeyValue(int nIndex);
	const char *GetEditorKeyValue(const char *szKey);
	void RemoveEditorKeys(void);
	void SetEditorKeyValue(const char *szKey, const char *szValue);

	virtual void InstanceMoved( void );

public:

	// Set to true while loading a VMF file so it can delay certain calls like UpdateBounds.
	// Drastically speeds up load times.
	static bool s_bLoadingVMF;

protected:

	//
	// Implements CMapAtom transformation interface:
	//
	virtual void DoTransform(const VMatrix &matrix);
		
	//
	// Serialization callbacks.
	//
	static ChunkFileResult_t LoadEditorCallback(CChunkFile *pFile, CMapClass *pObject);
	static ChunkFileResult_t LoadEditorKeyCallback(const char *szKey, const char *szValue, CMapClass *pObject);

	//
	// Has a list of objects that must be notified if it changes size or position.
	//
	void AddDependent(CMapClass *pDependent);
	void NotifyDependents(Notify_Dependent_t eNotifyType);
	void RemoveDependent(CMapClass *pDependent);
	virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject) {};
	CMapClass *UpdateDependency(CMapClass *pOldAttached, CMapClass *pNewAttached);

	void UpdateParent(CMapClass *pNewParent);

	void SetBoxFromFaceList( CMapFaceList *pFaces, BoundBox &Box );

	CSmartPtr< CSafeObject< CMapClass > > m_pSafeObject;

	BoundBox m_CullBox;				// Our bounds for culling in the 3D views and intersecting with the cordon.
	BoundBox m_BoundingBox;			// Our bounds for brushes / entities themselves.  This size may be smaller than m_CullBox ( i.e. spheres are not included )
	BoundBox m_Render2DBox;			// Our bounds for rendering in the 2D views.

	CMapObjectList m_Children;		// Each object can have many children. Children usually transform with their parents, etc.
	CMapObjectList m_Dependents;	// Objects that this object should notify if it changes.

	int m_nID;						// This object's unique ID.
	bool m_bTemporary;				// Whether to track this object for Undo/Redo.
	int m_nRenderFrame;				// Frame counter used to avoid rendering the same object twice in a 3D frame.

	bool m_bVisible2D;				// Whether this object is visible in the 2D view. Currently only used for morphing.
	bool m_bVisible;				// Whether this object is currently visible in the 2D and 3D views based on ALL factors: visgroups, cordon, etc.

	bool m_bVisGroupShown;			// Whether this object is shown or hidden by user visgroups. Kept separate from m_bVisible so we can
	// reflect this state in the visgroups list independent of the cordon, hide entities state, etc.

	bool m_bVisGroupAutoShown;		// Whether this object is shown or hidden by auto visgroups.

	CVisGroupList	m_VisGroups;	// Visgroups to which this object belongs, EMPTY if none.
	CVisGroup *m_pColorVisGroup;	// The visgroup from which we get our color, NULL if none.

	WCKeyValuesT<WCKVBase_Vector> *m_pEditorKeys;		// Temporary storage for keys loaded from the "editor" chunk of the VMF file, freed after loading.
	
	friend class CTrackEntry;						// Friends with Undo/Redo system so that parentage can be changed.
	friend void FixHiddenObject(MapError *pError);	// So that the Check for Problems dialog can fix visgroups problems.
};


//-----------------------------------------------------------------------------
// Purpose: Returns this object's unique ID.
//-----------------------------------------------------------------------------
int CMapClass::GetID(void)
{
	return(m_nID);
}


//-----------------------------------------------------------------------------
// Purpose: Sets this object's unique ID.
//-----------------------------------------------------------------------------
void CMapClass::SetID(int nID)
{
	m_nID = nID;
}

class CMapClassManager
{
public:

	virtual ~CMapClassManager();
	CMapClassManager(MAPCLASSTYPE Type, CMapClass * (*pfnNew)());

	static CMapClass * CreateObject(MAPCLASSTYPE Type);
};


#define MAPCLASS_TYPE(class_name) \
	(class_name::__Type)


#define IMPLEMENT_MAPCLASS(class_name) \
	char * class_name::__Type = #class_name; \
	MAPCLASSTYPE class_name::GetType() { return __Type; }	\
	BOOL class_name::IsMapClass(MAPCLASSTYPE Type) \
		{ return (Type == __Type) ? TRUE : FALSE; } \
	CMapClass * class_name##_CreateObject() \
		{ return new class_name; } \
	CMapClassManager mcm_##class_name(class_name::__Type, \
		class_name##_CreateObject);


#define DECLARE_MAPCLASS(class_name,class_base) \
	typedef class_base BaseClass; \
	static char * __Type; \
	virtual MAPCLASSTYPE GetType(); \
	virtual BOOL IsMapClass(MAPCLASSTYPE Type);


class CCheckFaceInfo
{
public:

	CCheckFaceInfo() { iPoint = -1; }
	char szDescription[128];
	int iPoint;
};


#endif // MAPCLASS_H
