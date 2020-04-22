//====== Copyright ©, Valve Corporation, All rights reserved. =================
//
// Purpose: Defines the GC interface exposed to the host
//
//=============================================================================


#ifndef GCINTERFACE_H
#define GCINTERFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "gamecoordinator/igamecoordinator.h"
#include "gamecoordinator/igamecoordinatorhost.h"

namespace GCSDK
{
class CGCDirProcess;
class CGCBase;

//-----------------------------------------------------------------------------
// Purpose: Defines the GC interface exposed to the host. This is here to find
//	out from the host which actual GC class to instantiate, and to pass through
//	all other calls
//-----------------------------------------------------------------------------
class CGCInterface : public IGameCoordinator
{
public:
	CGCInterface();
	~CGCInterface();

	//message sources that we tag message tracking with so they can be filtered accordingly
	enum EMsgSource
	{
		eMsgSource_System	= ( 1 << 0 ),
		eMsgSource_Client	= ( 1 << 1 ),
		eMsgSource_GC		= ( 1 << 2 ),
	};

	// Simple accessors
	IGameCoordinator *GetGC();

	AppId_t GetAppID() const;
	const char *GetDebugName() const;
	EUniverse GetUniverse() const;
	bool BIsDevMode() const;
	const char *GetGCDLLPath() const;
	//returns the process ID of the parent process, or 0 if it isn't specified
	HANDLE GetParentProcess() const		{ return m_hParentProcess; }
	
	// Exposed functions from IGameCoordinatorHost
	bool BProcessSystemMessage( uint32 unGCSysMsgType, const void *pubData, uint32 cubData );
	bool BSendMessageToClient( uint64 ullSteamID, uint32 unMsgType, const void *pubData, uint32 cubData );
	bool BSendMessageToGC( int iGCServerIDTarget, uint32 unMsgType, const void *pubData, uint32 cubData );
	void EmitSpew( const char *pchGroupName, SpewType_t spewType, int iSpewLevel, int iLevelLog, const char *pchMsg );
	void AsyncSQLQuery( IGCSQLQuery *pQuery, int eSchemaCatalog );
	void SetStartupComplete( bool bSuccess );
	void SetShutdownComplete();

	// Additional services
	uint64 GenerateGID();
	static uint32 GetGCDirIndexFromGID( GID_t gid );
	bool BSaveConvars();
	void RecordAssert( const char *pchFile, int nLine, const char *pchMessage, bool *pbShouldWriteMinidump );
	CSteamID ConstructSteamIDForClient( AccountID_t unAccountID ) const;
	
	// Implementation of IGameCoordinator
	virtual bool BAsyncInit( uint32 unAppID, const char *pchDebugName, int iGCIndex, IGameCoordinatorHost *pHost )  OVERRIDE;
	virtual bool BMainLoopOncePerFrame( uint64 ulLimitMicroseconds ) OVERRIDE;
	virtual bool BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds ) OVERRIDE;
	virtual bool BAsyncShutdown()  OVERRIDE;
	virtual void Unload()  OVERRIDE;
	virtual void HandleMessageFromClient( uint64 ullSenderID, uint32 unMsgType, void *pubData, uint32 cubData )  OVERRIDE;
	virtual void HandleMessageFromSystem( uint32 unGCSysMsgType, void *pubData, uint32 cubData )  OVERRIDE;
	virtual void HandleMessageFromGC( int iGCServerIDSender, uint32 unMsgType, void *pubData, uint32 cubData )  OVERRIDE;

	// Allows temporary capturing of log spew for ease of generating alerts
	void StartLogCapture();
	void EndLogCapture();
	const CUtlVector<CUtlString> *GetLogCapture();
	void ClearLogCapture();

	//the current version of the GC used for reporting purposes only
	uint32 GetVersion() const					{ return m_nVersion; }
	const char* GetMachineName() const			{ return m_sMachineName; }

	//provides access to the name of the binary for this GC for development builds. This is blank for non-development builds
	const char* GetDevBinaryName() const		{ return m_sDevBinaryName; }

	//access to stats recorded about various asserts triggered in the system
	struct AssertInfo_t
	{
		//the line the assert fired from
		uint32		m_nLine;
		//how many actually fired and recorded
		uint32		m_nTotalFired;
		uint32		m_nTotalRecorded;
		//how many fired since the last clear
		uint32		m_nWindowFired;
		//the string associated with the first firing
		CUtlString	m_sMsg;
		//the times we recorded last
		CUtlVector< RTime32 >	m_vRecordTimes;
	};
	typedef CUtlDict< CUtlVector< AssertInfo_t* >* > AssertInfoDict_t;
	const AssertInfoDict_t& GetAssertInfo() const		{ return m_dictAsserts; }

	//called to reset the assert window, this is useful for tracking how many asserts fired within a specific time range
	void	ClearAssertWindowCounts();

	//called to add a substring that console output will be filtered against. This is only intended for urgent text squelching
	void AddBlockEmitString( const char* pszStr, bool bBlockConsole, bool bBlockLog );
	void ClearBlockEmitStrings();

	//asserts are always rate limited. If you absolutely need to avoid that, you can use this object to force asserts to be recorded regardless of rate limiting
	class CDisableAssertRateLimit
	{
	public:
		CDisableAssertRateLimit()		{ s_nDisabledCount++; }
		~CDisableAssertRateLimit()		{ s_nDisabledCount--; }
		static int32	s_nDisabledCount;
	};

private:

	//the list of substrings we want to filter text based upon
	struct BlockString_t
	{
		CUtlString	m_sStr;
		bool		m_bBlockLog;
		bool		m_bBlockConsole;
	};
	CUtlVector< BlockString_t* >	m_BlockEmitStrings;

	//called to clear all existing assert stats
	void ClearAssertInfo();

	//this will handle loading the config file for the GC and initializing the directory, and will return the config key values
	//so that it can be used to load the convars from
	bool BReadConfigDirectory( KeyValuesAD& configValues );

	//handles loading in the convars from the key values loaded with BReadConfigDirectory, and will setup the convars based upon
	//what GC type this is
	bool BReadConvars( KeyValuesAD& configValues );
	void InitConVars( KeyValues *pkvConvars );

	IGameCoordinatorHost *m_pGCHost;
	CGCBase *m_pGC;
	const CGCDirProcess *m_pGCDirProcess;

	//the version of the GC. This is for reporting purposes only, and will be zero for development builds
	uint32		m_nVersion;

	AppId_t m_nAppID;
	CUtlConstString m_sDebugName;
	EUniverse m_eUniverse;
	bool m_bDevMode;
	CUtlConstString m_sGCDLLPath;

	uint64 m_ullGID;
	HANDLE m_hParentProcess;

	//all the asserts that have fired
	AssertInfoDict_t m_dictAsserts;

	CUtlVector<CUtlString> m_vecLogCapture;
	bool m_bLogCaptureEnabled;

	//the name of the binary, set for development build
	CUtlString		m_sDevBinaryName;

	//the name of the machine we are running on
	CUtlString		m_sMachineName;
};

extern CGCInterface *GGCInterface();

} // namespace GCSDK

#endif // GCINTERFACE_H
