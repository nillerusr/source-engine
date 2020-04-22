//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "psheet.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "dmxloader/dmxloader.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "tier0/vprof.h"
#include "tier1/KeyValues.h"
#include "tier1/lzmaDecoder.h"
#include "random_floats.h"
#include "vtf/vtf.h"
#include "studio.h"
#include "particles_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




// rename table from the great rename 
static const char *s_RemapOperatorNameTable[]={
	"alpha_fade", "Alpha Fade and Decay",
	"alpha_fade_in_random", "Alpha Fade In Random",
	"alpha_fade_out_random", "Alpha Fade Out Random",
	"basic_movement", "Movement Basic",
	"color_fade", "Color Fade",
	"controlpoint_light", "Color Light From Control Point",
	"Dampen Movement Relative to Control Point", "Movement Dampen Relative to Control Point",
	"Distance Between Control Points Scale", "Remap Distance Between Two Control Points to Scalar",
	"Distance to Control Points Scale", "Remap Distance to Control Point to Scalar",
	"lifespan_decay", "Lifespan Decay",
	"lock to bone",	"Movement Lock to Bone",
	"postion_lock_to_controlpoint", "Movement Lock to Control Point",
	"maintain position along path", "Movement Maintain Position Along Path",
	"Match Particle Velocities", "Movement Match Particle Velocities",
	"Max Velocity", "Movement Max Velocity",
	"noise", "Noise Scalar",
	"vector noise", "Noise Vector",
	"oscillate_scalar", "Oscillate Scalar",
	"oscillate_vector", "Oscillate Vector",
	"Orient Rotation to 2D Direction", "Rotation Orient to 2D Direction",
	"radius_scale", "Radius Scale",
	"Random Cull", "Cull Random",
	"remap_scalar", "Remap Scalar",
	"rotation_movement", "Rotation Basic",
	"rotation_spin", "Rotation Spin Roll",
	"rotation_spin yaw", "Rotation Spin Yaw",
	"alpha_random", "Alpha Random",
	"color_random", "Color Random",
	"create from parent particles", "Position From Parent Particles",
	"Create In Hierarchy", "Position In CP Hierarchy",
	"random position along path", "Position Along Path Random",
	"random position on model", "Position on Model Random",
	"sequential position along path", "Position Along Path Sequential",
	"position_offset_random", "Position Modify Offset Random",
	"position_warp_random", "Position Modify Warp Random",
	"position_within_box", "Position Within Box Random",
	"position_within_sphere", "Position Within Sphere Random",
	"Inherit Velocity", "Velocity Inherit from Control Point",
	"Initial Repulsion Velocity", "Velocity Repulse from World",
	"Initial Velocity Noise", "Velocity Noise",
	"Initial Scalar Noise", "Remap Noise to Scalar",
	"Lifespan from distance to world", "Lifetime from Time to Impact",
	"Pre-Age Noise", "Lifetime Pre-Age Noise",
	"lifetime_random", "Lifetime Random",
	"radius_random", "Radius Random",
	"random yaw", "Rotation Yaw Random",
	"Randomly Flip Yaw", "Rotation Yaw Flip Random",
	"rotation_random", "Rotation Random",
	"rotation_speed_random", "Rotation Speed Random",
	"sequence_random", "Sequence Random",
	"second_sequence_random", "Sequence Two Random",
	"trail_length_random", "Trail Length Random",
	"velocity_random", "Velocity Random",
};

static char const *RemapOperatorName( char const *pOpName )
{
	for( int i = 0 ; i < ARRAYSIZE( s_RemapOperatorNameTable ) ; i += 2 )
	{
		if ( Q_stricmp( pOpName, s_RemapOperatorNameTable[i] ) == 0 )
		{
			return s_RemapOperatorNameTable[i + 1 ];
		}
	}
	return pOpName;
}


// This is the soft limit - if we exceed this, we spit out a report of all the particles in the frame
#define MAX_PARTICLE_VERTS	50000
// These are some limits that control g_pParticleSystemMgr->ParticleThrottleScaling() and g_pParticleSystemMgr->ParticleThrottleRandomEnable()
//ConVar cl_particle_scale_lower ( "cl_particle_scale_lower", "20000", FCVAR_CLIENTDLL | FCVAR_CHEAT );
//ConVar cl_particle_scale_upper ( "cl_particle_scale_upper", "40000", FCVAR_CLIENTDLL | FCVAR_CHEAT );
#define CL_PARTICLE_SCALE_LOWER 20000
#define CL_PARTICLE_SCALE_UPPER 40000


//-----------------------------------------------------------------------------
// Default implementation of particle system mgr
//-----------------------------------------------------------------------------
static CParticleSystemMgr s_ParticleSystemMgr;
CParticleSystemMgr *g_pParticleSystemMgr = &s_ParticleSystemMgr;


int g_nParticle_Multiplier = 1;
	

//-----------------------------------------------------------------------------
// Particle dictionary
//-----------------------------------------------------------------------------
class CParticleSystemDictionary
{
public:
	~CParticleSystemDictionary();

	CParticleSystemDefinition* AddParticleSystem( CDmxElement *pParticleSystem );
	int Count() const;
	int NameCount() const;
	CParticleSystemDefinition* GetParticleSystem( int i );
	ParticleSystemHandle_t FindParticleSystemHandle( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( ParticleSystemHandle_t h );
	CParticleSystemDefinition* FindParticleSystem( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( const DmObjectId_t &id );
	
	CParticleSystemDefinition* operator[]( int idx )
	{
		return m_ParticleNameMap[ idx ];
	}

private:
	typedef CUtlStringMap< CParticleSystemDefinition * > ParticleNameMap_t;
	typedef CUtlVector< CParticleSystemDefinition* > ParticleIdMap_t;

	void DestroyExistingElement( CDmxElement *pElement );

	ParticleNameMap_t m_ParticleNameMap;
	ParticleIdMap_t m_ParticleIdMap;
};


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CParticleSystemDictionary::~CParticleSystemDictionary()
{
	int nCount = m_ParticleIdMap.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_ParticleIdMap[i];
	}
}


//-----------------------------------------------------------------------------
// Destroys an existing element, returns if this element should be added to the name list
//-----------------------------------------------------------------------------
void CParticleSystemDictionary::DestroyExistingElement( CDmxElement *pElement )
{
	const char *pParticleSystemName = pElement->GetName();
	bool bPreventNameBasedLookup = pElement->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		if ( m_ParticleNameMap.Defined( pParticleSystemName ) )
		{
			CParticleSystemDefinition *pDef = m_ParticleNameMap[ pParticleSystemName ];
			delete pDef;
			m_ParticleNameMap[ pParticleSystemName ] = NULL;
		}
		return;
	}
	
	// Use id based lookup instead
	int nCount = m_ParticleIdMap.Count();
	const DmObjectId_t& id = pElement->GetId();
	for ( int i = 0; i < nCount; ++i )
	{
		// Was already removed by the name lookup
		if ( !IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			continue;

		CParticleSystemDefinition *pDef = m_ParticleIdMap[ i ];
		m_ParticleIdMap.FastRemove( i );
		delete pDef;
		break;
	}
}


//-----------------------------------------------------------------------------
// Adds a destructor
//-----------------------------------------------------------------------------
CParticleSystemDefinition* CParticleSystemDictionary::AddParticleSystem( CDmxElement *pParticleSystem )
{
	if ( Q_stricmp( pParticleSystem->GetTypeString(), "DmeParticleSystemDefinition" ) )
		return NULL;

	DestroyExistingElement( pParticleSystem );

	CParticleSystemDefinition *pDef = new CParticleSystemDefinition;

	// Must add the def to the maps before Read() because Read() may create new child particle systems
	bool bPreventNameBasedLookup = pParticleSystem->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		m_ParticleNameMap[ pParticleSystem->GetName() ] = pDef;
	}
	else
	{
		m_ParticleIdMap.AddToTail( pDef );
	}

	pDef->Read( pParticleSystem );
	return pDef;
}

int CParticleSystemDictionary::NameCount() const
{
	return m_ParticleNameMap.GetNumStrings();
}

int CParticleSystemDictionary::Count() const
{
	return m_ParticleIdMap.Count();
}

CParticleSystemDefinition* CParticleSystemDictionary::GetParticleSystem( int i )
{
	return m_ParticleIdMap[i];
}

ParticleSystemHandle_t CParticleSystemDictionary::FindParticleSystemHandle( const char *pName )
{
	return m_ParticleNameMap.Find( pName );
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( ParticleSystemHandle_t h )
{
	if ( h == UTL_INVAL_SYMBOL || h >= m_ParticleNameMap.GetNumStrings() )
		return NULL;
	return m_ParticleNameMap[ h ];
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const char *pName )
{
	if ( m_ParticleNameMap.Defined( pName ) )
		return m_ParticleNameMap[ pName ];
	return NULL;
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const DmObjectId_t &id )
{
	int nCount = m_ParticleIdMap.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			return m_ParticleIdMap[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// For editing, create a faked particle operator definition for children
// The only thing used in here is GetUnpackStructure.
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( ParticleChildrenInfo_t ) 
	DMXELEMENT_UNPACK_FIELD( "delay", "0.0", float, m_flDelay )
END_DMXELEMENT_UNPACK( ParticleChildrenInfo_t, s_ChildrenInfoUnpack )

class CChildOperatorDefinition : public IParticleOperatorDefinition
{
public:
	virtual const char *GetName() const { Assert(0); return NULL; }
	virtual CParticleOperatorInstance *CreateInstance( const DmObjectId_t &id ) const { Assert(0); return NULL; }
	//	virtual void DestroyInstance( CParticleOperatorInstance *pInstance ) const { Assert(0); }
	virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const
	{
		return s_ChildrenInfoUnpack;
	}
	virtual ParticleOperatorId_t GetId() const { return OPERATOR_GENERIC; }
	virtual bool IsObsolete() const { return false; }
	virtual size_t GetClassSize() const { return 0; }
};

static CChildOperatorDefinition s_ChildOperatorDefinition;


//-----------------------------------------------------------------------------
//
// CParticleSystemDefinition
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Unpack structure for CParticleSystemDefinition
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( CParticleSystemDefinition ) 
	DMXELEMENT_UNPACK_FIELD( "max_particles", "1000", int, m_nMaxParticles )
	DMXELEMENT_UNPACK_FIELD( "initial_particles", "0", int, m_nInitialParticles )
	DMXELEMENT_UNPACK_FIELD_STRING_USERDATA( "material", "vgui/white", m_pszMaterialName, "vmtPicker" )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_min", "-10 -10 -10", Vector, m_BoundingBoxMin )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_max", "10 10 10", Vector, m_BoundingBoxMax )
	DMXELEMENT_UNPACK_FIELD( "cull_radius", "0", float, m_flCullRadius )
	DMXELEMENT_UNPACK_FIELD( "cull_cost", "1", float, m_flCullFillCost )
	DMXELEMENT_UNPACK_FIELD( "cull_control_point", "0", int, m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD_STRING( "cull_replacement_definition", "", m_pszCullReplacementName )
	DMXELEMENT_UNPACK_FIELD( "radius", "5", float, m_flConstantRadius )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_ConstantColor )
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_flConstantRotation )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed", "0", float, m_flConstantRotationSpeed )
	DMXELEMENT_UNPACK_FIELD( "sequence_number", "0", int, m_nConstantSequenceNumber )
	DMXELEMENT_UNPACK_FIELD( "sequence_number 1", "0", int, m_nConstantSequenceNumber1 )
	DMXELEMENT_UNPACK_FIELD( "group id", "0", int, m_nGroupID )
	DMXELEMENT_UNPACK_FIELD( "maximum time step", "0.1", float, m_flMaximumTimeStep )
	DMXELEMENT_UNPACK_FIELD( "maximum sim tick rate", "0.0", float, m_flMaximumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum sim tick rate", "0.0", float, m_flMinimumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum rendered frames", "0", int, m_nMinimumFrames )
	DMXELEMENT_UNPACK_FIELD( "control point to disable rendering if it is the camera", "-1", int, m_nSkipRenderControlPoint )
	DMXELEMENT_UNPACK_FIELD( "maximum draw distance", "100000.0", float, m_flMaxDrawDistance )
	DMXELEMENT_UNPACK_FIELD( "time to sleep when not drawn", "8", float, m_flNoDrawTimeToGoToSleep )
	DMXELEMENT_UNPACK_FIELD( "Sort particles", "1", bool, m_bShouldSort )
	DMXELEMENT_UNPACK_FIELD( "batch particle systems", "0", bool, m_bShouldBatch )
	DMXELEMENT_UNPACK_FIELD( "view model effect", "0", bool, m_bViewModelEffect )
END_DMXELEMENT_UNPACK( CParticleSystemDefinition, s_pParticleSystemDefinitionUnpack )



//-----------------------------------------------------------------------------
//
// CParticleOperatorDefinition begins here
// A template describing how a particle system will function
//
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::UnlinkAllCollections()
{
	while ( m_pFirstCollection )
	{
		m_pFirstCollection->UnlinkFromDefList();
	}
}

const char *CParticleSystemDefinition::GetName() const
{
	return m_Name;
}


//-----------------------------------------------------------------------------
// Should we always precache this?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::ShouldAlwaysPrecache() const
{
	return m_bAlwaysPrecache;
}


//-----------------------------------------------------------------------------
// Precache/uncache
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::Precache()
{
	if ( m_bIsPrecached )
		return;

	m_bIsPrecached = true;
#ifndef SWDS
	m_Material.Init( MaterialName(), TEXTURE_GROUP_OTHER, true );
#endif

	int nChildCount = m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Precache();
		}
	}
}

void CParticleSystemDefinition::Uncache()
{
	if ( !m_bIsPrecached )
		return;

	m_bIsPrecached = false;
	m_Material.Shutdown();	
//	m_Material.Init( "debug/particleerror", TEXTURE_GROUP_OTHER, true );

	int nChildCount = m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Uncache();
		}
	}
}


//-----------------------------------------------------------------------------
// Has this been precached?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::IsPrecached() const
{
	return m_bIsPrecached;
}

