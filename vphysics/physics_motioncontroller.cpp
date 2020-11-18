//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifdef _WIN32
#pragma warning (disable:4127)
#pragma warning (disable:4244)
#endif

#include "cbase.h"
#include "ivp_controller.hxx"

#include "physics_motioncontroller.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct vphysics_save_motioncontroller_t
{
	CUtlVector<IPhysicsObject *> m_objectList;
	int	m_nPriority;

	DECLARE_SIMPLE_DATADESC();
};


BEGIN_SIMPLE_DATADESC( vphysics_save_motioncontroller_t )
	DEFINE_VPHYSPTR_UTLVECTOR( m_objectList ),
	DEFINE_FIELD( m_nPriority,	FIELD_INTEGER ),
END_DATADESC()



class CPhysicsMotionController : public IVP_Controller_Independent, public IPhysicsMotionController
{
public:
	CPhysicsMotionController( IMotionEvent *pHandler, CPhysicsEnvironment *pVEnv );
	virtual ~CPhysicsMotionController( void );
    virtual void do_simulation_controller(IVP_Event_Sim *event,IVP_U_Vector<IVP_Core> *core_list);
    virtual IVP_CONTROLLER_PRIORITY get_controller_priority();
    virtual void core_is_going_to_be_deleted_event(IVP_Core *core) 
	{
		m_coreList.FindAndRemove( core );
	}
	virtual const char *get_controller_name() { return "vphysics:motion"; }

	virtual void SetEventHandler( IMotionEvent *handler );
	virtual void AttachObject( IPhysicsObject *pObject, bool checkIfAlreadyAttached );
	virtual void DetachObject( IPhysicsObject *pObject );

	void RemoveCore( IVP_Core *pCore );

	// Save/load
	void WriteToTemplate( vphysics_save_motioncontroller_t &controllerTemplate );
	void InitFromTemplate( const vphysics_save_motioncontroller_t &controllerTemplate );

	// returns the number of objects currently attached to the controller
	virtual int CountObjects( void )
	{
		return m_coreList.Count();
	}
	// NOTE: pObjectList is an array with at least CountObjects() allocated
	virtual void GetObjects( IPhysicsObject **pObjectList )
	{
		for ( int i = 0; i < m_coreList.Count(); i++ )
		{
			IVP_Core *pCore = m_coreList[i];

			IVP_Real_Object *pivp = pCore->objects.element_at(0);
			IPhysicsObject *pPhys = static_cast<IPhysicsObject *>(pivp->client_data);
			// copy out
			pObjectList[i] = pPhys;
		}
	}

	// detaches all attached objects
	virtual void ClearObjects( void )
	{
		while ( m_coreList.Count() )
		{
			int x = m_coreList.Count()-1;
			IVP_Core *pCore = m_coreList[x];
			RemoveCore( pCore );
		}
	}

	// wakes up all attached objects
	virtual void WakeObjects( void )
	{
		for ( int i = 0; i < m_coreList.Count(); i++ )
		{
			IVP_Core *pCore = m_coreList[i];
			pCore->ensure_core_to_be_in_simulation();
		}
	}
	virtual void SetPriority( priority_t priority );

private:
	IMotionEvent			*m_handler;
	CUtlVector<IVP_Core *>	m_coreList;
	CPhysicsEnvironment		*m_pVEnv;
	int						m_priority;
};


CPhysicsMotionController::CPhysicsMotionController( IMotionEvent *pHandler, CPhysicsEnvironment *pVEnv )
{
	m_handler = pHandler;
	m_pVEnv = pVEnv;
	SetPriority( MEDIUM_PRIORITY );
}

CPhysicsMotionController::~CPhysicsMotionController( void )
{
	Assert( !m_pVEnv->IsInSimulation() );
	for ( int i = 0; i < m_coreList.Count(); i++ )
	{
		m_coreList[i]->rem_core_controller( (IVP_Controller *)this );
	}
}

