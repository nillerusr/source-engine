//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "host.h"
#include "sysexternal.h"
#include "networkstringtable.h"
#include "utlbuffer.h"
#include "bitbuf.h"
#include "netmessages.h"
#include "net.h"
#include "filesystem_engine.h"
#include "baseclient.h"
#include "vprof.h"
#include <tier1/utlstring.h>
#include <tier1/utlhashtable.h>
#include <tier0/etwprof.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
ConVar sv_dumpstringtables( "sv_dumpstringtables", "0", FCVAR_CHEAT );
ConVar sv_compressstringtablebaselines_threshhold( "sv_compressstringtablebaselines_threshold", "2048", 0, "Minimum size (in bytes) for stringtablebaseline buffer to be compressed." );

#define SUBSTRING_BITS	5
struct StringHistoryEntry
{
	char string[ (1<<SUBSTRING_BITS) ];
};

static int CountSimilarCharacters( char const *str1, char const *str2 )
{
	int c = 0;
	while ( *str1 && *str2 &&
		*str1 == *str2 && c < ((1<<SUBSTRING_BITS) -1 ))
	{
		str1++;
		str2++;
		c++;
	}

	return c;
}

static int GetBestPreviousString( CUtlVector< StringHistoryEntry >& history, char const *newstring, int& substringsize )
{
	int bestindex = -1;
	int bestcount = 0;
	int c = history.Count();
	for ( int i = 0; i < c; i++ )
	{
		char const *prev = history[ i ].string;
		int similar = CountSimilarCharacters( prev, newstring );
		
		if ( similar < 3 )
			continue;

		if ( similar > bestcount )
		{
			bestcount = similar;
			bestindex = i;
		}
	}

	substringsize = bestcount;
	return bestindex;
}

bool CNetworkStringTable_LessFunc( FileNameHandle_t const &a, FileNameHandle_t const &b )
{
	return a < b;
}

//-----------------------------------------------------------------------------
// Implementation when dictionary strings are filenames
//-----------------------------------------------------------------------------
class CNetworkStringFilenameDict : public INetworkStringDict
{
public:
	CNetworkStringFilenameDict()
	{
		m_Items.SetLessFunc( CNetworkStringTable_LessFunc );
	}

	virtual ~CNetworkStringFilenameDict()
	{
		Purge();
	}

	unsigned int Count()
	{
		return m_Items.Count();
	}

	void Purge()
	{
		m_Items.Purge();
	}

	const char *String( int index )
	{
		char* pString = tmpstr512();
		g_pFileSystem->String( m_Items.Key( index ), pString, 512 );
		return pString;
	}

	bool IsValidIndex( int index )
	{
		return m_Items.IsValidIndex( index );
	}

	int Insert( const char *pString )
	{
		FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pString );
		return m_Items.Insert( fnHandle );
	}

	int Find( const char *pString )
	{
		FileNameHandle_t fnHandle = g_pFileSystem->FindFileName( pString );
		if ( !fnHandle )
			return m_Items.InvalidIndex();
		return m_Items.Find( fnHandle );
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Items.Element( index );
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Items.Element( index );
	}

private:
	CUtlMap< FileNameHandle_t, CNetworkStringTableItem > m_Items;
};

//-----------------------------------------------------------------------------
// Implementation for general purpose strings
//-----------------------------------------------------------------------------
class CNetworkStringDict : public INetworkStringDict
{
public:
	CNetworkStringDict() 
	{
	}

	virtual ~CNetworkStringDict() 
	{ 
	}

	unsigned int Count()
	{
		return m_Lookup.Count();
	}

	void Purge()
	{
		m_Lookup.Purge();
	}

	const char *String( int index )
	{
		return m_Lookup.Key( index ).Get();
	}

	bool IsValidIndex( int index )
	{
		return m_Lookup.IsValidHandle( index );
	}

	int Insert( const char *pString )
	{
		return m_Lookup.Insert( pString );
	}

