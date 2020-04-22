//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IMATERIALSYSTEMINTERNAL_H
#define IMATERIALSYSTEMINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"
#include "tier1/callqueue.h"
#include "tier1/memstack.h"

class IMaterialInternal;

//-----------------------------------------------------------------------------
// Special call queue that knows (a) single threaded access, and (b) all
// functions called after last function added
//-----------------------------------------------------------------------------

class CMatCallQueue
{
public:
	CMatCallQueue()
	{
		MEM_ALLOC_CREDIT_( "CMatCallQueue.m_Allocator" );
#ifdef SWDS
		m_Allocator.Init( 2*1024, 0, 0, 4 );
#else
		m_Allocator.Init( IsX360() ? 2*1024*1024 : 8*1024*1024, 64*1024, 256*1024, 4 );
#endif
		m_FunctorFactory.SetAllocator( &m_Allocator );
		m_pHead = m_pTail = NULL;
	}

	size_t GetMemoryUsed()
	{
		return m_Allocator.GetUsed();
	}

	int Count()
	{
		int i = 0;
		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			i++;
			pCurrent = pCurrent->pNext;
		}
		return i;
	}

	void CallQueued()
	{
		if ( !m_pHead )
		{
			return;
		}

		CFunctor *pFunctor;

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			pFunctor = pCurrent->pFunctor;
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = (unsigned)-1;
			}
#endif
			(*pFunctor)();
			pFunctor->Release();
			pCurrent = pCurrent->pNext;
		}
		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = NULL;
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		if ( !m_pHead )
		{
			return;
		}

		CFunctor *pFunctor;

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			pFunctor = pCurrent->pFunctor;
			pFunctor->Release();
			pCurrent = pCurrent->pNext;
		}

		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = NULL;
	}

	#define DEFINE_MATCALLQUEUE_NONMEMBER_QUEUE_CALL(N) \
		template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	#define DEFINE_MATCALLQUEUE_MEMBER_QUEUE_CALL(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	#define DEFINE_MATCALLQUEUE_CONST_MEMBER_QUEUE_CALL(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_NONMEMBER_QUEUE_CALL );
	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_MEMBER_QUEUE_CALL );
	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_CONST_MEMBER_QUEUE_CALL );

private:
	void QueueFunctorInternal( CFunctor *pFunctor )
	{
#ifdef _DEBUG
		pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
		MEM_ALLOC_CREDIT_( "CMatCallQueue.m_Allocator" );
		Elem_t *pNew = (Elem_t *)m_Allocator.Alloc( sizeof(Elem_t) );
		if ( m_pTail )
		{
			m_pTail->pNext = pNew;
			m_pTail = pNew;
		}
		else
		{
			m_pHead = m_pTail = pNew;
		}
		pNew->pNext = NULL;
		pNew->pFunctor = pFunctor;
	}

	struct Elem_t
	{
		Elem_t *pNext;
		CFunctor *pFunctor;
	};

	Elem_t *m_pHead;
	Elem_t *m_pTail;

	CMemoryStack m_Allocator;
	CCustomizedFunctorFactory<CMemoryStack, CRefCounted1<CFunctor, CRefCountServiceDestruct< CRefST > > > m_FunctorFactory;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};


class IMaterialProxy;


//-----------------------------------------------------------------------------
// Additional interfaces used internally to the library
//-----------------------------------------------------------------------------
abstract_class IMaterialSystemInternal : public IMaterialSystem
{
public:
	// Returns the current material
	virtual IMaterial* GetCurrentMaterial() = 0;

	virtual int GetLightmapPage( void ) = 0;

	// Gets the maximum lightmap page size...
	virtual int GetLightmapWidth( int lightmap ) const = 0;
	virtual int GetLightmapHeight( int lightmap ) const = 0;

	virtual ITexture *GetLocalCubemap( void ) = 0;

//	virtual bool RenderZOnlyWithHeightClipEnabled( void ) = 0;
	virtual void ForceDepthFuncEquals( bool bEnable ) = 0;
	virtual enum MaterialHeightClipMode_t GetHeightClipMode( void ) = 0;

	// FIXME: Remove? Here for debugging shaders in CShaderSystem
	virtual void AddMaterialToMaterialList( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterial( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterialSubRect( IMaterialInternal *pMaterial ) = 0;
	virtual bool InFlashlightMode() const = 0;

	// Can we use editor materials?
	virtual bool CanUseEditorMaterials() const = 0;
	virtual const char *GetForcedTextureLoadPathID() = 0;

	virtual CMatCallQueue *GetRenderCallQueue() = 0;

	virtual void UnbindMaterial( IMaterial *pMaterial ) = 0;
	virtual uint GetRenderThreadId() const = 0 ;

	virtual IMaterialProxy	*DetermineProxyReplacements( IMaterial *pMaterial, KeyValues *pFallbackKeyValues ) = 0;
};


#endif // IMATERIALSYSTEMINTERNAL_H
