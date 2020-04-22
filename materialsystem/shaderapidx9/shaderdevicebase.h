//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERDEVICEBASE_H
#define SHADERDEVICEBASE_H

#ifdef _WIN32
#pragma once
#endif


#include "togl/rendermechanism.h"
#include "shaderapi/IShaderDevice.h"
#include "IHardwareConfigInternal.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterialsystem.h"
#include "hardwareconfig.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class KeyValues;


//-----------------------------------------------------------------------------
// define this if you want to run with NVPERFHUD
//-----------------------------------------------------------------------------
//#define NVPERFHUD 1


//-----------------------------------------------------------------------------
// Uncomment this to activate the reference rasterizer
//-----------------------------------------------------------------------------
//#define USE_REFERENCE_RASTERIZER 1

//-----------------------------------------------------------------------------
// Uncomment to check for -nulldevice on command line and use D3DDEVTYPE_NULLREF.
//-----------------------------------------------------------------------------
#define ENABLE_NULLREF_DEVICE_SUPPORT

//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrBase : public IShaderDeviceMgr
{
public:
	// constructor, destructor
	CShaderDeviceMgrBase();
	virtual ~CShaderDeviceMgrBase();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

	// Methods of IShaderDeviceMgr
	virtual bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pCongifuration );
	virtual void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func );
	virtual void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func );

	// Reads in the hardware caps from the dxsupport.cfg file
	void ReadHardwareCaps( HardwareCaps_t &caps, int nDxLevel );

	// Reads in the max + preferred DX support level
	void ReadDXSupportLevels( HardwareCaps_t &caps );

	// Returns the hardware caps for a particular adapter
	const HardwareCaps_t& GetHardwareCaps( int nAdapter ) const;

	// Invokes mode change callbacks
	void InvokeModeChangeCallbacks();

	// Factory to return from SetMode
	static void* ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode );

	// Returns only valid dx levels
	int GetClosestActualDXLevel( int nDxLevel ) const;

protected:
	struct AdapterInfo_t
	{
		HardwareCaps_t m_ActualCaps;
	};

private:
	// Reads in the dxsupport.cfg keyvalues
	KeyValues *ReadDXSupportKeyValues();

	// Reads in ConVars + config variables
	void LoadConfig( KeyValues *pKeyValues, KeyValues *pConfiguration );

	// Loads the hardware caps, for cases in which the D3D caps lie or where we need to augment the caps
	void LoadHardwareCaps( KeyValues *pGroup, HardwareCaps_t &caps );

	// Gets the recommended configuration associated with a particular dx level
	bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, int nVendorID, int nDeviceID, KeyValues *pConfiguration );

	// Returns the amount of video memory in bytes for a particular adapter
	virtual int GetVidMemBytes( int nAdapter ) const = 0;

	// Looks for override keyvalues in the dxsupport cfg keyvalues
	KeyValues *FindDXLevelSpecificConfig( KeyValues *pKeyValues, int nDxLevel );
	KeyValues *FindDXLevelAndVendorSpecificConfig( KeyValues *pKeyValues, int nDxLevel, int nVendorID );
	KeyValues *FindCPUSpecificConfig( KeyValues *pKeyValues, int nCPUMhz, bool bAMD );
	KeyValues *FindMemorySpecificConfig( KeyValues *pKeyValues, int nSystemRamMB );
	KeyValues *FindVidMemSpecificConfig( KeyValues *pKeyValues, int nVideoRamMB );
	KeyValues *FindCardSpecificConfig( KeyValues *pKeyValues, int nVendorID, int nDeviceID );

protected:
	// Stores adapter info for all adapters
	CUtlVector<AdapterInfo_t> m_Adapters;

	// Installed mode change callbacks
	CUtlVector< ShaderModeChangeCallbackFunc_t > m_ModeChangeCallbacks;

	KeyValues *m_pDXSupport;
};


//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceBase : public IShaderDevice
{
public:
	enum IPCMessage_t
	{
		RELEASE_MESSAGE		= 0x5E740DE0,
		REACQUIRE_MESSAGE	= 0x5E740DE1,
		EVICT_MESSAGE		= 0x5E740DE2,
	};

	// Methods of IShaderDevice
public:
	virtual ImageFormat GetBackBufferFormat() const;
	virtual int StencilBufferBits() const;
	virtual bool IsAAEnabled() const;
	virtual bool AddView( void* hWnd );
	virtual void RemoveView( void* hWnd );
	virtual void SetView( void* hWnd );
	virtual void GetWindowSize( int& nWidth, int& nHeight ) const;

	// Methods exposed to the rest of shader api
	virtual bool InitDevice( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode ) = 0;
	virtual void ShutdownDevice() = 0;
	virtual bool IsDeactivated() const = 0;

public:
	// constructor, destructor
	CShaderDeviceBase();
	virtual ~CShaderDeviceBase();

	virtual void OtherAppInitializing( bool initializing ) {}
	virtual void EvictManagedResourcesInternal() {}

	void* GetIPCHWnd();
	void SendIPCMessage( IPCMessage_t message );

protected:
	// IPC communication for multiple shaderapi apps
	void InstallWindowHook( void *hWnd );
	void RemoveWindowHook( void *hWnd );
	void SetCurrentThreadAsOwner();
	void RemoveThreadOwner();
	bool ThreadOwnsDevice();

	// Finds a child window
	int  FindView( void* hWnd ) const;

	int m_nAdapter;
	void *m_hWnd;
	void* m_hWndCookie;
	bool m_bInitialized : 1;
	bool m_bIsMinimized : 1;

	// The current view hwnd
	void* m_ViewHWnd;

	int	m_nWindowWidth;
	int m_nWindowHeight;
	uint32 m_dwThreadId;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline void* CShaderDeviceBase::GetIPCHWnd()
{
	return m_hWndCookie;
}


//-----------------------------------------------------------------------------
// Helper class to reduce code related to shader buffers
//-----------------------------------------------------------------------------
template< class T >
class CShaderBuffer : public IShaderBuffer
{
public:
	CShaderBuffer( T *pBlob ) : m_pBlob( pBlob ) {}

	virtual size_t GetSize() const
	{
		return m_pBlob ? m_pBlob->GetBufferSize() : 0;
	}

	virtual const void* GetBits() const
	{
		return m_pBlob ? m_pBlob->GetBufferPointer() : NULL;
	}

	virtual void Release()
	{
		if ( m_pBlob )
		{
			m_pBlob->Release();
		}
		delete this;
	}

private:
	T *m_pBlob;
};



#endif // SHADERDEVICEBASE_H