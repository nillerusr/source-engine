#include "cbase.h"
#include "c_asw_client_corpse.h"
#include "c_asw_clientragdoll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Client_Corpse, DT_ASW_Client_Corpse, CASW_Client_Corpse)

END_RECV_TABLE()

C_ASW_Client_Corpse::C_ASW_Client_Corpse()
{
}

typedef CHandle<C_ASW_ClientRagdoll> ClientRagdollHandle_t;
CUtlVector<ClientRagdollHandle_t>	g_ClientRagdolls;

bool AlreadyCreatedRagdollFor(int iEntityIndex)
{
	// ASWTODO - put this back if m_iSourceEntityIndex is added to ragdolls again
	int c = g_ClientRagdolls.Count();
	for (int i=c-1;i>=0;i--)
	{
		C_ASW_ClientRagdoll *pRagdoll = g_ClientRagdolls[i].Get();
		if (!pRagdoll)
		{
			g_ClientRagdolls.Remove(i);
			continue;
		}
		if (pRagdoll->m_iSourceEntityIndex == iEntityIndex)
			return true;
	}
	return false;
}

C_ClientRagdoll *C_ASW_Client_Corpse::CreateClientRagdoll( bool bRestoring )
{
	return new C_ASW_ClientRagdoll( bRestoring );
}

C_BaseAnimating* C_ASW_Client_Corpse::BecomeRagdollOnClient( void )
{
	if ( AlreadyCreatedRagdollFor( entindex() ) )
	{
		//m_builtRagdoll = true;
		return NULL;
	}
	C_BaseAnimating *pAnim = BaseClass::BecomeRagdollOnClient();

	C_ASW_ClientRagdoll *pClientRagdoll = dynamic_cast<C_ASW_ClientRagdoll*>(pAnim);
	if (pClientRagdoll)
	{
		pClientRagdoll->m_iSourceEntityIndex = entindex();
		g_ClientRagdolls.AddToTail(pClientRagdoll);
	}
	else
	{
		Msg("Error: failed to cast client corpse into a client ragdoll!\n");
	}

	// turn off collision callbacks on this ragdoll, so it doesn't make noises falling to the ground on creation
	// todo: turn the callbacks back on again after we've settled?
	if (pAnim && pAnim->m_pRagdoll)
	{
		ragdoll_t *pRagdollT = pAnim->m_pRagdoll->GetRagdoll();
		if (pRagdollT)
		{
			for ( int i = 0; i < pRagdollT->listCount; i++ )
			{
				const ragdollelement_t &element = pRagdollT->list[i];
				if ( element.pObject )
				{
					element.pObject->SetCallbackFlags(element.pObject->GetCallbackFlags() & ~CALLBACK_GLOBAL_COLLISION);
				}
			}
		}
	}
	return pAnim;
}