//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "dme_controls/dmecontrols.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "matsys_controls/matsyscontrols.h"
#include "toolframework/ienginetool.h"
#include "vphysics_interface.h"
#include "dme_controls/inotifyui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace vgui
{

ISoundEmitterSystemBase *g_pSoundEmitterSystem = NULL;
ISoundEmitterSystemBase *SoundEmitterSystem()
{
	return g_pSoundEmitterSystem;
}

IEngineTool *enginetools = NULL;
IEngineTool *EngineTool()
{
	return enginetools;
}

IPhysicsCollision *g_pPhysicsCollision = NULL;
IPhysicsCollision *PhysicsCollision()
{
	return g_pPhysicsCollision;
}

class CDefaultElementPropertiesChoices : public CBaseElementPropertiesChoices
{
public:
};

static CDefaultElementPropertiesChoices s_DefaultChoices;
IElementPropertiesChoices *g_pElementPropertiesChoices = &s_DefaultChoices;
IElementPropertiesChoices *ElementPropertiesChoices()
{
	return g_pElementPropertiesChoices;
}

void SetElementPropertiesChoices( IElementPropertiesChoices *pElementPropertiesChoices )
{
	g_pElementPropertiesChoices = pElementPropertiesChoices ? pElementPropertiesChoices : &s_DefaultChoices;
}


//-----------------------------------------------------------------------------
// Purpose: finds a particular interface in the factory set
//-----------------------------------------------------------------------------
static void *InitializeInterface( char const *interfaceName, CreateInterfaceFn *factoryList, int numFactories )
{
	void *retval;

	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[ i ];
		if ( !factory )
			continue;

		retval = factory( interfaceName, NULL );
		if ( retval )
			return retval;
	}

	// No provider for requested interface!!!
	// Assert( !"No provider for requested interface!!!" );

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the controls
//-----------------------------------------------------------------------------
bool VGui_InitDmeInterfacesList( const char *moduleName, CreateInterfaceFn *factoryList, int numFactories )
{
	if ( !vgui::VGui_InitMatSysInterfacesList( moduleName, factoryList, numFactories ) )
		return false;

	g_pSoundEmitterSystem = (ISoundEmitterSystemBase*)InitializeInterface( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, factoryList, numFactories );
	enginetools = (IEngineTool*)InitializeInterface( VENGINETOOL_INTERFACE_VERSION, factoryList, numFactories );
	g_pPhysicsCollision = (IPhysicsCollision*)InitializeInterface( VPHYSICS_COLLISION_INTERFACE_VERSION, factoryList, numFactories );

	// Can function without either of these
	return true;
}


} // namespace vgui



