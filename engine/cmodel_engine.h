//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
// $NoKeywords: $
//=============================================================================//

#ifndef CMODEL_ENGINE_H
#define CMODEL_ENGINE_H

#ifdef _WIN32
#pragma once
#endif

#include "cmodel.h"
#include "cmodel_private.h"
#include "mathlib/vplane.h"
#include "bspfile.h"

class ICollideable;


cmodel_t	*CM_LoadMap( const char *name, bool allowReusePrevious, unsigned *checksum );
void		CM_FreeMap( void );
cmodel_t	*CM_InlineModel( const char *name );	// *1, *2, etc
cmodel_t	*CM_InlineModelNumber( int index );	// 1, 2, etc
int			CM_InlineModelContents( int index );	// 1, 2, etc

int			CM_NumClusters( void );
char		*CM_EntityString( void );
void		CM_DiscardEntityString( void );


// returns an ORed contents mask
int			CM_PointContents( const Vector &p, int headnode );
int			CM_TransformedPointContents( const Vector& p, int headnode, const Vector& origin, const QAngle& angles );

// sets the default values in a trace
void		CM_ClearTrace( trace_t *trace );

const byte	*CM_ClusterPVS( int cluster );
int			CM_ClusterPVSSize();

const byte	*CM_Vis( byte *dest, int destlen, int cluster, int visType );

int			CM_PointLeafnum( const Vector& p );
void		CM_SnapPointToReferenceLeaf(const Vector &referenceLeafPoint, float tolerance, Vector *pSnapPoint);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int			CM_BoxLeafnums( const Vector& mins, const Vector& maxs, int *list,
							int listsize, int *topnode );
//int			CM_TransformedBoxContents( const Vector& pos, const Vector& mins, const Vector& maxs, int headnode, const Vector& origin, const QAngle& angles );

// Versions that accept rays...
void		CM_TransformedBoxTrace (const Ray_t& ray, int headnode, int brushmask, const Vector& origin, QAngle const& angles, trace_t& tr );
void		CM_BoxTrace (const Ray_t& ray, int headnode, int brushmask, bool computeEndpt, trace_t& tr );
void		CM_BoxTraceAgainstLeafList( const Ray_t &ray, int *pLeafList, int nLeafCount, int nBrushMask, bool bComputeEndpoint, trace_t &trace );

void		CM_RayLeafnums( const Ray_t &ray, int *pLeafList, int nMaxLeafCount, int &nLeafCount );

int			CM_LeafContents( int leafnum );
int			CM_LeafCluster( int leafnum );
int			CM_LeafArea( int leafnum );
int			CM_LeafFlags( int leafnum );

void		CM_SetAreaPortalState( int portalnum, int isOpen );
void		CM_SetAreaPortalStates( const int *portalnums, const int *isOpen, int nPortals );
bool	CM_AreasConnected( int area1, int area2 );

int			CM_WriteAreaBits( byte *buffer, int buflen, int area );

// Given a view origin (which tells us the area to start looking in) and a portal key,
// fill in the plane that leads out of this area (it points into whatever area it leads to).
bool		CM_GetAreaPortalPlane( const Vector &vViewOrigin, int portalKey, VPlane *pPlane );

bool	CM_HeadnodeVisible( int headnode, const byte *visbits, int vissize );
// Test to see if the given box is in the given PVS/PAS
int			CM_BoxVisible( const Vector& mins, const Vector& maxs, const byte *visbits, int vissize );

typedef struct cmodel_collision_s cmodel_collision_t;
vcollide_t *CM_GetVCollide( int modelIndex );
vcollide_t* CM_VCollideForModel( int modelindex, const model_t* pModel );

// gets a virtual physcollide for a displacement
CPhysCollide *CM_PhysCollideForDisp( int index );
int			CM_SurfacepropsForDisp( int index );
void		CM_CreateDispPhysCollide( dphysdisp_t *pDispLump, int dispLumpSize );
void		CM_DestroyDispPhysCollide();

void		CM_WorldSpaceCenter( ICollideable *pCollideable, Vector *pCenter );
void		CM_WorldSpaceBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs );
void		CM_WorldAlignBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs );

void		CM_SetupAreaFloodNums( byte areaFloodNums[MAX_MAP_AREAS], int *pNumAreas );


//-----------------------------------------------------------------------------
// This can be used as a replacement for CM_PointLeafnum if the successive 
// origins will be close to each other.
//
// It caches the distance to the closest plane leading
// out of whatever leaf it was in last time you asked for the leaf index, and
// if it's within that distance the next time you ask for it, it'll 
//-----------------------------------------------------------------------------
class CFastPointLeafNum
{
public:
	CFastPointLeafNum();
	int GetLeaf( const Vector &vPos );

private:
	int m_iCachedLeaf;
	Vector m_vCachedPos;
	float m_flDistToExitLeafSqr;
};


#endif // CMODEL_ENGINE_H
