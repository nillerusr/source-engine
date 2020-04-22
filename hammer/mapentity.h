//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef MAPENTITY_H
#define MAPENTITY_H
#pragma once

#include "MapClass.h"
#include "MapFace.h"			// FIXME: For PLANE definition.
#include "EditGameClass.h"


class CMapAnimator;
class CRender2D;
class CManifest;

enum LogicalConnection_t
{
	LOGICAL_CONNECTION_INPUT = 0,
	LOGICAL_CONNECTION_OUTPUT,
};

int CompareEntityNames(const char *szName1, const char *szName2);

#define ENTITY_FLAG_IS_LIGHT			1
#define ENTITY_FLAG_SHOW_IN_LPREVIEW2	2
#define ENTITY_FLAG_IS_INSTANCE			4


class CMapEntity : public CMapClass, public CEditGameClass
{
	friend CManifest;

public:

	DECLARE_MAPCLASS(CMapEntity,CMapClass);

	CMapEntity(); 
	~CMapEntity();

	void Debug(void);

	size_t GetSize();

	int m_EntityTypeFlags;									// for fast checks w/o using class name
	
	//
	// For flags field.
	//
	enum
	{
		flagPlaceholder = 0x01,	// No solids - just a point entity
	};

	enum alignType_e
	{
		ALIGN_TOP,
		ALIGN_BOTTOM,
	};

	bool NameMatches(const char *szName);
	bool ClassNameMatches(const char *szName);

	virtual bool ShouldAppearInRaytracedLightingPreview(void)
	{
		return ( m_EntityTypeFlags & ENTITY_FLAG_SHOW_IN_LPREVIEW2 ) != 0;
	}

	static inline void ShowEntityNames(bool bShow) { s_bShowEntityNames = bShow; }
	static inline bool GetShowEntityNames(void) { return s_bShowEntityNames; }

	static inline void ShowEntityConnections(bool bShow) { s_bShowEntityConnections = bShow; }
	static inline bool GetShowEntityConnections(void) { return s_bShowEntityConnections; }

	static inline void ShowUnconnectedEntities(bool bShow) { s_bShowUnconnectedEntities = bShow; }
	static inline bool GetShowUnconnectedEntities(void) { return s_bShowUnconnectedEntities; }

	void ReplaceTargetname(const char *szOldName, const char *szNewName);

	void CalculateTypeFlags( void );

	virtual void SignalChanged(void );								// object has changed

	inline void SetPlaceholder(BOOL bSet)
	{
		if (bSet)
		{
			flags |= flagPlaceholder;
		}
		else
		{
			flags &= ~flagPlaceholder;
		}
	}

	inline BOOL IsPlaceholder(void)
	{
		return((flags & flagPlaceholder) ? TRUE : FALSE);
	}

	bool UpdateObjectColor();

