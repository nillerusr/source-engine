//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "utlstring.h"
#include "checksum_crc.h"
#include "userid.h"
#include "pure_server.h"
#include "common.h"
#include "tier1/KeyValues.h"
#include "convar.h"
#include "filesystem_engine.h"
#include "server.h"
#include "sv_filter.h"
#include "tier1/UtlSortVector.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

extern ConVar sv_pure_consensus;
extern ConVar sv_pure_retiretime;
extern ConVar sv_pure_trace;

CPureServerWhitelist::CCommand::CCommand()
{
}

CPureServerWhitelist::CCommand::~CCommand()
{
}


CPureServerWhitelist* CPureServerWhitelist::Create( IFileSystem *pFileSystem )
{
	CPureServerWhitelist *pRet = new CPureServerWhitelist;
	pRet->Init( pFileSystem );
	return pRet;
}


CPureServerWhitelist::CPureServerWhitelist()
{
	m_pFileSystem = NULL;
	m_LoadCounter = 0;
	m_RefCount = 1;
}


CPureServerWhitelist::~CPureServerWhitelist()
{
	Term();
}


void CPureServerWhitelist::Init( IFileSystem *pFileSystem )
{
	Term();
	m_pFileSystem = pFileSystem;
}

void CPureServerWhitelist::Load( int iPureMode )
{
	Term();

	// Not pure at all?
	if ( iPureMode < 0 )
		return;

	// Load base trusted keys
	{
		KeyValues *kv = new KeyValues( "" );
		bool bLoaded = kv->LoadFromFile( g_pFileSystem, "cfg/trusted_keys_base.txt", "game" );
		if ( bLoaded )
			bLoaded = LoadTrustedKeysFromKeyValues( kv );
		else
			Warning( "Error loading cfg/trusted_keys_base.txt\n" );
		kv->deleteThis();
	}

	// sv_pure 0: minimal rules only
	if ( iPureMode == 0 )
	{
		KeyValues *kv = new KeyValues( "" );
		bool bLoaded = kv->LoadFromFile( g_pFileSystem, "cfg/pure_server_minimal.txt", "game" );
		if ( bLoaded )
			bLoaded = LoadCommandsFromKeyValues( kv );
		else
			Warning( "Error loading cfg/pure_server_minimal.txt\n" );
		kv->deleteThis();
		return;
	}

	// Load up full pure rules
	{
		KeyValues *kv = new KeyValues( "" );
		bool bLoaded = kv->LoadFromFile( g_pFileSystem, "cfg/pure_server_full.txt", "game" );
		if ( bLoaded )
			bLoaded = LoadCommandsFromKeyValues( kv );
		else
			Warning( "Error loading cfg/pure_server_full.txt\n" );
		kv->deleteThis();
	}

	// Now load user customizations
	if ( iPureMode == 1 )
	{

		// Load custom whitelist
		KeyValues *kv = new KeyValues( "" );
		bool bLoaded = kv->LoadFromFile( g_pFileSystem, "cfg/pure_server_whitelist.txt", "game" );
		if ( !bLoaded )
			// Check the old location
			bLoaded = kv->LoadFromFile( g_pFileSystem, "pure_server_whitelist.txt", "game" );
		if ( bLoaded )
			bLoaded = LoadCommandsFromKeyValues( kv );
		else
			Msg( "pure_server_whitelist.txt not present; pure server using only base file rules\n" );
		kv->deleteThis();

		// Load custom trusted keys
		kv = new KeyValues( "" );
		bLoaded = kv->LoadFromFile( g_pFileSystem, "cfg/trusted_keys.txt", "game" );
		if ( bLoaded )
			bLoaded = LoadTrustedKeysFromKeyValues( kv );
		else
			Msg( "trusted_keys.txt not present; pure server using only base trusted key list\n" );
		kv->deleteThis();
	}

	// Hardcoded rules last
	AddHardcodedFileCommands();
}		

bool operator==( const PureServerPublicKey_t &a, const PureServerPublicKey_t &b )
{
	return a.Count() == b.Count() && V_memcmp( a.Base(), b.Base(), a.Count() ) == 0;
}

bool CPureServerWhitelist::CommandDictDifferent( const CUtlDict<CCommand*,int> &a, const CUtlDict<CCommand*,int> &b )
{
	FOR_EACH_DICT( a, idxA )
	{
		if ( a[idxA]->m_LoadOrder == kLoadOrder_HardcodedOverride )
			continue;
		int idxB = b.Find( a.GetElementName( idxA ) );
		if ( !b.IsValidIndex( idxB ) )
			return true;
		if ( b[idxB]->m_LoadOrder == kLoadOrder_HardcodedOverride )
			continue;
		if ( a[idxA]->m_eFileClass != b[idxB]->m_eFileClass
			|| a[idxA]->m_LoadOrder != b[idxB]->m_LoadOrder )
			return true;
	}

	return false;
}

bool CPureServerWhitelist::operator==( const CPureServerWhitelist &x ) const
{

	// Compare rule dictionaries
	if ( CommandDictDifferent( m_FileCommands, x.m_FileCommands )
		|| CommandDictDifferent( x.m_FileCommands, m_FileCommands )
		|| CommandDictDifferent( m_RecursiveDirCommands, x.m_RecursiveDirCommands )
		|| CommandDictDifferent( x.m_RecursiveDirCommands, m_RecursiveDirCommands )
		|| CommandDictDifferent( m_NonRecursiveDirCommands, x.m_NonRecursiveDirCommands )
		|| CommandDictDifferent( x.m_NonRecursiveDirCommands, m_NonRecursiveDirCommands ) )
		return false;

	// Compare trusted key list
	if ( m_vecTrustedKeys.Count() != x.m_vecTrustedKeys.Count() )
		return false;
	for ( int i = 0 ; i < m_vecTrustedKeys.Count() ; ++i )
		if ( !( m_vecTrustedKeys[i] == x.m_vecTrustedKeys[i] ) )
			return false;

	// they are the same
	return true;
}

