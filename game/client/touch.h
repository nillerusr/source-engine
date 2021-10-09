#include "utllinkedlist.h"
#include "vgui/VGUI.h"
#include <vgui_controls/Panel.h>
#include "cbase.h"
#include "kbutton.h"
#include "usercmd.h"

extern ConVar touch_enable;

#define STEAMCONTROLLER_A -3

#define MOUSE_EVENT_PRESS 0x02
#define MOUSE_EVENT_RELEASE 0x04

enum ActivationType_t
{
    ACTIVATE_ONPRESSEDANDRELEASED, // normal button behaviour
    ACTIVATE_ONPRESSED, // menu buttons, toggle buttons
    ACTIVATE_ONRELEASED, // menu items
};

#define CMD_SIZE 64

enum ETouchButtonType
{
	touch_command = 0, // Tap button
	touch_move,    // Like a joystick stick.
	touch_joy,     // Like a joystick stick, centered.
	touch_dpad,    // Only two directions.
	touch_look,     // Like a touchpad.
	touch_key
};

enum ETouchState
{
	state_none = 0,
	state_edit,
	state_edit_move
};

enum ETouchRound
{
	round_none = 0,
	round_grid,
	round_aspect
};

struct rgba_t
{
	rgba_t(	unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255 ) : r( r ), g( g ), b( b ), a( a ) { }
	rgba_t() : r( 0 ), g( 0 ), b( 0 ), a( 0 ) { }
	rgba_t( unsigned char *x ) : r( x[0] ), g( x[1] ), b( x[2] ), a( x[3] ) { }

	operator unsigned char*() { return &r; }

	unsigned char r, g, b, a; 
};

struct event_clientcmd_t
{
	char buf[CMD_SIZE];
};

struct event_s
{
	int type;
	int x;
	int y;
	int fingerid;
} typedef touch_event_t;


class CTouchButton
{
public:
	// Touch button type: tap, stick or slider
	ETouchButtonType type;

	// Field of button in pixels
	float x1, y1, x2, y2;

	// Button texture
	int texture;

	rgba_t color;
	char texturefile[256];
	char command[256];
	char name[32];
	int finger;
	int flags;
	float fade;
	float fadespeed;
	float fadeend;
	float aspect;
	int textureID;
};

struct touch_settings_s
{
	float pitch = 90;
	float yaw = 120;
	float sensitivity = 2;
	float forwardzone = 0.8;
	float sidezone = 0.12;
};

class CTouchPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTouchPanel, vgui::Panel );

public:
	CTouchPanel( vgui::VPANEL parent );
	virtual			~CTouchPanel( void );

	virtual void	Paint();
	virtual bool	ShouldDraw( void );
};

abstract_class ITouchPanel
{
public:
	virtual void		Create( vgui::VPANEL parent ) = 0;
	virtual void		Destroy( void ) = 0;
};

class VTouchPanel : public ITouchPanel
{
private:
	CTouchPanel *touchPanel;
public:
	VTouchPanel( void )
	{
		touchPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		touchPanel = new CTouchPanel( parent );
	}

	void Destroy( void )
	{
		if ( touchPanel )
		{
			touchPanel->SetParent( (vgui::Panel *)NULL );
			touchPanel->MarkForDeletion();
			touchPanel = NULL;
		}
	}
};


class CTouchControls
{
public:
	CTouchControls();
	~CTouchControls();

	void VidInit( );
	void Shutdown( );

	void Init( );
	void Paint( );
	void Frame( );

	void IN_TouchAddButton( const char *name, const char *texturefile, const char *command, ETouchButtonType type, float x1, float y1, float x2, float y2, rgba_t color );
	void Move( float frametime, CUserCmd *cmd );
	void IN_Look( );

	void ProcessEvent( touch_event_t *ev );
	void FingerPress( touch_event_t *ev );
	void FingerMotion( touch_event_t *ev );

	CTouchPanel *touchPanel;
private:
	bool initialized;
	ETouchState state;
	CUtlLinkedList<CTouchButton> btns;

	int look_finger;
	int move_finger;
	float forward, side, movecount;
	float yaw, pitch;
	CTouchButton *move;

	float move_start_x, move_start_y;
	float dx, dy;

	// editing
	CTouchButton *edit;
	CTouchButton *selection;
	int resize_finger;
	bool showbuttons;
	bool clientonly;
	rgba_t scolor;
	int swidth;
	bool precision;
	// textures
	int showtexture;
	int hidetexture;
	int resettexture;
	int closetexture;
	int joytexture; // touch indicator
	bool configchanged;
	vgui::HFont textfont;
	int mouse_events;

	struct touch_settings_s touch_settings;
};

extern CTouchControls gTouch;
extern VTouchPanel *touch_panel;
