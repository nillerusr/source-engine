#include "utlvector.h"
#include "materialsystem/imaterialsystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imesh.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/deformations.h"
#include "meshgl.h"
#include "shaderapigl.h"
#include "shaderapidevicegl.h"
#include "shadershadowgl.h"

//-----------------------------------------------------------------------------
// The main GL Shader util interface
//-----------------------------------------------------------------------------
IShaderUtil* g_pShaderUtil;

//-----------------------------------------------------------------------------
// Factory to return from SetMode
//-----------------------------------------------------------------------------
static void* ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_INTERFACE_VERSION ) )
		return static_cast< IShaderDevice* >( &s_ShaderDeviceGL );
	if ( !Q_stricmp( pInterfaceName, SHADERAPI_INTERFACE_VERSION ) )
		return static_cast< IShaderAPI* >( &g_ShaderAPIGL );
	if ( !Q_stricmp( pInterfaceName, SHADERSHADOW_INTERFACE_VERSION ) )
		return static_cast< IShaderShadow* >( &g_ShaderShadow );

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//
// Shader device empty
//
//-----------------------------------------------------------------------------
void CShaderDeviceGL::GetWindowSize( int &width, int &height ) const
{
	width = 0;
	height = 0;
}

void CShaderDeviceGL::GetBackBufferDimensions( int& width, int& height ) const
{
	width = 1600;
	height = 900;
}

// Use this to spew information about the 3D layer 
void CShaderDeviceGL::SpewDriverInfo() const
{
	Warning("GL shader\n");
}

// Creates/ destroys a child window
bool CShaderDeviceGL::AddView( void* hwnd )
{
	return true;
}

void CShaderDeviceGL::RemoveView( void* hwnd )
{
}

// Activates a view
void CShaderDeviceGL::SetView( void* hwnd )
{
}

void CShaderDeviceGL::ReleaseResources()
{
}

void CShaderDeviceGL::ReacquireResources()
{
}

// Creates/destroys Mesh
IMesh* CShaderDeviceGL::CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	return &m_Mesh;
}

void CShaderDeviceGL::DestroyStaticMesh( IMesh* mesh )
{
}

// Creates/destroys static vertex + index buffers
IVertexBuffer *CShaderDeviceGL::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup )
{
	return ( type == SHADER_BUFFER_TYPE_STATIC || type == SHADER_BUFFER_TYPE_STATIC_TEMP ) ? &m_Mesh : &m_DynamicMesh;
}

void CShaderDeviceGL::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{

}

IIndexBuffer *CShaderDeviceGL::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup )
{
	switch( bufferType )
	{
	case SHADER_BUFFER_TYPE_STATIC:
	case SHADER_BUFFER_TYPE_STATIC_TEMP:
		return &m_Mesh;
	default:
		Assert( 0 );
	case SHADER_BUFFER_TYPE_DYNAMIC:
	case SHADER_BUFFER_TYPE_DYNAMIC_TEMP:
		return &m_DynamicMesh;
	}
}

void CShaderDeviceGL::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{

}

IVertexBuffer *CShaderDeviceGL::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	return &m_DynamicMesh;
}

IIndexBuffer *CShaderDeviceGL::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	return &m_Mesh;
}





//-----------------------------------------------------------------------------
//
// CShaderDeviceMgrGL
//
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrGL::Connect( CreateInterfaceFn factory )
{
	// So others can access it
	g_pShaderUtil = (IShaderUtil*)factory( SHADER_UTIL_INTERFACE_VERSION, NULL );

	return true;
}

void CShaderDeviceMgrGL::Disconnect()
{
	g_pShaderUtil = NULL;
}

void *CShaderDeviceMgrGL::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) )
		return static_cast< IShaderDeviceMgr* >( this );
	if ( !Q_stricmp( pInterfaceName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) )
		return static_cast< IMaterialSystemHardwareConfig* >( &g_ShaderAPIGL );
	return NULL;
}

InitReturnVal_t CShaderDeviceMgrGL::Init()
{
	return INIT_OK;
}

void CShaderDeviceMgrGL::Shutdown()
{

}

// Sets the adapter
bool CShaderDeviceMgrGL::SetAdapter( int nAdapter, int nFlags )
{
	return true;
}

// FIXME: Is this a public interface? Might only need to be private to shaderapi
CreateInterfaceFn CShaderDeviceMgrGL::SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode ) 
{
	return ShaderInterfaceFactory;
}

// Gets the number of adapters...
int	 CShaderDeviceMgrGL::GetAdapterCount() const
{
	return 0;
}

bool CShaderDeviceMgrGL::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pKeyValues ) 
{
	return true;
}

// Returns info about each adapter
void CShaderDeviceMgrGL::GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
{
	memset( &info, 0, sizeof( info ) );
	info.m_nDXSupportLevel = 90;
}

// Returns the number of modes
int	 CShaderDeviceMgrGL::GetModeCount( int nAdapter ) const
{
	return 0;
}

// Returns mode information..
void CShaderDeviceMgrGL::GetModeInfo( ShaderDisplayMode_t *pInfo, int nAdapter, int nMode ) const
{
}

void CShaderDeviceMgrGL::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const
{
}