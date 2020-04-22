//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "physics_fluid.h"


#include "ivp_compact_surface.hxx"
#include "ivp_surman_polygon.hxx"
#include "ivp_phantom.hxx"
#include "ivp_controller_buoyancy.hxx"
#include "ivp_liquid_surface_descript.hxx"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// NOTE: This is auto-deleted by the phantom controller
class CBuoyancyAttacher : public IVP_Attacher_To_Cores_Buoyancy
{
public:
    virtual IVP_Template_Buoyancy *get_parameters_per_core( IVP_Core *pCore );
    CBuoyancyAttacher(IVP_Template_Buoyancy &templ, IVP_U_Set_Active<IVP_Core> *set_of_cores_, IVP_Liquid_Surface_Descriptor *liquid_surface_descriptor_);

	float m_density;
};

CPhysicsFluidController::CPhysicsFluidController( CBuoyancyAttacher *pBuoy, IVP_Liquid_Surface_Descriptor *pLiquid, CPhysicsObject *pObject, int nContents )
{
	m_pBuoyancy = pBuoy;
	m_pLiquidSurface = pLiquid;
	m_pObject = pObject;
	m_nContents = nContents;
}

CPhysicsFluidController::~CPhysicsFluidController( void )
{
	delete m_pLiquidSurface;
}

void CPhysicsFluidController::SetGameData( void *pGameData ) 
{ 
	m_pGameData = pGameData; 
}

void *CPhysicsFluidController::GetGameData( void ) const 
{ 
	return m_pGameData; 
}

void CPhysicsFluidController::GetSurfacePlane( Vector *pNormal, float *pDist ) const 
{
	IVP_U_Float_Hesse surface;
	IVP_U_Float_Point abs_speed_of_current;

	m_pLiquidSurface->calc_liquid_surface( GetIVPObject()->get_core()->environment, 
		GetIVPObject()->get_core(), &surface, &abs_speed_of_current );  
	ConvertPlaneToHL( surface, pNormal, pDist );
	if ( pNormal )
	{
		*pNormal *= -1;
	}
	if ( pDist )
	{
		*pDist *= -1;
	}
}

IVP_Real_Object *CPhysicsFluidController::GetIVPObject() 
{ 
	return m_pObject->GetObject();
}

const IVP_Real_Object *CPhysicsFluidController::GetIVPObject() const
{ 
	return m_pObject->GetObject();
}

float CPhysicsFluidController::GetDensity() const
{
	return m_pBuoyancy->m_density;
}

void CPhysicsFluidController::WakeAllSleepingObjects() 
{
	GetIVPObject()->get_controller_phantom()->wake_all_sleeping_objects();
}

int CPhysicsFluidController::GetContents() const
{
	return m_nContents;
}

IVP_Template_Buoyancy *CBuoyancyAttacher::get_parameters_per_core( IVP_Core *pCore )
{
	if ( pCore )
	{
		IVP_Real_Object *pivp = pCore->objects.element_at(0);
		CPhysicsObject *pPhys = static_cast<CPhysicsObject *>(pivp->client_data);

		// This ratio is for objects whose mass / (collision model) volume is not equal to their density.
		// Keep the fluid pressure/friction solution for the volume, but scale the buoyant force calculations
		// to be in line with the object's real density.  This is accompilshed by changing the fluid's density
		// on a per-object basis.
		float ratio = pPhys->GetBuoyancyRatio();

		if ( pPhys->GetShadowController() || !(pPhys->CallbackFlags() & CALLBACK_DO_FLUID_SIMULATION) )
		{
			// NOTE: don't do buoyancy on these guys for now!
			template_buoyancy.medium_density = 0;
		}
		else
		{
			template_buoyancy.medium_density = m_density * ratio;
		}
	}
	else
	{
		template_buoyancy.medium_density = m_density;
	}

	return &template_buoyancy;
}

CBuoyancyAttacher::CBuoyancyAttacher(IVP_Template_Buoyancy &templ, IVP_U_Set_Active<IVP_Core> *set_of_cores_, IVP_Liquid_Surface_Descriptor *liquid_surface_descriptor_)
	:IVP_Attacher_To_Cores_Buoyancy(templ, set_of_cores_, liquid_surface_descriptor_)
{
	m_density = templ.medium_density;
}


