//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_STACK_H
#define DATATABLE_STACK_H
#ifdef _WIN32
#pragma once
#endif


#include "dt.h"
#include "dt_recv_decoder.h"


class CSendNode;
static 	CSendProxyRecipients s_Recipients; // avoid calling constructor each time


// ----------------------------------------------------------------------------- //
//
// CDatatableStack
//
// CDatatableStack is used to walk through a datatable's tree, calling proxies
// along the way to update the current data pointer.
//
// ----------------------------------------------------------------------------- //

abstract_class CDatatableStack
{
public:
	
							CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID );

	// This must be called before accessing properties.
	void Init( bool bExplicitRoutes=false );

	// The stack is meant to be used by calling SeekToProp with increasing property
	// numbers.
	void			SeekToProp( int iProp );

	bool			IsCurProxyValid() const;
	bool			IsPropProxyValid(int iProp ) const;
	int				GetCurPropIndex() const;
	
	unsigned char*	GetCurStructBase() const;
	
	int				GetObjectID() const;

	// Derived classes must implement this. The server gets one and the client gets one.
	// It calls the proxy to move to the next datatable's data.
	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase ) = 0;


public:
	CSendTablePrecalc *m_pPrecalc;
	
	enum
	{
		MAX_PROXY_RESULTS = 256
	};

	// These point at the various values that the proxies returned. They are setup once, then 
	// the properties index them.
	unsigned char *m_pProxies[MAX_PROXY_RESULTS];
	unsigned char *m_pStructBase;
	int m_iCurProp;

protected:

	const SendProp *m_pCurProp;
	
	int m_ObjectID;

	bool m_bInitted;
};

inline bool CDatatableStack::IsPropProxyValid(int iProp ) const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[iProp]] != 0;
}

inline bool CDatatableStack::IsCurProxyValid() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]] != 0;
}

inline int CDatatableStack::GetCurPropIndex() const
{
	return m_iCurProp;
}

inline unsigned char* CDatatableStack::GetCurStructBase() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]]; 
}

inline void CDatatableStack::SeekToProp( int iProp )
{
	Assert( m_bInitted );
	
	m_iCurProp = iProp;
	m_pCurProp = m_pPrecalc->GetProp( iProp );
}

inline int CDatatableStack::GetObjectID() const
{
	return m_ObjectID;
}


// This can be used IF you called Init() with true for bExplicitRoutes.
// It is faster to use this route if you only are going to ask for a couple props.
// If you're going to ask for all the props, then you shouldn't use the "explicit" route.
template< class DTStack, class ProxyCaller >
inline unsigned char* UpdateRoutesExplicit_Template( DTStack *pStack, ProxyCaller *caller )
{
	// Early out.
	unsigned short iPropProxyIndex = pStack->m_pPrecalc->m_PropProxyIndices[pStack->m_iCurProp];
	unsigned char **pTest = &pStack->m_pProxies[iPropProxyIndex];
	if ( *pTest != (unsigned char*)-1 )
		return *pTest;
	
	// Ok.. setup this proxy.
	unsigned char *pStructBase = pStack->m_pStructBase;
	
	CSendTablePrecalc::CProxyPath &proxyPath = pStack->m_pPrecalc->m_ProxyPaths[iPropProxyIndex];
	for ( unsigned short i=0; i < proxyPath.m_nEntries; i++ )
	{
		CSendTablePrecalc::CProxyPathEntry *pEntry = &pStack->m_pPrecalc->m_ProxyPathEntries[proxyPath.m_iFirstEntry + i];
		int iProxy = pEntry->m_iProxy;
		
		if ( pStack->m_pProxies[iProxy] == (unsigned char*)-1 )
		{
			pStack->m_pProxies[iProxy] = ProxyCaller::CallProxy( pStack, pStructBase, pEntry->m_iDatatableProp );
			if ( !pStack->m_pProxies[iProxy] )
			{
				*pTest = NULL;
				break;
			}			
		}
		
		pStructBase = pStack->m_pProxies[iProxy];
	}
	
	return pStructBase;
}


