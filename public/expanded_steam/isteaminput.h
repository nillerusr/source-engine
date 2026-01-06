//=============================================================================//
//
// Purpose: Community integration of Steam Input on Source SDK 2013.
//
// Author: Blixibon
//
// $NoKeywords: $
//=============================================================================//

#ifndef IN_STEAMINPUT_H
#define IN_STEAMINPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "tier1/utlvector.h"
#include "appframework/IAppSystem.h"
#include "inputsystem/InputEnums.h"
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>

//-----------------------------------------------------------------------------

#define IsDeck() (CommandLine()->CheckParm("-deck") != 0)

//-----------------------------------------------------------------------------

#ifndef ISTEAMINPUT_H // Stubs for when isteaminput.h isn't included
#define STEAM_INPUT_MAX_COUNT 16

typedef uint64 InputHandle_t;
typedef uint64 InputActionSetHandle_t;
typedef uint64 InputDigitalActionHandle_t;
typedef uint64 InputAnalogActionHandle_t;
#endif

struct InputDigitalActionBind_t
{
	InputDigitalActionHandle_t handle = 0;
	InputHandle_t controller = 0; // The last controller pressing if it is down
	bool bDown = false;
	bool bQueue = false;

	virtual void OnDown() { ; }
	virtual void OnUp() { ; }
};

enum ActionSet_t
{
	AS_GameControls,
	AS_VehicleControls,
	AS_MenuControls,
};

//-----------------------------------------------------------------------------

// Not to be confused with STEAMINPUT_INTERFACE_VERSION
#define SOURCE2013STEAMINPUT_INTERFACE_VERSION		"Source2013SteamInput001"

abstract_class ISource2013SteamInput
{
public:

	virtual void Initialize( CreateInterfaceFn factory ) = 0;
	virtual void Shutdown() = 0;

	virtual void InitSteamInput() = 0;

	virtual void PostInit() = 0;

	virtual void LevelInitPreEntity() = 0;

	virtual void RunFrame( ActionSet_t &iActionSet ) = 0;
	
	// "Enabled" just means a controller is being used
	virtual bool IsEnabled() = 0;

	//-----------------------------------------------------------------------------

	virtual bool IsSteamRunningOnSteamDeck() = 0;
	virtual void SetGamepadUI( bool bToggle ) = 0;
	
	virtual InputHandle_t GetActiveController() = 0;
	virtual int GetConnectedControllers( InputHandle_t *nOutHandles ) = 0;

	virtual const char *GetControllerName( InputHandle_t nController ) = 0;
	virtual int GetControllerType( InputHandle_t nController ) = 0;

	virtual bool ShowBindingPanel( InputHandle_t nController ) = 0;

	//-----------------------------------------------------------------------------

	virtual bool ShowGamepadTextInput( const char *pszDescription, uint32 nMaxChars, bool bMultiline = false, const char *pszExistingText = "" ) = 0;
	virtual bool ShowFloatingGamepadTextInput( bool bMultiline, int nX, int nY, int nW, int nH ) = 0;

	virtual bool GetEnteredGamepadTextInput( char *pszText, uint32 nTextSize ) = 0;
	virtual uint32 GetEnteredGamepadTextLength() = 0;

	//-----------------------------------------------------------------------------

	virtual bool UsingJoysticks() = 0;
	virtual void GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
		bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw ) = 0;

	virtual void SetRumble( InputHandle_t nController, float fLeftMotor, float fRightMotor, int userId = INVALID_USER_ID ) = 0;
	virtual void StopRumble() = 0;

	//-------------------------------------------

	virtual void SetLEDColor( InputHandle_t nController, byte r, byte g, byte b ) = 0;
	virtual void ResetLEDColor( InputHandle_t nController ) = 0;

	//-----------------------------------------------------------------------------

	virtual bool UseGlyphs() = 0;
	virtual void GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char*> &szStringList, int iActionSet = -1 ) = 0;

	virtual void GetGlyphPNGsForCommand( CUtlVector<const char*> &szImgList, const char *pszCommand, int &iSize, int iStyle = 0 ) = 0;
	virtual void GetGlyphSVGsForCommand( CUtlVector<const char*> &szImgList, const char *pszCommand ) = 0;

	//-----------------------------------------------------------------------------

	virtual void RemapHudHint( const char **pszInputHint ) = 0;

private:
};

// TODO: Replace with proper singleton interface in the future
ISource2013SteamInput *CreateSource2013SteamInput();

#endif // IN_MAIN_H
