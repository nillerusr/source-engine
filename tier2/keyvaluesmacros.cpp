//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================


#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "tier2/keyvaluesmacros.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
// Returns true if the passed string matches the filename style glob, false otherwise
// * matches any characters, ? matches any single character, otherwise case insensitive matching
//--------------------------------------------------------------------------------------------------
bool GlobMatch( const char *pszGlob, const char *pszString )
{
	while ( ( *pszString != '\0' ) && ( *pszGlob != '*' ) )
	{
		if ( ( V_strnicmp( pszGlob, pszString, 1 ) != 0 ) && ( *pszGlob != '?' ) )
		{
			return false;
		}

		++pszGlob;
		++pszString;
	}

	const char *pszGlobTmp = nullptr;
	const char *pszStringTmp = nullptr;

	while ( *pszString )
	{
		if ( *pszGlob == '*' )
		{
			++pszGlob;

			if ( *pszGlob == '\0' )
			{
				return true;
			}

			pszGlobTmp = pszGlob;
			pszStringTmp = pszString + 1;
		}
		else if ( ( V_strnicmp( pszGlob, pszString, 1 ) == 0 ) || ( *pszGlob == '?' ) )
		{
			++pszGlob;
			++pszString;
		}
		else
		{
			pszGlob = pszGlobTmp;
			pszString = pszStringTmp++;
		}
	}

	while ( *pszGlob == '*' )
	{
		++pszGlob;
	}

	return *pszGlob == '\0';
}


