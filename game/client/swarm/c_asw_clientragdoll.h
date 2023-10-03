#ifndef C_ASW_CLIENTRAGDOLL_H
#define C_ASW_CLIENTRAGDOLL_H
#ifdef _WIN32
#pragma once
#endif

class C_ASW_ClientRagdoll : public C_ClientRagdoll
{

public:
	C_ASW_ClientRagdoll( bool bRestoring = true );
	DECLARE_CLASS( C_ASW_ClientRagdoll, C_ClientRagdoll );
	DECLARE_DATADESC();

	virtual void ClientThink( void );

	void BreakRagdoll();

	const Vector& GetDeathForce() { return m_vecForce; }
	
	float m_fASWGibTime;
	int m_iSourceEntityIndex;

	int m_nDeathStyle;

	bool m_bElectroShock;
	const char *pszGibParticleEffect;
	bool m_bHurled;
};


void ASWHurlRagdollAtCamera( C_ASW_ClientRagdoll * RESTRICT pEntity );
void ASWMeleeThrowRagdoll( C_ASW_ClientRagdoll * RESTRICT pEntity );

#endif // C_ASW_CLIENTRAGDOLL_H