//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef NETWORKSTRINGTABLE_H
#define NETWORKSTRINGTABLE_H
#ifdef _WIN32
#pragma once
#endif

#include "networkstringtabledefs.h"
#include "networkstringtableitem.h"

#include <utldict.h>
#include <utlbuffer.h>
#include "tier1/bitbuf.h"

class SVC_CreateStringTable;
class CBaseClient;

abstract_class INetworkStringDict
{
public:
	virtual ~INetworkStringDict() {}

	virtual unsigned int Count() = 0;
	virtual void Purge() = 0;
	virtual const char *String( int index ) = 0;
	virtual bool IsValidIndex( int index ) = 0;
	virtual int Insert( const char *pString ) = 0;
	virtual int Find( const char *pString ) = 0;
	virtual CNetworkStringTableItem	&Element( int index ) = 0;
	virtual const CNetworkStringTableItem &Element( int index ) const = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Client/Server shared string table definition
//-----------------------------------------------------------------------------
class CNetworkStringTable  : public INetworkStringTable
{
public:
	// Construction
					CNetworkStringTable( TABLEID id, const char *tableName, int maxentries, int userdatafixedsize, int userdatanetworkbits, bool bIsFilenames );
	virtual			~CNetworkStringTable( void );

public:
	// INetworkStringTable interface:

	const char		*GetTableName( void ) const;
	TABLEID			GetTableId( void ) const;
	int				GetNumStrings( void ) const;
	int				GetMaxStrings( void ) const;
	int				GetEntryBits( void ) const;

	// Networking
	void			SetTick( int tick );
	bool			ChangedSinceTick( int tick ) const;

	int				AddString( bool bIsServer, const char *value, int length = -1, const void *userdata = NULL ); 
	const char		*GetString( int stringNumber );

	void			SetStringUserData( int stringNumber, int length, const void *userdata );
	const void		*GetStringUserData( int stringNumber, int *length );
	int				FindStringIndex( char const *string );

	void			SetStringChangedCallback( void *object, pfnStringChanged changeFunc );

	bool			HasFileNameStrings() const;
	bool			IsUserDataFixedSize() const;
	int				GetUserDataSizeBits() const;
	int				GetUserDataSize() const;

public:
	
#ifndef SHARED_NET_STRING_TABLES
	int				WriteUpdate( CBaseClient *client, bf_write &buf, int tick_ack );
	void			ParseUpdate( bf_read &buf, int entries );

	// HLTV change history & rollback
	void			EnableRollback();
	void			RestoreTick(int tick);
	
	// local backdoor tables
	void			SetMirrorTable( INetworkStringTable *table );
	void			UpdateMirrorTable( int tick_ack  );
	void			CopyStringTable(CNetworkStringTable * table);
	// buffer IO
	void			WriteStringTable( bf_write& buf );
	bool			ReadStringTable( bf_read& buf );

	bool			WriteBaselines( SVC_CreateStringTable &msg, char *msg_buffer, int msg_buffer_size );
#endif

	void			TriggerCallbacks( int tick_ack  );
	
	CNetworkStringTableItem *GetItem( int i );
	
	// debug ouptput
	virtual void	Dump( void );
	virtual void	Lock( bool bLock );
	
	void SetAllowClientSideAddString( bool state );
	pfnStringChanged	GetCallback();

protected:
	void			DataChanged( int stringNumber, CNetworkStringTableItem *item );

	// Destroy string table
	void			DeleteAllStrings( void );

	CNetworkStringTable( const CNetworkStringTable & ); // not implemented, not allowed

	TABLEID					m_id;
	char					*m_pszTableName;
	// Must be a power of 2, so encoding can determine # of bits to use based on log2
	int						m_nMaxEntries;
	int						m_nEntryBits;
	int						m_nTickCount;
	int						m_nLastChangedTick;

	bool					m_bChangeHistoryEnabled : 1;
	bool					m_bLocked : 1;
	bool					m_bAllowClientSideAddString : 1;
	bool					m_bUserDataFixedSize : 1;
	bool					m_bIsFilenames : 1;

	int						m_nUserDataSize;
	int						m_nUserDataSizeBits;

	// Change function callback
	pfnStringChanged		m_changeFunc;
	// Optional context/object
	void					*m_pObject;

	// pointer to local backdoor table 
	INetworkStringTable		*m_pMirrorTable;

	INetworkStringDict		*m_pItems;
	INetworkStringDict		*m_pItemsClientSide;	 // For m_bAllowClientSideAddString, these items are non-networked and are referenced by a negative string index!!!
};

//-----------------------------------------------------------------------------
// Purpose: Implements game .dll string table interface
//-----------------------------------------------------------------------------
class CNetworkStringTableContainer : public INetworkStringTableContainer
{
public:
	// Construction
							CNetworkStringTableContainer( void );
							~CNetworkStringTableContainer( void );

public:

	// Implement INetworkStringTableContainer
	INetworkStringTable	*CreateStringTable( const char *tableName, int maxentries, int userdatafixedsize = 0, int userdatanetworkbits = 0 )	{ return CreateStringTableEx( tableName, maxentries, userdatafixedsize, userdatanetworkbits, false ); }
	INetworkStringTable	*CreateStringTableEx( const char *tableName, int maxentries, int userdatafixedsize = 0, int userdatanetworkbits = 0, bool bIsFilenames = false );
	void				RemoveAllTables( void );
	
	// table infos
	INetworkStringTable	*FindTable( const char *tableName ) const ;
	INetworkStringTable	*GetTable( TABLEID stringTable ) const;
	int					GetNumTables( void ) const;

	virtual void				SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString );

public:

	// Update a client (called once during packet sending per client)
	void		SetTick( int tick_count);

#ifndef SHARED_NET_STRING_TABLES
	// rollback feature
	void		EnableRollback( bool bState );
	void		RestoreTick( int tick );

	// Buffer I/O
	void		WriteStringTables( bf_write& buf );
	bool		ReadStringTables( bf_read& buf );

	void		WriteUpdateMessage( CBaseClient *client, int tick_ack, bf_write &buf );
	void		WriteBaselines( bf_write &buf );
	void		DirectUpdate( int tick_ack );	// fill mirror table directly with updates
#endif

	void		TriggerCallbacks( int tick_ack ); // fire callback functions 

	
		
	// Guards so game .dll can't create tables at the wrong time
	void		AllowCreation( bool state );

	
	
	// Print table data to console
	void		Dump( void );
	// Sets the lock and returns the previous lock state
	bool		Lock( bool bLock );

	void		SetAllowClientSideAddString( bool state );

private:
	bool		m_bAllowCreation;	// creat guard Guard
	int			m_nTickCount;		// current tick
	bool		m_bLocked;			// currently locked?
	bool		m_bEnableRollback;	// enables rollback feature

	CUtlVector < CNetworkStringTable* > m_Tables;	// the string tables
};

#endif // NETWORKSTRINGTABLE_H
