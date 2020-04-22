//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_obj_armor_upgrade.h"


IMPLEMENT_CLIENTCLASS_DT(C_ArmorUpgrade, DT_ArmorUpgrade, CArmorUpgrade)
END_RECV_TABLE()



C_ArmorUpgrade::C_ArmorUpgrade()
{
}


int C_ArmorUpgrade::DrawModel( int flags )
{
	C_BaseEntity *pParent = GetMoveParent();
	if ( pParent )
	{
		C_BaseAnimating *pAnimating = dynamic_cast< C_BaseAnimating* >( pParent );
		if ( pAnimating )
		{
			SetModelPointer( pParent->GetModel() );
			SetSequence( pAnimating->GetSequence() );
			m_nSkin = pAnimating->m_nSkin;
			m_nBody = pAnimating->m_nBody;

			SetLocalOrigin( Vector( 0, 0, 50 ) );
			SetLocalAngles( QAngle( 0, 0, 0 ) );
			InvalidateBoneCache();
		}
	}

	return BaseClass::DrawModel( 0 );
}


