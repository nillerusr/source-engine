//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "pch_tier0.h"

#if defined(_WIN32) && !defined(_X360)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_LINUX)
#include <stdlib.h>
#elif defined(OSX)
#include <sys/sysctl.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

const tchar* GetProcessorVendorId();

static bool cpuid(unsigned long function, unsigned long& out_eax, unsigned long& out_ebx, unsigned long& out_ecx, unsigned long& out_edx)
{
#if defined(GNUC)
	asm("mov %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchg %%esi, %%ebx"
		: "=a" (out_eax),
		"=S" (out_ebx),
		"=c" (out_ecx),
		"=d" (out_edx)
		: "a" (function) 
		);
	return true;
#elif defined( _X360 )
	return false;
#elif defined(_WIN64)
	int pCPUInfo[4];
	__cpuid( pCPUInfo, (int)function );
	out_eax = pCPUInfo[0];
	out_ebx = pCPUInfo[1];
	out_ecx = pCPUInfo[2];
	out_edx = pCPUInfo[3];
	return true;
#else
	bool retval = true;
	unsigned long local_eax, local_ebx, local_ecx, local_edx;
	_asm pushad;

	__try
	{
        _asm
		{
			xor edx, edx		// Clue the compiler that EDX is about to be used.
            mov eax, function   // set up CPUID to return processor version and features
								//      0 = vendor string, 1 = version info, 2 = cache info
            cpuid				// code bytes = 0fh,  0a2h
            mov local_eax, eax	// features returned in eax
            mov local_ebx, ebx	// features returned in ebx
            mov local_ecx, ecx	// features returned in ecx
            mov local_edx, edx	// features returned in edx
		}
    } 
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		retval = false; 
	}

	out_eax = local_eax;
	out_ebx = local_ebx;
	out_ecx = local_ecx;
	out_edx = local_edx;

	_asm popad

	return retval;
#endif
}

static bool CheckMMXTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 ) 
	return true;
#else
    unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

    return ( edx & 0x800000 ) != 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: This is a bit of a hack because it appears 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool IsWin98OrOlder()
{
#if defined( _X360 ) || defined( _PS3 ) || defined( POSIX )
	return false;
#else
	bool retval = false;

	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	BOOL bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);
	if( !bOsVersionInfoEx )
	{
		// If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
		
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if ( !GetVersionEx ( (OSVERSIONINFO *) &osvi) )
		{
			Error( _T("IsWin98OrOlder:  Unable to get OS version information") );
		}
	}

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		// NT, XP, Win2K, etc. all OK for SSE
		break;
	case VER_PLATFORM_WIN32_WINDOWS:
		// Win95, 98, Me can't do SSE
		retval = true;
		break;
	case VER_PLATFORM_WIN32s:
		// Can't really run this way I don't think...
		retval = true;
		break;
	default:
		break;
	}

	return retval;
#endif
}


static bool CheckSSETechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return true;
#else
	if ( IsWin98OrOlder() )
	{
		return false;
	}

    unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
	{
		return false;
	}

    return ( edx & 0x2000000L ) != 0;
#endif
}

static bool CheckSSE2Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

    return ( edx & 0x04000000 ) != 0;
#endif
}

bool CheckSSE3Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & 0x00000001 ) != 0;	// bit 1 of ECX
#endif
}

bool CheckSSSE3Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSSE 3 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 9 ) ) != 0;	// bit 9 of ECX
#endif
}

bool CheckSSE41Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.1 is implemented by both Intel and AMD
	// detection is done the same way for both vendors

	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 19 ) ) != 0;	// bit 19 of ECX
#endif
}

bool CheckSSE42Technology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE4.2 is an Intel-only feature

	const char *pchVendor = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( pchVendor, "GenuineIntel" ) )
		return false;

	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 20 ) ) != 0;	// bit 20 of ECX
#endif
}


bool CheckSSE4aTechnology( void )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4a is an AMD-only feature

	const char *pchVendor = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( pchVendor, "AuthenticAMD" ) )
		return false;

	unsigned long eax,ebx,edx,ecx;
	if( !cpuid( 0x80000001,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 6 ) ) != 0;	// bit 6 of ECX
#endif
}


static bool Check3DNowTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax, unused;
    if ( !cpuid(0x80000000,eax,unused,unused,unused) )
		return false;

    if ( eax > 0x80000000L )
    {
     	if ( !cpuid(0x80000001,unused,unused,unused,eax) )
			return false;

		return ( eax & 1<<31 ) != 0;
    }
    return false;
#endif
}

static bool CheckCMOVTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

    return ( edx & (1<<15) ) != 0;
#endif
}

static bool CheckFCMOVTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
    unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

    return ( edx & (1<<16) ) != 0;
#endif
}

