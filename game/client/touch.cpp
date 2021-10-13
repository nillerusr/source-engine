#include "convar.h"
#include <dlfcn.h>
#include <string.h>
#include "vgui/IInputInternal.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui/ISurface.h"
#include "touch.h"
#include "cdll_int.h"
#include "ienginevgui.h"
#include "in_buttons.h"

extern ConVar cl_sidespeed;
extern ConVar cl_forwardspeed;
extern ConVar cl_upspeed;

#ifdef ANDROID
#define TOUCH_DEFAULT "1"
#else
#define TOUCH_DEFAULT "0"
#endif

extern ConVar sensitivity;

ConVar touch_enable( "touch_enable", TOUCH_DEFAULT, FCVAR_ARCHIVE );
ConVar touch_forwardzone( "touch_forwardzone", "0.06", FCVAR_ARCHIVE, "forward touch zone" );
ConVar touch_sidezone( "touch_sidezone", "0.06", FCVAR_ARCHIVE, "side touch zone" );
ConVar touch_pitch( "touch_pitch", "90", FCVAR_ARCHIVE, "touch pitch sensitivity" );
ConVar touch_yaw( "touch_yaw", "120", FCVAR_ARCHIVE, "touch yaw sensitivity" );
ConVar touch_config_file( "touch_config_file", "touch.cfg", FCVAR_ARCHIVE, "current touch profile file" );
ConVar touch_grid_count( "touch_grid_count", "50", FCVAR_ARCHIVE, "touch grid count" );
ConVar touch_grid_enable( "touch_grid_enable", "1", FCVAR_ARCHIVE, "enable touch grid" );
ConVar touch_precise_amount( "touch_precise_amount", "0.5", FCVAR_ARCHIVE, "sensitivity multiplier for precise-look" );

#define boundmax( num, high ) ( (num) < (high) ? (num) : (high) )
#define boundmin( num, low )  ( (num) >= (low) ? (num) : (low)  )
#define bound( low, num, high ) ( boundmin( boundmax(num, high), low ))
#define S

extern IVEngineClient *engine;

CTouchControls gTouch;
static VTouchPanel g_TouchPanel;
VTouchPanel *touch_panel = &g_TouchPanel;

CTouchPanel::CTouchPanel( vgui::VPANEL parent ) : BaseClass( NULL, "TouchPanel" )
{
	SetParent( parent );

	int w, h;
	engine->GetScreenSize(w, h);
	SetBounds( 0, 0, w, h );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );

	SetVisible( true );
}

bool CTouchPanel::ShouldDraw( void )
{
	return touch_enable.GetBool() && !enginevgui->IsGameUIVisible();
}

void CTouchPanel::Paint()
{
	if( ShouldDraw() )
		gTouch.Frame();
}

CON_COMMAND( touch_addbutton, "add native touch button" )
{
	rgba_t color;
	int argc = args.ArgC();

	if( argc >= 12 )
	{
		color = rgba_t(Q_atoi(args[8]), Q_atoi(args[9]), Q_atoi(args[10]), Q_atoi(args[11])); 
		gTouch.IN_TouchAddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ) ,
			color );

		return;
	}
	
	if( argc >= 8 )
	{
		color = rgba_t(255,255,255);
		
		gTouch.IN_TouchAddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ),
			color );
		return;
	}
	if( argc >= 4 )
	{
		color = rgba_t(255,255,255);
		gTouch.IN_TouchAddButton( args[1], args[2], args[3], 0.4, 0.4, 0.6, 0.6 );
		return;
	}

	Msg( "Usage: touch_addbutton <name> <texture> <command> [<x1> <y1> <x2> <y2> [ r g b a ] ]\n" );
}

