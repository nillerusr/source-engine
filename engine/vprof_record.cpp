//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "tier0/vcrmode.h"
#undef PROTECT_FILEIO_FUNCTIONS
#include "tier0/vprof.h"
#include "utldict.h"
#include "client.h"
#include "cmd.h"
#include "filesystem_engine.h"
#include "vprof_record.h"


#ifdef VPROF_ENABLED

#if defined( _XBOX )

	extern CVProfile *g_pVProfileForDisplay;

#else

	CVProfile *g_pVProfileForDisplay = &g_VProfCurrentProfile;

	// memdbgon must be the last include file in a .cpp file!!!
	#include "tier0/memdbgon.h"

#endif


long GetFileSize( FILE *fp )
{
	int curPos = ftell( fp );
	fseek( fp, 0, SEEK_END );
	long ret = ftell( fp );
	fseek( fp, curPos, SEEK_SET );
	return ret;
}


// ------------------------------------------------------------------------------------------------------------------------------------ //
// VProf record mode. Turn it on to record all the vprof data, then when you're playing back, the engine's budget and vprof panels 
// show the data from the recording instead of the real data.
// ------------------------------------------------------------------------------------------------------------------------------------ //

class CVProfRecorder : public CVProfile
{
public:
	CVProfRecorder()
	{
		m_Mode = Mode_None;
		m_hFile = NULL;
		m_nQueuedStarts = 0;
		m_nQueuedStops = 0;
		m_iPlaybackTick = -1;
	}

	~CVProfRecorder()
	{
		Assert( m_Mode == Mode_None );
	}

	void Shutdown()
	{
		Stop();
	}

	void Stop()
	{
		if ( (m_Mode == Mode_Record || m_Mode == Mode_Playback) && m_hFile != NULL )
		{
			if ( m_Mode == Mode_Record )
				++m_nQueuedStops;

			g_pFileSystem->Close( m_hFile );
		}

		m_Mode = Mode_None;
		m_hFile = NULL;
		g_pVProfileForDisplay = &g_VProfCurrentProfile;	// Stop using us for vprofile displays.
		m_iPlaybackTick = -1;
		m_bNodesChanged = true;
		Term(); // clear the vprof data
	}


	bool IsPlayingBack()
	{
		return m_Mode == Mode_Playback;
	}


// RECORD FUNCTIONS.
public:

	bool Record_Start( const char *pFilename )
	{
		Stop();

		char tempFilename[512];
		if ( !strchr( pFilename, '.' ) )
		{
			Q_snprintf( tempFilename, sizeof( tempFilename ), "%s.vprof", pFilename );
			pFilename = tempFilename;
		}

		m_iLastUniqueNodeID = -1;
		m_hFile = g_pFileSystem->Open( pFilename, "wb" );
		m_Mode = Mode_Record;
		if ( m_hFile == NULL )
		{
			return false;
		}
		else
		{
			// Write the version number.
			int version = VPROF_FILE_VERSION;
			g_pFileSystem->Write( &version, sizeof( version ), m_hFile );

			// Write the root node ID.
			int nodeID = g_VProfCurrentProfile.GetRoot()->GetUniqueNodeID();
			g_pFileSystem->Write( &nodeID, sizeof( nodeID ), m_hFile );

			++m_nQueuedStarts;
			
			// Make sure vprof is recrding.
			Cbuf_AddText( "vprof_on\n" );
			return true;
		}
	}

	void Record_WriteToken( char val )
	{
		g_pFileSystem->Write( &val, sizeof( val ), m_hFile );
	}		

