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

ConVar touch_enable( "touch_enable", TOUCH_DEFAULT, FCVAR_ARCHIVE );

#define boundmax( num, high ) ( (num) < (high) ? (num) : (high) )
#define boundmin( num, low )  ( (num) >= (low) ? (num) : (low)  )
#define bound( low, num, high ) ( boundmin( boundmax(num, high), low ))
#define S

extern IVEngineClient *engine;
extern vgui::IInputInternal *g_pInputInternal;

static int g_LastDefaultButton = 0;
int screen_h, screen_w;

static CTouchButton g_Buttons[512];
static int g_LastButton = 0;

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

CTouchPanel::~CTouchPanel( void )
{
}

bool CTouchPanel::ShouldDraw( void )
{
	return touch_enable.GetBool() && !enginevgui->IsGameUIVisible();
}

void CTouchPanel::Paint()
{
	gTouch.Frame();
}

CTouchControls::CTouchControls()
{
}

CTouchControls::~CTouchControls()
{
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

	IN_TouchAddButton( "use", "vgui/touch/use", "+use", touch_command, 0.880000, 0.213333, 1.000000, 0.426667, color );
	IN_TouchAddButton( "jump", "vgui/touch/jump", "+jump", touch_command, 0.880000, 0.462222, 1.000000, 0.675556, color );
	IN_TouchAddButton( "attack", "vgui/touch/shoot", "+attack", touch_command, 0.760000, 0.583333, 0.880000, 0.796667, color );
	IN_TouchAddButton( "attack2", "vgui/touch/shoot_alt", "+attack2", touch_command, 0.760000, 0.320000, 0.880000, 0.533333, color );
	IN_TouchAddButton( "duck", "vgui/touch/crouch", "+duck", touch_command, 0.880000, 0.746667, 1.000000, 0.960000, color );
	IN_TouchAddButton( "tduck", "vgui/touch/tduck", ";+duck", touch_command, 0.560000, 0.817778, 0.620000, 0.924444, color );
	IN_TouchAddButton( "look", "", "", touch_look, 0.5, 0, 1, 1, color );
	IN_TouchAddButton( "move", "", "", touch_move, 0, 0, 0.5, 1, color );
	IN_TouchAddButton( "zoom", "vgui/touch/zoom", "+zoom", touch_command, 0.680000, 0.00000, 0.760000, 0.142222, color );
	IN_TouchAddButton( "speed", "vgui/touch/speed", "+speed", touch_command, 0.180000, 0.568889, 0.280000, 0.746667, color );
	IN_TouchAddButton( "loadquick", "vgui/touch/load", "load quick", touch_command, 0.760000, 0.000000, 0.840000, 0.142222, color );
	IN_TouchAddButton( "savequick", "vgui/touch/save", "save quick", touch_command, 0.840000, 0.000000, 0.920000, 0.142222, color );
	IN_TouchAddButton( "reload", "vgui/touch/reload", "+reload", touch_command, 0.000000, 0.320000, 0.120000, 0.533333, color );
	IN_TouchAddButton( "flashlight", "vgui/touch/flash_light_filled", "impulse 100", touch_command, 0.920000, 0.000000, 1.000000, 0.142222, color );
	IN_TouchAddButton( "invnext", "vgui/touch/next_weap", "invnext", touch_command, 0.000000, 0.533333, 0.120000, 0.746667, color );
	IN_TouchAddButton( "invprev", "vgui/touch/prev_weap", "invprev", touch_command, 0.000000, 0.071111, 0.120000, 0.284444, color );

	IN_TouchAddButton( "menu", "vgui/touch/menu", "gameui_activate", touch_command, 0.000000, 0.00000, 0.080000, 0.142222, color );
}

#define GRID_COUNT 50
#define GRID_COUNT_X (GRID_COUNT)
#define GRID_COUNT_Y (GRID_COUNT * screen_h / screen_w)
#define GRID_X (1.0/GRID_COUNT_X)
#define GRID_Y (screen_w/screen_h/GRID_COUNT_X)
#define GRID_ROUND_X(x) ((float)round( x * GRID_COUNT_X ) / GRID_COUNT_X)
#define GRID_ROUND_Y(x) ((float)round( x * GRID_COUNT_Y ) / GRID_COUNT_Y)

static void IN_TouchCheckCoords( float *x1, float *y1, float *x2, float *y2  )
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

	*x1 = GRID_ROUND_X( *x1 );
	*x2 = GRID_ROUND_X( *x2 );
	*y1 = GRID_ROUND_Y( *y1 );
	*y2 = GRID_ROUND_Y( *y2 );
}

void CTouchControls::VidInit( ) { }

