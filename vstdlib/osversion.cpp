//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "vstdlib/osversion.h"
#include "winlite.h"
#include "strtools.h"
#include "tier0/dbg.h"

#ifdef OSX
#include <CoreServices/CoreServices.h>
#endif

//-----------------------------------------------------------------------------
// Purpose: return the OS type for this machine
//-----------------------------------------------------------------------------
EOSType GetOSType()
{
	static EOSType eOSVersion = k_eOSUnknown;

#if defined( _WIN32 ) && !defined( _X360 )
	if ( eOSVersion == k_eOSUnknown || eOSVersion == k_eWinUnknown )
	{
		eOSVersion = k_eWinUnknown;
		OSVERSIONINFOEX osvi;
		Q_memset( &osvi, 0x00, sizeof(osvi) );
		osvi.dwOSVersionInfoSize = sizeof(osvi);
		
		if ( GetVersionEx( (OSVERSIONINFO *) &osvi ) )
		{
			switch ( osvi.dwPlatformId )
			{
			case VER_PLATFORM_WIN32_NT:
				if ( osvi.dwMajorVersion <= 4 )
				{
					eOSVersion = k_eWinNT;
				}
				else if ( osvi.dwMajorVersion == 5 )
				{
					switch(  osvi.dwMinorVersion ) 
					{
					case 0:
						eOSVersion = k_eWin2000;
						break;
					case 1:
						eOSVersion = k_eWinXP;
						break;
					case 2:
						eOSVersion = k_eWin2003;
						break;
					}
				}
				else if ( osvi.dwMajorVersion >= 6 )
				{
					if ( osvi.wProductType == VER_NT_WORKSTATION )
					{
						switch ( osvi.dwMinorVersion )
						{
						case 0:
							eOSVersion = k_eWinVista;
							break;
						case 1:
							eOSVersion = k_eWindows7;
							break;
						}
					}
					else /* ( osvi.wProductType != VER_NT_WORKSTATION ) */
					{
						switch ( osvi.dwMinorVersion )
						{
						case 0:
							eOSVersion = k_eWin2008;	// Windows 2008, not R2
							break;
						case 1:
							eOSVersion = k_eWin2008;	// Windows 2008 R2
							break;
						}
					}
				}
				break;
			case VER_PLATFORM_WIN32_WINDOWS:
				switch ( osvi.dwMinorVersion )
				{
				case 0:
					eOSVersion = k_eWin95;
					break;
				case 10:
					eOSVersion = k_eWin98;
					break;
				case 90:
					eOSVersion = k_eWinME;
					break;
				}
				break;
			case VER_PLATFORM_WIN32s:
				eOSVersion = k_eWin311;
				break;
			}
		}
	}
#elif defined(OSX)
	if ( eOSVersion == k_eOSUnknown )
	{
		SInt32 MajorVer = 0;
		SInt32 MinorVer = 0;
		SInt32 PatchVer = 0;
		OSErr err = noErr;
		err = Gestalt( gestaltSystemVersionMajor, &MajorVer );
		if ( err != noErr )
			return k_eOSUnknown;
		err = Gestalt( gestaltSystemVersionMinor, &MinorVer );
		if ( err != noErr )
			return k_eOSUnknown;
		err = Gestalt( gestaltSystemVersionBugFix, &PatchVer );
		if ( err != noErr )
			return k_eOSUnknown;
		
		switch ( MajorVer )
		{
			case 10:
			{
				switch( MinorVer )
				{
					case 4:
						eOSVersion = k_eMacOS104;
						break;
					case 5:
						eOSVersion = k_eMacOS105;						
						switch ( PatchVer )
						{
							case 8:
								eOSVersion = k_eMacOS1058;								
							default:
								break;
						}
						break;
					case 6:
						eOSVersion = k_eMacOS106;
						switch ( PatchVer )
						{
							case 1:
							case 2:
								break;
							case 3:
							default:
								// note the default here - 10.6.4 (5,6...) >= 10.6.3, so we want to 
								// identify as 10.6.3 for sysreqs purposes
								eOSVersion = k_eMacOS1063;
								break;
						}
						break;
					case 7:
						eOSVersion = k_eMacOS107;							
						break;
					default:
						break;
				}
			}
			default:
				break;
		}
	}
#elif defined(LINUX)
	if ( eOSVersion == k_eOSUnknown )
	{
		
		FILE *fpKernelVer = fopen( "/proc/version", "r" );
		
		if ( !fpKernelVer )
			return k_eLinuxUnknown;
		
		char rgchVersionLine[1024];
		char *pchRet = fgets( rgchVersionLine, sizeof(rgchVersionLine), fpKernelVer );
		fclose( fpKernelVer );
		
		eOSVersion = k_eLinuxUnknown;
		
		// move past "Linux version "
		const char *pchVersion = rgchVersionLine + Q_strlen( "Linux version " );
		if ( pchRet && *pchVersion == '2' && *(pchVersion+1) == '.' )
		{
			pchVersion += 2; // move past "2."
			if ( *pchVersion == '2' && *(pchVersion+1) == '.' )
				eOSVersion = k_eLinux22;
			else if ( *pchVersion == '4' && *(pchVersion+1) == '.' )
				eOSVersion = k_eLinux24;
			else if ( *pchVersion == '6' && *(pchVersion+1) == '.' )
				eOSVersion = k_eLinux26;
		}
	}
#endif
	return eOSVersion;
}

