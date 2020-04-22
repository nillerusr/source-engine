//-------------------------------------------------------------------------------------
// CpuTopology.cpp
// 
// CpuToplogy class implementation.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------
#include "pch_tier0.h"

#if defined(_WIN32) && !defined(_X360) && !defined( _PS3 )
#include "cputopology.h"
#include <stdlib.h>
#include <crtdbg.h>

#undef malloc
#undef free
#ifdef _WIN64
    // Inline assembly is not supported in 64-bit.  Ideally, we would prefer to
    // use the __cpuid intrinsic to avoid having to drop to assembly altogether.
    // However, as of MSVC 9.0, the __cpuid intrinsic does not enable the
    // ability to set the ECX register, which is required for obtaining certain
    // extended CPU information.  To overcome these issues, we must call the
    // Cpuid64() external function, which is written in assembly and located in
    // the cpuid64.asm file included in this project.  This project contains a build
    // step that invokes 64-bit MASM on cpuid64.asm when building a 64-bit target.
    extern "C" void Cpuid64(void* argsPtr);
#endif

//---------------------------------------------------------------------------------
// Name: ICpuToplogy
// Desc: Specifies the interface that each class that provides an implementation
//       for extracting cpu topology must conform to.  This is the Implementor
//       class in the traditional Bridge Pattern.
//---------------------------------------------------------------------------------
class ICpuTopology
{
public:
    virtual             ~ICpuTopology()
    {
    }
    virtual BOOL        IsDefaultImpl() const                   = 0;
    virtual DWORD       NumberOfProcessCores() const            = 0;
    virtual DWORD       NumberOfSystemCores() const             = 0;
    virtual DWORD_PTR   CoreAffinityMask( DWORD coreIdx ) const = 0;
};


namespace
{
///////////////////////////////////////////////////////////////////////////////////
// Local Class Definitions
///////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------
// Name: DefaultImpl
// Desc: Provides a default implementation for the ICpuTopology interface when
//       GetLogicalProcessorInformation and CPUID are not supported for whatever
//       reason.  This is a ConcreteImplementor class in the traditional Bridge
//       Pattern.
//---------------------------------------------------------------------------------
class DefaultImpl : public ICpuTopology
{
public:
    //-----------------------------------------------------------------------------
    // DefaultImpl::IsDefaultImpl
    //-----------------------------------------------------------------------------
    /*virtual*/ BOOL        IsDefaultImpl() const
    {
        return TRUE;
    }

    //-----------------------------------------------------------------------------
    // DefaultImpl::NumberOfProcessCores
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfProcessCores() const
    {
        return 1;
    }

    //-----------------------------------------------------------------------------
    // DefaultImpl::IsNumberOfSystemCores
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfSystemCores() const
    {
        return 1;
    }

    //-----------------------------------------------------------------------------
    // DefaultImpl::CoreAffinityMask
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD_PTR   CoreAffinityMask( DWORD coreIdx ) const
    {
        DWORD_PTR coreAffinity = 0;
        if( 1 == coreIdx )
        {
            DWORD_PTR dwSystemAffinity;
            GetProcessAffinityMask( GetCurrentProcess(), &coreAffinity, &dwSystemAffinity );
        }
        return coreAffinity;
    }
};

//---------------------------------------------------------------------------------
// Name: GlpiImpl
// Desc: Provides the GetLogicalProcessorInformation implementation for the
//       ICpuTopology interface.  This is a ConcreteImplementor class in the
//       traditional Bridge Pattern.
//---------------------------------------------------------------------------------
class GlpiImpl : public ICpuTopology
{
public:

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::GlpiImpl
    // Desc: Initializes the internal structures/data with information retrieved
    //       from a call to GetLogicalProcessorInformation.
    //-----------------------------------------------------------------------------
                            GlpiImpl() : m_pSlpi( NULL ),
                                         m_nItems( 0 )
                            {
                                _ASSERT( IsSupported() );

                                GlpiFnPtr pGlpi = GetGlpiFn_();
                                _ASSERT( pGlpi );

                                DWORD cbBuffer = 0;
                                pGlpi( 0, &cbBuffer );

                                m_pSlpi = ( SYSTEM_LOGICAL_PROCESSOR_INFORMATION* )malloc( cbBuffer );
                                pGlpi( m_pSlpi, &cbBuffer );
                                m_nItems = cbBuffer / sizeof( SYSTEM_LOGICAL_PROCESSOR_INFORMATION );
                            }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::~GlpiImpl
    //-----------------------------------------------------------------------------
                            /*virtual*/ ~GlpiImpl()
                            {
                                free( m_pSlpi );
                                m_pSlpi = 0;
                                m_nItems = 0;
                            }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::IsDefaultImpl
    //-----------------------------------------------------------------------------
    /*virtual*/ BOOL        IsDefaultImpl() const
    {
        return FALSE;
    }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::NumberOfProcessCores
    // Desc: Gets the total number of physical processor cores available to the
    //       current process.
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfProcessCores() const
    {
        DWORD_PTR dwProcessAffinity, dwSystemAffinity;
        GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity );

