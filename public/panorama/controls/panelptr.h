//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANELPTR_H
#define PANELPTR_H

#ifdef _WIN32
#pragma once
#endif

#include "panelhandle.h"
#include "../iuiengine.h"
#include "../iuipanelclient.h"
#include "panorama/panoramacxx.h"
#include "generichash.h"

namespace panorama
{

class CPanel2D;

//
// Safe pointer to a panel
//
template < class T >
class CPanelPtr
{
public:
	CPanelPtr() { Clear(); }
	CPanelPtr( const CPanelPtr &rhs ) { *this = rhs; }
	CPanelPtr( const IUIPanel *pPanel ) { Set( pPanel ); }
	CPanelPtr( const IUIPanelClient *pPanel ) { Set( pPanel ? pPanel->UIPanel() : (IUIPanel*)NULL ); }

	CPanelPtr< T > &operator=( const CPanelPtr< T > &ptr ) { m_handle = ptr.m_handle; return *this; }
	T *operator=( T *pPanel ) { Set( pPanel ); return pPanel; }

	void Clear()
	{
		m_handle = PanelHandle_t::InvalidHandle();
	}

	void Set( const IUIPanel *pPanel )
	{
		if( pPanel )
			m_handle = UIEngine()->GetPanelHandle( pPanel );
		else
			Clear();
	}

	void Set( const IUIPanelClient *pPanel )
	{
		if( pPanel )
			m_handle = UIEngine()->GetPanelHandle( pPanel->UIPanel() );
		else
			Clear();
	}

	T * Get() const
	{
		if ( m_handle == PanelHandle_t::InvalidHandle() )
			return NULL;

		// allow us to be called to return NULL pointers early
		if( UIEngine() == NULL )
		{
			return NULL;
		}

		if( panorama_is_base_of< IUIPanel, T >::value )
		{ 
			T* pPanel = (T *)UIEngine()->GetPanelPtr( m_handle );
			if ( pPanel )
			{
				return pPanel;
			}
			else 
			{
				m_handle = PanelHandle_t::InvalidHandle();
				return NULL;
			}
		}
		else
		{
			IUIPanel *pPanel = UIEngine()->GetPanelPtr( m_handle );
			if( pPanel )
				return (T*)(pPanel->ClientPtr());
			else
			{
				m_handle = PanelHandle_t::InvalidHandle();
				return NULL;
			}
		}
	}

	T *operator->() const
	{
		return Get();
	}

	template < class U >
	bool operator==( const CPanelPtr< U > &rhs ) const
	{
		return ( m_handle == rhs.GetPanelHandle() );
	}

	template < class U >
	bool operator!=( const CPanelPtr< U > &rhs ) const
	{
		return !operator==( rhs );
	}
	
	template < class U >
	bool operator<( const CPanelPtr< U > &rhs) const
	{
		return m_handle < rhs.m_handle;
	}

	uint64 GetHandleAsUInt64() const
	{
		uint64 val = 0;
		val = ((uint64)m_handle.m_unSerialNumber)<<32 | m_handle.m_iPanelIndex;
		return val;
	}

	void SetFromUInt64( uint64 ulValue )
	{
		m_handle.m_unSerialNumber = ulValue>>32;
		m_handle.m_iPanelIndex = 0xFFFFFFFF & ulValue;
	}

	const PanelHandle_t &GetPanelHandle() const { return m_handle; }
	bool BPreviouslySet() { return ( m_handle != PanelHandle_t::InvalidHandle() ); }

private:
	mutable PanelHandle_t m_handle;
};

template< class T > 
inline uint32 HashItem( const CPanelPtr<T> &item )
{
#if defined( SOURCE2_PANORAMA )
	return ::HashItem( item );
#else
	return HashItemAsBytes( item );
#endif
}

} // namespace panorama


#endif // PANELPTR_H