//-----------------------------------------------------------------------------
// Helper methods to help with unserialization
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::ParseOperators(
	const char *pszOpKey, ParticleFunctionType_t nFunctionType,
	CDmxElement *pElement,
	CUtlVector<CParticleOperatorInstance *> &outList)
{
	const CDmxAttribute* pAttribute = pElement->GetAttribute( pszOpKey );
	if ( !pAttribute || pAttribute->GetType() != AT_ELEMENT_ARRAY )
		return;

	const CUtlVector<IParticleOperatorDefinition *> &flist = g_pParticleSystemMgr->GetAvailableParticleOperatorList( nFunctionType );

	const CUtlVector< CDmxElement* >& ops = pAttribute->GetArray<CDmxElement*>( );
	int nCount = ops.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pOrigName = ops[i]->GetValueString( "functionName" );
		char const *pOpName = RemapOperatorName( pOrigName );
		if ( pOpName != pOrigName )
		{
			pElement->SetValue( "functionName", pOpName );
		}
		bool bFound = false;
		int nFunctionCount = flist.Count();
		for( int j = 0; j < nFunctionCount; ++j )
		{
			if ( Q_stricmp( pOpName, flist[j]->GetName() ) )
				continue;

			// found it!
			bFound = true;

			CParticleOperatorInstance *pNewRef = flist[j]->CreateInstance( ops[i]->GetId() );
			const DmxElementUnpackStructure_t *pUnpack = flist[j]->GetUnpackStructure();
			if ( pUnpack )
			{
				ops[i]->UnpackIntoStructure( pNewRef, flist[j]->GetClassSize(), pUnpack );
			}
			pNewRef->InitParams( this, pElement );
			m_nAttributeReadMask |= pNewRef->GetReadAttributes();
			m_nControlPointReadMask |= pNewRef->GetReadControlPointMask();

			switch( nFunctionType )
			{
			case FUNCTION_INITIALIZER:
			case FUNCTION_EMITTER:
				m_nPerParticleInitializedAttributeMask |= pNewRef->GetWrittenAttributes();
				Assert( pNewRef->GetReadInitialAttributes() == 0 );
				break;

			case FUNCTION_OPERATOR:
				m_nPerParticleUpdatedAttributeMask |= pNewRef->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef->GetReadInitialAttributes();
				break;
				
			case FUNCTION_RENDERER:
				m_nPerParticleUpdatedAttributeMask |= pNewRef->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef->GetReadInitialAttributes();
				break;
			}

			// Special case: Reading particle ID means we're reading the initial particle id
			if ( ( pNewRef->GetReadAttributes() | pNewRef->GetReadInitialAttributes() ) & PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK )
			{
				m_nInitialAttributeReadMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
				m_nPerParticleInitializedAttributeMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
			}

			outList.AddToTail( pNewRef );
			break;
		}

		if ( !bFound )
		{
			if ( flist.Count() )							// don't warn if no ops of that type defined (server)
				Warning( "Didn't find particle function %s\n", pOpName );
		}
	}
}

void CParticleSystemDefinition::ParseChildren( CDmxElement *pElement )
{
	const CUtlVector<CDmxElement*>& children = pElement->GetArray<CDmxElement*>( "children" );
	int nCount = children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChild = children[i]->GetValue<CDmxElement*>( "child" );
		if ( !pChild || Q_stricmp( pChild->GetTypeString(), "DmeParticleSystemDefinition" ) )
			continue;

		int j = m_Children.AddToTail();
		children[i]->UnpackIntoStructure( &m_Children[j], sizeof( m_Children[j] ), s_ChildrenInfoUnpack );
		m_Children[j].m_bUseNameBasedLookup = !pChild->GetValue<bool>( "preventNameBasedLookup" );
		if ( m_Children[j].m_bUseNameBasedLookup )
		{
			m_Children[j].m_Name = pChild->GetName();
		}
		else
		{
			CopyUniqueId( pChild->GetId(), &m_Children[j].m_Id );
		}

		// Check to see if this child has been encountered already, and if not, then
		// create a new particle definition for this child
		g_pParticleSystemMgr->AddParticleSystem( pChild );
	}
}

void CParticleSystemDefinition::Read( CDmxElement *pElement )
{
	m_Name = pElement->GetName();
	CopyUniqueId( pElement->GetId(), &m_Id );
	pElement->UnpackIntoStructure( this, sizeof( *this ), s_pParticleSystemDefinitionUnpack );

#ifndef SWDS												// avoid material/ texture load
// NOTE: This makes a X appear for uncached particles.
//	m_Material.Init( "debug/particleerror", TEXTURE_GROUP_OTHER, true );
#endif

	if ( m_nInitialParticles < 0 )
	{
		m_nInitialParticles = 0;
	}
	if ( m_nMaxParticles < 1 )
	{
		m_nMaxParticles = 1;
	}
	m_nMaxParticles *= g_nParticle_Multiplier;
	m_nMaxParticles = min( m_nMaxParticles, MAX_PARTICLES_IN_A_SYSTEM );
	if ( m_flCullRadius > 0 )
	{
		m_nControlPointReadMask |= 1ULL << m_nCullControlPoint;
	}

	ParseOperators( "renderers", FUNCTION_RENDERER, pElement, m_Renderers );
	ParseOperators( "operators", FUNCTION_OPERATOR, pElement, m_Operators );
	ParseOperators( "initializers", FUNCTION_INITIALIZER, pElement, m_Initializers );
	ParseOperators( "emitters", FUNCTION_EMITTER, pElement, m_Emitters );
	ParseChildren( pElement );
	ParseOperators( "forces", FUNCTION_FORCEGENERATOR, pElement, m_ForceGenerators );
	ParseOperators( "constraints", FUNCTION_CONSTRAINT, pElement, m_Constraints );
	SetupContextData();
}

IMaterial *CParticleSystemDefinition::GetMaterial() const
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	if ( !IsPrecached() )
		return NULL;
	return m_Material;
}


//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesPowerOfTwoFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	return m_Material->NeedsPowerOfTwoFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesFullFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	return m_Material->NeedsFullFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//-----------------------------------------------------------------------------
// Helper methods to write particle systems
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::WriteOperators( CDmxElement *pElement, 
	const char *pOpKeyName, const CUtlVector<CParticleOperatorInstance *> &inList )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( pOpKeyName );
	CUtlVector< CDmxElement* >& ops = pAttribute->GetArrayForEdit<CDmxElement*>( );

	int nCount = inList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pOperator = CreateDmxElement( "DmeParticleOperator" );
		ops.AddToTail( pOperator );

		const IParticleOperatorDefinition *pDef = inList[i]->GetDefinition();
		pOperator->SetValue( "name", pDef->GetName() );
		pOperator->SetValue( "functionName", pDef->GetName() );

		const DmxElementUnpackStructure_t *pUnpack = pDef->GetUnpackStructure();
		if ( pUnpack )
		{
			pOperator->AddAttributesFromStructure( inList[i], pUnpack );
		}
	}
}

void CParticleSystemDefinition::WriteChildren( CDmxElement *pElement )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( "children" );
	CUtlVector< CDmxElement* >& children = pAttribute->GetArrayForEdit<CDmxElement*>( );
	int nCount = m_Children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = CreateDmxElement( "DmeParticleChild" );
		children.AddToTail( pChildRef );
		children[i]->AddAttributesFromStructure( &m_Children[i], s_ChildrenInfoUnpack );
		CDmxElement *pChildParticleSystem;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( m_Children[i].m_Name );
		}
		else
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( m_Children[i].m_Id );
		}
		pChildRef->SetValue( "name", pChildParticleSystem->GetName() );
		pChildRef->SetValue( "child", pChildParticleSystem );
	}
}

CDmxElement *CParticleSystemDefinition::Write()
{
	const char *pName = GetName();

	CDmxElement *pElement = CreateDmxElement( "DmeParticleSystemDefinition" );
	pElement->SetValue( "name", pName );
	pElement->AddAttributesFromStructure( this, s_pParticleSystemDefinitionUnpack );
	WriteOperators( pElement, "renderers",m_Renderers );
	WriteOperators( pElement, "operators", m_Operators );
	WriteOperators( pElement, "initializers", m_Initializers );
	WriteOperators( pElement, "emitters", m_Emitters );
	WriteChildren( pElement );
	WriteOperators( pElement, "forces", m_ForceGenerators );
	WriteOperators( pElement, "constraints", m_Constraints );

	return pElement;
}

void CParticleSystemDefinition::SetupContextData( void )
{
	// calcuate sizes and offsets for context data
	CUtlVector<CParticleOperatorInstance *> *olists[] = {
		&m_Operators, &m_Renderers, &m_Initializers, &m_Emitters, &m_ForceGenerators,
		&m_Constraints
	};
	CUtlVector<size_t> *offsetLists[] = {
		&m_nOperatorsCtxOffsets, &m_nRenderersCtxOffsets,
		&m_nInitializersCtxOffsets, &m_nEmittersCtxOffsets,
		&m_nForceGeneratorsCtxOffsets, &m_nConstraintsCtxOffsets,
	};

	// loop through all operators, fill in offset entries, and calulate total data needed
	m_nContextDataSize = 0;
	for( int i = 0; i < NELEMS( olists ); i++ )
	{
		int nCount = olists[i]->Count();
		for( int j = 0; j < nCount; j++ )
		{
			offsetLists[i]->AddToTail( m_nContextDataSize );
			m_nContextDataSize += (*olists[i])[j]->GetRequiredContextBytes();
			// align context data
			m_nContextDataSize = (m_nContextDataSize + 15) & (~0xf );
		}
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
CUtlVector<CParticleOperatorInstance *> *CParticleSystemDefinition::GetOperatorList( ParticleFunctionType_t type )
{
	switch( type )
	{
	case FUNCTION_EMITTER:
		return &m_Emitters;
	case FUNCTION_RENDERER:
		return &m_Renderers;
	case FUNCTION_INITIALIZER:
		return &m_Initializers;
	case FUNCTION_OPERATOR:
		return &m_Operators;
	case FUNCTION_FORCEGENERATOR:
		return &m_ForceGenerators;
	case FUNCTION_CONSTRAINT:
		return &m_Constraints;
	default:
		Assert(0);
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
CParticleOperatorInstance *CParticleSystemDefinition::FindOperatorById( ParticleFunctionType_t type, const DmObjectId_t &id )
{
	CUtlVector<CParticleOperatorInstance *> *pVec = GetOperatorList( type );
	if ( !pVec )
		return NULL;

	int nCount = pVec->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( id, pVec->Element(i)->GetId() ) )
			return pVec->Element(i);
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//
// CParticleOperatorInstance
//
//-----------------------------------------------------------------------------
void CParticleOperatorInstance::InitNewParticles( CParticleCollection *pParticles,
												  int nFirstParticle, int nParticleCount,
												  int nAttributeWriteMask, void *pContext ) const
{
	if ( !nParticleCount )
		return;

	if ( nParticleCount < 16 )								// don't bother with vectorizing
															// unless enough particles to bother
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
		return;
	}
	
	int nHead = nFirstParticle & 3;
	if ( nHead )
	{
		// need to init up to 3 particles before we are block aligned
		int nHeadCount = min( nParticleCount, 4 - nHead );
		InitNewParticlesScalar( pParticles, nFirstParticle, nHeadCount, nAttributeWriteMask, pContext );
		nParticleCount -= nHeadCount;
		nFirstParticle += nHeadCount;
	}

	// now, we are aligned
	int nBlockCount = nParticleCount / 4;
	if ( nBlockCount )
	{
		InitNewParticlesBlock( pParticles, nFirstParticle / 4, nBlockCount, nAttributeWriteMask, pContext );
		nParticleCount -= 4 * nBlockCount;
		nFirstParticle += 4 * nBlockCount;
	}

	// do tail
	if ( nParticleCount )
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
	}
}


//-----------------------------------------------------------------------------
//
// CParticleCollection
//
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// need custom new/delete for alignment for simd
//------------------------------------------------------------------------------
#include "tier0/memdbgoff.h"
void *CParticleCollection::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CParticleCollection::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAligned( nSize, 16, pFileName, nLine );
}

void CParticleCollection::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleCollection::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void *CWorldCollideContextData::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CWorldCollideContextData::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAligned( nSize, 16, pFileName, nLine );
}

void CWorldCollideContextData::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CWorldCollideContextData::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}


#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CParticleCollection::CParticleCollection( )
{
	COMPILE_TIME_ASSERT( ( MAX_RANDOM_FLOATS & ( MAX_RANDOM_FLOATS - 1 ) ) == 0 );
	COMPILE_TIME_ASSERT( sizeof( s_pRandomFloats ) / sizeof( float ) >= MAX_RANDOM_FLOATS );

	m_pNextDef = m_pPrevDef = NULL;
	m_nUniqueParticleId = 0;
	m_nRandomQueryCount = 0;
	m_bIsScrubbable = false;
	m_bIsRunningInitializers = false;
	m_bIsRunningOperators = false;
	m_bIsTranslucent = false;
	m_bIsTwoPass = false;
	m_bIsBatchable = false;
	m_bUsesPowerOfTwoFrameBufferTexture = false;
	m_bUsesFullFrameBufferTexture = false;
	m_pRenderOp = NULL;
	m_nControlPointReadMask = 0;

	m_flLastMinDistSqr = m_flLastMaxDistSqr = 0.0f;
	m_flMinDistSqr = m_flMaxDistSqr = 0.0f;
	m_flOOMaxDistSqr = 1.0f;
	m_vecLastCameraPos.Init();
	m_MinBounds.Init();
	m_MaxBounds.Init();
	m_bBoundsValid = false;

	memset( m_ControlPoints, 0, sizeof(m_ControlPoints) );

	// align all control point orientations with the global world
	for( int i=0; i < MAX_PARTICLE_CONTROL_POINTS; i++ )
	{
		m_ControlPoints[i].m_ForwardVector.Init( 0, 1, 0 );
		m_ControlPoints[i].m_UpVector.Init( 0, 0, 1 );
		m_ControlPoints[i].m_RightVector.Init( 1, 0, 0 );
	}

	memset( m_pParticleInitialAttributes, 0, sizeof(m_pParticleInitialAttributes) );

	m_nPerParticleUpdatedAttributeMask = 0;
	m_nPerParticleInitializedAttributeMask = 0;
	m_nPerParticleReadInitialAttributeMask = 0;
	m_pParticleMemory = NULL;
	m_pParticleInitialMemory = NULL;
	m_pConstantMemory = NULL;
	m_nActiveParticles = 0;
	m_nPaddedActiveParticles = 0;
	m_flCurTime = 0.0f;
	m_fl4CurTime = Four_Zeros;
	m_flDt = 0.0f;
	m_flPreviousDt = 0.05f;
	m_nParticleFlags = PCFLAGS_FIRST_FRAME;
	m_pOperatorContextData = NULL;
	m_pNext = m_pPrev = NULL;
	m_nRandomSeed = 0;
	m_pDef = NULL;
	m_nAllocatedParticles = 0;
	m_nMaxAllowedParticles = 0;
	m_bDormant = false;
	m_bEmissionStopped = false;
	m_bRequiresOrderInvariance = false;
	m_nSimulatedFrames = 0;

	m_nNumParticlesToKill = 0;
	m_pParticleKillList = NULL;
	m_nHighestCP = 0;
	memset( m_pCollisionCacheData, 0, sizeof( m_pCollisionCacheData ) );
	m_pParent = NULL;
	m_LocalLighting = Color(255, 255, 255, 255);
	m_LocalLightingCP = -1;
	
}

