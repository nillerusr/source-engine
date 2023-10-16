//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

#include "physics_friction.h"
#include "vphysics/friction.h"

#include "ivp_mindist.hxx"
#include "ivp_mindist_intern.hxx"
#include "ivp_listener_collision.hxx"
#include "ivp_friction.hxx"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFrictionSnapshot : public IPhysicsFrictionSnapshot
{
public:
	CFrictionSnapshot( IVP_Real_Object *pObject );
	~CFrictionSnapshot();

	bool IsValid();

	// Object 0 is this object, Object 1 is the other object
	IPhysicsObject *GetObject( int index );
	int GetMaterial( int index );

	void GetContactPoint( Vector &out );
	void GetSurfaceNormal( Vector &out );
	float GetNormalForce();
	float GetEnergyAbsorbed();
	void RecomputeFriction();
	void ClearFrictionForce();

	void MarkContactForDelete();
	void DeleteAllMarkedContacts( bool wakeObjects );
	void NextFrictionData();
	float GetFrictionCoefficient();


private:
	void SetFrictionSynapse( IVP_Synapse_Friction *pSet );
	CUtlVector<IVP_Real_Object *> *m_pDeleteList;
	IVP_Real_Object			*m_pObject;
	IVP_Synapse_Friction	*m_pFriction;
	IVP_Contact_Point		*m_pContactPoint;
	int						m_synapseIndex;
};

CFrictionSnapshot::CFrictionSnapshot( IVP_Real_Object *pObject ) : m_pObject(pObject)
{
	m_pDeleteList = NULL;
	SetFrictionSynapse( pObject->get_first_friction_synapse() );
}

CFrictionSnapshot::~CFrictionSnapshot()
{
	delete m_pDeleteList;
}

void CFrictionSnapshot::DeleteAllMarkedContacts( bool wakeObjects )
{
	if ( !m_pDeleteList )
		return;

	for ( int i = 0; i < m_pDeleteList->Count(); i++ )
	{
		if ( wakeObjects )
		{
			m_pDeleteList->Element(i)->ensure_in_simulation();
		}
		DeleteAllFrictionPairs( m_pObject, m_pDeleteList->Element(i) );
	}
	m_pFriction = NULL;
}

void CFrictionSnapshot::SetFrictionSynapse( IVP_Synapse_Friction *pSet )
{
	if ( pSet )
	{
		m_pFriction = pSet;
		m_pContactPoint = pSet->get_contact_point();
		m_synapseIndex = (pSet == m_pContactPoint->get_synapse(0)) ? 0 : 1;
	}
	else
	{
		m_pFriction = NULL;
		m_pContactPoint = NULL;
		m_synapseIndex = 0;
	}
}

bool CFrictionSnapshot::IsValid()
{
	return m_pFriction != NULL ? true : false;
}

IPhysicsObject *CFrictionSnapshot::GetObject( int index )
{
	IVP_Synapse_Friction *pFriction = m_pFriction;
	if ( index == 1 )
	{
		pFriction = m_pContactPoint->get_synapse(!m_synapseIndex);
	}
	return static_cast<IPhysicsObject *>(pFriction->get_object()->client_data);
}

void CFrictionSnapshot::MarkContactForDelete() 
{ 
	IVP_Synapse_Friction *pFriction = m_pContactPoint->get_synapse(!m_synapseIndex);
	IVP_Real_Object *pObject = pFriction->get_object();
	Assert(pObject != m_pObject);
	if ( pObject != m_pObject )
	{
		if ( !m_pDeleteList )
		{
			m_pDeleteList = new CUtlVector<IVP_Real_Object *>;
		}
		m_pDeleteList->AddToTail( pObject );
	}
}

int CFrictionSnapshot::GetMaterial( int index )
{
	IVP_Material *ivpMats[2];

    m_pContactPoint->get_material_info(ivpMats);
	
	// index 1 is the other one
	index ^= m_synapseIndex;

	return physprops->GetIVPMaterialIndex( ivpMats[index] );
}

void CFrictionSnapshot::GetContactPoint( Vector &out )
{
	ConvertPositionToHL( *m_pContactPoint->get_contact_point_ws(), out ); 
}

void CFrictionSnapshot::GetSurfaceNormal( Vector &out )
{
	float sign[2] = {1,-1};
	IVP_U_Float_Point normal;
	IVP_Contact_Point_API::get_surface_normal_ws(const_cast<IVP_Contact_Point *>(m_pContactPoint), &normal );
	ConvertDirectionToHL( normal, out );
	out *= sign[m_synapseIndex];
	VectorNormalize(out);
}

float CFrictionSnapshot::GetFrictionCoefficient()
{
	return m_pContactPoint->get_friction_factor();
}

float CFrictionSnapshot::GetNormalForce()
{
	return ConvertDistanceToHL( IVP_Contact_Point_API::get_vert_force( m_pContactPoint ) );
}

float CFrictionSnapshot::GetEnergyAbsorbed()
{
	return ConvertEnergyToHL( IVP_Contact_Point_API::get_eliminated_energy( m_pContactPoint ) );
}

void CFrictionSnapshot::RecomputeFriction()
{
	m_pContactPoint->recompute_friction();
}

void CFrictionSnapshot::ClearFrictionForce()
{
	m_pContactPoint->set_friction_to_neutral();
}

void CFrictionSnapshot::NextFrictionData()
{
	SetFrictionSynapse( m_pFriction->get_next() );
}

IPhysicsFrictionSnapshot *CreateFrictionSnapshot( IVP_Real_Object *pObject )
{
	return new CFrictionSnapshot( pObject );
}

void DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot )
{
	delete pSnapshot;
}


void DeleteAllFrictionPairs( IVP_Real_Object *pObject0, IVP_Real_Object *pObject1 )
{
	pObject0->unlink_contact_points_for_object( pObject1 );
}
