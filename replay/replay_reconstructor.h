//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAY_RECONSTRUCTOR_H
#define REPLAY_RECONSTRUCTOR_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

class CReplay;

//----------------------------------------------------------------------------------------

bool Replay_Reconstruct( CReplay *pReplay, bool bDeleteBlocks = true );

//----------------------------------------------------------------------------------------

#endif // REPLAY_RECONSTRUCTOR_H