void CPureServerWhitelist::Term()
{
	m_FileCommands.PurgeAndDeleteElements();
	m_RecursiveDirCommands.PurgeAndDeleteElements();
	m_NonRecursiveDirCommands.PurgeAndDeleteElements();
	m_vecTrustedKeys.Purge();
	m_LoadCounter = 0;
}


bool CPureServerWhitelist::LoadCommandsFromKeyValues( KeyValues *kv )
{
	for ( KeyValues *pCurItem = kv->GetFirstValue(); pCurItem; pCurItem = pCurItem->GetNextValue() )
	{
		char szPathName[ MAX_PATH ];
		const char *pKeyValue = pCurItem->GetName();
		const char *pModifiers = pCurItem->GetString();
		if ( !pKeyValue || !pModifiers )
			continue;
	
		Q_strncpy( szPathName, pKeyValue, sizeof(szPathName) );
		Q_FixSlashes( szPathName );
		const char *pValue = szPathName;

		// Figure out the modifiers.
		bool bFromTrustedSource = false, bAllowFromDisk = false, bCheckCRC = false, bAny = false;
		CUtlVector<char*> mods;
		V_SplitString( pModifiers, "+", mods );
		for ( int i=0; i < mods.Count(); i++ )
		{
			if (
				V_stricmp( mods[i], "from_steam" ) == 0
				|| V_stricmp( mods[i], "trusted_source" ) == 0
			)
				bFromTrustedSource = true;
			else if ( V_stricmp( mods[i], "allow_from_disk" ) == 0 )
				bAllowFromDisk = true;
			else if (
				V_stricmp( mods[i], "check_crc" ) == 0
				|| V_stricmp( mods[i], "check_hash" ) == 0
			)
				bCheckCRC = true;
			else if ( V_stricmp( mods[i], "any" ) == 0 )
				bAny = true;
			else
				Warning( "Unknown modifier in whitelist file: %s.\n", mods[i] );
		}
		mods.PurgeAndDeleteElements();
		if (
			( bFromTrustedSource && ( bAllowFromDisk || bCheckCRC || bAny ) )
			|| ( bAny && bCheckCRC ) )
		{
			Warning( "Whitelist: incompatible flags used on %s.\n", pValue );
			continue;
		}

		EPureServerFileClass eFileClass;
		if ( bCheckCRC )
			eFileClass = ePureServerFileClass_CheckHash;
		else if ( bFromTrustedSource )
			eFileClass = ePureServerFileClass_AnyTrusted;
		else
			eFileClass = ePureServerFileClass_Any;

		// Setup a new CCommand to hold this command.
		AddFileCommand( pValue, eFileClass, m_LoadCounter++ );
	}	
	
	return true;
}

void CPureServerWhitelist::AddHardcodedFileCommands()
{
	AddFileCommand( "materials/vgui/replay/thumbnails/...", ePureServerFileClass_Any, kLoadOrder_HardcodedOverride );
	AddFileCommand( "sound/ui/hitsound.wav", ePureServerFileClass_Any, kLoadOrder_HardcodedOverride );
	AddFileCommand( "sound/ui/killsound.wav", ePureServerFileClass_Any, kLoadOrder_HardcodedOverride );
	AddFileCommand( "materials/vgui/logos/...", ePureServerFileClass_Any, kLoadOrder_HardcodedOverride );
}

void CPureServerWhitelist::AddFileCommand( const char *pszFilePath, EPureServerFileClass eFileClass, unsigned short nLoadOrder )
{

	CPureServerWhitelist::CCommand *pCommand = new CPureServerWhitelist::CCommand;
	pCommand->m_LoadOrder = nLoadOrder;
	pCommand->m_eFileClass = eFileClass;

	// Figure out if they're referencing a file, a recursive directory, or a nonrecursive directory.		
	CUtlDict<CCommand*,int> *pList;
	const char *pEndPart = V_UnqualifiedFileName( pszFilePath );
	if ( Q_stricmp( pEndPart, "..." ) == 0 )
		pList = &m_RecursiveDirCommands;
	else if ( Q_stricmp( pEndPart, "*.*" ) == 0 )
		pList = &m_NonRecursiveDirCommands;
	else
		pList = &m_FileCommands;
		
	// If it's a directory command, get rid of the *.* or ...
	char filePath[MAX_PATH];
	if ( pList == &m_RecursiveDirCommands || pList == &m_NonRecursiveDirCommands )
		V_ExtractFilePath( pszFilePath, filePath, sizeof( filePath ) );
	else
		V_strncpy( filePath, pszFilePath, sizeof( filePath ) );

	V_FixSlashes( filePath );

	int idxExisting = pList->Find( filePath );
	if ( idxExisting != pList->InvalidIndex() )
	{
		delete pList->Element( idxExisting );
		pList->RemoveAt( idxExisting );
	}
	pList->Insert( filePath, pCommand );
}

