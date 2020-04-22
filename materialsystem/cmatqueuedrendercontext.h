//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CMATQUEUEDRENDERCONTEXT_H
#define CMATQUEUEDRENDERCONTEXT_H

#include "imatrendercontextinternal.h"
#include "cmatrendercontext.h"
#include "tier1/callqueue.h"
#include "tier1/utlenvelope.h"
#include "tier1/memstack.h"
#include "mathlib/mathlib.h"

#include "tier0/memdbgon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifndef MATSYS_INTERNAL
#error "This file is private to the implementation of IMaterialSystem/IMaterialSystemInternal"
#endif

class CMaterialSystem;
class CMatQueuedMesh;
class CPrimList;

//-----------------------------------------------------------------------------

#define DEFINE_QUEUED_CALL_0( FuncName, ClassName, pObject ) void FuncName()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)())&ClassName::FuncName); }
#define DEFINE_QUEUED_CALL_1( FuncName, ArgType1, ClassName, pObject ) void FuncName( ArgType1 a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1))&ClassName::FuncName, a1 ); }
#define DEFINE_QUEUED_CALL_2( FuncName, ArgType1, ArgType2, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2))&ClassName::FuncName, a1, a2 ); }
#define DEFINE_QUEUED_CALL_3( FuncName, ArgType1, ArgType2, ArgType3, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3))&ClassName::FuncName, a1, a2, a3 ); }
#define DEFINE_QUEUED_CALL_4( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4))&ClassName::FuncName, a1, a2, a3, a4 ); }
#define DEFINE_QUEUED_CALL_5( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5))&ClassName::FuncName, a1, a2, a3, a4, a5 ); }
#define DEFINE_QUEUED_CALL_6( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6))&ClassName::FuncName, a1, a2, a3, a4, a5, a6 ); }
#define DEFINE_QUEUED_CALL_7( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7 ); }
#define DEFINE_QUEUED_CALL_8( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define DEFINE_QUEUED_CALL_9( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define DEFINE_QUEUED_CALL_11( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }
#define DEFINE_QUEUED_CALL_12( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11, ArgType12, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11, ArgType12 a12 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11, ArgType12))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12 ); }

