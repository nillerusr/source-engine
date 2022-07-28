#include "cbase.h"
#include "convar.h"
#include <string.h>
#include "vgui/IInputInternal.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui/ISurface.h"
#include "touch.h"
#include "cdll_int.h"
#include "ienginevgui.h"
#include "in_buttons.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "vgui_controls/Button.h"

extern ConVar cl_sidespeed;
extern ConVar cl_forwardspeed;
extern ConVar cl_upspeed;
extern ConVar default_fov;

#ifdef ANDROID
#define TOUCH_DEFAULT "1"
#else
#define TOUCH_DEFAULT "0"
#endif

extern ConVar sensitivity;

#define TOUCH_DEFAULT_CFG "touch_default.cfg"

ConVar touch_enable( "touch_enable", TOUCH_DEFAULT, FCVAR_ARCHIVE );
ConVar touch_draw( "touch_draw", "1", FCVAR_ARCHIVE );
ConVar touch_filter( "touch_filter", "0", FCVAR_ARCHIVE );
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


void CTouchPanel::Paint()
{
	gTouch.Frame();
}

void CTouchPanel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);

	int w,h;
	w = ScreenWidth();
	h = ScreenHeight();
	gTouch.screen_w = ScreenWidth(); gTouch.screen_h = h;

	SetBounds( 0, 0, w, h );
}

void CTouchPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	int w,h;
	w = ScreenWidth();
	h = ScreenHeight();
	gTouch.screen_w = ScreenWidth(); gTouch.screen_h = h;

	SetBounds( 0, 0, w, h );
}

CON_COMMAND( touch_addbutton, "add native touch button" )
{
	rgba_t color;
	int argc = args.ArgC();

	if( argc >= 12 )
	{
		float aspect = 1.f;
		int flags = 0;

		if( argc >= 13 )
			flags = Q_atoi( args[12] );
		if( argc >= 14 )
			aspect = Q_atof( args[13] );

		color = rgba_t(Q_atoi(args[8]), Q_atoi(args[9]), Q_atoi(args[10]), Q_atoi(args[11])); 
		gTouch.AddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ) ,
			color, round_aspect, aspect, flags );

		return;
	}

	if( argc >= 8 )
	{
		color = rgba_t(255,255,255);

		gTouch.AddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ),
			color );
		return;
	}
	if( argc >= 4 )
	{
		color = rgba_t(255,255,255);
		gTouch.AddButton( args[1], args[2], args[3], 0.4, 0.4, 0.6, 0.6 );
		return;
	}

	Msg( "Usage: touch_addbutton <name> <texture> <command> [<x1> <y1> <x2> <y2> [ r g b a ] ]\n" );
}

CON_COMMAND( touch_removebutton, "add native touch button" )
{
	if( args.ArgC() > 1 )
		gTouch.RemoveButton( args[1] );
	else
		Msg( "Usage: touch_removebutton <name>\n" );
}

CON_COMMAND( touch_settexture, "add native touch button" )
{
	if( args.ArgC() >= 3 )
	{
		gTouch.SetTexture( args[1], args[2] );
		return;
	}
	Msg( "Usage: touch_settexture <name> <file>\n" );
}

CON_COMMAND( touch_enableedit, "enable button editing mode" )
{
	gTouch.EnableTouchEdit(true);
}

CON_COMMAND( touch_disableedit, "disable button editing mode" )
{
	gTouch.EnableTouchEdit(false);
}

CON_COMMAND( touch_setcolor, "change button color" )
{
	if( args.ArgC() >= 6 )
	{
		rgba_t color( Q_atoi( args[2] ), Q_atoi( args[3] ), Q_atoi( args[4] ), Q_atoi( args[5] ) );
		gTouch.SetColor( args[1], color );
	}
	else
		Msg( "Usage: touch_setcolor <name> <r> <g> <b> <a>\n" );
}

CON_COMMAND( touch_setcommand, "change button command" )
{
	if( args.ArgC() >= 3 )
		gTouch.SetCommand( args[1], args[2] );
	else
		Msg( "Usage: touch_setcommand <name> <command>\n" );
}