CParticleCollection::~CParticleCollection( void )
{
	UnlinkFromDefList();

	m_Children.Purge();

	if ( m_pParticleMemory )
	{
		delete[] m_pParticleMemory;
	}
	if ( m_pParticleInitialMemory )
	{
		delete[] m_pParticleInitialMemory;
	}
	if ( m_pConstantMemory )
	{
		delete[] m_pConstantMemory;
	}
	if ( m_pOperatorContextData )
	{
		MemAlloc_FreeAligned( m_pOperatorContextData );
	}

	for( int i = 0 ; i < ARRAYSIZE( m_pCollisionCacheData ) ; i++ )
	{
		if ( m_pCollisionCacheData[i] )
		{
			delete m_pCollisionCacheData[i];
		}
	}
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CParticleCollection::Init( CParticleSystemDefinition *pDef, float flDelay, int nRandomSeed )
{
	m_pDef = pDef;

	// Link into def list
	LinkIntoDefList();

	InitStorage( pDef );

	// Initialize sheet data
	m_Sheet.Set( g_pParticleSystemMgr->FindOrLoadSheet( pDef->GetMaterial() ) );

	// FIXME: This seed needs to be recorded per instance!
	m_bIsScrubbable = ( nRandomSeed != 0 );
	if ( m_bIsScrubbable )
	{
		m_nRandomSeed = nRandomSeed;
	}
	else
	{
		m_nRandomSeed = (int)this;
#ifndef _DEBUG
		m_nRandomSeed += Plat_MSTime();
#endif
	}

	SetAttributeToConstant( PARTICLE_ATTRIBUTE_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PREV_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_LIFE_DURATION, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_RADIUS, pDef->m_flConstantRadius );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION, pDef->m_flConstantRotation );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION_SPEED, pDef->m_flConstantRotationSpeed );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TINT_RGB,
		pDef->m_ConstantColor.r() / 255.0f, pDef->m_ConstantColor.g() / 255.0f,
		pDef->m_ConstantColor.g() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA, pDef->m_ConstantColor.a() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_CREATION_TIME, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, pDef->m_nConstantSequenceNumber );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, pDef->m_nConstantSequenceNumber1 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, 0.1f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PARTICLE_ID, 0 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_YAW, 0 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA2, 1.0f );

	// Offset the child in time
	m_flCurTime = -flDelay;
	m_fl4CurTime = ReplicateX4( m_flCurTime );
	if ( m_pDef->m_nContextDataSize )
	{
		m_pOperatorContextData = reinterpret_cast<uint8 *> 
			( MemAlloc_AllocAligned( m_pDef->m_nContextDataSize, 16 ) );
	}
	
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + pDef->m_flNoDrawTimeToGoToSleep;

	// now, init context data
	CUtlVector<CParticleOperatorInstance *> *olists[] =
	{
		&(m_pDef->m_Operators), &(m_pDef->m_Renderers), 
		&(m_pDef->m_Initializers), &(m_pDef->m_Emitters),
		&(m_pDef->m_ForceGenerators),
		&(m_pDef->m_Constraints),
	};
	CUtlVector<size_t> *offsetlists[]=
	{
		&(m_pDef->m_nOperatorsCtxOffsets), &(m_pDef->m_nRenderersCtxOffsets),
		&(m_pDef->m_nInitializersCtxOffsets), &(m_pDef->m_nEmittersCtxOffsets),
		&(m_pDef->m_nForceGeneratorsCtxOffsets),
		&(m_pDef->m_nConstraintsCtxOffsets),

	};

	for( int i=0; i<NELEMS( olists ); i++ )
	{
		int nOperatorCount = olists[i]->Count();
		for( int j=0; j < nOperatorCount; j++ )
		{
			(*olists[i])[j]->InitializeContextData( this, m_pOperatorContextData+ (*offsetlists)[i][j] );
		}
	}

	m_nControlPointReadMask = pDef->m_nControlPointReadMask;

	// Instance child particle systems
	int nChildCount = pDef->m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		if ( nRandomSeed != 0 )
		{
			nRandomSeed += 129;
		}

		CParticleCollection *pChild;
		if ( pDef->m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Name, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		else
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Id, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		if ( pChild )
		{
			pChild->m_pParent = this;
			m_Children.AddToTail( pChild );
			m_nControlPointReadMask |= pChild->m_nControlPointReadMask;
		}
	}

	if ( !IsValid() )
		return;

	m_bIsTranslucent = ComputeIsTranslucent();
	m_bIsTwoPass = ComputeIsTwoPass();
	m_bIsBatchable = ComputeIsBatchable();
	LabelTextureUsage();
	m_bAnyUsesPowerOfTwoFrameBufferTexture = ComputeUsesPowerOfTwoFrameBufferTexture();
	m_bAnyUsesFullFrameBufferTexture = ComputeUsesFullFrameBufferTexture();
	m_bRequiresOrderInvariance = ComputeRequiresOrderInvariance();
}


//-----------------------------------------------------------------------------
// Used by client code
//-----------------------------------------------------------------------------
bool CParticleCollection::Init( CParticleSystemDefinition *pDef )
{
	if ( !pDef ) // || !pDef->IsPrecached() )
	{
		Warning( "Particlelib: Missing precache for particle system type \"%s\"!\n", pDef ? pDef->GetName() : "unknown" );
		CParticleSystemDefinition *pErrorDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
		if ( pErrorDef )
		{
			pDef = pErrorDef;
		}
	}

	Init( pDef, 0.0f, 0 );
	return IsValid();
}

bool CParticleCollection::Init( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return false;

	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", pParticleSystemName );
		return false;
	}
	return Init( pDef );
}


//-----------------------------------------------------------------------------
// List management for collections sharing the same particle definition
//-----------------------------------------------------------------------------
void CParticleCollection::LinkIntoDefList( )
{
	Assert( !m_pPrevDef && !m_pNextDef );

	m_pPrevDef = NULL;
	m_pNextDef = m_pDef->m_pFirstCollection;
	m_pDef->m_pFirstCollection = this;
	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = this;
	}

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		Assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif
}

void CParticleCollection::UnlinkFromDefList( )
{
	if ( !m_pDef )
		return;

	if ( m_pDef->m_pFirstCollection == this )
	{
		m_pDef->m_pFirstCollection = m_pNextDef;
		Assert( !m_pPrevDef );
	}
	else 
	{
		Assert( m_pPrevDef );
		m_pPrevDef->m_pNextDef = m_pNextDef;
	}

	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = m_pPrevDef;
	}

	m_pNextDef = m_pPrevDef = NULL;

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		Assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif
}

//-----------------------------------------------------------------------------
// Determine if this particle has moved since the last time it was simulated, 
// which will let us know if the bbox needs to be updated.
//-----------------------------------------------------------------------------
bool CParticleCollection::HasMoved() const
{
	// It's weird that this is possible, but it apparently is (see the many other functions that 
	// check).
	if ( !m_pDef )
		return false;

	Vector prevCP;
	for ( int i = 0; i <= m_nHighestCP; ++i )
	{
		if ( !m_pDef->ReadsControlPoint( i ) )
			continue;

		GetControlPointAtPrevTime( i, &prevCP );
		if ( prevCP != GetControlPointAtCurrentTime( i ) )
		{
			return true;
		}
	}

	for ( CParticleCollection *child = m_Children.m_pHead; child; child = child->m_pNext )
	{
		if ( child->HasMoved() )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Particle memory initialization
//-----------------------------------------------------------------------------
void CParticleCollection::InitStorage( CParticleSystemDefinition *pDef )
{
	Assert( pDef->m_nMaxParticles < 65536 );

	m_nMaxAllowedParticles = min ( MAX_PARTICLES_IN_A_SYSTEM, pDef->m_nMaxParticles );
	m_nAllocatedParticles = 4 + 4 * ( ( m_nMaxAllowedParticles + 3 ) / 4 );						    

	int nConstantMemorySize = 3 * 4 * MAX_PARTICLE_ATTRIBUTES * sizeof(float) + 16;
						 
	// Align allocation for constant attributes to 16 byte boundaries
	m_pConstantMemory =	new unsigned char[nConstantMemorySize];
	m_pConstantAttributes = (float*)( (size_t)( m_pConstantMemory + 15 ) & ~0xF );

	// We have to zero-init the memory so that any attributes that are not initialized
	// get predictable and sensible values.
	memset( m_pConstantMemory, 0, nConstantMemorySize );

	m_nPerParticleInitializedAttributeMask = pDef->m_nPerParticleInitializedAttributeMask;
	m_nPerParticleUpdatedAttributeMask = pDef->m_nPerParticleUpdatedAttributeMask;

	// Only worry about initial attributes that are per-particle *and* are updated at a later time
	// If they aren't updated at a later time, then we can just point the initial + current pointers at the same memory
	m_nPerParticleReadInitialAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & pDef->m_nPerParticleUpdatedAttributeMask );

	// This is the mask of attributes which are initialized per-particle, but never updated
	// *and* where operators want to read initial particle state
	int nPerParticleReadConstantAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & ( ~pDef->m_nPerParticleUpdatedAttributeMask ) );

	int sz = 0;
	int nInitialAttributeSize = 0;
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;
		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			sz += nAttrSize;
		}
		if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
		{
			nInitialAttributeSize += nAttrSize;
		}
	}

	// Gotta allocate a couple extra floats to account for 
	int nAllocationSize = m_nAllocatedParticles * sz * sizeof(float) + 16;
	m_pParticleMemory = new unsigned char[ nAllocationSize ];
	memset( m_pParticleMemory, 0, nAllocationSize );

	// Allocate space for the initial attributes
	if ( nInitialAttributeSize != 0 )
	{
		int nInitialAllocationSize = m_nAllocatedParticles * nInitialAttributeSize * sizeof(float) + 16;
		m_pParticleInitialMemory = new unsigned char[ nInitialAllocationSize ];
		memset( m_pParticleInitialMemory, 0, nInitialAllocationSize );
	}

	// Align allocation to 16-byte boundaries
	float *pMem = (float*)( (size_t)( m_pParticleMemory + 15 ) & ~0xF );
	float *pInitialMem = (float*)( (size_t)( m_pParticleInitialMemory + 15 ) & ~0xF );

	// Point each attribute to memory associated with that attribute
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;

		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			m_pParticleAttributes[ bit ] = pMem;
			m_nParticleFloatStrides[ bit ] = nAttrSize * 4;
			pMem += nAttrSize * m_nAllocatedParticles;
		}
		else
		{
			m_pParticleAttributes[ bit ] = GetConstantAttributeMemory( bit );
			m_nParticleFloatStrides[ bit ] = 0;
		}

		// Are we reading
		if ( pDef->m_nInitialAttributeReadMask & ( 1 << bit ) )
		{
			if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
			{
				Assert( pInitialMem );
				m_pParticleInitialAttributes[ bit ] = pInitialMem;
				m_nParticleInitialFloatStrides[ bit ] = nAttrSize * 4;
				pInitialMem += nAttrSize * m_nAllocatedParticles;
			}
			else if ( nPerParticleReadConstantAttributeMask & ( 1 << bit ) )
			{
				m_pParticleInitialAttributes[ bit ] = m_pParticleAttributes[ bit ];
				m_nParticleInitialFloatStrides[ bit ] = m_nParticleFloatStrides[ bit ];
			}
			else
			{
				m_pParticleInitialAttributes[ bit ] = GetConstantAttributeMemory( bit );
				m_nParticleInitialFloatStrides[ bit ] = 0;
			}
		}
		else
		{
			// Catch errors where code is reading data it didn't request
			m_pParticleInitialAttributes[ bit ] = NULL;
			m_nParticleInitialFloatStrides[ bit ] = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the particle collection name
//-----------------------------------------------------------------------------
const char *CParticleCollection::GetName() const
{
	return m_pDef ? m_pDef->GetName() : "";
}


//-----------------------------------------------------------------------------
// Does the particle system use the frame buffer texture (refraction?)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesPowerOfTwoFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesPowerOfTwoFrameBufferTexture )			// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesPowerOfTwoFrameBufferTexture );
	}
	return true;
}
//-----------------------------------------------------------------------------
// Does the particle system use the full frame buffer texture (soft particles)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesFullFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesFullFrameBufferTexture )				// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesFullFrameBufferTexture );
	}
	return true;
}

bool CParticleCollection::SystemContainsParticlesWithBoolSet( bool CParticleCollection::*pField ) const
{
	if ( m_nActiveParticles && ( this->*pField ) )
		return true;
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->SystemContainsParticlesWithBoolSet( pField ) )
			return true;
	}
	return false;

}

void CParticleCollection::LabelTextureUsage( void )
{
	if ( m_pDef )
	{
		m_bUsesPowerOfTwoFrameBufferTexture = m_pDef->UsesPowerOfTwoFrameBufferTexture();
		m_bUsesFullFrameBufferTexture = m_pDef->UsesFullFrameBufferTexture();
	}

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->LabelTextureUsage();
	}
}

bool CParticleCollection::ComputeUsesPowerOfTwoFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesPowerOfTwoFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->UsesPowerOfTwoFrameBufferTexture( false ) )
			return true;
	}

	return false;
}