//-----------------------------------------------------------------------------
// Purpose: get platform-specific OS details (distro, on linux)
// returns a pointer to the input buffer on success (for convenience), 
// NULL on failure.
//-----------------------------------------------------------------------------
const char *GetOSDetailString( char *pchOutBuf, int cchOutBuf )
{
#if defined WIN32 
	(void)( pchOutBuf );
	(void)( cchOutBuf );
	// no interesting details
	return NULL;
#else
#if defined LINUX
	// we're about to go poking around to see if we can figure out distribution
	// looking @ any /etc file is fragile (people can change 'em), 
	// but since this is just hardware survey data, we're not super concerned.  
	// a bunch of OS-specific issue files
	const char *pszIssueFile[] =
	{
		"/etc/redhat-release",
		"/etc/fedora-release",
		"/etc/slackware-release",
		"/etc/debian_release",
		"/etc/mandrake-release",
		"/etc/yellowdog-release",
		"/etc/gentoo-release",
		"/etc/lsb-release",
		"/etc/SUSE-release",
	};
	if ( !pchOutBuf )
		return NULL;

	for (int i = 0; i < Q_ARRAYSIZE( pszIssueFile ); i++ )
	{
		FILE *fdInfo = fopen( pszIssueFile[i], "r" );
		if ( !fdInfo  )
			continue;
		
		// prepend the buffer with the name of the file we found for easier grouping
		snprintf( pchOutBuf, cchOutBuf, "%s\n", pszIssueFile[i] );
		int cchIssueFile = strlen( pszIssueFile[i] ) + 1;
		ssize_t cubRead = fread( (void*) (pchOutBuf + cchIssueFile) , sizeof(char), cchOutBuf - cchIssueFile, fdInfo );
		fclose( fdInfo );

		if ( cubRead < 0 )
			return NULL;

		// null terminate
		pchOutBuf[ MIN( cubRead, cchOutBuf-1 ) ] = '\0';
		return pchOutBuf;
	}
#endif
	// if all else fails, just send back uname -a
	if ( !pchOutBuf )
		return NULL;
	FILE *fpUname = popen( "uname -mrsv", "r" );
	if ( !fpUname )
		return NULL;
	size_t cchRead = fread( pchOutBuf, sizeof(char), cchOutBuf, fpUname );
	pclose( fpUname );

	pchOutBuf[ MIN( cchRead, (size_t)cchOutBuf-1 ) ] = '\0';
	return pchOutBuf;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: get a friendly name for an OS type
//-----------------------------------------------------------------------------
const char *GetNameFromOSType( EOSType eOSType )
{
	switch ( eOSType )
	{
	case k_eWinUnknown:
		return "Windows";
	case k_eWin311:
		return "Windows 3.11";
	case k_eWin95:
		return "Windows 95";
	case k_eWin98:
		return "Windows 98";
	case k_eWinME:
		return "Windows ME";
	case k_eWinNT:
		return "Windows NT";
	case k_eWin2000:
		return "Windows 2000";
	case k_eWinXP:
		return "Windows XP";
	case k_eWin2003:
		return "Windows 2003";
	case k_eWinVista:
		return "Windows Vista";
	case k_eWindows7:
		return "Windows 7";
	case k_eWin2008:
		return "Windows 2008";
#ifdef POSIX
	case k_eMacOSUnknown:
		return "Mac OS";
	case k_eMacOS104:
		return "MacOS 10.4";
	case k_eMacOS105:
		return "MacOS 10.5";
	case k_eMacOS1058:
		return "MacOS 10.5.8";
	case k_eMacOS106:
		return "MacOS 10.6";
	case k_eMacOS1063:
		return "MacOS 10.6.3";
	case k_eMacOS107:
		return "MacOS 10.7";
	case k_eLinuxUnknown:
		return "Linux";
	case k_eLinux22:
		return "Linux 2.2";
	case k_eLinux24:
		return "Linux 2.4";
	case k_eLinux26:
		return "Linux 2.6";
#endif
	default:
	case k_eOSUnknown:
		return "Unknown";
	}
}


// friendly name to OS type, MUST be same size as EOSType enum
struct OSTypeNameTuple
{
	EOSType m_OSType;
	const char *m_pchOSName;

};

const OSTypeNameTuple k_rgOSTypeToName[] = 
{
	{ k_eOSUnknown, "unknown" },
	{ k_eMacOSUnknown, "macos" },
	{ k_eMacOS104, "macos104" },
	{ k_eMacOS105, "macos105" },
	{ k_eMacOS1058, "macos1058" },
	{ k_eMacOS106, "macos106" },
	{ k_eMacOS1063, "macos1063" },
	{ k_eMacOS107, "macos107" },
	{ k_eLinuxUnknown, "linux" },
	{ k_eLinux22, "linux22" },
	{ k_eLinux24, "linux24" },
	{ k_eLinux26, "linux26" },
	{ k_eWinUnknown, "windows" },
	{ k_eWin311, "win311" },
	{ k_eWin95, "win95" },
	{ k_eWin98, "win98" },
	{ k_eWinME, "winME" },
	{ k_eWinNT, "winNT" },
	{ k_eWin2000, "win200" },
	{ k_eWinXP, "winXP" },
	{ k_eWin2003, "win2003" },
	{ k_eWinVista, "winVista" },
	{ k_eWindows7, "win7" },
	{ k_eWin2008, "win2008" },
};

//-----------------------------------------------------------------------------
// Purpose: convert a friendly OS name to a eostype
//-----------------------------------------------------------------------------
EOSType GetOSTypeFromString_Deprecated( const char *pchName )
{
	EOSType eOSType;
#ifdef WIN32
	eOSType = k_eWinUnknown;
#else
	eOSType = k_eOSUnknown;
#endif

	// if this fires, make sure all OS types are in the map
	Assert( Q_ARRAYSIZE( k_rgOSTypeToName ) == k_eOSTypeMax ); 
	if ( !pchName || Q_strlen( pchName ) == 0 )
		return eOSType;

	for ( int iOS = 0; iOS < Q_ARRAYSIZE( k_rgOSTypeToName ) ; iOS++ )
	{
		if ( !Q_stricmp( k_rgOSTypeToName[iOS].m_pchOSName, pchName ) ) 
			return k_rgOSTypeToName[iOS].m_OSType;
	}
	return eOSType;
}

bool OSTypesAreCompatible( EOSType eOSTypeDetected, EOSType eOSTypeRequired )
{
	// check windows (on the positive side of the number line)
	if ( eOSTypeRequired >= k_eWinUnknown )
		return ( eOSTypeDetected >= eOSTypeRequired );

	if ( eOSTypeRequired == k_eOSUnknown )
		return true;

	// osx
	if ( eOSTypeRequired >= k_eMacOSUnknown && eOSTypeRequired < k_eOSUnknown )
		return ( eOSTypeDetected >= eOSTypeRequired && eOSTypeDetected < k_eOSUnknown );

	// and linux
	if ( eOSTypeRequired >= k_eLinuxUnknown && eOSTypeRequired < k_eMacOSUnknown )
		return ( eOSTypeDetected >= eOSTypeRequired && eOSTypeDetected < k_eMacOSUnknown );

	return false;
}

// these strings "windows", "macos", "linux" are part of the
// interface, which is why they're hard-coded here, rather than using
// the strings in k_rgOSTypeToName
const char *GetPlatformName( bool *pbIs64Bit )
{
	if ( pbIs64Bit )
		*pbIs64Bit = Is64BitOS();
	
	EOSType eType = GetOSType(); 
	if ( OSTypesAreCompatible( eType, k_eWinUnknown ) )
		return "windows";
	if ( OSTypesAreCompatible( eType, k_eMacOSUnknown ) )
		return "macos";
	if ( OSTypesAreCompatible( eType, k_eLinuxUnknown ) )
		return "linux";
	return "unknown";
}