bool CPureServerWhitelist::LoadTrustedKeysFromKeyValues( KeyValues *kv )
{
	for ( KeyValues *pCurItem = kv->GetFirstTrueSubKey(); pCurItem; pCurItem = pCurItem->GetNextTrueSubKey() )
	{
		if ( V_stricmp( pCurItem->GetName(), "public_key" ) != 0 )
		{
			Warning( "Trusted key list has unexpected block '%s'; expected only 'public_key' blocks\n", pCurItem->GetName() );
			continue;
		}

		const char *pszType = pCurItem->GetString( "type", "(none)" );
		if ( V_stricmp( pszType, "rsa" ) != 0 )
		{
			Warning( "Trusted key type '%s' not supported.\n", pszType );
			continue;
		}

		const char *pszKeyData = pCurItem->GetString( "rsa_public_key", "" );
		if ( *pszKeyData == '\0' )
		{
			Warning( "trusted key is missing 'rsa_public_key' data; ignored\n" );
			continue;
		}

		PureServerPublicKey_t &key = m_vecTrustedKeys[ m_vecTrustedKeys.AddToTail() ];
		int nKeyDataLen = V_strlen( pszKeyData );
		key.SetSize( nKeyDataLen / 2 );
		// Aaaannnnnnnnddddd V_hextobinary has no return code.
		// Because nobody could *ever* possible attempt to parse bad data.  It could never possibly happen.
		V_hextobinary( pszKeyData, nKeyDataLen, key.Base(), key.Count() );
	}

	return true;
}

void CPureServerWhitelist::UpdateCommandStats( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int *pHighest, int *pLongestPathName )
{
	for ( int i=commands.First(); i != commands.InvalidIndex(); i=commands.Next( i ) )
	{
		*pHighest = max( *pHighest, (int)commands[i]->m_LoadOrder );
		
		int len = V_strlen( commands.GetElementName( i ) );
		*pLongestPathName = max( *pLongestPathName, len );
	}
}

void CPureServerWhitelist::PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand )
{
	// Get rid of the trailing slash if there is one.
	char tempFileSpec[MAX_PATH];
	V_strncpy( tempFileSpec, pFileSpec, sizeof( tempFileSpec ) );
	int len = V_strlen( tempFileSpec );
	if ( len > 0 && (tempFileSpec[len-1] == '/' || tempFileSpec[len-1] == '\\') )
		tempFileSpec[len-1] = 0;

	CUtlString buf;
	if ( pExt )
		buf.Format( "%s%c%s", tempFileSpec, CORRECT_PATH_SEPARATOR, pExt );
	else
		buf.Format( "%s", tempFileSpec );

	len = V_strlen( pFileSpec );
	for ( int i=len; i < maxPathnameLen+6; i++ )
	{
		buf += " ";
	}

	buf += "\t";
	switch ( pCommand->m_eFileClass )
	{
		default:
			buf += va( "(bogus value %d)", (int)pCommand->m_eFileClass );
			Assert( false );
			break;

		case ePureServerFileClass_Any:
			buf += "any";
			break;

		case ePureServerFileClass_AnyTrusted:
			buf += "trusted_source";
			break;

		case ePureServerFileClass_CheckHash:
			buf += "check_crc";
			break;
	}
	
	Msg( "%s\n", buf.String() );
}


int CPureServerWhitelist::FindCommandByLoadOrder( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int iLoadOrder )
{
	for ( int i=commands.First(); i != commands.InvalidIndex(); i=commands.Next( i ) )
	{
		if ( commands[i]->m_LoadOrder == iLoadOrder )
			return i;
	}
	return -1;
}


void CPureServerWhitelist::PrintWhitelistContents()
{
	int highestLoadOrder = 0, longestPathName = 0;
	UpdateCommandStats( m_FileCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_RecursiveDirCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_NonRecursiveDirCommands, &highestLoadOrder, &longestPathName );
	
	for ( int iLoadOrder=0; iLoadOrder <= highestLoadOrder; iLoadOrder++ )
	{
		// Check regular file commands.
		int iCommand = FindCommandByLoadOrder( m_FileCommands, iLoadOrder );
		if ( iCommand != -1 )
		{
			PrintCommand( m_FileCommands.GetElementName( iCommand ), NULL, longestPathName, m_FileCommands[iCommand] );
		}
		else
		{
			// Check recursive commands.
			iCommand = FindCommandByLoadOrder( m_RecursiveDirCommands, iLoadOrder );
			if ( iCommand != -1 )
			{
				PrintCommand( m_RecursiveDirCommands.GetElementName( iCommand ), "...", longestPathName, m_RecursiveDirCommands[iCommand] );
			}
			else
			{
				// Check *.* commands.
				iCommand = FindCommandByLoadOrder( m_NonRecursiveDirCommands, iLoadOrder );
				if ( iCommand != -1 )
				{
					PrintCommand( m_NonRecursiveDirCommands.GetElementName( iCommand ), "*.*", longestPathName, m_NonRecursiveDirCommands[iCommand] );
				}
			}
		}
	}
}


void CPureServerWhitelist::Encode( CUtlBuffer &buf )
{
	// Put dummy version number
	buf.PutUnsignedInt( 0xffff );

	// Encode rules
	EncodeCommandList( m_FileCommands, buf );
	EncodeCommandList( m_RecursiveDirCommands, buf );
	EncodeCommandList( m_NonRecursiveDirCommands, buf );

	// Encode trusted keys
	buf.PutUnsignedInt( m_vecTrustedKeys.Count() );
	FOR_EACH_VEC( m_vecTrustedKeys, i )
	{
		uint32 nKeySize = m_vecTrustedKeys[i].Count();
		buf.PutUnsignedInt( nKeySize );
		buf.Put( m_vecTrustedKeys[i].Base(), nKeySize );
	}
}