bool CParticleCollection::ComputeUsesFullFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesFullFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->UsesFullFrameBufferTexture( false ) )
			return true;
	}

	return false;
}
//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::ContainsOpaqueCollections()
{
	if ( !m_pDef )
		return false;

	if ( !m_pDef->GetMaterial()->IsTranslucent() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->ContainsOpaqueCollections( ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTwoPass() const
{
	return m_bIsTwoPass;
}

bool CParticleCollection::ComputeIsTwoPass()
{
	if ( !ComputeIsTranslucent() )
		return false;

	return ContainsOpaqueCollections();
}


//-----------------------------------------------------------------------------
// Is the particle system translucent
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTranslucent() const
{
	return m_bIsTranslucent;
}

bool CParticleCollection::ComputeIsTranslucent()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->GetMaterial()->IsTranslucent() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->IsTranslucent( ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Is the particle system batchable
//-----------------------------------------------------------------------------
bool CParticleCollection::IsBatchable() const
{
	return m_bIsBatchable;
}

bool CParticleCollection::ComputeIsBatchable()
{
	int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( !GetRenderer( i )->IsBatchable() )
			return false;
	}

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( !p->IsBatchable() )
			return false;
	}

	return true;
}
//-----------------------------------------------------------------------------
// Does this system require order invariance of the particles?
//-----------------------------------------------------------------------------
bool CParticleCollection::ComputeRequiresOrderInvariance()
{
	const int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( GetRenderer( i )->RequiresOrderInvariance() )
			return true;
	}

	for (CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext)
	{
		if ( p->m_bRequiresOrderInvariance )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Renderer iteration
//-----------------------------------------------------------------------------
int CParticleCollection::GetRendererCount() const
{
	return IsValid() ? m_pDef->m_Renderers.Count() : 0;
}

CParticleOperatorInstance *CParticleCollection::GetRenderer( int i )
{
	return IsValid() ? m_pDef->m_Renderers[i] : NULL;
}

void *CParticleCollection::GetRendererContext( int i )
{
	return IsValid() ? m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] : NULL;
}


//-----------------------------------------------------------------------------
// Visualize operators (for editing/debugging)
//-----------------------------------------------------------------------------
void CParticleCollection::VisualizeOperator( const DmObjectId_t *pOpId )
{
	m_pRenderOp = NULL;
	if ( !pOpId || !m_pDef )
		return;

	m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_EMITTER, *pOpId );
	if ( !m_pRenderOp )
	{
		m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_INITIALIZER, *pOpId );
		if ( !m_pRenderOp )
		{
			m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_OPERATOR, *pOpId );
		}
	}
}


float FadeInOut( float flFadeInStart, float flFadeInEnd, float flFadeOutStart, float flFadeOutEnd, float flCurTime )
{
	if ( flFadeInStart > flCurTime )						// started yet?
		return 0.0;

	if ( ( flFadeOutEnd > 0. ) && ( flFadeOutEnd < flCurTime ) ) // timed out?
		return 0.;

	// handle out of order cases
	flFadeInEnd = max( flFadeInEnd, flFadeInStart );
	flFadeOutStart = max( flFadeOutStart, flFadeInEnd );
	flFadeOutEnd = max( flFadeOutEnd, flFadeOutStart );

	float flStrength = 1.0;
	if (
		( flFadeInEnd > flCurTime ) &&
		( flFadeInEnd > flFadeInStart ) )
		flStrength = min( flStrength, FLerp( 0, 1, flFadeInStart, flFadeInEnd, flCurTime ) );

	if ( ( flCurTime > flFadeOutStart) &&
		 ( flFadeOutEnd > flFadeOutStart) )
		flStrength = min ( flStrength, FLerp( 0, 1, flFadeOutEnd, flFadeOutStart, flCurTime ) );

	return flStrength;

}

bool CParticleCollection::CheckIfOperatorShouldRun( 
	CParticleOperatorInstance const * pOp ,
	float *pflCurStrength)
{
	float flTime=m_flCurTime;
	if ( pOp->m_flOpFadeOscillatePeriod > 0.0 )
	{
		flTime=fmod( m_flCurTime*( 1.0/pOp->m_flOpFadeOscillatePeriod ), 1.0 );
	}

	float flStrength = FadeInOut( pOp->m_flOpStartFadeInTime, pOp->m_flOpEndFadeInTime,
								  pOp->m_flOpStartFadeOutTime, pOp->m_flOpEndFadeOutTime,
								  flTime );
	if ( pflCurStrength )
		*pflCurStrength = flStrength;
	return ( flStrength > 0.0 );
}


//-----------------------------------------------------------------------------
// Restarts a particle system
//-----------------------------------------------------------------------------
void CParticleCollection::Restart()
{
	int i;
	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( i = 0; i < nEmitterCount; i++ )
	{
		m_pDef->m_Emitters[i]->Restart( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
	}

	// Update all children
	CParticleCollection *pChild;
	for( i = 0, pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext, i++ )
	{
		// Remove any delays from the time (otherwise we're offset by it oddly)
		pChild->Restart( );
	}
}


//-----------------------------------------------------------------------------
// Main entry point for rendering
//-----------------------------------------------------------------------------
void CParticleCollection::Render( IMatRenderContext *pRenderContext, bool bTranslucentOnly, void *pCameraObject )
{
	if ( !IsValid() )
		return;

	m_flNextSleepTime = Max ( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep ));

	if ( m_nActiveParticles != 0 )
	{
		if ( !bTranslucentOnly || m_pDef->GetMaterial()->IsTranslucent() )
		{
			int nCount = m_pDef->m_Renderers.Count();
			for( int i = 0; i < nCount; i++ )
			{
				if ( CheckIfOperatorShouldRun( m_pDef->m_Renderers[i] ) )
				{
// 					pRenderContext->MatrixMode( MATERIAL_VIEW );
// 					pRenderContext->PushMatrix();
// 					pRenderContext->LoadIdentity();
// 					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
// 					pRenderContext->PushMatrix();
// 					pRenderContext->LoadIdentity();
// 					pRenderContext->Ortho( -100, -100, 100, 100, -100, 100 );
					m_pDef->m_Renderers[i]->Render(
						pRenderContext, this, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] );
// 					pRenderContext->MatrixMode( MATERIAL_VIEW );
// 					pRenderContext->PopMatrix();
// 					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
// 					pRenderContext->PopMatrix();
				}
			}
		}
	}
	
	// let children render
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->Render( pRenderContext, bTranslucentOnly, pCameraObject );
	}

	// Visualize specific ops for debugging/editing
	if ( m_pRenderOp )
	{
		m_pRenderOp->Render( this );
	}
}

void CParticleCollection::UpdatePrevControlPoints( float dt )
{
	m_flPreviousDt = dt;
	for(int i=0; i <= m_nHighestCP; i++ )
		m_ControlPoints[i].m_PrevPosition = m_ControlPoints[i].m_Position;
	m_nParticleFlags |= PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED;
}

#if MEASURE_PARTICLE_PERF

#if VPROF_LEVEL > 0
#define START_OP float flOpStartTime = Plat_FloatTime(); VPROF_ENTER_SCOPE(pOp->GetDefinition()->GetName())
#else
#define START_OP float flOpStartTime = Plat_FloatTime();
#endif

#if VPROF_LEVEL > 0
#define END_OP  if ( 1 ) {																						\
	float flETime = Plat_FloatTime() - flOpStartTime;									\
	IParticleOperatorDefinition *pDef = (IParticleOperatorDefinition *) pOp->m_pDef;	\
	pDef->RecordExecutionTime( flETime );												\
} \
	VPROF_EXIT_SCOPE()
#else
#define END_OP  if ( 1 ) {																						\
	float flETime = Plat_FloatTime() - flOpStartTime;									\
	IParticleOperatorDefinition *pDef = (IParticleOperatorDefinition *) pOp->m_pDef;	\
	pDef->RecordExecutionTime( flETime );												\
}
#endif
#else
#define START_OP
#define END_OP
#endif

void CParticleCollection::InitializeNewParticles( int nFirstParticle, int nParticleCount, uint32 nInittedMask )
{
	VPROF_BUDGET( "CParticleCollection::InitializeNewParticles", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );

#ifdef _DEBUG
	m_bIsRunningInitializers = true;
#endif

	// now, initialize the attributes of all the new particles
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	int nAttrsLeftToInit = nPerParticleAttributeMask & ~nInittedMask;
	int nInitializerCount = m_pDef->m_Initializers.Count();
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i];
		int nInitializerAttrMask = pOp->GetWrittenAttributes();
		if ( ( ( nInitializerAttrMask & nAttrsLeftToInit ) == 0 ) || pOp->InitMultipleOverride() )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

	// always run second tier initializers (modifiers) after first tier - this ensures they don't get stomped.
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		int nInitializerAttrMask = m_pDef->m_Initializers[i]->GetWrittenAttributes();
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i];
		if ( !pOp->InitMultipleOverride() )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

#ifdef _DEBUG
	m_bIsRunningInitializers = false;
#endif

	InitParticleAttributes( nFirstParticle, nParticleCount, nAttrsLeftToInit );

	CopyInitialAttributeValues( nFirstParticle, nParticleCount );
}

void CParticleCollection::SkipToTime( float t )
{
	if ( t > m_flCurTime )
	{
		UpdatePrevControlPoints( t - m_flCurTime );
		m_flCurTime = t;	
		m_fl4CurTime = ReplicateX4( t );
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
		
		// FIXME: In future, we may have to tell operators, initializers about this too
		int nEmitterCount = m_pDef->m_Emitters.Count();
		int i;
		for( i = 0; i < nEmitterCount; i++ )
		{
			m_pDef->m_Emitters[i]->SkipToTime( t, this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
		}

		CParticleCollection *pChild;

		// Update all children
		for( i = 0, pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext, i++ )
		{
			// Remove any delays from the time (otherwise we're offset by it oddly)
			pChild->SkipToTime( t - m_pDef->m_Children[i].m_flDelay );
		}
	}
}

#ifdef NDEBUG
#define CHECKSYSTEM( p ) 0
#else
static void CHECKSYSTEM( CParticleCollection *pParticles )
{
//	Assert( pParticles->m_nActiveParticles <= pParticles->m_pDef->m_nMaxParticles );
	for ( int i = 0; i < pParticles->m_nActiveParticles; ++i )
	{
		const float *xyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
		const float *xyz_prev = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, i );
/*		
		const float *rad = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i );
		Assert( IsFinite( rad[0] ) );

		RJ: Disabling this assert.  While the proper way is to fix the math which leads to the bad number, the fix would result in more particles being drawn and in a post shipping world, users were not happy.
		See Changelists #1368648, #1368635, and #1368434 for proper math calculation fixes.

		In a post shipping world, as these particles would not render with infinites, code was added C_OP_RenderSprites::RenderSpriteCard() to check for the infinite and not add the vert to meshbuilder.
*/
		Assert( IsFinite( xyz[0] ) );
		Assert( IsFinite( xyz[4] ) );
		Assert( IsFinite( xyz[8] ) );
		Assert( IsFinite( xyz_prev[0] ) );
		Assert( IsFinite( xyz_prev[4] ) );
		Assert( IsFinite( xyz_prev[8] ) );
	}
}
#endif

void CParticleCollection::SimulateFirstFrame( )
{
	m_flDt = 0.0f;
	m_nDrawnFrames = 0;
	m_nSimulatedFrames = 1;

	// For the first frame, copy over the initial control points
	if ( ( m_nParticleFlags & PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED ) == 0 )
	{
		UpdatePrevControlPoints( 0.05f );
	}

	m_nOperatorRandomSampleOffset = 0;
	int nCount = m_pDef->m_Operators.Count();
	for( int i = 0; i < nCount; i++ )
	{
		float flStrength;
		CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
		if ( pOp->ShouldRunBeforeEmitters() &&
			 CheckIfOperatorShouldRun( pOp, &flStrength ) )
		{
			pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
			CHECKSYSTEM( this );
			UpdatePrevControlPoints( 0.05f );
		}
		m_nOperatorRandomSampleOffset += 17;
	}
	
	// first, create initial particles
	int nNumToCreate = min( m_pDef->m_nInitialParticles, m_nMaxAllowedParticles );
	if ( nNumToCreate > 0 )
	{
		SetNActiveParticles( nNumToCreate );
		InitializeNewParticles( 0, nNumToCreate, 0 );
		CHECKSYSTEM( this );
	}
}



