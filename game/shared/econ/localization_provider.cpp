//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "localization_provider.h"

enum { kScratchBufferSize = 1024 };



// ----------------------------------------------------------------------------
// Find a localized string, but return something safe if the key is null or the localized
// string is missing.
// ----------------------------------------------------------------------------
locchar_t* CLocalizationProvider::FindSafe( const char* pchKey ) const
{
	if ( pchKey )
	{
		locchar_t* wszLocalized = Find( pchKey );
		if ( !wszLocalized )
		{
#ifdef STAGING_ONLY
			return const_cast< locchar_t* >(LOCCHAR("<NULL LOC STRING>"));		// Super janky cast alert! This method should really return a const locchar_t* but making that change breaks all the callsites...fix later.
#else
			return const_cast<locchar_t*>(LOCCHAR(""));
#endif
		}
		else
		{
			return wszLocalized;
		}
	}
	else
	{
#ifdef STAGING_ONLY
		return const_cast<locchar_t*>(LOCCHAR("<NULL LOC KEY>"));
#else
		return const_cast<locchar_t*>(LOCCHAR(""));
#endif
	}
}

#ifdef GC 
#include "gcsdk/gcbase.h"

// GC Localization implementation

static CGCLocalizationProvider *GGCLocalizationProvider() 
{
	static CGCLocalizationProvider *g_pGCLocalizationProvider = NULL;
	if ( !g_pGCLocalizationProvider )
		g_pGCLocalizationProvider = new CGCLocalizationProvider( GGCGameBase() );
	return g_pGCLocalizationProvider;
}

CLocalizationProvider *GLocalizationProvider()
{
	AssertMsg( false, "Using global localization provider in GC - All strings will be in English.  For proper localization, CLocalizationProvider instance should be created and passed in." );
	return GGCLocalizationProvider();
}

locchar_t *CGCLocalizationProvider::Find( const char *pchKey ) const
{
	// we emulate VGUI's behavior of returning an empty string for keys that are not found
	return (locchar_t*)m_pGC->LocalizeToken( pchKey, m_eLang, false );
}

bool CGCLocalizationProvider::BEnsureCleanUTF8Truncation( char *unicodeOutput )
{
	int nStringLength = V_strlen( unicodeOutput );

	// make sure we're not in the middle of a multibyte character
	int iPos = nStringLength - 1;
	char c = unicodeOutput[iPos];
	if ( (c & 0x80) != 0 )
	{
		// not an ascii char, so do some multibyte char checking
		int cBytes = 0;
		// count up all continuation bytes
		while ( (c & 0xC0) == 0x80 && iPos > 0 ) // first two bits are 10xxxx, continuation
		{
			cBytes++;
			c = unicodeOutput[--iPos];
		}

		// make sure we had the expected number of continuation bytes for the last
		//  multibyte lead character
		bool bTruncateOK = true;
		if ( ( c & 0xF8 ) == 0xF0 )	// first 5 bits are 11110, should be 3 following bytes
			bTruncateOK = ( cBytes == 3 );
		else if ( ( c & 0xF0 ) == 0xE0 ) // first 4 bits are 1110, should be 2 following bytes
			bTruncateOK = ( cBytes == 2 );
		else if ( ( c & 0xE0 ) == 0xC0 ) // first 3 bits are 110, should be 1 following byte
			bTruncateOK = ( cBytes == 1 );
			
		// if we truncated in the middle of a multi-byte char, move the end point back to this character
		if ( !bTruncateOK )
			unicodeOutput[iPos] = '\0';

		return !bTruncateOK;
	}

	return false;
}

void CGCLocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc_In, CUtlConstString *out_ansi ) const
{
	*out_ansi = loc_In;
}

void CGCLocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc_In, CUtlConstWideString *out_unicode ) const
{
	wchar_t utf16_Scratch[kScratchBufferSize];

	V_UTF8ToUnicode( loc_In, utf16_Scratch, kScratchBufferSize );
	*out_unicode = utf16_Scratch;
}

void CGCLocalizationProvider::ConvertUTF8ToLocchar( const char *utf8_In, CUtlConstStringBase<locchar_t> *out_loc ) const
{
	*out_loc = utf8_In;
}

int CGCLocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc, char *ansi, int ansiBufferSize ) const
{
	Q_strncpy( ansi, loc, ansiBufferSize );
	return 0;
}

int CGCLocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc, wchar_t *unicode, int unicodeBufferSize ) const
{
	return V_UTF8ToUnicode( loc, unicode, unicodeBufferSize );
}


void CGCLocalizationProvider::ConvertUTF8ToLocchar( const char *utf8, locchar_t *locchar, int loccharBufferSize ) const
{
	Q_strncpy( locchar, utf8, loccharBufferSize );
}


#else

CLocalizationProvider *GLocalizationProvider() 
{
	static CVGUILocalizationProvider g_VGUILocalizationProvider;
	return &g_VGUILocalizationProvider;
}

// vgui localization implementation

CVGUILocalizationProvider::CVGUILocalizationProvider()
{

}

locchar_t *CVGUILocalizationProvider::Find( const char *pchKey ) const
{
	return (locchar_t*)g_pVGuiLocalize->Find( pchKey );
}

void CVGUILocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc_In, CUtlConstString *out_ansi ) const
{
	char ansi_Scratch[kScratchBufferSize];

	g_pVGuiLocalize->ConvertUnicodeToANSI( loc_In, ansi_Scratch, kScratchBufferSize );
	*out_ansi = ansi_Scratch;
}

void CVGUILocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc_In, CUtlConstWideString *out_unicode ) const
{
	*out_unicode = loc_In;
}

void CVGUILocalizationProvider::ConvertUTF8ToLocchar( const char *utf8_In, CUtlConstStringBase<locchar_t> *out_loc ) const
{
	locchar_t loc_Scratch[kScratchBufferSize];

	V_UTF8ToUnicode( utf8_In, loc_Scratch, kScratchBufferSize );
	*out_loc = loc_Scratch;
}

void CVGUILocalizationProvider::ConvertUTF8ToLocchar( const char *utf8, locchar_t *locchar, int loccharBufferSize ) const
{
	V_UTF8ToUnicode( utf8, locchar, loccharBufferSize );
}

int CVGUILocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc, char *ansi, int ansiBufferSize ) const
{
	return g_pVGuiLocalize->ConvertUnicodeToANSI( loc, ansi, ansiBufferSize );
}

int CVGUILocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc, wchar_t *unicode, int unicodeBufferSize ) const
{
	Q_wcsncpy( unicode, loc, unicodeBufferSize );
	return 0;
}


#endif