#define DEFINE_QUEUED_CALL_0C( FuncName, ClassName, pObject ) void FuncName()	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)() const)ClassName::FuncName); }
#define DEFINE_QUEUED_CALL_1C( FuncName, ArgType1, ClassName, pObject ) void FuncName( ArgType1 a1 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1) const)ClassName::FuncName, a1 ); }
#define DEFINE_QUEUED_CALL_2C( FuncName, ArgType1, ArgType2, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2) const)ClassName::FuncName, a1, a2 ); }
#define DEFINE_QUEUED_CALL_3C( FuncName, ArgType1, ArgType2, ArgType3, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3) const)ClassName::FuncName, a1, a2, a3 ); }
#define DEFINE_QUEUED_CALL_4C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4) const)ClassName::FuncName, a1, a2, a3, a4 ); }
#define DEFINE_QUEUED_CALL_5C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5) const)ClassName::FuncName, a1, a2, a3, a4, a5 ); }
#define DEFINE_QUEUED_CALL_6C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6) const)ClassName::FuncName, a1, a2, a3, a4, a5, a6 ); }
#define DEFINE_QUEUED_CALL_7C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7) const)ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7 ); }
#define DEFINE_QUEUED_CALL_8C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8) const)ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define DEFINE_QUEUED_CALL_9C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9) const)ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#define DEFINE_QUEUED_CALL_AFTER_BASE_0( FuncName, ClassName, pObject ) void FuncName()	{ BaseClass::FuncName(); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)())&ClassName::FuncName ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_1( FuncName, ArgType1, ClassName, pObject ) void FuncName( ArgType1 a1 )	{ BaseClass::FuncName( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1))&ClassName::FuncName, a1 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_2( FuncName, ArgType1, ArgType2, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2 )	{ BaseClass::FuncName( a1, a2 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2))&ClassName::FuncName, a1, a2 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_3( FuncName, ArgType1, ArgType2, ArgType3, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ BaseClass::FuncName( a1, a2, a3 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3))&ClassName::FuncName, a1, a2, a3 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_4( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ BaseClass::FuncName( a1, a2, a3, a4 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4))&ClassName::FuncName, a1, a2, a3, a4 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_5( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5))&ClassName::FuncName, a1, a2, a3, a4, a5 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_6( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6))&ClassName::FuncName, a1, a2, a3, a4, a5, a6 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_7( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_8( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_9( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#define DEFINE_QUEUED_CALL_AFTER_BASE_0C( FuncName, ClassName, pObject ) void FuncName() const	{ BaseClass::FuncName(); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)())&ClassName::FuncName ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_1C( FuncName, ArgType1, ClassName, pObject ) void FuncName( ArgType1 a1 ) const	{ BaseClass::FuncName( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1))&ClassName::FuncName, a1 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_2C( FuncName, ArgType1, ArgType2, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2 ) const	{ BaseClass::FuncName( a1, a2 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2))&ClassName::FuncName, a1, a2 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_3C( FuncName, ArgType1, ArgType2, ArgType3, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 ) const	{ BaseClass::FuncName( a1, a2, a3 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3))&ClassName::FuncName, a1, a2, a3 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_4C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 ) const	{ BaseClass::FuncName( a1, a2, a3, a4 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4))&ClassName::FuncName, a1, a2, a3, a4 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_5C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 ) const	{ BaseClass::FuncName( a1, a2, a3, a4, a5 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5))&ClassName::FuncName, a1, a2, a3, a4, a5 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_6C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 ) const	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6))&ClassName::FuncName, a1, a2, a3, a4, a5, a6 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_7C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 ) const	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_8C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 ) const	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define DEFINE_QUEUED_CALL_AFTER_BASE_9C( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ClassName, pObject ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 ) const	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( pObject, (void (ClassName::*)(ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9))&ClassName::FuncName, a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

//-----------------------------------------------------------------------------

#define Unsupported( funcName ) ExecuteOnce( Msg( "CMatQueuedRenderContext: %s is unsupported\n", #funcName ) )
#define FATAL_QUEUE 1

