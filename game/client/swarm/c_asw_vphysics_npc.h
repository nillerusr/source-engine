#ifndef _INCLUDED_C_ASW_VPHYSICS_NPC_H
#define _INCLUDED_C_ASW_VPHYSICS_NPC_H
#include "c_ai_basenpc.h"
#ifdef WIN_32
#pragma once
#endif

class C_ASW_VPhysics_NPC : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_ASW_VPhysics_NPC, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_VPhysics_NPC();
	virtual			~C_ASW_VPhysics_NPC();

private:
	C_ASW_VPhysics_NPC( const C_ASW_VPhysics_NPC & ); // not defined, not accessible
};
#endif // _INCLUDED_C_ASW_VPHYSICS_NPC_H