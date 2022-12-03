//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifdef _WIN32
#include <windows.h>

#pragma warning( disable : 4530 )   // warning: exception handler -GX option

#include "tier0/valve_off.h"
#include "tier0/pmelib.h"
#if _MSC_VER >=1300
#else
#include "winioctl.h"
#endif
#include "tier0/valve_on.h"

#include "tier0/ioctlcodes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


PME* PME::_singleton = 0;

// Single interface.
PME* PME::Instance()
{
   if (_singleton == 0)
   {
      _singleton = new PME;
   }      
   return _singleton;
}    

//---------------------------------------------------------------------------
// Open the device driver and detect the processor
//---------------------------------------------------------------------------
HRESULT PME::Init( void )
{
    OSVERSIONINFO	OS;

    if ( bDriverOpen )
        return E_DRIVER_ALREADY_OPEN;

    switch( vendor )
    {
    case INTEL:
    case AMD:
        break;
    default:
        bDriverOpen = FALSE;		// not an Intel or Athlon processor so return false
        return E_UNKNOWN_CPU_VENDOR;
    }

    //-----------------------------------------------------------------------
    // Get the operating system version
    //-----------------------------------------------------------------------
    OS.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
    GetVersionEx( &OS );

    if ( OS.dwPlatformId == VER_PLATFORM_WIN32_NT )
    {
        hFile = CreateFile(						// WINDOWS NT
            "\\\\.\\GDPERF",
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }
    else
    {
        hFile = CreateFile(						// WINDOWS 95
            "\\\\.\\GDPERF.VXD",
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

    if (hFile == INVALID_HANDLE_VALUE )
        return E_CANT_OPEN_DRIVER;


    bDriverOpen = TRUE;

    return S_OK;
}

//---------------------------------------------------------------------------
// Close the device driver
//---------------------------------------------------------------------------
HRESULT PME::Close(void)
{
	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

    bDriverOpen = false;

	if (hFile)					// if we have no driver handle, return FALSE
	{
        BOOL result = CloseHandle(hFile);

        hFile = NULL;
		return result ? S_OK : HRESULT_FROM_WIN32( GetLastError() );
	}  
    else
	    return E_DRIVER_NOT_OPEN;


}

//---------------------------------------------------------------------------
// Read model specific register
//---------------------------------------------------------------------------
HRESULT PME::ReadMSR(uint32 dw_reg, int64 * pi64_value)
{
	DWORD	dw_ret_len;

	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

	BOOL result = DeviceIoControl
	(
		hFile,						// Handle to device
		(DWORD) IOCTL_READ_MSR,		// IO Control code for Read
		&dw_reg,					// Input Buffer to driver.
		sizeof(uint32),				// Length of input buffer.
		pi64_value,					// Output Buffer from driver.
		sizeof(int64),			// Length of output buffer in bytes.
		&dw_ret_len,				// Bytes placed in output buffer.
		NULL						// NULL means wait till op. completes
	);

	HRESULT hr = result ? S_OK : HRESULT_FROM_WIN32( GetLastError() );
	if (hr == S_OK && dw_ret_len != sizeof(int64))
		hr = E_BAD_DATA;

	return hr;
}

HRESULT PME::ReadMSR(uint32 dw_reg, uint64 * pi64_value)
{
	DWORD	dw_ret_len;

	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

	BOOL result = DeviceIoControl
	(
		hFile,						// Handle to device
		(DWORD) IOCTL_READ_MSR,		// IO Control code for Read
		&dw_reg,					// Input Buffer to driver.
		sizeof(uint32),				// Length of input buffer.
		pi64_value,					// Output Buffer from driver.
		sizeof(uint64),			    // Length of output buffer in bytes.
		&dw_ret_len,				// Bytes placed in output buffer.
		NULL						// NULL means wait till op. completes
	);

	HRESULT hr = result ? S_OK : HRESULT_FROM_WIN32( GetLastError() );
	if (hr == S_OK && dw_ret_len != sizeof(uint64))
		hr = E_BAD_DATA;

	return hr;
}

//---------------------------------------------------------------------------
// Write model specific register
//---------------------------------------------------------------------------
HRESULT PME::WriteMSR(uint32 dw_reg, const int64 & i64_value)
{
	DWORD	dw_buffer[3];
	DWORD	dw_ret_len;

	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

	dw_buffer[0]				= dw_reg;			// setup the 12 byte input
	*((int64*)(&dw_buffer[1]))= i64_value;

	BOOL result = DeviceIoControl
	(
		hFile,						// Handle to device
		(DWORD) IOCTL_WRITE_MSR,	// IO Control code for Read
		dw_buffer,					// Input Buffer to driver.
		12,							// Length of Input buffer
		NULL,						// Buffer from driver, None for WRMSR
		0,							// Length of output buffer in bytes.
		&dw_ret_len,			// Bytes placed in DataBuffer.
		NULL					  	// NULL means wait till op. completes.
	);

	HRESULT hr = result ? S_OK : HRESULT_FROM_WIN32( GetLastError() );
	if (hr == S_OK && dw_ret_len != 0)
		hr = E_BAD_DATA;

	return hr;
}



HRESULT PME::WriteMSR(uint32 dw_reg, const uint64 & i64_value)
{
	DWORD	dw_buffer[3];
	DWORD	dw_ret_len;

	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

	dw_buffer[0]				= dw_reg;			// setup the 12 byte input
	*((uint64*)(&dw_buffer[1]))= i64_value;

	BOOL result = DeviceIoControl
	(
		hFile,						// Handle to device
		(DWORD) IOCTL_WRITE_MSR,	// IO Control code for Read
		dw_buffer,					// Input Buffer to driver.
		12,							// Length of Input buffer
		NULL,						// Buffer from driver, None for WRMSR
		0,							// Length of output buffer in bytes.
		&dw_ret_len,			// Bytes placed in DataBuffer.
		NULL					  	// NULL means wait till op. completes.
	);

    //E_POINTER
	HRESULT hr = result ? S_OK : HRESULT_FROM_WIN32( GetLastError() );
	if (hr == S_OK && dw_ret_len != 0)
		hr = E_BAD_DATA;

	return hr;
}













#pragma hdrstop




//---------------------------------------------------------------------------
// Return the frequency of the processor in Hz.
//

double PME::GetCPUClockSpeedFast(void)
{
	int64	i64_perf_start, i64_perf_freq, i64_perf_end;
	int64	i64_clock_start,i64_clock_end;
	double d_loop_period, d_clock_freq;

	//-----------------------------------------------------------------------
	// Query the performance of the Windows high resolution timer.
	//-----------------------------------------------------------------------
	QueryPerformanceFrequency((LARGE_INTEGER*)&i64_perf_freq);

	//-----------------------------------------------------------------------
	// Query the current value of the Windows high resolution timer.
	//-----------------------------------------------------------------------
	QueryPerformanceCounter((LARGE_INTEGER*)&i64_perf_start);
	i64_perf_end = 0;

	//-----------------------------------------------------------------------
	// Time of loop of 250000 windows cycles with RDTSC
	//-----------------------------------------------------------------------
	RDTSC(i64_clock_start);
	while(i64_perf_end<i64_perf_start+250000)
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&i64_perf_end);
	}
	RDTSC(i64_clock_end);

	//-----------------------------------------------------------------------
	// Caclulate the frequency of the RDTSC timer and therefore calculate
	// the frequency of the processor.
	//-----------------------------------------------------------------------
	i64_clock_end -= i64_clock_start;

	d_loop_period = ((double)(i64_perf_freq)) / 250000.0;
	d_clock_freq = ((double)(i64_clock_end & 0xffffffff))*d_loop_period;

	return (float)d_clock_freq;
}



// takes 1 second
double PME::GetCPUClockSpeedSlow(void)
{

    if (m_CPUClockSpeed != 0)
        return m_CPUClockSpeed;

    unsigned long start_ms, stop_ms;
    unsigned long start_tsc,stop_tsc;

    // boosting priority helps with noise. its optional and i dont think
    //  it helps all that much

    PME * pme = PME::Instance();

    pme->SetProcessPriority(ProcessPriorityHigh);

    // wait for millisecond boundary
    start_ms = GetTickCount() + 5;
    while (start_ms <= GetTickCount());

    // read timestamp (you could use QueryPerformanceCounter in hires mode if you want)
#ifdef COMPILER_MSVC64 
    RDTSC(start_tsc);
#else
    __asm
    {
        rdtsc
        mov dword ptr [start_tsc+0],eax
        mov dword ptr [start_tsc+4],edx
    }
#endif

    // wait for end
    stop_ms = start_ms + 1000; // longer wait gives better resolution
    while (stop_ms > GetTickCount());

    // read timestamp (you could use QueryPerformanceCounter in hires mode if you want)
#ifdef COMPILER_MSVC64
    RDTSC(stop_tsc);
#else
    __asm
    {
        rdtsc
        mov dword ptr [stop_tsc+0],eax
        mov dword ptr [stop_tsc+4],edx
    }
#endif


    // normalize priority
    pme->SetProcessPriority(ProcessPriorityNormal);

    // return clock speed
    //  optionally here you could round to known clocks, like speeds that are multimples
    //  of 100, 133, 166, etc.
    m_CPUClockSpeed =  ((stop_tsc - start_tsc) * 1000.0) / (double)(stop_ms - start_ms);
    return m_CPUClockSpeed;

}

#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void PME::Validate( CValidator &validator, tchar *pchName )
{
	validator.Push( _T("PME"), this, pchName );

	validator.ClaimMemory( this );

	validator.ClaimMemory( cache );

	validator.ClaimMemory( ( void * ) vendor_name.c_str( ) );
	validator.ClaimMemory( ( void * ) brand.c_str( ) );

	validator.Pop( );
}
#endif // DBGFLAG_VALIDATE

#pragma warning( default : 4530 )   // warning: exception handler -GX option
#endif