	void Record_MatchTree_R( CVProfNode *pOut, const CVProfNode *pIn, CVProfile *pInProfile )
	{
		// Add any new nodes at the beginning of the list..
		if ( pIn->m_pChild )
		{
			while ( !pOut->m_pChild || pIn->m_pChild->GetUniqueNodeID() != pOut->m_pChild->GetUniqueNodeID() )
			{
				// Find the last new node in the list.
				const CVProfNode *pToAdd = NULL;
				const CVProfNode *pCur = pIn->m_pChild;
				while ( pCur )
				{
					// If the out node has no children then we add the last one in the input node.
					if ( pOut->m_pChild && pCur->GetUniqueNodeID() == pOut->m_pChild->GetUniqueNodeID() )
						break;

					pToAdd = pCur;
					pCur = pCur->m_pSibling;
				}

				Assert( pToAdd );

				// Write this to the file.
				int budgetGroupID = pToAdd->m_BudgetGroupID;
				int parentNodeID = pIn->GetUniqueNodeID();
				int nodeID = pToAdd->GetUniqueNodeID();
				
				Record_WriteToken( Token_AddNode );
				g_pFileSystem->Write( &parentNodeID, sizeof( parentNodeID ), m_hFile );						// Parent node ID.
				g_pFileSystem->Write( pToAdd->m_pszName, strlen( pToAdd->m_pszName ) + 1, m_hFile );	// Name of the new node.
				g_pFileSystem->Write( &budgetGroupID, sizeof( budgetGroupID ), m_hFile );
				g_pFileSystem->Write( &nodeID, sizeof( nodeID ), m_hFile );

				// There's a new one here.
				const char *pBudgetGroupName = g_VProfCurrentProfile.GetBudgetGroupName( pToAdd->m_BudgetGroupID );
				int budgetGroupFlags = g_VProfCurrentProfile.GetBudgetGroupFlags( pToAdd->m_BudgetGroupID );
				CVProfNode *pNewNode = pOut->GetSubNode( pToAdd->m_pszName, 0, pBudgetGroupName, budgetGroupFlags );
				pNewNode->SetBudgetGroupID( pToAdd->m_BudgetGroupID );
				pNewNode->SetUniqueNodeID( pToAdd->GetUniqueNodeID() );
			}
		}
		
		// Recurse.
		CVProfNode *pOutChild = pOut->m_pChild;
		const CVProfNode *pInChild = pIn->m_pChild;
		while ( pOutChild && pInChild )
		{
			Assert( Q_stricmp( pInChild->m_pszName, pOutChild->m_pszName ) == 0 );
			Assert( pInChild->GetUniqueNodeID() == pOutChild->GetUniqueNodeID() );
			Record_MatchTree_R( pOutChild, pInChild, pInProfile );
			
			pOutChild = pOutChild->m_pSibling;
			pInChild = pInChild->m_pSibling;
		}
	}

	void Record_MatchBudgetGroups( CVProfile *pInProfile )
	{
		Assert( GetNumBudgetGroups() <= pInProfile->GetNumBudgetGroups() );

		int nOriginalGroups = GetNumBudgetGroups();
		for ( int i=nOriginalGroups; i < pInProfile->GetNumBudgetGroups(); i++ )
		{
			const char *pName = pInProfile->GetBudgetGroupName( i );
			int flags = pInProfile->GetBudgetGroupFlags( i );
			Record_WriteToken( Token_AddBudgetGroup );
			g_pFileSystem->Write( pName, strlen( pName ) + 1, m_hFile );
			g_pFileSystem->Write( &flags, sizeof( flags ), m_hFile );

			AddBudgetGroupName( pName, flags );
		}
	}

	void Record_WriteTimings_R( const CVProfNode *pIn )
	{
		unsigned short curCalls = min( pIn->m_nCurFrameCalls, (unsigned)0xFFFF );
		if ( curCalls >= 255 )
		{
			unsigned char token = 255;
			g_pFileSystem->Write( &token, sizeof( token ), m_hFile );
			g_pFileSystem->Write( &curCalls, sizeof( curCalls ), m_hFile );
		}
		else
		{
			// Get away with one byte if we can.
			unsigned char token = (char)curCalls;
			g_pFileSystem->Write( &token, sizeof( token ), m_hFile );
		}

		// This allows us to write 2 bytes unless it's > 256 milliseconds (unlikely).
		unsigned long nMicroseconds = pIn->m_CurFrameTime.GetMicroseconds() / 4; 
		if ( nMicroseconds >= 0xFFFF )
		{
			unsigned short token = 0xFFFF;
			g_pFileSystem->Write( &token, sizeof( token ), m_hFile );
			g_pFileSystem->Write( &nMicroseconds, sizeof( nMicroseconds ), m_hFile );
		}
		else
		{
			unsigned short token = (unsigned short)nMicroseconds;
			g_pFileSystem->Write( &token, sizeof( token ), m_hFile );
		}

		for ( const CVProfNode *pChild = pIn->m_pChild; pChild; pChild = pChild->m_pSibling )
			Record_WriteTimings_R( pChild );
	}

