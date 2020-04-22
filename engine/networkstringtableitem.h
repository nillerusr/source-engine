//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NETWORKSTRINGTABLEITEM_H
#define NETWORKSTRINGTABLEITEM_H
#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"
#include "utlvector.h"

#ifdef _XBOX
#define SHARED_NET_STRING_TABLES
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CNetworkStringTableItem
{
public:
	enum
	{
		MAX_USERDATA_BITS = 14,
		MAX_USERDATA_SIZE = (1 << MAX_USERDATA_BITS)
	};

	struct itemchange_s {
		int				tick;
		int				length;
		unsigned char	*data;
	};

	CNetworkStringTableItem( void );
	~CNetworkStringTableItem( void );

#ifndef SHARED_NET_STRING_TABLES
	void			EnableChangeHistory( void );
	void 			UpdateChangeList( int tick, int length, const void *userData );
	int				RestoreTick( int tick );
	inline int		GetTickCreated( void ) const { return m_nTickCreated; }
#endif
	
	bool			SetUserData( int tick, int length, const void *userdata );
	const void		*GetUserData( int *length=0 );
	inline int		GetUserDataLength() const { return m_nUserDataLength; }
	
	// Used by server only
	// void			SetTickCount( int count ) ;
	inline int		GetTickChanged( void ) const { return m_nTickChanged; }

public:
	unsigned char	*m_pUserData;
	int				m_nUserDataLength;
	int				m_nTickChanged;

#ifndef SHARED_NET_STRING_TABLES
	int				m_nTickCreated;
	CUtlVector< itemchange_s > *m_pChangeList;	
#endif
};

#endif // NETWORKSTRINGTABLEITEM_H
