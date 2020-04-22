//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clients CBaseObject
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_BASEOBJECT_H
#define C_BASEOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseobject_shared.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include "vgui_healthbar.h"
#include "commanderoverlay.h"
#include "hud_minimap.h"
#include "particlemgr.h"
#include "particle_prototype.h"
#include "particle_util.h"
#include "c_basecombatcharacter.h"
#include "ihasbuildpoints.h"

class C_BaseTFPlayer;

// Max Length of ID Strings
#define MAX_ID_STRING		256

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_BaseObject : public C_BaseCombatCharacter, public IHasBuildPoints
{
	DECLARE_CLASS( C_BaseObject, C_BaseCombatCharacter );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL();

	C_BaseObject();
	~C_BaseObject( void );

	virtual bool	IsBaseObject( void ) const { return true; }
	virtual bool	IsAnUpgrade(void ) const { return false; }
	virtual bool	IsAVehicle( void ) const { return false; }

	virtual void	SetType( int iObjectType );

	virtual void	AddEntity();
	virtual void	Select( void );

	void			SetActivity( Activity act );
	Activity		GetActivity( ) const;
	void			SetObjectSequence( int sequence );
	virtual void	OnActivityChanged( Activity act );

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	Release( void );

	virtual int		GetHealth() const { return m_iHealth; }
	void			SetHealth( int health ) { m_iHealth = health; }
	virtual int		GetMaxHealth() const { return m_iMaxHealth; }
	int				GetObjectFlags( void ) { return m_fObjectFlags; }
	void			SetObjectFlags( int flags ) { m_fObjectFlags = flags; }

	// Derive to customize an object's attached version
	virtual	void	SetupAttachedVersion( void ) { return; }
	virtual void	SetupUnattachedVersion( void ) { return; }
	virtual void	OnLostPower( void ) { return; };

	virtual const char	*GetTargetDescription( void ) const;
	virtual char	*GetIDString( void );
	virtual bool	IsValidIDTarget( void );

	void			AttemptToGoActive( void );
	virtual bool	ShouldBeActive( void );
	virtual void	OnGoActive( void );
	virtual void	OnGoInactive( void );

	virtual void	SetDormant( bool bDormant );

	void			SendClientCommand( const char *pCmd );

	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	// Builder preview...
	void			ActivateYawPreview( bool enable );
	void			PreviewYaw( float yaw );
	bool			IsPreviewingYaw() const;

	// This is called to get the initial builder yaw...
	virtual float	GetInitialBuilderYaw();
	
	virtual void	RecalculateIDString( void );

	int GetType() const { return m_iObjectType; }
	bool IsOwnedByLocalPlayer() const;
	C_BaseTFPlayer *GetOwner();

	// Are we under attack?
	bool			IsUnderAttack( );

	virtual int		DrawModel( int flags );
	virtual bool	IsIdentityBrush( void );

	// Effects
	void			DrawRunningEffects( void );
	void			DrawBuildEffects( void );
	void			DrawDamageEffects( void );

	// Deterioration
	bool			IsDeteriorating( void ) { return m_bDeteriorating; };

	float			GetPercentageConstructed( void ) { return m_flPercentageConstructed; }

	bool			IsPlacing( void ) const { return m_bPlacing; }
	bool			IsBuilding( void ) const { return m_bBuilding; }

	virtual void	FinishedBuilding( void ) { return; }

	virtual bool	OffsetObjectOrigin( Vector& origin );

	virtual const char* GetStatusName() const;

	// Object Previews
	void			HighlightBuildPoints( int flags );

public:
	// Client/Server shared build point code
	void				CreateBuildPoints( void );
	void				AddAndParseBuildPoint( int iAttachmentNumber, KeyValues *pkvBuildPoint );
	virtual int			AddBuildPoint( int iAttachmentNum );
	virtual void		AddValidObjectToBuildPoint( int iPoint, int iObjectType );
	virtual CBaseObject *GetBuildPointObject( int iPoint );
	bool				IsBuiltOnAttachment( void ) { return (m_hBuiltOnEntity != NULL); }
	void				AttachObjectToObject( CBaseEntity *pEntity, int iPoint, Vector &vecOrigin );
	CBaseObject			*GetParentObject( void );
	void				SetBuildPointPassenger( int iPoint, int iPassenger );
	int					GetBuildPointPassenger( int iPoint ) const;

	// Build points
	CUtlVector<BuildPoint_t>	m_BuildPoints;

	// Power
	bool				IsPowered( void );
	virtual bool		CanPowerupEver( int iPowerup );
	virtual bool		CanPowerupNow( int iPowerup );

	bool				IsDisabled( void ) { return m_bDisabled; }

	virtual float		GetSapperAttachTime( void );

// IHasBuildPoints
public:
	virtual int			GetNumBuildPoints( void ) const;
	virtual bool		GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );
	virtual int			GetBuildPointAttachmentIndex( int iPoint ) const;
	virtual bool		CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );
	virtual void		SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );
	virtual float		GetMaxSnapDistance( int iBuildPoint );
	virtual bool		ShouldCheckForMovement( void ) { return true; }
	virtual int			GetNumObjectsOnMe( void );
	virtual CBaseEntity	*GetFirstObjectOnMe( void );
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );
	virtual void		RemoveAllObjects( void );
	virtual int			FindObjectOnBuildPoint( CBaseObject *pObject );
 	virtual void		GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles );

	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

protected:
	char			m_szIDString[ MAX_ID_STRING ];

private:
	enum
	{
		YAW_PREVIEW_OFF	= 0,
		YAW_PREVIEW_ON,
		YAW_PREVIEW_WAITING_FOR_UPDATE
	};

	Activity		m_Activity;

	int				m_fObjectFlags;
	float			m_fYawPreview;
	char			m_YawPreviewState;
	CHandle< C_BaseTFPlayer >	m_hOldOwner;
	CHandle< C_BaseTFPlayer > m_hBuilder;
	bool			m_bWasActive;
	int				m_iOldHealth;
	bool			m_bHasSapper;
	bool			m_bOldSapper;
	int				m_iObjectType;
	int				m_iHealth;
	int				m_iMaxHealth;
	bool			m_bWasBuilding;
	bool			m_bBuilding;
	bool			m_bPlacing;
	bool			m_bDeteriorating;
	bool			m_bDisabled;
	float			m_flPercentageConstructed;
	EHANDLE			m_hBuiltOnEntity;

	CHealthBarPanel	*m_pHealthBar;
	vgui::Label		*m_pNameLabel;
	float			m_flDamageFlash;		// Used to flash the panel when the object takes damage
	int				m_iFlashes;

	float				m_flAttackTime;
	CMaterialReference	m_ThermalMaterial;

	// Effects
	float			m_flNextEffect;

private:
	C_BaseObject( const C_BaseObject & ); // not defined, not accessible
};

#endif // C_BASEOBJECT_H