	void Record_Snapshot()
	{
		CVProfile *pInProfile = &g_VProfCurrentProfile;

		// Don't record the overhead of writing in the filesystem here.
		pInProfile->Pause();

		// Record the tick count and start of frame.
		Record_WriteToken( Token_StartFrame );
#ifdef SWDS
		g_pFileSystem->Write( &host_tickcount, sizeof( host_tickcount ), m_hFile );		
#else
		g_pFileSystem->Write( &g_ClientGlobalVariables.tickcount, sizeof( g_ClientGlobalVariables.tickcount ), m_hFile );
#endif
		
		// Record all the changes to get our tree and budget groups to g_VProfCurrentProfile.
		Record_MatchBudgetGroups( pInProfile );
		if ( m_iLastUniqueNodeID != CVProfNode::s_iCurrentUniqueNodeID )
		{
			Record_MatchTree_R( GetRoot(), pInProfile->GetRoot(), pInProfile );
		}
		
		// Now that we have a matching tree, write all the timings.
		Record_WriteToken( Token_Timings );
		Record_WriteTimings_R( pInProfile->GetRoot() );
		Record_WriteToken( Token_EndOfFrame );

		pInProfile->Resume();
	}

	
// PLAYBACK FUNCTIONS.
public:

	#define Playback_Assert( theTest ) Playback_AssertFn( !!(theTest), __LINE__ )
	bool Playback_AssertFn( bool bTest, int iLine )
	{
		if ( bTest )
		{
			return true;
		}
		else
		{
			Stop();
			Warning( "VPROF PLAYBACK ASSERT (%s, line %d) - stopping playback.\n", __FILE__, iLine );
			return false;
		}
	}


	bool Playback_Start( const char *pFilename )
	{
		Stop();

		char tempFilename[512];
		if ( !strchr( pFilename, '.' ) )
		{
			Q_snprintf( tempFilename, sizeof( tempFilename ), "%s.vprof", pFilename );
			pFilename = tempFilename;
		}

		m_iLastUniqueNodeID = -1;
		m_hFile = g_pFileSystem->Open( pFilename, "rb" );
		m_Mode = Mode_Playback;
		m_bPlaybackPaused = true;
		if ( m_hFile == NULL )
		{
			Warning( "vprof_playback_start: Open( %s ) failed.\n", pFilename );
			return false;
		}
		else
		{
			int version;
			g_pFileSystem->Read( &version, sizeof( version ), m_hFile );
			if ( !Playback_Assert( version == VPROF_FILE_VERSION ) )
				return false;

			// Read the root node ID.
			int nodeID;
			g_pFileSystem->Read( &nodeID, sizeof( nodeID ), m_hFile );
			GetRoot()->SetUniqueNodeID( nodeID );

			m_iSkipPastHeaderPos = g_pFileSystem->Tell( m_hFile );
			m_iLastTick = -1;	// We don't know the last tick in the file yet.
			m_FileLen = g_pFileSystem->Size( m_hFile );

			m_enabled = true; // So IsEnabled() returns true..
			Playback_ReadTick();
			g_pVProfileForDisplay = this;	// Start using this CVProfile for displays.
			return true;
		}
	}

	void Playback_Restart()
	{
		if ( m_Mode != Mode_Playback )
		{
			Assert( false );
			return;
		}
		
		// Clear the data and restart playback.
		m_iPlaybackTick = -1;
		Term(); // clear the vprof data
		m_bNodesChanged = true;

		g_pFileSystem->Seek( m_hFile, m_iSkipPastHeaderPos, FILESYSTEM_SEEK_HEAD );
		Playback_ReadTick();	// Read in one tick's worth of data.
	}

	char Playback_ReadToken()
	{
		Assert( m_Mode == Mode_Playback );
		char token;
		if ( g_pFileSystem->Read( &token, 1, m_hFile ) != 1 )
			token = TOKEN_FILE_FINISHED;
		
		return token;
	}

	
	bool Playback_ReadString( char *pOut, int maxLen )
	{
		int i = 0;
		while ( 1 )
		{
			char ch;
			if ( g_pFileSystem->Read( &ch, 1, m_hFile ) == 0 )
			{
				Playback_Assert( false );
				return false;
			}
			if ( ch == 0 )
			{
				pOut[i] = 0;
				break;
			}
			else
			{
				if ( i < (maxLen-1) )
				{
					pOut[i] = ch;
					++i;
				}
			}
		}
		return true;
	}


