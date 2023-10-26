#include "cbase.h"
#include "asw_prop_dynamic.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Remap old prop physics entities to infested versions too
LINK_ENTITY_TO_CLASS( dynamic_prop, CASW_Prop_Dynamic );
LINK_ENTITY_TO_CLASS( prop_dynamic, CASW_Prop_Dynamic );	
LINK_ENTITY_TO_CLASS( prop_dynamic_override, CASW_Prop_Dynamic );	

int CASW_Prop_Dynamic::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	// check for notifying a marine that he's shooting a breakable prop
	if ( inputInfo.GetAttacker() && inputInfo.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(inputInfo.GetAttacker());
		if (pMarine)
		{
			pMarine->HurtJunkItem(this, inputInfo);
		}
	}

	return BaseClass::OnTakeDamage(inputInfo);
}