void CPhysicsMotionController::do_simulation_controller(IVP_Event_Sim *event,IVP_U_Vector<IVP_Core> *core_list)
{
	if ( m_handler )
	{
		for ( int i = 0; i < core_list->len(); i++ )
		{
			IVP_U_Float_Point ivpSpeed, ivpRot;
			IVP_Core *pCore = core_list->element_at(i);

			IVP_Real_Object *pivp = pCore->objects.element_at(0);
			IPhysicsObject *pPhys = static_cast<IPhysicsObject *>(pivp->client_data);
			if ( !pPhys->IsMoveable() )
				continue;

			Vector speed;
			AngularImpulse rot;
			speed.Init();
			rot.Init();

			IMotionEvent::simresult_e ret = m_handler->Simulate( this, pPhys, event->delta_time, speed, rot );

			switch( ret )
			{
			case IMotionEvent::SIM_NOTHING:
				break;
			case IMotionEvent::SIM_LOCAL_ACCELERATION:
				{
					ConvertForceImpulseToIVP( speed, ivpSpeed );
					ConvertAngularImpulseToIVP( rot, ivpRot );
					const IVP_U_Matrix *m_world_f_core = pCore->get_m_world_f_core_PSI();
					// transform to world space
					m_world_f_core->inline_vmult3( &ivpSpeed, &ivpSpeed );
					// UNDONE: Put these values into speed change / rot_speed_change instead?
					pCore->speed.add_multiple( &ivpSpeed, event->delta_time );
					pCore->rot_speed.add_multiple( &ivpRot, event->delta_time );
				}
				break;
			case IMotionEvent::SIM_LOCAL_FORCE:
				{
					ConvertForceImpulseToIVP( speed, ivpSpeed );
					ConvertAngularImpulseToIVP( rot, ivpRot );
					const IVP_U_Matrix *m_world_f_core = pCore->get_m_world_f_core_PSI();
					// transform to world space
					m_world_f_core->inline_vmult3( &ivpSpeed, &ivpSpeed );
					pCore->center_push_core_multiple_ws( &ivpSpeed, event->delta_time );
					pCore->rot_push_core_multiple_cs( &ivpRot, event->delta_time );
				}
				break;
			case IMotionEvent::SIM_GLOBAL_ACCELERATION:
				{
					ConvertAngularImpulseToIVP( rot, ivpRot );
					ConvertForceImpulseToIVP( speed, ivpSpeed );
					pCore->speed.add_multiple( &ivpSpeed, event->delta_time );
					pCore->rot_speed.add_multiple( &ivpRot, event->delta_time );
				}
				break;
			case IMotionEvent::SIM_GLOBAL_FORCE:
				{
					ConvertAngularImpulseToIVP( rot, ivpRot );
					ConvertForceImpulseToIVP( speed, ivpSpeed );
					pCore->center_push_core_multiple_ws( &ivpSpeed, event->delta_time );
					pCore->rot_push_core_multiple_cs( &ivpRot, event->delta_time );
				}
				break;
			}
			// TODO(mastercoms): apply sv_maxvelocity?
			//pCore->apply_velocity_limit();
		}
	}
}


IVP_CONTROLLER_PRIORITY CPhysicsMotionController::get_controller_priority()
{
	return (IVP_CONTROLLER_PRIORITY) m_priority;
}

void CPhysicsMotionController::SetPriority( priority_t priority )
{
	switch ( priority )
	{
	case LOW_PRIORITY:
		m_priority = IVP_CP_CONSTRAINTS_MIN;
		break;
	default:
	case MEDIUM_PRIORITY:
		m_priority = IVP_CP_MOTION;
		break;
	case HIGH_PRIORITY:
		m_priority = IVP_CP_FORCEFIELDS+1;
		break;
	}
}

void CPhysicsMotionController::SetEventHandler( IMotionEvent *handler )
{
	m_handler = handler;
}