	bool Playback_ReadAddBudgetGroup()
	{
		char name[512];
		if ( !Playback_ReadString( name, sizeof( name ) ) )
			return false;

		int flags = 0;
		g_pFileSystem->Read( &flags, sizeof( flags ), m_hFile );

		AddBudgetGroupName( name, flags );
		return true;
	}


	CVProfNode* FindVProfNodeByID_R( CVProfNode *pNode, int id )
	{
		if ( pNode->GetUniqueNodeID() == id )
			return pNode;
		
		for ( CVProfNode *pCur=pNode->m_pChild; pCur; pCur=pCur->m_pSibling )
		{
			CVProfNode *pTest = FindVProfNodeByID_R( pCur, id );
			if ( pTest )
				return pTest;
		}
		
		return NULL;
	}


	bool Playback_ReadAddNode()
	{
		int budgetGroupID;
		int parentNodeID;
		int nodeID;
		
		char nodeName[512];

		g_pFileSystem->Read( &parentNodeID, sizeof( parentNodeID ), m_hFile );					// Parent node ID.
		if ( !Playback_ReadString( nodeName, sizeof( nodeName ) ) )
			return false;
		g_pFileSystem->Read( &budgetGroupID, sizeof( budgetGroupID ), m_hFile );
		g_pFileSystem->Read( &nodeID, sizeof( nodeID ), m_hFile );

		// Now find the parent node.
		CVProfNode *pParentNode = FindVProfNodeByID_R( GetRoot(), parentNodeID );
		if ( !Playback_Assert( pParentNode != NULL ) )
			return false;

		const char *pBudgetGroupName = GetBudgetGroupName( 0 );
		int budgetGroupFlags = 0;
		CVProfNode *pNewNode = pParentNode->GetSubNode( PoolString( nodeName ), 0, pBudgetGroupName, budgetGroupFlags );
		pNewNode->SetBudgetGroupID( budgetGroupID );
		pNewNode->SetUniqueNodeID( nodeID );

		m_bNodesChanged = true;
		return true;
	}


	bool Playback_ReadTimings_R( CVProfNode *pNode )
	{
		// Read the timing.
		unsigned char token;
		if ( g_pFileSystem->Read( &token, sizeof( token ), m_hFile ) != sizeof( token ) )
			return false;

		if ( token == 255 )
		{
			unsigned short curCalls;
			if ( g_pFileSystem->Read( &curCalls, sizeof( curCalls ), m_hFile ) != sizeof( curCalls ) )
				return false;

			pNode->m_nCurFrameCalls = curCalls;
		}
		else
		{
			pNode->m_nCurFrameCalls = token;
		}
		pNode->m_nPrevFrameCalls = pNode->m_nCurFrameCalls;

		// This allows us to write 2 bytes unless it's > 256 milliseconds (unlikely).
		unsigned short microsecondsToken;
		if ( g_pFileSystem->Read( &microsecondsToken, sizeof( microsecondsToken ), m_hFile ) != sizeof( microsecondsToken ) )
			return false;

		if ( microsecondsToken == 0xFFFF )
		{
			unsigned long nMicroseconds;
			if ( g_pFileSystem->Read( &nMicroseconds, sizeof( nMicroseconds ), m_hFile ) != sizeof( nMicroseconds ) )
				return false;

			pNode->m_CurFrameTime.SetMicroseconds( nMicroseconds * 4 );
		}
		else
		{
			pNode->m_CurFrameTime.SetMicroseconds( (unsigned long)microsecondsToken * 4 );
		}
		pNode->m_PrevFrameTime = pNode->m_CurFrameTime;

		// Recurse.
		for ( CVProfNode *pCur=pNode->m_pChild; pCur; pCur=pCur->m_pSibling )
		{
			if ( !Playback_ReadTimings_R( pCur ) )
				return false;
		}

		return true;
	}