        DWORD nCores = 0;
        for( DWORD i = 0; i < m_nItems; ++i )
        {
            if( ( RelationProcessorCore == m_pSlpi[i].Relationship ) &&
                ( m_pSlpi[i].ProcessorMask & dwProcessAffinity ) )
            {
                ++nCores;
            }
        }
        return nCores;
    }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::NumberOfSystemCores
    // Desc: Gets the total number of physical processor cores enabled on the
    //       system.
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfSystemCores() const
    {
        DWORD nCores = 0;
        for( DWORD i = 0; i < m_nItems; ++i )
        {
            if( RelationProcessorCore == m_pSlpi[i].Relationship )
                ++nCores;
        }							
        return nCores;
    }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::CoreAffinityMask
    // Desc: Gets an affinity mask that corresponds to the requested processor
    //       core.
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD_PTR   CoreAffinityMask( DWORD coreIdx ) const
    {
        DWORD_PTR dwProcessAffinity, dwSystemAffinity;
        GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity );

        for( DWORD i = 0; i < m_nItems; ++i )
        {
            if( RelationProcessorCore == m_pSlpi[i].Relationship )
            {
                if( !coreIdx-- )
                {
                    return m_pSlpi[i].ProcessorMask & dwProcessAffinity;
                }
            }
        }
        return 0;
    }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::IsSupported
    //-----------------------------------------------------------------------------
    static BOOL             IsSupported()
    {
        return NULL != GetGlpiFn_();
    }

private:
    // GetLogicalProcessorInformation function pointer
    typedef                 BOOL( WINAPI* GlpiFnPtr )(
SYSTEM_LOGICAL_PROCESSOR_INFORMATION*,
PDWORD
);

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::VerifyGlpiFn_
    // Desc: Gets a pointer to the GetLogicalProcessorInformation function only if
    //       it is supported on the current platform.
    //       GetLogicalProcessorInformation is supported on Windows Server 2003 and
    //       XP64, however there is a bug with the implementation.  Therefore, only
    //       GetLogicalProcessorInformation on Windows Vista is supported in this
    //       sample.
    //-----------------------------------------------------------------------------
    static GlpiFnPtr        VerifyGlpiFn_()
    {
        // VerifyVersionInfo function pointer
        typedef BOOL ( WINAPI* VviFnPtr )( LPOSVERSIONINFOEX,
                                           DWORD,
                                           DWORDLONG );

        HMODULE hMod = GetModuleHandle( TEXT( "kernel32" ) );
#ifdef _UNICODE
            VviFnPtr pVvi   = (VviFnPtr) GetProcAddress( hMod, "VerifyVersionInfoW" );
        #else
        VviFnPtr pVvi = ( VviFnPtr )GetProcAddress( hMod, "VerifyVersionInfoA" );
#endif
        GlpiFnPtr pGlpi = NULL;

        if( pVvi )
        {
            // VerSetConditionMask function pointer
            typedef ULONGLONG ( WINAPI* VscmFnPtr )( ULONGLONG,
                                                     DWORD,
                                                     BYTE );

            VscmFnPtr pVscm = ( VscmFnPtr )GetProcAddress( hMod, "VerSetConditionMask" );

            _ASSERT( pVscm );

            // Check for Windows Vista
            OSVERSIONINFOEX osvi = { sizeof( OSVERSIONINFOEX ) };
            osvi.dwMajorVersion = 6;
            osvi.dwMinorVersion = 0;
            osvi.wServicePackMajor = 0;
            osvi.wServicePackMinor = 0;

            ULONGLONG dwlMask = 0;
            dwlMask = pVscm( dwlMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
            dwlMask = pVscm( dwlMask, VER_MINORVERSION, VER_GREATER_EQUAL );
            dwlMask = pVscm( dwlMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );
            dwlMask = pVscm( dwlMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL );

            if( pVvi( &osvi, VER_MAJORVERSION
                      | VER_MINORVERSION
                      | VER_SERVICEPACKMAJOR
                      | VER_SERVICEPACKMINOR,
                      dwlMask ) )
            {
                pGlpi = ( GlpiFnPtr )GetProcAddress( hMod, "GetLogicalProcessorInformation" );
                _ASSERT( pGlpi );
            }
        }

        return pGlpi;

    }

    //-----------------------------------------------------------------------------
    // Name: GlpiImpl::GetGlpiFn_
    // Desc: Gets a cached pointer to the GetLogicalProcessorInformation function.
    //-----------------------------------------------------------------------------
    static GlpiFnPtr        GetGlpiFn_()
    {
        static GlpiFnPtr pGlpi = VerifyGlpiFn_();
        return pGlpi;
    }

    // Private Members
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* m_pSlpi;
    DWORD m_nItems;
};