void CParticleCollection::Simulate( float dt, bool updateBboxOnly )
{
	VPROF_BUDGET( "CParticleCollection::Simulate", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );
	if ( dt < 0.0f )
		return;

	if ( !m_pDef )
		return;

	// Don't do anything until we've hit t == 0
	// This is used for delayed children
	if ( m_flCurTime < 0.0f )
	{
		if ( dt >= 1.0e-22 )
		{
			m_flCurTime += dt;
			m_fl4CurTime = ReplicateX4( m_flCurTime );
			UpdatePrevControlPoints( dt );
		}
		return;
	}

	// run initializers if necessary (once we hit t == 0)
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
	{
		SimulateFirstFrame();
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
	}

	if ( dt < 1.0e-22 )
		return;


#if MEASURE_PARTICLE_PERF
	float flStartSimTime = Plat_FloatTime();
#endif

	bool bAttachedKillList = false;

	if (!HasAttachedKillList())
	{
		g_pParticleSystemMgr->AttachKillList(this);
		bAttachedKillList = true;
	}

	if (!updateBboxOnly)
	{
		++m_nSimulatedFrames;

		float flRemainingDt = dt;
		float flMaxDT = 0.1;									// default
		if ( m_pDef->m_flMaximumTimeStep > 0.0 )
			flMaxDT = m_pDef->m_flMaximumTimeStep;

		// Limit timestep if needed (prevents short lived particles from being created and destroyed before being rendered.
		//if ( m_pDef->m_flMaximumSimTime != 0.0 && !m_bHasDrawnOnce )
		if ( m_pDef->m_flMaximumSimTime != 0.0 && ( m_nDrawnFrames <= m_pDef->m_nMinimumFrames ) )
		{
			if ( ( flRemainingDt + m_flCurTime ) > m_pDef->m_flMaximumSimTime )
			{
				//if delta+current > checkpoint then delta = checkpoint - current
				flRemainingDt = m_pDef->m_flMaximumSimTime - m_flCurTime;
				flRemainingDt = max( m_pDef->m_flMinimumSimTime, flRemainingDt );
			}
			m_nDrawnFrames += 1;
		}

		flRemainingDt = min( flRemainingDt, 10 * flMaxDT );	// no more than 10 passes ever

		while( flRemainingDt > 0.0 )
		{
			float flDT_ThisStep = min( flRemainingDt, flMaxDT );
			flRemainingDt -= flDT_ThisStep;
			m_flDt = flDT_ThisStep;
			m_flCurTime += flDT_ThisStep;
			m_fl4CurTime = ReplicateX4( m_flCurTime );
		
#ifdef _DEBUG
			m_bIsRunningOperators = true;
#endif
		
			m_nOperatorRandomSampleOffset = 0;
			int nCount = m_pDef->m_Operators.Count();
			for( int i = 0; i < nCount; i++ )
			{
				float flStrength;
				CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
				if ( pOp->ShouldRunBeforeEmitters() &&
					 CheckIfOperatorShouldRun( pOp, &flStrength ) )
				{
					START_OP;
					pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
					END_OP;
					CHECKSYSTEM( this );
					if ( m_nNumParticlesToKill )
					{
						ApplyKillList();
					}
					m_nOperatorRandomSampleOffset += 17;
				}
			}
#ifdef _DEBUG
			m_bIsRunningOperators = false;
#endif


			int nEmitterCount = m_pDef->m_Emitters.Count();
			for( int i=0; i < nEmitterCount; i++ )
			{
				int nOldParticleCount = m_nActiveParticles;
				float flEmitStrength;
				if ( CheckIfOperatorShouldRun( m_pDef->m_Emitters[i], &flEmitStrength ) )
				{
					uint32 nInittedMask = m_pDef->m_Emitters[i]->Emit( 
						this, flEmitStrength, 
						m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
					if ( nOldParticleCount != m_nActiveParticles )
					{
						// init newly emitted particles
						InitializeNewParticles( nOldParticleCount, m_nActiveParticles - nOldParticleCount, nInittedMask );
						CHECKSYSTEM( this );
					}
				}
			}
		
			m_nOperatorRandomSampleOffset = 0;
			nCount = m_pDef->m_Operators.Count();
			if ( m_nActiveParticles )
			{
#ifdef FP_EXCEPTIONS_ENABLED
				const int processedParticles = m_nPaddedActiveParticles * 4;
				for ( int unusedParticle = m_nActiveParticles; unusedParticle < processedParticles; ++unusedParticle )
				{
					// Set the unused-but-processed particle lifetimes to a value that
					// won't cause division by zero or other madness. This allows us
					// to enable floating-point exceptions during particle processing,
					// which helps us to find bugs.
					float *dtime = GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, unusedParticle );
					*dtime = 1.0f;
				}
#endif
#ifdef _DEBUG
				m_bIsRunningOperators = true;
#endif
				for( int i = 0; i < nCount; i++ )
				{
					float flStrength;
					CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
					if ( (!  pOp->ShouldRunBeforeEmitters() ) &&
						 CheckIfOperatorShouldRun( pOp, &flStrength ) )
					{
						START_OP;
						pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
						END_OP;
						CHECKSYSTEM( this );
						if ( m_nNumParticlesToKill )
						{
							ApplyKillList();
							if ( ! m_nActiveParticles )
								break;								// don't run any more operators
						}
						m_nOperatorRandomSampleOffset += 17;
					}
				}
#ifdef _DEBUG
				m_bIsRunningOperators = false;
#endif
			}
		}

#if MEASURE_PARTICLE_PERF
		m_pDef->m_nMaximumActiveParticles = max( m_pDef->m_nMaximumActiveParticles, m_nActiveParticles );
		float flETime = Plat_FloatTime() - flStartSimTime;
		m_pDef->m_flUncomittedTotalSimTime += flETime;
		m_pDef->m_flMaxMeasuredSimTime = max( m_pDef->m_flMaxMeasuredSimTime, flETime );
#endif
	}

	// let children simulate
	for (CParticleCollection *i = m_Children.m_pHead; i; i = i->m_pNext)
	{
		LoanKillListTo(i);								// re-use the allocated kill list for the children
		i->Simulate(dt, updateBboxOnly);
		i->m_pParticleKillList = NULL;
	}

	if (bAttachedKillList)
		g_pParticleSystemMgr->DetachKillList(this);

	UpdatePrevControlPoints(dt);

	// Bloat the bounding box by bounds around the control point
	BloatBoundsUsingControlPoint();

}


//-----------------------------------------------------------------------------
// Copies the constant attributes into the per-particle attributes
//-----------------------------------------------------------------------------
void CParticleCollection::InitParticleAttributes( int nStartParticle, int nNumParticles, int nAttrsLeftToInit )
{
	if ( nAttrsLeftToInit == 0 )
		return;

	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( ( nAttrsLeftToInit & ( 1 << nAttr ) ) == 0 )
				continue;

			float *pAttrData = GetFloatAttributePtrForWrite( nAttr, i );

			// Special case for particle id
			if ( nAttr == PARTICLE_ATTRIBUTE_PARTICLE_ID )
			{
				*( (int*)pAttrData ) = ( m_nRandomSeed + m_nUniqueParticleId ) & RANDOM_FLOAT_MASK;
				m_nUniqueParticleId++;
				continue;
			}

			// Special case for the creation time mask
			if ( nAttr == PARTICLE_ATTRIBUTE_CREATION_TIME )
			{
				*pAttrData = m_flCurTime;
				continue;
			}

			// If this assertion fails, it means we're writing into constant memory, which is a nono
			Assert( m_nParticleFloatStrides[nAttr] != 0 );
			float *pConstantAttr = GetConstantAttributeMemory( nAttr );
			*pAttrData = *pConstantAttr;
			if ( m_nParticleFloatStrides[nAttr] == 12 )
			{
				pAttrData[4] = pConstantAttr[4];
				pAttrData[8] = pConstantAttr[8];
			}
		}
	}
}

void CParticleCollection::CopyInitialAttributeValues( int nStartParticle, int nNumParticles )
{
	if ( m_nPerParticleReadInitialAttributeMask == 0 )
		return;

	// FIXME: Do SSE copy here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( m_nPerParticleReadInitialAttributeMask & (1 << nAttr) )
			{
				const float *pSrcAttribute = GetFloatAttributePtr( nAttr, i );
				float *pDestAttribute = GetInitialFloatAttributePtrForWrite( nAttr, i );
				Assert( m_nParticleInitialFloatStrides[nAttr] != 0 );
				Assert( m_nParticleFloatStrides[nAttr] == m_nParticleInitialFloatStrides[nAttr] );
				*pDestAttribute = *pSrcAttribute;
				if ( m_nParticleFloatStrides[nAttr] == 12 )
				{
					pDestAttribute[4] = pSrcAttribute[4];
					pDestAttribute[8] = pSrcAttribute[8];
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------e
// Computes a random vector inside a sphere
//-----------------------------------------------------------------------------
float CParticleCollection::RandomVectorInUnitSphere( int nRandomSampleId, Vector *pVector )
{
	// Guarantee uniform random distribution within a sphere
	// Graphics gems III contains this algorithm ("Nonuniform random point sets via warping")
	float u = RandomFloat( nRandomSampleId, 0.0001f, 1.0f );
	float v = RandomFloat( nRandomSampleId+1, 0.0001f, 1.0f );
	float w = RandomFloat( nRandomSampleId+2, 0.0001f, 1.0f );

	float flPhi = acos( 1 - 2 * u );
	float flTheta = 2 * M_PI * v;
	float flRadius = powf( w, 1.0f / 3.0f );

	float flSinPhi, flCosPhi;
	float flSinTheta, flCosTheta;
	SinCos( flPhi, &flSinPhi, &flCosPhi );
	SinCos( flTheta, &flSinTheta, &flCosTheta );

	pVector->x = flRadius * flSinPhi * flCosTheta;
	pVector->y = flRadius * flSinPhi * flSinTheta;
	pVector->z = flRadius * flCosPhi;
	return flRadius;
}



//-----------------------------------------------------------------------------
// Used to retrieve the position of a control point
// somewhere between m_flCurTime and m_flCurTime - m_fPreviousDT
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtTime( int nControlPoint, float flTime, Vector *pControlPoint ) const
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	if ( nControlPoint > GetHighestControlPoint() )
	{
		DevWarning(2, "Warning : Particle system (%s) using unassigned ControlPoint %d!\n", GetName(), nControlPoint );
	}
	if ( m_flDt == 0.0f )
	{
		VectorCopy( m_ControlPoints[nControlPoint].m_Position, *pControlPoint );
		return;
	}

	// The original calculation for 't' was this:
	//     float flPrevTime = m_flCurTime - m_flDt;
	//     float t = ( flTime - flPrevTime ) / m_flDt;
	// Which is mathematically equivalent to this:
	//     float t = ( flTime - ( m_flCurTime - m_flDt ) ) / m_flDt;
	// However if m_flCurTime and flTime are large then significant precision
	// is lost during subtraction -- catastrophic cancellation
	// is the technical term. This starts out being an error of one part in
	// ten million, but after running for just a few minutes it increases to
	// one part in ten thousand -- and continues to get worse.
	// This calculation even fails in the simple case where flTime == m_flCurTime,
	// giving an answer that is not 1.0 and may be out of range.

	// If the calculation is arranged as shown below then, because flTime and
	// m_flCurTime are close to each other, the subtraction loses *no* precision.
	// The subtraction will not necessarily be 'correct', since eventually flTime
	// and m_flCurTime will not have enough precision, but it will give results
	// that are as accurate as possible given the inputs.
	// flHowLongAgo stores how far before the current time flTime is.
	const float flHowLongAgo = m_flCurTime - flTime;
	float t = ( m_flDt - flHowLongAgo ) / m_flDt;
	// The original code had a comment saying:
	//     Precision errors can cause this problem
	// in regards to issues that can cause t to go negative. Actually this function
	// is just sometimes called (from InitNewParticlesScalar) with values that cause
	// 't' to go massively negative. So I clamp it.
	if ( t < 0.0f )
		t = 0.0f;
	Assert( t <= 1.0f );

	VectorLerp( m_ControlPoints[nControlPoint].m_PrevPosition, m_ControlPoints[nControlPoint].m_Position, t, *pControlPoint );
	Assert( IsFinite(pControlPoint->x) && IsFinite(pControlPoint->y) && IsFinite(pControlPoint->z) );
}

//-----------------------------------------------------------------------------
// Used to retrieve the previous position of a control point
// 
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtPrevTime( int nControlPoint, Vector *pControlPoint ) const
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	*pControlPoint = m_ControlPoints[nControlPoint].m_PrevPosition;
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, matrix3x4_t *pMat )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	const Vector &vecControlPoint = GetControlPointAtCurrentTime( nControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( m_ControlPoints[nControlPoint].m_RightVector, -1.0f, left );
	pMat->Init( m_ControlPoints[nControlPoint].m_ForwardVector, left, m_ControlPoints[nControlPoint].m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, VMatrix *pMat )
{
	GetControlPointTransformAtCurrentTime( nControlPoint, const_cast<matrix3x4_t *> ( &pMat->As3x4() ) );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointOrientationAtTime( int nControlPoint, float flTime, Vector *pForward, Vector *pRight, Vector *pUp )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );

	// FIXME: Use quaternion lerp to get control point transform at time
	*pForward = m_ControlPoints[nControlPoint].m_ForwardVector;
	*pRight = m_ControlPoints[nControlPoint].m_RightVector;
	*pUp = m_ControlPoints[nControlPoint].m_UpVector;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, matrix3x4_t *pMat )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( m_ControlPoints[nControlPoint].m_RightVector, -1.0f, left );
	pMat->Init( m_ControlPoints[nControlPoint].m_ForwardVector, left, m_ControlPoints[nControlPoint].m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, VMatrix *pMat )
{
	GetControlPointTransformAtTime( nControlPoint, flTime, const_cast< matrix3x4_t * > ( &pMat->As3x4() ) );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, CParticleSIMDTransformation *pXForm )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	pXForm->m_v4Fwd.DuplicateVector( m_ControlPoints[nControlPoint].m_ForwardVector );
	pXForm->m_v4Up.DuplicateVector( m_ControlPoints[nControlPoint].m_UpVector );
	//Vector left;
	//VectorMultiply( m_ControlPoints[nControlPoint].m_RightVector, -1.0f, left );
	pXForm->m_v4Right.DuplicateVector( m_ControlPoints[nControlPoint].m_RightVector );

}

int CParticleCollection::GetHighestControlPoint( void ) const
{
	return m_nHighestCP;
}

//-----------------------------------------------------------------------------
// Returns the render bounds
//-----------------------------------------------------------------------------
void CParticleCollection::GetBounds( Vector *pMin, Vector *pMax )
{
	*pMin = m_MinBounds;
	*pMax = m_MaxBounds;
}

//-----------------------------------------------------------------------------
// Bloat the bounding box by bounds around the control point
//-----------------------------------------------------------------------------
void CParticleCollection::BloatBoundsUsingControlPoint()
{
	// more specifically, some particle systems were using "start" as an input, so it got set as control point 1,
	// so other particle systems had an extra point in their bounding box, that generally remained at the world origin
	RecomputeBounds();

	// Don't do the bounding box fixup until after the second simulation (first real simulation)
	// so that we know they're in their correct position.
	if ( m_nSimulatedFrames > 2 )
	{
		// Include control points in the bbox. 
		for (int i = 0; i <= m_nHighestCP; ++i) {
			if ( !m_pDef->ReadsControlPoint( i ) )
				continue;

			const Vector& cp = GetControlPointAtCurrentTime(i);
			VectorMin( m_MinBounds, cp, m_MinBounds );
			VectorMax( m_MaxBounds, cp, m_MaxBounds );
		}
	}

	// Deal with children
	// NOTE: Bounds have been recomputed for children prior to this call in Simulate
	Vector vecMins, vecMaxs;
	for( CParticleCollection *i = m_Children.m_pHead; i; i = i->m_pNext )
	{
		i->GetBounds( &vecMins, &vecMaxs );
		VectorMin( m_MinBounds, vecMins, m_MinBounds );
		VectorMax( m_MaxBounds, vecMaxs, m_MaxBounds );
	}
}