void CTouchControls::Init()
{
	engine->GetScreenSize( screen_w, screen_h );
	initialized = true;
	btns.EnsureCapacity( 64 );
	look_finger = move_finger = resize_finger = -1;
	forward = side = 0;
	scolor = rgba_t( -1, -1, -1, -1 );
	state = state_none;
	swidth = 1;
	move = edit = selection = NULL;
	showbuttons = true;
	clientonly = false;
	precision = false;
	mouse_events = 0;
	move_start_x = move_start_y = 0.0f;

	showtexture = hidetexture = resettexture = closetexture = joytexture = 0;
	configchanged = false;

	rgba_t color(255, 255, 255, 255);

	IN_TouchAddButton( "use", "vgui/touch/use", "+use", 0.880000, 0.213333, 1.000000, 0.426667, color );
	IN_TouchAddButton( "jump", "vgui/touch/jump", "+jump", 0.880000, 0.462222, 1.000000, 0.675556, color );
	IN_TouchAddButton( "attack", "vgui/touch/shoot", "+attack", 0.760000, 0.583333, 0.880000, 0.796667, color );
	IN_TouchAddButton( "attack2", "vgui/touch/shoot_alt", "+attack2", 0.760000, 0.320000, 0.880000, 0.533333, color );
	IN_TouchAddButton( "duck", "vgui/touch/crouch", "+duck", 0.880000, 0.746667, 1.000000, 0.960000, color );
	IN_TouchAddButton( "tduck", "vgui/touch/tduck", ";+duck", 0.560000, 0.817778, 0.620000, 0.924444, color );
	IN_TouchAddButton( "_look", "", "", 0.5, 0, 1, 1, color );
	IN_TouchAddButton( "_move", "", "", 0, 0, 0.5, 1, color );
	IN_TouchAddButton( "zoom", "vgui/touch/zoom", "+zoom", 0.680000, 0.00000, 0.760000, 0.142222, color );
	IN_TouchAddButton( "speed", "vgui/touch/speed", "+speed", 0.180000, 0.568889, 0.280000, 0.746667, color );
	IN_TouchAddButton( "loadquick", "vgui/touch/load", "load quick", 0.760000, 0.000000, 0.840000, 0.142222, color );
	IN_TouchAddButton( "savequick", "vgui/touch/save", "save quick", 0.840000, 0.000000, 0.920000, 0.142222, color );
	IN_TouchAddButton( "reload", "vgui/touch/reload", "+reload", 0.000000, 0.320000, 0.120000, 0.533333, color );
	IN_TouchAddButton( "flashlight", "vgui/touch/flash_light_filled", "impulse 100", 0.920000, 0.000000, 1.000000, 0.142222, color );
	IN_TouchAddButton( "invnext", "vgui/touch/next_weap", "invnext", 0.000000, 0.533333, 0.120000, 0.746667, color );
	IN_TouchAddButton( "invprev", "vgui/touch/prev_weap", "invprev", 0.000000, 0.071111, 0.120000, 0.284444, color );
	//IN_TouchAddButton( "edit", "vgui/touch/settings", "touch_enableedit", 0.420000, 0.000000, 0.500000, 0.151486, color );
	IN_TouchAddButton( "menu", "vgui/touch/menu", "gameui_activate", 0.000000, 0.00000, 0.080000, 0.142222, color );
}

void CTouchControls::Shutdown( )
{
	btns.PurgeAndDeleteElements();
}

void CTouchControls::IN_TouchCheckCoords( float *x1, float *y1, float *x2, float *y2  )
{
	/// TODO: grid check here
	if( *x2 - *x1 < GRID_X * 2 )
		*x2 = *x1 + GRID_X * 2;
	if( *y2 - *y1 < GRID_Y * 2)
		*y2 = *y1 + GRID_Y * 2;
	if( *x1 < 0 )
		*x2 -= *x1, *x1 = 0;
	if( *y1 < 0 )
		*y2 -= *y1, *y1 = 0;
	if( *y2 > 1 )
		*y1 -= *y2 - 1, *y2 = 1;
	if( *x2 > 1 )
		*x1 -= *x2 - 1, *x2 = 1;

	if ( touch_grid_enable.GetBool() )
	{
		*x1 = GRID_ROUND_X( *x1 );
		*x2 = GRID_ROUND_X( *x2 );
		*y1 = GRID_ROUND_Y( *y1 );
		*y2 = GRID_ROUND_Y( *y2 );
	}
}

void CTouchControls::Move( float /*frametime*/, CUserCmd *cmd )
{
	cmd->sidemove -= cl_sidespeed.GetFloat() * side;
	cmd->forwardmove += cl_forwardspeed.GetFloat() * forward;
}

void CTouchControls::IN_Look()
{
	if( !pitch && !yaw )
		return;

	QAngle ang;
	engine->GetViewAngles( ang );
	ang.x += pitch;
	ang.y += yaw;
	engine->SetViewAngles( ang );
	pitch = yaw = 0;
}

void CTouchControls::Frame()
{
	if (!initialized)
		return;

	IN_Look();
	Paint();
}