	// Read the next tick. If iDontGoPast is set, then it will abort IF the next tick's index
	// is greater than iDontGoPast. In that case, sets pWouldHaveGonePast to true, 
	// stays where it was before the call, and returns true.
	bool Playback_ReadTick( int iDontGoPast = -1, bool *pWouldHaveGonePast = NULL )
	{
		if ( pWouldHaveGonePast )
			*pWouldHaveGonePast = false;

		if ( m_Mode != Mode_Playback )
			return false;

		// Read the next tick..
		int token = Playback_ReadToken();
		if ( token == TOKEN_FILE_FINISHED )
		{
			Msg( "VPROF playback finished.\n" );
			m_iLastTick = m_iPlaybackTick;	// Now we know our last tick.
			return true;
		}
			
		if ( !Playback_Assert( token == Token_StartFrame ) )
			return false;

		int iPlaybackTick = m_iPlaybackTick;
		g_pFileSystem->Read( &iPlaybackTick, sizeof( iPlaybackTick ), m_hFile );
		
		// First test if this tick would go past the number they don't want us to go past.
		if ( iDontGoPast != -1 && iPlaybackTick > iDontGoPast )
		{
			*pWouldHaveGonePast = true;
			g_pFileSystem->Seek( m_hFile, -5, FILESYSTEM_SEEK_CURRENT );
			return true;
		}
		else
		{
			m_iPlaybackTick = iPlaybackTick;
		}

		while ( 1 )
		{
			token = Playback_ReadToken();
			if ( token == Token_EndOfFrame )
				break;

			if ( token == Token_AddBudgetGroup )
			{
				if ( !Playback_ReadAddBudgetGroup() )
					return false;
			}
			else if ( token == Token_AddNode )
			{
				if ( !Playback_ReadAddNode() )
					return false;
			}
			else if ( token == Token_Timings )
			{
				if ( !Playback_ReadTimings_R( GetRoot() ) )
					return false;
			}
			else
			{
				Playback_Assert( false );
				return false;
			}
		}

		return true;
	}

	void Playback_Snapshot()
	{
		if ( m_Mode == Mode_Playback && !m_bPlaybackPaused )
			Playback_ReadTick();
	}

	
	void Playback_Step()
	{
		Playback_ReadTick();
	}


	class CNodeAverage
	{
	public:
		CVProfNode *m_pNode;
		
		CCycleCount m_CurFrameTime_Total;
		int m_nCurFrameCalls_Total;
		
		int m_nSamples;
	};

	CNodeAverage* FindNodeAverage( CUtlVector<CNodeAverage> &averages, CVProfNode *pNode )
	{
		for ( int i=0; i < averages.Count(); i++ )
		{
			if ( averages[i].m_pNode == pNode )
				return &averages[i];
		}
		return NULL;
	}

	void UpdateAverages_R( CUtlVector<CNodeAverage> &averages, CVProfNode *pNode )
	{
		CNodeAverage *pAverage = FindNodeAverage( averages, pNode );
		if ( !pAverage )
		{
			pAverage = &averages[ averages.AddToTail() ];
			memset( pAverage, 0, sizeof( *pAverage ) );
			pAverage->m_pNode = pNode;
		}
		pAverage->m_CurFrameTime_Total += pNode->m_CurFrameTime;
		pAverage->m_nCurFrameCalls_Total += pNode->m_nCurFrameCalls;
		pAverage->m_nSamples++;
		
		// Recurse.
		for ( CVProfNode *pCur=pNode->m_pChild; pCur; pCur=pCur->m_pSibling )
			UpdateAverages_R( averages, pCur );
	}

	void DumpAverages_R( CUtlVector<CNodeAverage> &averages, CVProfNode *pNode )
	{
		CNodeAverage *pAverage = FindNodeAverage( averages, pNode );
		if ( pAverage )
		{
			pNode->m_CurFrameTime.m_Int64 = pAverage->m_CurFrameTime_Total.m_Int64 / pAverage->m_nSamples;
			pNode->m_nCurFrameCalls = pAverage->m_nCurFrameCalls_Total / pAverage->m_nSamples;
		}
		pNode->m_PrevFrameTime = pNode->m_CurFrameTime;
		pNode->m_nPrevFrameCalls = pNode->m_nCurFrameCalls;

		// Recurse.
		for ( CVProfNode *pCur=pNode->m_pChild; pCur; pCur=pCur->m_pSibling )
			DumpAverages_R( averages, pCur );
	}			


