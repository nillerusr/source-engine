//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: interface and implementation of PID.  
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef PID_H
#define PID_H
#ifdef WIN32
#pragma once
#pragma warning(disable:4786)
#endif
typedef unsigned long PID;
#include <map>
extern std::map<int,PID> pidMap;
#endif // PID_H