	//
	// CMapClass overrides.
	//
	bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs);

	//
	// Serialization.
	//
	ChunkFileResult_t LoadVMF(CChunkFile *pFile);
	ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
	int SerializeRMF(std::fstream&, BOOL);
	int SerializeMAP(std::fstream&, BOOL);
	virtual void PostloadWorld(CMapWorld *pWorld);
	virtual ChunkFileResult_t SaveEditorData(CChunkFile *pFile);

	//
	// Rendering.
	//
	virtual void Render2D(CRender2D *pRender);
	virtual void RenderLogical( CRender2D *pRender );
	virtual bool IsLogical();
	virtual bool IsVisibleLogical(void);
	virtual void SetLogicalPosition( const Vector2D &vecPosition );
	virtual const Vector2D& GetLogicalPosition( );
	virtual void GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs );
	void GetLogicalConnectionPosition( LogicalConnection_t i, Vector2D &vecPosition );

	virtual bool ShouldSnapToHalfGrid();

	virtual void SetOrigin(Vector& o);
	virtual void CalcBounds(BOOL bFullUpdate = FALSE);
	inline void SetClass(GDclass *pClass) { CEditGameClass::SetClass(pClass); } // Works around a namespace issue.
	virtual void SetClass(LPCTSTR pszClassname, bool bLoading = false);
	virtual void AlignOnPlane( Vector& pos, PLANE *plane, alignType_e align );

	//
	// Hit testing/selection.
	//
	virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);
	virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);
	virtual bool HitTestLogical(CMapViewLogical *pView, const Vector2D &point, HitInfo_t &nHitData);

	//
	// Notifications.
	//
	virtual void OnClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnPreClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren );
	virtual void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
	virtual void OnPrePaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList );
	virtual void OnPaste( CMapClass *pCopyObject, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList );

	virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject);

	virtual bool OnApply( void );

	//
	// Keyvalue access. We need to know any time one of our keyvalues changes.
	//
	virtual void SetKeyValue(LPCSTR pszKey, LPCSTR pszValue);
	virtual void DeleteKeyValue(LPCTSTR pszKey);

	int GetNodeID(void);
	int SetNodeID(int nNodeID);

	void NotifyChildKeyChanged(CMapClass *pChild, const char *szKey, const char *szValue);

	virtual CMapEntity *FindChildByKeyValue( LPCSTR key, LPCSTR value, bool *bIsInInstance = NULL, VMatrix *InstanceMatrix = NULL );

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual void AddChild(CMapClass *pChild);

	bool HasSolidChildren(void);

	void AssignNodeID(void);

	const char* GetDescription();
	bool IsScaleable() { return !IsPlaceholder(); }

	// animation
	bool GetTransformMatrix( VMatrix& matrix );
	BOOL IsAnimationController( void ) { return IsMoveClass(); }

	//-----------------------------------------------------------------------------
	// Purpose: If the first child of this entity is of type MapClass, this function
	//			returns a pointer to that child.
	// Output : Returns a pointer to the MapClass that is a child of this
	//			entity, NULL if the first child of this entity is not MapClass.
	//-----------------------------------------------------------------------------
	template< class MapClass >
	MapClass *GetChildOfType( MapClass *null )
	{
		FOR_EACH_OBJ( m_Children, pos )
		{
			MapClass *pChild = dynamic_cast<MapClass*>( m_Children.Element(pos) );
			if ( pChild != NULL )
			{
				return pChild;
			}
		}

		return NULL;
	}

	//
	// CMapAtom implementation.
	//
	virtual void GetRenderColor( CRender2D *pRender, unsigned char &red, unsigned char &green, unsigned char &blue);
	virtual color32 GetRenderColor( CRender2D *pRender );

// 	char const* GetKeyValue( char *symbol )
// 	{
// 		return m_KeyValues.GetValue(symbol );
// 	}

private:

	void EnsureUniqueNodeID(CMapWorld *pWorld);
	void OnKeyValueChanged(const char *pszKey, const char *pszOldValue, const char *pszValue);

	//
	// Each CMapEntity may have one or more helpers as its children, depending
	// on its class definition in the FGD file.
	//
	void AddBoundBoxForClass(GDclass *pClass, bool bLoading);
	void AddHelper(CMapClass *pHelper, bool bLoading);
	void AddHelpersForClass(GDclass *pClass, bool bLoading);
	void RemoveHelpers(bool bRemoveSolids);
	void UpdateHelpers(bool bLoading);

	// Safely sets the move parent. Will assert and not set it if pEnt is equal to this ent,
	// or if this ent is already a parent of pEnt.
	void SetMoveParent( CMapEntity *pEnt );

	//
	// Chunk and key value handlers for loading.
	//
	static ChunkFileResult_t LoadSolidCallback(CChunkFile *pFile, CMapEntity *pEntity);
	static ChunkFileResult_t LoadEditorCallback(CChunkFile *pFile, CMapEntity *pEntity);
	static ChunkFileResult_t LoadHiddenCallback(CChunkFile *pFile, CMapEntity *pEntity);
	static ChunkFileResult_t LoadKeyCallback(const char *szKey, const char *szValue, CMapEntity *pEntity);
	static ChunkFileResult_t LoadEditorKeyCallback(const char *szKey, const char *szValue, CMapEntity *pEntity);

	static bool s_bShowEntityNames;			// Whether to render entity names in the 2D views.
	static bool s_bShowEntityConnections;	// Whether to render lines indicating entity connections in the 2D views.
	static bool s_bShowUnconnectedEntities;	// Whether to render unconnected entities in logical views

	WORD flags;							// flagPlaceholder
	CMapEntity *m_pMoveParent;			// for entity movement hierarchy
	CMapAnimator *m_pAnimatorChild;
	Vector2D m_vecLogicalPosition;	// Position in logical space
};


typedef CUtlVector<CMapEntity*> CMapEntityList;

bool MapEntityList_HasInput(const CMapEntityList *pList, const char *szInput, InputOutputType_t eType = iotInvalid);

#endif // MAPENTITY_H
