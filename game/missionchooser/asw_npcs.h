//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================
#ifndef ASW_NPCS_H
#define ASW_NPCS_H
#ifdef _WIN32
#pragma once
#endif

class CMapLayout;
class CLayoutSystem;

class CASWMissionChooserNPCs
{
public:
	static void InitFixedSpawns( CLayoutSystem *pLayoutSystem, CMapLayout *pLayout );
	static void PushEncountersApart( CMapLayout *pLayout );
};

#endif // ASW_NPCS_H

