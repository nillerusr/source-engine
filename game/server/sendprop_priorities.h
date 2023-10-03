//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#ifndef SENDPROP_PRIORITIES_H
#define SENDPROP_PRIORITIES_H
#ifdef _WIN32
#pragma once
#endif


#define SENDPROP_SIMULATION_TIME_PRIORITY		0
#define SENDPROP_TICKBASE_PRIORITY				1

#define SENDPROP_LOCALPLAYER_ORIGINXY_PRIORITY	2
#define SENDPROP_PLAYER_EYE_ANGLES_PRIORITY		3
#define SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY	4

#define SENDPROP_PLAYER_VELOCITY_XY_PRIORITY	5
#define SENDPROP_PLAYER_VELOCITY_Z_PRIORITY		6

// Nonlocal players exclude local player origin X and Y, not vice-versa,
// so our props should come after the most frequent local player props
// so we don't eat up their prop index bits.
#define SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY	7
#define SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY	8

#define SENDPROP_CELL_INFO_PRIORITY				32		// SendProp priority for cell bits and x/y/z.


#endif // SENDPROP_PRIORITIES_H
