//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: linux dependant ASM code for CPU capability detection
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#include "platform.h"

#if defined __SANITIZE_ADDRESS__
bool CheckMMXTechnology(void) { return false; }
bool CheckSSETechnology(void) { return false; }
bool CheckSSE2Technology(void) { return false; }
bool Check3DNowTechnology(void) { return false; }
#elif defined (__arm__) || defined (__aarch64__)
bool CheckMMXTechnology(void) { return false; }
bool CheckSSETechnology(void) { return false; }
bool CheckSSE2Technology(void) { return false; }
bool Check3DNowTechnology(void) { return false; }
#else

static void cpuid(uint32 function, uint32& out_eax, uint32& out_ebx, uint32& out_ecx, uint32& out_edx)
{
#if defined(PLATFORM_64BITS)
        asm("mov %%rbx, %%rsi\n\t"
                "cpuid\n\t"
                "xchg %%rsi, %%rbx"
                : "=a" (out_eax),
                  "=S" (out_ebx),
                  "=c" (out_ecx),
                  "=d" (out_edx)
                : "a" (function) 
        );
#else
        asm("mov %%ebx, %%esi\n\t"
                "cpuid\n\t"
                "xchg %%esi, %%ebx"
                : "=a" (out_eax),
                  "=S" (out_ebx),
                  "=c" (out_ecx),
                  "=d" (out_edx)
                : "a" (function) 
        );
#endif
}

bool CheckMMXTechnology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x800000;
}

bool CheckSSETechnology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x2000000L;
}

bool CheckSSE2Technology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x04000000;
}

bool Check3DNowTechnology(void)
{
    uint32 eax, unused;
    cpuid(0x80000000,eax,unused,unused,unused);

    if ( eax > 0x80000000L )
    {
     	cpuid(0x80000001,unused,unused,unused,eax);
		return ( eax & 1<<31 );
    }
    return false;
}

#endif