//---------------------------------------------------------------------------------
// Name: ApicExtractor
// Desc: A utility class that provides an interface for decoding a processor
//       APIC ID.  An APIC ID is an 8-bit identifier given to each logical
//       processor on system boot and can be retrieved by the CPUID instruction.
//       Each APIC ID is composed of a PACKAGE_ID, CORE_ID and SMT_ID that describe
//       the relationship of a logical processor within the processor topology of
//       the system.
//---------------------------------------------------------------------------------
class ApicExtractor
{
public:
    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::ApicExtractor
    //-----------------------------------------------------------------------------
                ApicExtractor( DWORD nLogProcsPerPkg = 1, DWORD nCoresPerPkg = 1 )
                {
                    SetPackageTopology( nLogProcsPerPkg, nCoresPerPkg );
                }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::SmtId
    //-----------------------------------------------------------------------------
    BYTE        SmtId( BYTE apicId ) const
    {
        return apicId & m_smtIdMask.mask;
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::CoreId
    //-----------------------------------------------------------------------------
    BYTE        CoreId( BYTE apicId ) const
    {
        return ( apicId & m_coreIdMask.mask ) >> m_smtIdMask.width;
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::PackageId
    //-----------------------------------------------------------------------------
    BYTE        PackageId( BYTE apicId ) const
    {
        return ( apicId & m_pkgIdMask.mask ) >>
            ( m_smtIdMask.width + m_coreIdMask.width );
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::PackageCoreId
    //-----------------------------------------------------------------------------
    BYTE        PackageCoreId( BYTE apicId ) const
    {
        return ( apicId & ( m_pkgIdMask.mask | m_coreIdMask.mask ) ) >>
            m_smtIdMask.width;
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::GetLogProcsPerPkg
    //-----------------------------------------------------------------------------
    DWORD       GetLogProcsPerPkg() const
    {
        return m_nLogProcsPerPkg;
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::GetCoresPerPkg
    //-----------------------------------------------------------------------------
    DWORD       GetCoresPerPkg() const
    {
        return m_nCoresPerPkg;
    }

    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::SetPackageTopology
    // Desc: You should call SetPackageTopology with the number of logical
    //       processors per package and number of cores per package before calling
    //       the sub id accessors (SmtId(), CoreId(), PackageId(), PackageCoreId())
    //       as this information is required to effectively decode an APIC ID into
    //       its sub parts.
    //-----------------------------------------------------------------------------
    void        SetPackageTopology( DWORD nLogProcsPerPkg, DWORD nCoresPerPkg )
    {
        m_nLogProcsPerPkg = ( BYTE )nLogProcsPerPkg;
        m_nCoresPerPkg = ( BYTE )nCoresPerPkg;

		// fix for Phenom x3 and similar CPUs - it reports 3 logical processors per package, and 4 cores per package
		// so one core is probably just disabled for yield, but it causes a bug in GetMaskWidth that propagates
		if( m_nCoresPerPkg > m_nLogProcsPerPkg )
		{	
			m_nCoresPerPkg = m_nLogProcsPerPkg;
		}

        m_smtIdMask.width = GetMaskWidth_( m_nLogProcsPerPkg / m_nCoresPerPkg );
        m_coreIdMask.width = GetMaskWidth_( m_nCoresPerPkg );
        m_pkgIdMask.width = 8 - ( m_smtIdMask.width + m_coreIdMask.width );

        m_pkgIdMask.mask = ( BYTE )( 0xFF << ( m_smtIdMask.width + m_coreIdMask.width ) );
        m_coreIdMask.mask = ( BYTE )( ( 0xFF << m_smtIdMask.width ) ^ m_pkgIdMask.mask );
        m_smtIdMask.mask = ( BYTE )~( 0xFF << m_smtIdMask.width );

    }

private:
    //-----------------------------------------------------------------------------
    // Name: ApicExtractor::GetMaskWidth_
    // Desc: Gets the width of a sub id bit field in an APIC ID.  The width of a
    //       sub id (CORE_ID, SMT_ID) is only wide enough to support the maximum
    //       number of ids that needs to be represented in the topology.
    //-----------------------------------------------------------------------------
    static BYTE GetMaskWidth_( BYTE maxIds )
    {
        --maxIds;

        // find index of msb
        BYTE msbIdx = 8;
        BYTE msbMask = 0x80;
        while( msbMask && !( msbMask & maxIds ) )
        {
            --msbIdx;
            msbMask >>= 1;
        }
        return msbIdx;
    }

    struct IdMask
    {
        BYTE width;
        BYTE mask;
    };

    // Private Members
    BYTE m_nLogProcsPerPkg;
    BYTE m_nCoresPerPkg;
    IdMask m_smtIdMask;
    IdMask m_coreIdMask;
    IdMask m_pkgIdMask;
};

//---------------------------------------------------------------------------------
// Name: Cpuid
// Desc: A utility class that wraps the functionality of the CPUID instruction.
//       Call the Call() method with the desired CPUID function, and use the
//       register accessors to retrieve the register values.
//---------------------------------------------------------------------------------
class Cpuid
{
public:
    // FnSet values are used to indicate a CPUID function set.
    enum FnSet
    {
        Std = 0x00000000,
        Ext = 0x80000000
    };

    //-----------------------------------------------------------------------------
    // Name: Cpuid::Cpuid
    //-----------------------------------------------------------------------------
                Cpuid() : m_eax( 0 ),
                          m_ebx( 0 ),
                          m_ecx( 0 ),
                          m_edx( 0 )
                {
                }

    // Register accessors
    DWORD       Eax() const
    {
        return m_eax;
    }
    DWORD       Ebx() const
    {
        return m_ebx;
    }
    DWORD       Ecx() const
    {
        return m_ecx;
    }
    DWORD       Edx() const
    {
        return m_edx;
    }

    //-----------------------------------------------------------------------------
    // Name: Cpuid::Call
    // Desc: Calls the CPUID instruction with the specified function.  Returns TRUE
    //       if the CPUID function was supported, FALSE if it wasn't.
    //-----------------------------------------------------------------------------
    BOOL        Call( FnSet fnSet, DWORD fn )
    {
        if( IsFnSupported( fnSet, fn ) )
        {
            UncheckedCall_( fnSet, fn );
            return true;
        }
        return false;
    }

    //-----------------------------------------------------------------------------
    // Name: Cpuid::IsVendor
    // Desc: Compares a string with the vendor string encoded in the CPUID
    //       instruction.
    //-----------------------------------------------------------------------------
    static BOOL IsVendor( const char* strVendor )
    {
        // Cache the vendor string
        static const Cpuid cpu( Std );
        return cpu.Ebx() == *reinterpret_cast<const DWORD*>( strVendor )
            && cpu.Ecx() == *reinterpret_cast<const DWORD*>( strVendor + 8 )
            && cpu.Edx() == *reinterpret_cast<const DWORD*>( strVendor + 4 );
    }

    //-----------------------------------------------------------------------------
    // Name: Cpuid::IsFnSupported
    // Desc: Checks to see if a CPUID function is supported.  Different processors
    //       support different functions.  This method is automatically called from
    //       the Call() method, so you don't need to call it beforehand.
    //-----------------------------------------------------------------------------
    static BOOL IsFnSupported( FnSet fnSet, DWORD fn )
    {
        // Cache the maximum supported standard function
        static const DWORD MaxStdFn = Cpuid( Std ).Eax();
        // Cache the maximum supported extended function
        static const DWORD MaxExtFn = Cpuid( Ext ).Eax();

        bool ret = false;
        switch( fnSet )
        {
            case Std:
                ret = ( fn <= MaxStdFn );
                break;
            case Ext:
                ret = ( fn <= MaxExtFn );
                break;
            default:
                _ASSERT( 0 );   // should never get here
                break;
        }
        return ret;
    }

private:
    //-----------------------------------------------------------------------------
    // Name: Cpuid::Cpuid
    // Desc: This constructor is private and is only used to set a Cpuid object to
    //       initial values retrieved from CPUID functions 0x00000000 and
    //       0x80000000.  Good for caching values from the CPUID instruction that
    //       are not variable, like the encoded vendor string and the maximum
    //       supported CPUID function values.
    //-----------------------------------------------------------------------------
    explicit    Cpuid( FnSet fnSet )
    {
        UncheckedCall_( fnSet, 0 );
    }

    //-----------------------------------------------------------------------------
    // Name: Cpuid::UncheckedCall_
    // Desc: Calls the CPUID instruction without checking for CPUID function
    //       support.
    //-----------------------------------------------------------------------------
    void        UncheckedCall_( FnSet fnSet, DWORD fn )
    {
#ifdef _WIN64
            // Inline assembly is not supported in 64-bit.  Ideally, we would prefer to
            // use the __cpuid intrinsic to avoid having to drop to assembly altogether.
            // However, as of MSVC 9.0, the __cpuid intrinsic does not enable the
            // ability to set the ECX register, which is required for obtaining certain
            // extended CPU information.  To overcome these issues, we must call the
            // Cpuid64() external function, which is written in assembly and located in
            // the cpuid64.asm file included in this project.  This project contains a build
            // step that invokes 64-bit MASM on cpuid64.asm when building a 64-bit target.
            m_eax = fnSet | fn;
            m_ecx = 0;
            Cpuid64(this);
        #else
        __asm
            {
            mov ecx, 0
            mov eax, fn
            or  eax, fnSet
            cpuid
            mov edi, this
            mov [edi].m_eax, eax
            mov [edi].m_ebx, ebx
            mov [edi].m_ecx, ecx
            mov [edi].m_edx, edx
            }
#endif
    }

    // Private Members
    DWORD m_eax;
    DWORD m_ebx;
    DWORD m_ecx;
    DWORD m_edx;
};

//---------------------------------------------------------------------------------
// Name: CpuidImpl
// Desc: Provides the CPUID instruction implementation for the ICpuTopology
//       interface.  This is a ConcreteImplementor class in the traditional Bridge
//       Pattern.
//---------------------------------------------------------------------------------
class CpuidImpl : public ICpuTopology
{
public:
    // CpuidFnMasks are used when extracting bit-encoded information retrieved from
    // the CPUID instruction
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

    enum
    {
        MaxLogicalProcessors = sizeof( DWORD_PTR ) * 8
    };

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::CpuidImpl
    // Desc: Initializes internal structures/data with information retrieved from
    //       calling the CPUID instruction.
    //-----------------------------------------------------------------------------
                            CpuidImpl() : m_nItems( 0 )
                            {
                                _ASSERT( IsSupported() );

                                DWORD nLogProcsPerPkg = 1;
                                DWORD nCoresPerPkg = 1;

                                Cpuid cpu;

                                // Determine if hardware threading is enabled.
                                cpu.Call( Cpuid::Std, 1 );
                                if( cpu.Edx() & HTT )
                                {
                                    // Determine the total number of logical processors per package.
                                    nLogProcsPerPkg = ( cpu.Ebx() & LogicalProcessorCount ) >> 16;

                                    // Determine the total number of cores per package.  This info
                                    // is extracted differently dependending on the cpu vendor.
                                    if( Cpuid::IsVendor( GenuineIntel ) )
                                    {
                                        if( cpu.Call( Cpuid::Std, 4 ) )
                                        {
                                            nCoresPerPkg = ( ( cpu.Eax() & NC_Intel ) >> 26 ) + 1;
                                        }
                                    }
                                    else
                                    {
                                        _ASSERT( Cpuid::IsVendor( AuthenticAMD ) );
                                        if( cpu.Call( Cpuid::Ext, 8 ) )
                                        {
                                            // AMD reports the msb width of the CORE_ID bit field of the APIC ID
                                            // in ApicIdCoreIdSize_Amd.  The maximum value represented by the msb
                                            // width is the theoretical number of cores the processor can support
                                            // and not the actual number of current cores, which is how the msb width
                                            // of the CORE_ID bit field has been traditionally determined.  If the
                                            // ApicIdCoreIdSize_Amd value is zero, then you use the traditional method
                                            // to determine the CORE_ID msb width.
                                            DWORD msbWidth = cpu.Ecx() & ApicIdCoreIdSize_Amd;
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
                                                nCoresPerPkg = ( cpu.Ecx() & NC_Amd ) + 1;
                                            }
                                        }
                                    }
                                }

                                // Configure the APIC extractor object with the information it needs to
                                // be able to decode the APIC.
                                m_apicExtractor.SetPackageTopology( nLogProcsPerPkg, nCoresPerPkg );

                                DWORD_PTR dwProcessAffinity, dwSystemAffinity;
                                HANDLE hProcess = GetCurrentProcess();
                                HANDLE hThread = GetCurrentThread();
                                GetProcessAffinityMask( hProcess, &dwProcessAffinity, &dwSystemAffinity );
                                if( 1 == dwSystemAffinity )
                                {
                                    // Since we only have 1 logical processor present on the system, we
                                    // can explicitly set a single APIC ID to zero.
                                    _ASSERT( 1 == nLogProcsPerPkg );
                                    m_apicIds[m_nItems++] = 0;
                                }
                                else
                                {
                                    // Set the process affinity to the system affinity if they are not
                                    // equal so that all logical processors can be accounted for.
                                    if( dwProcessAffinity != dwSystemAffinity )
                                    {
                                        SetProcessAffinityMask( hProcess, dwSystemAffinity );
                                    }

                                    // Call cpuid on each active logical processor in the system affinity.
                                    DWORD_PTR dwPrevThreadAffinity = 0;
                                    for( DWORD_PTR dwThreadAffinity = 1;
                                         dwThreadAffinity && dwThreadAffinity <= dwSystemAffinity;
                                         dwThreadAffinity <<= 1 )
                                    {
                                        if( dwSystemAffinity & dwThreadAffinity )
                                        {
                                            if( 0 == dwPrevThreadAffinity )
                                            {
                                                // Save the previous thread affinity so we can return
                                                // the executing thread affinity back to this state.
                                                _ASSERT( 0 == m_nItems );
                                                dwPrevThreadAffinity = SetThreadAffinityMask( hThread,
                                                                                              dwThreadAffinity );
                                            }
                                            else
                                            {
                                                _ASSERT( m_nItems > 0 );
                                                SetThreadAffinityMask( hThread, dwThreadAffinity );
                                            }

                                            // Allow the thread to switch to masked logical processor.
                                            Sleep( 0 );

                                            // Store the APIC ID
                                            cpu.Call( Cpuid::Std, 1 );
                                            m_apicIds[m_nItems++] = ( BYTE )( ( cpu.Ebx() & ApicId ) >> 24 );
                                        }
                                    }

                                    // Restore the previous process and thread affinity state.
                                    SetProcessAffinityMask( hProcess, dwProcessAffinity );
                                    SetThreadAffinityMask( hThread, dwPrevThreadAffinity );
                                    Sleep( 0 );
                                }

                            }

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::IsDefaultImpl
    //-----------------------------------------------------------------------------
    /*virtual*/ BOOL        IsDefaultImpl() const
    {
        return FALSE;
    }

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::NumberOfProcessCores
    // Desc: Gets the number of processor cores available to the current process.
    //       The total accounts for cores that may have been masked out by process
    //       affinity.
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfProcessCores() const
    {
        DWORD_PTR dwProcessAffinity, dwSystemAffinity;
        GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity );

        BYTE pkgCoreIds[MaxLogicalProcessors] = { 0 };
        DWORD nPkgCoreIds = 0;

        for( DWORD i = 0; i < m_nItems; ++i )
        {
            if( dwProcessAffinity & ( ( DWORD_PTR )1 << i ) )
            {
                AddUniquePkgCoreId_( i, pkgCoreIds, nPkgCoreIds );
            }
        }
        return nPkgCoreIds;
    }

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::NumberOfSystemCores
    // Desc: Gets the number of processor cores on the system.
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD       NumberOfSystemCores() const
    {
        BYTE pkgCoreIds[MaxLogicalProcessors] = { 0 };
        DWORD nPkgCoreIds = 0;
        for( DWORD i = 0; i < m_nItems; ++i )
        {
            AddUniquePkgCoreId_( i, pkgCoreIds, nPkgCoreIds );
        }
        return nPkgCoreIds;
    }

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::CoreAffinityMask
    // Desc: Gets an affinity mask that corresponds to a specific processor core.
    //       coreIdx must be less than the total number of processor cores
    //       recognized by the operating system (NumberOfSystemCores()).
    //-----------------------------------------------------------------------------
    /*virtual*/ DWORD_PTR   CoreAffinityMask( DWORD coreIdx ) const
    {
        BYTE pkgCoreIds[MaxLogicalProcessors] = { 0 };
        DWORD nPkgCoreIds = 0;
        for( DWORD i = 0; i < m_nItems; ++i )
        {
            AddUniquePkgCoreId_( i, pkgCoreIds, nPkgCoreIds );
        }

        DWORD_PTR dwProcessAffinity, dwSystemAffinity;
        GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity );

        DWORD_PTR coreAffinity = 0;
        if( coreIdx < nPkgCoreIds )
        {
            for( DWORD i = 0; i < m_nItems; ++i )
            {
                if( m_apicExtractor.PackageCoreId( m_apicIds[i] ) == pkgCoreIds[coreIdx] )
                {
                    coreAffinity |= ( dwProcessAffinity & ( ( DWORD_PTR )1 << i ) );
                }
            }
        }
        return coreAffinity;
    }

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::IsSupported
    // Desc: Indicates if a CpuidImpl object is supported on this platform.
    //       Support is only granted on Intel and AMD platforms where the current
    //       calling process has security rights to query process affinity and
    //       change it if the process and system affinity differ.  CpuidImpl is
    //       also not supported if thread affinity cannot be set on systems with
    //       more than 1 logical processor.
    //-----------------------------------------------------------------------------
    static BOOL             IsSupported()
    {
        BOOL bSupported = Cpuid::IsVendor( GenuineIntel )
            || Cpuid::IsVendor( AuthenticAMD );

        if( bSupported )
        {
            DWORD_PTR dwProcessAffinity, dwSystemAffinity;
            HANDLE hProcess = GetCurrentProcess();

            // Query process affinity mask
            bSupported = GetProcessAffinityMask( hProcess, &dwProcessAffinity, &dwSystemAffinity );
            if( bSupported )
            {
                if( dwProcessAffinity != dwSystemAffinity )
                {
                    // The process and system affinities differ.  Attempt to set
                    // the process affinity to the system affinity.
                    bSupported = SetProcessAffinityMask( hProcess, dwSystemAffinity );
                    if( bSupported )
                    {
                        // Restore previous process affinity
                        bSupported = SetProcessAffinityMask( hProcess, dwProcessAffinity );
                    }
                }

                if( bSupported && ( dwSystemAffinity > 1 ) )
                {
                    // Attempt to set the thread affinity 
                    HANDLE hThread = GetCurrentThread();
                    DWORD_PTR dwThreadAffinity = SetThreadAffinityMask( hThread, dwProcessAffinity );
                    if( dwThreadAffinity )
                    {
                        // Restore the previous thread affinity
                        bSupported = 0 != SetThreadAffinityMask( hThread, dwThreadAffinity );
                    }
                    else
                    {
                        bSupported = FALSE;
                    }
                }
            }
        }
        return bSupported;
    }

private:

    //-----------------------------------------------------------------------------
    // Name: CpuidImpl::AddUniquePkgCoreId_
    // Desc: Adds the package/core id extracted from the APIC ID at m_apicIds[idx]
    //       in the if the package/core id is unique to the pkgCoreIds array.
    //       nPkgCore is an in/out parm that will reflect the total number of items
    //       in pkgCoreIds array.  It will be incrememted if a unique package/core
    //       id is found and added.
    //-----------------------------------------------------------------------------
    void                    AddUniquePkgCoreId_( DWORD idx, BYTE* pkgCoreIds, DWORD& nPkgCoreIds ) const
    {
        _ASSERT( idx < m_nItems );
        _ASSERT( NULL != pkgCoreIds );

        DWORD j;
        for( j = 0; j < nPkgCoreIds; ++j )
        {
            if( pkgCoreIds[j] == m_apicExtractor.PackageCoreId( m_apicIds[idx] ) )
                break;
        }
        if( j == nPkgCoreIds )
        {
            pkgCoreIds[j] = m_apicExtractor.PackageCoreId( m_apicIds[idx] );
            ++nPkgCoreIds;
        }
    }

    // Private Members
    BYTE                    m_apicIds[MaxLogicalProcessors];
    BYTE m_nItems;
    ApicExtractor m_apicExtractor;

    // Supported Vendor Strings
    static const char       GenuineIntel[];
    static const char       AuthenticAMD[];
};

// Static initialization of vendor strings
const char CpuidImpl::GenuineIntel[] = "GenuineIntel";
const char CpuidImpl::AuthenticAMD[] = "AuthenticAMD";

}   // unnamed-namespace

//-------------------------------------------------------------------------------------
// Name: CpuTopology::CpuTopology
// Desc: Initializes this object with the appropriately supported cpu topology
//       implementation object.
//-------------------------------------------------------------------------------------
CpuTopology::CpuTopology( BOOL bForceCpuid ) : m_pImpl( NULL )
{
    ForceCpuid( bForceCpuid );
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::~CpuTopology
//-------------------------------------------------------------------------------------
CpuTopology::~CpuTopology()
{
    Destroy_();
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::NumberOfProcessCores
// Desc: Gets the total number of physical processor cores available to the current
//       process.
//-------------------------------------------------------------------------------------
DWORD CpuTopology::NumberOfProcessCores() const
{
    return m_pImpl->NumberOfProcessCores();
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::NumberOfSystemCores
// Desc: Gets the total number of physical processor cores enabled on the system.
//-------------------------------------------------------------------------------------
DWORD CpuTopology::NumberOfSystemCores() const
{
    return m_pImpl->NumberOfSystemCores();
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::CoreAffinityMask
// Desc: Gets an affinity mask that corresponds to the requested processor core.
//-------------------------------------------------------------------------------------
DWORD_PTR CpuTopology::CoreAffinityMask( DWORD coreIdx ) const
{
    return m_pImpl->CoreAffinityMask( coreIdx );
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::IsDefaultImpl
// Desc: Returns TRUE if m_pImpl is a DefaultImpl object, FALSE if not.  Used to
//       indicate whether or not the prescribed methods (CPUID or
//       GetLogicalProcessorInformation) are supported on the system.
//-------------------------------------------------------------------------------------
BOOL CpuTopology::IsDefaultImpl() const
{
    return m_pImpl->IsDefaultImpl();
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::ForceCpuid
// Desc: Constructs a cpu topology object.  If bForce is FALSE, then a GlpiImpl object
//       is first attempted, then CpuidImpl, then finally DefaultImpl.  If bForce is
//       TRUE, then GlpiImpl is never attempted.
//-------------------------------------------------------------------------------------
void CpuTopology::ForceCpuid( BOOL bForce )
{
    Destroy_();

    if( !bForce && GlpiImpl::IsSupported() )
    {
        m_pImpl = new GlpiImpl();
    }
    else if( CpuidImpl::IsSupported() )
    {
        m_pImpl = new CpuidImpl();
    }
    else
    {
        m_pImpl = new DefaultImpl();
    }
}

//-------------------------------------------------------------------------------------
// Name: CpuTopology::Destroy_
//-------------------------------------------------------------------------------------
void CpuTopology::Destroy_()
{
    delete m_pImpl;
    m_pImpl = NULL;
}
#endif