void CPhysicsMotionController::AttachObject( IPhysicsObject *pObject, bool checkIfAlreadyAttached )
{
	// BUGBUG: Sometimes restore comes back with a NULL, REVISIT
	if ( !pObject || pObject->IsStatic() )
		return;

	CPhysicsObject *pPhys = static_cast<CPhysicsObject *>(pObject);
	IVP_Real_Object *pIVP = pPhys->GetObject();
	IVP_Core *pCore = pIVP->get_core();

	// UNDONE: On save/load, trigger-based motion controllers re-attach their objects.
	// UNDONE: Do something cheaper about this?
	// OPTIMIZE: Linear search here?
	if ( checkIfAlreadyAttached )
	{
		int index = m_coreList.Find(pCore);
		if ( m_coreList.IsValidIndex(index) )
		{
			DevMsg(1,"Attached core twice!!!\n");
			return;
		}
	}

	m_coreList.AddToTail( pCore );

	MEM_ALLOC_CREDIT();
	pCore->add_core_controller( (IVP_Controller *)this );
}


void CPhysicsMotionController::RemoveCore( IVP_Core *pCore )
{
	int index = m_coreList.Find(pCore);
	if ( !m_coreList.IsValidIndex(index) )
	{
#if DEBUG
		Msg("removed invalid core !!!\n");
#endif
		return;
	}
	//Assert( !m_pVEnv->IsInSimulation() );
	m_coreList.Remove( index );
	pCore->rem_core_controller( static_cast<IVP_Controller_Independent *>(this) );
}


void CPhysicsMotionController::DetachObject( IPhysicsObject *pObject )
{
	CPhysicsObject *pPhys = static_cast<CPhysicsObject *>(pObject);
	IVP_Real_Object *pIVP = pPhys->GetObject();
	IVP_Core *core = pIVP->get_core();

	RemoveCore(core);
}

// Save/load
void CPhysicsMotionController::WriteToTemplate( vphysics_save_motioncontroller_t &controllerTemplate )
{
	controllerTemplate.m_nPriority = m_priority;

	int nObjectCount = CountObjects();
	controllerTemplate.m_objectList.AddMultipleToTail( nObjectCount );
	GetObjects( controllerTemplate.m_objectList.Base() );
}

void CPhysicsMotionController::InitFromTemplate(  const vphysics_save_motioncontroller_t &controllerTemplate )
{
	m_priority = controllerTemplate.m_nPriority;

	int nObjectCount = controllerTemplate.m_objectList.Count();
	for ( int i = 0; i < nObjectCount; ++i )
	{
		AttachObject( controllerTemplate.m_objectList[i], true );
	}
}


IPhysicsMotionController *CreateMotionController( CPhysicsEnvironment *pPhysEnv, IMotionEvent *pHandler )
{
	if ( !pHandler )
		return NULL;

	return new CPhysicsMotionController( pHandler, pPhysEnv );
}

bool SavePhysicsMotionController( const physsaveparams_t &params, IPhysicsMotionController *pMotionController )
{
	vphysics_save_motioncontroller_t controllerTemplate;
	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );
  
	CPhysicsMotionController *pControllerImp = static_cast<CPhysicsMotionController*>(pMotionController);
	pControllerImp->WriteToTemplate( controllerTemplate );
	params.pSave->WriteAll( &controllerTemplate );

	return true;
}

bool RestorePhysicsMotionController( const physrestoreparams_t &params, IPhysicsMotionController **ppMotionController )
{		
	CPhysicsMotionController *pControllerImp = new CPhysicsMotionController( NULL, static_cast<CPhysicsEnvironment *>(params.pEnvironment) );
	
	vphysics_save_motioncontroller_t controllerTemplate;
	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );
	params.pRestore->ReadAll( &controllerTemplate );
  
	pControllerImp->InitFromTemplate( controllerTemplate );
	*ppMotionController = pControllerImp; 
  
	return true;
}