void CPureServerWhitelist::EncodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf )
{
	// Count how many we're really going to write
	int nCount = 0;
	for ( int i=theList.First(); i != theList.InvalidIndex(); i = theList.Next( i ) )
	{
		if ( theList[i]->m_LoadOrder != kLoadOrder_HardcodedOverride )
			++nCount;
	}
	buf.PutInt( nCount );

	// Write them
	for ( int i=theList.First(); i != theList.InvalidIndex(); i = theList.Next( i ) )
	{
		CPureServerWhitelist::CCommand *pCommand = theList[i];
		if ( pCommand->m_LoadOrder == kLoadOrder_HardcodedOverride )
			continue;

		unsigned char val = (unsigned char)pCommand->m_eFileClass;

		buf.PutUnsignedChar( val );
		buf.PutUnsignedShort( pCommand->m_LoadOrder );
		buf.PutString( theList.GetElementName( i ) );
	}
}


void CPureServerWhitelist::Decode( CUtlBuffer &buf )
{
	Term();

	uint32 nVersionTag = *(uint32 *)buf.PeekGet();
	uint32 nFormatVersion = 0;
	if ( nVersionTag == 0xffff )
	{
		buf.GetUnsignedInt();
		nFormatVersion = 1;
	}
	else
	{
		// Talking to legacy server -- load up default rules,
		// the rest of his list are supposed to be exceptions to
		// the base
		Load( 2 );
	}
	DecodeCommandList( m_FileCommands, buf, nFormatVersion );
	DecodeCommandList( m_RecursiveDirCommands, buf, nFormatVersion );
	DecodeCommandList( m_NonRecursiveDirCommands, buf, nFormatVersion );

	// Hardcoded
	AddHardcodedFileCommands();

	if ( nFormatVersion >= 1 )
	{
		uint32 nKeyCount = buf.GetUnsignedInt();
		m_vecTrustedKeys.SetCount( nKeyCount );
		FOR_EACH_VEC( m_vecTrustedKeys, i )
		{
			uint32 nKeySize = buf.GetUnsignedInt();
			m_vecTrustedKeys[i].SetCount( nKeySize );
			buf.Get( m_vecTrustedKeys[i].Base(), nKeySize );
		}
	}
}


void CPureServerWhitelist::CacheFileCRCs()
{
	InternalCacheFileCRCs( m_FileCommands, k_eCacheCRCType_SingleFile );
	InternalCacheFileCRCs( m_NonRecursiveDirCommands, k_eCacheCRCType_Directory );
	InternalCacheFileCRCs( m_RecursiveDirCommands, k_eCacheCRCType_Directory_Recursive );
}


// !SV_PURE FIXME! Do we need this?
void CPureServerWhitelist::InternalCacheFileCRCs( CUtlDict<CCommand*,int> &theList, ECacheCRCType eType )
{
//	for ( int i=theList.First(); i != theList.InvalidIndex(); i = theList.Next( i ) )
//	{
//		CCommand *pCommand = theList[i];
//		if ( pCommand->m_bCheckCRC )
//		{
//			const char *pPathname = theList.GetElementName( i );
//			m_pFileSystem->CacheFileCRCs( pPathname, eType, &m_ForceMatchList );
//		}
//	}
}


