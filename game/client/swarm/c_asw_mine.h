#ifndef _INCLUDED_C_ASW_MINE_H
#define _INCLUDED_C_ASW_MINE_H

#include "c_basecombatcharacter.h"

struct dlight_t;

class C_ASW_Mine : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Mine, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Mine();
	virtual			~C_ASW_Mine();
	void ClientThink(void);
	void OnDataChanged(DataUpdateType_t updateType);

	CNetworkVar( bool, m_bMineTriggered );

private:
	C_ASW_Mine( const C_ASW_Mine & ); // not defined, not accessible
};

#endif // _INCLUDED_C_ASW_MINE_H