//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANELHANDLE_H
#define PANELHANDLE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

namespace panorama
{

const uint64 k_ulInvalidPanelHandle64 = 0x00000000FFFFFFFF;

//
// Safe handle to a panel. To get pointer to actual panel, call IUIEngine::GetPanelPtr
//
struct PanelHandle_t
{
	int32 m_iPanelIndex;			// index into panel map
	uint32 m_unSerialNumber;		// unique number used to ensure that panel at m_iPanelIndex is still the panel we originally pointed to

	bool operator<( const PanelHandle_t &rhs ) const
	{
		if ( m_iPanelIndex != rhs.m_iPanelIndex )
			return m_iPanelIndex < rhs.m_iPanelIndex;

		return m_unSerialNumber < rhs.m_unSerialNumber;
	}

	bool operator==( const PanelHandle_t &rhs ) const
	{
		return (m_iPanelIndex == rhs.m_iPanelIndex) && (m_unSerialNumber == rhs.m_unSerialNumber);
	}

	bool operator!=( const PanelHandle_t &rhs ) const
	{
		return !(*this == rhs);
	}


	static const PanelHandle_t &InvalidHandle()
	{
		static PanelHandle_t s_invalid = { k_ulInvalidPanelHandle64 >> 32, 0xffffffff & k_ulInvalidPanelHandle64 };
		return s_invalid;
	}
};

} // namespace panorama

#endif // PANELHANDLE_H
