//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A blank body part
//
//===========================================================================//

#ifndef DMEBLANKBODYPART_H
#define DMEBLANKBODYPART_H


#ifdef _WIN32
#pragma once
#endif


#include "mdlobjects/dmebodypart.h"


//-----------------------------------------------------------------------------
// A blank body part
//-----------------------------------------------------------------------------
class CDmeBlankBodyPart : public CDmeBodyPart
{
	DEFINE_ELEMENT( CDmeBlankBodyPart, CDmeBodyPart );
};


#endif // DMEBLANKBODYPART_H
