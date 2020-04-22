//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "physics_vehicle.h"

#include "ivp_material.hxx"
#include <ctype.h>
#include "utlsymbol.h"
#include "tier1/strtools.h" 
#include "vcollide_parse_private.h"
#include "ctype.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: This is the data stored for each material/surface propery list
//-----------------------------------------------------------------------------
class CSurface : public IVP_Material
{
public:

	// IVP_Material
    virtual IVP_DOUBLE get_friction_factor()
	{
		return data.physics.friction;
	}
    
    virtual IVP_DOUBLE get_elasticity()
	{
		return data.physics.elasticity;
	}
    virtual const char *get_name();
	// UNDONE: not implemented here.
	virtual IVP_DOUBLE get_second_friction_factor() 
	{ 
		return 0; 
	}
    virtual IVP_DOUBLE get_adhesion()
	{
		return 0;
	}

	virtual IVP_DOUBLE get_damping()
	{
		return data.physics.dampening;
	}

	// strings
	CUtlSymbol			m_name;
	unsigned short		m_pad;

	// physics properties
	surfacedata_t	data;
};


class CPhysicsSurfaceProps;

class CIVPMaterialManager : public IVP_Material_Manager
{
	typedef IVP_Material_Manager BaseClass;
public:
	CIVPMaterialManager( void );
	void Init( CPhysicsSurfaceProps *pProps ) { m_props = pProps; }
	void SetPropMap( int *map, int mapSize );
	int RemapIVPMaterialIndex( int ivpMaterialIndex ) const;

	// IVP_Material_Manager
	virtual IVP_Material *get_material_by_index(IVP_Real_Object *pObject, const IVP_U_Point *world_position, int index);

    virtual IVP_DOUBLE get_friction_factor(IVP_Contact_Situation *situation)	// returns values >0, value of 1.0f means object stands on a 45 degres hill
	{
		// vehicle wheels get no friction with stuff that isn't ground
		// helps keep control of the car
		// traction on less than 60 degree slopes.
		float wheelFriction = 1.0f;
		if ( ShouldOverrideWheelContactFriction( &wheelFriction, situation->objects[0], situation->objects[1], &situation->surf_normal ) )
		{
			return wheelFriction;
		}

		IVP_DOUBLE factor = BaseClass::get_friction_factor( situation );
		factor = clamp(factor,0.0,1.0);

		return factor;
	}

    virtual IVP_DOUBLE get_elasticity(IVP_Contact_Situation *situation)		// range [0, 1.0f[, the relative speed after a collision compared to the speed before
	{
		IVP_DOUBLE flElasticity = BaseClass::get_elasticity( situation );
		if ( flElasticity > 1.0f )
		{
			flElasticity = 1.0f;
		}
		else if ( flElasticity < 0 )
		{
			flElasticity = 0;
		}
		return flElasticity;
	}

private:
	CPhysicsSurfaceProps	*m_props;
	unsigned short			m_propMap[128];
};


//-----------------------------------------------------------------------------
// Purpose: This is the main database of materials
//-----------------------------------------------------------------------------
class CPhysicsSurfaceProps : public IPhysicsSurfacePropsInternal
{
public:
	CPhysicsSurfaceProps( void );
	~CPhysicsSurfaceProps( void );

	virtual int		ParseSurfaceData( const char *pFilename, const char *pTextfile );
	virtual int		SurfacePropCount( void ) const;
	virtual int		GetSurfaceIndex( const char *pPropertyName ) const;
	virtual void	GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const;
	virtual void	GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const;
	virtual surfacedata_t *GetSurfaceData( int surfaceDataIndex );
	virtual const char *GetString( unsigned short stringTableIndex ) const;
	virtual const char *GetPropName( int surfaceDataIndex ) const;
	virtual void SetWorldMaterialIndexTable( int *pMapArray, int mapSize );
	virtual int RemapIVPMaterialIndex( int ivpMaterialIndex ) const
	{
		return m_ivpManager.RemapIVPMaterialIndex( ivpMaterialIndex );
	}
	bool IsReservedMaterialIndex( int materialIndex ) const;
	virtual const char *GetReservedMaterialName( int materialIndex ) const;
	int	GetReservedFallBack( int materialIndex ) const;

	int GetReservedSurfaceIndex( const char *pPropertyName ) const;

	// The database is derived from the IVP material class
	const IVP_Material *GetIVPMaterial( int materialIndex ) const;
	IVP_Material *GetIVPMaterial( int materialIndex );
	virtual int GetIVPMaterialIndex( const IVP_Material *pIVP ) const;
	IVP_Material_Manager *GetIVPManager( void ) { return &m_ivpManager; }

