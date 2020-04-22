//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef MAPGROUP_H
#define MAPGROUP_H
#ifdef _WIN32
#pragma once
#endif

#include "MapDefs.h"
#include "MapClass.h"


class CMapGroup : public CMapClass
{
	public:
		DECLARE_MAPCLASS(CMapGroup,CMapClass)
		
		CMapGroup() : m_vecLogicalPosition(COORD_NOTINIT, COORD_NOTINIT) {}

		const char* GetDescription(void);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		virtual bool IsGroup(void) { return true; }

		// Groups have to be treated as logical because they potentially have logical children
		virtual bool IsLogical(void) { return true; }
		virtual bool IsVisibleLogical(void) { return IsVisible(); }

		void AddChild(CMapClass *pChild);
		void AddVisGroup(CVisGroup *pVisGroup);

		// NOTE: Logical position is in global space
		virtual void SetLogicalPosition( const Vector2D &vecPosition );
		virtual const Vector2D& GetLogicalPosition( );
		virtual void GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs );

		//
		// Serialization.
		//
		ChunkFileResult_t LoadVMF(CChunkFile *pFile);
		ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);

 		Vector2D m_vecLogicalPosition;	// Position in logical space
};


typedef CUtlVector<CMapGroup *> CMapGroupList;


#endif // MAPGROUP_H