void CPureServerWhitelist::DecodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf, uint32 nFormatVersion )
{
	int nCommands = buf.GetInt();
	
	for ( int i=0; i < nCommands; i++ )
	{
		CPureServerWhitelist::CCommand *pCommand = new CPureServerWhitelist::CCommand;

		unsigned char val = buf.GetUnsignedChar();
		unsigned short nLoadOrder = buf.GetUnsignedShort();

		if ( nFormatVersion == 0 )
		{
			pCommand->m_eFileClass = ( val & 1 ) ? ePureServerFileClass_Any : ePureServerFileClass_AnyTrusted;
			pCommand->m_LoadOrder = nLoadOrder + m_LoadCounter;
		}
		else
		{
			pCommand->m_eFileClass = (EPureServerFileClass)val;
			pCommand->m_LoadOrder = nLoadOrder;
		}

		char str[MAX_PATH];
		buf.GetString( str );
		V_FixSlashes( str );
		
		theList.Insert( str, pCommand );
	}
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::GetBestEntry( const char *pFilename )
{
	// NOTE: Since this is a user-specified file, we don't have the added complexity of path IDs in here.
	// So when the filesystem asks if a file is in the whitelist, we just ignore the path ID.
	
	// Make sure we have a relative pathname with fixed slashes..
	char relativeFilename[MAX_PATH];
	V_strncpy( relativeFilename, pFilename, sizeof( relativeFilename ) );

	// Convert the path to relative if necessary.
	if ( !V_IsAbsolutePath( relativeFilename ) || m_pFileSystem->FullPathToRelativePath( pFilename, relativeFilename, sizeof( relativeFilename ) ) )
	{
		V_FixSlashes( relativeFilename );
		
		// Get the directory this thing is in.
		char relativeDir[MAX_PATH];
		if ( !V_ExtractFilePath( relativeFilename, relativeDir, sizeof( relativeDir ) )	)
			relativeDir[0] = 0;
		
		
		// Check each of our dictionaries to see if there is an entry for this thing.
		CCommand *pBestEntry = NULL;
		
		pBestEntry = CheckEntry( m_FileCommands, relativeFilename, pBestEntry );
		if ( relativeDir[0] != 0 )
		{
			pBestEntry = CheckEntry( m_NonRecursiveDirCommands, relativeDir, pBestEntry );

			while ( relativeDir[0] != 0 )
			{
				// Check for this directory.
				pBestEntry = CheckEntry( m_RecursiveDirCommands, relativeDir, pBestEntry );
				if ( !V_StripLastDir( relativeDir, sizeof( relativeDir ) ) )
					break;
			}
		}
			
		return pBestEntry;
	}
	
	// Either we couldn't find an entry, or they specified an absolute path that we could not convert to a relative path.
	return NULL;
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::CheckEntry( 
	CUtlDict<CPureServerWhitelist::CCommand*,int> &dict, 
	const char *pEntryName, 
	CPureServerWhitelist::CCommand *pBestEntry )
{
	int i = dict.Find( pEntryName );
	if ( i != dict.InvalidIndex() && (!pBestEntry || dict[i]->m_LoadOrder > pBestEntry->m_LoadOrder) )
		pBestEntry = dict[i];
	
	return pBestEntry;
}


void CPureServerWhitelist::AddRef()
{
	ThreadInterlockedIncrement( &m_RefCount );
}

void CPureServerWhitelist::Release()
{
	if ( ThreadInterlockedDecrement( &m_RefCount ) <= 0 )
		delete this;
}

int CPureServerWhitelist::GetTrustedKeyCount() const
{
	return m_vecTrustedKeys.Count();
}

const byte *CPureServerWhitelist::GetTrustedKey( int iKeyIndex, int *nKeySize ) const
{
	Assert( nKeySize != NULL );
	if ( !m_vecTrustedKeys.IsValidIndex( iKeyIndex ) )
	{
		*nKeySize = 0;
		return NULL;
	}

	*nKeySize = m_vecTrustedKeys[iKeyIndex].Count();
	return m_vecTrustedKeys[iKeyIndex].Base();
}


EPureServerFileClass	CPureServerWhitelist::GetFileClass( const char *pszFilename )
{
	CCommand *pCommand = GetBestEntry( pszFilename );
	if ( pCommand )
		return pCommand->m_eFileClass;

	// Default action is to be permissive.  (The default whitelist protects certain directories and files at a root level.)
	return ePureServerFileClass_Any;
}

void CPureFileTracker::AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord )
{
	UserReportedFileHash_t userFileHash;
	userFileHash.m_idxFile = idxFile;
	userFileHash.m_userID = userID;
	userFileHash.m_FileHash = *pFileHash;
	int idxUserReported = m_treeUserReportedFileHash.Find( userFileHash );
	if ( idxUserReported == m_treeUserReportedFileHash.InvalidIndex() )
	{
		idxUserReported = m_treeUserReportedFileHash.Insert( userFileHash );
		if ( bAddMasterRecord )
		{
			// count the number of matches for this idxFile
			// if it exceeds > 5 then make a master record
			int idxFirst = idxUserReported;
			int idxLast = idxUserReported;
			int ctMatches = 1;
			int ctTotalFiles = 1;
			// first go forward
			int idx = m_treeUserReportedFileHash.NextInorder( idxUserReported );
			while ( idx != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxUserReported].m_idxFile )
			{
				if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				idxLast = idx;
				idx = m_treeUserReportedFileHash.NextInorder( idx );
			}
			// then backwards
			idx = m_treeUserReportedFileHash.PrevInorder( idxUserReported );
			while ( idx != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxUserReported].m_idxFile )
			{
				if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				idxFirst = idx;
				idx = m_treeUserReportedFileHash.PrevInorder( idx );
			}
			// if ctTotalFiles >> ctMatches then that means clients are reading different bits from the file.
			// in order to get this right we need to ask them to read the entire thing
			if ( ctMatches >= sv_pure_consensus.GetInt() )
			{
				MasterFileHash_t masterFileHashNew;
				masterFileHashNew.m_idxFile = m_treeUserReportedFileHash[idxUserReported].m_idxFile;
				masterFileHashNew.m_cMatches = ctMatches;
				masterFileHashNew.m_FileHash = m_treeUserReportedFileHash[idxUserReported].m_FileHash;
				m_treeMasterFileHashes.Insert( masterFileHashNew );
				// remove all the individual records that matched the new master, we don't need them anymore
				int idxRemove = idxFirst;
				while ( idxRemove != m_treeUserReportedFileHash.InvalidIndex() )
				{
					int idxNext = m_treeUserReportedFileHash.NextInorder( idxRemove );
					if ( m_treeUserReportedFileHash[idxRemove].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
						m_treeUserReportedFileHash.RemoveAt( idxRemove );
					if ( idxRemove == idxLast )
						break;
					idxRemove = idxNext;
				}
			}
		}
	}
	else
	{
		m_treeUserReportedFileHash[idxUserReported].m_FileHash = *pFileHash;
	}
	// we dont have enough data to decide if you match or not yet - so we call it a match
}


