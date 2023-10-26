// ai_addon.h

#ifndef AI_ADDON_H
#define AI_ADDON_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

#include "baseanimating.h"
#include "ai_agent.h"
#include "ai_behavior.h"
#include "player_pickup.h"
#include "IEffects.h"

class CAI_BaseNPC;

#define INVALID_ADDON_ATTACHMENT_ID	-1

int GetIDForAttachmentName( char *pszAttachmentName );
int GetNumAddOnAttachmentPoints();
bool IsAddOnAttachmentAvailable( CAI_BaseNPC *pHost, char *pszAttachmentName );
int CountAddOns( CAI_BaseNPC *pHost );

//=========================================================
//=========================================================
class CAI_AddOn : public CBaseAnimating, public CAI_Agent, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CAI_AddOn, CBaseAnimating );

	//---------------------------------
	// CTor/DTor
	//---------------------------------
	CAI_AddOn() { m_iAttachmentID = INVALID_ADDON_ATTACHMENT_ID; }

	//---------------------------------
	// Precache/Spawn/etc
	//---------------------------------
	virtual void Precache();
	virtual void Spawn();
	virtual void UpdateOnRemove();

	const char *GetDebugName()	{ return CBaseEntity::GetDebugName(); }
	int entindex()				{ return CBaseEntity::entindex(); }

	virtual int			DrawDebugTextOverlays(void);

	//---------------------------------
	//---------------------------------
	bool SUB_AllowedToFade( void )
	{
		return true;
	}

	int Save( ISave &save )
	{
		int result = CBaseAnimating::Save( save );
		if ( result )
		{
			result = CAI_Agent::Save( save );
		}
		return result;
	}

	//-------------------------------------

	int Restore( IRestore &restore )
	{
		int result = CBaseAnimating::Restore( restore );
		if ( result )
		{
			result = CAI_Agent::Restore( restore );
		}
		return result;
	}

	//---------------------------------
	// Agent stuff
	//---------------------------------
	virtual void		GatherConditions();
	virtual int			SelectSchedule();
	virtual void		StartTask( const Task_t *pTask );
	virtual void		RunTask( const Task_t *pTask );

	//---------------------------------
	// Install/Remove AddOns
	//---------------------------------
	virtual bool Install( CAI_BaseNPC *pHost, bool bRemoveOnFail = true );
	void SetPhysReplacement( CBaseEntity *pEntity );
	bool Attach( CAI_BaseNPC *pHost );
	void Dettach( void );
	virtual void Remove() { Unbind(); Dettach(); }
	virtual void Bind() {}
	virtual void Unbind() {}

	//---------------------------------
	// Hosts/Outer accessors
	//---------------------------------
	CAI_BaseNPC *GetNPCHost();
	CBaseEntity *GetHostEnemy();

	//---------------------------------
	// Thinking
	//---------------------------------
	void DispatchAddOnThink();

	virtual float GetThinkInterval() { return 0.1f; }

	//---------------------------------
	// Appearance & Position
	//---------------------------------
	virtual void PickAttachment( CAI_BaseNPC *pHost, char *pchAttachment ) = 0;
	virtual char *GetAddOnModelName() = 0;
	virtual Vector GetAttachOffset( QAngle &attachmentAngles )			{ return vec3_origin; }
	virtual QAngle GetAttachOrientation( QAngle &attachmentAngles )		{ return attachmentAngles; }
	virtual QAngle GetLocalOrientation( void );
	int GetAttachmentID() { return m_iAttachmentID; }
	virtual void EjectFromHost();

	//---------------------------------
	// Entity I/O
	//---------------------------------
	void InputInstall( inputdata_t &data );
	void InputRemove( inputdata_t &data );

	//---------------------------------
	// Schedule/Task/Conditions
	//---------------------------------
	enum
	{
		TASK_ADDON_WAIT = NEXT_TASK,
		TASK_ADDON_WAIT_RANDOM,
		NEXT_TASK,
	};

	enum
	{
		COND_ADDON_LOST_HOST = NEXT_CONDITION,
		NEXT_CONDITION,
	};

	enum
	{
		SCHED_ADDON_NO_OWNER = NEXT_SCHEDULE,
		NEXT_SCHEDULE,
	};

	DECLARE_DATADESC();

protected:
	CHandle<CAI_BaseNPC>	m_hNPCHost;
	CHandle<CBaseEntity>	m_hPhysReplacement;
	int						m_iPhysReplacementSolidFlags;
	int						m_iPhysReplacementMoveType;
	QAngle					m_angPhysReplacementLocalOrientation;
	Vector					m_vecPhysReplacementDetatchForce;

	bool	m_bWasAttached;

private:
	//---------------------------------
	// Fields used by AI
	//---------------------------------
	float	m_flWaitFinished;	// Same as for AI_BaseNPC
	int		m_iAttachmentID;	// Which attachment point am I connected to on my host's model?
	float 	m_flNextAttachTime;


