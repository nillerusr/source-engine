//========= Copyright Valve Corporation, All rights reserved. ============//
// vp4mutex.cpp : Defines the entry point for the console application.
//

#define Error DbgError

#include "tier0/platform.h"

#include <stdio.h>
#include <conio.h>
#include <vector> // SEE NOTES BELOW ABOUT REVERTING THIS IF REQUIRED

#undef Error
#undef Verify

#include "clientapi.h"

#include <time.h>
#include <ctype.h>
#include <windows.h>

#undef SetPort

#define RemoveAll clear
#define AddToTail push_back
#define Count	  size

#define CLIENTSPEC_BUFFER_SIZE		(8 * 1024)

//-----------------------------------------------------------------------------
// internal
//-----------------------------------------------------------------------------
ClientApi client;
ClientUser user;

//
// NOTE: All of this crap is here since we don't want to have the .exe depend on tier0 or vstdlib.dll.  If we change that, this can go away and std::vector can go back
//  to CUtlVector and CUtlSymbol can be used like it's supposed to be used....
//
//
//
//

static void Q_strncpy( char *pDest, char const *pSrc, int maxLen )
{
	strncpy( pDest, pSrc, maxLen );
	if ( maxLen > 0 )
	{
		pDest[maxLen-1] = 0;
	}
}

static int	Q_strlen( const char *str )
{
	return strlen( str );
}


#if defined( _WIN32 ) || defined( WIN32 )
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#else	//_WIN32
#define PATHSEPARATOR(c) ((c) == '/')
#endif	//_WIN32

static void Q_FileBase( const char *in, char *out, int maxlen )
{
	if ( !in || !in[ 0 ] )
	{
		*out = 0;
		return;
	}

	int len, start, end;

	len = Q_strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end&& in[end] != '.' && !PATHSEPARATOR( in[end] ) )
	{
		end--;
	}
	
	if ( in[end] != '.' )		// no '.', copy to end
	{
		end = len-1;
	}
	else 
	{
		end--;					// Found ',', copy to left of '.'
	}

	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && !PATHSEPARATOR( in[start] ) )
	{
		start--;
	}

	if ( start < 0 || !PATHSEPARATOR( in[start] ) )
	{
		start = 0;
	}
	else 
	{
		start++;
	}

	// Length of new sting
	len = end - start + 1;

	int maxcopy = min( len + 1, maxlen );

	// Copy partial string
	Q_strncpy( out, &in[start], maxcopy );
}

#define COPY_ALL_CHARACTERS -1
static char *Q_strncat(char *pDest, const char *pSrc, size_t destBufferSize, int max_chars_to_copy )
{
	size_t charstocopy = (size_t)0;

	size_t len = strlen(pDest);
	size_t srclen = strlen( pSrc );
	if ( max_chars_to_copy <= COPY_ALL_CHARACTERS )
	{
		charstocopy = srclen;
	}
	else
	{
		charstocopy = (size_t)min( max_chars_to_copy, (int)srclen );
	}

	if ( len + charstocopy >= destBufferSize )
	{
		charstocopy = destBufferSize - len - 1;
	}

	if ( !charstocopy )
	{
		return pDest;
	}

	char *pOut = strncat( pDest, pSrc, charstocopy );
	pOut[destBufferSize-1] = 0;
	return pOut;
}

