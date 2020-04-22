//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <stdio.h>
#include <memory.h>
#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif

#include "ContentControlDialog.h"
#include "checksum_md5.h"
#include "EngineInterface.h"

#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include "tier1/KeyValues.h"
#include "tier1/convar.h"

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/TextEntry.h>
#include <tier0/vcrmode.h>

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CContentControlDialog::CContentControlDialog(vgui::Panel *parent) : vgui::Frame(parent, "ContentControlDialog")
{
	SetBounds(0, 0, 372, 160);
	SetSizeable( false );

	SetTitle( "#GameUI_ContentLock", true );

	m_pStatus = new vgui::Label( this, "ContentStatus", "" );

	m_pPasswordLabel = new vgui::Label( this, "PasswordPrompt", "#GameUI_PasswordPrompt" );
	m_pPassword2Label = new vgui::Label( this, "PasswordReentryPrompt", "#GameUI_PasswordReentryPrompt" );

	m_pExplain = new vgui::Label( this, "ContentControlExplain", "" );

	m_pPassword = new vgui::TextEntry( this, "Password" );
	m_pPassword2 = new vgui::TextEntry( this, "Password2" );

	m_pOK = new vgui::Button( this, "Ok", "#GameUI_OK" );
	m_pOK->SetCommand( "Ok" );

	vgui::Button *cancel = new vgui::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Cancel" );

	m_szGorePW[ 0 ] = 0;
    ResetPassword();

	LoadControlSettings("Resource\\ContentControlDialog.res");

//	Explain("");
//	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CContentControlDialog::~CContentControlDialog()
{
}

void CContentControlDialog::Activate()
{
    BaseClass::Activate();

    m_pPassword->SetText("");
    m_pPassword->RequestFocus();
    m_pPassword2->SetText("");
	Explain("");
	UpdateContentControlStatus();

	input()->SetAppModalSurface(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::ResetPassword()
{
	// Set initial value
#ifdef WIN32
#ifndef _XBOX
	HKEY key;
	if ( ERROR_SUCCESS == VCRHook_RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Half-Life\\Settings", 0, KEY_READ, &key))
	{
		DWORD type;
		DWORD bufSize = sizeof(m_szGorePW);

		VCRHook_RegQueryValueEx(key, "User Token 2", NULL, &type, (unsigned char *)m_szGorePW, &bufSize );
		VCRHook_RegCloseKey( key );
	}
    else
#endif
    {
        m_szGorePW[ 0 ] = 0;
    }
#else
	vgui::system()->SetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", m_szGorePW );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::ApplyPassword()
{
    WriteToken( m_szGorePW );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::Explain( char const *fmt, ... )
{
	if ( !m_pExplain )
		return;

	va_list		argptr;
	char		text[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	m_pExplain->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CContentControlDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		bool canclose = false;

		char pw1[ 256 ];
		char pw2[ 256 ];

		m_pPassword->GetText( pw1, 256 );
		m_pPassword2->GetText( pw2, 256 );

        // Get text and check
//        bool enabled = PasswordEnabled(); //( m_szGorePW[0]!=0 ) ? true : false;
//		bool pwMatch = stricmp( pw1, pw2 ) == 0 ? true : false;

        if (IsPasswordEnabledInDialog())
        {
            canclose = DisablePassword(pw1);
//            canclose = CheckPassword( m_szGorePW, pw1, false );
        }
        else if (!strcmp(pw1, pw2))
        {
            canclose = EnablePassword(pw1);
//            canclose = CheckPassword( NULL, pw1, true );
        }
		else
		{
			Explain( "#GameUI_PasswordsDontMatch" );
		}

		if ( canclose )
		{
			OnClose();
		}
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		OnClose();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::OnClose()
{
	BaseClass::OnClose();
    PostActionSignal(new KeyValues("ContentControlClose"));
//	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::WriteToken( const char *str )
{
	// Set initial value
#ifdef WIN32
#ifndef _XBOX
	HKEY key;
	if ( ERROR_SUCCESS == VCRHook_RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Half-Life\\Settings", 0, KEY_WRITE, &key))
	{
		DWORD type = REG_SZ;
		DWORD bufSize = strlen( str ) + 1;

		VCRHook_RegSetValueEx(key, "User Token 2", 0, type, (const unsigned char *)str, bufSize );

		VCRHook_RegCloseKey( key );
	}
#endif
#else
	vgui::system()->SetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", m_szGorePW );
#endif
	Q_strncpy( m_szGorePW, str, sizeof( m_szGorePW ) );

	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::HashPassword(const char *newPW, char *hashBuffer, int maxlen )
{
	// Compute the md5 hash and save it.
	unsigned char md5_hash[16];
	MD5Context_t ctx;

	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char const *)newPW, strlen( newPW ) );
	MD5Final( md5_hash, &ctx );

	char hex[ 128 ];
	Q_binarytohex( md5_hash, sizeof( md5_hash ), hex, sizeof( hex ) );

//	char digestedPW[ 128 ];
	Q_strncpy( hashBuffer, hex, maxlen );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
bool CContentControlDialog::CheckPassword( char const *oldPW, char const *newPW, bool enableContentControl )
{
	char digestedPW[ 128 ];
    HashPassword(newPW, digestedPW, sizeof( digestedPW ) );
	
    // Compute the md5 hash and save it.
	unsigned char md5_hash[16];
	MD5Context_t ctx;

	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char const *)(LPCSTR)newPW, strlen( newPW ) );
	MD5Final( md5_hash, &ctx );

	char hex[ 128 ];
	Q_binarytohex( md5_hash, sizeof( md5_hash ), hex, sizeof( hex ) );

	Q_strncpy( digestedPW, hex, sizeof( digestedPW ) );
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CContentControlDialog::EnablePassword(const char *newPW)
{
    if ( !newPW[ 0 ] )
    {
        Explain( "#GameUI_MustEnterPassword" );
        return false;
    }

	char digestedPW[ 128 ];
    HashPassword(newPW, digestedPW, sizeof( digestedPW ) );

	// disable violence
/*	engine->Cvar_SetValue("violence_hblood", 0.0 );
	engine->Cvar_SetValue("violence_hgibs" , 0.0 );
	engine->Cvar_SetValue("violence_ablood", 0.0 );
	engine->Cvar_SetValue("violence_agibs" , 0.0 );
	*/

	ConVarRef violence_hblood( "violence_hblood" );
	violence_hblood.SetValue(false);

	ConVarRef violence_hgibs( "violence_hgibs" );
	violence_hgibs.SetValue(false);

	ConVarRef violence_ablood( "violence_ablood" );
	violence_ablood.SetValue(false);

	ConVarRef violence_agibs( "violence_agibs" );
	violence_agibs.SetValue(false);
	
    // Store digest to registry
//    WriteToken( digestedPW );
    Q_strncpy(m_szGorePW, digestedPW, sizeof( m_szGorePW ) );
    /*
		}
		else
		{
			if ( stricmp( oldPW, digestedPW ) )
			{
				// Warn that password is invalid
				Explain( "#GameUI_IncorrectPassword" );
				return false;
			}
		}
	}*/
    return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CContentControlDialog::DisablePassword(const char *oldPW)
{
    if ( !oldPW[ 0 ] )
    {
        Explain( "#GameUI_MustEnterPassword" );
        return false;
    }

	char digestedPW[ 128 ];
    HashPassword(oldPW, digestedPW, sizeof( digestedPW ) );

    if( stricmp( m_szGorePW, digestedPW ) )
    {
        Explain( "#GameUI_IncorrectPassword" );
        return false;
    }

    m_szGorePW[0] = 0;

	// set the violence cvars
/*	engine->Cvar_SetValue("violence_hblood", 1.0 );
	engine->Cvar_SetValue("violence_hgibs" , 1.0 );
	engine->Cvar_SetValue("violence_ablood", 1.0 );
	engine->Cvar_SetValue("violence_agibs" , 1.0 );
	*/
	ConVarRef violence_hblood( "violence_hblood" );
	violence_hblood.SetValue(true);

	ConVarRef violence_hgibs( "violence_hgibs" );
	violence_hgibs.SetValue(true);

	ConVarRef violence_ablood( "violence_ablood" );
	violence_ablood.SetValue(true);

	ConVarRef violence_agibs( "violence_agibs" );
	violence_agibs.SetValue(true);


//		// Remove digest value
//		WriteToken( "" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CContentControlDialog::IsPasswordEnabledInDialog()
{
    return m_szGorePW[0] != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::UpdateContentControlStatus( void )
{
	bool enabled = IsPasswordEnabledInDialog(); //( m_szGorePW[0]!=0 ) ? true : false;
	m_pStatus->SetText( enabled ? "#GameUI_ContentStatusEnabled" : "#GameUI_ContentStatusDisabled" );

    if (enabled)
    {
        m_pPasswordLabel->SetText("#GameUI_PasswordDisablePrompt");
    }
    else
    {
        m_pPasswordLabel->SetText("#GameUI_PasswordPrompt");
    }

    // hide the re-entry
    m_pPassword2Label->SetVisible(!enabled);
    m_pPassword2->SetVisible(!enabled);
//	m_pOK->SetText( enabled ? "#GameUI_Disable" : "#GameUI_Enable" );
}
