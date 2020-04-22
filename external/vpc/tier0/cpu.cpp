//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "pch_tier0.h"

#if defined(_WIN32) && !defined(_X360)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include "cputopology.h"
#elif defined( PLATFORM_OSX )
#include <sys/sysctl.h>
#endif

#ifndef _PS3
#include "tier0_strtools.h"
#endif

//#include "tier1/strtools.h" // this is included for the definition of V_isspace()
#ifdef PLATFORM_WINDOWS_PC
#include <intrin.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

const tchar* GetProcessorVendorId();

static bool cpuid(uint32 function, uint32& out_eax, uint32& out_ebx, uint32& out_ecx, uint32& out_edx)
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#elif defined(GNUC)
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
#elif defined(_WIN64)
	int pCPUInfo[4];
	__cpuid( pCPUInfo, (int)function );
	out_eax = pCPUInfo[0];
	out_ebx = pCPUInfo[1];
	out_ecx = pCPUInfo[2];
	out_edx = pCPUInfo[3];
	return false;
#else
	bool retval = true;
	uint32 local_eax, local_ebx, local_ecx, local_edx;
	_asm pushad;

	__try
	{
        _asm
		{
			xor edx, edx		// Clue the compiler that EDX & others is about to be used. 
			xor ecx, ecx
			xor ebx, ebx        // <Sergiy> Note: if I don't zero these out, cpuid sometimes won't work, I didn't find out why yet
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
    uint32 eax,ebx,edx,unused;
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

    uint32 eax,ebx,edx,unused;
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
	uint32 eax,ebx,edx,unused;
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
	uint32 eax,ebx,edx,ecx;
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
	uint32 eax,ebx,edx,ecx;
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

	uint32 eax,ebx,edx,ecx;
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

	uint32 eax,ebx,edx,ecx;
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

	uint32 eax,ebx,edx,ecx;
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
	uint32 eax, unused;
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
	uint32 eax,ebx,edx,unused;
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
    uint32 eax,ebx,edx,unused;
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
	uint32 eax,ebx,edx,unused;
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
	uint32 unused, VendorIDRegisters[3];

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

// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.
// If it's supported, it does not mean that it's been enabled. So we test another flag to see if it's enabled
// See Intel Processor Identification and the CPUID instruction Application Note 485
// http://www.intel.com/Assets/PDF/appnote/241618.pdf
static bool HTSupported(void)
{
#if ( defined( _X360 ) || defined( _PS3 ) )
	// not entirtely sure about the semantic of HT support, it being an intel name
	// are we asking about HW threads or HT?
	return true;
#else
	enum {
		HT_BIT		 = 0x10000000,  // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
		FAMILY_ID     = 0x0f00,      // EAX[11:8] - Bit 11 thru 8 contains family processor id
		EXT_FAMILY_ID = 0x0f00000,	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
		FAMILY_ID_386 = 0x0300,
		FAMILY_ID_486 = 0x0400,     // EAX[8:12]  -  486, 487 and overdrive
		FAMILY_ID_PENTIUM = 0x0500, //               Pentium, Pentium OverDrive  60 - 200
		FAMILY_ID_PENTIUM_PRO = 0x0600,//            P Pro, P II, P III, P M, Celeron M, Core Duo, Core Solo, Core2 Duo, Core2 Extreme, P D, Xeon model F,
		                               //            also 45-nm : Intel Atom, Core i7, Xeon MP ; see Intel Processor Identification and the CPUID instruction pg 20,21
		                               
		FAMILY_ID_EXTENDED = 0x0F00 //               P IV, Xeon, Celeron D, P D, 
	};

	uint32 unused,
				  reg_eax = 0, 
				  reg_ebx = 0,
				  reg_edx = 0,
				  vendor_id[3] = {0, 0, 0};

	// verify cpuid instruction is supported
	if( !cpuid(0,unused, vendor_id[0],vendor_id[2],vendor_id[1]) 
	 || !cpuid(1,reg_eax,reg_ebx,unused,reg_edx) )
	 return false;

	// <Sergiy> Previously, we detected P4 specifically; now, we detect GenuineIntel with HT enabled in general
	// if (((reg_eax & FAMILY_ID) ==  FAMILY_ID_EXTENDED) || (reg_eax & EXT_FAMILY_ID))
	
	//  Check to see if this is an Intel Processor with HT or CMT capability , and if HT/CMT is enabled
	if (vendor_id[0] == 'uneG' && vendor_id[1] == 'Ieni' && vendor_id[2] == 'letn')
		return (reg_edx & HT_BIT) != 0 && // Genuine Intel Processor with Hyper-Threading Technology implemented
			   ((reg_ebx >> 16) & 0xFF) > 1 ; // Hyper-Threading OR Core Multi-Processing has been enabled

	return false;  // This is not a genuine Intel processor.
#endif
}

// See Intel Processor Identification and the CPUID instruction Application Note 485
// http://www.intel.com/Assets/PDF/appnote/241618.pdf
int LogicalProcessorsPerCore()
{
#if defined( _X360 ) || defined( _PS3 ) || defined( LINUX )
	return 2; // 
#elif defined(_WIN32) 
	uint32 nMaxStandardFnSupported, nVendorId[3];
	if( !cpuid( 0, nMaxStandardFnSupported,nVendorId[0],nVendorId[2],nVendorId[1] ) )
	{
		return 1;
	}
	
	uint32 nFn1_Eax, nFn1_Ebx, nFn1_Ecx, nFn1_Edx;
	if( !cpuid( 1, nFn1_Eax, nFn1_Ebx, nFn1_Ecx, nFn1_Edx) )
	{
		return 1;
	}
	
	enum CpuidFnMasks
	{
		HTT                     = 0x10000000,   // Fn0000_0001  EDX[28]
		LogicalProcessorCount   = 0x00FF0000,   // Fn0000_0001  EBX[23:16]
		ApicId                  = 0xFF000000,   // Fn0000_0001  EBX[31:24]
		NC_Intel                = 0xFC000000,   // Fn0000_0004  EAX[31:26]
		NC_Amd                  = 0x000000FF,   // Fn8000_0008  ECX[7:0]
		CmpLegacy_Amd           = 0x00000002,   // Fn8000_0001  ECX[1]
		ApicIdCoreIdSize_Amd    = 0x0000F000    // Fn8000_0008  ECX[15:12]
	};

	// Determine if hardware threading is enabled.
	if( nFn1_Edx & HTT )
	{
		// Determine the total number of logical processors per package.
		int nLogProcsPerPkg = ( nFn1_Ebx & LogicalProcessorCount ) >> 16;
		int nCoresPerPkg = 1;
		
		if( ( ( nFn1_Ebx >> 16 ) & 0xFF ) <= 1 ) // Has Hyper-Threading OR Core Multi-Processing not been enabled ?
		{
			 // NOTE:  This is only tested on Intel CPUs; I don't know if it's true on AMD, as I have no HT AMD to test on
			return 1; // HT was turned off, for all intents and purposes in our engine it means one logical CPU per core
		}

		// Determine the total number of cores per package.  This info
		// is extracted differently dependending on the cpu vendor.
		if( nVendorId[0] == 'uneG' && nVendorId[1] == 'Ieni' && nVendorId[2] == 'letn' ) // GenuineIntel 
		{
			if( nMaxStandardFnSupported >= 4 )
			{
				uint32 nFn4_Eax, nFn4_Ebx, nFn4_Ecx, nFn4_Edx ;
				if( cpuid( 4, nFn4_Eax, nFn4_Ebx, nFn4_Ecx, nFn4_Edx ) )
				{
					nCoresPerPkg = ( ( nFn4_Eax & NC_Intel ) >> 26 ) + 1;
					
				}
			}
			// <Sergiy> as the DirectX CoreDetection sample goes, the logic is that on old processors where
			// the functions aren't supported, we assume one core per package, multiple logical processors per package
			// I suspect this may be wrong, especially for AMD processors. 
			return nLogProcsPerPkg / nCoresPerPkg; 
		}
#if 0	 // <Sergiy> To make as concervative change as possible now, I'll skip AMD hyperthread detection	
		else
		{
			if( nVendorId[0] == 'htuA' && nVendorId[1] == 'itne' && nVendorId[2] == 'DMAc' ) // AuthenticAMD
			{
				uint32 nFnx8_Eax, nFnx8_Ebx, nFnx8_Ecx, nFnx8_Edx ;
				if( cpuid( 0x80000008, nFnx8_Eax, nFnx8_Ebx, nFnx8_Ecx, nFnx8_Edx ) )
				{
					// AMD reports the msb width of the CORE_ID bit field of the APIC ID
					// in ApicIdCoreIdSize_Amd.  The maximum value represented by the msb
					// width is the theoretical number of cores the processor can support
					// and not the actual number of current cores, which is how the msb width
					// of the CORE_ID bit field has been traditionally determined.  If the
					// ApicIdCoreIdSize_Amd value is zero, then you use the traditional method
					// to determine the CORE_ID msb width.
					DWORD msbWidth = nFnx8_Ecx & ApicIdCoreIdSize_Amd;
					if( msbWidth )
					{
						// Set nCoresPerPkg to the maximum theortical number of cores
						// the processor package can support (2 ^ width) so the APIC
						// extractor object can be configured to extract the proper
						// values from an APIC.
						nCoresPerPkg = 1 << ( msbWidth >> 12 );
					}
					else
					{
						// Set nCoresPerPkg to the actual number of cores being reported
						// by the CPUID instruction.
						nCoresPerPkg = ( nFnx8_Ecx & NC_Amd ) + 1;
					}
				}
			}
			// <Sergiy> as the DirectX CoreDetection sample goes, the logic is that on old processors where
			// the functions aren't supported, we assume one core per package, multiple logical processors per package
			// I suspect this may be wrong, especially for AMD processors. 
			return nLogProcsPerPkg / nCoresPerPkg; 
		}
#endif
	}
	return 1;
#endif
}



// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _X360 ) || defined(_PS3)
	// Xbox360 and PS3 have the same clock speed and share a lot of characteristics on PPU
	return 3200000000LL;
#else	
#if defined( _WIN32 )
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

	return (end.m_Int64 - start.m_Int64) << scale;
#elif defined(POSIX)
	uint64 CalculateCPUFreq(); // from cpu_linux.cpp
	int64 freq =(int64)CalculateCPUFreq();
	if ( freq == 0 ) // couldn't calculate clock speed
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	return freq;
#else
	#error "Please implement Clock Speed function for this platform"
#endif
#endif
}

static CPUInformation s_cpuInformation;

const CPUInformation& GetCPUInformation()
{
	CPUInformation &pi = s_cpuInformation;
	// Has the structure already been initialized and filled out?
	if ( pi.m_Size == sizeof(pi) )
		return pi;

	// Redundant, but just in case the user somehow messes with the size.
	memset(&pi, 0x0, sizeof(pi));

	// Fill out the structure, and return it: 
	pi.m_Size = sizeof(pi);

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();
	
	// Get the logical and physical processor counts:

#if defined( _X360 )
	pi.m_nPhysicalProcessors = 3;
	pi.m_nLogicalProcessors  = 6;
#elif defined( _PS3 )
	pi.m_nPhysicalProcessors = 1;
	pi.m_nLogicalProcessors  = 2;
#elif defined(_WIN32) && !defined( _X360 )
	SYSTEM_INFO si;
	ZeroMemory( &si, sizeof(si) );

	GetSystemInfo( &si );

	// Sergiy: fixing: si.dwNumberOfProcessors is the number of logical processors according to experiments on i7, P4 and a DirectX sample (Aug'09)
	//         this is contrary to MSDN documentation on GetSystemInfo()
	// 
	pi.m_nLogicalProcessors = si.dwNumberOfProcessors;
	if ( 0 == V_tier0_stricmp( GetProcessorVendorId(), "AuthenticAMD" ) )
	{
		// quick fix for AMD Phenom: it reports 3 logical cores and 4 physical cores;
		// no AMD CPUs by the end of 2009 have HT, so we'll override HT detection here
		pi.m_nPhysicalProcessors = pi.m_nLogicalProcessors;
	}
	else
	{
		CpuTopology topo;
		pi.m_nPhysicalProcessors = topo.NumberOfSystemCores();
	}

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		Assert( !"Sergiy: apparently I didn't fix some CPU detection code completely. Let me know and I'll do my best to fix it soon." );
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
	}
#elif defined(LINUX)
	pi.m_nLogicalProcessors = 0;
	pi.m_nPhysicalProcessors = 0;
	const int k_cMaxProcessors = 256;
	bool rgbProcessors[k_cMaxProcessors];
	memset( rgbProcessors, 0, sizeof( rgbProcessors ) );
	int cMaxCoreId = 0;

	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( fpCpuInfo )
	{
		char rgchLine[256];
		while ( fgets( rgchLine, sizeof( rgchLine ), fpCpuInfo ) )
		{
			if ( !strncasecmp( rgchLine, "processor", strlen( "processor" ) ) )
			{
				pi.m_nLogicalProcessors++;
			}
			if ( !strncasecmp( rgchLine, "core id", strlen( "core id" ) ) )
			{
				char *pchValue = strchr( rgchLine, ':' );
				cMaxCoreId = MAX( cMaxCoreId, atoi( pchValue + 1 ) );
			}
			if ( !strncasecmp( rgchLine, "physical id", strlen( "physical id" ) ) )
			{
				// it seems (based on survey data) that we can see
				// processor N (N > 0) when it's the only processor in
				// the system.  so keep track of each processor
				char *pchValue = strchr( rgchLine, ':' );
				int cPhysicalId = atoi( pchValue + 1 );
				if ( cPhysicalId < k_cMaxProcessors )
					rgbProcessors[cPhysicalId] = true;
			}
			/* this code will tell us how many physical chips are in the machine, but we want
			   core count, so for the moment, each processor counts as both logical and physical.
			if ( !strncasecmp( rgchLine, "physical id ", strlen( "physical id " ) ) )
			{
				char *pchValue = strchr( rgchLine, ':' );
				pi.m_nPhysicalProcessors = MAX( pi.m_nPhysicalProcessors, atol( pchValue ) );
			}
			*/
		}
		fclose( fpCpuInfo );
		for ( int i = 0; i < k_cMaxProcessors; i++ )
			if ( rgbProcessors[i] )
				pi.m_nPhysicalProcessors++;
		pi.m_nPhysicalProcessors *= ( cMaxCoreId + 1 );
	}
	else
	{
		pi.m_nLogicalProcessors = 1;
		pi.m_nPhysicalProcessors = 1;
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
	pi.m_bHT		   = pi.m_nPhysicalProcessors < pi.m_nLogicalProcessors; //HTSupported();

	return pi;
}

