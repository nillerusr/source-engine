//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_ENGINEER_H
#define TFC_ENGINEER_H
#ifdef _WIN32
#pragma once
#endif


class CTFCPlayer;


void DestroyBuilding(CTFCPlayer *eng, char *bld);
void DestroyTeleporter(CTFCPlayer *eng, int type);


#endif // TFC_ENGINEER_H
