//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_ENTS_PARSE_H
#define CL_ENTS_PARSE_H
#ifdef _WIN32
#pragma once
#endif

class CEntityReadInfo;		

void CL_DeleteDLLEntity( int iEnt, const char *reason, bool bOnRecreatingAllEntities = false );
void CL_CopyExistingEntity( CEntityReadInfo &u );
void CL_PreserveExistingEntity( int nOldEntity );
void CL_CopyNewEntity( CEntityReadInfo &u, int iClass, int iSerialNum );
void CL_PreprocessEntities( void );
bool CL_ProcessPacketEntities ( SVC_PacketEntities * entmsg );


#endif // CL_ENTS_PARSE_H