CON_COMMAND( touch_setflags, "change button flags" )
{
	if( args.ArgC() >= 3 )
		gTouch.SetFlags( args[1], Q_atoi(args[2]) );
	else
		Msg( "Usage: touch_setflags <name> <flags>\n" );
}

CON_COMMAND( touch_show, "show button" )
{
	if( args.ArgC() >= 2 )
		gTouch.ShowButton( args[1] );
	else
		Msg( "Usage: touch_show <name>\n" );
}

CON_COMMAND( touch_hide, "hide button" )
{
	if( args.ArgC() >= 2 )
		gTouch.HideButton( args[1] );
	else
		Msg( "Usage: touch_hide <name>\n" );
}

CON_COMMAND( touch_list, "list buttons" )
{
	gTouch.ListButtons();
}

CON_COMMAND( touch_removeall, "remove all buttons" )
{
	gTouch.RemoveButtons();
}

CON_COMMAND( touch_writeconfig, "save current config" )
{
	gTouch.WriteConfig();
}


CON_COMMAND( touch_loaddefaults, "generate config from defaults" )
{
	gTouch.ResetToDefaults();
}
/*
CON_COMMAND( touch_roundall, "round all buttons coordinates to grid" )
{
	
}

CON_COMMAND( touch_exportconfig, "export config keeping aspect ratio" )
{
	
}

CON_COMMAND( touch_reloadconfig, "load config, not saving changes" )
{
	
}
*/

/*
CON_COMMAND( touch_fade, "start fade animation for selected buttons" )
{
	
}

CON_COMMAND( touch_toggleselection, "toggle visibility on selected button in editor" )
{

}*/

void CTouchControls::GetTouchAccumulators( float *side, float *forward, float *yaw, float *pitch )
{
	*forward = this->forward;
	*side = this->side;
	*pitch = this->pitch;
	*yaw = this->yaw;
	this->yaw = 0.f;
	this->pitch = 0.f;
}

void CTouchControls::GetTouchDelta( float yaw, float pitch, float *dx, float *dy )
{
	// Apply filtering?
	if( touch_filter.GetBool() )
	{
		// Average over last two samples
		*dx = ( yaw + m_flPreviousYaw ) * 0.5f;
		*dy = ( pitch + m_flPreviousPitch ) * 0.5f;
	}
	else
	{
		*dx = yaw;
		*dy = pitch;
	}

	// Latch previous
	m_flPreviousYaw = yaw;
	m_flPreviousPitch = pitch;
}

