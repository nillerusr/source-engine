//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef KEY_TRANSLATION_H
#define KEY_TRANSLATION_H
#ifdef _WIN32
#pragma once
#endif

#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"

// Call this to initialize the system
void ButtonCode_InitKeyTranslationTable();

// Convert from Windows scan codes to Button codes.
ButtonCode_t ButtonCode_ScanCodeToButtonCode( int lParam );

// Update scan codes for foreign keyboards
void ButtonCode_UpdateScanCodeLayout( );

// Convert from Windows virtual key codes to Button codes.
ButtonCode_t ButtonCode_VirtualKeyToButtonCode( int keyCode );
int ButtonCode_ButtonCodeToVirtualKey( ButtonCode_t code );

ButtonCode_t ButtonCode_XKeyToButtonCode( int nPort, int keyCode );

// Convert back + forth between ButtonCode/AnalogCode + strings
const char *ButtonCode_ButtonCodeToString( ButtonCode_t code, bool bXController );
const char *AnalogCode_AnalogCodeToString( AnalogCode_t code );
ButtonCode_t ButtonCode_StringToButtonCode( const char *pString, bool bXController );
AnalogCode_t AnalogCode_StringToAnalogCode( const char *pString );
ButtonCode_t ButtonCode_SKeyToButtonCode( int nPort, int keyCode );

#endif // KEY_TRANSLATION_H
