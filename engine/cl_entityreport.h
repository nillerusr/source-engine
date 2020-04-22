//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( CL_ENTITYREPORT_H )
#define CL_ENTITYREPORT_H
#ifdef _WIN32
#pragma once
#endif

// Bits needed for action
void CL_RecordEntityBits( int entnum, int bitcount );

// Special events to record
void CL_RecordAddEntity( int entnum );
void CL_RecordLeavePVS( int entnum );
void CL_RecordDeleteEntity( int entnum, ClientClass *pclass );

extern ConVar cl_entityreport;

#endif // CL_ENTITYREPORT_H