static bool CheckRDTSCTechnology(void)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
    if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

    return ( edx & 0x10 ) != 0;
#endif
}

// Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
const tchar* GetProcessorVendorId()
{
#if defined( _X360 ) || defined( _PS3 )
	return "PPC";
#else
	unsigned long unused, VendorIDRegisters[3];

	static tchar VendorID[13];
	
	memset( VendorID, 0, sizeof(VendorID) );
	if ( !cpuid(0,unused, VendorIDRegisters[0], VendorIDRegisters[2], VendorIDRegisters[1] ) )
	{
		if ( IsPC() )
		{
			_tcscpy( VendorID, _T( "Generic_x86" ) ); 
		}
		else if ( IsX360() )
		{
			_tcscpy( VendorID, _T( "PowerPC" ) ); 
		}
	}
	else
	{
		memcpy( VendorID+0, &(VendorIDRegisters[0]), sizeof( VendorIDRegisters[0] ) );
		memcpy( VendorID+4, &(VendorIDRegisters[1]), sizeof( VendorIDRegisters[1] ) );
		memcpy( VendorID+8, &(VendorIDRegisters[2]), sizeof( VendorIDRegisters[2] ) );
	}

	return VendorID;
#endif
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.  This does not mean that 
// Hyper-Threading Technology is necessarily enabled.
static bool HTSupported(void)
{
#if defined( _X360 )
	// not entirtely sure about the semantic of HT support, it being an intel name
	// are we asking about HW threads or HT?
	return true;
#else
	const unsigned int HT_BIT		 = 0x10000000;  // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
	const unsigned int FAMILY_ID     = 0x0f00;      // EAX[11:8] - Bit 11 thru 8 contains family processor id
	const unsigned int EXT_FAMILY_ID = 0x0f00000;	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
	const unsigned int PENTIUM4_ID   = 0x0f00;		// Pentium 4 family processor id

	unsigned long unused,
				  reg_eax = 0, 
				  reg_edx = 0,
				  vendor_id[3] = {0, 0, 0};

	// verify cpuid instruction is supported
	if( !cpuid(0,unused, vendor_id[0],vendor_id[2],vendor_id[1]) 
	 || !cpuid(1,reg_eax,unused,unused,reg_edx) )
	 return false;

	//  Check to see if this is a Pentium 4 or later processor
	if (((reg_eax & FAMILY_ID) ==  PENTIUM4_ID) || (reg_eax & EXT_FAMILY_ID))
		if (vendor_id[0] == 'uneG' && vendor_id[1] == 'Ieni' && vendor_id[2] == 'letn')
			return (reg_edx & HT_BIT) != 0;	// Genuine Intel Processor with Hyper-Threading Technology

	return false;  // This is not a genuine Intel processor.
#endif
}

// Returns the number of logical processors per physical processors.
static uint8 LogicalProcessorsPerPackage(void)
{
#if defined( _X360 )
	return 2;
#else
	// EBX[23:16] indicate number of logical processors per package
	const unsigned NUM_LOGICAL_BITS = 0x00FF0000;

    unsigned long unused, reg_ebx = 0;

	if ( !HTSupported() ) 
		return 1; 

	if ( !cpuid(1,unused,reg_ebx,unused,unused) )
		return 1;

	return (uint8) ((reg_ebx & NUM_LOGICAL_BITS) >> 16);
#endif
}

#if defined(POSIX)
// Move this declaration out of the CalculateClockSpeed() function because
// otherwise clang warns that it is non-obvious whether it is a variable
// or a function declaration: [-Wvexing-parse]
uint64 CalculateCPUFreq(); // from cpu_linux.cpp
#endif

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _WIN32 )
#if !defined( _X360 )
	LARGE_INTEGER waitTime, startCount, curCount;
	CCycleCount start, end;

	// Take 1/32 of a second for the measurement.
	QueryPerformanceFrequency( &waitTime );
	int scale = 5;
	waitTime.QuadPart >>= scale;

	QueryPerformanceCounter( &startCount );
	start.Sample();
	do
	{
		QueryPerformanceCounter( &curCount );
	}
	while ( curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart );
	end.Sample();

	int64 freq = (end.m_Int64 - start.m_Int64) << scale;
	if ( freq == 0 )
	{
		// Steam was seeing Divide-by-zero crashes on some Windows machines due to
		// WIN64_AMD_DUALCORE_TIMER_WORKAROUND that can cause rdtsc to effectively
		// stop. Staging doesn't have the workaround but I'm checking in the fix
		// anyway. Return a plausible speed and get on with our day.
		freq = 2000000000;
	}
	return freq;

#else
	return 3200000000LL;