void CTouchControls::Shutdown( ) { }

void CTouchControls::Move( float frametime, CUserCmd *cmd )
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

	if ( !enginevgui->IsGameUIVisible() )
	{
		for (int i = 0; i < g_LastButton; i++)
		{
			if( g_Buttons[i].type == touch_move || g_Buttons[i].type == touch_look )
				continue;
			g_pMatSystemSurface->DrawSetColor(255, 255, 255, 155);
			g_pMatSystemSurface->DrawSetTexture( g_Buttons[i].textureID );
			g_pMatSystemSurface->DrawTexturedRect( g_Buttons[i].x1*screen_w, g_Buttons[i].y1*screen_h, g_Buttons[i].x2*screen_w, g_Buttons[i].y2*screen_h );
		}
	}
}

void CTouchControls::IN_TouchAddButton( const char *name, const char *texturefile, const char *command, ETouchButtonType type, float x1, float y1, float x2, float y2, rgba_t color )
{
	if( g_LastButton >= 64 )
		return;

	Q_strncpy( g_Buttons[g_LastButton].name, name, 32 );
	Q_strncpy( g_Buttons[g_LastButton].texturefile, texturefile, 256 );
	Q_strncpy( g_Buttons[g_LastButton].command, command, 256 );

	IN_TouchCheckCoords(&x1, &y1, &x2, &y2);

	g_Buttons[g_LastButton].x1 = x1;
	g_Buttons[g_LastButton].y1 = y1;
	g_Buttons[g_LastButton].x2 = x2;
	g_Buttons[g_LastButton].y2 = y1 + ( x2 - x1 ) * (((float)screen_w)/screen_h);

	IN_TouchCheckCoords(&g_Buttons[g_LastButton].x1, &g_Buttons[g_LastButton].y1, &g_Buttons[g_LastButton].x2, &g_Buttons[g_LastButton].y2);

	g_Buttons[g_LastButton].color = color;
	g_Buttons[g_LastButton].type = type;
	g_Buttons[g_LastButton].finger = -1;
	g_Buttons[g_LastButton].textureID = g_pMatSystemSurface->CreateNewTextureID();
	g_pMatSystemSurface->DrawSetTextureFile( g_Buttons[g_LastButton].textureID, g_Buttons[g_LastButton].texturefile, true, false);

	g_LastButton++;
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

	for (int i = 0; i < g_LastButton; i++)
	{
		if( g_Buttons[i].finger == ev->fingerid )
		{
			if( g_Buttons[i].type == touch_move )
			{
				f = ( move_start_y - y ) / touch_settings.sidezone;
				s = ( move_start_x - x ) / touch_settings.sidezone;
				forward = bound( -1, f, 1 );
				side = bound( -1, s, 1 );
			}
			else if( g_Buttons[i].type == touch_look )
			{
				yaw += touch_settings.yaw * ( dx - x ) * touch_settings.sensitivity;
				pitch -= touch_settings.pitch * ( dy - y ) * touch_settings.sensitivity;
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

	if( ev->type == IE_FingerDown )
	{
		for (int i = 0; i < g_LastButton; i++)
		{
			if(  x > g_Buttons[i].x1 && x < g_Buttons[i].x2 && y > g_Buttons[i].y1 && y < g_Buttons[i].y2 )
			{
				g_Buttons[i].finger = ev->fingerid;
				if( g_Buttons[i].type == touch_move  )
				{
					if( move_finger == -1 )
					{
						move_start_x = x;
						move_start_y = y;
						move_finger = ev->fingerid;
					}
					else
						g_Buttons[i].finger = move_finger;
				}
				else if( g_Buttons[i].type == touch_look )
				{
					if( look_finger == -1 )
					{
						dx = x;
						dy = y;
						look_finger = ev->fingerid;
					}
					else
						g_Buttons[i].finger = look_finger;
				}
				else
					engine->ClientCmd( g_Buttons[i].command );
			}
		}
    }
    else if( ev->type == IE_FingerUp )
    {
        {
            for (int i = 0; i < g_LastButton; i++)
            {
                if( g_Buttons[i].finger == ev->fingerid )
                {
                    g_Buttons[i].finger = -1;

                    if( g_Buttons[i].type == touch_move )
                    {
                        forward = side = 0;
                        move_finger = -1;
                    }
                    else if( g_Buttons[i].type == touch_look )
                        look_finger = -1;
                    else if( g_Buttons[i].command[0] == '+' )
                    {
                        char cmd[256];

                        snprintf( cmd, sizeof cmd, "%s", g_Buttons[i].command );
                        cmd[0] = '-';
                        engine->ClientCmd( cmd );
                    }
                }
            }
        }
    }
}