#ifdef FATAL_QUEUE
#define CannotSupport() ExecuteOnce( Msg( "Called function that cannot be supported\n" ) ); ExecuteOnce( DebuggerBreakIfDebugging() )
#else
#define CannotSupport() ExecuteOnce( Msg( "Called function that cannot be supported\n" ) )
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CMatQueuedRenderContext : public CMatRenderContextBase
{
	typedef CMatRenderContextBase BaseClass;
public:
	CMatQueuedRenderContext()
	 :	m_pHardwareContext( NULL ), 
		m_iRenderDepth( 0 ), 
		m_pQueuedMesh( NULL ),
		m_WidthBackBuffer( 0 ), 
		m_HeightBackBuffer( 0 ),
		m_FogMode( MATERIAL_FOG_NONE ),
		m_flFogStart( 0 ),
		m_flFogEnd( 0 ),
		m_flFogZ( 0 ),
		m_flFogMaxDensity( 1.0 )
	{
		memset( &m_FogColor, 0, sizeof(m_FogColor) );
	}

	bool									Init( CMaterialSystem *pMaterialSystem, CMatRenderContextBase *pHardwareContext );
	void									Shutdown();
	void									Free();

	void									CompactMemory();

	bool									IsInitialized() const { return ( m_pHardwareContext != NULL ); }

	void									BeginQueue( CMatRenderContextBase *pInitialState = NULL );
	void									EndQueue( bool bCallQueued = false );

	void									BeginRender();
	void									EndRender();

	void									CallQueued( bool bTermAfterCall = false );
	void									FlushQueued();

	ICallQueue *							GetCallQueue();
	CMatCallQueue *							GetCallQueueInternal() { return &m_queue; }

	bool									OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool									OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool									OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool									OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool									OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type );
	bool									OnFlushBufferedPrimitives();


	DEFINE_QUEUED_CALL_1(					Flush, bool, IMatRenderContext, m_pHardwareContext );

	DEFINE_QUEUED_CALL_0(					SwapBuffers, IMatRenderContextInternal, m_pHardwareContext );

	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );
	void									GetRenderTargetDimensions( int &, int &) const;
	void									GetViewport( int& x, int& y, int& width, int& height ) const;

	void									Bind( IMaterial *, void * );
	DEFINE_QUEUED_CALL_AFTER_BASE_1(		BindLocalCubemap, ITexture *, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_1(		BindLightmapPage, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					DepthRange, float, float, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_3(					ClearBuffers, bool, bool, bool, IMatRenderContext, m_pHardwareContext );

	void									ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat );

	DEFINE_QUEUED_CALL_3(					SetAmbientLight, float, float, float, IMatRenderContext, m_pHardwareContext );

	void									SetLight( int i, const LightDesc_t &desc );

	void									SetLightingOrigin( Vector vLightingOrigin );

	void									SetAmbientLightCube( LightCube_t cube );

	DEFINE_QUEUED_CALL_1(					CopyRenderTargetToTexture, ITexture *, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_2(		SetFrameBufferCopyTexture, ITexture *, int, IMatRenderContext, m_pHardwareContext );

	// matrix api
	void									MatrixMode( MaterialMatrixMode_t);
	void									PushMatrix();
	void									PopMatrix();
	void									LoadMatrix( const VMatrix& matrix );
	void									LoadMatrix( const matrix3x4_t& matrix );
	void									MultMatrix( const VMatrix& matrix );
	void									MultMatrixLocal( const VMatrix& matrix );
	void									MultMatrix( const matrix3x4_t& matrix );
	void									MultMatrixLocal( const matrix3x4_t& matrix );
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	void									Viewport( int x, int y, int width, int height );

	DEFINE_QUEUED_CALL_1(					CullMode, MaterialCullMode_t, IMatRenderContext, m_pHardwareContext );

	void									FogMode( MaterialFogMode_t fogMode );
	void									FogStart( float fStart );
	void									FogEnd( float fEnd );
	void									SetFogZ( float fogZ );
	MaterialFogMode_t						GetFogMode( void );
	void									GetFogDistances( float *fStart, float *fEnd, float *fFogZ );
	void									FogMaxDensity( float flMaxDensity );

	void									FogColor3f( float r, float g, float b );
	void									FogColor3fv( float const* rgb );
	void									FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void									FogColor3ubv( unsigned char const* rgb );

	void									GetFogColor( unsigned char *rgb );

	int	GetCurrentNumBones( ) const;
	void SetNumBoneWeights( int nBoneCount );

	DELEGATE_TO_OBJECT_3( IMesh *,			CreateStaticMesh, VertexFormat_t, const char *, IMaterial *, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					DestroyStaticMesh, IMesh *, IMatRenderContext, m_pHardwareContext );

	IMesh *									GetDynamicMesh(bool, IMesh *, IMesh *, IMaterial * );
	IMesh*									GetDynamicMeshEx( VertexFormat_t, bool, IMesh*, IMesh*, IMaterial* );

// ------------ New Vertex/Index Buffer interface ----------------------------
	DELEGATE_TO_OBJECT_3( IVertexBuffer *,	CreateStaticVertexBuffer, VertexFormat_t, int, const char *, m_pHardwareContext );
	DELEGATE_TO_OBJECT_3( IIndexBuffer *, 	CreateStaticIndexBuffer, MaterialIndexFormat_t, int, const char *, m_pHardwareContext );
	DELEGATE_TO_OBJECT_1V(					DestroyVertexBuffer, IVertexBuffer *, m_pHardwareContext );
	DELEGATE_TO_OBJECT_1V(					DestroyIndexBuffer, IIndexBuffer *, m_pHardwareContext );
	DELEGATE_TO_OBJECT_3( IVertexBuffer *, 	GetDynamicVertexBuffer, int, VertexFormat_t, bool, m_pHardwareContext );
	DELEGATE_TO_OBJECT_2( IIndexBuffer *, 	GetDynamicIndexBuffer, MaterialIndexFormat_t, bool, m_pHardwareContext );
	DELEGATE_TO_OBJECT_7V(					BindVertexBuffer, int, IVertexBuffer *, int, int, int, VertexFormat_t, int, m_pHardwareContext );
	DELEGATE_TO_OBJECT_2V(					BindIndexBuffer, IIndexBuffer *, int, m_pHardwareContext );
	DELEGATE_TO_OBJECT_3V(					Draw, MaterialPrimitiveType_t, int, int, m_pHardwareContext );
// ------------ End ----------------------------

	int SelectionMode( bool )
	{
		CannotSupport();
		return 0;
	}

	void SelectionBuffer( unsigned int *, int )
	{
		CannotSupport();
	}

	DEFINE_QUEUED_CALL_0(					ClearSelectionNames, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					LoadSelectionName, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					PushSelectionName, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					PopSelectionName, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_3(					ClearColor3ub, unsigned char, unsigned char, unsigned char, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_4(					ClearColor4ub, unsigned char, unsigned char, unsigned char, unsigned char, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					OverrideDepthEnable, bool, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					OverrideAlphaWriteEnable, bool, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					OverrideColorWriteEnable, bool, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					DrawScreenSpaceQuad, IMaterial *, IMatRenderContext, m_pHardwareContext );

	void									SyncToken( const char *p );

	// Allocate and delete query objects.
	OcclusionQueryObjectHandle_t			CreateOcclusionQueryObject();
	DEFINE_QUEUED_CALL_1(					DestroyOcclusionQueryObject, OcclusionQueryObjectHandle_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					BeginOcclusionQueryDrawing, OcclusionQueryObjectHandle_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					EndOcclusionQueryDrawing, OcclusionQueryObjectHandle_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					ResetOcclusionQueryObject, OcclusionQueryObjectHandle_t, IMatRenderContext, m_pHardwareContext );
	int										OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t );

	void									SetFlashlightState( const FlashlightState_t &s, const VMatrix &m );

	DEFINE_QUEUED_CALL_AFTER_BASE_1(		SetHeightClipMode, MaterialHeightClipMode_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_1(		SetHeightClipZ, float, IMatRenderContext, m_pHardwareContext );

	bool									EnableClipping( bool bEnable );

	DEFINE_QUEUED_CALL_1(					EnableUserClipTransformOverride, bool, IMatRenderContext, m_pHardwareContext );

	void									UserClipTransform( const VMatrix &m );

	IMorph *CreateMorph( MorphFormat_t format, const char *pDebugName )
	{
		CannotSupport();
		return NULL;
	}

	DEFINE_QUEUED_CALL_1(					DestroyMorph, IMorph *, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					BindMorph, IMorph *, IMatRenderContext, m_pHardwareContext );

	void									ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride );

	void									GetWindowSize( int &width, int &height ) const;

	void									DrawScreenSpaceRectangle( IMaterial *pMaterial,
																		int destx, int desty,
																		int width, int height,
																		float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
																															// destx/y
																		float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
																															// destx+width-1, desty+height-1
																		int src_texture_width, int src_texture_height,		// needed for fixup
																		void *pClientRenderable = NULL,
																		int nXDice = 1,
																		int nYDice = 1 );

	void									LoadBoneMatrix( int i, const matrix3x4_t &m );

	DEFINE_QUEUED_CALL_AFTER_BASE_5(		PushRenderTargetAndViewport, ITexture *, int, int, int, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_6(		PushRenderTargetAndViewport, ITexture *, ITexture *, int, int, int, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_1(		PushRenderTargetAndViewport, ITexture *, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_0(		PushRenderTargetAndViewport, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_0(		PopRenderTargetAndViewport, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					BindLightmapTexture, ITexture *, IMatRenderContext, m_pHardwareContext );

	void									CopyRenderTargetToTextureEx( ITexture *pTexture, int i, Rect_t *pSrc, Rect_t *pDst );
	void									CopyTextureToRenderTargetEx( int i, ITexture *pTexture, Rect_t *pSrc, Rect_t *pDst );

	DEFINE_QUEUED_CALL_2(					SetFloatRenderingParameter, int, float, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					SetIntRenderingParameter, int, int, IMatRenderContext, m_pHardwareContext );

	void SetVectorRenderingParameter( int i, const Vector &v )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetVectorRenderingParameter, i, RefToVal( v ) );
	}

	float GetFloatRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return 0;
	}

	Vector GetVectorRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return Vector(0,0,0);
	}

	DEFINE_QUEUED_CALL_1(					SetStencilEnable, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilFailOperation, StencilOperation_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilZFailOperation, StencilOperation_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilPassOperation, StencilOperation_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilCompareFunction, StencilComparisonFunction_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilReferenceValue, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilTestMask, uint32, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					SetStencilWriteMask, uint32, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_5(					ClearStencilBufferRectangle, int, int, int, int, int, IMatRenderContext, m_pHardwareContext );

	DEFINE_QUEUED_CALL_2(					SetShadowDepthBiasFactors, float, float, IMatRenderContext, m_pHardwareContext );

	void PushCustomClipPlane( const float *p )
	{

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushCustomClipPlane, CUtlEnvelope<float>( p, 4 ) );
	}

	DEFINE_QUEUED_CALL_0(					PopCustomClipPlane, IMatRenderContext, m_pHardwareContext );

	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial );
	virtual int GetMaxIndicesToRender( )
	{
		return INDEX_BUFFER_SIZE;
	}

	DEFINE_QUEUED_CALL_0(					DisableAllLocalLights, IMatRenderContext, m_pHardwareContext );

	int CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightmapID1, int lightmapID2 ) 
	{ 
		CannotSupport();
		return 0; 
	}

	IMesh *GetFlexMesh();

	void SetFlashlightStateEx( const FlashlightState_t &s, const VMatrix &m, ITexture *p )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightStateEx, RefToVal( s ), RefToVal( m ), p );
	}

	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetScissorRect, nLeft, nTop, nRight, nBottom, bEnableScissor );
	}

	virtual void PushDeformation( const DeformationBase_t *pDef )
	{
		CannotSupport();
	}

	void PopDeformation( )
	{
		CannotSupport();
	}

	int GetNumActiveDeformations( ) const
	{
		return 0;
	}
	
	ITexture *GetLocalCubemap()
	{
		return m_pLocalCubemapTexture;
	}

	DEFINE_QUEUED_CALL_2(					ClearBuffersObeyStencil, bool, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_3(					ClearBuffersObeyStencilEx, bool, bool, bool, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					PerformFullScreenStencilOperation, IMatRenderContext, m_pHardwareContext );

	virtual void AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) OVERRIDE
	{
		OnAsyncCreateTextureFromRenderTarget( pSrcRt, &pDstName, pRecipient );

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::AsyncCreateTextureFromRenderTarget, pSrcRt, pDstName, dstFmt, bGenMips, nAdditionalCreationFlags, pRecipient, pExtraArgs );
	}

	virtual void							AsyncMap( ITextureInternal* pTexToMap, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) OVERRIDE
	{
		OnAsyncMap( pTexToMap, pRecipient, pExtraArgs );

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContextInternal::AsyncMap, pTexToMap, pRecipient, pExtraArgs );
	}

	virtual void							AsyncUnmap( ITextureInternal* pTexToUnmap ) OVERRIDE
	{
		OnAsyncUnmap( pTexToUnmap );

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContextInternal::AsyncUnmap, pTexToUnmap );	
	}

	virtual void AsyncCopyRenderTargetToStagingTexture( ITexture* pDst, ITexture* pSrc, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) OVERRIDE
	{
		OnAsyncCopyRenderTargetToStagingTexture( pDst, pSrc, pRecipient );

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContextInternal::AsyncCopyRenderTargetToStagingTexture, pDst, pSrc, pRecipient, pExtraArgs );
	}

	DEFINE_QUEUED_CALL_0(					TextureManagerUpdate, IMatRenderContextInternal, m_pHardwareContext );
	
	bool GetUserClipTransform( VMatrix &worldToView )
	{
		CannotSupport();
		return false;
	}

	bool InFlashlightMode( void ) const
	{
		CannotSupport();
		return false;
	}

	virtual void SetFlashlightMode( bool bEnable )
	{
		m_bFlashlightEnable = bEnable;
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightMode, bEnable );
	}

	virtual bool GetFlashlightMode( void ) const { return m_bFlashlightEnable; }

	void BeginPIXEvent( unsigned long color, const char *pszName )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BeginPIXEvent, color, CUtlEnvelope<const char *>( pszName ) );
	}

	DEFINE_QUEUED_CALL_0(					EndPIXEvent, IMatRenderContext, m_pHardwareContext );

	void SetPIXMarker( unsigned long color, const char *pszName )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetPIXMarker, color, CUtlEnvelope<const char *>( pszName ) );
	}

	DEFINE_QUEUED_CALL_0(					ForceHardwareSync, IMatRenderContextInternal, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					BeginFrame, IMatRenderContextInternal, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					EndFrame, IMatRenderContextInternal, m_pHardwareContext );

	void									SetFrameTime( float frameTime )
	{ 
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContextInternal::SetFrameTime, frameTime );
		m_FrameTime = frameTime;
	}

	DEFINE_QUEUED_CALL_0(					BeginMorphAccumulation, IMatRenderContextInternal, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					EndMorphAccumulation, IMatRenderContextInternal, m_pHardwareContext );
	virtual void AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
	{
		// FIXME: Must copy off the morph weights here. Not sure the pattern for this.
		Assert( 0 );
	}
	virtual bool GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pIMorph, int nVertex )
	{
		// FIXME: We must assign morph ids in the queued mode 
		// and pass the ids down to the morph mgr to get the texcoord
		Assert(0);
		pTexCoord->Init();
		return false;
	}

	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
	{
		Assert(0);
	}

	//-------------------------------------------------------------------------

	DEFINE_QUEUED_CALL_AFTER_BASE_1(		SetCurrentMaterialInternal, IMaterialInternal *, IMatRenderContextInternal, m_pHardwareContext );
	DEFINE_QUEUED_CALL_AFTER_BASE_1(		SetCurrentProxy, void *, IMatRenderContextInternal, m_pHardwareContext );

	int GetLightmapPage()
	{
		CannotSupport();
		return 0;
	}

	void ForceDepthFuncEquals( bool )
	{
		CannotSupport();
	}

	void BindStandardTexture( Sampler_t, StandardTextureId_t )
	{
		CannotSupport();
	}

	void GetLightmapDimensions( int *, int * )
	{
		CannotSupport();
	}

	MorphFormat_t GetBoundMorphFormat()
	{
		CannotSupport();
		return (MorphFormat_t)0;
	}

	void DrawClearBufferQuad( unsigned char, unsigned char, unsigned char, unsigned char, bool, bool, bool )
	{
		CannotSupport();
	}

	void BeginBatch( IMesh* pIndices );
	void BindBatch( IMesh* pVertices, IMaterial *pAutoBind = NULL );
	void DrawBatch(int firstIndex, int numIndices );
	void EndBatch();

	// Color correction related methods..
	// Client cannot call IColorCorrectionSystem directly because it is not thread-safe
	// FIXME: Make IColorCorrectionSystem threadsafe?
	virtual ColorCorrectionHandle_t AddLookup( const char *pName );
	virtual void LockLookup( ColorCorrectionHandle_t handle );
	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName );
	virtual void UnlockLookup( ColorCorrectionHandle_t handle );
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle );

	DEFINE_QUEUED_CALL_1( EnableColorCorrection, bool, IColorCorrectionSystem, g_pColorCorrectionSystem );
	DEFINE_QUEUED_CALL_0( ResetLookupWeights, IColorCorrectionSystem, g_pColorCorrectionSystem );
	DEFINE_QUEUED_CALL_2( SetLookupWeight, ColorCorrectionHandle_t, float, IColorCorrectionSystem, g_pColorCorrectionSystem );
	DEFINE_QUEUED_CALL_2( SetResetable, ColorCorrectionHandle_t, bool, IColorCorrectionSystem, g_pColorCorrectionSystem );

	DEFINE_QUEUED_CALL_1( SetFullScreenDepthTextureValidityFlag, bool, IMatRenderContext, m_pHardwareContext );

	void SetToneMappingScaleLinear( const Vector &scale )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetToneMappingScaleLinear, RefToVal( scale ) );
		m_LastSetToneMapScale = Vector( scale );
	}


	void DeferredBeginBatch( uint16 *pIndexData, int nIndices );
	void DeferredDrawPrimList( IMesh *pMesh, CPrimList *pLists, int nLists );
	void DeferredSetFlexMesh( IMesh *pStaticMesh, int nVertexOffsetInBytes );