void CTouchControls::ResetToDefaults()
{
	rgba_t color(255, 255, 255, 155);
	char buf[MAX_PATH];

	RemoveButtons();

	Q_snprintf(buf, sizeof buf, "cfg/%s", TOUCH_DEFAULT_CFG);
	if( !filesystem->FileExists(buf) )
	{
		AddButton( "look", "", "_look", 0.5, 0, 1, 1, color, 0, 0, 0 );
		AddButton( "move", "", "_move", 0, 0, 0.5, 1, color, 0, 0, 0 );

		AddButton( "use", "vgui/touch/use", "+use", 0.880000, 0.213333, 1.000000, 0.426667, color );
		AddButton( "jump", "vgui/touch/jump", "+jump", 0.880000, 0.462222, 1.000000, 0.675556, color );
		AddButton( "attack", "vgui/touch/shoot", "+attack", 0.760000, 0.583333, 0.880000, 0.796667, color );
		AddButton( "attack2", "vgui/touch/shoot_alt", "+attack2", 0.760000, 0.320000, 0.880000, 0.533333, color );
		AddButton( "duck", "vgui/touch/crouch", "+duck", 0.880000, 0.746667, 1.000000, 0.960000, color );
		AddButton( "tduck", "vgui/touch/tduck", ";+duck", 0.560000, 0.817778, 0.620000, 0.924444, color );
		AddButton( "zoom", "vgui/touch/zoom", "+zoom", 0.680000, 0.00000, 0.760000, 0.142222, color );
		AddButton( "speed", "vgui/touch/speed", "+speed", 0.180000, 0.568889, 0.280000, 0.746667, color );
		AddButton( "loadquick", "vgui/touch/load", "load quick", 0.760000, 0.000000, 0.840000, 0.142222, color );
		AddButton( "savequick", "vgui/touch/save", "save quick", 0.840000, 0.000000, 0.920000, 0.142222, color );
		AddButton( "reload", "vgui/touch/reload", "+reload", 0.000000, 0.320000, 0.120000, 0.533333, color );
		AddButton( "flashlight", "vgui/touch/flash_light_filled", "impulse 100", 0.920000, 0.000000, 1.000000, 0.142222, color );
		AddButton( "invnext", "vgui/touch/next_weap", "invnext", 0.000000, 0.533333, 0.120000, 0.746667, color );
		AddButton( "invprev", "vgui/touch/prev_weap", "invprev", 0.000000, 0.071111, 0.120000, 0.284444, color );
		AddButton( "edit", "vgui/touch/settings", "touch_enableedit", 0.420000, 0.000000, 0.500000, 0.151486, color );
		AddButton( "menu", "vgui/touch/menu", "gameui_activate", 0.000000, 0.00000, 0.080000, 0.142222, color );
	}
	else
	{
		Q_snprintf(buf, sizeof buf, "exec %s", TOUCH_DEFAULT_CFG);
		engine->ExecuteClientCmd(buf);
	}

	WriteConfig();
}

void CTouchControls::Init()
{
	int w,h;
	engine->GetScreenSize( w, h );
	screen_w = w; screen_h = h;

	configchanged = false;
	config_loaded = false;
	btns.EnsureCapacity( 64 );
	look_finger = move_finger = resize_finger = -1;
	forward = side = 0.f;
	pitch = yaw = 0.f;
	scolor = rgba_t( -1, -1, -1, -1 );
	state = state_none;
	swidth = 1;
	move_button = edit = selection = NULL;
	showbuttons = true;
	clientonly = false;
	precision = false;
	mouse_events = 0;
	move_start_x = move_start_y = 0.0f;
	m_flPreviousYaw = m_flPreviousPitch = 0.f;

	showtexture = hidetexture = resettexture = closetexture = joytexture = 0;
	configchanged = false;

	rgba_t color(255, 255, 255, 155);

	AddButton( "look", "", "_look", 0.5, 0, 1, 1, color, 0, 0, 0 );
	AddButton( "move", "", "_move", 0, 0, 0.5, 1, color, 0, 0, 0 );

	AddButton( "use", "vgui/touch/use", "+use", 0.880000, 0.213333, 1.000000, 0.426667, color );
	AddButton( "jump", "vgui/touch/jump", "+jump", 0.880000, 0.462222, 1.000000, 0.675556, color );
	AddButton( "attack", "vgui/touch/shoot", "+attack", 0.760000, 0.583333, 0.880000, 0.796667, color );
	AddButton( "attack2", "vgui/touch/shoot_alt", "+attack2", 0.760000, 0.320000, 0.880000, 0.533333, color );
	AddButton( "duck", "vgui/touch/crouch", "+duck", 0.880000, 0.746667, 1.000000, 0.960000, color );
	AddButton( "tduck", "vgui/touch/tduck", ";+duck", 0.560000, 0.817778, 0.620000, 0.924444, color );
	AddButton( "zoom", "vgui/touch/zoom", "+zoom", 0.680000, 0.00000, 0.760000, 0.142222, color );
	AddButton( "speed", "vgui/touch/speed", "+speed", 0.180000, 0.568889, 0.280000, 0.746667, color );
	AddButton( "loadquick", "vgui/touch/load", "load quick", 0.760000, 0.000000, 0.840000, 0.142222, color );
	AddButton( "savequick", "vgui/touch/save", "save quick", 0.840000, 0.000000, 0.920000, 0.142222, color );
	AddButton( "reload", "vgui/touch/reload", "+reload", 0.000000, 0.320000, 0.120000, 0.533333, color );
	AddButton( "flashlight", "vgui/touch/flash_light_filled", "impulse 100", 0.920000, 0.000000, 1.000000, 0.142222, color );
	AddButton( "invnext", "vgui/touch/next_weap", "invnext", 0.000000, 0.533333, 0.120000, 0.746667, color );
	AddButton( "invprev", "vgui/touch/prev_weap", "invprev", 0.000000, 0.071111, 0.120000, 0.284444, color );
	AddButton( "edit", "vgui/touch/settings", "touch_enableedit", 0.420000, 0.000000, 0.500000, 0.151486, color );
	AddButton( "menu", "vgui/touch/menu", "gameui_activate", 0.000000, 0.00000, 0.080000, 0.142222, color );

	char buf[256];

	Q_snprintf(buf, sizeof buf, "cfg/%s", touch_config_file.GetString());
	if( filesystem->FileExists(buf, "MOD") )
	{
		Q_snprintf(buf, sizeof buf, "exec %s\n", touch_config_file.GetString());
		engine->ClientCmd_Unrestricted(buf);
	}
	else
		ResetToDefaults();

	initialized = true;
}

