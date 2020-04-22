//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================


#include "stdafx.h"
#include "netpacketpool.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


namespace GCSDK
{

CClassMemoryPool<CNetPacket> CNetPacketPool::sm_MemPoolNetPacket( k_cInitialNetworkBuffers, UTLMEMORYPOOL_GROW_FAST );

static const CThreadSafeMultiMemoryPool::MemPoolConfig_t s_MemPoolConfigAllocSize[] = {
	{ 32, 32 },
	{ 64, 32 },
	{ 96, 32 },
	{ 128, 32 },
	{ 256, 32 },
	{ k_cubMsgSizeSmall, 32 },
	{ 1312 /* sizeof( UDPRecvBuffer_t ) + some rounding */, 32 },
	{ 2048, 4 },
	{ 4096, 4 },
	{ 16384, 4 },
	{ 64*1024, 4 }
};
CThreadSafeMultiMemoryPool g_MemPoolMsg( s_MemPoolConfigAllocSize, Q_ARRAYSIZE(s_MemPoolConfigAllocSize), UTLMEMORYPOOL_GROW_FAST );

// hacky global
int g_cNetPacket = 0;

} // namespace GCSDK
