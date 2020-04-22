//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PURE_SERVER_H
#define PURE_SERVER_H
#ifdef _WIN32
#pragma once
#endif


#include "ifilelist.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "userid.h"

class KeyValues;

typedef CUtlVector<uint8> PureServerPublicKey_t;

bool operator==( const PureServerPublicKey_t &a, const PureServerPublicKey_t &b );

class CPureServerWhitelist : public IPureServerWhitelist
{
public:

	virtual void AddRef() OVERRIDE;
	virtual void Release() OVERRIDE;
	virtual EPureServerFileClass GetFileClass( const char *pszFilename ) OVERRIDE;
	virtual int GetTrustedKeyCount() const OVERRIDE;
	virtual const byte *GetTrustedKey( int iKeyIndex, int *nKeySize ) const OVERRIDE;

	static CPureServerWhitelist* Create( IFileSystem *pFileSystem );

	// Load up whitelist from config files
	void Load( int iPureMode );

	// Load up any entries in the KV file.
	bool				LoadCommandsFromKeyValues( KeyValues *kv );

	// Load list of trusted keys from specified file
	bool				LoadTrustedKeysFromKeyValues( KeyValues *kv );

	// Encode for networking.
	void				Encode( CUtlBuffer &buf );
	void				Decode( CUtlBuffer &buf );

	// Calls IFileSystem::CacheFileCRCs for all files and directories marked check_crc.
	void				CacheFileCRCs();

	// Used by sv_pure command when you're connected to a server.
	void				PrintWhitelistContents();

	// Compare for equivalence
	bool operator==( const CPureServerWhitelist &x ) const;

private:

	CPureServerWhitelist();
	~CPureServerWhitelist();

	void				Init( IFileSystem *pFileSystem );
	void				Term();

	class CCommand
	{
	public:
		CCommand();
		~CCommand();

		EPureServerFileClass m_eFileClass;
		unsigned short	m_LoadOrder;	// What order this thing was specified in the whitelist file?  Used to resolve rule conflicts.
	};

	void				UpdateCommandStats( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int *pHighest, int *pLongestPathName );
	void				PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand );
	int					FindCommandByLoadOrder( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int iLoadOrder );

	void InternalCacheFileCRCs( CUtlDict<CCommand*,int> &theList, ECacheCRCType eType );

	CCommand* GetBestEntry( const char *pFilename );

	void EncodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf );
	void DecodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf, uint32 nFormatVersion );

	void AddHardcodedFileCommands();
	void AddFileCommand( const char *pszFilePath, EPureServerFileClass eFileClass, unsigned short nLoadOrder );

	CPureServerWhitelist::CCommand* CheckEntry( 
		CUtlDict<CPureServerWhitelist::CCommand*,int> &dict, 
		const char *pEntryName, 
		CPureServerWhitelist::CCommand *pBestEntry );

	unsigned short	m_LoadCounter;	// Incremented as we load things so their m_LoadOrder increases.
	volatile long int m_RefCount;

	// Commands are applied to files in order.
	CUtlDict<CCommand*,int>	m_FileCommands;				// file commands
	CUtlDict<CCommand*,int> m_RecursiveDirCommands;		// ... commands
	CUtlDict<CCommand*,int> m_NonRecursiveDirCommands;	// *.* commands
	IFileSystem *m_pFileSystem;

	enum { kLoadOrder_HardcodedOverride = 0xffff };

	static bool CommandDictDifferent( const CUtlDict<CCommand*,int> &a, const CUtlDict<CCommand*,int> &b );

	// List of trusted keys
	CUtlVector< PureServerPublicKey_t > m_vecTrustedKeys;
};


CPureServerWhitelist* CreatePureServerWhitelist( IFileSystem *pFileSystem );

struct UserReportedFileHash_t
{
	int m_idxFile;
	USERID_t m_userID;
	FileHash_t m_FileHash;

	static bool Less( const UserReportedFileHash_t& lhs, const UserReportedFileHash_t& rhs )
	{
		if ( lhs.m_idxFile < rhs.m_idxFile )
			return true;
		if ( lhs.m_idxFile > rhs.m_idxFile )
			return false;
		if ( lhs.m_userID.steamid < rhs.m_userID.steamid )
			return true;
		return false;
	}
};

struct UserReportedFile_t
{
	CRC32_t m_crcIdentifier;
	CUtlString m_filename;
	CUtlString m_path;
	int m_nFileFraction;

	static bool Less( const UserReportedFile_t& lhs, const UserReportedFile_t& rhs )
	{
		if ( lhs.m_crcIdentifier < rhs.m_crcIdentifier )
			return true;
		if ( lhs.m_crcIdentifier > rhs.m_crcIdentifier )
			return false;
		if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
			return true;
		if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
			return false;
		int nCmp = Q_strcmp( lhs.m_filename.String(), rhs.m_filename.String() );
		if ( nCmp < 0 )
			return true;
		if ( nCmp > 0 )
			return false;
		nCmp = Q_strcmp( lhs.m_path.String(), rhs.m_path.String() );
		if ( nCmp < 0 )
			return true;
		return false;
	}
};

struct MasterFileHash_t
{
	int m_idxFile;
	int m_cMatches;
	FileHash_t m_FileHash;

	static bool Less( const MasterFileHash_t& lhs, const MasterFileHash_t& rhs )
	{
		return lhs.m_idxFile < rhs.m_idxFile;
	}

};

class CPureFileTracker
{
public:
	CPureFileTracker():
	  m_treeAllReportedFiles( UserReportedFile_t::Less ),
		  m_treeMasterFileHashes( MasterFileHash_t::Less ),
		  m_treeUserReportedFileHash( UserReportedFileHash_t::Less )
	  {
		  m_flLastFileReceivedTime = 0.f;
		  m_cMatchedFile = 0;
		  m_cMatchedMasterFile = 0;
		  m_cMatchedMasterFileHash = 0;
		  m_cMatchedFileFullHash = 0;
	  }

	  void AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord );
	  bool DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID );
	  int ListUserFiles( bool bListAll, const char *pchFilenameFind );
	  int ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax );

	  CUtlRBTree< UserReportedFile_t, int > m_treeAllReportedFiles;
	  CUtlRBTree< MasterFileHash_t, int > m_treeMasterFileHashes;
	  CUtlRBTree< UserReportedFileHash_t, int > m_treeUserReportedFileHash;

	  float m_flLastFileReceivedTime;
	  int m_cMatchedFile;
	  int m_cMatchedMasterFile;
	  int m_cMatchedMasterFileHash;
	  int m_cMatchedFileFullHash;

};

extern CPureFileTracker g_PureFileTracker;


#endif // PURE_SERVER_H

