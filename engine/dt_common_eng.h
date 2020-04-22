//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DT_COMMON_ENG_H
#define DT_COMMON_ENG_H
#ifdef _WIN32
#pragma once
#endif

class CBaseClientState;
class ServerClass;

// For shortcutting when server and client have the same game .dll
//  data
void DataTable_CreateClientTablesFromServerTables();
void DataTable_CreateClientClassInfosFromServerClasses( CBaseClientState *pState );

void DataTable_ClearWriteFlags( ServerClass *pClasses );
bool DataTable_LoadDataTablesFromBuffer( bf_read *pBuf, int nDemoProtocol );

void DataTable_WriteSendTablesBuffer( ServerClass *pClasses, bf_write *pBuf );
void DataTable_WriteClassInfosBuffer(ServerClass *pClasses, bf_write *pBuf );

bool DataTable_SetupReceiveTableFromSendTable( SendTable *sendTable, bool bNeedsDecoder );

#endif // DT_COMMON_ENG_H
