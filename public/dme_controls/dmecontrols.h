//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef DMECONTROLS_H
#define DMECONTROLS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ISoundEmitterSystemBase;
class IEngineTool;
class IPhysicsCollision;
class IElementPropertiesChoices;

namespace vgui
{

//-----------------------------------------------------------------------------
// handles the initialization of the vgui interfaces.
// NOTE: Calls into VGui_InitMatSysInterfacesList
// interfaces (listed below) are first attempted to be loaded from primaryProvider, then secondaryProvider
// moduleName should be the name of the module that this instance of the vgui_controls has been compiled into
//-----------------------------------------------------------------------------
bool VGui_InitDmeInterfacesList( const char *moduleName, CreateInterfaceFn *factoryList, int numFactories );


//-----------------------------------------------------------------------------
// set of accessor functions to matsys interfaces
// the appropriate header file for each is listed above the item
//-----------------------------------------------------------------------------

// #include "soundemittersystem/isoundemittersystembase.h"
ISoundEmitterSystemBase *SoundEmitterSystem();

// #include "toolsframework/ienginetool.h"
IEngineTool *EngineTool();

// #include "vphysics_interface.h"
IPhysicsCollision *PhysicsCollision();

// #include "dme_controls/INotifyUI.h"
IElementPropertiesChoices *ElementPropertiesChoices();
void SetElementPropertiesChoices( IElementPropertiesChoices *pChoices );

} // end namespace vgui


//-----------------------------------------------------------------------------
// predeclare all the matsys control class names
//-----------------------------------------------------------------------------
class CDmeMDLPanel;
class CMDLSequencePicker;
class CMDLPicker;
class CSequencePicker;
class CDmePicker;
class CSoundPicker;
class CFilterComboBox;
class CGameFileTreeView;


#endif // DMECONTROLS_H