void CTouchControls::Shutdown( )
{
	btns.PurgeAndDeleteElements();
}

void CTouchControls::RemoveButtons()
{
	btns.PurgeAndDeleteElements();	
}

void CTouchControls::ListButtons()
{
	CUtlLinkedList<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *b = *it;
	
		Msg( "%s %s %s %f %f %f %f %d %d %d %d %d\n",
			b->name, b->texturefile, b->command,
			b->x1, b->y1, b->x2, b->y2,
			b->color.r, b->color.g, b->color.b, b->color.a, b->flags );
	}
}

void CTouchControls::IN_CheckCoords( float *x1, float *y1, float *x2, float *y2  )
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
}

void CTouchControls::IN_Look()
{
}

void CTouchControls::Frame()
{
	if (!initialized)
		return;

	if( touch_enable.GetBool() && touch_draw.GetBool() && !enginevgui->IsGameUIVisible() ) Paint();
}

void CTouchControls::Paint( )
{
	if (!initialized)
		return;

	if( state == state_edit )
	{
		vgui::surface()->DrawSetColor(255, 0, 0, 200);
		float x,y;

		for( x = 0.0f; x < 1.0f; x += GRID_X )
			vgui::surface()->DrawLine( screen_w*x, 0, screen_w*x, screen_h );

		for( y = 0.0f; y < 1.0f; y += GRID_Y )
			vgui::surface()->DrawLine( 0, screen_h*y, screen_w, screen_h*y );
	}

	CUtlLinkedList<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;

		if( btn->type != touch_move && btn->type != touch_look && !(btn->flags & TOUCH_FL_HIDE) )
		{
			vgui::surface()->DrawSetTexture( btn->textureID );
			vgui::surface()->DrawSetColor(btn->color.r, btn->color.g, btn->color.b, btn->color.a);
			vgui::surface()->DrawTexturedRect( btn->x1*screen_w, btn->y1*screen_h, btn->x2*screen_w, btn->y2*screen_h );
		}

		if( state == state_edit && !(btn->flags & TOUCH_FL_NOEDIT) )
		{
			vgui::surface()->DrawSetColor(255, 0, 0, 50);
			vgui::surface()->DrawFilledRect( btn->x1*screen_w, btn->y1*screen_h, btn->x2*screen_w, btn->y2*screen_h );
		}
	}
}