	int Find( const char *pString )
	{
		return pString ? m_Lookup.Find( pString ) : m_Lookup.InvalidHandle();
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Lookup.Element( index );
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Lookup.Element( index );
	}

private:
	CUtlStableHashtable< CUtlConstString, CNetworkStringTableItem, CaselessStringHashFunctor, UTLConstStringCaselessStringEqualFunctor<char> > m_Lookup;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			*tableName - 
//			maxentries - 
//-----------------------------------------------------------------------------
CNetworkStringTable::CNetworkStringTable( TABLEID id, const char *tableName, int maxentries, int userdatafixedsize, int userdatanetworkbits, bool bIsFilenames ) :
	m_bAllowClientSideAddString( false ),
	m_pItemsClientSide( NULL )
{
	m_id = id;
	int len = strlen( tableName ) + 1;
	m_pszTableName = new char[ len ];
	Assert( m_pszTableName );
	Assert( tableName );
	Q_strncpy( m_pszTableName, tableName, len );

	m_changeFunc = NULL;
	m_pObject = NULL;
	m_nTickCount = 0;
	m_pMirrorTable = NULL;
	m_nLastChangedTick = 0;
	m_bChangeHistoryEnabled = false;
	m_bLocked = false;

	m_nMaxEntries = maxentries;
	m_nEntryBits = Q_log2( m_nMaxEntries );

	m_bUserDataFixedSize = userdatafixedsize != 0;
	m_nUserDataSize = userdatafixedsize;
	m_nUserDataSizeBits = userdatanetworkbits;

	if ( m_nUserDataSizeBits > CNetworkStringTableItem::MAX_USERDATA_BITS )
	{
		Host_Error( "String tables user data bits restricted to %i bits, requested %i is too large\n", 
			CNetworkStringTableItem::MAX_USERDATA_BITS,
			m_nUserDataSizeBits );
	}

	if ( m_nUserDataSize > CNetworkStringTableItem::MAX_USERDATA_SIZE )
	{
		Host_Error( "String tables user data size restricted to %i bytes, requested %i is too large\n", 
			CNetworkStringTableItem::MAX_USERDATA_SIZE,
			m_nUserDataSize );
	}

	// Make sure maxentries is power of 2
	if ( ( 1 << m_nEntryBits ) != maxentries )
	{
		Host_Error( "String tables must be powers of two in size!, %i is not a power of 2\n", maxentries );
	}

	if ( IsXbox() || bIsFilenames )
	{
		m_bIsFilenames = true;
		m_pItems = new CNetworkStringFilenameDict;
	}
	else
	{
		m_bIsFilenames = false;
		m_pItems = new CNetworkStringDict;
	}
}

void CNetworkStringTable::SetAllowClientSideAddString( bool state )
{
	if ( state == m_bAllowClientSideAddString )
		return;

	m_bAllowClientSideAddString = state;
	if ( m_pItemsClientSide )
	{
		delete m_pItemsClientSide; 
		m_pItemsClientSide = NULL;
	}

	if ( m_bAllowClientSideAddString )
	{
		m_pItemsClientSide = new CNetworkStringDict;
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder0___" ); // 0 slot can't be used
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder1___" ); // -1 can't be used since it looks like the "invalid" index from other string lookups
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetworkStringTable::IsUserDataFixedSize() const
{
	return m_bUserDataFixedSize;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetworkStringTable::HasFileNameStrings() const
{
	return m_bIsFilenames;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNetworkStringTable::GetUserDataSize() const
{
	return m_nUserDataSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNetworkStringTable::GetUserDataSizeBits() const
{
	return m_nUserDataSizeBits;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTable::~CNetworkStringTable( void )
{
	delete[] m_pszTableName;
	delete m_pItems;
	delete m_pItemsClientSide;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::DeleteAllStrings( void )
{
	delete m_pItems;
	if ( m_bIsFilenames )
	{
		m_pItems = new CNetworkStringFilenameDict;
	}
	else
	{
		m_pItems = new CNetworkStringDict;
	}

	if ( m_pItemsClientSide )
	{
		delete m_pItemsClientSide;
		m_pItemsClientSide = new CNetworkStringDict;
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder0___" ); // 0 slot can't be used
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder1___" ); // -1 can't be used since it looks like the "invalid" index from other string lookups
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : CNetworkStringTableItem
//-----------------------------------------------------------------------------
CNetworkStringTableItem *CNetworkStringTable::GetItem( int i )
{
	if ( i >= 0 )
	{
		return &m_pItems->Element( i );		
	}

	Assert( m_pItemsClientSide );
	return &m_pItemsClientSide->Element( -i );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the table identifier
// Output : TABLEID
//-----------------------------------------------------------------------------
TABLEID CNetworkStringTable::GetTableId( void ) const
{
	return m_id;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the max size of the table
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetMaxStrings( void ) const
{
	return m_nMaxEntries;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a table, by name
// Output : const char
//-----------------------------------------------------------------------------
const char *CNetworkStringTable::GetTableName( void ) const
{
	return m_pszTableName;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of bits needed to encode an entry index
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetEntryBits( void ) const
{
	return m_nEntryBits;
}


void CNetworkStringTable::SetTick(int tick_count)
{
	Assert( tick_count >= m_nTickCount );
	m_nTickCount = tick_count;
}

void CNetworkStringTable::Lock(	bool bLock )
{
	m_bLocked = bLock;
}

pfnStringChanged CNetworkStringTable::GetCallback()
{ 
	return m_changeFunc; 
}

#ifndef SHARED_NET_STRING_TABLES

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::EnableRollback()
{
	// stringtable must be empty 
	Assert( m_pItems->Count() == 0);
	m_bChangeHistoryEnabled = true;
}

void CNetworkStringTable::SetMirrorTable(INetworkStringTable *table)
{
	m_pMirrorTable = table;
}

void CNetworkStringTable::RestoreTick(int tick)
{
	// TODO optimize this, most of the time the tables doens't really change

	m_nLastChangedTick = 0;

	int count = m_pItems->Count();
		
	for ( int i = 0; i < count; i++ )
	{
		// restore tick in all entries
		int tickChanged = m_pItems->Element( i ).RestoreTick( tick );

		if ( tickChanged > m_nLastChangedTick )
			m_nLastChangedTick = tickChanged;
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates the mirror table, if set one
// Output : return true if some entries were updates
//-----------------------------------------------------------------------------
void CNetworkStringTable::UpdateMirrorTable( int tick_ack  )
{
	if ( !m_pMirrorTable )
		return;

	m_pMirrorTable->SetTick( m_nTickCount ); // use same tick

	int count = m_pItems->Count();
	
	for ( int i = 0; i < count; i++ )
	{
		CNetworkStringTableItem *p = &m_pItems->Element( i );

		// mirror is up to date
		if ( p->GetTickChanged() <= tick_ack )
			continue;

		const void *pUserData = p->GetUserData();

		int nBytes = p->GetUserDataLength();

		if ( !nBytes || !pUserData )
		{
			nBytes = 0;
			pUserData = NULL;
		}

		// Check if we are updating an old entry or adding a new one
		if ( i < m_pMirrorTable->GetNumStrings() )
		{
			m_pMirrorTable->SetStringUserData( i, nBytes, pUserData );
		}
		else
		{
			// Grow the table (entryindex must be the next empty slot)
			Assert( i == m_pMirrorTable->GetNumStrings() );
			char const *pName = m_pItems->String( i );
			m_pMirrorTable->AddString( true, pName, nBytes, pUserData );
		}
	}
}

int CNetworkStringTable::WriteUpdate( CBaseClient *client, bf_write &buf, int tick_ack )
{
	CUtlVector< StringHistoryEntry > history;

	int entriesUpdated = 0;
	int lastEntry = -1;
	int nTableStartBit = buf.GetNumBitsWritten();

	int count = m_pItems->Count();

	for ( int i = 0; i < count; i++ )
	{
		CNetworkStringTableItem *p = &m_pItems->Element( i );

		// Client is up to date
		if ( p->GetTickChanged() <= tick_ack )
			continue;

		int nStartBit = buf.GetNumBitsWritten();

		// Write Entry index
		if ( (lastEntry+1) == i )
		{
			buf.WriteOneBit( 1 );
		}
		else
		{
			buf.WriteOneBit( 0 );
			buf.WriteUBitLong( i, m_nEntryBits );
		}

		// check if string can use older string as base eg "models/weapons/gun1" & "models/weapons/gun2"
		char const *pEntry = m_pItems->String( i );

		if ( p->GetTickCreated() > tick_ack )
		{
			// this item has just been created, send string itself
			buf.WriteOneBit( 1 );
			
			int substringsize = 0;
			int bestprevious = GetBestPreviousString( history, pEntry, substringsize );
			if ( bestprevious != -1 )
			{
				buf.WriteOneBit( 1 );
				buf.WriteUBitLong( bestprevious, 5 );	// history never has more than 32 entries
				buf.WriteUBitLong( substringsize, SUBSTRING_BITS );
				buf.WriteString( pEntry + substringsize );
			}
			else
			{
				buf.WriteOneBit( 0 );
				buf.WriteString( pEntry  );
			}
		}
		else
		{
			buf.WriteOneBit( 0 );
		}

		// Write the item's user data.
		int len;
		const void *pUserData = GetStringUserData( i, &len );
		if ( pUserData && len > 0 )
		{
			buf.WriteOneBit( 1 );

			if ( IsUserDataFixedSize() )
			{
				// Don't have to send length, it was sent as part of the table definition
				buf.WriteBits( pUserData, GetUserDataSizeBits() );
			}
			else
			{
				buf.WriteUBitLong( len, CNetworkStringTableItem::MAX_USERDATA_BITS );
				buf.WriteBits( pUserData, len*8 );
			}
		}
		else
		{
			buf.WriteOneBit( 0 );
		}

		// limit string history to 32 entries
		if ( history.Count() > 31 )
		{
			history.Remove( 0 );
		}

		// add string to string history
		StringHistoryEntry she;
		Q_strncpy( she.string, pEntry, sizeof( she.string ) );
		history.AddToTail( she );

		entriesUpdated++;
		lastEntry = i;

		if ( client && client->IsTracing() )
		{
			int nBits = buf.GetNumBitsWritten() - nStartBit;
			client->TraceNetworkMsg( nBits, " [%s] %d:%s ", GetTableName(), i, GetString( i ) );
		}
	}

	ETWMark2I( GetTableName(), entriesUpdated, buf.GetNumBitsWritten() - nTableStartBit );

	return entriesUpdated;
}


//-----------------------------------------------------------------------------
// Purpose: Parse string update
//-----------------------------------------------------------------------------
void CNetworkStringTable::ParseUpdate( bf_read &buf, int entries )
{
	int lastEntry = -1;

	CUtlVector< StringHistoryEntry > history;

	for (int i=0; i<entries; i++)
	{
		int entryIndex = lastEntry + 1;

		if ( !buf.ReadOneBit() )
		{
			entryIndex = buf.ReadUBitLong( GetEntryBits() );
		}

		lastEntry = entryIndex;
		
		if ( entryIndex < 0 || entryIndex >= GetMaxStrings() )
		{
			Host_Error( "Server sent bogus string index %i for table %s\n", entryIndex, GetTableName() );
		}

		const char *pEntry = NULL;
		char entry[ 1024 ]; 
		char substr[ 1024 ];

		if ( buf.ReadOneBit() )
		{
			bool substringcheck = buf.ReadOneBit() ? true : false;

			if ( substringcheck )
			{
				unsigned int index = buf.ReadUBitLong( 5 );
				unsigned int bytestocopy = buf.ReadUBitLong( SUBSTRING_BITS );
				if ( index >= (unsigned int)history.Count() )
				{
					Host_Error( "Server sent bogus substring index %i for table %s\n",
					            entryIndex, GetTableName() );
				}
				Q_strncpy( entry, history[ index ].string, Min( sizeof( entry ), (size_t)bytestocopy + 1 ) );
				buf.ReadString( substr, sizeof(substr) );
				Q_strncat( entry, substr, sizeof(entry), COPY_ALL_CHARACTERS );
			}
			else
			{
				buf.ReadString( entry, sizeof( entry ) );
			}

			pEntry = entry;
		}
		
		// Read in the user data.
		unsigned char tempbuf[ CNetworkStringTableItem::MAX_USERDATA_SIZE ];
		memset( tempbuf, 0, sizeof( tempbuf ) );
		const void *pUserData = NULL;
		int nBytes = 0;

		if ( buf.ReadOneBit() )
		{
			if ( IsUserDataFixedSize() )
			{
				// Don't need to read length, it's fixed length and the length was networked down already.
				nBytes = GetUserDataSize();
				Assert( nBytes > 0 );
				tempbuf[nBytes-1] = 0; // be safe, clear last byte
				buf.ReadBits( tempbuf, GetUserDataSizeBits() );
			}
			else
			{
				nBytes = buf.ReadUBitLong( CNetworkStringTableItem::MAX_USERDATA_BITS );
				ErrorIfNot( nBytes <= sizeof( tempbuf ),
					("CNetworkStringTableClient::ParseUpdate: message too large (%d bytes).", nBytes)
				);

				buf.ReadBytes( tempbuf, nBytes );
			}

			pUserData = tempbuf;
		}

		// Check if we are updating an old entry or adding a new one
		if ( entryIndex < GetNumStrings() )
		{
			SetStringUserData( entryIndex, nBytes, pUserData );
#ifdef _DEBUG
			if ( pEntry )
			{
				Assert( !Q_strcmp( pEntry, GetString( entryIndex ) ) ); // make sure string didn't change
			}
#endif
			pEntry = GetString( entryIndex ); // string didn't change
		}
		else
		{
			// Grow the table (entryindex must be the next empty slot)
			Assert( (entryIndex == GetNumStrings()) && (pEntry != NULL) );
				
			if ( pEntry == NULL )
			{
				Msg("CNetworkStringTable::ParseUpdate: NULL pEntry, table %s, index %i\n", GetTableName(), entryIndex );
				pEntry = "";// avoid crash because of NULL strings
			}

			AddString( true, pEntry, nBytes, pUserData );
		}

		if ( history.Count() > 31 )
		{
			history.Remove( 0 );
		}

		StringHistoryEntry she;
		Q_strncpy( she.string, pEntry, sizeof( she.string ) );
		history.AddToTail( she );
	}
}

void CNetworkStringTable::CopyStringTable(CNetworkStringTable * table)
{
	Assert (m_pItems->Count() == 0); // table must be empty before coping

	for ( unsigned int i = 0; i < table->m_pItems->Count() ; ++i )
	{
		CNetworkStringTableItem	*item = &table->m_pItems->Element( i );

		m_nTickCount = item->m_nTickChanged;

		AddString( true, table->GetString( i ), item->m_nUserDataLength, item->m_pUserData );
	}
}

#endif

void CNetworkStringTable::TriggerCallbacks( int tick_ack )
{
	if ( m_changeFunc == NULL )
		return;

	COM_TimestampedLog( "Change(%s):Start", GetTableName() );

	int count = m_pItems->Count();

	for ( int i = 0; i < count; i++ )
	{
		CNetworkStringTableItem *pItem = &m_pItems->Element( i );

		// mirror is up to date
		if ( pItem->GetTickChanged() <= tick_ack )
			continue;

		int userDataSize;
		const void *pUserData = pItem->GetUserData( &userDataSize );

		// fire the callback function
		( *m_changeFunc )( m_pObject, this, i, GetString( i ), pUserData );
	}

	COM_TimestampedLog( "Change(%s):End", GetTableName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : changeFunc - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::SetStringChangedCallback( void *object, pfnStringChanged changeFunc )
{
	m_changeFunc = changeFunc;
	m_pObject = object;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *client - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetworkStringTable::ChangedSinceTick( int tick ) const
{
	return ( m_nLastChangedTick > tick );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::AddString( bool bIsServer, const char *string, int length /*= -1*/, const void *userdata /*= NULL*/ )
{
	bool bHasChanged;
	CNetworkStringTableItem *item;
	
	if ( !string )
	{
		Assert( string );
		ConMsg( "Warning:  Can't add NULL string to table %s\n", GetTableName() );
		return INVALID_STRING_INDEX;
	}

#ifdef _DEBUG
	if ( m_bLocked )
	{
		DevMsg("Warning! CNetworkStringTable::AddString: adding '%s' while locked.\n", string );
	}
#endif

	int i = m_pItems->Find( string );
	if ( !bIsServer )
	{
		if ( m_pItems->IsValidIndex( i ) && !m_pItemsClientSide )
		{
			bIsServer = true;
		}
	}

	if ( !bIsServer && m_pItemsClientSide )
	{
		i = m_pItemsClientSide->Find( string );

		if ( !m_pItemsClientSide->IsValidIndex( i ) )
		{
			// not in list yet, create it now
			if ( m_pItemsClientSide->Count() >= (unsigned int)GetMaxStrings() )
			{
				// Too many strings, FIXME: Print warning message
				ConMsg( "Warning:  Table %s is full, can't add %s\n", GetTableName(), string );
				return INVALID_STRING_INDEX;
			}

			// create new item
			{
			MEM_ALLOC_CREDIT();
			i = m_pItemsClientSide->Insert( string );
			}

			item = &m_pItemsClientSide->Element( i );

			// set changed ticks

			item->m_nTickChanged = m_nTickCount;

	#ifndef	SHARED_NET_STRING_TABLES
			item->m_nTickCreated = m_nTickCount;

			if ( m_bChangeHistoryEnabled )
			{
				item->EnableChangeHistory();
			}
	#endif

			bHasChanged = true;
		}
		else
		{
			item = &m_pItemsClientSide->Element( i ); // item already exists
			bHasChanged = false;  // not changed yet
		}

		if ( length > -1 )
		{
			if ( item->SetUserData( m_nTickCount, length, userdata ) )
			{
				bHasChanged = true;
			}
		}

		if ( bHasChanged && !m_bChangeHistoryEnabled )
		{
			DataChanged( -i, item );
		}

		// Negate i for returning to client
		i = -i;
	}
	else
	{
		// See if it's already there
		i = m_pItems->Find( string );

		if ( !m_pItems->IsValidIndex( i ) )
		{
			// not in list yet, create it now
			if ( m_pItems->Count() >= (unsigned int)GetMaxStrings() )
			{
				// Too many strings, FIXME: Print warning message
				ConMsg( "Warning:  Table %s is full, can't add %s\n", GetTableName(), string );
				return INVALID_STRING_INDEX;
			}

			// create new item
			{
			MEM_ALLOC_CREDIT();
			i = m_pItems->Insert( string );
			}

			item = &m_pItems->Element( i );

			// set changed ticks

			item->m_nTickChanged = m_nTickCount;

	#ifndef	SHARED_NET_STRING_TABLES
			item->m_nTickCreated = m_nTickCount;

			if ( m_bChangeHistoryEnabled )
			{
				item->EnableChangeHistory();
			}
	#endif

			bHasChanged = true;
		}
		else
		{
			item = &m_pItems->Element( i ); // item already exists
			bHasChanged = false;  // not changed yet
		}

		if ( length > -1 )
		{
			if ( item->SetUserData( m_nTickCount, length, userdata ) )
			{
				bHasChanged = true;
			}
		}

		if ( bHasChanged && !m_bChangeHistoryEnabled )
		{
			DataChanged( i, item );
		}
	}

	return i;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CNetworkStringTable::GetString( int stringNumber )
{
	INetworkStringDict *dict = m_pItems;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	Assert( dict->IsValidIndex( stringNumber ) );

	if ( dict->IsValidIndex( stringNumber ) )
	{
		return dict->String( stringNumber );
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
//			length - 
//			*userdata - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::SetStringUserData( int stringNumber, int length /*=0*/, const void *userdata /*= 0*/ )
{
#ifdef _DEBUG
	if ( m_bLocked )
	{
		DevMsg("Warning! CNetworkStringTable::SetStringUserData (%s): changing entry %i while locked.\n", GetTableName(), stringNumber );
	}
#endif

	INetworkStringDict *dict = m_pItems;
	int saveStringNumber = stringNumber;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	Assert( (length == 0 && userdata == NULL) || ( length > 0 && userdata != NULL) );
	Assert( dict->IsValidIndex( stringNumber ) );
	CNetworkStringTableItem *p = &dict->Element( stringNumber );
	Assert( p );

	if ( p->SetUserData( m_nTickCount, length, userdata ) )
	{
		// Mark changed
		DataChanged( saveStringNumber, p );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *item - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::DataChanged( int stringNumber, CNetworkStringTableItem *item )
{
	Assert( item );

	if ( !item )
		return;

	// Mark table as changed
	m_nLastChangedTick = m_nTickCount;
	
	// Invoke callback if one was installed
	
#ifndef SHARED_NET_STRING_TABLES // but not if client & server share the same containers, we trigger that later

	if ( m_changeFunc != NULL )
	{
		int userDataSize;
		const void *pUserData = item->GetUserData( &userDataSize );
		( *m_changeFunc )( m_pObject, this, stringNumber, GetString( stringNumber ), pUserData );
	}

#endif
}

#ifndef SHARED_NET_STRING_TABLES

void CNetworkStringTable::WriteStringTable( bf_write& buf )
{
	int numstrings = m_pItems->Count();
	buf.WriteWord( numstrings );
	for ( int i = 0 ; i < numstrings; i++ )
	{
		buf.WriteString( GetString( i ) );
		int userDataSize;
		const void *pUserData = GetStringUserData( i, &userDataSize );
		if ( userDataSize > 0 )
		{
			buf.WriteOneBit( 1 );
			buf.WriteWord( (short)userDataSize );
			buf.WriteBytes( pUserData, userDataSize );
		}
		else
		{
			buf.WriteOneBit( 0 );
		}
	}

	if ( m_pItemsClientSide )
	{
		buf.WriteOneBit( 1 );

		numstrings = m_pItemsClientSide->Count();
		buf.WriteWord( numstrings );
		for ( int i = 0 ; i < numstrings; i++ )
		{
			buf.WriteString( m_pItemsClientSide->String( i ) );
			int userDataSize;
			const void *pUserData = m_pItemsClientSide->Element( i ).GetUserData( &userDataSize );
			if ( userDataSize > 0 )
			{
				buf.WriteOneBit( 1 );
				buf.WriteWord( (short)userDataSize );
				buf.WriteBytes( pUserData, userDataSize );
			}
			else
			{
				buf.WriteOneBit( 0 );
			}
		}

	}
	else
	{
		buf.WriteOneBit( 0 );
	}
}

bool CNetworkStringTable::ReadStringTable( bf_read& buf )
{
	DeleteAllStrings();

	int numstrings = buf.ReadWord();
	for ( int i = 0 ; i < numstrings; i++ )
	{
		char stringname[4096];
		
		buf.ReadString( stringname, sizeof( stringname ) );

		if ( buf.ReadOneBit() == 1 )
		{
			int userDataSize = (int)buf.ReadWord();
			Assert( userDataSize > 0 );
			byte *data = new byte[ userDataSize + 4 ];
			Assert( data );

			buf.ReadBytes( data, userDataSize );

			AddString( true, stringname, userDataSize, data );

			delete[] data;
			
		}
		else
		{
			AddString( true, stringname );
		}
	}

	// Client side stuff
	if ( buf.ReadOneBit() == 1 )
	{
		numstrings = buf.ReadWord();
		for ( int i = 0 ; i < numstrings; i++ )
		{
			char stringname[4096];

			buf.ReadString( stringname, sizeof( stringname ) );

			if ( buf.ReadOneBit() == 1 )
			{
				int userDataSize = (int)buf.ReadWord();
				Assert( userDataSize > 0 );
				byte *data = new byte[ userDataSize + 4 ];
				Assert( data );

				buf.ReadBytes( data, userDataSize );

				if ( i >= 2 )
				{
					AddString( false, stringname, userDataSize, data );
				}

				delete[] data;

			}
			else
			{
				if ( i >= 2 )
				{
					AddString( false, stringname );
				}
			}
		}
	}

	return true;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
//			length - 
// Output : const void
//-----------------------------------------------------------------------------
const void *CNetworkStringTable::GetStringUserData( int stringNumber, int *length )
{
	INetworkStringDict *dict = m_pItems;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	CNetworkStringTableItem *p;

	Assert( dict->IsValidIndex( stringNumber ) );
	p = &dict->Element( stringNumber );
	Assert( p );
	return p->GetUserData( length );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetNumStrings( void ) const
{
	return m_pItems->Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringTable - 
//			*string - 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::FindStringIndex( char const *string )
{
	if ( !string )
		return INVALID_STRING_INDEX;
	int i = m_pItems->Find( string );
	if ( m_pItems->IsValidIndex( i ) )
		return i;
	return INVALID_STRING_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::Dump( void )
{
	ConMsg( "Table %s\n", GetTableName() );
	ConMsg( "  %i/%i items\n", GetNumStrings(), GetMaxStrings() );
	for ( int i = 0; i < GetNumStrings() ; i++ )
	{
		ConMsg( "  %i : %s\n", i, GetString( i ) );
	}
	if ( m_pItemsClientSide )
	{
		for ( int i = 0; i < (int)m_pItemsClientSide->Count() ; i++ )
		{
			ConMsg( "  (c)%i : %s\n", i, m_pItemsClientSide->String( i ) );
		}
	}
	ConMsg( "\n" );
}

#ifndef SHARED_NET_STRING_TABLES

bool CNetworkStringTable::WriteBaselines( SVC_CreateStringTable &msg, char *msg_buffer, int msg_buffer_size )
{
	VPROF_BUDGET( "CNetworkStringTable::WriteBaselines", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	msg.m_DataOut.StartWriting( msg_buffer, msg_buffer_size );

	msg.m_bIsFilenames          = m_bIsFilenames;
	msg.m_szTableName			= GetTableName();
	msg.m_nMaxEntries			= GetMaxStrings();
	msg.m_nNumEntries			= GetNumStrings();
	msg.m_bUserDataFixedSize	= IsUserDataFixedSize();
	msg.m_nUserDataSize			= GetUserDataSize();
	msg.m_nUserDataSizeBits		= GetUserDataSizeBits();

	// tick = -1 ensures that all entries are updated = baseline
	int entries = WriteUpdate( NULL, msg.m_DataOut, -1 );

	return entries == msg.m_nNumEntries;
}

#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableContainer::CNetworkStringTableContainer( void )
{
	m_bAllowCreation = false;
	m_bLocked = true;
	m_nTickCount = 0;
	m_bEnableRollback = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableContainer::~CNetworkStringTableContainer( void )
{
	RemoveAllTables();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::AllowCreation( bool state )
{
	m_bAllowCreation = state;
}

bool  CNetworkStringTableContainer::Lock( bool bLock )
{
	bool oldLock = m_bLocked;

	m_bLocked = bLock;

	// Determine if an update is needed
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		table->Lock( bLock );
	}
	return oldLock;
}

void CNetworkStringTableContainer::SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString )
{
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *t = (CNetworkStringTable*) GetTable( i );
		if ( t == table )
		{
			t->SetAllowClientSideAddString( bAllowClientSideAddString );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tableName - 
//			maxentries - 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::CreateStringTableEx( const char *tableName, int maxentries, int userdatafixedsize /*= 0*/, int userdatanetworkbits /*= 0*/, bool bIsFilenames /*= false */ )
{
	if ( !m_bAllowCreation )
	{
		Sys_Error( "Tried to create string table '%s' at wrong time\n", tableName );
		return NULL;
	}

	CNetworkStringTable *pTable = (CNetworkStringTable*) FindTable( tableName );

	if ( pTable != NULL )
	{
		Sys_Error( "Tried to create string table '%s' twice\n", tableName );
		return NULL;
	}

	if ( m_Tables.Count() >= MAX_TABLES )
	{
		Sys_Error( "Only %i string tables allowed, can't create'%s'", MAX_TABLES, tableName);
		return NULL;
	}

	TABLEID id = m_Tables.Count();

	pTable = new CNetworkStringTable( id, tableName, maxentries, userdatafixedsize, userdatanetworkbits, bIsFilenames );

	Assert( pTable );

#ifndef SHARED_NET_STRING_TABLES
	if ( m_bEnableRollback )
	{
		pTable->EnableRollback();
	}
#endif

	pTable->SetTick( m_nTickCount );

	m_Tables.AddToTail( pTable );

	return pTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tableName - 
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::FindTable( const char *tableName ) const
{
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		if ( !Q_stricmp( tableName, m_Tables[ i ]->GetTableName() ) )
			return m_Tables[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringTable - 
// Output : CNetworkStringTableServer
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::GetTable( TABLEID stringTable ) const
{
	if ( stringTable < 0 || stringTable >= m_Tables.Count() )
		return NULL;

	return m_Tables[ stringTable ];
}

int CNetworkStringTableContainer::GetNumTables( void ) const
{
	return m_Tables.Count();
}

#ifndef SHARED_NET_STRING_TABLES

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::WriteBaselines( bf_write &buf )
{
	VPROF_BUDGET( "CNetworkStringTableContainer::WriteBaselines", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	SVC_CreateStringTable msg;

	size_t msg_buffer_size = 2 * NET_MAX_PAYLOAD;
	char *msg_buffer = new char[ msg_buffer_size ];
	if ( !msg_buffer )
	{
		Host_Error( "Failed to allocate %llu bytes of memory in CNetworkStringTableContainer::WriteBaselines\n", (uint64)msg_buffer_size );
	}

	for ( int i = 0 ; i < m_Tables.Count() ; i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		int before = buf.GetNumBytesWritten();
		if ( !table->WriteBaselines( msg, msg_buffer, msg_buffer_size ) )
		{
			Host_Error( "Index error writing string table baseline %s\n", table->GetTableName() );
		}

		if ( msg.m_DataOut.IsOverflowed() )
		{
			Warning( "Warning:  Overflowed writing uncompressed string table data for %s\n", table->GetTableName() );
		}

		msg.m_bDataCompressed = false;
		if ( msg.m_DataOut.GetNumBytesWritten() >= sv_compressstringtablebaselines_threshhold.GetInt() )
		{
			CFastTimer compressTimer;
			compressTimer.Start();

			// TERROR: bzip-compress the stringtable before adding it to the packet.  Yes, the whole packet will be bzip'd,
			// but the uncompressed data also has to be under the NET_MAX_PAYLOAD limit.
			unsigned int numBytes = msg.m_DataOut.GetNumBytesWritten();
			unsigned int compressedSize = (unsigned int)numBytes;
			char *compressedData = new char[numBytes];

			if ( COM_BufferToBufferCompress_Snappy( compressedData, &compressedSize, (char *)msg.m_DataOut.GetData(), numBytes ) )
			{
				msg.m_bDataCompressed = true;
				msg.m_DataOut.Reset();
				msg.m_DataOut.WriteLong( numBytes );	// uncompressed size
				msg.m_DataOut.WriteLong( compressedSize );	// compressed size
				msg.m_DataOut.WriteBits( compressedData, compressedSize * 8 );	// compressed data

				// if ( compressstringtablbaselines > 1 )
				{
					compressTimer.End(); 
					DevMsg( "Stringtable %s compression: %d -> %d bytes: %.2fms\n",
							table->GetTableName(), numBytes, compressedSize, compressTimer.GetDuration().GetMillisecondsF() );
				}
			}

			delete [] compressedData;
		}

		if ( !msg.WriteToBuffer( buf ) )
		{
			Host_Error( "Overflow error writing string table baseline %s\n", table->GetTableName() );
		}

		int after = buf.GetNumBytesWritten();
		if ( sv_dumpstringtables.GetBool() )
		{
			DevMsg( "CNetworkStringTableContainer::WriteBaselines wrote %d bytes for table %s [space remaining %d bytes]\n", after - before, table->GetTableName(), buf.GetNumBytesLeft() );
		}
	}

	delete[] msg_buffer;
}

void CNetworkStringTableContainer::WriteStringTables( bf_write& buf )
{
	int numTables = m_Tables.Size();

	buf.WriteByte( numTables );
	for ( int i = 0; i < numTables; i++ )
	{
		CNetworkStringTable *table = m_Tables[ i ];
		buf.WriteString( table->GetTableName() );
		table->WriteStringTable( buf );
	}
}

bool CNetworkStringTableContainer::ReadStringTables( bf_read& buf )
{
	int numTables = buf.ReadByte();
	for ( int i = 0 ; i < numTables; i++ )
	{
		char tablename[ 256 ];
		buf.ReadString( tablename, sizeof( tablename ) );

		// Find this table by name
		CNetworkStringTable *table = (CNetworkStringTable*)FindTable( tablename );
		Assert( table );

		// Now read the data for the table
		if ( table && !table->ReadStringTable( buf ) )
		{
			Host_Error( "Error reading string table %s\n", tablename );
		}
		else
		{
			Warning( "Could not find table \"%s\"\n", tablename );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//			*msg - 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::WriteUpdateMessage( CBaseClient *client, int tick_ack, bf_write &buf )
{
	VPROF_BUDGET( "CNetworkStringTableContainer::WriteUpdateMessage", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	char buffer[NET_MAX_PAYLOAD];

	// Determine if an update is needed
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		if ( !table )
			continue;

		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		SVC_UpdateStringTable msg;

		msg.m_DataOut.StartWriting( buffer, NET_MAX_PAYLOAD );
		msg.m_nTableID = table->GetTableId();
		msg.m_nChangedEntries = table->WriteUpdate( client, msg.m_DataOut, tick_ack );

		Assert( msg.m_nChangedEntries > 0 ); // don't send unnecessary empty updates

		msg.WriteToBuffer( buf );

		if ( client &&
			 client->IsTracing() )
		{
			client->TraceNetworkData( buf, "StringTable %s", table->GetTableName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//			*msg - 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::DirectUpdate( int tick_ack )
{
	VPROF_BUDGET( "CNetworkStringTableContainer::DirectUpdate", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// Determine if an update is needed
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		Assert( table );
		
		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		table->UpdateMirrorTable( tick_ack );
	}
}

void CNetworkStringTableContainer::EnableRollback( bool bState )
{
	// we can't dis/enable rollback if we already created tabled
	Assert( m_Tables.Count() == 0 );

	m_bEnableRollback = bState;
}


void CNetworkStringTableContainer::RestoreTick( int tick )
{
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		Assert( table );

		table->RestoreTick( tick );
	}
}

#endif

void CNetworkStringTableContainer::TriggerCallbacks( int tick_ack )
{
	// Determine if an update is needed
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		Assert( table );

		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		table->TriggerCallbacks( tick_ack );
	}
}

void CNetworkStringTableContainer::SetTick( int tick_count)
{
	Assert( tick_count > 0 );

	m_nTickCount = tick_count;

	// Determine if an update is needed
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		Assert( table );

		table->SetTick( tick_count );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::RemoveAllTables( void )
{
	while ( m_Tables.Count() > 0 )
	{
		CNetworkStringTable *table = m_Tables[ 0 ];
		m_Tables.Remove( 0 );
		delete table;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::Dump( void )
{
	for ( int i = 0; i < m_Tables.Count(); i++ )
	{
		m_Tables[ i ]->Dump();
	}
}
