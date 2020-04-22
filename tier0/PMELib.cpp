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


    //-------------------------------------------------------------------
    // We have successfully opened the device driver, get the family
    // of the processor.
    //-------------------------------------------------------------------



    //-------------------------------------------------------------------
    // We need to write to counter 0 on the pro family to enable both
    // of the performance counters. We write to both so they start in a
    // known state. For the pentium this is not necessary.
    //-------------------------------------------------------------------
    if (vendor == INTEL && version.Family == PENTIUMPRO_FAMILY)
    {
        SelectP5P6PerformanceEvent(P6_CLOCK, 0, TRUE, TRUE);
        SelectP5P6PerformanceEvent(P6_CLOCK, 1, TRUE, TRUE);
    }

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
// Select the event to monitor with counter 0
//
HRESULT PME::SelectP5P6PerformanceEvent(uint32 dw_event, uint32 dw_counter,
								  bool b_user, bool b_kernel)
{
	HRESULT	hr = S_OK;

	if (dw_counter>1)		// is the counter valid
		return E_BAD_COUNTER;

	if (bDriverOpen == false)				// driver is not going
		return E_DRIVER_NOT_OPEN;

	if ( ((dw_event>>28)&0xF) != (uint32)version.Family)
	{
		return E_ILLEGAL_OPERATION;		// this operation is not for this processor
	}

	if ( (((dw_event & 0x300)>>8) & (dw_counter+1)) == 0 )
	{
		return E_ILLEGAL_OPERATION;		// this operation is not for this counter
	}

    switch(version.Family)
    {
    case PENTIUM_FAMILY:
        {
            uint64	i64_cesr;
            int	i_kernel_bit,i_user_bit;
            BYTE u1_event = (BYTE)((dw_event & (0x3F0000))>>16);

            if (dw_counter==0)		// the kernel and user mode bits depend on
            {						// counter being used.
                i_kernel_bit = 6;
                i_user_bit = 7;
            }
            else
            {
                i_kernel_bit = 22;
                i_user_bit = 23;
            }

            ReadMSR(0x11, &i64_cesr);	// get current P5 event select (cesr)

            // top 32bits of cesr are not valid so ignore them
            i64_cesr &= ((dw_counter == 0)?0xffff0000:0x0000ffff); 
            WriteMSR(0x11,i64_cesr); 				// stop the counter
            WriteMSR((dw_counter==0)?0x12:0x13,0ui64);	// clear the p.counter

            // set the user and kernel mode bits
            i64_cesr |= ( b_user?(1<<7):0 ) | ( b_kernel?(1<<6):0 );

            // is this the special P5 value that signals count clocks??
            if (u1_event == 0x3f)
            {
                WriteMSR(0x11, i64_cesr|0x100);	// Count clocks
            }
            else
            {
                WriteMSR(0x11, i64_cesr|u1_event);	// Count events
            }

        }
        break;

    case PENTIUMPRO_FAMILY:

        {
            BYTE u1_event = (BYTE)((dw_event & (0xFF0000))>>16);
            BYTE u1_mask = (BYTE)((dw_event & 0xFF));

            // Event select 0 and 1 are identical.
            hr = WriteMSR((dw_counter==0)?0x186:0x187,
                
                
                uint64((u1_event | (b_user?(1<<16):0) | (b_kernel?(1<<17):0) | (1<<22) | (1<<18) | (u1_mask<<8)) ) 
                );
        }
        break;

    case PENTIUM4_FAMILY:
        // use the p4 path
        break;

    default:
		return E_UNKNOWN_CPU;
	}

	return hr;
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



const unsigned short cccr_escr_map[NCOUNTERS][8] = 
{
      {       
      0x3B2,
      0x3B4,
      0x3AA,
      0x3B6,
      0x3AC,
      0x3C8,
      0x3A2,
      0x3A0,
      },
      {   
      0x3B2,
      0x3B4,
      0x3AA,
      0x3B6,
      0x3AC,
      0x3C8, 
      0x3A2,
      0x3A0,
      },
      {       
      0x3B3,
      0x3B5,
      0x3AB,
      0x3B7,
      0x3AD,
      0x3C9, 
      0x3A3,
      0x3A1,
      },
      {   
      0x3B3,
      0x3B5,
      0x3AB,
      0x3B7,
      0x3AD,
      0x3C9, 
      0x3A3,
      0x3A1,
      },
      {       
          
      0x3C0,
      0x3C4, 
      0x3C2,
      },
      {   
      0x3C0,
      0x3C4, 
      0x3C2,
      },
      {       
      0x3C1,
      0x3C5, 
      0x3C3,
      },
      {   
      0x3C1,
      0x3C5,
      0x3C3,
      },
      {       
      0x3A6,
      0x3A4,
      0x3AE,
      0x3B0, 
      0,
      0x3A8,
      },
      {   
      0x3A6,
      0x3A4,
      0x3AE,
      0x3B0, 
      0,
      0x3A8,
      },
      {       
        
      0x3A7,
      0x3A5,
      0x3AF,
      0x3B1, 
      0,
      0x3A9,
      },
      {   
          
      0x3A7,
      0x3A5,
      0x3AF,
      0x3B1, 
      0,
      0x3A9,
      },
      {       

      0x3BA,
      0x3CA, 
      0x3BC,
      0x3BE,
      0x3B8,
      0x3CC,
      0x3E0,
      },
      {   

      0x3BA,
      0x3CA, 
      0x3BC,
      0x3BE,
      0x3B8,
      0x3CC,
      0x3E0,
      },
      {       

      0x3BB,
      0x3CB, 
      0x3BD,
      0,
      0x3B9,
      0x3CD,
      0x3E1,
      },
      {   
          

      0x3BB,
      0x3CB, 
      0x3BD,
      0,
      0x3B9,
      0x3CD,
      0x3E1,
      },
      {       
      0x3BA,
      0x3CA, 
      0x3BC,
      0x3BE,
      0x3B8,
      0x3CC,
      0x3E0,
      },
      {    

      0x3BB,
      0x3CB,
      0x3BD,
      0,
      0x3B9,
      0x3CD,
      0x3E1,
      },
};

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

