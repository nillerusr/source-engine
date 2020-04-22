//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_commando.h"
#include "c_tf_class_defender.h"
#include "c_tf_class_escort.h"
#include "c_tf_class_infiltrator.h"
#include "c_tf_class_medic.h"
#include "c_tf_class_recon.h"
#include "c_tf_class_sniper.h"
#include "c_tf_class_support.h"
#include "c_tf_class_sapper.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClass::C_PlayerClass( C_BaseTFPlayer *pPlayer )
{
	// Save peer.
	m_pPlayer = pPlayer;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClass::~C_PlayerClass()
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClass *C_PlayerClass::Create( C_BaseTFPlayer *pPlayer, int iClassType )
{
	// Create the class type
	switch ( iClassType )
	{
	case TFCLASS_COMMANDO: { return ( new C_PlayerClassCommando( pPlayer ) ); }
	case TFCLASS_DEFENDER: { return ( new C_PlayerClassDefender( pPlayer ) ); }
	case TFCLASS_ESCORT: { return ( new C_PlayerClassEscort( pPlayer ) ); }
	case TFCLASS_INFILTRATOR:  { return ( new C_PlayerClassInfiltrator( pPlayer ) ); }
	case TFCLASS_MEDIC: { return ( new C_PlayerClassMedic( pPlayer ) ); }
	case TFCLASS_RECON: { return ( new C_PlayerClassRecon( pPlayer ) ); }
	case TFCLASS_SNIPER:  { return ( new C_PlayerClassSniper( pPlayer ) ); }
	case TFCLASS_SUPPORT:  { return ( new C_PlayerClassSupport( pPlayer ) ); }
	case TFCLASS_SAPPER:  { return ( new C_PlayerClassSapper( pPlayer ) ); }
	default: { return NULL; }
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_PlayerClass::Destroy( C_PlayerClass *pPlayerClass )
{
	if ( pPlayerClass )
	{
		delete pPlayerClass;
	}
}
