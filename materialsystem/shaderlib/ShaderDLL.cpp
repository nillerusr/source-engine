//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "shaderlib/ShaderDLL.h"
#include "materialsystem/IShader.h"
#include "tier1/utlvector.h"
#include "tier0/dbg.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "IShaderSystem.h"
#include "materialsystem/ishaderapi.h"
#include "shaderlib_cvar.h"
#include "mathlib/mathlib.h"
#include "tier1/tier1.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// The standard implementation of CShaderDLL
//-----------------------------------------------------------------------------
class CShaderDLL : public IShaderDLLInternal, public IShaderDLL
{
public:
	CShaderDLL();

	// methods of IShaderDLL
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual int ShaderCount() const;
	virtual IShader *GetShader( int nShader );

	// methods of IShaderDLLInternal
	virtual bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem );
	virtual void Disconnect( bool bIsMaterialSystem );
	virtual void InsertShader( IShader *pShader );

private:
	CUtlVector< IShader * >	m_ShaderList;
};


//-----------------------------------------------------------------------------
// Global interfaces/structures
//-----------------------------------------------------------------------------
IMaterialSystemHardwareConfig* g_pHardwareConfig;
const MaterialSystem_Config_t *g_pConfig;


//-----------------------------------------------------------------------------
// Interfaces/structures local to shaderlib
//-----------------------------------------------------------------------------
IShaderSystem* g_pSLShaderSystem;


// Pattern necessary because shaders register themselves in global constructors
static CShaderDLL *s_pShaderDLL;


//-----------------------------------------------------------------------------
// Global accessor
//-----------------------------------------------------------------------------
IShaderDLL *GetShaderDLL()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
	{
		s_pShaderDLL = new CShaderDLL;
	}

	return s_pShaderDLL;
}

IShaderDLLInternal *GetShaderDLLInternal()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
	{
		s_pShaderDLL = new CShaderDLL;
	}

	return static_cast<IShaderDLLInternal*>( s_pShaderDLL );
}

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
EXPOSE_INTERFACE_FN( (InstantiateInterfaceFn)GetShaderDLLInternal, IShaderDLLInternal, SHADER_DLL_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
CShaderDLL::CShaderDLL()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
}


//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
bool CShaderDLL::Connect( CreateInterfaceFn factory, bool bIsMaterialSystem )
{
	g_pHardwareConfig =  (IMaterialSystemHardwareConfig*)factory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, NULL );
	g_pConfig = (const MaterialSystem_Config_t*)factory( MATERIALSYSTEM_CONFIG_VERSION, NULL );
	g_pSLShaderSystem =  (IShaderSystem*)factory( SHADERSYSTEM_INTERFACE_VERSION, NULL );

	if ( !bIsMaterialSystem )
	{
		ConnectTier1Libraries( &factory, 1 );
  		InitShaderLibCVars( factory );
	}

	return ( g_pConfig != NULL ) && (g_pHardwareConfig != NULL) && ( g_pSLShaderSystem != NULL );
}

void CShaderDLL::Disconnect( bool bIsMaterialSystem )
{
	if ( !bIsMaterialSystem )
	{
		ConVar_Unregister();
		DisconnectTier1Libraries();
	}

	g_pHardwareConfig = NULL;
	g_pConfig = NULL;
	g_pSLShaderSystem = NULL;
}

bool CShaderDLL::Connect( CreateInterfaceFn factory )
{
	return Connect( factory, false );
}

void CShaderDLL::Disconnect()
{
	Disconnect( false );
}


//-----------------------------------------------------------------------------
// Iterates over all shaders
//-----------------------------------------------------------------------------
int CShaderDLL::ShaderCount() const
{
	return m_ShaderList.Count();
}

IShader *CShaderDLL::GetShader( int nShader ) 
{
	if ( ( nShader < 0 ) || ( nShader >= m_ShaderList.Count() ) )
		return NULL;

	return m_ShaderList[nShader];
}


//-----------------------------------------------------------------------------
// Adds to the shader lists
//-----------------------------------------------------------------------------
void CShaderDLL::InsertShader( IShader *pShader )
{
	Assert( pShader );
	m_ShaderList.AddToTail( pShader );
}

