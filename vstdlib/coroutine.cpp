//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// Build Notes: In order for the coroutine system to work a few build options
//				need to be set for coroutine.cpp itself.  These are the VPC
//				entries for those options:
//	$Compiler
//	{
//		$EnableC++Exceptions				"No"
//		$BasicRuntimeChecks					"Default"
//		$EnableFloatingPointExceptions		"No"
//	}
//
//			If you have not set these options you will get a strange popup in
//		Visual Studio at the end of Coroutine_Continue().
//
//=============================================================================

//#include "pch_vstdlib.h"
#if defined(_DEBUG)
// Verify that something is false
#define DbgVerifyNot(x) Assert(!x)
#else
#define DbgVerifyNot(x) x
#endif

#include "vstdlib/coroutine.h"
#include "tier0/vprof.h"
#include "tier0/minidump.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlvector.h"
#include <setjmp.h>

// for debugging
//#define CHECK_STACK_CORRUPTION


#ifndef STEAM
#define PvAlloc(x) malloc(x)
#define FreePv(x) free(x)
#endif

#ifdef CHECK_STACK_CORRUPTION
#include "tier1/checksum_md5.h"
#include "../tier1/checksum_md5.cpp"
#endif // CHECK_STACK_CORRUPTION

//#define COROUTINE_TRACE
#ifdef COROUTINE_TRACE
#include "tier1/fmtstr.h"
static CFmtStr g_fmtstr;
#ifdef WIN32
extern "C"	__declspec(dllimport) void __stdcall OutputDebugStringA( const char * );
#else
void OutputDebugStringA( const char *pchMsg ) { fprintf( stderr, pchMsg ); fflush( stderr ); } 
#endif
#define CoroutineDbgMsg( fmt, ... ) \
{ \
 g_fmtstr.sprintf( fmt, ##__VA_ARGS__ ); \
 OutputDebugStringA( g_fmtstr ); \
}
#else
#define CoroutineDbgMsg( pchMsg, ... )
#endif // COROUTINE_TRACE

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( _MSC_VER ) && ( _MSC_VER >= 1900 ) && defined( PLATFORM_64BITS )
//the VS2105 longjmp() seems to freak out jumping back into a coroutine (just like linux if _FORTIFY_SOURCE is defined)
// I can't find an analogy to _FORTIFY_SOURCE for MSVC at the moment, so I wrote a quick assembly to longjmp() without any safety checks
extern "C" NORETURN void Coroutine_LongJmp_Unchecked( jmp_buf buffer, int nResult );
#define Coroutine_longjmp Coroutine_LongJmp_Unchecked

#ifdef  _WIN64
#define Q_offsetof(s,m)   (size_t)( (ptrdiff_t)&reinterpret_cast<const volatile char&>((((s *)0)->m)) )
#else
#define Q_offsetof(s,m)   (size_t)&reinterpret_cast<const volatile char&>((((s *)0)->m))
#endif
#define SIZEOF_MEMBER( className, memberName ) sizeof( ((className*)nullptr)->memberName )


#define Validate_Jump_Buffer( _Member ) COMPILE_TIME_ASSERT( (Q_offsetof( _JUMP_BUFFER, _Member ) == Q_offsetof( _Duplicate_JUMP_BUFFER, _Member )) && (SIZEOF_MEMBER( _JUMP_BUFFER, _Member ) == SIZEOF_MEMBER( _Duplicate_JUMP_BUFFER, _Member )) )

	//validate that the structure in assembly matches what the crt setjmp thinks it is
#	if defined( PLATFORM_64BITS )
		struct _Duplicate_JUMP_BUFFER
		{
			unsigned __int64 Frame;
			unsigned __int64 Rbx;
			unsigned __int64 Rsp;
			unsigned __int64 Rbp;
			unsigned __int64 Rsi;
			unsigned __int64 Rdi;
			unsigned __int64 R12;
			unsigned __int64 R13;
			unsigned __int64 R14;
			unsigned __int64 R15;
			unsigned __int64 Rip;
			unsigned long MxCsr;
			unsigned short FpCsr;
			unsigned short Spare;

			SETJMP_FLOAT128 Xmm6;
			SETJMP_FLOAT128 Xmm7;
			SETJMP_FLOAT128 Xmm8;
			SETJMP_FLOAT128 Xmm9;
			SETJMP_FLOAT128 Xmm10;
			SETJMP_FLOAT128 Xmm11;
			SETJMP_FLOAT128 Xmm12;
			SETJMP_FLOAT128 Xmm13;
			SETJMP_FLOAT128 Xmm14;
			SETJMP_FLOAT128 Xmm15;
		};

		COMPILE_TIME_ASSERT( sizeof( _JUMP_BUFFER ) == sizeof( _Duplicate_JUMP_BUFFER ) );
		Validate_Jump_Buffer( Frame );
		Validate_Jump_Buffer( Rbx );
		Validate_Jump_Buffer( Rsp );
		Validate_Jump_Buffer( Rbp );
		Validate_Jump_Buffer( Rsi );
		Validate_Jump_Buffer( Rdi );
		Validate_Jump_Buffer( R12 );
		Validate_Jump_Buffer( R13 );
		Validate_Jump_Buffer( R14 );
		Validate_Jump_Buffer( R15 );
		Validate_Jump_Buffer( Rip );
		Validate_Jump_Buffer( MxCsr );
		Validate_Jump_Buffer( FpCsr );
		Validate_Jump_Buffer( Spare );

		Validate_Jump_Buffer( Xmm6 );
		Validate_Jump_Buffer( Xmm7 );
		Validate_Jump_Buffer( Xmm8 );
		Validate_Jump_Buffer( Xmm9 );
		Validate_Jump_Buffer( Xmm10 );
		Validate_Jump_Buffer( Xmm11 );
		Validate_Jump_Buffer( Xmm12 );
		Validate_Jump_Buffer( Xmm13 );
		Validate_Jump_Buffer( Xmm14 );
		Validate_Jump_Buffer( Xmm15 );
#	else
		struct _Duplicate_JUMP_BUFFER
		{
			unsigned long Ebp;
			unsigned long Ebx;
			unsigned long Edi;
			unsigned long Esi;
			unsigned long Esp;
			unsigned long Eip;
			unsigned long Registration;
			unsigned long TryLevel;
			unsigned long Cookie;
			unsigned long UnwindFunc;
			unsigned long UnwindData[6];
		};

		COMPILE_TIME_ASSERT( sizeof( _JUMP_BUFFER ) == sizeof( _Duplicate_JUMP_BUFFER ) );

		Validate_Jump_Buffer( Ebp );
		Validate_Jump_Buffer( Ebx );
		Validate_Jump_Buffer( Edi );
		Validate_Jump_Buffer( Esi );
		Validate_Jump_Buffer( Esp );
		Validate_Jump_Buffer( Eip );
		Validate_Jump_Buffer( Registration );
		Validate_Jump_Buffer( TryLevel );
		Validate_Jump_Buffer( Cookie );
		Validate_Jump_Buffer( UnwindFunc );
		Validate_Jump_Buffer( UnwindData[6] );
#	endif

#else
#define Coroutine_longjmp longjmp
#endif


// it *feels* like we should need barriers around our setjmp/longjmp calls, and the memcpy's
// to make sure the optimizer doesn't reorder us across register load/stores, so I've put them
// in what seem like the appropriate spots, but we seem to run ok without them, so...
#ifdef GNUC
#define RW_MEMORY_BARRIER /* __sync_synchronize() */
#else
#define RW_MEMORY_BARRIER /* _ReadWriteBarrier() */
#endif



// return values from setjmp()
static const int k_iSetJmpStateSaved = 0x00;
static const int k_iSetJmpContinue	= 0x01;
static const int k_iSetJmpDone		= 0x02;
static const int k_iSetJmpDbgBreak	= 0x03;

// distance up the stack that coroutine functions stacks' start
#ifdef _PS3
// PS3 has a small stack. Hopefully we dont need 64k of padding!
static const int k_cubCoroutineStackGap = (3 * 1024);	
static const int k_cubCoroutineStackGapSmall = 64;	
#else
static const int k_cubCoroutineStackGap = (64 * 1024);	
static const int k_cubCoroutineStackGapSmall = 64;	
#endif

// Warning size for allocated stacks
#ifdef _DEBUG
// In debug builds, we'll end up with much more stack usage in some scenarios that isn't representative of release
// builds.  We should still warn if we're going way above what we could expect the optimizer to save us from, but the
// warning is more salient in release.
static const int k_cubMaxCoroutineStackSize = (48 * 1024);
#else
static const int k_cubMaxCoroutineStackSize = (32 * 1024);
#endif // defined( _DEBUG )

#ifdef _WIN64
extern "C" byte *GetStackPtr64();
#define GetStackPtr( pStackPtr)		byte *pStackPtr = GetStackPtr64();
#else
#ifdef WIN32
#define GetStackPtr( pStackPtr )	byte *pStackPtr;	__asm mov pStackPtr, esp	
#elif defined(GNUC)
// Apple's version of gcc/g++ doesn't return the expected value using the intrinsic, so 
// do it the old fashioned way - this will also use asm on linux (since we don't compile
// with llvm/clang there) but that seems fine.
#if defined(__llvm__) || defined(__clang__)
#define GetStackPtr( pStackPtr )	byte *pStackPtr = (byte*)__builtin_frame_address(0)
#else
#define GetStackPtr( pStackPtr )	register byte *pStackPtr __asm__( "esp" )
#endif
#elif defined(__SNC__)
#define GetStackPtr( pStackPtr )	byte *pStackPtr = (byte*)__builtin_frame_address(0)
#else
#error
#endif
#endif

#ifdef _M_X64
#define _REGISTER_ALIGNMENT 16ull

int CalcAlignOffset( const unsigned char *p )
{
	return static_cast<int>( AlignValue( p, _REGISTER_ALIGNMENT ) - p );
}

#endif


//-----------------------------------------------------------------------------
// Purpose: single coroutine descriptor
//-----------------------------------------------------------------------------
#if defined( _PS3 ) && defined( _DEBUG )
byte rgStackTempBuffer[65535];
#endif
class CCoroutine
{
public:

	CCoroutine()
	{
		m_pSavedStack = NULL;
		m_pStackHigh = m_pStackLow = NULL;
		m_cubSavedStack = 0;
		m_pFunc = NULL;
		m_pchName = "(none)";
		m_iJumpCode = 0;
		m_pchDebugMsg = NULL;
#ifdef COROUTINE_TRACE
		m_hCoroutine = -1;
#endif
#ifdef _M_X64
		m_nAlignmentBytes = CalcAlignOffset( m_rgubRegisters );
#endif	
#if defined( VPROF_ENABLED )
		m_pVProfNodeScope = NULL;
#endif
	}

	jmp_buf &GetRegisters()
	{
#ifdef _M_X64
		// Did we get moved in memory in such a way that the registers became unaligned?
		// If so, fix them up now
		size_t align = _REGISTER_ALIGNMENT - 1;
		unsigned char *pRegistersCur = &m_rgubRegisters[m_nAlignmentBytes];
		if ( (size_t)pRegistersCur & align )
		{
			m_nAlignmentBytes = CalcAlignOffset( m_rgubRegisters );
			unsigned char *pRegistersNew = &m_rgubRegisters[m_nAlignmentBytes];
			Q_memmove( pRegistersNew, pRegistersCur, sizeof(jmp_buf) );
			pRegistersCur = pRegistersNew;
		}

		return *reinterpret_cast<jmp_buf *>( pRegistersCur );
#else
		return m_Registers;
#endif
	}

	~CCoroutine()
	{
		if ( m_pSavedStack )
		{
			FreePv( m_pSavedStack );
		}
	}

	FORCEINLINE void RestoreStack()
	{
		if ( m_cubSavedStack )
		{
			Assert( m_pStackHigh );
			Assert( m_pSavedStack );
			
#if defined( _PS3 ) && defined( _DEBUG )
			// Our (and Sony's) memory tracking tools may try to walk the stack during a free() call
			// if we do the free here at our normal point though the stack is invalid since it's in 
			// the middle of swapping.  Instead move it to a temp buffer now and free  while the stack 
			// frames in place are still ok.
			Assert( m_cubSavedStack < Q_ARRAYSIZE( rgStackTempBuffer ) );
			memcpy( &rgStackTempBuffer[0], m_pSavedStack, m_cubSavedStack );

			FreePv( m_pSavedStack );
			m_pSavedStack = &rgStackTempBuffer[0];
#endif

			// Assert we're not about to trash our own immediate stack
			GetStackPtr( pStack );
			if ( pStack >= m_pStackLow && pStack <= m_pStackHigh )
			{
				CoroutineDbgMsg( g_fmtstr.sprintf( "Restoring stack over ESP (%x, %x, %x)\n", pStack, m_pStackLow, m_pStackHigh ) );
				AssertMsg3( false, "Restoring stack over ESP (%p, %p, %p)\n", pStack, m_pStackLow, m_pStackHigh );
			}

			// Make sure we can access the our instance pointer after restoring the stack. This function is inlined, so the compiler could decide to
			// use an existing coroutine pointer that is already on the stack from the previous function (does so on the PS3), and will be overwritten
			// when we memcpy below. Any allocations here should be ok, as the caller should have advanced the stack past the stack area where the
			// new stack will be copied
			CCoroutine *pThis = (CCoroutine*)stackalloc( sizeof( CCoroutine* ) );
			pThis = this;
			
			RW_MEMORY_BARRIER;
			memcpy( m_pStackLow, m_pSavedStack, m_cubSavedStack );

			// WARNING: The stack has been replaced.. do not use previous stack variables or this

#ifdef CHECK_STACK_CORRUPTION
			MD5Init( &pThis->m_md52 );
			MD5Update( &pThis->m_md52, pThis->m_pStackLow, pThis->m_cubSavedStack );
			MD5Final( pThis->m_digest2, &pThis->m_md52 );
			Assert( 0 == Q_memcmp( pThis->m_digest, pThis->m_digest2, MD5_DIGEST_LENGTH  ) );

#endif

			// free the saved stack info
			pThis->m_cubSavedStack = 0;
#if !defined( _PS3 ) || !defined( _DEBUG )
			FreePv( pThis->m_pSavedStack );
#endif
			pThis->m_pSavedStack = NULL;

			// If we were the "main thread", reset our stack pos to zero
			if ( NULL == pThis->m_pFunc )
			{
				pThis->m_pStackLow = pThis->m_pStackHigh = 0;
			}

			// resume accounting against the vprof node we were in when we yielded
			// Make sure we are added after the coroutine we just copied onto the stack
#if defined( VPROF_ENABLED )
			pThis->m_pVProfNodeScope = g_VProfCurrentProfile.GetCurrentNode();

			if ( g_VProfCurrentProfile.IsEnabled() )
			{
				FOR_EACH_VEC_BACK( pThis->m_vecProfNodeStack, i )
				{
					g_VProfCurrentProfile.EnterScope( 
						pThis->m_vecProfNodeStack[i]->GetName(), 
						0,  
						g_VProfCurrentProfile.GetBudgetGroupName( pThis->m_vecProfNodeStack[i]->GetBudgetGroupID() ), 
						false, 
						g_VProfCurrentProfile.GetBudgetGroupFlags( pThis->m_vecProfNodeStack[i]->GetBudgetGroupID() ) 
					);
				}
			}

			pThis->m_vecProfNodeStack.Purge();
#endif
		}
	}

	FORCEINLINE void SaveStack()
	{
		MEM_ALLOC_CREDIT_( "Coroutine saved stack" );
		if ( m_pSavedStack )
		{
			FreePv( m_pSavedStack );
		}


		GetStackPtr( pLocal );
    
		m_pStackLow = pLocal;
		m_cubSavedStack = (m_pStackHigh - m_pStackLow);
		m_pSavedStack = (byte *)PvAlloc( m_cubSavedStack );

		// if you hit this assert, it's because you're allocating way too much stuff on the stack in your job
		// check you haven't got any overly large string buffers allocated on the stack
		Assert( m_cubSavedStack < k_cubMaxCoroutineStackSize );

#if defined( VPROF_ENABLED )
		// Exit any current vprof scope when we yield, and remember the vprof stack so we can restore it when we run again
		m_vecProfNodeStack.RemoveAll();

		CVProfNode *pCurNode = g_VProfCurrentProfile.GetCurrentNode();
		while ( pCurNode && m_pVProfNodeScope && pCurNode != m_pVProfNodeScope && pCurNode != g_VProfCurrentProfile.GetRoot() )
		{
			m_vecProfNodeStack.AddToTail( pCurNode );
			g_VProfCurrentProfile.ExitScope();
			pCurNode = g_VProfCurrentProfile.GetCurrentNode();
		}

		m_pVProfNodeScope = NULL;
#endif

		RW_MEMORY_BARRIER;
		// save the stack in the newly allocated slot
		memcpy( m_pSavedStack, m_pStackLow, m_cubSavedStack );

#ifdef CHECK_STACK_CORRUPTION
		MD5Init( &m_md5 );
		MD5Update( &m_md5, m_pSavedStack, m_cubSavedStack );
		MD5Final( m_digest, &m_md5 );
#endif
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		validator.Push( "CCoroutine", this, pchName );
		validator.ClaimMemory( m_pSavedStack );
		validator.Pop();
	}
#endif

#ifdef _M_X64
	unsigned char m_rgubRegisters[sizeof(jmp_buf) + _REGISTER_ALIGNMENT];
	int m_nAlignmentBytes;
#else
	jmp_buf m_Registers;
#endif

	byte *m_pStackHigh;		// position of initial entry to the coroutine (stack ptr before continue is ran)
	byte *m_pStackLow;		// low point on the stack we plan on saving (stack ptr when we yield)
	byte *m_pSavedStack;	// pointer to the saved stack (allocated on heap)
	int m_cubSavedStack;	// amount of data on stack
	const char *m_pchName;
	int m_iJumpCode;
	const char *m_pchDebugMsg;

#ifdef COROUTINE_TRACE
	HCoroutine m_hCoroutine;	// for debugging
#endif

	CoroutineFunc_t m_pFunc;
	void *m_pvParam;
#if defined( VPROF_ENABLED )
	CUtlVector<CVProfNode *> m_vecProfNodeStack;
	CVProfNode *m_pVProfNodeScope;
#endif

#ifdef CHECK_STACK_CORRUPTION
	MD5Context_t m_md5;
	unsigned char m_digest[MD5_DIGEST_LENGTH];
	MD5Context_t m_md52;
	unsigned char m_digest2[MD5_DIGEST_LENGTH];
#endif
};

//-----------------------------------------------------------------------------
// Purpose: manages list of all coroutines
//-----------------------------------------------------------------------------
class CCoroutineMgr
{
public:
	CCoroutineMgr()
	{
		m_topofexceptionchain = 0;

		// reserve the 0 index as the main coroutine
		HCoroutine hMainCoroutine = m_ListCoroutines.AddToTail();

		m_ListCoroutines[hMainCoroutine].m_pchName = "(main)";
#ifdef COROUTINE_TRACE
		m_ListCoroutines[hMainCoroutine].m_hCoroutine = hMainCoroutine;
#endif

		// mark it as currently running
		m_VecCoroutineStack.AddToTail( hMainCoroutine );
	}

	HCoroutine CreateCoroutine( CoroutineFunc_t pFunc, void *pvParam )
	{
		HCoroutine hCoroutine = m_ListCoroutines.AddToTail();

		CoroutineDbgMsg( g_fmtstr.sprintf( "Coroutine_Create() hCoroutine = %x pFunc = 0x%x pvParam = 0x%x\n", hCoroutine, pFunc, pvParam ) );

		m_ListCoroutines[hCoroutine].m_pFunc = pFunc;
		m_ListCoroutines[hCoroutine].m_pvParam = pvParam;
		m_ListCoroutines[hCoroutine].m_pSavedStack = NULL;
		m_ListCoroutines[hCoroutine].m_cubSavedStack = 0;
		m_ListCoroutines[hCoroutine].m_pStackHigh = m_ListCoroutines[hCoroutine].m_pStackLow = NULL;
		m_ListCoroutines[hCoroutine].m_pchName = "(no name set)";
#ifdef COROUTINE_TRACE
		m_ListCoroutines[hCoroutine].m_hCoroutine = hCoroutine;
#endif

		return hCoroutine;
	}

	HCoroutine GetActiveCoroutineHandle()
	{
		// look up the coroutine of the last item on the stack
		return m_VecCoroutineStack[m_VecCoroutineStack.Count() - 1];
	}

	CCoroutine &GetActiveCoroutine()
	{
		// look up the coroutine of the last item on the stack
		return m_ListCoroutines[GetActiveCoroutineHandle()];
	}

	CCoroutine &GetPreviouslyActiveCoroutine()
	{
		// look up the coroutine that ran the current coroutine
		return m_ListCoroutines[m_VecCoroutineStack[m_VecCoroutineStack.Count() - 2]];
	}

	bool IsValidCoroutine( HCoroutine hCoroutine )
	{
		return m_ListCoroutines.IsValidIndex( hCoroutine ) && hCoroutine > 0;
	}

	void SetActiveCoroutine( HCoroutine hCoroutine )
	{
		m_VecCoroutineStack.AddToTail( hCoroutine );
	}

	void PopCoroutineStack()
	{
		Assert( m_VecCoroutineStack.Count() > 1 );
		m_VecCoroutineStack.Remove( m_VecCoroutineStack.Count() - 1 );
	}

	bool IsAnyCoroutineActive()
	{
		return m_VecCoroutineStack.Count() > 1;
	}

	void DeleteCoroutine( HCoroutine hCoroutine )
	{
		m_ListCoroutines.Remove( hCoroutine );
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		validator.Push( "CCoroutineMgr", this, pchName );

		ValidateObj( m_ListCoroutines );
		FOR_EACH_LL( m_ListCoroutines, iRoutine )
		{
			ValidateObj( m_ListCoroutines[iRoutine] );
		}
		ValidateObj( m_VecCoroutineStack );

		validator.Pop();
	}
#endif // DBGFLAG_VALIDATE

	uint32 m_topofexceptionchain;

private:
	CUtlLinkedList<CCoroutine, HCoroutine> m_ListCoroutines;
	CUtlVector<HCoroutine> m_VecCoroutineStack;
};

CThreadLocalPtr< CCoroutineMgr > g_ThreadLocalCoroutineMgr;

CUtlVector< CCoroutineMgr * > g_VecPCoroutineMgr;
CThreadMutex g_ThreadMutexCoroutineMgr;

CCoroutineMgr &GCoroutineMgr()
{
	if ( !g_ThreadLocalCoroutineMgr )
	{
		AUTO_LOCK( g_ThreadMutexCoroutineMgr );
		g_ThreadLocalCoroutineMgr = new CCoroutineMgr();
		g_VecPCoroutineMgr.AddToTail( g_ThreadLocalCoroutineMgr );
	}
	
	return *g_ThreadLocalCoroutineMgr;
}


//-----------------------------------------------------------------------------
// Purpose: call when a thread is quiting to release any per-thread memory
//-----------------------------------------------------------------------------
void Coroutine_ReleaseThreadMemory()
{
	AUTO_LOCK( g_ThreadMutexCoroutineMgr );

	if ( g_ThreadLocalCoroutineMgr != NULL )
	{
		int iCoroutineMgr = g_VecPCoroutineMgr.Find( g_ThreadLocalCoroutineMgr );
		delete g_VecPCoroutineMgr[iCoroutineMgr];
		g_VecPCoroutineMgr.Remove( iCoroutineMgr );
	}
}


// predecs
void Coroutine_Launch( CCoroutine &coroutine );
void Coroutine_Finish();


//-----------------------------------------------------------------------------
// Purpose: Creates a soroutine, specified by the function, returns a handle
//-----------------------------------------------------------------------------
HCoroutine Coroutine_Create( CoroutineFunc_t pFunc, void *pvParam )
{
	return GCoroutineMgr().CreateCoroutine( pFunc, pvParam );
}


//-----------------------------------------------------------------------------
// Purpose: Continues a current coroutine
// input:	hCoroutine -		the coroutine to continue
//			pchDebugMsg -		if non-NULL, it will generate an assertion in
//								that coroutine, then that coroutine will 
//								immediately yield back to this thread
//-----------------------------------------------------------------------------
static const char *k_pchDebugMsg_GenericBreak = (const char *)1;

bool Internal_Coroutine_Continue( HCoroutine hCoroutine, const char *pchDebugMsg, const char *pchName )
{
	Assert( GCoroutineMgr().IsValidCoroutine(hCoroutine) );

	bool bInCoroutineAlready = GCoroutineMgr().IsAnyCoroutineActive();

#ifdef _WIN32
#ifndef _WIN64
	// make sure nobody has a try/catch block and then yielded
	// because we hate that and we will crash
	uint32 topofexceptionchain;
	__asm mov eax, dword ptr fs:[0]
	__asm mov topofexceptionchain, eax
	if ( GCoroutineMgr().m_topofexceptionchain == 0 )
		GCoroutineMgr().m_topofexceptionchain = topofexceptionchain;
	else
	{
		Assert( topofexceptionchain == GCoroutineMgr().m_topofexceptionchain );
	}
#endif
#endif

	// start the new coroutine
	GCoroutineMgr().SetActiveCoroutine( hCoroutine );

	CCoroutine &coroutinePrev = GCoroutineMgr().GetPreviouslyActiveCoroutine();
	CCoroutine &coroutine = GCoroutineMgr().GetActiveCoroutine();
	if ( pchName )
		coroutine.m_pchName = pchName;

	CoroutineDbgMsg( g_fmtstr.sprintf( "Coroutine_Continue() %s#%x -> %s#%x\n", coroutinePrev.m_pchName, coroutinePrev.m_hCoroutine, coroutine.m_pchName, coroutine.m_hCoroutine ) );

	bool bStillRunning = true;

	// set the point for the coroutine to jump back to
	RW_MEMORY_BARRIER;
	int iResult = setjmp( coroutinePrev.GetRegisters() );
	if ( iResult == k_iSetJmpStateSaved )
	{
		// copy the new stack in place
		if ( coroutine.m_pSavedStack )
		{
			// save any of the main stack that overlaps where the coroutine stack is going to go
			GetStackPtr( pStackSavePoint );
			if ( pStackSavePoint <= coroutine.m_pStackHigh )
			{
				// save the main stack from where the coroutine stack wishes to start
				// if the previous coroutine already had a stack save point, just save
				// the whole thing.
				if ( NULL == coroutinePrev.m_pStackHigh )
				{
					coroutinePrev.m_pStackHigh = coroutine.m_pStackHigh;
				}
				else
				{
					Assert( coroutine.m_pStackHigh <= coroutinePrev.m_pStackHigh );
				}
				coroutinePrev.SaveStack();
				CoroutineDbgMsg( g_fmtstr.sprintf( "SaveStack() %s#%x [%x - %x]\n", coroutinePrev.m_pchName, coroutinePrev.m_hCoroutine, coroutinePrev.m_pStackLow, coroutinePrev.m_pStackHigh ) );
			}			

			// If the coroutine's stack is close enough to where we are on the stack, we need to push ourselves
			// down past it, so that the memcpy() doesn't screw up the RestoreStack->memcpy call chain.
			if ( coroutine.m_pStackHigh > ( pStackSavePoint - 2048 ) )
			{
				// If the entire CR stack is above us, we don't need to pad ourselves.
				if ( coroutine.m_pStackLow < pStackSavePoint )
				{
					// push ourselves down
					int cubPush = pStackSavePoint - coroutine.m_pStackLow + 512;
					volatile byte *pvStackGap = (byte*)stackalloc( cubPush );
					pvStackGap[ cubPush-1 ] = 0xF;
					CoroutineDbgMsg( g_fmtstr.sprintf( "Adjusting stack point by %d (%x <- %x)\n", cubPush, pvStackGap, &pvStackGap[cubPush] ) );
				}
			}

			// This needs to go right here - after we've maybe padded the stack (so that iJumpCode does not
			// get stepped on) and before the RestoreStack() call (because that might step on pchDebugMsg!).
			if ( pchDebugMsg == NULL )
			{					
				coroutine.m_iJumpCode = k_iSetJmpContinue;
				coroutine.m_pchDebugMsg = NULL;
			}
			else if ( pchDebugMsg == k_pchDebugMsg_GenericBreak )
			{
				coroutine.m_iJumpCode = k_iSetJmpDbgBreak;
				coroutine.m_pchDebugMsg = NULL;
			}
			else
			{
				coroutine.m_iJumpCode = k_iSetJmpDbgBreak;
				coroutine.m_pchDebugMsg = pchDebugMsg;
			}

			// restore the coroutine stack
			CoroutineDbgMsg( g_fmtstr.sprintf( "RestoreStack() %s#%x [%x - %x] (current %x)\n", coroutine.m_pchName, coroutine.m_hCoroutine, coroutine.m_pStackLow, coroutine.m_pStackHigh, pStackSavePoint ) );
			coroutine.RestoreStack();
			
			// the new stack is in place, so no code here can reference local stack vars
			// move the program counter
			RW_MEMORY_BARRIER;
			Coroutine_longjmp( GCoroutineMgr().GetActiveCoroutine().GetRegisters(), GCoroutineMgr().GetActiveCoroutine().m_iJumpCode );
		}
		else
		{

			// set the stack pos for the new coroutine
			// jump a long way forward on the stack
			// this needs to be a stackalloc() instead of a static buffer, so it won't get optimized out in release build
			int cubGap = bInCoroutineAlready ? k_cubCoroutineStackGapSmall : k_cubCoroutineStackGap;
			volatile byte *pvStackGap = (byte*)stackalloc( cubGap );
			pvStackGap[ cubGap-1 ] = 0xF;

			// hasn't started yet, so launch
			Coroutine_Launch( coroutine );
		}

		// when the job yields, the above setjmp() will be called again with non-zero value
		// code here will never run
	}
	else if ( iResult == k_iSetJmpContinue )
	{
		// just pass through
	}
	else if ( iResult == k_iSetJmpDone )
	{
		// we're done, remove the coroutine
		GCoroutineMgr().DeleteCoroutine( Coroutine_GetCurrentlyActive() );
		bStillRunning = false;
	}

	// job has suspended itself, we'll get back to it later
	GCoroutineMgr().PopCoroutineStack();
	return bStillRunning;
}


//-----------------------------------------------------------------------------
// Purpose: Continues a current coroutine
//-----------------------------------------------------------------------------
bool Coroutine_Continue( HCoroutine hCoroutine, const char *pchName )
{
	return Internal_Coroutine_Continue( hCoroutine, NULL, pchName );
}


//-----------------------------------------------------------------------------
// Purpose: launches a coroutine way ahead on the stack
//-----------------------------------------------------------------------------
void NOINLINE Coroutine_Launch( CCoroutine &coroutine ) 
{
#if defined( VPROF_ENABLED )
	coroutine.m_pVProfNodeScope = g_VProfCurrentProfile.GetCurrentNode();
#endif

	// set our marker
#ifndef _PS3
	GetStackPtr( pEsp );
#else
	// The stack pointer for the current stack frame points to the top of the stack which already includes space for the 
	// ABI linkage area. We need to include this area as part of our coroutine stack, as the calling function will copy
	// the link register (return address to this function) into this area after calling m_pFunc below. Failing to do so
	// could result in the coroutine to return to garbage when complete
	uint64 *pStackFrameTwoUp = (uint64*)__builtin_frame_address(2);

	// Need to terminate the stack frame sequence so if someone tries to walk the stack in a co-routine they don't go forever.
	*pStackFrameTwoUp = 0;	

	// Need to track where we we save up to on yield, add a few bytes so we save just the beginning linkage area of the stack frame 
	// we added  the null termination to.
	byte * pEsp = ((byte*)pStackFrameTwoUp)+32;

#endif
	#ifdef _WIN64
		// Add a little extra padding, to capture the spill space for the registers
		// that is required for us to reserve ABOVE the return address), and also
		// align the stack
		coroutine.m_pStackHigh = (byte *)( ((uintptr_t)pEsp + 32 + 15) & ~(uintptr_t)15 );

		// On Win64, we need to be able to find an exception handler
		// if we walk the stack to this point.  Currently,
		// this is as close to the root as we can go.  If we
		// try to go higher, we wil fail.  That's actually
		// OK at run time, because Coroutine_Finish doesn't
		// return!
		CatchAndWriteMiniDumpForVoidPtrFn( coroutine.m_pFunc, coroutine.m_pvParam, /*bExitQuietly*/ true );
	#else
		coroutine.m_pStackHigh = (byte *)pEsp;

		// run the function directly
		coroutine.m_pFunc( coroutine.m_pvParam );
	#endif

	// longjmp back to the main 'thread'
	Coroutine_Finish();
}


//-----------------------------------------------------------------------------
// Purpose: cancels a currently running coroutine
//-----------------------------------------------------------------------------
void Coroutine_Cancel( HCoroutine hCoroutine )
{
	GCoroutineMgr().DeleteCoroutine( hCoroutine );
}

//-----------------------------------------------------------------------------
// Purpose: cause a debug break in the specified coroutine
//-----------------------------------------------------------------------------
void Coroutine_DebugBreak( HCoroutine hCoroutine )
{
	Internal_Coroutine_Continue( hCoroutine, k_pchDebugMsg_GenericBreak, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: generate an assert (perhaps generating a minidump), with the
// specified failure message, in the specified coroutine
//-----------------------------------------------------------------------------
void Coroutine_DebugAssert( HCoroutine hCoroutine, const char *pchMsg )
{
	Assert( pchMsg );
	Internal_Coroutine_Continue( hCoroutine, pchMsg, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the code is currently running inside of a coroutine
//-----------------------------------------------------------------------------
bool Coroutine_IsActive()
{
	return GCoroutineMgr().IsAnyCoroutineActive();
}


//-----------------------------------------------------------------------------
// Purpose: returns a handle the currently active coroutine
//-----------------------------------------------------------------------------
HCoroutine Coroutine_GetCurrentlyActive()
{
	Assert( Coroutine_IsActive() );
	return GCoroutineMgr().GetActiveCoroutineHandle();
}


//-----------------------------------------------------------------------------
// Purpose: lets the main thread continue
//-----------------------------------------------------------------------------
void Coroutine_YieldToMain()
{
	// if you've hit this assert, it's because you're calling yield when not in a coroutine
	Assert( Coroutine_IsActive() );
	CCoroutine &coroutinePrev = GCoroutineMgr().GetPreviouslyActiveCoroutine();
	CCoroutine &coroutine = GCoroutineMgr().GetActiveCoroutine();
	CoroutineDbgMsg( g_fmtstr.sprintf( "Coroutine_YieldToMain() %s#%x -> %s#%x\n", coroutine.m_pchName, coroutine.m_hCoroutine, coroutinePrev.m_pchName, coroutinePrev.m_hCoroutine ) );

#ifdef _WIN32
#ifndef _WIN64
	// make sure nobody has a try/catch block and then yielded
	// because we hate that and we will crash
	uint32 topofexceptionchain;
	__asm mov eax, dword ptr fs:[0]
	__asm mov topofexceptionchain, eax
		if ( GCoroutineMgr().m_topofexceptionchain == 0 )
			GCoroutineMgr().m_topofexceptionchain = topofexceptionchain;
		else
		{
			Assert( topofexceptionchain == GCoroutineMgr().m_topofexceptionchain );
		}
#endif
#endif

	RW_MEMORY_BARRIER;
	int iResult = setjmp( coroutine.GetRegisters() );
	if ( ( iResult == k_iSetJmpStateSaved ) || ( iResult == k_iSetJmpDbgBreak ) )
	{

		
		// break / assert requested?
		if ( iResult == k_iSetJmpDbgBreak )
		{
			// Assert (minidump) requested?
			if ( coroutine.m_pchDebugMsg )
			{
				// Generate a failed assertion
				AssertMsg1( !"Coroutine assert requested", "%s", coroutine.m_pchDebugMsg );
			}
			else
			{
				// If we were loaded only to debug, call a break
				DebuggerBreakIfDebugging();
			}

			// Now IMMEDIATELY yield back to the main thread
		}

		// Clear message, regardless
		coroutine.m_pchDebugMsg = NULL;

		// save our stack - all the way to the top, err bottom err, the end of it ( where esp is )
		coroutine.SaveStack();
		CoroutineDbgMsg( g_fmtstr.sprintf( "SaveStack() %s#%x [%x - %x]\n", coroutine.m_pchName, coroutine.m_hCoroutine, coroutine.m_pStackLow, coroutine.m_pStackHigh ) );

		// restore the main thread stack
		// allocate a bunch of stack padding so we don't kill ourselves while in stack restoration
		// If the coroutine's stack is close enough to where we are on the stack, we need to push ourselves
		// down past it, so that the memcpy() doesn't screw up the RestoreStack->memcpy call chain.
		GetStackPtr( pStackPtr );
		if ( pStackPtr >= (coroutinePrev.m_pStackHigh - coroutinePrev.m_cubSavedStack) && ( pStackPtr - 2048 ) <= coroutinePrev.m_pStackHigh )
		{
			int cubPush = coroutinePrev.m_cubSavedStack + 512;
			volatile byte *pvStackGap = (byte*)stackalloc( cubPush );
			pvStackGap[ cubPush - 1 ] = 0xF;
			CoroutineDbgMsg( g_fmtstr.sprintf( "Adjusting stack point by %d (%x <- %x)\n", cubPush, pvStackGap, &pvStackGap[cubPush] ) );
		}

		CoroutineDbgMsg( g_fmtstr.sprintf( "RestoreStack() %s#%x [%x - %x]\n", coroutinePrev.m_pchName, coroutinePrev.m_hCoroutine, coroutinePrev.m_pStackLow, coroutinePrev.m_pStackHigh ) );
		coroutinePrev.RestoreStack();

		// jump back to the main thread
		// Our stack may have been mucked with, can't use local vars anymore!
		RW_MEMORY_BARRIER;
		Coroutine_longjmp( GCoroutineMgr().GetPreviouslyActiveCoroutine().GetRegisters(), k_iSetJmpContinue );

		UNREACHABLE();
	}
	else
	{
		// we've been restored, now continue on our merry way
	}
}

//-----------------------------------------------------------------------------
// Purpose: done with the Coroutine, terminate safely
//-----------------------------------------------------------------------------
void Coroutine_Finish()
{
	Assert( Coroutine_IsActive() );

	CoroutineDbgMsg( g_fmtstr.sprintf( "Coroutine_Finish() %s#%x -> %s#%x\n", GCoroutineMgr().GetActiveCoroutine().m_pchName, GCoroutineMgr().GetActiveCoroutineHandle(), GCoroutineMgr().GetPreviouslyActiveCoroutine().m_pchName, &GCoroutineMgr().GetPreviouslyActiveCoroutine() ) );

	// allocate a bunch of stack padding so we don't kill ourselves while in stack restoration
	volatile byte *pvStackGap = (byte*)stackalloc( GCoroutineMgr().GetPreviouslyActiveCoroutine().m_cubSavedStack + 512 );
	pvStackGap[ GCoroutineMgr().GetPreviouslyActiveCoroutine().m_cubSavedStack + 511 ] = 0xf;

	GCoroutineMgr().GetPreviouslyActiveCoroutine().RestoreStack();

	RW_MEMORY_BARRIER;
	// go back to the main thread, signaling that we're done
	Coroutine_longjmp( GCoroutineMgr().GetPreviouslyActiveCoroutine().GetRegisters(), k_iSetJmpDone );

	UNREACHABLE();
}

//-----------------------------------------------------------------------------
// Purpose: Coroutine that spawns another coroutine
//-----------------------------------------------------------------------------
void CoroutineTestFunc( void *pvRelaunch )
{
	static const char *g_pchTestString = "test string";

	char rgchT[256];
	Q_strncpy( rgchT, g_pchTestString, sizeof(rgchT) );

	// yield
	Coroutine_YieldToMain();

	// ensure the string is still valid
	DbgVerifyNot( Q_strcmp( rgchT, g_pchTestString ) );

	if ( !pvRelaunch )
	{
		// test launching coroutines inside of coroutines
		HCoroutine hCoroutine = Coroutine_Create( &CoroutineTestFunc, (void *)(size_t)0xFFFFFFFF );
		// first pass the coroutines should all still be running
		DbgVerify( Coroutine_Continue( hCoroutine, NULL ) );
		// second pass the coroutines should all be finished
		DbgVerifyNot( Coroutine_Continue( hCoroutine, NULL ) );
	}
}


// test that just spins a few times
void CoroutineTestL2( void * )
{
	// spin a few times
	for ( int i = 0; i < 5; i++ )
	{
		Coroutine_YieldToMain();
	}
}


// level 1 of a test
void CoroutineTestL1( void *pvecCoroutineL2 )
{
	CUtlVector<HCoroutine> &vecCoroutineL2 = *(CUtlVector<HCoroutine> *)pvecCoroutineL2;

	int i = 20;

	// launch a set of coroutines
	for ( i = 0; i < 20; i++ )
	{
		HCoroutine hCoroutine = Coroutine_Create( &CoroutineTestL2, NULL );
		vecCoroutineL2.AddToTail( hCoroutine );
		Coroutine_Continue( hCoroutine, NULL );

		// now yield back to main occasionally
		if ( i % 2 == 1 )
			Coroutine_YieldToMain();
	}

	Assert( i == 20 );
}


//-----------------------------------------------------------------------------
// Purpose: runs a self-test of the coroutine system
//			it's working if it doesn't crash
//-----------------------------------------------------------------------------
bool Coroutine_Test()
{
	// basic calling of a  coroutine
	HCoroutine hCoroutine = Coroutine_Create( &CoroutineTestFunc, NULL );
	Coroutine_Continue( hCoroutine, NULL );
	Coroutine_Continue( hCoroutine, NULL );

	// now test 
	CUtlVector<HCoroutine> vecCoroutineL2;
	hCoroutine = Coroutine_Create( &CoroutineTestL1, &vecCoroutineL2 );
	Coroutine_Continue( hCoroutine, NULL );

	// run the sub-coroutines until they're all done
	while ( vecCoroutineL2.Count() )
	{
		if ( hCoroutine && !Coroutine_Continue( hCoroutine, NULL ) )
			hCoroutine = NULL;

		FOR_EACH_VEC_BACK( vecCoroutineL2, i )
		{
			if ( !Coroutine_Continue( vecCoroutineL2[i], NULL ) )
				vecCoroutineL2.Remove( i );
		}
	}


	// new one
	hCoroutine = Coroutine_Create( &CoroutineTestFunc, NULL );
	// it has yielded, now continue it's call
	{
		// pop our stack up so it collides with the coroutine stack position
		Coroutine_Continue( hCoroutine, NULL );
		volatile byte *pvAlloca = (byte*)stackalloc( k_cubCoroutineStackGapSmall );
		pvAlloca[ k_cubCoroutineStackGapSmall-1 ] = 0xF;
		
		Coroutine_Continue( hCoroutine, NULL );
	}

	// now do a whole bunch of them
	static const int k_nSimultaneousCoroutines = 10 * 1000;
	CUtlVector<HCoroutine> coroutines;
	Assert( coroutines.Base() == NULL );
	for (int i = 0; i < k_nSimultaneousCoroutines; i++)
	{
		coroutines.AddToTail( Coroutine_Create( &CoroutineTestFunc, NULL ) );
	}

	for (int i = 0; i < coroutines.Count(); i++)
	{
		// first pass the coroutines should all still be running
		DbgVerify( Coroutine_Continue( coroutines[i], NULL ) );
	}

	for (int i = 0; i < coroutines.Count(); i++)
	{
		// second pass the coroutines should all be finished
		DbgVerifyNot( Coroutine_Continue( coroutines[i], NULL ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns approximate stack depth of current coroutine.  
//-----------------------------------------------------------------------------
size_t Coroutine_GetStackDepth()
{
	// should only get called from a coroutine
	Assert( GCoroutineMgr().IsAnyCoroutineActive() );
	if ( !GCoroutineMgr().IsAnyCoroutineActive() )
		return 0;

	GetStackPtr( pLocal );
	CCoroutine &coroutine = GCoroutineMgr().GetActiveCoroutine();
	return ( coroutine.m_pStackHigh - pLocal );
}


//-----------------------------------------------------------------------------
// Purpose: validates memory
//-----------------------------------------------------------------------------
void Coroutine_ValidateGlobals( class CValidator &validator )
{
#ifdef DBGFLAG_VALIDATE
	AUTO_LOCK( g_ThreadMutexCoroutineMgr );

	for ( int i = 0; i < g_VecPCoroutineMgr.Count(); i++ )
	{
		ValidatePtr( g_VecPCoroutineMgr[i] );
	}
	ValidateObj( g_VecPCoroutineMgr );

#endif
}
