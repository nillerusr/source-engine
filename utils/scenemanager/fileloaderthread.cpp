//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "sentence.h"
#include "wavefile.h"
#include "tier2/riff.h"
#include "filesystem.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include "IFileLoader.h"

bool SceneManager_LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io );

//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class ThreadIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		char filename[ 512 ];
		Q_snprintf( filename, sizeof( filename ), "%s%s", SceneManager_GetGameDirectory(), pFileName );
		return (int)_open( filename, _O_BINARY | _O_RDONLY );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( file == -1 )
			return 0;

		return _read( file, pOutput, size );
	}

	void seek( int file, int pos )
	{
		if ( file == -1 )
			return;

		_lseek( file, pos, SEEK_SET );
	}

	unsigned int tell( int file )
	{
		if ( file == -1 )
			return 0;

		return _tell( file );
	}

	unsigned int size( int file )
	{
		if ( file == -1 )
			return 0;

		long curpos = tell( file );
		_lseek( file, 0, SEEK_END );
		int s = tell( file );
		_lseek( file, curpos, SEEK_SET );

		return s;
	}

	void close( int file )
	{
		if ( file == -1 )
			return;

		_close( file );
	}
};

//-----------------------------------------------------------------------------
// Purpose: All wavefile I/O occurs on a thread
//-----------------------------------------------------------------------------
class CFileLoaderThread : public IFileLoader
{
public:
	struct SentenceRequest
	{
		SentenceRequest()
		{
			filename[ 0 ] = 0;
			sentence.Reset();
			wavefile = NULL;
			valid	= false;
		}

		bool		valid;
		char		filename[ 256 ];
		CSentence	sentence;

		CWaveFile	*wavefile;
	};

	// Construction
							CFileLoaderThread( void );
	virtual 				~CFileLoaderThread( void );

	// Sockets add/remove themselves via their constructor
	virtual void			AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles );

	// Lock changes to wavefile list, etc.
	virtual void			Lock( void );
	// Unlock wavefile list, etc.
	virtual void			Unlock( void );

	// Retrieve handle to shutdown event
	virtual HANDLE			GetShutdownHandle( void );

	// Caller should call lock before accessing any of these methods and unlock afterwards!!!
	virtual	int				ProcessCompleted();

	int						DoThreadWork();

	virtual	void			Start();

private:
	// Critical section used for synchronizing access to wavefile list
	CRITICAL_SECTION		cs;
	// List of wavefiles we are listening on
	CUtlVector< SentenceRequest	* > m_FileList;

	CUtlVector< SentenceRequest * > m_Pending;
	CUtlVector< SentenceRequest	* > m_Completed;
	// Thread handle
	HANDLE					m_hThread;
	// Thread id
	DWORD					m_nThreadId;
	// Event to set when we want to tell the thread to shut itself down
	HANDLE					m_hShutdown;

	ThreadIOReadBinary		m_ThreadIO;
	bool					m_bLocked;

	int						m_nTotalAdds;
	int						m_nTotalPending;
	int						m_nTotalProcessed;
	int						m_nTotalCompleted;
};

// Singleton handler
static CFileLoaderThread g_WaveLoader;
extern IFileLoader *fileloader = &g_WaveLoader;

int CFileLoaderThread::DoThreadWork()
{
	int i;
	// Check for shutdown event
	if ( WAIT_OBJECT_0 == WaitForSingleObject( GetShutdownHandle(), 0 ) )
	{
		return 0;
	}

	// No changes to list right now
	Lock();
	// Move new items to work list
	int newItems = m_FileList.Count();
	for ( i = 0; i < newItems; i++ )
	{
		// Move to pending and issue async i/o calls
		m_Pending.AddToTail( m_FileList[ i ] );

		m_nTotalPending++;
	}
	m_FileList.RemoveAll();
	// Done adding new work items
	Unlock();

	int remaining = m_Pending.Count();
	if ( !remaining )
		return 1;

	int workitems = remaining; // min( remaining, 1000 );

	CUtlVector< SentenceRequest * > transfer;

	for ( i = 0; i < workitems; i++ )
	{
		SentenceRequest *r = m_Pending[ 0 ];
		m_Pending.Remove( 0 );

		transfer.AddToTail( r );
		// Do the work
		
		m_nTotalProcessed++;

		r->valid = SceneManager_LoadSentenceFromWavFileUsingIO( r->filename, r->sentence, m_ThreadIO );
	}

	// Now move to completed list
	Lock();
	for ( i = 0; i < workitems; i++ )
	{
		SentenceRequest *r = transfer[ i ];
		if ( r->valid )
		{
			m_nTotalCompleted++;

			m_Completed.AddToTail( r );
		}
		else
		{
			delete r;
		}
	}
	Unlock();
	return 1;
}