void FileRenderHelper( USERID_t userID, const char *pchMessage, const char *pchPath, const char *pchFileName, FileHash_t *pFileHash, int nFileFraction, FileHash_t *pFileHashLocal )
{
	char rgch[256];
	char hex[ 34 ];
	Q_memset( hex, 0, sizeof( hex ) );
	Q_binarytohex( (const byte *)&pFileHash->m_md5contents.bits, sizeof( pFileHash->m_md5contents.bits ), hex, sizeof( hex ) );

	char hex2[ 34 ];
	Q_memset( hex2, 0, sizeof( hex2 ) );
	if ( pFileHashLocal )
		Q_binarytohex( (const byte *)&pFileHashLocal->m_md5contents.bits, sizeof( pFileHashLocal->m_md5contents.bits ), hex2, sizeof( hex2 ) );

	if ( pFileHash->m_PackFileID )
	{
		Q_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %8.8x %6.6x ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_PackFileID, pFileHash->m_nPackFileNumber, nFileFraction, pFileHash->m_cbFileLen,
			pchMessage, 
			hex, hex2 );
	}
	else
	{
		Q_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %x ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_eFileHashType, pFileHash->m_cbFileLen, pFileHash->m_eFileHashType ? pFileHash->m_crcIOSequence : 0,
			pchMessage, 
			hex, hex2 );
	}
	if ( userID.idtype != 0 )
		Msg( "[%s] %s\n", GetUserIDString(userID), rgch );
	else
		Msg( "%s", rgch );

}


bool CPureFileTracker::DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID )
{
//	// if the server has been idle for more than 15 minutes, discard all this data
//	const float flRetireTime = sv_pure_retiretime.GetFloat();
//	float flCurTime = Plat_FloatTime();
//	if ( ( flCurTime - m_flLastFileReceivedTime ) > flRetireTime )
//	{
//		m_treeMasterFileHashes.RemoveAll();
//		m_treeUserReportedFileHash.RemoveAll();
//		m_treeMasterFileHashes.RemoveAll();
//	}
//	m_flLastFileReceivedTime = flCurTime;
//
//	// The clients must send us all files. We decide if it is whitelisted or not
//	// That way the clients can not hide modified files in a whitelisted directory
//	if ( pFileHash->m_PackFileID == 0 && 
//		sv.GetPureServerWhitelist()->GetFileClass( pRelativeFilename ) != ePureServerFileClass_CheckHash )
//	{
//
//		if ( sv_pure_trace.GetInt() == 4 )
//		{
//			char warningStr[1024] = {0};
//			V_snprintf( warningStr, sizeof( warningStr ), "Pure server: file [%s]\\%s ignored by whitelist.", pPathID, pRelativeFilename );
//			Msg( "[%s] %s\n", GetUserIDString(userID), warningStr );
//		}
//
//		return true;
//	}
//
//	char rgchFilenameFixed[MAX_PATH];
//	Q_strncpy( rgchFilenameFixed, pRelativeFilename, sizeof( rgchFilenameFixed ) );
//	Q_FixSlashes( rgchFilenameFixed );
//
//	// first look up the file and see if we have ever seen it before
//	CRC32_t crcFilename;
//	CRC32_Init( &crcFilename );
//	CRC32_ProcessBuffer( &crcFilename, rgchFilenameFixed, Q_strlen( rgchFilenameFixed ) );
//	CRC32_ProcessBuffer( &crcFilename, pPathID, Q_strlen( pPathID ) );
//	CRC32_Final( &crcFilename );
//	UserReportedFile_t ufile;
//	ufile.m_crcIdentifier = crcFilename;
//	ufile.m_filename = rgchFilenameFixed;
//	ufile.m_path = pPathID;
//	ufile.m_nFileFraction = nFileFraction;
//	int idxFile = m_treeAllReportedFiles.Find( ufile );
//	if ( idxFile == m_treeAllReportedFiles.InvalidIndex() )
//	{
//		idxFile = m_treeAllReportedFiles.Insert( ufile );
//	}
//	else
//	{
//		m_cMatchedFile++;
//	}
//	// then check if we have a master CRC for the file
//	MasterFileHash_t masterFileHash;
//	masterFileHash.m_idxFile = idxFile;
//	int idxMaster = m_treeMasterFileHashes.Find( masterFileHash );
//	// dont do anything with this yet
//
//	// check to see if we have loaded the file locally and can match it
//	FileHash_t filehashLocal;
//	EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
//	if ( eStatus == k_eFileCRCStatus_FileInVPK)
//	{
//		// you managed to load a file outside a VPK that the server has in the VPK
//		// this is possible if the user explodes the VPKs into individual files and then deletes the VPKs
//		FileRenderHelper( userID, "file should be in VPK", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
//		return false;
//	}
//	// if the user sent us a full file hash, but we dont have one, hash it now
//	if ( pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile && 
//		( eStatus != k_eFileCRCStatus_GotCRC || filehashLocal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile ) )
//	{
//		// lets actually read the file so we get a complete file hash
//		FileHandle_t f = g_pFileSystem->Open( rgchFilenameFixed, "rb", pPathID);
//		// try to load the file and really compute the hash - should only have to do this once ever
//		if ( f )
//		{
//			// load file into a null-terminated buffer
//			int fileSize = g_pFileSystem->Size( f );
//			unsigned bufSize = g_pFileSystem->GetOptimalReadSize( f, fileSize );
//
//			char *buffer = (char*)g_pFileSystem->AllocOptimalReadBuffer( f, bufSize );
//			Assert( buffer );
//
//			// read into local buffer
//			bool bRetOK = ( g_pFileSystem->ReadEx( buffer, bufSize, fileSize, f ) != 0 );
//			bRetOK;
//			g_pFileSystem->FreeOptimalReadBuffer( buffer );
//
//			g_pFileSystem->Close( f );	// close file after reading
//
//			eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
//		}
//		else
//		{
//			// what should we do if we couldn't open the file? should probably kick
//			FileRenderHelper( userID, "could not open file to hash ( benign for now )", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
//		}
//	}
//	if ( eStatus == k_eFileCRCStatus_GotCRC )
//	{
//		if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
//			pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile )
//		{
//			if ( filehashLocal == *pFileHash )
//			{
//				m_cMatchedFileFullHash++;
//				return true;
//			}
//			else
//			{
//				// don't need to check anything else
//				// did not match - record so that we have a record of the file that did not match ( just for reporting )
//				AddUserReportedFileHash( idxFile, pFileHash, userID, false );
//				FileRenderHelper( userID, "file does not match", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, &filehashLocal );
//				return false;
//			}
//		}
//	}
//
//	// if this is a VPK file, we have completely cataloged all the VPK files, so no suprises are allowed
//	if ( pFileHash->m_PackFileID )
//	{
//		AddUserReportedFileHash( idxFile, pFileHash, userID, false );
//		FileRenderHelper( userID, "unrecognized vpk file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
//		return false;
//	}
//
//
//	// now lets see if we have a master file hash for this
//	if ( idxMaster != m_treeMasterFileHashes.InvalidIndex() )
//	{
//		m_cMatchedMasterFile++;
//
//		FileHash_t *pFileHashLocal = &m_treeMasterFileHashes[idxMaster].m_FileHash;
//		if ( *pFileHashLocal == *pFileHash )
//		{
//			m_cMatchedMasterFileHash++;
//			return true;
//		}
//		else
//		{
//			// did not match - record so that we have a record of the file that did not match ( just for reporting )
//			AddUserReportedFileHash( idxFile, pFileHash, userID, false );
//			// and then return failure
//			FileRenderHelper( userID, "file does not match server master file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, pFileHashLocal );
//			return false;
//		}
//	}
//
//	// no master record, accumulate individual record so we can get a consensus
//	if ( sv_pure_trace.GetInt() == 3 )
//	{
//		FileRenderHelper( userID, "server does not have hash for this file. Waiting for consensus", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
//	}
//
//	AddUserReportedFileHash( idxFile, pFileHash, userID, true );

	// we dont have enough data to decide if you match or not yet - so we call it a match
	return true;
}