//-----------------------------------------------------------------------------
// Recomputes the bounds
//-----------------------------------------------------------------------------
void CParticleCollection::RecomputeBounds( void )
{
	if ( m_nActiveParticles == 0.0f )
	{
		m_bBoundsValid = false;
		m_MinBounds.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		m_MaxBounds.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		m_Center.Init();
		return;
	}

	fltx4 min_x = ReplicateX4(1.0e23);
	fltx4 min_y = min_x;
	fltx4 min_z = min_x;
	fltx4 max_x = ReplicateX4(-1.0e23);
	fltx4 max_y = max_x;
	fltx4 max_z = max_x;

	fltx4 sum_x = Four_Zeros;
	fltx4 sum_y = Four_Zeros;
	fltx4 sum_z = Four_Zeros;

	size_t xyz_stride;
	const fltx4 *xyz = GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	int ctr = m_nActiveParticles/4;
	while ( ctr-- )
	{
		min_x = MinSIMD( min_x, xyz[0] );
		max_x = MaxSIMD( max_x, xyz[0] );
		sum_x = AddSIMD( sum_x, xyz[0] );

		min_y = MinSIMD( min_y, xyz[1] );
		max_y = MaxSIMD( max_y, xyz[1] );
		sum_y = AddSIMD( sum_y, xyz[1] );

		min_z = MinSIMD( min_z, xyz[2] );
		max_z = MaxSIMD( max_z, xyz[2] );
		sum_z = AddSIMD( sum_z, xyz[2] );

		xyz += xyz_stride;
	}
	m_bBoundsValid = true;
	m_MinBounds.x = min( min( SubFloat( min_x, 0 ), SubFloat( min_x, 1 ) ), min( SubFloat( min_x, 2 ), SubFloat( min_x, 3 ) ) );
	m_MinBounds.y = min( min( SubFloat( min_y, 0 ), SubFloat( min_y, 1 ) ), min( SubFloat( min_y, 2 ), SubFloat( min_y, 3 ) ) );
	m_MinBounds.z = min( min( SubFloat( min_z, 0 ), SubFloat( min_z, 1 ) ), min( SubFloat( min_z, 2 ), SubFloat( min_z, 3 ) ) );
							  
	m_MaxBounds.x = max( max( SubFloat( max_x, 0 ), SubFloat( max_x, 1 ) ), max( SubFloat( max_x, 2 ), SubFloat( max_x, 3 ) ) );
	m_MaxBounds.y = max( max( SubFloat( max_y, 0 ), SubFloat( max_y, 1 ) ), max( SubFloat( max_y, 2 ), SubFloat( max_y, 3 ) ) );
	m_MaxBounds.z = max( max( SubFloat( max_z, 0 ), SubFloat( max_z, 1 ) ), max( SubFloat( max_z, 2 ), SubFloat( max_z, 3 ) ) );

	float fsum_x = SubFloat( sum_x, 0 ) + SubFloat( sum_x, 1 ) + SubFloat( sum_x, 2 ) + SubFloat( sum_x, 3 );
	float fsum_y = SubFloat( sum_y, 0 ) + SubFloat( sum_y, 1 ) + SubFloat( sum_y, 2 ) + SubFloat( sum_y, 3 );
	float fsum_z = SubFloat( sum_z, 0 ) + SubFloat( sum_z, 1 ) + SubFloat( sum_z, 2 ) + SubFloat( sum_z, 3 );

	// now, handle "tail" in a non-sse manner
	for( int i=0; i < (m_nActiveParticles & 3); i++)
	{
		m_MinBounds.x = min( m_MinBounds.x, SubFloat( xyz[0], i ) );
		m_MaxBounds.x = max( m_MaxBounds.x, SubFloat( xyz[0], i ) );
		fsum_x += SubFloat( xyz[0], i );

		m_MinBounds.y = min( m_MinBounds.y, SubFloat( xyz[1], i ) );
		m_MaxBounds.y = max( m_MaxBounds.y, SubFloat( xyz[1], i ) );
		fsum_y += SubFloat( xyz[1], i );

		m_MinBounds.z = min( m_MinBounds.z, SubFloat( xyz[2], i ) );
		m_MaxBounds.z = max( m_MaxBounds.z, SubFloat( xyz[2], i ) );
		fsum_z += SubFloat( xyz[2], i );
	}

	VectorAdd( m_MinBounds, m_pDef->m_BoundingBoxMin, m_MinBounds );
	VectorAdd( m_MaxBounds, m_pDef->m_BoundingBoxMax, m_MaxBounds );

	// calculate center
	float flOONumParticles = 1.0 / m_nActiveParticles;
	m_Center.x = flOONumParticles * fsum_x;
	m_Center.y = flOONumParticles * fsum_y;
	m_Center.z = flOONumParticles * fsum_z;
}


//-----------------------------------------------------------------------------
// Is the particle system finished emitting + all its particles are dead?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsFinished( void )
{
	if ( !m_pDef )
		return true;
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
		return false;
	if ( m_nActiveParticles )
		return false;
	if ( m_bDormant ) 
		return false;

	// no particles. See if any emmitters intead to create more particles
	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( int i=0; i < nEmitterCount; i++ )
	{
		if ( m_pDef->m_Emitters[i]->MayCreateMoreParticles( this, m_pOperatorContextData+m_pDef->m_nEmittersCtxOffsets[i] ) )
			return false;
	}

	// make sure all children are finished
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		if ( !i->IsFinished() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StopEmission( bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop )
{
	if ( !m_pDef )
		return;

	// Whenever we call stop emission, we clear out our dormancy. This ensures we
	// get deleted if we're told to stop emission while dormant. SetDormant() ensures
	// dormancy is set to true after stopping out emission.
	m_bDormant = false;
	
	if ( bWakeOnStop )
	{
		// Set next sleep time - an additional fudge factor is added over the normal time
		// so that existing particles have a chance to go away.
		m_flNextSleepTime = Max ( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + 10 ));
	}
		 
	m_bEmissionStopped = true;

	for( int i=0; i < m_pDef->m_Emitters.Count(); i++ )
	{
		m_pDef->m_Emitters[i]->StopEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	if ( bRemoveAllParticles )
	{
		SetNActiveParticles( 0 );
	}

	// Stop our children as well
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->StopEmission( bInfiniteOnly, bRemoveAllParticles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StartEmission( bool bInfiniteOnly )
{
	if ( !m_pDef )
		return;

	m_bEmissionStopped = false;

	for( int i=0; i < m_pDef->m_Emitters.Count(); i++ )
	{
		m_pDef->m_Emitters[i]->StartEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	// Stop our children as well
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->StartEmission( bInfiniteOnly );
	}

	// Set our sleep time to some time in the future so we update again
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep;
}

//-----------------------------------------------------------------------------
// Purpose: Dormant particle systems simulate their particles, but don't emit 
//			new ones. Unlike particle systems that have StopEmission() called
//			dormant particle systems don't remove themselves when they're
//			out of particles, assuming they'll return at some point.
//-----------------------------------------------------------------------------
void CParticleCollection::SetDormant( bool bDormant )
{
	// Don't stop or start emission if we are not changing dormancy state
	if ( bDormant == m_bDormant )
		return;

	// If emissions have already been stopped, don't go dormant, we're supposed to be dying.
	if ( m_bEmissionStopped && bDormant )
		return;

	if ( bDormant )
	{
		StopEmission();
	}
	else
	{
		StartEmission();
	}

	m_bDormant = bDormant;
}


void CParticleCollection::MoveParticle( int nInitialIndex, int nNewIndex )
{
	// Copy the per-particle attributes
	for( int p = 0; p < MAX_PARTICLE_ATTRIBUTES; ++p )
	{
		switch( m_nParticleFloatStrides[ p ] )
		{
			case 4:										// move a float
				m_pParticleAttributes[p][nNewIndex] = m_pParticleAttributes[p][nInitialIndex];
				break;

			case 12:										// move a vec3
			{
				// sse weirdness
				int oldidxsse = 12 * ( nNewIndex >> 2 );
				int oldofs = oldidxsse + ( nNewIndex & 3 );
				int lastidxsse = 12 * ( nInitialIndex >> 2 );
				int lastofs = lastidxsse + ( nInitialIndex & 3 );
				
				m_pParticleAttributes[p][oldofs] = m_pParticleAttributes[p][lastofs];
				m_pParticleAttributes[p][4+oldofs] = m_pParticleAttributes[p][4+lastofs];
				m_pParticleAttributes[p][8+oldofs] = m_pParticleAttributes[p][8+lastofs];
				break;
			}
		}

		switch( m_nParticleInitialFloatStrides[ p ] )
		{
			case 4:										// move a float
				m_pParticleInitialAttributes[p][nNewIndex] = m_pParticleInitialAttributes[p][nInitialIndex];
				break;
				
			case 12:										// move a vec3
			{
				// sse weirdness
				int oldidxsse = 12 * ( nNewIndex>>2 );
				int oldofs = oldidxsse + ( nNewIndex & 3 );
				int lastidxsse = 12 * ( nInitialIndex >> 2 );
				int lastofs = lastidxsse + ( nInitialIndex & 3 );
				
				m_pParticleInitialAttributes[p][oldofs] = m_pParticleInitialAttributes[p][lastofs];
				m_pParticleInitialAttributes[p][4+oldofs] = m_pParticleInitialAttributes[p][4+lastofs];
				m_pParticleInitialAttributes[p][8+oldofs] = m_pParticleInitialAttributes[p][8+lastofs];
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Kill List processing.
//-----------------------------------------------------------------------------

#define THREADED_PARTICLES 1

#if THREADED_PARTICLES
#define MAX_SIMULTANEOUS_KILL_LISTS 16
static volatile int g_nKillBufferInUse[MAX_SIMULTANEOUS_KILL_LISTS];
static int32 *g_pKillBuffers[MAX_SIMULTANEOUS_KILL_LISTS];

void CParticleSystemMgr::DetachKillList( CParticleCollection *pParticles )
{
	if ( pParticles->m_pParticleKillList )
	{
		// find which it is
		for(int i=0; i < NELEMS( g_pKillBuffers ); i++)
		{
			if ( g_pKillBuffers[i] == pParticles->m_pParticleKillList )
			{
				pParticles->m_pParticleKillList = NULL;
				g_nKillBufferInUse[i] = 0;					// no need to interlock
				return;
			}
		}
		Assert( 0 );										// how did we get here?
	}
}

void CParticleSystemMgr::AttachKillList( CParticleCollection *pParticles )
{
	// look for a free slot
	for(;;)
	{
		for(int i=0; i < NELEMS( g_nKillBufferInUse ); i++)
		{
			if ( ! g_nKillBufferInUse[i] )					// available?
			{
				// try to take it!
				if ( ThreadInterlockedAssignIf( &( g_nKillBufferInUse[i]), 1, 0 ) )
				{
					if ( ! g_pKillBuffers[i] )
					{
						g_pKillBuffers[i] = new int32[MAX_PARTICLES_IN_A_SYSTEM];
					}
					pParticles->m_pParticleKillList = g_pKillBuffers[i];
					return;									// done!
				}

			}
		}
		Assert(0);											// why don't we have enough buffers?
		ThreadSleep();
	}
}
#else
// use one static kill list. no worries because of not threading
static int g_nParticleKillList[MAX_PARTICLES_IN_A_SYSTEM];
void CParticleSystemMgr::AttachKillList( CParticleCollection *pParticles )
{
	pParticles->m_pParticleKillList = g_nParticleKillList;
}
void CParticleCollection::DetachKillList( CParticleCollection *pParticles )
{
	Assert( pParticles->m_nNumParticlesToKill == 0 );
	pParticles->m_pParticleKillList = NULL;
}
#endif


void CParticleCollection::ApplyKillList( void )
{
	int nLeftInKillList = m_nNumParticlesToKill;
	if ( nLeftInKillList == 0 )
		return;

	int nParticlesActiveNow = m_nActiveParticles;
	const int *pCurKillListSlot = m_pParticleKillList;

#ifdef _DEBUG
	// This algorithm assumes the particles listed in the kill list are in ascending order
	for ( int i = 1; i < nLeftInKillList; ++i )
	{
		Assert( pCurKillListSlot[i] > pCurKillListSlot[i-1] );
	}
#endif

	// first, kill particles past bounds
	while ( nLeftInKillList && pCurKillListSlot[nLeftInKillList - 1] >= nParticlesActiveNow )
	{
		nLeftInKillList--;
	}

	Assert( nLeftInKillList <= m_nActiveParticles );

	// now, execute kill list
	// Previously, this code would swap the last item that wasn't dead into this slot. 
	// However, some lists require order invariance, so we need to collapse over holes instead of
	// doing the (cheaper) swap from the end to the hole. 
	if ( !m_bRequiresOrderInvariance )
	{
		while( nLeftInKillList )
		{
			int nKillIndex = *(pCurKillListSlot++);
			nLeftInKillList--;

			// now, we will move a particle from the end to where we are
			// first, we have to find the last particle (which is not in the kill list)
			while ( nLeftInKillList &&
				( pCurKillListSlot[ nLeftInKillList-1 ] == nParticlesActiveNow-1 ))
			{
				nLeftInKillList--;
				nParticlesActiveNow--;
			}

			// we might be killing the last particle
			if ( nKillIndex == nParticlesActiveNow-1 )
			{
				// killing last one
				nParticlesActiveNow--;
				break;											// we are done
			}

			// move the last particle to this one and chop off the end of the list
			MoveParticle( nParticlesActiveNow-1, nKillIndex );
			nParticlesActiveNow--;
		}
	} 
	else 
	{
		// The calling code may tell us to kill particles that are already out of bounds.
		// That causes this code to kill more particles than we're supposed to (possibly even causing a crash).
		// So remember how many particles we had left to kill so we can properly decrement the count below.
		int decrementValue = nLeftInKillList;
		int writeLoc = *(pCurKillListSlot++);
		--nLeftInKillList;

		for ( int readLoc = 1 + writeLoc; readLoc < nParticlesActiveNow; ++readLoc )
		{
			if ( nLeftInKillList > 0 && readLoc == *pCurKillListSlot ) 
			{
				pCurKillListSlot++;
				--nLeftInKillList;
				continue;
			}

			MoveParticle( readLoc, writeLoc );
			++writeLoc;
		}

		nParticlesActiveNow -= decrementValue;
	}

	// set count in system and wipe kill list
	SetNActiveParticles( nParticlesActiveNow );
	m_nNumParticlesToKill = 0;
}

void CParticleCollection::CalculatePathValues( CPathParameters const &PathIn,
											   float flTimeStamp,
											   Vector *pStartPnt,
											   Vector *pMidPnt,
											   Vector *pEndPnt
											   )
{
	Vector StartPnt;
	GetControlPointAtTime( PathIn.m_nStartControlPointNumber, flTimeStamp, &StartPnt );
	Vector EndPnt;
	GetControlPointAtTime( PathIn.m_nEndControlPointNumber, flTimeStamp, &EndPnt );
	
	Vector MidP;
	VectorLerp(StartPnt, EndPnt, PathIn.m_flMidPoint, MidP);

	if ( PathIn.m_nBulgeControl )
	{
		Vector vTarget=(EndPnt-StartPnt);
		float flBulgeScale = 0.0;
		int nCP=PathIn.m_nStartControlPointNumber;
		if ( PathIn.m_nBulgeControl == 2)
			nCP = PathIn.m_nEndControlPointNumber;
		Vector Fwd = m_ControlPoints[nCP].m_ForwardVector;
		float len=VectorLength( vTarget);
		if ( len > 1.0e-6 )
		{
			vTarget *= (1.0/len);						// normalize
			flBulgeScale = 1.0-fabs( DotProduct( vTarget, Fwd )); // bulge inversely scaled
		}
		Vector Potential_MidP=Fwd;
		float flOffsetDist = VectorLength( Potential_MidP );
		if ( flOffsetDist > 1.0e-6 )
		{
			Potential_MidP *= (PathIn.m_flBulge*len*flBulgeScale)/flOffsetDist;
			MidP += Potential_MidP;
		}
	}
	else
	{
		Vector RndVector;
		RandomVector( 0, -PathIn.m_flBulge, PathIn.m_flBulge, &RndVector);
		MidP+=RndVector;
	}
	
	*pStartPnt = StartPnt;
	*pMidPnt = MidP;
	*pEndPnt = EndPnt;
}


//-----------------------------------------------------------------------------
//
// Default impelemtation of the query
//
//-----------------------------------------------------------------------------

class CDefaultParticleSystemQuery : public CBaseAppSystem< IParticleSystemQuery >
{
public:
	virtual void GetLightingAtPoint( const Vector& vecOrigin, Color &tint )
	{
		tint.SetColor( 255, 255, 255, 255 );
	}
	virtual void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const class IHandleEntity *ignore,
							int collisionGroup, CBaseTrace *ptr )
	{
		ptr->fraction = 1.0;								// no hit
	}

	virtual void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionBias,
		Vector *pHitBoxRelativeCoordOut, int *pHitBoxIndexOut ) 
	{
		for ( int i = 0; i < nNumPtsOut; ++i )
		{
			pPntsOut[i].Init();
		}
	}

	virtual float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale ) { return 0.0f; }
};

static CDefaultParticleSystemQuery s_DefaultParticleSystemQuery;


//-----------------------------------------------------------------------------
//
// Particle system manager
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemMgr::CParticleSystemMgr()
	// m_SheetList( DefLessFunc( ITexture * ) )
{
	m_pQuery = &s_DefaultParticleSystemQuery;
	m_bDidInit = false;
	m_bUsingDefaultQuery = true;
	m_bShouldLoadSheets = true;
	m_pParticleSystemDictionary = NULL;
	m_nNumFramesMeasured = 0;
	m_flLastSimulationTime = 0.0f;
	m_nParticleVertexCount = m_nParticleIndexCount = 0;
	m_bFrameWarningNeeded = false;

	for ( int i = 0; i < c_nNumFramesTracked; i++ )
	{
		m_nParticleVertexCountHistory[i] = 0;
	}
	m_fParticleCountScaling = 1.0f;
}

CParticleSystemMgr::~CParticleSystemMgr()
{
	if ( m_pParticleSystemDictionary )
	{
		delete m_pParticleSystemDictionary;
		m_pParticleSystemDictionary = NULL;
	}
	FlushAllSheets();
}


//-----------------------------------------------------------------------------
// Initialize the particle system
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::Init( IParticleSystemQuery *pQuery )
{
	if ( !g_pMaterialSystem->QueryInterface( MATERIAL_SYSTEM_INTERFACE_VERSION ) )
	{
		Msg( "CParticleSystemMgr compiled using an old IMaterialSystem\n" );
		return false;
	}

	if ( m_bUsingDefaultQuery && pQuery )
	{
		m_pQuery = pQuery;
		m_bUsingDefaultQuery = false;
	}

	if ( !m_bDidInit )
	{
		m_pParticleSystemDictionary = new CParticleSystemDictionary;
		// NOTE: This is for the editor only
		AddParticleOperator( FUNCTION_CHILDREN, &s_ChildOperatorDefinition );

		m_pShadowDepthMaterial = NULL;
		if( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 )
		{
			KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
			pVMTKeyValues->SetInt( "$no_fullbright", 1 );
			pVMTKeyValues->SetInt( "$model", 0 );
			pVMTKeyValues->SetInt( "$alphatest", 0 );
			m_pShadowDepthMaterial = g_pMaterialSystem->CreateMaterial( "__particlesDepthWrite", pVMTKeyValues );
		}

		SeedRandSIMD( 12345678 );
		m_bDidInit = true;
	}

	return true;
}

//----------------------------------------------------------------------------------
// Cache/uncache materials used by particle systems
//----------------------------------------------------------------------------------
void CParticleSystemMgr::PrecacheParticleSystem( const char *pName )
{
	if ( !pName || !pName[0] )
	{
		return;
	}

	CParticleSystemDefinition* pDef = FindParticleSystem( pName );
	if ( !pDef )
	{
		Warning( "Attemped to precache unknown particle system \"%s\"!\n", pName );
		return;
	}

	pDef->Precache();
}

void CParticleSystemMgr::UncacheAllParticleSystems()
{
	if ( !m_pParticleSystemDictionary )
		return;

	int nCount = m_pParticleSystemDictionary->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_pParticleSystemDictionary->GetParticleSystem( i )->Uncache();
	}

	nCount = m_pParticleSystemDictionary->NameCount();
	for ( ParticleSystemHandle_t h = 0; h < nCount; ++h )
	{
		m_pParticleSystemDictionary->FindParticleSystem( h )->Uncache();
	}
	
	// Flush sheets, as they can accumulate several MB of memory per map
	FlushAllSheets();
}


//-----------------------------------------------------------------------------
// return the particle field name
//-----------------------------------------------------------------------------
static const char *s_pParticleFieldNames[MAX_PARTICLE_ATTRIBUTES] = 
{
	"Position", // XYZ, 0
	"Life Duration", // LIFE_DURATION, 1 );
	NULL, // PREV_XYZ is for internal use only
	"Radius", // RADIUS, 3 );

	"Roll", // ROTATION, 4 );
	"Roll Speed", // ROTATION_SPEED, 5 );
	"Color", // TINT_RGB, 6 );
	"Alpha", // ALPHA, 7 );

	"Creation Time", // CREATION_TIME, 8 );
	"Sequence Number", // SEQUENCE_NUMBER, 9 );
	"Trail Length", // TRAIL_LENGTH, 10 );
	"Particle ID", // PARTICLE_ID, 11 ); 

	"Yaw", // YAW, 12 );
	"Sequence Number 1", // SEQUENCE_NUMBER1, 13 );
	NULL, // HITBOX_INDEX is for internal use only
	NULL, // HITBOX_XYZ_RELATIVE is for internal use only

	"Alpha Alternate", // ALPHA2, 16
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
};

const char* CParticleSystemMgr::GetParticleFieldName( int nParticleField ) const
{
	return s_pParticleFieldNames[nParticleField];
}

	
//-----------------------------------------------------------------------------
// Returns the available particle operators
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleOperator( ParticleFunctionType_t nOpType,
						 IParticleOperatorDefinition *pOpFactory )
{
	m_ParticleOperators[nOpType].AddToTail( pOpFactory );
}

CUtlVector< IParticleOperatorDefinition *> &CParticleSystemMgr::GetAvailableParticleOperatorList( ParticleFunctionType_t nWhichList )
{
	return m_ParticleOperators[nWhichList];
}

const DmxElementUnpackStructure_t *CParticleSystemMgr::GetParticleSystemDefinitionUnpackStructure()
{
	return s_pParticleSystemDefinitionUnpack;
}


//------------------------------------------------------------------------------
// custom allocators for operators so simd aligned
//------------------------------------------------------------------------------
#include "tier0/memdbgoff.h"
void *CParticleOperatorInstance::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CParticleOperatorInstance::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAligned( nSize, 16, pFileName, nLine );
}

void CParticleOperatorInstance::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleOperatorInstance::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleDefinitions( CUtlBuffer &buf, const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	DECLARE_DMX_CONTEXT_DECOMMIT( bDecommitTempMemory );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot, pFileName ) || !pRoot )
	{
		Warning( "Unable to read particle definition %s! UtlBuffer is the wrong type!\n", pFileName );
		return false;
	}

	if ( !Q_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( pRoot );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( IsPC() )
			{
				pDef->Precache();
			}
		}
		CleanupDMX( pRoot );
		return true;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return false;
	}

	const CUtlVector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = definitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( definitions[i] );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( IsPC() )
			{
				pDef->Precache();
			}
		}
	}

	CleanupDMX( pRoot );
	return true;
}


