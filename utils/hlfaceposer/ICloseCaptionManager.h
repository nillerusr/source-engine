//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ICLOSECAPTIONMANAGER_H
#define ICLOSECAPTIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

typedef struct tagRECT RECT;
class CSentence;
class StudioModel;
class CChoreoWidgetDrawHelper;

class ICloseCaptionManager
{
public:
	virtual void Reset( void ) = 0;

	virtual void Process( char const *tokenname, float duration, int languageid ) = 0;

	virtual bool LookupUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count ) = 0;
	// Same as above, except strips out <> command tokens
	virtual bool LookupStrippedUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count ) = 0;
};

extern ICloseCaptionManager *closecaptionmanager;

#endif // ICLOSECAPTIONMANAGER_H