#endif
#elif defined(POSIX)
	int64 freq =(int64)CalculateCPUFreq();
	if ( freq == 0 ) // couldn't calculate clock speed
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	return freq;
#endif
}

const CPUInformation* GetCPUInformation()
{
	static CPUInformation pi;

	// Has the structure already been initialized and filled out?
	if ( pi.m_Size == sizeof(pi) )
		return &pi;

	// Redundant, but just in case the user somehow messes with the size.
	memset(&pi, 0x0, sizeof(pi));

	// Fill out the structure, and return it:
	pi.m_Size = sizeof(pi);

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();
	
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage();

#if defined(_WIN32) && !defined( _X360 )
	SYSTEM_INFO si;
	ZeroMemory( &si, sizeof(si) );

	GetSystemInfo( &si );

	pi.m_nPhysicalProcessors = (unsigned char)(si.dwNumberOfProcessors / pi.m_nLogicalProcessors);
	pi.m_nLogicalProcessors = (unsigned char)(pi.m_nLogicalProcessors * pi.m_nPhysicalProcessors);

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
	}
#elif defined( _X360 )
	pi.m_nPhysicalProcessors = 3;
	pi.m_nLogicalProcessors  = 6;
#elif defined(_LINUX)
	// TODO: poll /dev/cpuinfo when we have some benefits from multithreading
	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( fpCpuInfo )
	{
		int nLogicalProcs = 0;
		int nProcId = -1, nCoreId = -1;
		const int kMaxPhysicalCores = 128;
		int anKnownIds[kMaxPhysicalCores];
		int nKnownIdCount = 0;
		char buf[255];
		while ( fgets( buf, ARRAYSIZE(buf), fpCpuInfo ) )
		{
			if ( char *value = strchr( buf, ':' ) )
			{
				for ( char *p = value - 1; p > buf && isspace((unsigned char)*p); --p )
				{
					*p = 0;
				}
				for ( char *p = buf; p < value && *p; ++p )
				{
					*p = tolower((unsigned char)*p);
				}
				if ( !strcmp( buf, "processor" ) )
				{
					++nLogicalProcs;
					nProcId = nCoreId = -1;
				}
				else if ( !strcmp( buf, "physical id" ) )
				{
					nProcId = atoi( value+1 );
				}
				else if ( !strcmp( buf, "core id" ) )
				{
					nCoreId = atoi( value+1 );
				}

				if (nProcId != -1 && nCoreId != -1) // as soon as we have a complete id, process it
				{
					int i = 0, nId = (nProcId << 16) + nCoreId;
					while ( i < nKnownIdCount && anKnownIds[i] != nId ) { ++i; }
					if ( i == nKnownIdCount && nKnownIdCount < kMaxPhysicalCores )
						anKnownIds[nKnownIdCount++] = nId;
					nProcId = nCoreId = -1;
				}
			}
		}
		fclose( fpCpuInfo );
		pi.m_nLogicalProcessors = MAX( 1, nLogicalProcs );
		pi.m_nPhysicalProcessors = MAX( 1, nKnownIdCount );
	}
	else
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
		Assert( !"couldn't read cpu information from /proc/cpuinfo" );
	}
#elif defined(OSX)
	int mib[2], num_cpu = 1;
	size_t len;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(num_cpu);
	sysctl(mib, 2, &num_cpu, &len, NULL, 0);
	pi.m_nPhysicalProcessors = num_cpu;
	pi.m_nLogicalProcessors  = num_cpu;
#endif

	// Determine Processor Features:
	pi.m_bRDTSC        = CheckRDTSCTechnology();
	pi.m_bCMOV         = CheckCMOVTechnology();
	pi.m_bFCMOV        = CheckFCMOVTechnology();
	pi.m_bMMX          = CheckMMXTechnology();
	pi.m_bSSE          = CheckSSETechnology();
	pi.m_bSSE2         = CheckSSE2Technology();
	pi.m_bSSE3         = CheckSSE3Technology();
	pi.m_bSSSE3		   = CheckSSSE3Technology();
	pi.m_bSSE4a        = CheckSSE4aTechnology();
	pi.m_bSSE41        = CheckSSE41Technology();
	pi.m_bSSE42        = CheckSSE42Technology();
	pi.m_b3DNow        = Check3DNowTechnology();
	pi.m_szProcessorID = (tchar*)GetProcessorVendorId();
	pi.m_bHT		   = HTSupported();

	unsigned long eax, ebx, edx, ecx;
	if (cpuid(1, eax, ebx, ecx, edx))
	{
		pi.m_nModel = eax; // full CPU model info
		pi.m_nFeatures[0] = edx; // x87+ features
		pi.m_nFeatures[1] = ecx; // sse3+ features
		pi.m_nFeatures[2] = ebx; // some additional features
	}



	return &pi;
}

