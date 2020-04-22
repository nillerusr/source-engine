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

#ifndef DISPMANAGER_H
#define DISPMANAGER_H
#pragma once

class CMapDisp;

//=============================================================================
//
// Global Displacement Manager
//
typedef unsigned short EditDispHandle_t;
enum
{
	EDITDISPHANDLE_INVALID = ( EditDispHandle_t )~0
};

class IEditDispMgr
{
public:

	// creation/destruction
	virtual EditDispHandle_t Create( void ) = 0;
	virtual void Destroy( EditDispHandle_t handle ) = 0;

	virtual CMapDisp *GetDisp( EditDispHandle_t handle ) = 0;
};

IEditDispMgr *EditDispMgr( void );


//=============================================================================
//
// World Displacement Manager(s)
//
class IWorldEditDispMgr
{
public:

	virtual ~IWorldEditDispMgr() {}

	//
	// World List
	//
	virtual int WorldCount( void ) = 0;
	virtual CMapDisp *GetFromWorld( int iWorldList ) = 0;
	virtual CMapDisp *GetFromWorld( EditDispHandle_t handle ) = 0;

	virtual void AddToWorld( EditDispHandle_t handle ) = 0;
	virtual void RemoveFromWorld( EditDispHandle_t handle ) = 0;
	
	virtual void FindWorldNeighbors( EditDispHandle_t handle ) = 0;
	
	//
	// Selection List
	//
	virtual int SelectCount( void ) = 0;
	virtual void SelectClear( void ) = 0;
	virtual CMapDisp *GetFromSelect( int iSelectList ) = 0;

	virtual void AddToSelect( EditDispHandle_t handle ) = 0;
	virtual void RemoveFromSelect( EditDispHandle_t handle ) = 0;
	virtual bool IsInSelect( EditDispHandle_t handle ) = 0;

	virtual void CatmullClarkSubdivide( void ) = 0;

	//
	// Undo
	//
	virtual void PreUndo( const char *pszMarkName ) = 0;
	virtual void Undo( EditDispHandle_t handle, bool bAddNeighbors ) = 0;
	virtual void PostUndo( void ) = 0;

	//
	// Helpers.
	//
	
	// Return the # of points that these disps share (1=corner neighbors, 2=edge neighbors).
	virtual int NumSharedPoints( CMapDisp *pDisp, CMapDisp *pNeighborDisp, int *edge1, int *edge2 ) = 0;
};

//
// Global functions.
//
IWorldEditDispMgr *CreateWorldEditDispMgr( void );
void DestroyWorldEditDispMgr( IWorldEditDispMgr **pDispMgr );


#endif // DISPMANAGER_H