void CTouchControls::AddButton( const char *name, const char *texturefile, const char *command, float x1, float y1, float x2, float y2, rgba_t color, int round, float aspect, int flags )
{
	CTouchButton *btn = new CTouchButton;
	ETouchButtonType type = touch_command;

	Q_strncpy( btn->name, name, sizeof(btn->name) );
	Q_strncpy( btn->texturefile, texturefile, sizeof(btn->texturefile) );
	Q_strncpy( btn->command, command, sizeof(btn->command) );

	if( round )
		IN_CheckCoords(&x1, &y1, &x2, &y2);

	if( round == round_aspect )
		y2 = y1 + ( x2 - x1 ) * (((float)screen_w)/screen_h) * aspect;

	btn->x1 = x1;
	btn->y1 = y1;
	btn->x2 = x2;
	btn->y2 = y2;
	btn->flags = flags;

	//IN_CheckCoords(&btn->x1, &btn->y1, &btn->x2, &btn->y2);

	if( Q_strcmp(command, "_look") == 0 )
		type = touch_look;
	else if( Q_strcmp(command, "_move") == 0 )
		type = touch_move;

	btn->color = color;
	btn->type = type;
	btn->finger = -1;
	btn->textureID = vgui::surface()->CreateNewTextureID();

	vgui::surface()->DrawSetTextureFile( btn->textureID, btn->texturefile, true, false);

	btns.AddToTail(btn);
}

void CTouchControls::ShowButton( const char *name )
{
	CTouchButton *btn =	FindButton( name );
	if( btn )
		btn->flags &= ~TOUCH_FL_HIDE;
}

void CTouchControls::HideButton(const char *name)
{
	CTouchButton *btn =	FindButton( name );
	if( btn )
		btn->flags |= TOUCH_FL_HIDE;
}

void CTouchControls::SetTexture(const char *name, const char *file)
{
	CTouchButton *btn = FindButton(name);

	if( btn )
	{
		Q_strncpy( btn->texturefile, file, sizeof(btn->texturefile) );

		btn->textureID = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( btn->textureID, file, true, false);
	}
}

void CTouchControls::SetColor(const char *name, rgba_t color)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) btn->color = color;
}

void CTouchControls::SetCommand(const char *name, const char *cmd)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) Q_strncpy(btn->command, cmd, sizeof btn->command);
}

void CTouchControls::SetFlags(const char *name, int flags)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) btn->flags = flags;
}

void CTouchControls::RemoveButton( const char *name )
{
	for( int i = 0; i < btns.Count(); i++ )
	{
		if( Q_strncmp( btns[i]->name, name, sizeof(btns[i]->name)) == 0 )
			btns.Free(i);
	}
}

CTouchButton *CTouchControls::FindButton( const char *name )
{
	CUtlLinkedList<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *button = *it;

		if( Q_strncmp( button->name, name, sizeof(button->name)) == 0 )
			return button;
	}
	return NULL;
}

void CTouchControls::ProcessEvent(touch_event_t *ev)
{
	if( !touch_enable.GetBool() )
		return;

	if( state == state_edit )
	{
		EditEvent(ev);
		return;
	}
	
	if( ev->type == IE_FingerMotion )
		FingerMotion( ev );
	else
		FingerPress( ev );
}

void CTouchControls::EditEvent(touch_event_t *ev)
{
	const float x = ev->x;
	const float y = ev->y;

	//CUtlLinkedList<CTouchButton*>::iterator it;
	
	if( ev->type == IE_FingerDown )
	{		
		//for( it = btns.end(); it != btns.begin(); it-- ) unexpected, doesn't work
		for( int i = btns.Count()-1; i >= 0; i-- )
		{
			CTouchButton *btn = btns[i];
			if( x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )
			{
				if( btn->flags & TOUCH_FL_HIDE )
					continue;
				
				if( btn->flags & TOUCH_FL_NOEDIT )
				{
					engine->ClientCmd_Unrestricted( btn->command );
					continue;
				}

				if( move_finger == -1 )
				{
					move_finger = ev->fingerid;
					selection = btn;
					break;
				}
				else if( resize_finger == -1 )
				{
					resize_finger = ev->fingerid;
				}
			}
		}
	}
	else if( ev->type == IE_FingerUp )
	{
		if( ev->fingerid == move_finger )
		{
			move_finger = -1;
			IN_CheckCoords( &selection->x1, &selection->y1, &selection->x2, &selection->y2 );
			selection = nullptr;
		}
		else if( ev->fingerid == resize_finger )
			resize_finger = -1;
	}
	else // IE_FingerMotion
	{
		if( !selection )
			return;

		if( move_finger == ev->fingerid )
		{
			selection->x1 += ev->dx;
			selection->x2 += ev->dx;
			selection->y1 += ev->dy;
			selection->y2 += ev->dy;
		}
		else if( resize_finger == ev->fingerid )
		{
			selection->x2 += ev->dx;
			selection->y2 += ev->dy;
		}
	}
}


