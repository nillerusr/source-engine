//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS_SAVERESTORE_H
#define VPHYSICS_SAVERESTORE_H

#if defined( _WIN32 )
#pragma once
#endif


#include "datamap.h"
#include "utlmap.h"
#include "isaverestore.h"
#include "utlvector.h"


//-------------------------------------

class ISave;
class IRestore;

class CPhysicsObject;
class CPhysicsFluidController;
class CPhysicsSpring;
class CPhysicsConstraint;
class CPhysicsConstraintGroup;
class CShadowController;
class CPlayerController;
class CPhysicsMotionController;
class CVehicleController;
struct physsaveparams_t;
struct physrestoreparams_t;
class ISaveRestoreOps;

//-----------------------------------------------------------------------------
// Purpose: Fixes up pointers beteween vphysics objects
//-----------------------------------------------------------------------------

class CVPhysPtrSaveRestoreOps : public CDefSaveRestoreOps
{
public:
	CVPhysPtrSaveRestoreOps();
	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave );
	void PreRestore();
	void PostRestore();
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore );
};

extern CVPhysPtrSaveRestoreOps g_VPhysPtrSaveRestoreOps;

#define DEFINE_VPHYSPTR(name) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, &g_VPhysPtrSaveRestoreOps, NULL }

#define DEFINE_VPHYSPTR_ARRAY(name,count) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, count, FTYPEDESC_SAVE, NULL, &g_VPhysPtrSaveRestoreOps, NULL }


//-----------------------------------------------------------------------------

class CVPhysPtrUtlVectorSaveRestoreOps : public CVPhysPtrSaveRestoreOps
{
public:
	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave );
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore );

private:
	typedef CUtlVector<int> VPhysPtrVector;
};

extern CVPhysPtrUtlVectorSaveRestoreOps g_VPhysPtrUtlVectorSaveRestoreOps;

#define DEFINE_VPHYSPTR_UTLVECTOR(name) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, &g_VPhysPtrUtlVectorSaveRestoreOps, NULL }


//-----------------------------------------------------------------------------

typedef bool (*PhysSaveFunc_t)( const physsaveparams_t &params, void *pCastedObject ); // second parameter for convenience
typedef bool (*PhysRestoreFunc_t)( const physrestoreparams_t &params, void **ppCastedObject );

bool SavePhysicsObject( const physsaveparams_t &params, CPhysicsObject *pObject );
bool RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject );

bool SavePhysicsFluidController( const physsaveparams_t &params, CPhysicsFluidController *pFluidObject );
bool RestorePhysicsFluidController( const physrestoreparams_t &params, CPhysicsFluidController **ppFluidObject );

bool SavePhysicsSpring( const physsaveparams_t &params, CPhysicsSpring *pSpring );
bool RestorePhysicsSpring( const physrestoreparams_t &params, CPhysicsSpring **ppSpring );

bool SavePhysicsConstraint( const physsaveparams_t &params, CPhysicsConstraint *pConstraint );
bool RestorePhysicsConstraint( const physrestoreparams_t &params, CPhysicsConstraint **ppConstraint );

bool SavePhysicsConstraintGroup( const physsaveparams_t &params, CPhysicsConstraintGroup *pConstraintGroup );
bool RestorePhysicsConstraintGroup( const physrestoreparams_t &params, CPhysicsConstraintGroup **ppConstraintGroup );
void PostRestorePhysicsConstraintGroup();

bool SavePhysicsShadowController( const physsaveparams_t &params, IPhysicsShadowController *pShadowController );
bool RestorePhysicsShadowController( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController );
bool RestorePhysicsShadowControllerInternal( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController, CPhysicsObject *pObject );

bool SavePhysicsPlayerController( const physsaveparams_t &params, CPlayerController *pPlayerController );
bool RestorePhysicsPlayerController( const physrestoreparams_t &params, CPlayerController **ppPlayerController );

bool SavePhysicsMotionController( const physsaveparams_t &params, IPhysicsMotionController *pMotionController );
bool RestorePhysicsMotionController( const physrestoreparams_t &params, IPhysicsMotionController **ppMotionController );

bool SavePhysicsVehicleController( const physsaveparams_t &params, CVehicleController *pVehicleController );
bool RestorePhysicsVehicleController( const physrestoreparams_t &params, CVehicleController **ppVehicleController );

//-----------------------------------------------------------------------------

ISaveRestoreOps* MaterialIndexDataOps();

#endif // VPHYSICS_SAVERESTORE_H
