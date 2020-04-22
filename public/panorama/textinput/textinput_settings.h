//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_SETTINGS_H
#define PANORAMA_TEXTINPUT_SETTINGS_H

#include "panorama/controls/panel2d.h"
#include "panorama/input/iuiinput.h"
#include "panorama/textinput/textinput.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Handler settings that get passed in at construction time
//-----------------------------------------------------------------------------
class CTextInputHandlerSettings
{
public:
	CTextInputHandlerSettings();

	// Parses property from configuration
	bool BSetProperty( CPanoramaSymbol symName, const char *pchValue );

	// Convenient accessor methods
	void SetCancellable( bool bCancellable ) { m_bCancellable = bCancellable; }
	bool BCancellable() const { return m_bCancellable; }

	void SetHideSuggestions( bool bHideSuggestions ) { m_bHideSuggestions = bHideSuggestions; }
	bool BHideSuggestions() const { return m_bHideSuggestions; }

	void SetDoubleSpaceToDotSpace( bool bDoubleSpaceToDotSpace ) { m_bDoubleSpaceToDotSpace = bDoubleSpaceToDotSpace; }
	bool BDoubleSpaceToDotSpace() const { return m_bDoubleSpaceToDotSpace; }

	void SetAutoCaps( bool bAutoCaps ) { m_bAutoCaps = bAutoCaps; }
	bool BAutoCaps() const { return m_bAutoCaps; }

	void SetID( const char *pszID ) { m_strID = pszID; }
	const char *GetID() const { return m_strID; }

	void SetClasses( const char *pszClasses ) { m_strClasses = pszClasses; }
	const char *GetClasses() const { return m_strClasses; }

	void SetDoneActionString( const char *pszActionString ) { m_strDoneActionString = pszActionString; }
	const char *GetDoneActionString() const { return m_strDoneActionString; }

	void SetCancelActionString( const char *pszCancelActionString ) { m_strCancelActionString = pszCancelActionString; }
	const char *GetCancelActionString() const { return m_strCancelActionString; }

	void SetMode( ETextInputMode_t mode ) { m_mode = mode; }
	ETextInputMode_t GetMode() const { return m_mode; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

public:
	bool m_bCancellable;
	bool m_bHideSuggestions;
	bool m_bDoubleSpaceToDotSpace;
	bool m_bAutoCaps;
	CUtlString m_strID;
	CUtlString m_strClasses;
	CUtlString m_strDoneActionString;	
	CUtlString m_strCancelActionString;	
	ETextInputMode_t m_mode;
};

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_SETTINGS_H