void CTouchControls::FingerMotion(touch_event_t *ev) // finger in my ass
{
	const float x = ev->x;
	const float y = ev->y;

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
				yaw += ev->dx;
				pitch += ev->dy;
			}
		}
	}
}

void CTouchControls::FingerPress(touch_event_t *ev)
{
	const float x = ev->x;
	const float y = ev->y;

	CUtlLinkedList<CTouchButton*>::iterator it;

	if( ev->type == IE_FingerDown )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;
			if(  x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )
			{
				if( btn->flags & TOUCH_FL_HIDE )
					continue;

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
						look_finger = ev->fingerid;
					else
						btn->finger = look_finger;
				}
				else
					engine->ClientCmd_Unrestricted( btn->command );
			}
		}
	}
	else if( ev->type == IE_FingerUp )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;

			if( btn->flags & TOUCH_FL_HIDE )
				continue;

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
					engine->ClientCmd_Unrestricted( cmd );
				}
			}
		}
	}
}

void CTouchControls::EnableTouchEdit(bool enable)
{
	if( enable )
	{
		state = state_edit;
		resize_finger = move_finger = look_finger = wheel_finger = -1;
		move_button = NULL;
		configchanged = true;
		AddButton( "close_edit", "vgui/touch/back", "touch_disableedit", 0.010000, 0.837778, 0.080000, 0.980000, rgba_t(255,255,255,255), 0, 1.f, TOUCH_FL_NOEDIT );
	}
	else
	{
		state = state_none;
		resize_finger = move_finger = look_finger = wheel_finger = -1;
		move_button = NULL;
		configchanged = false;
		RemoveButton("close_edit");
		WriteConfig();
	}
}