//-----------------------------------------------------------------------------
// Decommits temporary memory
//-----------------------------------------------------------------------------
void CParticleSystemMgr::DecommitTempMemory()
{
	DecommitDMXMemory();
}


//-----------------------------------------------------------------------------
// Sets the last simulation time, used for particle system sleeping logic
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetLastSimulationTime( float flTime )
{
	m_flLastSimulationTime = flTime;

	int nParticleVertexCountHistoryMax = 0;
	for ( int i = 0; i < c_nNumFramesTracked-1; i++ )
	{
		m_nParticleVertexCountHistory[i] = m_nParticleVertexCountHistory[i+1];
		nParticleVertexCountHistoryMax = Max ( nParticleVertexCountHistoryMax, m_nParticleVertexCountHistory[i] );
	}
	m_nParticleVertexCountHistory[c_nNumFramesTracked-1] = m_nParticleVertexCount;
	nParticleVertexCountHistoryMax = Max ( nParticleVertexCountHistoryMax, m_nParticleVertexCount );

	// We need to take an average over a decent number of frames because this throttling has a direct feedback effect. Worried about oscillation problems!
	int nLower = CL_PARTICLE_SCALE_LOWER;//cl_particle_scale_lower.GetInt();
	m_fParticleCountScaling = 1.0f;
	int nHowManyOver = nParticleVertexCountHistoryMax - nLower;
	if ( nHowManyOver > 0 )
	{
		int nUpper = CL_PARTICLE_SCALE_UPPER;//cl_particle_scale_upper.GetInt();
		int nRange = nUpper - nLower;
		m_fParticleCountScaling = 1.0f - ( (float)nHowManyOver / (float)nRange );
		m_fParticleCountScaling = Clamp ( m_fParticleCountScaling, 0.0f, 1.0f );
	}

	m_nParticleVertexCount = m_nParticleIndexCount = 0;
}

float CParticleSystemMgr::GetLastSimulationTime() const
{
	return m_flLastSimulationTime;
}

bool CParticleSystemMgr::Debug_FrameWarningNeededTestAndReset()
{
	bool bTemp = m_bFrameWarningNeeded;
	m_bFrameWarningNeeded = false;
	return bTemp;
}

int CParticleSystemMgr::Debug_GetTotalParticleCount() const
{
	return m_nParticleVertexCountHistory[c_nNumFramesTracked-1];
}

float CParticleSystemMgr::ParticleThrottleScaling() const
{
	return m_fParticleCountScaling;
}

bool CParticleSystemMgr::ParticleThrottleRandomEnable() const
{
	if ( m_fParticleCountScaling == 1.0f )
	{
		// No throttling.
		return true;
	}
	else if ( m_fParticleCountScaling > RandomFloat ( 0.0f, 1.0f ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Unserialization-related methods
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleSystem( CDmxElement *pParticleSystem )
{
	m_pParticleSystemDictionary->AddParticleSystem( pParticleSystem );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const char *pName )
{
	return m_pParticleSystemDictionary->FindParticleSystem( pName );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const DmObjectId_t& id )
{
	return m_pParticleSystemDictionary->FindParticleSystem( id );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( CUtlBuffer &buf, bool bPrecache, bool bDecommitTempMemory, const char *pFileName )
{
	return ReadParticleDefinitions( buf, pFileName, bPrecache, bDecommitTempMemory );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	// Names starting with a '!' are always precached.
	if ( pFileName[0] == '!' )
	{
		bPrecache = true;
		++pFileName;
	}

	if ( IsX360() )
	{
		char szTargetName[MAX_PATH];
		CreateX360Filename( pFileName, szTargetName, sizeof( szTargetName ) );

		CUtlBuffer fileBuffer;
		bool bHaveParticles = g_pFullFileSystem->ReadFile( szTargetName, "GAME", fileBuffer );
		if ( bHaveParticles )
		{			
			fileBuffer.SetBigEndian( false );
			return ReadParticleConfigFile( fileBuffer, bPrecache, bDecommitTempMemory, szTargetName );
		}
		else if ( g_pFullFileSystem->GetDVDMode() != DVDMODE_OFF )
		{
			// 360 version should have been there, 360 zips can only have binary particles
			Warning( "Particles: Missing '%s'\n", szTargetName );
			return false;
		}
	}

	char pFallbackBuf[MAX_PATH];
	if ( IsPC() )
	{
		// Look for fallback particle systems
		char pTemp[MAX_PATH];
		Q_StripExtension( pFileName, pTemp, sizeof(pTemp) );
		const char *pExt = Q_GetFileExtension( pFileName );
		if ( !pExt )
		{
			pExt = "pcf";
		}
		
		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 )
		{
			Q_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx80.%s", pTemp, pExt );
			if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
			{
				pFileName = pFallbackBuf;
			}
		}
		else if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() == 90 &&  g_pMaterialSystemHardwareConfig->PreferReducedFillrate() )
		{
			Q_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx90_slow.%s", pTemp, pExt );
			if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
			{
				pFileName = pFallbackBuf;
			}
		}
	}

	CUtlBuffer buf( 0, 0, 0 );
	if ( IsX360() )
	{
		// fell through, load as pc particle resource file
		buf.ActivateByteSwapping( true );
	}

	if ( g_pFullFileSystem->ReadFile( pFileName, "GAME", buf ) )
	{
		return ReadParticleConfigFile( buf, bPrecache, bDecommitTempMemory, pFileName );
	}

	Warning( "Particles: Missing '%s'\n", pFileName );
	return false;
}


//-----------------------------------------------------------------------------
// Write a specific particle config to a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::WriteParticleConfigFile( const char *pParticleSystemName, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( pParticleSystemName );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( const DmObjectId_t& id, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( id );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( CDmxElement *pParticleSystem, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	pParticleSystem->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );

	CDmxAttribute* pAttribute = pParticleSystem->GetAttribute( "children" );
	const CUtlVector< CDmxElement* >& children = pAttribute->GetArray<CDmxElement*>( );
	int nCount = children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = children[ i ];
		CDmxElement *pChild = pChildRef->GetValue<CDmxElement*>( "child" );
		pChild->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );
	}

	// Now write the DMX elements out
	bool bOk = SerializeDMX( buf, pParticleSystem );
	CleanupDMX( pParticleSystem );
	return bOk;
}

ParticleSystemHandle_t CParticleSystemMgr::GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return UTL_INVAL_SYMBOL;

	return m_pParticleSystemDictionary->FindParticleSystemHandle( pParticleSystemName );
}