	const char *GetNameString( CUtlSymbol name ) const
	{
		return m_strings.String(name);
	}

private:
	const CSurface	*GetInternalSurface( int materialIndex ) const;
	CSurface	*GetInternalSurface( int materialIndex );
	
	void			CopyPhysicsProperties( CSurface *pOut, int baseIndex );
	bool			AddFileToDatabase( const char *pFilename );

private:
	CUtlSymbolTableMT			m_strings;
	CUtlVector<CSurface>		m_props;
	CUtlVector<CUtlSymbol>		m_fileList;
	CIVPMaterialManager			m_ivpManager;
	bool						m_init;
	int							m_shadowFallback;
};


// Singleton database object
CPhysicsSurfaceProps g_SurfaceDatabase;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CPhysicsSurfaceProps, IPhysicsSurfaceProps, VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, g_SurfaceDatabase);


// Global pointer to singleton for VPHYSICS.DLL internal access
IPhysicsSurfacePropsInternal *physprops = &g_SurfaceDatabase;


const char *CSurface::get_name()
{
	return g_SurfaceDatabase.GetNameString( m_name );
}

CPhysicsSurfaceProps::CPhysicsSurfaceProps( void ) : m_fileList(8,8), m_strings( 0, 32, true )
{
	m_ivpManager.Init( this );
	// Force index 0 to be the empty string.  Allows game code to check for zero, but
	// still resolve to a string
	m_strings.AddString("");
	m_init = false;
	m_shadowFallback = 0;
}


CPhysicsSurfaceProps::~CPhysicsSurfaceProps( void )
{
}

int CPhysicsSurfaceProps::SurfacePropCount( void ) const
{
	return m_props.Size();
}

// Add the filename to a list to make sure each file is only processed once
bool CPhysicsSurfaceProps::AddFileToDatabase( const char *pFilename )
{
	CUtlSymbol id = m_strings.AddString( pFilename );

	for ( int i = 0; i < m_fileList.Size(); i++ )
	{
		if ( m_fileList[i] == id )
			return false;
	}

	m_fileList.AddToTail( id );
	return true;
}

int CPhysicsSurfaceProps::GetSurfaceIndex( const char *pPropertyName ) const
{
	if ( pPropertyName[0] == '$' )
	{
		int index = GetReservedSurfaceIndex( pPropertyName );
		if ( index >= 0 )
			return index;
	}

	CUtlSymbol id = m_strings.Find( pPropertyName );
	if ( id.IsValid() )
	{
		// BUGBUG: Linear search is slow!!!
		for ( int i = 0; i < m_props.Size(); i++ )
		{
			// NOTE: Just comparing strings by index is pretty fast though
			if ( m_props[i].m_name == id )
				return i;
		}
	}

	return -1;
}


const char *CPhysicsSurfaceProps::GetPropName( int surfaceDataIndex ) const
{
	const CSurface *pSurface = GetInternalSurface( surfaceDataIndex );
	if ( pSurface )
	{
		return GetNameString( pSurface->m_name );
	}
	return NULL;
}


// UNDONE: move reserved materials into this table, or into a parallel table
// that gets hooked out here.
CSurface *CPhysicsSurfaceProps::GetInternalSurface( int materialIndex )
{
	if ( IsReservedMaterialIndex( materialIndex ) )
	{
		materialIndex = GetReservedFallBack( materialIndex );
	}
	if ( materialIndex < 0 || materialIndex > m_props.Size()-1 )
	{
		return NULL;
	}
	return &m_props[materialIndex];
}

// this function is actually const except for the return type, so this is safe
const CSurface *CPhysicsSurfaceProps::GetInternalSurface( int materialIndex ) const
{
	return const_cast<CPhysicsSurfaceProps *>(this)->GetInternalSurface(materialIndex);
}

void CPhysicsSurfaceProps::GetPhysicsProperties( int materialIndex, float *density, float *thickness, float *friction, float *elasticity ) const
{
	const CSurface *pSurface = GetInternalSurface( materialIndex );
	if ( !pSurface )
	{
		pSurface = GetInternalSurface( GetSurfaceIndex( "default" ) );
		Assert ( pSurface );
	}
	if ( pSurface )
	{
		if ( friction )
		{
			*friction = (float)pSurface->data.physics.friction;
		}
		if ( elasticity )
		{
			*elasticity = (float)pSurface->data.physics.elasticity;
		}
		if ( density )
		{
			*density = pSurface->data.physics.density;
		}
		if ( thickness )
		{
			*thickness = pSurface->data.physics.thickness;
		}
	}
}

void CPhysicsSurfaceProps::GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const
{
	if ( !pParamsOut )
		return;

	const CSurface *pSurface = GetInternalSurface( surfaceDataIndex );
	if ( pSurface )
	{
		*pParamsOut = pSurface->data.physics;
	}
}

