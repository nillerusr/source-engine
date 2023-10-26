//======= Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Rumble effects mixer for XBox
//
// $NoKeywords: $
//
//=============================================================================//
#pragma once
#ifndef C_RUMBLE_H
#define C_RUMBLE_H

extern void RumbleEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags );
extern void UpdateRumbleEffects( int userID );
extern void UpdateScreenShakeRumble( int userID, float shake, float balance = 0 );
extern void EnableRumbleOutput( int userID, bool bEnable );
extern void StopAllRumbleEffects( int userID );

#endif//C_RUMBLE_H

