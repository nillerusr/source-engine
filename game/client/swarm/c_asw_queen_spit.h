#ifndef _INCLUDED_C_ASW_QUEEN_SPIT_H
#define _INCLUDED_C_ASW_QUEEN_SPIT_H
#pragma once

class C_ASW_Emitter;

class C_ASW_Queen_Spit : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Queen_Spit, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	C_ASW_Queen_Spit();
	virtual ~C_ASW_Queen_Spit();

	void OnDataChanged( DataUpdateType_t updateType );	
	void OnRestore();
	void CreateEffects();	

	C_ASW_Emitter *m_pGooEmitter;
	bool m_bCreatedEffects;

private:
	C_ASW_Queen_Spit( const C_ASW_Queen_Spit & );
};

#endif // _INCLUDED_C_ASW_QUEEN_SPIT_H