surfacedata_t *CPhysicsSurfaceProps::GetSurfaceData( int materialIndex )
{
	CSurface *pSurface = GetInternalSurface( materialIndex );
	if (!pSurface)
		pSurface = GetInternalSurface( 0 ); // Zero is always the "default" property

	Assert ( pSurface );
	return &pSurface->data;
}

const char *CPhysicsSurfaceProps::GetString( unsigned short stringTableIndex ) const
{
	return m_strings.String( stringTableIndex );
}


bool CPhysicsSurfaceProps::IsReservedMaterialIndex( int materialIndex ) const
{
	return (materialIndex > 127) ? true : false;
}

const char *CPhysicsSurfaceProps::GetReservedMaterialName( int materialIndex ) const
{
	// NOTE: All of these must start with '$'
	switch( materialIndex )
	{
	case MATERIAL_INDEX_SHADOW:
		return "$MATERIAL_INDEX_SHADOW";
	}

	return NULL;
}

int CPhysicsSurfaceProps::GetReservedSurfaceIndex( const char *pPropertyName ) const
{
	if ( !Q_stricmp( pPropertyName, "$MATERIAL_INDEX_SHADOW" ) )
	{
		return MATERIAL_INDEX_SHADOW;
	}
	return -1;
}

const IVP_Material *CPhysicsSurfaceProps::GetIVPMaterial( int materialIndex ) const
{
	return GetInternalSurface(materialIndex);
}

IVP_Material *CPhysicsSurfaceProps::GetIVPMaterial( int materialIndex )
{
	return GetInternalSurface(materialIndex);
}


int	CPhysicsSurfaceProps::GetReservedFallBack( int materialIndex ) const
{
	switch( materialIndex )
	{
	case MATERIAL_INDEX_SHADOW:
		return m_shadowFallback;
	}

	return 0;
}


int CPhysicsSurfaceProps::GetIVPMaterialIndex( const IVP_Material *pIVP ) const
{
	int index = (const CSurface *)pIVP - m_props.Base();
	if ( index >= 0 && index < m_props.Size() )
		return index;

	return -1;
}


void CPhysicsSurfaceProps::CopyPhysicsProperties( CSurface *pOut, int baseIndex )
{
	const CSurface *pSurface = GetInternalSurface( baseIndex );
	if ( pSurface )
	{
		pOut->data = pSurface->data;
	}
}