void CTouchControls::WriteConfig()
{
	FileHandle_t f;
	char newconfigfile[128];
	char oldconfigfile[128];
	char configfile[128];

	if( btns.IsEmpty() )
		return;

	if( CommandLine()->FindParm("-nowriteconfig") )
		return;

	DevMsg( "Touch_WriteConfig(): %s\n", touch_config_file.GetString() );

	Q_snprintf( newconfigfile, 64, "cfg/%s.new", touch_config_file.GetString() );
	Q_snprintf( oldconfigfile, 64, "cfg/%s.bak", touch_config_file.GetString() );
	Q_snprintf( configfile, 64, "cfg/%s", touch_config_file.GetString() );

	f = filesystem->Open( newconfigfile , "w+");

	if( f )
	{
		filesystem->FPrintf( f, "//=======================================================================\n");
		filesystem->FPrintf( f, "//\t\t\ttouchscreen config\n" );
		filesystem->FPrintf( f, "//=======================================================================\n" );
		filesystem->FPrintf( f, "\ntouch_config_file \"%s\"\n", touch_config_file.GetString() );
		filesystem->FPrintf( f, "\n// touch cvars\n" );
		filesystem->FPrintf( f, "\n// sensitivity settings\n" );
		filesystem->FPrintf( f, "touch_pitch \"%f\"\n", touch_pitch.GetFloat() );
		filesystem->FPrintf( f, "touch_yaw \"%f\"\n", touch_yaw.GetFloat() );
		filesystem->FPrintf( f, "touch_forwardzone \"%f\"\n", touch_forwardzone.GetFloat() );
		filesystem->FPrintf( f, "touch_sidezone \"%f\"\n", touch_sidezone.GetFloat() );
/*		filesystem->FPrintf( f, "touch_nonlinear_look \"%d\"\n",touch_nonlinear_look.GetBool() );
		filesystem->FPrintf( f, "touch_pow_factor \"%f\"\n", touch_pow_factor->value );
		filesystem->FPrintf( f, "touch_pow_mult \"%f\"\n", touch_pow_mult->value );
		filesystem->FPrintf( f, "touch_exp_mult \"%f\"\n", touch_exp_mult->value );*/
		filesystem->FPrintf( f, "\n// grid settings\n" );
		filesystem->FPrintf( f, "touch_grid_count \"%d\"\n", touch_grid_count.GetInt() );
		filesystem->FPrintf( f, "touch_grid_enable \"%d\"\n", touch_grid_enable.GetInt() );
/*
		filesystem->FPrintf( f, "\n// global overstroke (width, r, g, b, a)\n" );
		filesystem->FPrintf( f, "touch_set_stroke %d %d %d %d %d\n", touch.swidth, touch.scolor[0], touch.scolor[1], touch.scolor[2], touch.scolor[3] );
		filesystem->FPrintf( f, "\n// highlight when pressed\n" );
		filesystem->FPrintf( f, "touch_highlight_r \"%f\"\n", touch_highlight_r->value );
		filesystem->FPrintf( f, "touch_highlight_g \"%f\"\n", touch_highlight_g->value );
		filesystem->FPrintf( f, "touch_highlight_b \"%f\"\n", touch_highlight_b->value );
		filesystem->FPrintf( f, "touch_highlight_a \"%f\"\n", touch_highlight_a->value );
		filesystem->FPrintf( f, "\n// _joy and _dpad options\n" );
		filesystem->FPrintf( f, "touch_dpad_radius \"%f\"\n", touch_dpad_radius->value );
		filesystem->FPrintf( f, "touch_joy_radius \"%f\"\n", touch_joy_radius->value );
*/
		filesystem->FPrintf( f, "\n// how much slowdown when Precise Look button pressed\n" );
		filesystem->FPrintf( f, "touch_precise_amount \"%f\"\n", touch_precise_amount.GetFloat() );
//		filesystem->FPrintf( f, "\n// enable/disable move indicator\n" );
//		filesystem->FPrintf( f, "touch_move_indicator \"%f\"\n", touch_move_indicator );

		filesystem->FPrintf( f, "\n// reset menu state when execing config\n" );
		//filesystem->FPrintf( f, "touch_setclientonly 0\n" );
		filesystem->FPrintf( f, "\n// touch buttons\n" );
		filesystem->FPrintf( f, "touch_removeall\n" );

		CUtlLinkedList<CTouchButton*>::iterator it;
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *b = *it;
		
			if( b->flags & TOUCH_FL_CLIENT )
				continue; //skip temporary buttons

			if( b->flags & TOUCH_FL_DEF_SHOW )
				b->flags &= ~TOUCH_FL_HIDE;

			if( b->flags & TOUCH_FL_DEF_HIDE )
				b->flags |= TOUCH_FL_HIDE;
			
			float aspect = ( b->y2 - b->y1 ) / ( ( b->x2 - b->x1 )/(screen_h/screen_w) );
	
			filesystem->FPrintf( f, "touch_addbutton \"%s\" \"%s\" \"%s\" %f %f %f %f %d %d %d %d %d %f\n",
				b->name, b->texturefile, b->command,
				b->x1, b->y1, b->x2, b->y2,
				b->color.r, b->color.g, b->color.b, b->color.a, b->flags, aspect );
		}

		filesystem->Close(f);

		filesystem->RemoveFile(oldconfigfile);
		filesystem->RenameFile(configfile, oldconfigfile );
		filesystem->RenameFile(newconfigfile, configfile);
	}
	else DevMsg( "Couldn't write %s.\n", configfile );
}
