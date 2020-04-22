//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UISETTINGS_H
#define UISETTINGS_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: interface to query settings for this panorama UI
//-----------------------------------------------------------------------------
class IUISettings
{
public:
	virtual const char *GetUILanguage() = 0;
	virtual void GetPreferredResolution( int &dxPreferred, int &dyPreferred ) = 0;
	virtual void UpdateGamepadMappingHints( const char *pszConnectedMappings ) = 0;
	virtual bool BAllowOSModalDialog() = 0;

	virtual void GetDefaultAudioDevice( CUtlString &strDevice ) = 0;
	virtual void SetDefaultAudioDevice( const char *pchDevice ) = 0;

	// if unset then the default audio device is used for voice output
	virtual void GetDefaultVoiceDevice( CUtlString &strDevice ) = 0;
	virtual void SetDefaultVoiceDevice( const char *pchDevice ) = 0;

	virtual bool GetUseSystemAudioForVoice() = 0;
	virtual void SetUseSystemAudioForVoice( bool bUseSystemAudio ) = 0;

	virtual ETextInputHandlerType_t GetActiveTextInputHandlerType() const = 0;
	virtual void SetDefaultTextInputHandlerType( ETextInputHandlerType_t eType ) = 0;

	virtual ELanguage GetDefaultInputLanguage() const = 0;
	virtual void SetDefaultInputLanguage( ELanguage ) = 0;

	// should we show the screen saver?
	virtual bool GetScreenSaverEnabled() = 0;
	virtual void SetScreenSaverEnabled( bool bEnabled ) = 0;
};

} // namespace panorama

#endif // UISETTINGS_H
