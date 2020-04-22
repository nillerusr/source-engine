//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "inputsystem.h"
#include "key_translation.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "tier1/convar.h"

typedef void*(*NovintGetIHaptics_t)(void);
typedef bool (*NovintAttemptHWND_t)(void *hWnd);
typedef void (*NovintDisableHWND_t)(void);
typedef void (*NovintPollDevices_t)(void);
typedef bool (*NovintButtonState_t)(int nDevice,
									int &down, 
									int &pressed, 
									int &released);
typedef int	 (*NovintDeviceCount_t)();
typedef int	 (*NovintButtonCount_t)(int nDevice);
typedef bool (__cdecl *NovintMouseModeCallback_t) (void);
typedef void (*NovintInputActive_t)(bool bGameInput);
typedef __int64 (*NovintGetDeviceID_t)(int nDevice);

NovintGetIHaptics_t NovintGetIHaptics = NULL;
NovintAttemptHWND_t NovintAttemptHWND = NULL;
NovintDisableHWND_t NovintDisableHWND = NULL;
NovintPollDevices_t NovintPollDevices = NULL;
NovintButtonState_t NovintButtonState = NULL;
NovintDeviceCount_t NovintDeviceCount = NULL;
NovintButtonCount_t NovintButtonCount = NULL;
NovintInputActive_t NovintInputActive = NULL;
NovintGetDeviceID_t NovintGetDeviceID = NULL;

static CInputSystem *pNovintInputSystem = 0;

static bool bNovintPure = false;

bool __cdecl NovintDevicesInputMode(void)
{
	if(pNovintInputSystem)
	{
		return !bNovintPure;
	}
	return false;
}

void *CInputSystem::GetHapticsInterfaceAddress(void)const
{
	if( NovintGetIHaptics )
		return NovintGetIHaptics();

	return NULL;
}

void CInputSystem::AttachWindowToNovintDevices( void * hWnd )
{
	if( NovintAttemptHWND )
		NovintAttemptHWND( hWnd );
}

void CInputSystem::DetachWindowFromNovintDevices(void)
{
	if( NovintDisableHWND )
		NovintDisableHWND();
}

void CInputSystem::InitializeNovintDevices()
{
	pNovintInputSystem = this;
 	// assume no novint devices
	m_bNovintDevices = false;
	m_nNovintDeviceCount = 0; 

	if(!m_pNovintDLL)
		return;

	NovintGetIHaptics = (NovintGetIHaptics_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintGetIHaptics" );
	NovintAttemptHWND = (NovintAttemptHWND_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintAttemptHWND" );
	NovintDisableHWND = (NovintDisableHWND_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintDisableHWND" );
	NovintPollDevices = (NovintPollDevices_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintPollDevices" );
	NovintButtonState = (NovintButtonState_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintButtonState" );
	NovintDeviceCount = (NovintDeviceCount_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintDeviceCount" );
	NovintButtonCount = (NovintButtonCount_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintButtonCount" );
	NovintGetDeviceID = (NovintGetDeviceID_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintGetDeviceID" );
	NovintInputActive = (NovintInputActive_t)GetProcAddress( (HMODULE)m_pNovintDLL, "NovintInputActive" );


	m_nNovintDeviceCount = NovintDeviceCount();

	if( m_nNovintDeviceCount > 0 )
		m_bNovintDevices = true;
}

void CInputSystem::PollNovintDevices()
{
	if( NovintPollDevices )
		NovintPollDevices();

	for ( int i = 0; i < m_nNovintDeviceCount; i++ )
	{
		UpdateNovintDeviceButtonState(i);
	}
}

void CInputSystem::UpdateNovintDeviceButtonState(int nDevice)
{
	int nDown;
	int nPressed;
	int nReleased;
	if(NovintButtonState(nDevice, nDown, nPressed, nReleased))
	{
		for ( int i = 0; i < 4; i++ )
		{
			ButtonCode_t code = (ButtonCode_t)( NOVINT_FIRST + ( nDevice * 4 ) + i );
			
			if( nPressed & (1 << i) )
			{
				PostButtonPressedEvent(IE_ButtonPressed, m_nLastSampleTick, code, KEY_NONE);
			}

			if( nReleased & (1 << i) )
			{
				PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, code, KEY_NONE);
			}
		}
	}
}

void CInputSystem::SetNovintPure( bool bPure )
{
	bNovintPure = bPure;
	if(NovintInputActive)
		NovintInputActive(bNovintPure);

}