struct FindFileIndex_t
{
	int	idxFindFile;
};

class CStupidLess
{
public:
	bool Less( const FindFileIndex_t &src1, const FindFileIndex_t &src2, void *pCtx )
	{
		if ( src1.idxFindFile < src2.idxFindFile )
			return true;

		return false;
	}
};

int CPureFileTracker::ListUserFiles( bool bListAll, const char *pchFilenameFind )
{
	CUtlSortVector< FindFileIndex_t, CStupidLess > m_vecReportedFiles;
	int idxFindFile = m_treeAllReportedFiles.FirstInorder();
	while ( idxFindFile != m_treeAllReportedFiles.InvalidIndex() )
	{
		UserReportedFile_t &ufile = m_treeAllReportedFiles[idxFindFile];
		if ( pchFilenameFind && Q_stristr( ufile.m_filename, pchFilenameFind ) )
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.String(), ufile.m_filename.String(), 0, &filehashLocal );

			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				USERID_t useridFake;
				useridFake.idtype = IDTYPE_STEAM;
				FileRenderHelper( useridFake, "Found: ",ufile.m_path.String(),ufile.m_filename.String(), &filehashLocal, 0, NULL );
				FindFileIndex_t ffi;
				ffi.idxFindFile = idxFindFile;
				m_vecReportedFiles.Insert( ffi );
			}
			else
			{
				Msg( "File not found %s %s %x\n", ufile.m_filename.String(), ufile.m_path.String(), idxFindFile );
			}
		}
		idxFindFile = m_treeAllReportedFiles.NextInorder( idxFindFile );
	}


	int cTotalFiles = 0;
	int cTotalMatches = 0;
	int idx = m_treeUserReportedFileHash.FirstInorder();
	while ( idx != m_treeUserReportedFileHash.InvalidIndex() )
	{
		UserReportedFileHash_t &file = m_treeUserReportedFileHash[idx];

		int idxNext = m_treeUserReportedFileHash.NextInorder( idx );
		int ctMatches = 1;
		int ctFiles = 1;
		// check this against all others for the same file
		while ( idxNext != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxNext].m_idxFile )
		{
			if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxNext].m_FileHash )
			{
				ctMatches++;
				cTotalMatches++;
			}
			ctFiles++;
			idxNext = m_treeUserReportedFileHash.NextInorder( idxNext );
		}
		idx = m_treeUserReportedFileHash.NextInorder( idx );
		cTotalFiles++;

		// do we have a master for this one?
		MasterFileHash_t masterFileHashFind;
		masterFileHashFind.m_idxFile = file.m_idxFile;
		int idxMaster = m_treeMasterFileHashes.Find( masterFileHashFind );

		UserReportedFile_t &ufile = m_treeAllReportedFiles[file.m_idxFile];

		bool bOutput = false;
		if ( Q_stristr( ufile.m_filename.String(), "bin\\pak01" )!=NULL || Q_stristr( ufile.m_filename.String(), ".vpk" )!=NULL )
			bOutput = true;
		else
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.String(), ufile.m_filename.String(), 0, &filehashLocal );
			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					file.m_FileHash.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					filehashLocal != file.m_FileHash )
				{
					bOutput = true;
				}
			}
		}

		FindFileIndex_t ffi;
		ffi.idxFindFile = file.m_idxFile;

		if ( ctMatches != ctFiles || idxMaster != m_treeMasterFileHashes.InvalidIndex() || bListAll || ( pchFilenameFind && m_vecReportedFiles.Find( ffi ) != -1 ) || bOutput )
		{
			char rgch[256];
			Q_snprintf( rgch, 256, "reports=%d matches=%d Hash details:", ctFiles, ctMatches );
			FileHash_t *pFileHashMaster = NULL;
			if ( idxMaster != m_treeMasterFileHashes.InvalidIndex() )
				pFileHashMaster = &m_treeMasterFileHashes[idxMaster].m_FileHash;
			FileRenderHelper( file.m_userID, rgch, ufile.m_path.String(), ufile.m_filename.String(), &file.m_FileHash, 0, pFileHashMaster );
		}
	}
	Msg( "Total user files %d %d %d \n", m_treeUserReportedFileHash.Count(), cTotalFiles, cTotalMatches );
	Msg( "Total files %d, total with authoritative hashes %d \n", m_treeAllReportedFiles.Count(), m_treeMasterFileHashes.Count() );
	Msg( "Matching files %d %d %d \n", m_cMatchedFile, m_cMatchedMasterFile, m_cMatchedMasterFileHash );

	return 0;
}