#if defined( _X360 )
	DEFINE_QUEUED_CALL_1(					PushVertexShaderGPRAllocation, int, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					PopVertexShaderGPRAllocation, IMatRenderContext, m_pHardwareContext );
#endif

	// A special path used to tick the front buffer while loading on the 360
	DEFINE_QUEUED_CALL_4(					SetNonInteractivePacifierTexture, ITexture *, float, float, float, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_2(					SetNonInteractiveTempFullscreenBuffer, ITexture *, MaterialNonInteractiveMode_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_1(					EnableNonInteractiveMode, MaterialNonInteractiveMode_t, IMatRenderContext, m_pHardwareContext );
	DEFINE_QUEUED_CALL_0(					RefreshFrontBufferNonInteractive, IMatRenderContext, m_pHardwareContext );

	//--------------------------------------------------------
	// Memory allocation calls for queued mesh, et. al.
	//--------------------------------------------------------
	byte *AllocVertices( int nVerts, int nVertexSize );
	uint16 *AllocIndices( int nIndices );
	CPrimList *AllocPrimLists( int nPrimLists );
	byte *ReallocVertices( byte *pVerts, int nVertsOld, int nVertsNew, int nVertexSize );
	uint16 *ReallocIndices( uint16 *pIndices, int nIndicesOld, int nIndicesNew );
	void FreeVertices( byte *pVerts, int nVerts, int nVertexSize );
	void FreeIndices( uint16 *pIndices, int nIndices );
	void FreePrimLists( CPrimList * );

	//--------------------------------------------------------
	// debug logging - no-op in queued context
	//--------------------------------------------------------
	virtual void							Printf( PRINTF_FORMAT_STRING const char *fmt, ... ) {};
	virtual void							PrintfVA( char *fmt, va_list vargs ){};
	virtual float							Knob( char *knobname, float *setvalue=NULL ) { return 0.0f; };
	
#ifdef DX_TO_GL_ABSTRACTION
	void									DoStartupShaderPreloading( void ) {};
#endif

private:
	void QueueMatrixSync();

	//friend class CMatQueuedMesh;
	friend class CCallQueueExternal;

	CMatCallQueue m_queue;
	CMatQueuedMesh *m_pQueuedMesh;

	CMatRenderContextBase *m_pHardwareContext;

	int m_iRenderDepth;
	int m_WidthBackBuffer, m_HeightBackBuffer;
	int m_nBoneCount;
	MaterialFogMode_t m_FogMode;
	float m_flFogStart;
	float m_flFogEnd;
	float m_flFogZ;
	float m_flFogMaxDensity;
	color24 m_FogColor;

	CMemoryStack m_Vertices;
	CMemoryStack m_Indices;

	class CCallQueueExternal : public ICallQueue
	{
		void QueueFunctorInternal( CFunctor *pFunctor )
		{
			GET_OUTER( CMatQueuedRenderContext, m_CallQueueExternal )->m_queue.QueueFunctor( pFunctor );
			pFunctor->Release();
		}
	};

	CCallQueueExternal m_CallQueueExternal;
};

//-----------------------------------------------------------------------------

#include "tier0/memdbgoff.h"

#endif // CMATQUEUEDRENDERCONTEXT_H
