//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/**
***  Copyright  (C) 1999-2001 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
**/

/*********************************************************************************
 * vtuneapi.h 03-21-2001
 * Intel Corporation
 *
 * This header file describes VTune api's which are exported by vtuneapi.dll.
 *
 * To use these api's, include this header file and either link with vtuneapi.lib or load
 * vtuneapi.dll at runtime.
 *
 * VTune API's
 * -----------
 *
 * VOID VTPause(void) and VOID VTResume(void)
 *
 * VTPause and VTResume pause or resume data collection during a VTune Sampling, Counter Monitor, or Callgraph activity.
 *
 * If VTPause is called while a VTune Sampling collection is active, a flag is set which
 * suspends collection of PC samples on the current machine. Collection of PC samples
 * can be resumed by calling VTResume which clears the flag. The overhead to set and clear the flag
 * is very low, so the VTPause and VTResume can be called at a high frequency.
 *
 * If VTPause is called while a VTune Callgraph collection is active, Callgraph data collection
 * is paused for the current process. Callgraph data collection for the current process can
 * be resumed by calling VTResume.
 *
 * If VTPause is called while a VTune Counter Monitor collection is active, Counter Monitor data collection
 * is paused. Counter Monitor data collection can be resumed by calling VTResume.
 *
 * VTPause and VTResume can be safely called when the Sampling, Counter Monitor, and Callgraph collectors are not active.
 * In this case, the VTPause and VTResume do nothing.
 *
 * Note:
 *
 * VTune Sampling, Counter Monitor, and Callgraph activities are typically started with the VTune application.
 * The VTune GUI allows Sampling, Counter Monitor, and Callgraph activities to be started in "Pause" mode
 * which suspends data collection until a VTResume is called.
 * Data collection can also be paused and resumed by the Pause/Resume button in the VTune GUI.
 * See VTune onlilne help for more details.
 *
\*********************************************************************************/

#ifndef _VTUNEAPI_H_
#define _VTUNEAPI_H_

#ifndef _XBOX
#include <windows.h>
#else
#include <XTL.h>
#endif //!_XBOX

#ifdef _XBOX
#define VTUNEAPI
#elif !defined(_VTUNEAPI_)
#define VTUNEAPI __declspec(dllimport)
#else
#define VTUNEAPI __declspec(dllexport)
#endif


#define VTUNEAPICALL __cdecl


#ifdef __cplusplus
extern "C" {
#endif   // __cplusplus


//
// Pause and Resume data collection during VTune PC Sampling and Callgraph sessions.
// The VTPause and VTResume api's effect
// both VTune PC Sampling and VTune Callgraph
//
VTUNEAPI
VOID VTUNEAPICALL VTPause(void);

VTUNEAPI
VOID VTUNEAPICALL VTResume(void);


//Preserve VtPauseSampling and VtResumeSampling for backward compatibility...
VTUNEAPI
void VTUNEAPICALL VTPauseSampling(void);

VTUNEAPI
void VTUNEAPICALL VTResumeSampling(void);

VTUNEAPI
void VTUNEAPICALL CMPause(void);

VTUNEAPI
void VTUNEAPICALL CMResume(void);

#ifdef __cplusplus
}
#endif   // __cplusplus

#endif  // _VTUNEAPI_H_