const char *CParticleSystemMgr::GetParticleSystemNameFromIndex( ParticleSystemHandle_t iIndex )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( iIndex );
	return pDef ? pDef->GetName() : "Unknown";
}

int CParticleSystemMgr::GetParticleSystemCount( void )
{
	return m_pParticleSystemDictionary->NameCount();
}


//-----------------------------------------------------------------------------
// Factory method for creating particle collections
//-----------------------------------------------------------------------------
CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const char *pParticleSystemName, float flDelay, int nRandomSeed )
{
	if ( !pParticleSystemName )
		return NULL;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type %s\n", pParticleSystemName );
		return NULL;
	}

	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}


CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const DmObjectId_t &id, float flDelay, int nRandomSeed )
{
	if ( !IsUniqueIdValid( id ) )
		return NULL;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	if ( !pDef )
	{
		char pBuf[256];
		UniqueIdToString( id, pBuf, sizeof(pBuf) );
		Warning( "Attempted to create unknown particle system id %s\n", pBuf );
		return NULL;
	}

	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const DmObjectId_t &id )
{
	if ( !IsUniqueIdValid( id ) )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	return ( pDef != NULL );
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const char *pName )
{
	if ( !pName || !pName[0] )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pName );
	return ( pDef != NULL );
}


//--------------------------------------------------------------------------------
// Particle kill list
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
// Serialization-related methods
//--------------------------------------------------------------------------------
CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const DmObjectId_t &id )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}

CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const char *pParticleSystemName )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}

//--------------------------------------------------------------------------------
// Client loads sheets for rendering, server doesn't need to.
//--------------------------------------------------------------------------------
void CParticleSystemMgr::ShouldLoadSheets( bool bLoadSheets )
{
	m_bShouldLoadSheets = bLoadSheets;
}

//--------------------------------------------------------------------------------
// Particle sheets
//--------------------------------------------------------------------------------
CSheet *CParticleSystemMgr::FindOrLoadSheet( char const *pszFname, ITexture *pTexture )
{
	if ( !m_bShouldLoadSheets )
		return NULL;

	if ( m_SheetList.Defined( pszFname ) )
		return m_SheetList[ pszFname ];

	CSheet *pNewSheet = NULL;

	size_t numBytes;
	void const *pSheet =  pTexture->GetResourceData( VTF_RSRC_SHEET, &numBytes );

	if ( pSheet )
	{
		CUtlBuffer bufLoad( pSheet, numBytes, CUtlBuffer::READ_ONLY );
		pNewSheet = new CSheet( bufLoad );
	}

	m_SheetList[ pszFname ] = pNewSheet;
	return pNewSheet;
}

CSheet *CParticleSystemMgr::FindOrLoadSheet( IMaterial *pMaterial )
{
	if ( !pMaterial )
		return NULL;

	bool bFoundVar = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$basetexture", &bFoundVar, true );

	if ( bFoundVar && pVar && pVar->IsDefined() )
	{
		ITexture *pTex = pVar->GetTextureValue();
		if ( pTex && !pTex->IsError() )
			return FindOrLoadSheet( pTex->GetName(), pTex );
	}

	return NULL;
}

void CParticleSystemMgr::FlushAllSheets( void )
{
// 	for( int i = 0, iEnd = m_SheetList.Count(); i < iEnd; i++ )
// 	{
// 		delete m_SheetList.Element(i);
// 	}
// 
// 	m_SheetList.RemoveAll();

	m_SheetList.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Render cache
//-----------------------------------------------------------------------------
void CParticleSystemMgr::ResetRenderCache( void )
{
	int nCount = m_RenderCache.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_RenderCache[i].m_ParticleCollections.RemoveAll();
	}
}

void CParticleSystemMgr::AddToRenderCache( CParticleCollection *pParticles )
{
	if ( !pParticles->IsValid() || pParticles->m_pDef->GetMaterial()->IsTranslucent() )
		return;

	pParticles->m_flNextSleepTime = Max ( pParticles->m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + pParticles->m_pDef->m_flNoDrawTimeToGoToSleep ));
	// Find the current rope list.
	int iRenderCache = 0;
	int nRenderCacheCount = m_RenderCache.Count();
	for ( ; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		if ( ( pParticles->m_pDef->GetMaterial() == m_RenderCache[iRenderCache].m_pMaterial ) )
			break;
	}

	// A full rope list should have been generate in CreateRenderCache
	// If we didn't find one, then allocate the mofo.
	if ( iRenderCache == nRenderCacheCount )
	{
		iRenderCache = m_RenderCache.AddToTail();
		m_RenderCache[iRenderCache].m_pMaterial = pParticles->m_pDef->GetMaterial();
	}

	m_RenderCache[iRenderCache].m_ParticleCollections.AddToTail( pParticles );

	for( CParticleCollection *p = pParticles->m_Children.m_pHead; p; p = p->m_pNext )
	{
		AddToRenderCache( p );
	}
}


void CParticleSystemMgr::BuildBatchList( int iRenderCache, IMatRenderContext *pRenderContext, CUtlVector< Batch_t >& batches )
{
	batches.RemoveAll();

	IMaterial *pMaterial = m_RenderCache[iRenderCache].m_pMaterial;
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nRemainingVertices = nMaxVertices;
	int nRemainingIndices = nMaxIndices;

	int i = batches.AddToTail();
	Batch_t* pBatch = &batches[i];
	pBatch->m_nVertCount = 0;
	pBatch->m_nIndexCount = 0;

	// Ask each renderer about the # of verts + ints it will draw
	int nCacheCount = m_RenderCache[iRenderCache].m_ParticleCollections.Count();
	for ( int iCache = 0; iCache < nCacheCount; ++iCache )
	{
		CParticleCollection *pParticles = m_RenderCache[iRenderCache].m_ParticleCollections[iCache];
		if ( !pParticles->IsValid() )
			continue;

		int nRenderCount = pParticles->GetRendererCount();
		for ( int j = 0; j < nRenderCount; ++j )
		{
			int nFirstParticle = 0;
			while ( nFirstParticle < pParticles->m_nActiveParticles )
			{
				int iPart;
				BatchStep_t step;
				step.m_pParticles = pParticles;
				step.m_pRenderer = pParticles->GetRenderer( j );
				step.m_pContext = pParticles->GetRendererContext( j ); 
				step.m_nFirstParticle = nFirstParticle;
				step.m_nParticleCount = step.m_pRenderer->GetParticlesToRender( pParticles, 
					step.m_pContext, nFirstParticle, nRemainingVertices, nRemainingIndices, &step.m_nVertCount, &iPart );
				nFirstParticle += step.m_nParticleCount;

				if ( step.m_nParticleCount > 0 )
				{
					pBatch->m_nVertCount += step.m_nVertCount;
					pBatch->m_nIndexCount += iPart;
					pBatch->m_BatchStep.AddToTail( step );
					Assert( pBatch->m_nVertCount <= nMaxVertices && pBatch->m_nIndexCount <= nMaxIndices );
				}
				else
				{
					if ( pBatch->m_nVertCount == 0 )
						break;

					// Not enough room
					Assert( pBatch->m_nVertCount > 0 && pBatch->m_nIndexCount > 0 ); 
					pBatch = &batches[batches.AddToTail()];
					pBatch->m_nVertCount = 0;
					pBatch->m_nIndexCount = 0;
					nRemainingVertices = nMaxVertices;
					nRemainingIndices = nMaxIndices;
				}
			}
		}
	}

	if ( pBatch->m_nVertCount <= 0 || pBatch->m_nIndexCount <= 0 ) 
	{
		batches.FastRemove( batches.Count() - 1 );
	}
}

void CParticleSystemMgr::DumpProfileInformation( void )
{
#if MEASURE_PARTICLE_PERF
	FileHandle_t fh = g_pFullFileSystem->Open( "particle_profile.csv", "w" );
	g_pFullFileSystem->FPrintf( fh, "numframes,%d\n", m_nNumFramesMeasured );
	g_pFullFileSystem->FPrintf( fh, "name, total time, max time, max particles, allocated particles\n");
	for( int i=0; i < m_pParticleSystemDictionary->NameCount(); i++ )
	{
		CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];
		if ( p->m_nMaximumActiveParticles )
			g_pFullFileSystem->FPrintf( fh, "%s,%f,%f,%d,%d\n", p->m_Name.Get(), p->m_flTotalSimTime, p->m_flMaxMeasuredSimTime, p->m_nMaximumActiveParticles, p->m_nMaxParticles );
	}
	g_pFullFileSystem->FPrintf( fh, "\n\nopname, total time, max time\n");
	for(int i=0; i < ARRAYSIZE( m_ParticleOperators ); i++)
	{
		for(int j=0; j < m_ParticleOperators[i].Count() ; j++ )
		{
			float flmax = m_ParticleOperators[i][j]->MaximumRecordedExecutionTime();
			float fltotal = m_ParticleOperators[i][j]->TotalRecordedExecutionTime();
			if ( fltotal > 0.0 )
				g_pFullFileSystem->FPrintf( fh, "%s,%f,%f\n", 
											m_ParticleOperators[i][j]->GetName(), fltotal, flmax );
		}	   
	}
	g_pFullFileSystem->Close( fh );
#endif
}

void CParticleSystemMgr::CommitProfileInformation( bool bCommit )
{
#if MEASURE_PARTICLE_PERF
	if ( 1 )
	{
		if ( bCommit )
			m_nNumFramesMeasured++;
		for( int i=0; i < m_pParticleSystemDictionary->NameCount(); i++ )
		{
			CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];
			if ( bCommit )
				p->m_flTotalSimTime += p->m_flUncomittedTotalSimTime;
			p->m_flUncomittedTotalSimTime = 0.;
		}
		for(int i=0; i < ARRAYSIZE( m_ParticleOperators ); i++)
		{
			for(int j=0; j < m_ParticleOperators[i].Count() ; j++ )
			{
				if ( bCommit )
					m_ParticleOperators[i][j]->m_flTotalExecutionTime += m_ParticleOperators[i][j]->m_flUncomittedTime;
				m_ParticleOperators[i][j]->m_flUncomittedTime = 0;
			}
		}
	}
#endif
}

void CParticleSystemMgr::DrawRenderCache( bool bShadowDepth )
{
	int nRenderCacheCount = m_RenderCache.Count();
	if ( nRenderCacheCount == 0 )
		return;

	VPROF_BUDGET( "CParticleSystemMgr::DrawRenderCache", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CUtlVector< Batch_t > batches( 0, 8 );

	for ( int iRenderCache = 0; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		int nCacheCount = m_RenderCache[iRenderCache].m_ParticleCollections.Count();
		if ( nCacheCount == 0 )
			continue;

		// FIXME: When rendering shadow depth, do it all in 1 batch
		IMaterial *pMaterial = bShadowDepth ? m_pShadowDepthMaterial : m_RenderCache[iRenderCache].m_pMaterial;

		BuildBatchList( iRenderCache, pRenderContext, batches );
		int nBatchCount = batches.Count();
		if ( nBatchCount == 0 )
			continue;

		pRenderContext->Bind( pMaterial );
		CMeshBuilder meshBuilder;
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );

		for ( int i = 0; i < nBatchCount; ++i )
		{
			const Batch_t& batch = batches[i];
			Assert( batch.m_nVertCount > 0 && batch.m_nIndexCount > 0 );

			g_pParticleSystemMgr->TallyParticlesRendered( batch.m_nVertCount * 3, batch.m_nIndexCount * 3 );

			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, batch.m_nVertCount, batch.m_nIndexCount );

			int nVertexOffset = 0;
			int nBatchStepCount = batch.m_BatchStep.Count();
			for ( int j = 0; j < nBatchStepCount; ++j )
			{
				const BatchStep_t &step = batch.m_BatchStep[j];
				// FIXME: this will break if it ever calls into C_OP_RenderSprites::Render[TwoSequence]SpriteCard()
				//        (need to protect against that and/or split the meshBuilder batch to support that path here)
				step.m_pRenderer->RenderUnsorted( step.m_pParticles, step.m_pContext, pRenderContext, 
					meshBuilder, nVertexOffset, step.m_nFirstParticle, step.m_nParticleCount );
				nVertexOffset += step.m_nVertCount;
			}

			meshBuilder.End();
			pMesh->Draw();
		}
	}

	ResetRenderCache( );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}


void CParticleSystemMgr::TallyParticlesRendered( int nVertexCount, int nIndexCount )
{
	m_nParticleIndexCount += nIndexCount;
	m_nParticleVertexCount += nVertexCount;

	if ( m_nParticleVertexCount > MAX_PARTICLE_VERTS )
	{
		m_bFrameWarningNeeded = true;
	}
}






void IParticleSystemQuery::GetRandomPointsOnControllingObjectHitBox(
	CParticleCollection *pParticles,
	int nControlPointNumber, 
	int nNumPtsOut,
	float flBBoxScale,
	int nNumTrysToGetAPointInsideTheModel,
	Vector *pPntsOut,
	Vector vecDirectionalBias,
	Vector *pHitBoxRelativeCoordOut,
	int *pHitBoxIndexOut
	)
{
	for(int i=0; i < nNumPtsOut; i++)
	{
		pPntsOut[i]=pParticles->m_ControlPoints[nControlPointNumber].m_Position;
		if ( pHitBoxRelativeCoordOut )
			pHitBoxRelativeCoordOut[i].Init();
		if ( pHitBoxIndexOut )
			pHitBoxIndexOut[i] = -1;
	}
}



void CParticleCollection::UpdateHitBoxInfo( int nControlPointNumber )
{
	CModelHitBoxesInfo &hb = m_ControlPointHitBoxes[nControlPointNumber];

	if ( hb.m_flLastUpdateTime == m_flCurTime )
		return;												// up to date

	hb.m_flLastUpdateTime = m_flCurTime;

	// make sure space allocated
	if ( ! hb.m_pHitBoxes )
		hb.m_pHitBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];
	if ( ! hb.m_pPrevBoxes )
		hb.m_pPrevBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];

	// save current into prev
	hb.m_nNumPrevHitBoxes = hb.m_nNumHitBoxes;
	hb.m_flPrevLastUpdateTime = hb.m_flLastUpdateTime;
	V_swap( hb.m_pHitBoxes, hb.m_pPrevBoxes );

	// issue hitbox query
	hb.m_nNumHitBoxes = g_pParticleSystemMgr->Query()->GetControllingObjectHitBoxInfo(
		this, nControlPointNumber, MAXSTUDIOBONES, hb.m_pHitBoxes );

}