void CTouchControls::Paint( )
{
	if (!initialized)
		return;

	CUtlLinkedList<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;
		if( btn->type == touch_move || btn->type == touch_look )
			continue;

		g_pMatSystemSurface->DrawSetColor(255, 255, 255, 155);
		g_pMatSystemSurface->DrawSetTexture( btn->textureID );
		g_pMatSystemSurface->DrawTexturedRect( btn->x1*screen_w, btn->y1*screen_h, btn->x2*screen_w, btn->y2*screen_h );
	}
}

void CTouchControls::IN_TouchAddButton( const char *name, const char *texturefile, const char *command, float x1, float y1, float x2, float y2, rgba_t color )
{
	CTouchButton *btn = new CTouchButton;
	ETouchButtonType type = touch_command;
	
	Q_strncpy( btn->name, name, sizeof(btn->name) );
	Q_strncpy( btn->texturefile, texturefile, sizeof(btn->texturefile) );
	Q_strncpy( btn->command, command, sizeof(btn->command) );

	IN_TouchCheckCoords(&x1, &y1, &x2, &y2);

	btn->x1 = x1;
	btn->y1 = y1;
	btn->x2 = x2;
	btn->y2 = y1 + ( x2 - x1 ) * (((float)screen_w)/screen_h);

	IN_TouchCheckCoords(&btn->x1, &btn->y1, &btn->x2, &btn->y2);

	if( Q_strcmp(name, "_look") == 0 )
		type = touch_look;
	else if( Q_strcmp(name, "_move") == 0 )
		type = touch_move;		
	
	btn->color = color;
	btn->type = type;
	btn->finger = -1;
	btn->textureID = g_pMatSystemSurface->CreateNewTextureID();

	g_pMatSystemSurface->DrawSetTextureFile( btn->textureID, btn->texturefile, true, false);

	btns.AddToTail(btn);
}

void CTouchControls::ProcessEvent(touch_event_t *ev)
{
	if( !touch_enable.GetBool() )
		return;

	if( ev->type == IE_FingerMotion )
		FingerMotion( ev );
	else
		FingerPress( ev );
}

void CTouchControls::FingerMotion(touch_event_t *ev)
{
	float x = ev->x / (float)screen_w;
	float y = ev->y / (float)screen_h;

	float f, s;

	CUtlLinkedList<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;
		if( btn->finger == ev->fingerid )
		{
			if( btn->type == touch_move )
			{
				f = ( move_start_y - y ) / touch_forwardzone.GetFloat();
				s = ( move_start_x - x ) / touch_sidezone.GetFloat();
				forward = bound( -1, f, 1 );
				side = bound( -1, s, 1 );
			}
			else if( btn->type == touch_look )
			{
				yaw += touch_yaw.GetFloat() * ( dx - x ) * sensitivity.GetFloat();
				pitch -= touch_pitch.GetFloat() * ( dy - y ) * sensitivity.GetFloat();
				dx = x;
				dy = y;
			}
		}
	}
}

void CTouchControls::FingerPress(touch_event_t *ev)
{
	float x = ev->x / (float)screen_w;
	float y = ev->y / (float)screen_h;

	CUtlLinkedList<CTouchButton*>::iterator it;
	
	if( ev->type == IE_FingerDown )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;
			if(  x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )
			{
				btn->finger = ev->fingerid;
				if( btn->type == touch_move  )
				{
					if( move_finger == -1 )
					{
						move_start_x = x;
						move_start_y = y;
						move_finger = ev->fingerid;
					}
					else
						btn->finger = move_finger;
				}
				else if( btn->type == touch_look )
				{
					if( look_finger == -1 )
					{
						dx = x;
						dy = y;
						look_finger = ev->fingerid;
					}
					else
						btn->finger = look_finger;
				}
				else
					engine->ClientCmd( btn->command );
			}
		}
	}
	else if( ev->type == IE_FingerUp )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;
			if( btn->finger == ev->fingerid )
			{
				btn->finger = -1;

				if( btn->type == touch_move )
				{
					forward = side = 0;
					move_finger = -1;
				}
				else if( btn->type == touch_look )
					look_finger = -1;
				else if( btn->command[0] == '+' )
				{
					char cmd[256];

					snprintf( cmd, sizeof cmd, "%s", btn->command );
					cmd[0] = '-';
					engine->ClientCmd( cmd );
				}
			}
		}
	}
}