// ------------------------------------------------------------------------------------ //
// The datatable stack for a RecvTable.
// ------------------------------------------------------------------------------------ //
class CClientDatatableStack : public CDatatableStack
{
public:
						CClientDatatableStack( CRecvDecoder *pDecoder, unsigned char *pStructBase, int objectID ) :
							CDatatableStack( &pDecoder->m_Precalc, pStructBase, objectID )
						{
							m_pDecoder = pDecoder;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		const RecvProp *pProp = m_pDecoder->GetDatatableProp( iProp );

		void *pVal = NULL;

		Assert( pProp );

		// We may crash later for doing this, but at least this will allow users to watch their demos
		if ( !pProp )
			return NULL;

		pProp->GetDataTableProxyFn()( 
			pProp,
			&pVal,
			pStructBase + pProp->GetOffset(), 
			GetObjectID()
			);

		return (unsigned char*)pVal;
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[pNode->GetRecursiveProxyIndex()] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = NULL;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

	class CRecvProxyCaller
	{
	public:
		static inline unsigned char* CallProxy( CClientDatatableStack *pStack, unsigned char *pStructBase, unsigned short iDatatableProp )
		{
			const RecvProp *pProp = pStack->m_pDecoder->GetDatatableProp( iDatatableProp );

			void *pVal = NULL;
			pProp->GetDataTableProxyFn()( 
				pProp,
				&pVal, 
				pStructBase + pProp->GetOffset(), 
				pStack->m_ObjectID
				);
				
			return (unsigned char*)pVal;
		}
	};
	
	inline unsigned char* UpdateRoutesExplicit()
	{
		return UpdateRoutesExplicit_Template( this, (CRecvProxyCaller*)NULL );
	}
			

public:
	
	CRecvDecoder	*m_pDecoder;
};


class CServerDatatableStack : public CDatatableStack
{
public:
						CServerDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID ) :
							CDatatableStack( pPrecalc, pStructBase, objectID )
						{
							m_pPrecalc = pPrecalc;
							m_pRecipients = NULL;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		const SendProp *pProp = m_pPrecalc->GetDatatableProp( iProp );

		CSendProxyRecipients *pRecipients;

		if ( m_pRecipients && pNode->GetDataTableProxyIndex() != DATATABLE_PROXY_INDEX_NOPROXY )
		{
			// set recipients pointer and all clients by default
			pRecipients = &m_pRecipients->Element( pNode->GetDataTableProxyIndex() );
			pRecipients->SetAllRecipients();
		}
		else
		{
			// we don't care about recipients, just provide a valid pointer
			pRecipients = &s_Recipients; 
		}

		unsigned char *pRet = (unsigned char*)pProp->GetDataTableProxyFn()( 
			pProp,
			pStructBase, 
			pStructBase + pProp->GetOffset(), 
			pRecipients,
			GetObjectID()
			);
	
		return pRet;
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[pNode->GetRecursiveProxyIndex()] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = NULL;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

	// This can be used IF you called Init() with true for bExplicitRoutes.
	// It is faster to use this route if you only are going to ask for a couple props.
	// If you're going to ask for all the props, then you shouldn't use the "explicit" route.
	class CSendProxyCaller
	{
	public:
		static inline unsigned char* CallProxy( CServerDatatableStack *pStack, unsigned char *pStructBase, unsigned short iDatatableProp )
		{
			const SendProp *pProp = pStack->m_pPrecalc->GetDatatableProp( iDatatableProp );
			
			return (unsigned char*)pProp->GetDataTableProxyFn()( 
				pProp,
				pStructBase, 
				pStructBase + pProp->GetOffset(), 
				&s_Recipients,
				pStack->GetObjectID()
				);
		}
	};
	
	inline unsigned char* UpdateRoutesExplicit()
	{
		return UpdateRoutesExplicit_Template( this, (CSendProxyCaller*)NULL );
	}

	
	const SendProp*	GetCurProp() const;


public:
	
	CSendTablePrecalc					*m_pPrecalc;
	CUtlMemory<CSendProxyRecipients>	*m_pRecipients;
};


inline const SendProp* CServerDatatableStack::GetCurProp() const
{
	return m_pPrecalc->GetProp( GetCurPropIndex() );
}


#endif // DATATABLE_STACK_H
