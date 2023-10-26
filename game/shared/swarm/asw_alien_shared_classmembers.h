// This file gets included *inside* both C_ASW_Alien and CASW_Alien class definitions.
// thus you can have shared types inside the appropriate namespace.
// It's either this, or copy the types here into both client and server and 
// hope people remember to update the other when they touch each one.
// This seems safer.
#ifndef _INCLUDED_ASW_ALIEN_SHARED_CLASSMEMBERS_H
#define _INCLUDED_ASW_ALIEN_SHARED_CLASSMEMBERS_H

#ifdef _WIN32
#pragma once
#endif

/// information sent from the server to the client about
/// how this particular alien should act when it dies.
/// NOTE: if you add to this, then you must also
/// update the bit-count in every SendProp that 
/// transmits this type.
enum DeathStyle_t
{
	kDIE_TUMBLEGIB = 0, ///< ragdoll for a second, and then gib.
	kDIE_RAGDOLLFADE, ///< ragdoll for a bit and then fade.
	kDIE_INSTAGIB,  ///< gib instantly on death.
	kDIE_BREAKABLE, ///< break into ragdoll pieces
	kDIE_FANCY,		///< play a fancy death animation
	kDIE_HURL,		///< fling ragdoll towards the camera in a humorous fashion	
	kDIE_MELEE_THROW,		// throw a ragdoll away from the marine
	kDIE_MAX,		///< high water mark.

	// this isn't actually an enum constant, but
	// represents the number of bits needed to 
	// store all the values above. It gets used
	// to make SendProps, so you **MUST UPDATE**
	// this if you add something above.
	kDEATHSTYLE_NUM_TRANSMIT_BITS = 3, 
};

#endif // _INCLUDED_ASW_ALIEN_SHARED_CLASSMEMBERS_H