	void Playback_Average( int nFrames )
	{
		// Remember where we started.
		unsigned long seekPos = g_pFileSystem->Tell( m_hFile );
		int iOldLastTick = m_iLastTick;
		int iOldPlaybackTick = m_iPlaybackTick;
		
		// Take the average of the next N ticks.
		CUtlVector<CNodeAverage> averages;
		while ( nFrames > 0 && m_iLastTick == -1 )
		{
			Playback_ReadTick();
			UpdateAverages_R( averages, GetRoot() );
			--nFrames;
		}
		DumpAverages_R( averages, GetRoot() );
		
		// Now seek back to where we started.
		g_pFileSystem->Seek( m_hFile, seekPos, FILESYSTEM_SEEK_HEAD );
		m_iLastTick = iOldLastTick;
		m_iPlaybackTick = iOldPlaybackTick;
	}

	
	int Playback_SetPlaybackTick( int iTick )
	{
		if ( m_Mode != Mode_Playback )
			return 0;

		m_bNodesChanged = false;	// We want to pickup changes to this, so reset it here.
		if ( iTick == m_iPlaybackTick )
		{
			return 1;
		}
		else if ( iTick < m_iPlaybackTick )
		{
			// Crap.. have to go back. Restart and seek to this tick.
			Playback_Restart();
			
			// If this tick has a smaller value than the first tick in the file, then we can't seek forward to it...
			if ( iTick <= m_iPlaybackTick )
			{
				return 1 + m_bNodesChanged;	// return 2 if the nodes changed
			}
		}

		// Now seek forward to the tick they want.
		while ( m_iPlaybackTick < iTick )
		{
			bool bWouldHaveGonePast;
			if ( !Playback_ReadTick( iTick, &bWouldHaveGonePast ) )
				return 0;	// error

			// If reading this tick would have gone past the tick they're asking us to go for,
			// stay on the current tick.
			if ( bWouldHaveGonePast )
				break;
			
			// If we went to the last tick in the file, then stop here.
			if ( m_iLastTick != -1 && m_iPlaybackTick >= m_iLastTick )
				return 1 + m_bNodesChanged;
		}

		return 1 + m_bNodesChanged;
	}


	// 0-1 value.
	float Playback_GetCurrentPercent()
	{
		return (float)g_pFileSystem->Tell( m_hFile ) / m_FileLen;
	}


	int Playback_SeekToPercent( float flWantedPercent )
	{
		if ( m_Mode != Mode_Playback )
			return 0;	// error

		m_bNodesChanged = false;	// We want to pickup changes to this, so reset it here.

		float flCurPercent = Playback_GetCurrentPercent();
		if ( flWantedPercent < flCurPercent )
		{
			// Crap.. have to go back. Restart and seek to this tick.
			Playback_Restart();
			
			// If this tick has a smaller value than the first tick in the file, then we can't seek forward to it...
			if ( flWantedPercent <= 0 )
				return 1 + m_bNodesChanged;	// return 2 if nodes changed
		}

		// Now seek forward to the tick they want.
		while ( Playback_GetCurrentPercent() < flWantedPercent )
		{
			if ( !Playback_ReadTick() )
				return 0;	// error
			
			// If we went to the last tick in the file, then stop here.
			if ( m_iLastTick != -1 && m_iPlaybackTick >= m_iLastTick )
				return 1 + m_bNodesChanged; // return 2 if nodes changed
		}

		return 1 + m_bNodesChanged;
	}


	int Playback_GetCurrentTick()
	{
		return m_iPlaybackTick;
	}



// OTHER FUNCTIONS.
public:

	void Snapshot()
	{
		if ( m_Mode == Mode_Record )
			Record_Snapshot();
		else if ( m_Mode == Mode_Playback )
			Playback_Snapshot();
	}

	void StartOrStop()
	{
		while ( m_nQueuedStarts > 0 )
		{
			--m_nQueuedStarts;
			g_VProfCurrentProfile.Start();
		}

		while ( m_nQueuedStops > 0 )
		{
			--m_nQueuedStops;
			g_VProfCurrentProfile.Stop();
		}
	}

	inline CVProfile* GetActiveProfile()
	{
		if ( m_Mode == Mode_Playback )
			return this;
		else
			return &g_VProfCurrentProfile;
	}


private:

	const char* PoolString( const char *pStr )
	{
		int i = m_PooledStrings.Find( pStr );
		if ( i == m_PooledStrings.InvalidIndex() )
			i = m_PooledStrings.Insert( pStr, 0 );

		return m_PooledStrings.GetElementName( i );
	}			
			

private:
	enum
	{
		Token_StartFrame=0,
		Token_AddNode,
		Token_AddBudgetGroup,
		Token_Timings,
		Token_EndOfFrame,
		TOKEN_FILE_FINISHED
	};

	enum
	{
		VPROF_FILE_VERSION = 1
	};
	
	enum
	{
		Mode_None,
		Mode_Record,
		Mode_Playback
	};

	CUtlDict<int,int> m_PooledStrings;

	int m_Mode;
	FileHandle_t m_hFile;
	int m_iLastUniqueNodeID;
	int m_iPlaybackTick;		// Our current tick.
	int m_iSkipPastHeaderPos;
	int m_iLastTick;			// We only know this when we hit the end of the file.
	int m_FileLen;
	bool m_bNodesChanged;		// Set if the nodes were added or removed.

	int m_nQueuedStarts;
	int m_nQueuedStops;

	bool m_bPlaybackPaused;
};



static CVProfRecorder g_VProfRecorder;




CON_COMMAND( vprof_record_start, "Start recording vprof data for playback later." )
{
	if ( args.ArgC() != 2 )
	{
		Warning( "vprof_record_start requires a filename\n" );
		return;
	}

	g_VProfRecorder.Record_Start( args[1] );
}

CON_COMMAND( vprof_record_stop, "Stop recording vprof data" )
{
	Warning( "Stopping vprof recording...\n" );
	g_VProfRecorder.Stop();
}

CON_COMMAND( vprof_playback_start, "Start playing back a recorded .vprof file." )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "vprof_playback_start requires a filename\n" );
		return;
	}

	// Console parser treats colons as a break, so join all the tokens together here.
	char fullFilename[512];
	fullFilename[0] = 0;
	for ( int i=1; i < args.ArgC(); i++ )
	{
		Q_strncat( fullFilename, args[i], sizeof( fullFilename ), COPY_ALL_CHARACTERS );
	}

	g_VProfRecorder.Playback_Start( fullFilename );
}

CON_COMMAND( vprof_playback_stop, "Stop playing back a recorded .vprof file." )
{
	Warning( "Stopping vprof playback...\n" );
	g_VProfRecorder.Stop();
}

CON_COMMAND( vprof_playback_step, "While playing back a .vprof file, step to the next tick." )
{
	VProfPlayback_Step();
}

CON_COMMAND( vprof_playback_stepback, "While playing back a .vprof file, step to the previous tick." )
{
	VProfPlayback_StepBack();
}

CON_COMMAND( vprof_playback_average, "Average the next N frames." )
{
	if ( args.ArgC() >= 2 )
	{
		int nFrames = atoi( args[ 1 ] );
		if ( nFrames == -1 )
			nFrames = 9999999;
			
		g_VProfRecorder.Playback_Average( nFrames );
	}
	else
	{
		Warning( "vprof_playback_average [# frames]\n" );
		Warning( "If # frames is -1, then it will average all the remaining frames in the vprof file.\n" );
	}	
}


void VProfRecord_Snapshot()
{
	g_VProfRecorder.Snapshot();
}


void VProfRecord_StartOrStop()
{
	g_VProfRecorder.StartOrStop();
}


void VProfRecord_Shutdown()
{
	g_VProfRecorder.Shutdown();
}



bool VProfRecord_IsPlayingBack()
{
	return g_VProfRecorder.IsPlayingBack();
}


int VProfPlayback_GetCurrentTick()
{
	return g_VProfRecorder.Playback_GetCurrentTick();
}


float VProfPlayback_GetCurrentPercent()
{
	return g_VProfRecorder.Playback_GetCurrentPercent();
}


int VProfPlayback_SetPlaybackTick( int iTick )
{
	return g_VProfRecorder.Playback_SetPlaybackTick( iTick );
}


int VProfPlayback_SeekToPercent( float percent )
{
	return g_VProfRecorder.Playback_SeekToPercent( percent );
}

void VProfPlayback_Step()
{
	g_VProfRecorder.Playback_Step();
}

int VProfPlayback_StepBack()
{
	return g_VProfRecorder.Playback_SetPlaybackTick( g_VProfRecorder.Playback_GetCurrentTick() - 1 );
}


#endif // VPROF_ENABLED