int CPhysicsSurfaceProps::ParseSurfaceData( const char *pFileName, const char *pTextfile )
{
	if ( !AddFileToDatabase( pFileName ) )
	{
		return 0;
	}

	const char *pText = pTextfile;

	do
	{
		char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

		pText = ParseKeyvalue( pText, key, value );
		if ( !strcmp(value, "{") )
		{
			CSurface prop;
			memset( &prop.data, 0, sizeof(prop.data) );
			prop.m_name = m_strings.AddString( key );
			int baseMaterial = GetSurfaceIndex( key );
			if ( baseMaterial < 0 )
			{
				baseMaterial = GetSurfaceIndex( "default" );
			}

			CopyPhysicsProperties( &prop, baseMaterial );

			do
			{
				pText = ParseKeyvalue( pText, key, value );
				if ( !strcmpi( key, "}" ) )
				{
					// already in the database, don't add again, override values instead
					const char *pOverride = m_strings.String(prop.m_name);
					int propIndex = GetSurfaceIndex( pOverride );
					if (  propIndex >= 0 )
					{
						CSurface *pSurface = GetInternalSurface( propIndex );
						pSurface->data = prop.data;
						break;
					}

					m_props.AddToTail( prop );
					break;
				}
				else if ( !strcmpi( key, "base" ) )
				{
					baseMaterial = GetSurfaceIndex( value );
					CopyPhysicsProperties( &prop, baseMaterial );
				}
				else if ( !strcmpi( key, "thickness" ) )
				{
					prop.data.physics.thickness = atof(value);
				}
				else if ( !strcmpi( key, "density" ) )
				{
					prop.data.physics.density = atof(value);
				}
				else if ( !strcmpi( key, "elasticity" ) )
				{
					prop.data.physics.elasticity = atof(value);
				}
				else if ( !strcmpi( key, "friction" ) )
				{
					prop.data.physics.friction = atof(value);
				}
				else if ( !strcmpi( key, "maxspeedfactor" ) )
				{
					prop.data.game.maxSpeedFactor = atof(value);
				}
				else if ( !strcmpi( key, "jumpfactor" ) )
				{
					prop.data.game.jumpFactor = atof(value);
				}
				else if ( !strcmpi( key, "climbable" ) )
				{
					prop.data.game.climbable = atoi(value);
				}
				// audio parameters
				else if ( !strcmpi( key, "audioReflectivity" ) )
				{
					prop.data.audio.reflectivity = atof(value);
				}
				else if ( !strcmpi( key, "audioHardnessFactor" ) )
				{
					prop.data.audio.hardnessFactor = atof(value);
				}
				else if ( !strcmpi( key, "audioHardMinVelocity" ) )
				{
					prop.data.audio.hardVelocityThreshold = atof(value);
				}
				else if ( !strcmpi( key, "audioRoughnessFactor" ) )
				{
					prop.data.audio.roughnessFactor = atof(value);
				}
				else if ( !strcmpi( key, "scrapeRoughThreshold" ) )
				{
					prop.data.audio.roughThreshold = atof(value);
				}
				else if ( !strcmpi( key, "impactHardThreshold" ) )
				{
					prop.data.audio.hardThreshold = atof(value);
				}
				// sound names
				else if ( !strcmpi( key, "stepleft" ) )
				{
					prop.data.sounds.stepleft = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "stepright" ) )
				{
					prop.data.sounds.stepright = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "impactsoft" ) )
				{
					prop.data.sounds.impactSoft = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "impacthard" ) )
				{
					prop.data.sounds.impactHard = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "scrapesmooth" ) )
				{
					prop.data.sounds.scrapeSmooth = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "scraperough" ) )
				{
					prop.data.sounds.scrapeRough = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "bulletimpact" ) )
				{
					prop.data.sounds.bulletImpact = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "break" ) )
				{
					prop.data.sounds.breakSound = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "strain" ) )
				{
					prop.data.sounds.strainSound = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "rolling" ) )
				{
					prop.data.sounds.rolling = m_strings.AddString( value );
				}
				else if ( !strcmpi( key, "gamematerial" ) )
				{
					if ( strlen(value) == 1 && !V_isdigit( value[0]) )
					{
						prop.data.game.material = toupper(value[0]);
					}
					else
					{
						prop.data.game.material = atoi(value);
					}
				}
				else if ( !strcmpi( key, "dampening" ) )
				{
					prop.data.physics.dampening = atof(value);
				}
				else
				{
					// force a breakpoint
					AssertMsg2( 0, "Bad surfaceprop key %s (%s)\n", key, value );
				}
			} while (pText);
		}
	} while (pText);

	if ( !m_init )
	{
		m_init = true;
		//AddReservedMaterials
		CSurface prop;
		
		int baseMaterial = GetSurfaceIndex( "default" );
		memset( &prop.data, 0, sizeof(prop.data) );
		prop.m_name = m_strings.AddString( GetReservedMaterialName(MATERIAL_INDEX_SHADOW) );
		CopyPhysicsProperties( &prop, baseMaterial );
		prop.data.physics.elasticity = 1e-3f;
		prop.data.physics.friction = 0.8f;
		m_shadowFallback = m_props.AddToTail( prop );
	}
	return m_props.Size();
}


void CPhysicsSurfaceProps::SetWorldMaterialIndexTable( int *pMapArray, int mapSize )
{
	m_ivpManager.SetPropMap( pMapArray, mapSize );
}

CIVPMaterialManager::CIVPMaterialManager( void ) : IVP_Material_Manager( IVP_FALSE )
{
	// by default every index maps to itself (NULL translation)
	for ( int i = 0; i < ARRAYSIZE(m_propMap); i++ )
	{
		m_propMap[i] = i;
	}
}

int CIVPMaterialManager::RemapIVPMaterialIndex( int ivpMaterialIndex ) const
{
	if ( ivpMaterialIndex > 127 )
		return ivpMaterialIndex;
	
	return m_propMap[ivpMaterialIndex];
}

// remap the incoming (from IVP) index and get the appropriate material
// note that ivp will only supply indices between 1 and 127
IVP_Material *CIVPMaterialManager::get_material_by_index(IVP_Real_Object *pObject, const IVP_U_Point *world_position, int index)
{
	IVP_Material *tmp = m_props->GetIVPMaterial( RemapIVPMaterialIndex(index) );
	Assert(tmp);
	if ( tmp )
	{
		return tmp;
	}
	else
	{
		return m_props->GetIVPMaterial( m_props->GetSurfaceIndex( "default" ) );
	}
}

// Installs a LUT for remapping IVP material indices to physprop indices
// A table of the names of the materials in index order is stored with the 
// compiled bsp file.  This is then remapped dynamically without touching the
// per-triangle indices on load.  If we wanted to support multiple LUTs, it would
// be better to preprocess/remap the triangles in the collision models at load time
void CIVPMaterialManager::SetPropMap( int *map, int mapSize )
{
	// ??? just ignore any extra bits
	if ( mapSize > 128 )
	{
		mapSize = 128;
	}

	for ( int i = 0; i < mapSize; i++ )
	{
		m_propMap[i] = (unsigned short)map[i];
	}
}