//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test
//-----------------------------------------------------------------------------
static char const* Q_stristr( char const* pStr, char const* pSearch )
{
	if (!pStr || !pSearch) 
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower((unsigned char)*pLetter) == tolower((unsigned char)*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower((unsigned char)*pMatch) != tolower((unsigned char)*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

static int	Q_stricmp( const char *s1, const char *s2 )
{
	return stricmp( s1, s2 );
}

//-----------------------------------------------------------------------------
// Purpose: utility function to split a typical P4 line output into var and value
//-----------------------------------------------------------------------------
static void SplitP4Output(const_char *data, char *pszCmd, char *pszInfo, int bufLen)
{
	Q_strncpy(pszCmd, data, bufLen);
	
	char *mid = (char *)Q_stristr(pszCmd, " ");
	if (mid)
	{
		*mid = 0;
		Q_strncpy(pszInfo, data + (mid - pszCmd) + 1, bufLen);
	}
	else
	{
		pszInfo[0] = 0;
	}
}

static int Q_atoi (const char *str)
{
	int             val;
	int             sign;
	int             c;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val<<4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val*10 + c - '0';
	}
	
	return 0;
}

static int Q_snprintf( char *pDest, int maxLen, char const *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
#ifdef _WIN32
	int len = _vsnprintf( pDest, maxLen, pFormat, marker );
#elif _LINUX
	int len = vsnprintf( pDest, maxLen, pFormat, marker );
#else
	#error "define vsnprintf type."
#endif
	va_end( marker );

	// Len < 0 represents an overflow
	if( len < 0 )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}

//-----------------------------------------------------------------------------
// Purpose: base class for parse input from the P4 server
//-----------------------------------------------------------------------------
template< class T >
class CDataRetrievalUser : public ClientUser
{
public:
	std::vector<T> &GetData()
	{
		return m_Data;
	}

	// call this to start retrieving data
	void InitRetrievingData()
	{
		m_bAwaitingNewRecord = true;
		m_Data.RemoveAll();
	}

	// implement this to parse out input from the server into the specified object
	virtual void OutputRecord(T &obj, const char *pszVar, const char *pszInfo) = 0;


private:
	bool m_bAwaitingNewRecord;
	std::vector<T> m_Data;


	virtual void OutputInfo(char level, const_char *data)
	{
		if (Q_strlen(data) < 1)
		{
			// end of a record, await the new one
			m_bAwaitingNewRecord = true;
			return;
		}

		if (m_bAwaitingNewRecord)
		{
			// add in the new record
			T newRec;
			m_Data.AddToTail( newRec );

			T &record = m_Data[ m_Data.Count() - 1 ];
			memset(&record, 0, sizeof(record));
			m_bAwaitingNewRecord = false;
		}

		// parse
		char szVar[_MAX_PATH];
		char szInfo[_MAX_PATH];
		SplitP4Output(data, szVar, szInfo, sizeof(szVar));

		// emit
		T &record = m_Data[m_Data.Count() - 1];
		OutputRecord(record, szVar, szInfo);
	}
};

class CP4Counter
{
public:
	CP4Counter() :
	  m_nValue( 0 )
	{
		m_szName[ 0 ] = 0;
	}

	char const *GetCounterName() const
	{
		return m_szName;
	}

	int GetValue() const
	{
		return m_nValue;
	}

	void	SetName( char const *name )
	{
		Q_strncpy( m_szName, name, sizeof( m_szName ) );
	}

	void	SetValue( int value )
	{
		m_nValue = value;
	}
private:

	char		m_szName[ 128 ];
	int			m_nValue;
};

//-----------------------------------------------------------------------------
// Purpose: Retrieves a file list
//-----------------------------------------------------------------------------
class CCountersUser : public CDataRetrievalUser<CP4Counter>
{
public:
	void RetrieveCounters()
	{
		// clear the list
		InitRetrievingData();

		client.Run("counters", this);
	}

private:
	virtual void OutputRecord(CP4Counter &counter, const char *szCmd, const char *szInfo)
	{
		if ( !Q_stricmp( szCmd, "counter" ) )
		{
            counter.SetName( szInfo );
		}
		else if ( !Q_stricmp( szCmd, "value" ) )
		{
			counter.SetValue( Q_atoi( szInfo ) );
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: Retrieves a file list
//-----------------------------------------------------------------------------
class CSetCounterUser : public ClientUser
{
public:
	void SetCounter( char *countername, int value )
	{
		char valuestr[ 32 ];
		Q_snprintf( valuestr, sizeof( valuestr ), "%d", value );
		char *argv[] = { countername, valuestr, NULL };
		client.SetArgv( 2, argv );
		client.Run("counter", this);
	}

	virtual void 	HandleError( Error *err )
	{
	}
	virtual void 	Message( Error *err )
	{
	}
	virtual void 	OutputError( const_char *errBuf )
	{
	}
	virtual void	OutputInfo( char level, const_char *data )
	{
	}
	virtual void 	OutputBinary( const_char *data, int length )
	{
	}
	virtual void 	OutputText( const_char *data, int length )
	{
	}
};

static CSetCounterUser g_SetCounterUser;
static CCountersUser g_CountersUser;

typedef enum
{
	MUTEX_QUERY = 0,
	MUTEX_LOCK,
	MUTEX_RELEASE,
} MUTEXACTION;

static void printusage( char const *basefile )
{
	printf( "usage:  %s \n\
\t< query | release | lock > <branchname> <sleepseconds> <clientname> [<ip:port>]\n\
\te.g.:\n\
\t%s query src_main 3 yahn\n\
\t%s query\n\
", basefile, basefile, basefile );
}

struct CUtlSymbol
{
public:
	CUtlSymbol()
	{
		m_szValue[ 0 ] = 0;
	}

	CUtlSymbol& operator =( const char * lhs )
	{
		Q_strncpy( m_szValue, lhs, sizeof( m_szValue ) );
		return *this;
	}

	char const *String()
	{
		return m_szValue;
	}
private:
	char m_szValue[ 256 ];
};

int FindLockUsers( CCountersUser& counters, char const *branchspec, std::vector< CUtlSymbol >& users, std::vector< int >& locktimes, std::vector< CUtlSymbol >* branchnames = NULL )
{
	users.RemoveAll();

	char lockstr[ 256 ];
	if ( branchspec != NULL )
	{
		Q_snprintf( lockstr, sizeof( lockstr ), "%s_lock_", branchspec );
	}
	else
	{
		Q_snprintf( lockstr, sizeof( lockstr ), "_lock_", branchspec );
	}

	counters.RetrieveCounters();
	std::vector< CP4Counter >& list = counters.GetData();
	int count = list.Count();
	for ( int i = 0; i < count; ++i )
	{
		char const *name = list[ i ].GetCounterName(); 
		int value = list[ i ].GetValue();

		char const *p = Q_stristr( name, lockstr );
		if ( !p )
			continue;

		if ( value != 0 )
		{
			CUtlSymbol sym;
			sym = p + Q_strlen( lockstr );

			users.AddToTail( sym );
			locktimes.AddToTail( value );

			if ( branchnames )
			{
				char branchname[ 512 ];
				Q_strncpy( branchname, name, p - name + 1 );
				CUtlSymbol sym;
				sym = branchname;
				branchnames->AddToTail( sym );
			}
		}
	}

	return users.Count();
}

static void GetHourMinuteSecondsString( int nInputSeconds, char *pOut, int outLen )
{
	int nMinutes = nInputSeconds / 60;
	int nSeconds = nInputSeconds - nMinutes * 60;
	int nHours = nMinutes / 60;
	nMinutes -= nHours * 60;

	char *extra[2] = { "", "s" };
	
	if ( nHours > 0 )
		Q_snprintf( pOut, outLen, "%d hour%s, %d minute%s, %d second%s", nHours, extra[nHours != 1], nMinutes, extra[nMinutes != 1], nSeconds, extra[nSeconds != 1] );
	else if ( nMinutes > 0 )
		Q_snprintf( pOut, outLen, "%d minute%s, %d second%s", nMinutes, extra[nMinutes != 1], nSeconds, extra[nSeconds != 1] );
	else
		Q_snprintf( pOut, outLen, "%d second%s", nSeconds, extra[nSeconds != 1] );
}

static void ComputeHoldTime( int holdtime, char *buf, size_t bufsize )
{
	buf[ 0 ] = 0;

	if ( holdtime < 100 )
	{
		Q_snprintf( buf, bufsize, "UNKNOWN" );
	}
	else
	{
		// Prepend the time.
		time_t aclock = (time_t)holdtime;
		struct tm *newtime = localtime( &aclock );
		
		// Get rid of the \n.
		Q_strncpy( buf, asctime( newtime ), bufsize );
		char *pEnd = (char *)Q_stristr( buf, "\n" );
		if ( pEnd )
		{
			*pEnd = 0;
		}

		time_t curtime;
		time( &curtime );

		int holdSeconds = curtime - holdtime;
		if ( holdSeconds > 0 )
		{
			char durstring[ 256 ];
			durstring[ 0 ] = 0;
			GetHourMinuteSecondsString( holdSeconds, durstring, sizeof( durstring ) );

			Q_strncat( buf, ", held for ", bufsize, COPY_ALL_CHARACTERS );
			Q_strncat( buf, durstring, bufsize, COPY_ALL_CHARACTERS );
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	char basefile[ 256 ];
	Q_FileBase( argv[ 0 ], basefile, sizeof( basefile ) );

	bool validAction = false;
	MUTEXACTION action = MUTEX_QUERY;
	bool validBranch = false;
	CUtlSymbol branchspec;
	bool validSleepSeconds = false;
	int sleepSeconds = 1;
	bool validClient = false;
	CUtlSymbol clientname;
	bool validIP = false;
	CUtlSymbol ipport;

	for ( int i = 1; i < argc; ++i )
	{
		switch ( i )
		{
		default:
			break;
		case 1:
			validAction = true;
			if ( !Q_stricmp( argv[ i ], "query" ) )
			{
				action = MUTEX_QUERY;
			}
			else if ( !Q_stricmp( argv[ i ], "release" ) )
			{
				action = MUTEX_RELEASE;
			}
			else if ( !Q_stricmp( argv[ i ], "lock" ) )
			{
				action = MUTEX_LOCK;
			}
			else 
			{
				validAction = false;
			}
			break;
		case 2:
			{
				validBranch = true;
				branchspec = argv[ i ];
			}
			break;
		case 3:
			{
				validSleepSeconds = true;
				sleepSeconds = clamp( Q_atoi( argv[ i ] ), 0, 100 );
			}
			break;
		case 4:
			{
				validClient = true;
				clientname = argv[ i ];
			}
			break;
		case 5:
			{
				validIP = true;
				ipport = argv[ i ];
			}
			break;
		}
	}

	bool describeLocksOnly = false;
	if ( !validBranch ||
		 !validSleepSeconds ||
		 !validClient ||
		 !validIP )
	{
		if ( !validAction || action != MUTEX_QUERY )
		{
			printusage( basefile );
			return -1;
		}

		describeLocksOnly = true;
		sleepSeconds = 5;
	}

	// set the protocol return all data as key/value pairs
	client.SetProtocol( "tag", "" );

	// connect to the p4 server
	Error e;
	if ( ipport.String()[ 0 ] )
	{
		client.SetPort( ipport.String() );
	}
	if ( clientname.String()[ 0 ] )
	{
		client.SetUser( clientname.String() );
	}

	client.Init( &e );
	bool connected = ( e.Test() == 0 ) ? true : false;
	if ( !connected )
	{
		printf( "Unable to connect to perforce server\n" );
		return -1;
	}

	if ( describeLocksOnly )
	{
		std::vector< CUtlSymbol > users;
		std::vector< int > locktimes;
		std::vector< CUtlSymbol > branchnames;
		int holdCount = FindLockUsers( g_CountersUser, NULL, users, locktimes, &branchnames );

		for ( int i = 0; i < holdCount; ++i )
		{
			char timestr[ 128 ];
			ComputeHoldTime( locktimes[ i ], timestr, sizeof( timestr ) );
			printf( "'%s' HELD by:  %s\n\ttime: %s\n", branchnames[ i ].String(), users[ i ].String(), timestr );
		}
	}
	else
	{

		std::vector< CUtlSymbol > users;
		std::vector< int > locktimes;
		int holdCount = FindLockUsers( g_CountersUser, branchspec.String(), users, locktimes );

		if ( holdCount >= 2 )
		{
			char userlist[ 1024 ];
			userlist[ 0 ] = 0;
			for ( int i = 0; i < (int)users.Count(); ++i )
			{
				Q_strncat( userlist, users[ i ].String(), sizeof( userlist ), COPY_ALL_CHARACTERS );
				if ( i != users.Count() - 1 )
				{
					Q_strncat( userlist, ", ", sizeof( userlist ), COPY_ALL_CHARACTERS );
				}
			}
			printf( "%s:  ERROR, multiple users (%s) holding lock on '%s'\n", basefile, userlist, branchspec.String() );
			printusage( basefile );
			return -1;
		}

		char setcountername[ 256 ];
		Q_snprintf( setcountername, sizeof( setcountername ), "%s_lock_%s", branchspec.String(), clientname.String() );

		switch ( action )
		{
		default:
			break;
		case MUTEX_QUERY:
			{
				if ( holdCount == 1 )
				{
					bool isHeldByLocal = false;

					if ( !Q_stricmp( users[ 0 ].String(), clientname.String() ) )
					{
						isHeldByLocal = true;
					}

					char timestr[ 128 ];
					ComputeHoldTime( locktimes[ 0 ], timestr, sizeof( timestr ) );
					printf( "%s: '%s' lock on %s is HELD by:  %s\nHOLD INFO:  %s\n", basefile, branchspec.String(), ipport.String(), users[ 0 ].String(), timestr );
				}
				else if ( holdCount == 0 )
				{
					printf( "%s: '%s' lock on %s is FREE\n", basefile, branchspec.String(), ipport.String() );
				}
			}
			break;
		case MUTEX_LOCK:
			{
				printf( "%s: Attempting to lock the '%s' codeline for %s on %s\n\n", basefile, branchspec.String(), clientname.String(), ipport.String() );

				// p4mutex: Attempting to lock the 'main_src' codeline for yahn on 207.173.178.12:1666.

				if ( holdCount == 1 )
				{
					bool isHeldByLocal = false;

					if ( !Q_stricmp( users[ 0 ].String(), clientname.String() ) )
					{
						isHeldByLocal = true;
					}

					char timestr[ 128 ];
					ComputeHoldTime( locktimes[ 0 ], timestr, sizeof( timestr ) );

					if ( isHeldByLocal )
					{
						// Success: You already have the 'main_src' codeline lock.
							printf( "Success: You already have the '%s' codeline lock\nInfo:  %s\n", branchspec.String(), timestr );
					}
					else
					{
						// Failed: 'main_goldsrc' lock currently owned by alfred
						printf( "Failed: '%s' lock currently owned by %s\nInfo:  %s\n", branchspec.String(), users[ 0 ].String(), timestr );
					}
				}
				else if ( holdCount == 0 )
				{
					// Success: 'main_src' codeline lock granted to yahn.
					// Set the counter
					time_t aclock;
					time( &aclock );
					g_SetCounterUser.SetCounter( setcountername, (int)aclock );

					printf( "Success: '%s' codeline lock granted to %s\n", branchspec.String(), clientname.String() );
				}
			}
			break;
		case MUTEX_RELEASE:
			{
				printf( "%s: Attempting to release the '%s' codeline for %s on %s\n\n", basefile, branchspec.String(), clientname.String(), ipport.String() );

				// p4mutex: Attempting to release the 'main_src' codeline for yahn on 207.173.178.12:1666.

				if ( holdCount == 1 )
				{
					bool isHeldByLocal = false;

					if ( !Q_stricmp( users[ 0 ].String(), clientname.String() ) )
					{
						isHeldByLocal = true;
					}

					char timestr[ 128 ];
					ComputeHoldTime( locktimes[ 0 ], timestr, sizeof( timestr ) );

					if ( isHeldByLocal )
					{
						// Success: 'main_src' codeline lock released.
						
						// Set the counter
						g_SetCounterUser.SetCounter( setcountername, 0 );

						printf( "Success: '%s' codeline lock released.\n", branchspec.String() );
					}
					else
					{
						// Failed: 'main_goldsrc' lock currently owned by alfred
						printf( "Failed: '%s' lock currently owned by %s\nInfo:  %s\n", branchspec.String(), users[ 0 ].String(), timestr );
					}
				}
				else if ( holdCount == 0 )
				{
					// Success: The 'main_src' codeline lock is already free.
					
					printf( "Success: The '%s' codeline lock is already free\n", branchspec.String() );
				}
			}
			break;
		}
	}

	if ( sleepSeconds > 0 )
	{
		time_t starttime;
		time( &starttime );

		int elapsed = 0;

		do 
		{
			time_t curtime;
			time( &curtime );
			elapsed = curtime - starttime;

			Sleep( 50 );

		} while ( elapsed < sleepSeconds && !kbhit() );
	}

	return 0;
}

