//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Function to call ShellExecute on the .exe version of the current executable
// intended for a .com/.exe pair named the same thing to enable a hybrid
// console/gui app
//
//=============================================================================

#ifndef RUN_EXE_H
#define RUN_EXE_H

#if COMPILER_MSVC
#pragma once
#endif

void RunExe();

#endif // RUN_EXE_H
