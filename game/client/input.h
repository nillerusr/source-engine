//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#if !defined( INPUT_H )
#define INPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "iinput.h"
#include "mathlib/vector.h"
#include "kbutton.h"
#include "ehandle.h"
#include "inputsystem/AnalogCode.h"
#include "shareddefs.h"

typedef unsigned long CRC32_t;

// TrackIR
extern QAngle	g_angleCenter;
// TrackIR

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CKeyboardKey
{
public:
	// Name for key
	char				name[ 32 ];
	// Pointer to the underlying structure
	kbutton_t			*pkey;
	// Next key in key list.
	CKeyboardKey		*next;
};

class ConVar;

class CVerifiedUserCmd
{
public:
	CUserCmd	m_cmd;
	CRC32_t		m_crc;
};

class CInput : public ::IInput
{
// Interface
public:
							CInput( void );
							~CInput( void );

	virtual		void		Init_All( void );
	virtual		void		Shutdown_All( void );
	virtual		int			GetButtonBits( bool bResetState );
	virtual		void		CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual		void		ExtraMouseSample( float frametime, bool active );
	virtual		bool		WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand );
	virtual		void		EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot );
	virtual		void		DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot );

	virtual		CUserCmd	*GetUserCmd( int nSlot, int sequence_number );

	virtual		void		MakeWeaponSelection( C_BaseCombatWeapon *weapon );

	virtual		float		KeyState( kbutton_t *key );
	virtual		int			KeyEvent( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual		kbutton_t	*FindKey( const char *name );

	virtual		void		ControllerCommands( void );
	virtual		void		Joystick_Advanced( bool bSilent );
	virtual		void		Joystick_SetSampleTime(float frametime);
	virtual		void		IN_SetSampleTime( float frametime );

	virtual		void		AccumulateMouse( int nSlot );
	virtual		void		ActivateMouse( void );
	virtual		void		DeactivateMouse( void );

	virtual		void		ClearStates( void );
	virtual		float		GetLookSpring( void );

	virtual		void		GetFullscreenMousePos( int *mx, int *my, int *unclampedx = NULL, int *unclampedy = NULL );
	virtual		void		SetFullscreenMousePos( int mx, int my );
	virtual		void		ResetMouse( void );

//	virtual		bool		IsNoClipping( void );
	virtual		float		GetLastForwardMove( void );
	virtual		void		ClearInputButton( int bits );

	virtual		void		CAM_Think( void );
	virtual		int			CAM_IsThirdPerson( int nSlot = -1 );
	virtual		void		CAM_GetCameraOffset( Vector& ofs );
	virtual		void		CAM_ToThirdPerson(void);
	virtual		void		CAM_ToFirstPerson(void);
	virtual		void		CAM_ToThirdPersonShoulder(void);
	virtual		void		CAM_StartMouseMove(void);
	virtual		void		CAM_EndMouseMove(void);
	virtual		void		CAM_StartDistance(void);
	virtual		void		CAM_EndDistance(void);
	virtual		int			CAM_InterceptingMouse( void );
	virtual		void		CAM_Command( int command );

	// orthographic camera info
	virtual		void		CAM_ToOrthographic();
	virtual		bool		CAM_IsOrthographic() const;
	virtual		void		CAM_OrthographicSize( float& w, float& h ) const;
	
#if defined( HL2_CLIENT_DLL )
	// IK back channel info
	virtual		void		AddIKGroundContactInfo( int entindex, float minheight, float maxheight );
#endif
	virtual		void		LevelInit( void );

	virtual		void		CAM_SetCameraThirdData( CameraThirdData_t *pCameraData, const QAngle &vecCameraOffset );
	virtual		void		CAM_CameraThirdThink( void );	

	virtual		void		CheckPaused( CUserCmd *cmd );
	virtual		void		CheckSplitScreenMimic( int nSlot, CUserCmd *cmd, CUserCmd *pPlayer0Command );

// Private Implementation
protected:
	// Implementation specific initialization
	virtual void		Init_Camera( void );
	void		Init_Keyboard( void );
	void		Init_Mouse( void );
	void		Shutdown_Keyboard( void );
	// Add a named key to the list queryable by the engine
	void		AddKeyButton( const char *name, kbutton_t *pkb );
	// Mouse/keyboard movement input helpers
	void		ScaleMovements( CUserCmd *cmd );
	void		ComputeForwardMove( int nSlot, CUserCmd *cmd );
	void		ComputeUpwardMove( int nSlot, CUserCmd *cmd );
	void		ComputeSideMove( int nSlot, CUserCmd *cmd );
	void		AdjustAngles ( int nSlot, float frametime );
	void		ClampAngles( QAngle& viewangles );
	void		AdjustPitch( int nSlot, float speed, QAngle& viewangles );
	void		AdjustYaw( int nSlot, float speed, QAngle& viewangles );
	float		DetermineKeySpeed( int nSlot, float frametime );
	void		GetAccumulatedMouseDeltasAndResetAccumulators( int nSlot, float *mx, float *my );
	void		GetMouseDelta( int nSlot, float inmousex, float inmousey, float *pOutMouseX, float *pOutMouseY );
	void		ScaleMouse( int nSlot, float *x, float *y );
	virtual void		ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y );
	void		MouseMove ( int nSlot, CUserCmd *cmd );

	// Joystick  movement input helpers
	void			ControllerMove( int nSlot, float frametime, CUserCmd *cmd );	
	float			ScaleAxisValue( const float axisValue, const float axisThreshold );
	virtual void	JoyStickMove( float frametime, CUserCmd *cmd );

	virtual bool	ControllerModeActive( void );
	virtual bool	JoyStickActive();
	virtual void	JoyStickSampleAxes( float &forward, float &side, float &pitch, float &yaw, bool &bAbsoluteYaw, bool &bAbsolutePitch );
	virtual void	JoyStickThirdPersonPlatformer( CUserCmd *cmd, float &forward, float &side, float &pitch, float &yaw );
	virtual void	JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch );
	virtual void	JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove );
	virtual void	JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove );

	// Call this to get the cursor position. The call will be logged in the VCR file if there is one.
	void		GetMousePos(int &x, int &y);
	void		SetMousePos(int x, int y);
	virtual void GetWindowCenter( int&x, int& y );
	// Called once per frame to allow convar overrides to acceleration settings when mouse is active
	void		CheckMouseAcclerationVars();

	void		ValidateUserCmd( CUserCmd *usercmd, int sequence_number );

	// TrackIR
	void		TrackIRMove ( float frametime, CUserCmd *cmd );
	void		Init_TrackIR( void );
	void		Shutdown_TrackIR( void );

	bool		m_fTrackIRAvailable;
	// TrackIR

