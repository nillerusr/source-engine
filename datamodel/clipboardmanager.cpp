//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "clipboardmanager.h"
#include "datamodel.h"
#include "tier1/KeyValues.h"

#ifndef _LINUX
#define USE_WINDOWS_CLIPBOARD
#endif

#if defined( USE_WINDOWS_CLIPBOARD )
#include <windows.h>
#endif

CClipboardManager::CClipboardManager( ) :
	m_pfnCleanup( NULL )
{
}

CClipboardManager::~CClipboardManager()
{
	EmptyClipboard( false );
}

void CClipboardManager::EmptyClipboard( bool bClearWindowsClipboard )
{
	// Call optional cleanup function if there is one...
	if ( m_pfnCleanup )
	{
		m_pfnCleanup->ReleaseClipboardData( m_Data );
	}
	int c = m_Data.Count();
	for ( int i = 0; i < c; ++i )
	{
		m_Data[ i ]->deleteThis();
	}
	m_Data.RemoveAll();
	m_pfnCleanup = NULL;

#if defined( USE_WINDOWS_CLIPBOARD )
	if ( bClearWindowsClipboard )
	{
		if ( ::OpenClipboard( ::GetDesktopWindow() ) )
		{
			::EmptyClipboard();
			::CloseClipboard();
		}
	}
#endif
}

void CClipboardManager::SetClipboardData( CUtlVector< KeyValues * >& data, IClipboardCleanup *pfnOptionalCleanuFunction )
{
	EmptyClipboard( true );
	m_Data = data;
	m_pfnCleanup = pfnOptionalCleanuFunction;

#if defined( USE_WINDOWS_CLIPBOARD )
	if ( m_Data.Count() >= 0 )
	{
		// Only stick the first item's data into the clipboard
		char const *text = m_Data[ 0 ]->GetString( "text", "" );
		if ( text && text[ 0 ] )
		{
			int textLen = Q_strlen( text );

			if ( ::OpenClipboard( ::GetDesktopWindow() ) )
			{
				HANDLE hmem = ::GlobalAlloc(GMEM_MOVEABLE, textLen + 1);
				if (hmem)
				{
					void *ptr = ::GlobalLock( hmem );
					if ( ptr  )
					{
						Q_memset( ptr, 0, textLen + 1 );
						Q_memcpy( ptr, text, textLen );
						::GlobalUnlock( hmem );

						::SetClipboardData( CF_TEXT, hmem );
					}
				}
				::CloseClipboard();
			}
		}
	}
#endif
}

void CClipboardManager::AddToClipboardData( KeyValues *add )
{
	m_Data.AddToTail( add );
#if defined( USE_WINDOWS_CLIPBOARD )
	if ( m_Data.Count() >= 0 )
	{
		// Only stick the first item's data into the clipboard
		char const *text = m_Data[ 0 ]->GetString( "text", "" );
		if ( text && text[ 0 ] )
		{
			int textLen = Q_strlen( text );


			if ( ::OpenClipboard( ::GetDesktopWindow() ) )
			{
				::EmptyClipboard();

				HANDLE hmem = ::GlobalAlloc(GMEM_MOVEABLE, textLen + 1);
				if (hmem)
				{
					void *ptr = ::GlobalLock( hmem );
					if ( ptr  )
					{
						Q_memset( ptr, 0, textLen + 1 );
						Q_memcpy( ptr, text, textLen );
						::GlobalUnlock( hmem );

						::SetClipboardData( CF_TEXT, hmem );
					}
				}
				::CloseClipboard();
			}
		}
	}
#endif
}

void CClipboardManager::GetClipboardData( CUtlVector< KeyValues * >& data )
{
	data.RemoveAll();
	data = m_Data;
#if defined( USE_WINDOWS_CLIPBOARD )
	if ( data.Count() == 0 )
	{
		// See if windows has some text since we didn't have any internally
		if ( ::OpenClipboard( ::GetDesktopWindow() ) )
		{
			HANDLE hmem = ::GetClipboardData( CF_TEXT );
			if ( hmem )
			{
				int len = GlobalSize( hmem );
				if ( len > 0 )
				{
					void *ptr = GlobalLock(hmem);
					if ( ptr )
					{
						char buf[ 8192 ];
						len = min( len, 8191 );
						Q_memcpy( buf, ( char * )ptr, len );
						buf[ 8191 ] = 0;
						GlobalUnlock(hmem);

						KeyValues *newData = new KeyValues( "ClipBoard", "text", buf );
						data.AddToTail( newData );
					}
				}
			}
			::CloseClipboard();
		}
	}
#endif
}

bool CClipboardManager::HasClipboardData() const
{
	return m_Data.Count() > 0 ? true : false;
}
