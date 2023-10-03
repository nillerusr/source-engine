#ifndef _INCLUDED_ASW_INPUT_H
#define _INCLUDED_ASW_INPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "input.h"
#include "glow_outline_effect.h"

extern bool MarineBusy();

class CASW_Player;
class C_ASW_Player;
class IASW_Client_Aim_Target;

// storing autoaim data for debug visualisation
void ASW_StoreLineCircle(int index, float alien_x, float alien_y, float alien_radius, float marine_x, float marine_y, Vector2D LineDir, int iCol);
void ASW_GetLineCircle(int index, float &alien_x, float &alien_y, float &alien_radius, float &marine_x, float &marine_y, Vector2D &LineDir, int &iCol);
void ASW_StoreClearAll();

bool MarineControllingTurret();

bool HUDTraceToWorld(float screenx, float screeny, Vector &HitLocation, bool bUseMarineHull=false);
C_BaseEntity* HUDToWorld(float screenx, float screeny,
	Vector &HitLocation, IASW_Client_Aim_Target* &pAutoAimEnt, bool bPreferFlatAiming=false, bool bIgnoreCursorPosition = false, float flForwardMove = 0.0f, float flSideMove = 0.0f);
void RoundToPixel(Vector &vecPos);
void SmoothTurningYaw(CASW_Player *pPlayer, float &yaw);
void SmoothControllerYaw(CASW_Player *pPlayer, float &yaw);

bool PlayerDriving();
void ASW_UpdateControllerCodes();

// finding key names (uses controller binds in controller mode, translates names)
const char* ASW_FindKeyBoundTo(const char *binding);
const char* MakeHumanReadable(const char *key);

bool ASW_TryGroundShooting();
void ASW_AdjustViewAngleForGroundShooting(QAngle &viewangles);

#define ASW_MAX_AIM_TRACE 3000.0f
#define ASW_MAX_AUTO_AIM_RANGE 2560000.0f			// 1600 squared

//-----------------------------------------------------------------------------
// Purpose: ASW Input interface
//-----------------------------------------------------------------------------
class CASWInput : public CInput, public IClientEntityListener
{
public:
	CASWInput();

	virtual void		ASW_GetWindowCenter( int&x, int& y )	{ GetWindowCenter(x, y); }
	virtual	void		GetFullscreenMousePos( int *mx, int *my, int *unclampedx = NULL, int *unclampedy = NULL );
	virtual void		GetSimulatedFullscreenMousePos( int *mx, int *my, int *unclampedx = 0, int *unclampedy = 0 );
	virtual void		GetSimulatedFullscreenMousePosFromController( int *mx, int *my, float fControllerPitch, float fControllerYaw, float flForwardFraction = 0.4f );
	void				TurnTowardMouse(QAngle& viewangles, CUserCmd *cmd);	// asw	

