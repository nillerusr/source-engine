//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "c_basetfplayer.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"

CPhysCollide *PhysCreateBbox( const Vector &mins, const Vector &maxs );
extern CSolidSetDefaults g_SolidSetup;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_RagdollShadow : public C_BaseAnimating
{
	DECLARE_CLASS( C_RagdollShadow, C_BaseAnimating );
public:
	DECLARE_CLIENTCLASS();

	C_RagdollShadow( void );
	~C_RagdollShadow( void );

	virtual void OnDataChanged( DataUpdateType_t updateType );

	virtual void ClientThink( void );
	virtual int DrawModel( int flags );

public:
	IPhysicsObject *VPhysicsInitShadow( bool allowPhysicsMovement, bool allowPhysicsRotation );
	void			VPhysicsSetObject( IPhysicsObject *pPhysics );
	void			VPhysicsDestroyObject( void );

	int			m_nPlayer;
	EHANDLE		m_hPlayer;

	IPhysicsObject	*m_pPhysicsObject;
	IPhysicsSpring		*m_pSpring;
};

IMPLEMENT_CLIENTCLASS_DT( C_RagdollShadow, DT_RagdollShadow, CRagdollShadow )
	RecvPropInt( RECVINFO( m_nPlayer ) ),
END_RECV_TABLE()

C_RagdollShadow::C_RagdollShadow( void )
{
	m_nPlayer = -1;
	m_hPlayer = NULL;

	m_pPhysicsObject = NULL;
	m_pSpring = NULL;
}

C_RagdollShadow::~C_RagdollShadow( void )
{
	VPhysicsDestroyObject();

	delete m_pSpring;
}

void C_RagdollShadow::VPhysicsDestroyObject( void )
{
	if ( m_pPhysicsObject )
	{
		physenv->DestroyObject( m_pPhysicsObject );
		m_pPhysicsObject = NULL;
	}
}

// Create a physics thingy based on an existing collision model
IPhysicsObject *PhysModelCreateCustom( C_BaseEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *props )
{
	solid_t solid;
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	int surfaceProp = -1;
	if ( props && props[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( props );
	}
	solid.params.pGameData = static_cast<void *>(pEntity);
	IPhysicsObject *pObject = physenv->CreatePolyObject( pModel, surfaceProp, origin, angles, &solid.params );
	return pObject;
}

IPhysicsObject *PhysModelCreateRagdoll( C_BaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide )
		return NULL;

	solid_t solid;
	memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide->pKeyValues );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			pParse->ParseSolid( &solid, &g_SolidSetup );
			break;
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( solid.surfaceprop );
	}
	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = "ragdoll_player";
	IPhysicsObject *pObject = physenv->CreatePolyObject( pCollide->solids[0], surfaceProp, origin, angles, &solid.params );
	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );
	return pObject;
}

void C_RagdollShadow::VPhysicsSetObject( IPhysicsObject *pPhysics )
{
	if ( m_pPhysicsObject && pPhysics )
	{
		Warning( "C_RagdollShadow::Overwriting physics object!\n" );
	}
	m_pPhysicsObject = pPhysics;
}


// This creates a vphysics object with a shadow controller that follows the AI
IPhysicsObject *C_RagdollShadow::VPhysicsInitShadow( bool allowPhysicsMovement, bool allowPhysicsRotation )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return NULL;
	}

	// If this entity already has a physics object, then it should have been deleted prior to making this call.
	Assert(!m_pPhysicsObject);

	// make sure m_vecOrigin / m_vecAngles are correct
	const Vector &origin = GetAbsOrigin();
	QAngle angles = GetAbsAngles();
	IPhysicsObject *pPhysicsObject = NULL;

	if ( GetSolid() == SOLID_BBOX )
	{
		const char *pSurfaceProps = "flesh";
		if ( GetModelIndex() && modelinfo->GetModelType( GetModel() ) == mod_studio )
		{
			pSurfaceProps = Studio_GetDefaultSurfaceProps( hdr );
		}
		angles = vec3_angle;
		CPhysCollide *pCollide = PhysCreateBbox( WorldAlignMins(), WorldAlignMaxs() );
		if ( !pCollide )
			return NULL;
		pPhysicsObject = PhysModelCreateCustom( this, pCollide, origin, angles, pSurfaceProps );
	}
	else
	{
		pPhysicsObject = PhysModelCreateRagdoll( this, GetModelIndex(), origin, angles );
	}
	VPhysicsSetObject( pPhysicsObject );
	pPhysicsObject->SetShadow( 1e4, 1e4, allowPhysicsMovement, allowPhysicsRotation );
	pPhysicsObject->UpdateShadow( GetAbsOrigin(), GetAbsAngles(), false, 0 );
//	PhysAddShadow( this );
	return pPhysicsObject;
}	

void C_RagdollShadow::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Has to happen *after* the client handle is set
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	bool bnewentity = (updateType == DATA_UPDATE_CREATED);
	if ( bnewentity && ( m_nPlayer != 0 ) )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		Assert( !m_pPhysicsObject );

		C_BaseEntity *pl = static_cast< C_BaseEntity * >( cl_entitylist->GetEnt( m_nPlayer ) );
		if ( pl )
		{
			m_hPlayer = pl;
		}

		m_pPhysicsObject = VPhysicsInitShadow( true, false );
	}

	if ( m_pPhysicsObject )
	{
		// Create the spring if we don't have one yet
		if ( !m_pSpring )
		{
			C_BaseTFPlayer *pl = static_cast< C_BaseTFPlayer * >( (C_BaseEntity *)m_hPlayer );
			if ( pl && pl->VPhysicsGetObject() )
			{
				springparams_t spring;
				spring.constant = 15000;
				spring.damping = 1.0;
				spring.naturalLength = 0.0f;
				spring.relativeDamping = 100.0f;
				VectorCopy( vec3_origin, spring.startPosition );
				VectorCopy( vec3_origin, spring.endPosition );
				spring.useLocalPositions = true;

				m_pSpring = physenv->CreateSpring( m_pPhysicsObject, pl->VPhysicsGetObject(), &spring );

				PhysDisableObjectCollisions( m_pPhysicsObject, pl->VPhysicsGetObject() );
			}
		}

		m_pPhysicsObject->UpdateShadow( GetAbsOrigin(), GetAbsAngles(), false, 0 );
	}

}

void C_RagdollShadow::ClientThink( void )
{
	BaseClass::ClientThink();
}

int C_RagdollShadow::DrawModel( int flags )
{
//	int drawn = BaseClass::DrawModel( flags );
//	return drawn;
	return 0;
}