int CFileLoaderThread::ProcessCompleted()
{
	Lock();
	int c = m_Completed.Count();
	for ( int i = c - 1; i >= 0 ; i-- )
	{
		SentenceRequest *r = m_Completed[ i ];

		r->wavefile->SetThreadLoadedSentence( r->sentence );

		delete r;
	}
	m_Completed.RemoveAll();
	Unlock();
	return c;
}


//-----------------------------------------------------------------------------
// Purpose: Main winsock processing thread
// Input  : threadobject - 
// Output : static DWORD WINAPI
//-----------------------------------------------------------------------------
static DWORD WINAPI FileLoaderThreadFunc( LPVOID threadobject )
{
	// Get pointer to CFileLoaderThread object
	CFileLoaderThread *wavefilethread = ( CFileLoaderThread * )threadobject;
	Assert( wavefilethread );
	if ( !wavefilethread )
	{
		return 0;
	}

	// Keep looking for data until shutdown event is triggered
	while ( 1 )
	{
		if( !wavefilethread->DoThreadWork() )
			break;

		// Yield a small bit of time to main app
		Sleep( 100 );
	}

	ExitThread( 0 );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CFileLoaderThread::CFileLoaderThread( void )
{
	m_nTotalAdds = 0;
	m_nTotalProcessed = 0;
	m_nTotalCompleted = 0;
	m_nTotalPending = 0;

	m_bLocked = false;

	InitializeCriticalSection( &cs );

	m_hShutdown	= CreateEvent( NULL, TRUE, FALSE, NULL );
	Assert( m_hShutdown );

	m_hThread = NULL;
	Start();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileLoaderThread::Start()
{
	m_hThread = CreateThread( NULL, 0, FileLoaderThreadFunc, (void *)this, 0, &m_nThreadId );
	Assert( m_hThread );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileLoaderThread::~CFileLoaderThread( void )
{
	Lock();
	{
		SetEvent( m_hShutdown );
		Sleep( 2 );
		TerminateThread( m_hThread, 0 );
	}
	Unlock();

	// Kill the wavefile
//!! need to validate this line
//	Assert( !m_FileList );

	CloseHandle( m_hThread );

	CloseHandle( m_hShutdown );

	DeleteCriticalSection( &cs );
}

//-----------------------------------------------------------------------------
// Purpose: Returns handle of shutdown event
// Output : HANDLE
//-----------------------------------------------------------------------------
HANDLE CFileLoaderThread::GetShutdownHandle( void )
{
	return m_hShutdown;
}

//-----------------------------------------------------------------------------
// Purpose: Locks object and adds wavefile to thread
// Input  : *wavefile - 
//-----------------------------------------------------------------------------
void CFileLoaderThread::AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles )
{
	Lock();

	int c = wavefiles.Count();
	for ( int i = 0; i < c; i++ )
	{
		SentenceRequest *request = new SentenceRequest;
		request->wavefile = wavefiles[ i ];
		Q_strncpy( request->filename, request->wavefile->GetFileName(), sizeof( request->filename ) );
	

		m_FileList.AddToTail( request );

		m_nTotalAdds++;
	}

	Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileLoaderThread::Lock( void )
{
	EnterCriticalSection( &cs );
	Assert( !m_bLocked );
	m_bLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileLoaderThread::Unlock( void )
{
	Assert( m_bLocked );
	m_bLocked = false;
	LeaveCriticalSection( &cs );
}