int CPureFileTracker::ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax )
{
	g_pFileSystem->MarkAllCRCsUnverified();
	
	int cTotal = 0;
	int cTotalMatch = 0;
	int count = 0;
	do
	{
		CUnverifiedFileHash rgUnverifiedFiles[1];
		count = g_pFileSystem->GetUnverifiedFileHashes( rgUnverifiedFiles, ARRAYSIZE( rgUnverifiedFiles ) );

		if ( count && ( bListAll || ( pchFilenameFind && Q_stristr( rgUnverifiedFiles[0].m_Filename, pchFilenameFind ) && rgUnverifiedFiles[0].m_nFileFraction >= nFileFractionMin && rgUnverifiedFiles[0].m_nFileFraction <= nFileFractionMax ) ) )
		{
			USERID_t useridFake;
			useridFake.idtype = IDTYPE_STEAM;
			FileRenderHelper( useridFake, "", rgUnverifiedFiles[0].m_PathID, rgUnverifiedFiles[0].m_Filename, &rgUnverifiedFiles[0].m_FileHash, rgUnverifiedFiles[0].m_nFileFraction, NULL );
			if ( rgUnverifiedFiles[0].m_FileHash.m_PackFileID )
			{
				g_pFileSystem->CheckVPKFileHash( rgUnverifiedFiles[0].m_FileHash.m_PackFileID, rgUnverifiedFiles[0].m_FileHash.m_nPackFileNumber, rgUnverifiedFiles[0].m_nFileFraction, rgUnverifiedFiles[0].m_FileHash.m_md5contents );
			}
			cTotalMatch++;
		}
		if ( count )
			cTotal++;
	} while ( count );

	Msg( "Total files %d Matching files %d \n", cTotal, cTotalMatch );

	return 0;
}


CPureFileTracker g_PureFileTracker;

//#define DEBUG_PURE_SERVER
#ifdef DEBUG_PURE_SERVER
void CC_ListPureServerFiles(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( args.ArgC() > 1 && (atoi(args[1]) > 0), NULL );
}

static ConCommand svpurelistuserfiles("sv_pure_listuserfiles", CC_ListPureServerFiles, "ListPureServerFiles");


void CC_PureServerFindFile(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( false, args[1] );
}

static ConCommand svpurefinduserfiles("sv_pure_finduserfiles", CC_PureServerFindFile, "ListPureServerFiles");

void CC_PureServerListTrackedFiles(const CCommand &args)
{
	// BUGBUG! Because this code is in engine instead of server, it exists in the client - ugh!
	// Remove this command from client before shipping for realz.
	//if ( !sv.IsDedicated() )
	//	return;
	int nFileFractionMin = args.ArgC() >= 3 ? Q_atoi(args[2]) : 0;
	int nFileFractionMax = args.ArgC() >= 4 ? Q_atoi(args[3]) : nFileFractionMin;
	if ( nFileFractionMax < 0 ) 
		nFileFractionMax = 0x7FFFFFFF;
	g_PureFileTracker.ListAllTrackedFiles( args.ArgC() <= 1, args.ArgC() >= 2 ? args[1] : NULL, nFileFractionMin, nFileFractionMax );
}

static ConCommand svpurelistfiles("sv_pure_listfiles", CC_PureServerListTrackedFiles, "ListPureServerFiles");

void CC_PureServerCheckVPKFiles(const CCommand &args)
{
	if ( sv.IsDedicated() )
		Plat_BeginWatchdogTimer( 5 * 60 );							// reset watchdog timer to allow 5 minutes for the VPK check
	g_pFileSystem->CacheAllVPKFileHashes( false, true );
	if ( sv.IsDedicated() )
		Plat_EndWatchdogTimer();
}

static ConCommand svpurecheckvpks("sv_pure_checkvpk", CC_PureServerCheckVPKFiles, "CheckPureServerVPKFiles");

#endif
