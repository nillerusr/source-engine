#include "cbase.h"
#include "AI_Criteria.h"
#include "npc_bullseye.h"

class CInfoRemarkable : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoRemarkable, CPointEntity );

	DECLARE_DATADESC();

	void	Spawn( void );

	bool IsAlive() { return true; } // Required for NPCs to see this entity
	bool CanBeSeenBy( CAI_BaseNPC *pNPC ) { return true; } // allows entities to be 'invisible' to NPC senses.
		
	string_t GetContextSubject() { return m_bDisabled ? AllocPooledString( "disabled" ) : m_iszContextSubject; }
	void SetContextSubject( string_t contextSubject ) { m_iszContextSubject = contextSubject; }

	int GetTimesRemarked() const { return m_iTimesRemarked; }

	void OnRemarked() { m_iTimesRemarked++; m_flLastTimeRemarked = gpGlobals->curtime; }

	bool IsRemarkable() { return true; }

	Class_T Classify( void ) { return CLASS_BULLSEYE; } // Get around visibility checks

	AI_CriteriaSet& GetModifiers( CBaseEntity * pEntity );

	void	DrawDebugGeometryOverlays( void );

	// Input handlers
	virtual void InputEnable( inputdata_t &inputdata );
	virtual void InputDisable( inputdata_t &inputdata );

protected:
	string_t m_iszContextSubject;
	bool m_bDisabled;
	int m_iTimesRemarked;
	float m_flLastTimeRemarked = -1; // Currently only for debug purposes
};