	void ComputeNewMarineFacing( C_ASW_Player *pPlayer, const Vector &HitLocation, C_BaseEntity *pHitEnt, IASW_Client_Aim_Target *pAutoAimEnt, bool bPreferFlatAiming, float *pPitch, Vector *pNewMarineFacing );
	void				TurnTowardController(QAngle& viewangles);	// asw	
	bool				IsAttacking();
	virtual void		ControllerMove( int nSlot, float frametime, CUserCmd *cmd );
	virtual bool		ControllerModeActive() { return m_bControllerMode; }
	virtual bool		JoyStickActive();
	virtual void		JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch );
	virtual void		JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove );
	virtual void		JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove );

	int m_LastMouseX, m_LastMouseY;
	EHANDLE m_hLastMarine;
	bool m_bDontTurnMarine;	// set when changing marines, so we don't turn them until the cursor moves
	float m_fJoypadPitch;		// up/down on analogue stick
	float m_fJoypadYaw;			// left/right on analogue stick
	float m_fJoypadFacingYaw;	// desired yaw for our marine
	bool m_bAutoAttacking;
	
	bool m_bCursorPlacement;	// set to true when the aiming joystick should be used like a mouse for skill placement, rather than robotron-style shooting
	int m_nRelativeCursorX, m_nRelativeCursorY;
	float m_flDesiredCursorRadius;
	float m_flTimeSinceLastTurn;

	virtual float ASW_GetCameraPitch( const float *pfDeathCamInterp = NULL );
	virtual float ASW_GetCameraYaw( const float *pfDeathCamInterp = NULL );
	virtual float ASW_GetCameraDist( const float *pfDeathCamInterp = NULL );
	void ASW_GetCameraLocation( C_ASW_Player *pPlayer, Vector &vecCameraLocation, QAngle &angCamera, int &nMouseX, int &nMouseY, bool bApplySmoothing );
	virtual int ASW_GetOrderingMarine() { return m_iOrderingMarine; }	// ent index of the current marine we're ordering around
	virtual void ASW_SetOrderingMarine(int iMarineEntIndex) { m_iOrderingMarine = iMarineEntIndex; }


	// asw_in_camera.cpp:
	virtual void CAM_Think( void );
	virtual void CAM_ToThirdPerson( void );
	virtual	int CAM_IsThirdPerson( int nSlot = -1 );
	virtual void CAM_ToFirstPerson( void );
	virtual void CAM_StartMouseMove( void );
	virtual void CAM_StartDistance( void );
	virtual void Init_Camera( void );

	// asw_in_mouse.cpp:
	virtual void ActivateMouse( void );
	virtual void ResetMouse( void );
	virtual void AccumulateMouse( int nSlot );
	virtual void ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y );

	// asw_in_main.cpp:
	virtual int KeyEvent( int down, ButtonCode_t code, const char *pszCurrentBinding );
	virtual void ExtraMouseSample( float frametime, bool active );
	virtual void CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual void Init_All( void );
	virtual int	 GetButtonBits( bool bResetState );

	// This is the world position of our crosshair.  It is usually raised from the floor to match the marine's gun height.
	//  If the marine is auto-aiming up/down, this position will be raised/lowered.
	const Vector& GetCrosshairAimingPos() { return m_vecCrosshairAimingPos; }

	// This is the world position of the floor/wall directly beneath the cursor (i.e. a ray traced from the camera through the crosshair into the world)
	const Vector& GetCrosshairTracePos() { return m_vecCrosshairTracePos; }

	// the entity we're targeting 
	void UpdateHighlightEntity();
	void SetHighlightEntity( C_BaseEntity* pEnt, bool bGlow );
	C_BaseEntity* GetHighlightEntity() const;

	// the entity that can be used by the local player
	void SetUseGlowEntity( C_BaseEntity* pEnt );
	C_BaseEntity* GetUseGlowEntity() { return m_hUseGlowEntity.Get(); }

	// the entity we're mousing over
	void SetMouseOverEntity( C_BaseEntity* pEnt );
	// the entity under our crosshair
	C_BaseEntity* GetMouseOverEntity() { return m_hMouseOverEntity.Get(); }

	// the entity our weapon is autoaiming at
	void SetAutoaimEntity( C_BaseEntity* pEnt ) { m_hAutoaimEnt = pEnt; }
	// the entity under our crosshair
	C_BaseEntity* GetAutoaimEntity() { return m_hAutoaimEnt.Get(); }
	
	// controller mode
	void SetControllerMode( bool bControllerMode );

	virtual void OnEntityDeleted( C_BaseEntity *pEntity );

	// Camera shift
	void SetCameraFixed( bool bFixed ) { m_bCameraFixed = bFixed; }

private:
	float		m_fCurrentCameraPitch;
	float		m_flCurrentCameraDist;
	Vector		m_vecCameraVelocity;
	float		m_fShiftFraction;
	bool		m_bCameraFixed;

	void CalculateCameraShift( C_ASW_Player *pPlayer, float flDeltaX, float flDeltaY, float &flShiftX, float &flShiftY );
	void SmoothCamera( C_ASW_Player *pPlayer, Vector &vecCameraLocation );

	virtual bool			ASWWriteVehicleMessage( bf_write *buf );	
	void EngageControllerMode();

	int m_iOrderingMarine;	// entindex of marine we're ordering around

	Vector m_vecCrosshairAimingPos;
	Vector m_vecCrosshairTracePos;
	EHANDLE m_hMouseOverEntity;
	bool m_bIsMouseOverEntFriendly;
	EHANDLE m_hHighlightEntity;
	EHANDLE m_hUseGlowEntity;
	EHANDLE m_hAutoaimEnt;

	//CGlowObject m_MouseOverGlowObject;
	CGlowObject m_HighLightGlowObject;
	CGlowObject m_UseGlowObject;

	bool m_bControllerMode;
};

extern CASWInput *ASWInput();

#endif // _INCLUDED_ASW_INPUT_H