//-----------------------------------------------------------------------------
// Defines the surface descriptor in local space
//-----------------------------------------------------------------------------
class CLiquidSurfaceDescriptor : public IVP_Liquid_Surface_Descriptor
{
public:
	CLiquidSurfaceDescriptor( CPhysicsObject *pFluidObject, const Vector4D &plane, const Vector &current )
	{
		cplane_t worldPlane;
		worldPlane.normal = plane.AsVector3D();
		worldPlane.dist = plane[3];

		matrix3x4_t matObjectToWorld;
		pFluidObject->GetPositionMatrix( &matObjectToWorld );
		MatrixITransformPlane( matObjectToWorld, worldPlane, m_objectSpacePlane );

		VectorIRotate( current, matObjectToWorld, m_vecObjectSpaceCurrent );
		m_pFluidObject = pFluidObject;
	}
	
	virtual void calc_liquid_surface( IVP_Environment * /*environment*/,
								IVP_Core * /*core*/,
								IVP_U_Float_Hesse *surface_normal_out,
								IVP_U_Float_Point *abs_speed_of_current_out)
	{
		cplane_t worldPlane;
		matrix3x4_t matObjectToWorld;
		m_pFluidObject->GetPositionMatrix( &matObjectToWorld );
		MatrixTransformPlane( matObjectToWorld, m_objectSpacePlane, worldPlane );

		worldPlane.normal *= -1.0f;
		worldPlane.dist *= -1.0f;

		IVP_U_Float_Hesse worldSurface;
		ConvertPlaneToIVP( worldPlane.normal, worldPlane.dist, worldSurface );
		surface_normal_out->set(&worldSurface);
		surface_normal_out->hesse_val = worldSurface.hesse_val;

		Vector worldSpaceCurrent;
		VectorRotate( m_vecObjectSpaceCurrent, matObjectToWorld, worldSpaceCurrent );  

		IVP_U_Float_Point ivpWorldSpaceCurrent; 
		ConvertDirectionToIVP( worldSpaceCurrent, ivpWorldSpaceCurrent );
		abs_speed_of_current_out->set( &ivpWorldSpaceCurrent );
	}

private:
	Vector m_vecObjectSpaceCurrent;
	cplane_t m_objectSpacePlane;
	CPhysicsObject *m_pFluidObject;
};


CPhysicsFluidController *CreateFluidController( IVP_Environment *pEnvironment, CPhysicsObject *pFluidObject, fluidparams_t *pParams )
{
	pFluidObject->BecomeTrigger();

	IVP_Controller_Phantom *pPhantom = pFluidObject->GetObject()->get_controller_phantom();
	if ( !pPhantom )
		return NULL;

    IVP_Liquid_Surface_Descriptor *lsd = new CLiquidSurfaceDescriptor( pFluidObject, pParams->surfacePlane, pParams->currentVelocity );
	int surfaceprops = pFluidObject->GetMaterialIndex();
	float density = physprops->GetSurfaceData( surfaceprops )->physics.density;
    // ---------------------------------------------
    // create parameter template for Buoyancy_Solver
    // ---------------------------------------------
	// UNDONE: Expose these other parametersd
    IVP_Template_Buoyancy buoyancy_input;
    buoyancy_input.medium_density			= ConvertDensityToIVP(density); // density of water (unit: kg/m^3)
    buoyancy_input.pressure_damp_factor     = pParams->damping;
    buoyancy_input.viscosity_factor       = 0.0f;
    buoyancy_input.torque_factor          = 0.01f;
    buoyancy_input.viscosity_input_factor = 0.1f;
    // -------------------------------------------------------------------------------
    // create "water" (i.e. buoyancy solver) and attach a dynamic list of object cores
    // -------------------------------------------------------------------------------
    CBuoyancyAttacher *attacher_to_cores_buoyancy = new CBuoyancyAttacher( buoyancy_input, pPhantom->get_intruding_cores(), lsd );

	CPhysicsFluidController *pFluid = new CPhysicsFluidController( attacher_to_cores_buoyancy, lsd, pFluidObject, pParams->contents );
	pFluid->SetGameData( pParams->pGameData );
	pPhantom->client_data = static_cast<void *>(pFluid);

	return pFluid;
}


bool SavePhysicsFluidController( const physsaveparams_t &params, CPhysicsFluidController *pFluidObject )
{
	return false;
}

bool RestorePhysicsFluidController( const physrestoreparams_t &params, CPhysicsFluidController **ppFluidObject )
{
	return false;
}