protected:
	typedef struct
	{
		unsigned int AxisFlags;
		unsigned int AxisMap;
		unsigned int ControlMap;
	} joy_axis_t;

	void		DescribeJoystickAxis( int nJoystick, char const *axis, joy_axis_t *mapping );
	char const	*DescribeAxis( int index );

	enum
	{
		GAME_AXIS_NONE = 0,
		GAME_AXIS_FORWARD,
		GAME_AXIS_PITCH,
		GAME_AXIS_SIDE,
		GAME_AXIS_YAW,
		MAX_GAME_AXES
	};

	enum
	{
		CAM_COMMAND_NONE = 0,
		CAM_COMMAND_TOTHIRDPERSON = 1,
		CAM_COMMAND_TOFIRSTPERSON = 2
	};

	enum
	{
		MOUSE_ACCEL_THRESHHOLD1 = 0,	// if mouse moves > this many mickey's double it
		MOUSE_ACCEL_THRESHHOLD2,		// if mouse moves > this many mickey's double it a second time
		MOUSE_SPEED_FACTOR,				// 1 - 20 (default 10) scale factor to accelerated mouse setting

		NUM_MOUSE_PARAMS,
	};

	// Has the mouse been initialized?
	bool		m_fMouseInitialized;
	// Is the mosue active?
	bool		m_fMouseActive;
	// Has the joystick advanced initialization been run?
	bool		m_fJoystickAdvancedInit;
	// Accumulated mouse deltas

	struct PerUserInput_t
	{
		PerUserInput_t()
		{
			m_flAccumulatedMouseXMovement = 0;
			m_flAccumulatedMouseYMovement = 0;
			m_flPreviousMouseXPosition = 0;
			m_flPreviousMouseYPosition = 0;
			m_flRemainingJoystickSampleTime = 0;
			m_flKeyboardSampleTime = 0;

			m_flSpinFrameTime = 0;
			m_flSpinRate = 0;
			m_flLastYawAngle = 0;

			// Is the 3rd person camera using the mouse?
			m_fCameraInterceptingMouse = 0;
			// Are we in 3rd person view?
			m_fCameraInThirdPerson = 0;
			// Should we move view along with mouse?
			m_fCameraMovingWithMouse = 0;
			// What is the current camera offset from the view origin?
			m_vecCameraOffset.Init();
			// Is the camera in distance moving mode?
			m_fCameraDistanceMove = 0;
			// Old and current mouse position readings.
			m_nCameraOldX = 0;
			m_nCameraOldY = 0;
			m_nCameraX = 0;
			m_nCameraY = 0;

			// orthographic camera settings
			m_CameraIsOrthographic = 0;

			m_angPreviousViewAngles.Init();
			m_angPreviousViewAnglesTilt.Init();

			m_flLastForwardMove = 0;

			m_pCommands = 0;
			m_pVerifiedCommands = NULL;

			m_nClearInputState = 0;
			m_pCameraThirdData = NULL;
			m_nCamCommand = 0;
		}

		float		m_flAccumulatedMouseXMovement;
		float		m_flAccumulatedMouseYMovement;
		float		m_flPreviousMouseXPosition;
		float		m_flPreviousMouseYPosition;
		float		m_flRemainingJoystickSampleTime;
		float		m_flKeyboardSampleTime;

		float		m_flSpinFrameTime;
		float		m_flSpinRate;
		float		m_flLastYawAngle;

		// Joystick Axis data
		joy_axis_t m_rgAxes[ MAX_JOYSTICK_AXES ];

		// Is the 3rd person camera using the mouse?
		bool		m_fCameraInterceptingMouse;
		// Are we in 3rd person view?
		bool		m_fCameraInThirdPerson;
		// Should we move view along with mouse?
		bool		m_fCameraMovingWithMouse;
		// What is the current camera offset from the view origin?
		Vector		m_vecCameraOffset;
		// Is the camera in distance moving mode?
		bool		m_fCameraDistanceMove;
		// Old and current mouse position readings.
		int			m_nCameraOldX;
		int			m_nCameraOldY;
		int			m_nCameraX;
		int			m_nCameraY;

		// orthographic camera settings
		bool		m_CameraIsOrthographic;

		QAngle		m_angPreviousViewAngles;
		QAngle		m_angPreviousViewAnglesTilt;

		float		m_flLastForwardMove;

		int			m_nClearInputState;

		CUserCmd	*m_pCommands;
		CVerifiedUserCmd	*m_pVerifiedCommands;

		// Set until polled by CreateMove and cleared
		CHandle< C_BaseCombatWeapon > m_hSelectedWeapon;

#if defined( HL2_CLIENT_DLL )
		CUtlVector< CEntityGroundContact > m_EntityGroundContact;
#endif

		CameraThirdData_t	*m_pCameraThirdData;
		int					m_nCamCommand;
	};

	PerUserInput_t &GetPerUser( int nSlot = -1 );
	const PerUserInput_t &GetPerUser( int nSlot = -1 ) const;

	// Flag to restore systemparameters when exiting
	bool		m_fRestoreSPI;
	// Original mouse parameters
	int			m_rgOrigMouseParms[ NUM_MOUSE_PARAMS ];
	// Current mouse parameters.
	int			m_rgNewMouseParms[ NUM_MOUSE_PARAMS ];
	bool		m_rgCheckMouseParam[ NUM_MOUSE_PARAMS ];
	// Are the parameters valid
	bool		m_fMouseParmsValid;
	// List of queryable keys
	CKeyboardKey *m_pKeys;
	
	PerUserInput_t	m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];

	InputContextHandle_t m_hInputContext;
	CThreadFastMutex m_IKContactPointMutex;
};

extern kbutton_t in_strafe;
extern kbutton_t in_speed;
extern kbutton_t in_jlook;
extern kbutton_t in_graph;  
extern kbutton_t in_moveleft;
extern kbutton_t in_moveright;
extern kbutton_t in_forward;
extern kbutton_t in_back;
extern kbutton_t in_joyspeed;
extern kbutton_t in_lookspin;

extern class ConVar in_joystick;
extern class ConVar joy_autosprint;

extern void KeyDown( kbutton_t *b, const char *c );
extern void KeyUp( kbutton_t *b, const char *c );


#endif // INPUT_H
	