public:
	//---------------------------------
	// Hammer keyfields
	//---------------------------------

	DEFINE_AGENT();
};

//=========================================================
//=========================================================

class CAI_AddOnBehaviorBase : public CAI_SimpleBehavior
{
public:
	virtual bool ShouldNPCSave() 
	{ 
		return false; 
	}

	virtual CAI_AddOn **GetAddOnsBase() { return NULL; }
	const CAI_AddOn **GetAddOnsBase() const { return (const CAI_AddOn **)const_cast<CAI_AddOnBehaviorBase *>(this)->GetAddOnsBase(); }
	virtual int NumAddOns() const { return 0; }
};


template <class ADDON>
class CAI_AddOnBehavior : public CAI_AddOnBehaviorBase
{
public:
	virtual int BindAddOn( ADDON *pAddOn )	
	{ 
		Assert( m_AddOns.Find( pAddOn ) == m_AddOns.InvalidIndex() );
		Assert( static_cast<CAI_AddOn *>(pAddOn) ); // Satic cast is used to trap improper inheritance
		m_AddOns.AddToTail( pAddOn );
		return m_AddOns.Count(); 
	}

	virtual int UnbindAddOn( ADDON *pAddOn ) 
	{ 
		Assert( m_AddOns.Find( pAddOn ) != m_AddOns.InvalidIndex() );
		m_AddOns.FindAndRemove( pAddOn );
		return m_AddOns.Count(); 
	}

	virtual CAI_AddOn **GetAddOnsBase() 
	{ 
		return (CAI_AddOn **)m_AddOns.Base(); 
	}

	virtual ADDON **GetAddOns() 
	{ 
		return m_AddOns.Base(); 
	}

	virtual int NumAddOns() const
	{ 
		return m_AddOns.Count(); 
	}

protected:
	CUtlVector<ADDON *> m_AddOns;
};


//=========================================================
//=========================================================
template <class ADDON, class BEHAVIOR>
class CAI_AddOnBehaviorConnector : public ADDON
{
	DECLARE_CLASS( CAI_AddOnBehaviorConnector, ADDON );

	virtual int ObjectCaps( void )
	{
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );

		if ( pPlayer == NULL )
			return;

		if ( this->m_bWasAttached )
		{
			this->Remove();

			g_pEffects->Sparks( this->GetAbsOrigin(), 2, 2 );
		}

		pPlayer->PickupObject( this );
	}

	virtual void Bind()
	{
		if ( this->m_hNPCHost )
		{
			BEHAVIOR *pBehavior;
			if ( !this->m_hNPCHost->GetBehavior( &pBehavior ) )
			{
				pBehavior =  new BEHAVIOR;
				this->m_hNPCHost->AddBehavior( pBehavior );
			}
			pBehavior->BindAddOn( this );
		}
	}

	virtual void Unbind()
	{
		if ( this->m_hNPCHost )
		{
			BEHAVIOR *pBehavior;
			if ( this->m_hNPCHost->GetBehavior( &pBehavior ) )
			{
				if ( pBehavior->UnbindAddOn( this ) == 0 )
				{
					this->m_hNPCHost->RemoveAndDestroyBehavior( pBehavior );
				}
			}
		}
	}

	//-------------------------------------

	int Save( ISave &save )
	{
		int result = BaseClass::Save( save );
		if ( result )
		{
			bool bSaved = false;
			BEHAVIOR *pBehavior;
			if ( this->m_hNPCHost && this->m_hNPCHost->GetBehavior( &pBehavior ) )
			{
				bSaved = true;
				CAI_BehaviorBase::SaveBehaviors( save, pBehavior, (CAI_BehaviorBase **)&pBehavior, 1, false );
			}

			if ( !bSaved )
			{
				CAI_BehaviorBase::SaveBehaviors( save, NULL, NULL, 0 );
			}
		}

		return result;
	}

	//-------------------------------------

	int Restore( IRestore &restore )
	{
		int result = BaseClass::Restore( restore );
		if ( result )
		{
			Bind();
			BEHAVIOR *pBehavior;
			if ( this->m_hNPCHost && this->m_hNPCHost->GetBehavior( &pBehavior ) )
			{
				CAI_BehaviorBase::RestoreBehaviors( restore, (CAI_BehaviorBase **)&pBehavior, 1, false );
			}
			else
			{
				CAI_BehaviorBase::RestoreBehaviors( restore, NULL, 0,false );
			}
		}
		return result;
	}

};

#define LINK_ENTITY_TO_ADDON_AND_BEHAVIOR( mapClassName, DLLClassName, BehaviorName ) \
	typedef CAI_AddOnBehaviorConnector<DLLClassName, BehaviorName> DLLClassName##_##BehaviorName##_Binder; \
	LINK_ENTITY_TO_CLASS( mapClassName, DLLClassName##_##BehaviorName##_Binder )

#endif // AI_ADDON_H
