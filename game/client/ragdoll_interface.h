//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:  the interface to the old and new physics ragdoll glue code between Entity and Physics systems
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef RAGDOLL_INTERFACE_H
#define RAGDOLL_INTERFACE_H


class C_BaseEntity;
class CStudioHdr;
struct mstudiobone_t;
class Vector;
class IPhysicsObject;
class CBoneAccessor;

abstract_class IRagdoll
{
public:
	virtual ~IRagdoll() {}

	virtual void RagdollBone( C_BaseEntity *ent, mstudiobone_t *pbones, int boneCount, bool *boneSimulated, CBoneAccessor &pBoneToWorld ) = 0;
	virtual const Vector& GetRagdollOrigin( ) = 0;
	virtual void GetRagdollBounds( Vector &mins, Vector &maxs ) = 0;
	virtual int RagdollBoneCount() const = 0;
	virtual IPhysicsObject *GetElement( int elementNum ) = 0;
	virtual void DrawWireframe( void ) = 0;
	virtual void VPhysicsUpdate( IPhysicsObject *pObject ) = 0;
	virtual bool AllowStretch() const = 0;
	virtual void BuildRagdollBounds( C_BaseEntity *ent ) = 0;

	virtual bool SolvePenetrations(IPhysicsObject *pObj0, IPhysicsObject *pObj1) = 0;
	virtual datamap_t *GetDataMap() = 0;
	virtual void ResetRagdollSleepAfterTime(void) = 0;
	virtual float GetLastVPhysicsUpdateTime() const = 0;

	ragdoll_t *GetRagdoll( void ) {return NULL;} // the old ragdoll needs this for now

	virtual void Init( 
		C_BaseEntity *ent, 
		CStudioHdr *pstudiohdr, 
		const Vector &forceVector, 
		int forceBone, 
		const matrix3x4_t *pDeltaBones0, 
		const matrix3x4_t *pDeltaBones1, 
		const matrix3x4_t *pCurrentBonePosition, 
		float boneDt ) = 0;
	virtual bool IsValid()const = 0;
};

extern IRagdoll *CreateRagdoll();
extern IRagdoll *CreateRagdoll2();

#endif