//--------------------------------------------------------------------------------------------------
// Inserts pkvToInsert after pkvAfter but setting pkvAfter's NextKey to pkvInsert
//--------------------------------------------------------------------------------------------------
static void InsertKeyValuesAfter( KeyValues *pkvAfter, KeyValues *pkvToInsert )
{
	Assert( pkvAfter );
	Assert( pkvToInsert );

	pkvToInsert->SetNextKey( pkvAfter->GetNextKey() );
	pkvAfter->SetNextKey( pkvToInsert );
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static KeyValues *ReplaceSubKeyWithCopy( KeyValues *pkvParent, KeyValues *pkvToReplace, KeyValues *pkvReplaceWith )
{
	Assert( pkvReplaceWith->GetFirstSubKey() == nullptr );

	KeyValues *pkvCopy = pkvReplaceWith->MakeCopy();
	Assert( pkvCopy->GetFirstSubKey() == nullptr );
	Assert( pkvCopy->GetNextKey() == nullptr );

	InsertKeyValuesAfter( pkvToReplace, pkvCopy );
	pkvParent->RemoveSubKey( pkvToReplace );
	pkvToReplace->deleteThis();

	return pkvCopy;
}


//--------------------------------------------------------------------------------------------------
// Handles a KeyValues #insert macro.  Replaces the #insert KeyValues with the specified file
// if it can be loaded.  This is not called #include because base KeyValue's already has #include
// but it has two issues.  The #include is relative to the keyvalues, these paths are resolved
// normally via IFileSystem and #include only works at the top level, #insert works no matter
// how deep the #insert macro is
//--------------------------------------------------------------------------------------------------
static KeyValues *HandleKeyValuesMacro_Insert( KeyValues *pkvInsert, KeyValues *pkvParent )
{
	const char *pszName = pkvInsert->GetName();

	if ( V_stricmp( "#insert", pszName ) != 0 )
		return nullptr;

	// Have an #insert key

	if ( pkvInsert->GetFirstSubKey() )
	{
		// Invalid, has sub keys
		Msg( "Error: #insert on key with subkeys, can only do #insert with simple key/value with string value\n" );
		return nullptr;
	}

	if ( pkvInsert->GetDataType() != KeyValues::TYPE_STRING )
	{
		// Invalid, value isn't a string
		Msg( "Error: #insert on key without a data type of string, can only do #insert with simple key/value with string value\n" );
		return nullptr;
	}

	const char *pszInsert = pkvInsert->GetString();
	if ( !pszInsert && *pszInsert == '\0' )
	{
		// Invalid, value is empty string
		Msg( "Error: #insert on key with empty string value, can only do #insert with simple key/value with string value\n" );
		return nullptr;
	}

	FileHandle_t f = g_pFullFileSystem->Open( pszInsert, "rb" );
	if ( !f )
	{
		// Invalid, couldn't open #insert file
		Msg( "Error: #insert couldn't open file: %s\n", pszInsert );
		return nullptr;
	}

	uint nFileSize = g_pFullFileSystem->Size( f );
	if ( nFileSize == 0 )
	{
		// Invalid, empty file
		Msg( "Error: #insert empty file: %s\n", pszInsert );
		return nullptr;
	}

	uint nBufSize = g_pFullFileSystem->GetOptimalReadSize( f, nFileSize + 2 /* null termination */ + 8 /* "\"x\"\n{\n}\n" */ );
	char *pBuf = ( char* )g_pFullFileSystem->AllocOptimalReadBuffer( f, nBufSize );
	pBuf[0] = '"';
	pBuf[1] = 'i';
	pBuf[2] = '"';
	pBuf[3] = '\n';
	pBuf[4] = '{';
	pBuf[5] = '\n';

	bool bRetOK = ( g_pFullFileSystem->ReadEx( pBuf + 6, nBufSize - 6, nFileSize, f ) != 0 );

	g_pFullFileSystem->Close( f );

	KeyValues *pkvNew = nullptr;

	if ( bRetOK )
	{
		pBuf[nFileSize + 6 + 0] = '}';
		pBuf[nFileSize + 6 + 1] = '\n';
		pBuf[nFileSize + 6 + 2] = '\0';
		pBuf[nFileSize + 6 + 3] = '\0';	// Double NULL termination

		pkvNew = new KeyValues( pszInsert );

		bRetOK = pkvNew->LoadFromBuffer( pszInsert, pBuf, g_pFullFileSystem );
	}
	else
	{
		Msg( "Error: #insert couldn't read file: %s\n", pszInsert );
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( pBuf );

	KeyValues *pkvReturn = nullptr;

	CUtlVector< KeyValues * > newKeyList;

	if ( bRetOK )
	{
		KeyValues *pkvInsertAfter = pkvInsert;

		KeyValues *pkvNewSubKey = pkvNew->GetFirstSubKey();
		pkvReturn = pkvNewSubKey;

		while ( pkvNewSubKey )
		{
			KeyValues *pkvNextNewSubKey = pkvNewSubKey->GetNextKey();

			pkvNew->RemoveSubKey( pkvNewSubKey );

			bool bFound = false;

			if ( pkvNewSubKey->GetFirstSubKey() == nullptr )
			{
				for ( KeyValues *pkvChild = pkvParent->GetFirstSubKey(); pkvChild; pkvChild = pkvChild->GetNextKey() )
				{
					if ( pkvChild == pkvInsert )
						continue;

					if ( pkvChild->GetNameSymbol() == pkvNewSubKey->GetNameSymbol() )
					{
						bFound = true;
						break;
					}
				}
			}

			if ( !bFound )
			{
				InsertKeyValuesAfter( pkvInsertAfter, pkvNewSubKey );

				pkvInsertAfter = pkvNewSubKey;

				newKeyList.AddToTail( pkvNewSubKey );
			}

			pkvNewSubKey = pkvNextNewSubKey;
		}

		pkvParent->RemoveSubKey( pkvInsert );
		pkvInsert->deleteThis();
	}

	if ( pkvNew )
	{
		pkvNew->deleteThis();
	}

	for ( int i = 0; i < newKeyList.Count(); ++i )
	{
		HandleKeyValuesMacros( pkvParent, newKeyList[i] );
	}

	return pkvReturn;
}


//-----------------------------------------------------------------------------
// Merge pkvSrc over pkvDst, adding any new keys from src to dst but overwriting
// existing keys in dst with keys with matching names from src
//-----------------------------------------------------------------------------
static void UpdateKeyValuesBlock( KeyValues *pkvDst, KeyValues *pkvUpdate )
{
	Assert( pkvDst->GetFirstSubKey() );
	Assert( pkvUpdate->GetFirstSubKey() );

	for ( KeyValues *pkvUpdateSubKey = pkvUpdate->GetFirstSubKey(); pkvUpdateSubKey; pkvUpdateSubKey = pkvUpdateSubKey->GetNextKey() )
	{
		const int nSrcName = pkvUpdateSubKey->GetNameSymbol();

		if ( pkvUpdateSubKey->GetFirstSubKey() )
		{
			Msg( "Error: #update has a key with subkeys, only simple key/values are allowed for #update, skipping: %s\n", pkvUpdateSubKey->GetName() );
			continue;
		}

		KeyValues *pkvNew = nullptr;

		// Check for an existing key with the same name
		for ( KeyValues *pkvDstSubKey = pkvDst->GetFirstSubKey(); pkvDstSubKey; pkvDstSubKey = pkvDstSubKey->GetNextKey() )
		{
			if ( pkvDstSubKey == pkvUpdate )
				continue;

			const int nDstName = pkvDstSubKey->GetNameSymbol();

			if ( nSrcName == nDstName )
			{
				pkvNew = ReplaceSubKeyWithCopy( pkvDst, pkvDstSubKey, pkvUpdateSubKey );
				break;
			}
		}

		if ( !pkvNew )
		{
			// Didn't update an existing key, add a key
			pkvNew = pkvUpdateSubKey->MakeCopy();
			pkvDst->AddSubKey( pkvNew );	// TODO: Perhaps add this right after the #update block?
		}

		Assert( pkvNew );

		// Do inserts right away
		if ( !V_strcmp( pkvNew->GetName(), "#insert" ) )
		{
			while ( pkvNew )
			{
				pkvNew = HandleKeyValuesMacros( pkvNew, pkvDst );
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------
// Handle's #update macros
//
// An #update must be a KeyValue block with a KeyValue block as a parent.  It will look at sibling
// KeyValue blocks which match an optional "#glob", or all sibling KeyValue blocks if no "#glob" is
// specified and will merge all of the #update block's subkeys into each sibling block.
// overwriting KeyValues if they already exist and adding new KeyValues if they don't.
//
// Example:
//
// Before:
//
//	"example"
//	{
//		"wear_level_1"
//		{
//			"one"	"one_val"
//			"two"	"two_val"
//		}
//		"wear_level_2"
//		{
//			"one"	"one_val"
//			"two"	"two_val"
//		}
//		"subblock"
//		{
//			"one"	"one_val"
//			"two"	"two_val"
//		}
//		"#update"
//		{
//			"#glob"	"wear_level_*"
//			"one"	"updated_one_val"
//			"three"	"three_val"
//		}
//	}
//
// After:
//
//	"example"
//	{
//		"wear_level_1"
//		{
//			"one"	"updated_one_val"
//			"two"	"two_val"
//			"three"	"three_val"
//		}
//		"wear_level_2"
//		{
//			"one"	"updated_one_val"
//			"two"	"two_val"
//			"three"	"three_val"
//		}
//		"subblock"
//		{
//			"one"	"one_val"
//			"two"	"two_val"
//		}
//	}
//
//--------------------------------------------------------------------------------------------------
static KeyValues *HandleKeyValuesMacro_Update( KeyValues *pkvUpdate, KeyValues *pkvParent, bool *pbDidUpdate )
{
	const char *pszName = pkvUpdate->GetName();

	if ( V_stricmp( "#update", pszName ) != 0 )
		return nullptr;

	// Have an #update key

	if ( pkvUpdate->GetFirstSubKey() == nullptr )
	{
		// Invalid, has sub keys
		Msg( "Error: #insert on key without subkeys, can only do #update with a key with subkeys\n" );
		return nullptr;
	}

	if ( pkvUpdate->GetDataType() != KeyValues::TYPE_NONE )
	{
		// Invalid, value isn't a TYPE_NONE
		Msg( "Error: #update on key without a data type of NONE, can only do #update with a key with subkeys\n" );
		return nullptr;
	}

	const char *pszGlob = nullptr;

	KeyValues *pkvGlob = pkvUpdate->FindKey( "#glob" );
	if ( !pkvGlob )
	{
		pkvGlob = pkvUpdate->FindKey( "glob" );
	}

	if ( pkvGlob )
	{
		pszGlob = pkvGlob->GetString( nullptr, nullptr );
		pkvUpdate->RemoveSubKey( pkvGlob );
	}

	for ( KeyValues *pkvParentSubKey = pkvParent->GetFirstSubKey(); pkvParentSubKey; pkvParentSubKey = pkvParentSubKey->GetNextKey() )
	{
		if ( pkvParentSubKey == pkvUpdate )
			continue;

		if ( pszGlob && !GlobMatch( pszGlob, pkvParentSubKey->GetName() ) )
			continue;

		UpdateKeyValuesBlock( pkvParentSubKey, pkvUpdate );
	}

	KeyValues *pkvReturn = pkvUpdate->GetNextKey();

	pkvParent->RemoveSubKey( pkvUpdate );
	pkvUpdate->deleteThis();

	if ( pbDidUpdate )
	{
		*pbDidUpdate = true;
	}

	return pkvReturn;
}


//--------------------------------------------------------------------------------------------------
// Main external extry point
//--------------------------------------------------------------------------------------------------
KeyValues *HandleKeyValuesMacros( KeyValues *kv, KeyValues *pkvParent /* = nullptr */ )
{
	KeyValues *pkvNextKey = HandleKeyValuesMacro_Insert( kv, pkvParent );
	if ( pkvNextKey )
	{
		Assert( kv->GetFirstSubKey() == nullptr );

		return pkvNextKey;
	}

	bool bDidLocalUpdate = false;
	pkvNextKey = HandleKeyValuesMacro_Update( kv, pkvParent, &bDidLocalUpdate );
	if ( bDidLocalUpdate )
	{
		Assert( kv->GetFirstSubKey() != nullptr );

		return pkvNextKey;
	}

	KeyValues *pkvSub = kv->GetFirstSubKey();
	while ( pkvSub )
	{
		pkvSub = HandleKeyValuesMacros( pkvSub, kv );
	}

	return kv->GetNextKey();
}
