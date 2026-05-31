//============= Copyright (c) StargateTC Source Team ==========================
//
// Client-side MCP debug console commands. Companion to sg_mcp_debug.cpp on
// the server.
//
// Client-only — only compiled into client.dll.
//
//=============================================================================
#ifdef CLIENT_DLL

#include "cbase.h"
#include "c_baseentity.h"
#include "c_baseplayer.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/IInputInternal.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/iinputsystem.h"

#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// sg_cl_ping — client liveness probe.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_ping, "Client: echo CL_PONG + map + local player info." )
{
    C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
    Msg( "CL_PONG map=%s local_player=%s\n",
         engine->GetLevelName(),
         pPlayer ? pPlayer->GetPlayerName() : "none" );
}

//-----------------------------------------------------------------------------
// sg_cl_list_entities — dump client-side entity table.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_list_entities, "Client: dump client entity table." )
{
    const char *pFilter = args.ArgC() > 1 ? args.Arg( 1 ) : NULL;
    int nCount = 0;
    for ( C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity();
          pEnt;
          pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
    {
        const char *pClass = pEnt->GetClassname();
        if ( pFilter && pFilter[0] && !Q_stristr( pClass, pFilter ) )
            continue;

        Vector v = pEnt->GetAbsOrigin();
        Msg( "CL_ENT idx=%d class=%s origin=%.1f,%.1f,%.1f visible=%d\n",
             pEnt->entindex(),
             pClass,
             v.x, v.y, v.z,
             pEnt->ShouldDraw() ? 1 : 0 );
        ++nCount;
    }
    Msg( "CL_ENT_COUNT %d\n", nCount );
}

//-----------------------------------------------------------------------------
// sg_cl_player — local player state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_player, "Client: dump local player state." )
{
    C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
    if ( !pPlayer )
    {
        Msg( "CL_PLAYER none\n" );
        return;
    }
    Vector v = pPlayer->GetAbsOrigin();
    QAngle a = pPlayer->GetAbsAngles();
    C_BaseCombatWeapon *pWep = pPlayer->GetActiveWeapon();
    Msg( "CL_PLAYER name=%s team=%d health=%d origin=%.1f,%.1f,%.1f angles=%.1f,%.1f,%.1f weapon=%s\n",
         pPlayer->GetPlayerName(),
         pPlayer->GetTeamNumber(),
         pPlayer->GetHealth(),
         v.x, v.y, v.z,
         a.x, a.y, a.z,
         pWep ? pWep->GetClassname() : "-" );
}

//-----------------------------------------------------------------------------
// sg_cl_check_materials [filter]
// Walk the material system and flag missing/error materials.
// Helps diagnose the pink-checker placeholder problem.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_check_materials, "Client: scan materials and report errors." )
{
    if ( !materials )
    {
        Msg( "CL_MAT_FAIL no material system\n" );
        return;
    }

    const char *pFilter = args.ArgC() > 1 ? args.Arg( 1 ) : NULL;

    MaterialHandle_t h = materials->FirstMaterial();
    int nTotal = 0;
    int nErrors = 0;
    while ( h != materials->InvalidMaterial() )
    {
        IMaterial *pMat = materials->GetMaterial( h );
        if ( pMat )
        {
            const char *pName = pMat->GetName();
            bool bIsError = pMat->IsErrorMaterial();
            if ( pFilter && pFilter[0] && !Q_stristr( pName, pFilter ) )
            {
                h = materials->NextMaterial( h );
                continue;
            }
            if ( bIsError )
            {
                Msg( "CL_MAT_ERR name=%s\n", pName );
                ++nErrors;
            }
            else if ( pFilter && pFilter[0] )
            {
                Msg( "CL_MAT_OK name=%s shader=%s\n",
                     pName, pMat->GetShaderName() ? pMat->GetShaderName() : "-" );
            }
            ++nTotal;
        }
        h = materials->NextMaterial( h );
    }
    Msg( "CL_MAT_SUMMARY total=%d errors=%d\n", nTotal, nErrors );
}

//-----------------------------------------------------------------------------
// sg_cl_net — report client network state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_net, "Client: dump network connection state." )
{
    if ( !engine )
    {
        Msg( "CL_NET_FAIL no engine\n" );
        return;
    }
    Msg( "CL_NET in_game=%d connected=%d level=%s server=%s\n",
         engine->IsInGame() ? 1 : 0,
         engine->IsConnected() ? 1 : 0,
         engine->GetLevelName(),
         engine->GetGameDirectory() );
}

//-----------------------------------------------------------------------------
// sg_cl_debugbreak — controlled DebugBreak on client side.
//-----------------------------------------------------------------------------
CON_COMMAND_F( sg_cl_debugbreak, "Client: trigger DebugBreak() for verifying attached debugger.", FCVAR_CHEAT )
{
    Msg( "Client: about to DebugBreak — if a debugger is attached it will catch this.\n" );
    DebuggerBreak();
}

//-----------------------------------------------------------------------------
// sg_cl_vgui_tree [filter] — recursively walk vgui panel tree.
// Emits one line per panel:
//   VGUI depth=N vpanel=PTR name=<name> class=<class> pos=x,y size=w,h
//        visible=0/1 enabled=0/1 alpha=A keyboardinput=0/1 mouseinput=0/1
//-----------------------------------------------------------------------------
static void SG_DumpPanelRecursive( VPANEL vp, int depth, const char *pFilter, int &nEmitted )
{
    if ( !vp ) return;
    IPanel *ip = ipanel();
    if ( !ip ) return;

    const char *pName = ip->GetName( vp );
    const char *pClass = ip->GetClassName( vp );
    if ( !pName ) pName = "-";
    if ( !pClass ) pClass = "-";

    if ( !pFilter || !pFilter[0] ||
         Q_stristr( pName, pFilter ) || Q_stristr( pClass, pFilter ) )
    {
        int x = 0, y = 0, w = 0, h = 0;
        ip->GetAbsPos( vp, x, y );
        ip->GetSize( vp, w, h );
        Msg( "VGUI depth=%d vpanel=%p name=%s class=%s pos=%d,%d size=%d,%d "
             "visible=%d enabled=%d keyboardinput=%d mouseinput=%d\n",
             depth, (void*)vp, pName, pClass, x, y, w, h,
             ip->IsVisible( vp ) ? 1 : 0,
             ip->IsEnabled( vp ) ? 1 : 0,
             ip->IsKeyBoardInputEnabled( vp ) ? 1 : 0,
             ip->IsMouseInputEnabled( vp ) ? 1 : 0 );
        ++nEmitted;
    }

    int nChildren = ip->GetChildCount( vp );
    for ( int i = 0; i < nChildren; ++i )
    {
        VPANEL child = ip->GetChild( vp, i );
        SG_DumpPanelRecursive( child, depth + 1, pFilter, nEmitted );
    }
}

CON_COMMAND( sg_cl_vgui_tree, "Client: dump the VGUI panel tree (optional substring filter)." )
{
    const char *pFilter = args.ArgC() > 1 ? args.Arg( 1 ) : NULL;
    if ( !vgui::ivgui() || !ipanel() || !surface() )
    {
        Msg( "VGUI_FAIL no_vgui\n" );
        return;
    }
    // GetEmbeddedPanel() is the top-level VGUI root that contains the HUD,
    // menus, console, etc.
    VPANEL root = surface()->GetEmbeddedPanel();
    if ( !root )
    {
        Msg( "VGUI_FAIL no_root\n" );
        return;
    }
    int nEmitted = 0;
    SG_DumpPanelRecursive( root, 0, pFilter, nEmitted );
    Msg( "VGUI_COUNT %d\n", nEmitted );
}

//-----------------------------------------------------------------------------
// sg_cl_vgui_visible — list ONLY visible panels at the top of the tree
// (useful when investigating "what's on screen right now").
//-----------------------------------------------------------------------------
static void SG_DumpVisibleRecursive( VPANEL vp, int depth, int &nEmitted )
{
    IPanel *ip = ipanel();
    if ( !vp || !ip ) return;
    if ( !ip->IsVisible( vp ) ) return;  // prune invisible subtrees

    int x = 0, y = 0, w = 0, h = 0;
    ip->GetAbsPos( vp, x, y );
    ip->GetSize( vp, w, h );
    const char *pName = ip->GetName( vp );
    const char *pClass = ip->GetClassName( vp );
    Msg( "VGUI_VIS depth=%d vpanel=%p name=%s class=%s pos=%d,%d size=%d,%d\n",
         depth, (void*)vp,
         pName ? pName : "-",
         pClass ? pClass : "-",
         x, y, w, h );
    ++nEmitted;

    int n = ip->GetChildCount( vp );
    for ( int i = 0; i < n; ++i )
    {
        SG_DumpVisibleRecursive( ip->GetChild( vp, i ), depth + 1, nEmitted );
    }
}

CON_COMMAND( sg_cl_vgui_visible, "Client: dump only currently-visible panels." )
{
    if ( !surface() ) { Msg( "VGUI_FAIL no_surface\n" ); return; }
    VPANEL root = surface()->GetEmbeddedPanel();
    if ( !root ) { Msg( "VGUI_FAIL no_root\n" ); return; }
    int n = 0;
    SG_DumpVisibleRecursive( root, 0, n );
    Msg( "VGUI_VIS_COUNT %d\n", n );
}

//-----------------------------------------------------------------------------
// sg_cl_set_cursor <x> <y> — move VGUI cursor to absolute screen pixel pos.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_set_cursor, "Client: move VGUI cursor to <x> <y>." )
{
    if ( args.ArgC() < 3 ) { Msg( "Usage: sg_cl_set_cursor <x> <y>\n" ); return; }
    int x = atoi( args.Arg(1) ), y = atoi( args.Arg(2) );
    if ( !vgui::input() ) { Msg( "CURSOR_FAIL no_input\n" ); return; }
    vgui::input()->SetCursorPos( x, y );
    Msg( "CURSOR ok x=%d y=%d\n", x, y );
}

//-----------------------------------------------------------------------------
// sg_cl_click <x> <y> [button] — synthesize a mouse click at the given coords.
// button: "left" (default), "right", "middle".
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_click, "Client: VGUI mouse click at <x> <y> [left|right|middle]." )
{
    if ( args.ArgC() < 3 ) { Msg( "Usage: sg_cl_click <x> <y> [left|right|middle]\n" ); return; }
    IInputInternal *pInputInternal = (IInputInternal *)vgui::input();
    if ( !pInputInternal ) { Msg( "CLICK_FAIL no_input\n" ); return; }
    int x = atoi( args.Arg(1) ), y = atoi( args.Arg(2) );
    const char *btn = args.ArgC() >= 4 ? args.Arg(3) : "left";
    MouseCode code = MOUSE_LEFT;
    if ( !Q_stricmp( btn, "right" ) )       code = MOUSE_RIGHT;
    else if ( !Q_stricmp( btn, "middle" ) ) code = MOUSE_MIDDLE;

    pInputInternal->SetCursorPos( x, y );
    pInputInternal->InternalCursorMoved( x, y );
    pInputInternal->UpdateMouseFocus( x, y );
    pInputInternal->InternalMousePressed( code );
    pInputInternal->InternalMouseReleased( code );
    Msg( "CLICK ok x=%d y=%d button=%s\n", x, y, btn );
}

//-----------------------------------------------------------------------------
// sg_cl_double_click <x> <y> — double-click at given coords (left button).
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_double_click, "Client: VGUI double-click at <x> <y>." )
{
    if ( args.ArgC() < 3 ) { Msg( "Usage: sg_cl_double_click <x> <y>\n" ); return; }
    IInputInternal *pInputInternal = (IInputInternal *)vgui::input();
    if ( !pInputInternal ) { Msg( "DBLCLICK_FAIL no_input\n" ); return; }
    int x = atoi( args.Arg(1) ), y = atoi( args.Arg(2) );
    pInputInternal->SetCursorPos( x, y );
    pInputInternal->InternalCursorMoved( x, y );
    pInputInternal->UpdateMouseFocus( x, y );
    pInputInternal->InternalMouseDoublePressed( MOUSE_LEFT );
    pInputInternal->InternalMouseReleased( MOUSE_LEFT );
    Msg( "DBLCLICK ok x=%d y=%d\n", x, y );
}

//-----------------------------------------------------------------------------
// sg_cl_keypress <keyname> — fire a VGUI key code press.
// Examples: KEY_ENTER, KEY_ESCAPE, KEY_TAB, KEY_F1, KEY_A.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_keypress, "Client: synthesize VGUI key press by KEY_* name." )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_cl_keypress <KEY_NAME>\n" ); return; }
    IInputInternal *pInputInternal = (IInputInternal *)vgui::input();
    if ( !pInputInternal || !g_pInputSystem ) { Msg( "KEY_FAIL no_input\n" ); return; }
    ButtonCode_t bc = g_pInputSystem->StringToButtonCode( args.Arg(1) );
    KeyCode k = (KeyCode)bc;  // ButtonCode and KeyCode share the same enum space
    if ( bc == BUTTON_CODE_INVALID ) { Msg( "KEY_FAIL unknown=%s\n", args.Arg(1) ); return; }
    pInputInternal->InternalKeyCodePressed( k );
    pInputInternal->InternalKeyCodeTyped( k );
    pInputInternal->InternalKeyCodeReleased( k );
    Msg( "KEYPRESS ok key=%s code=%d\n", args.Arg(1), (int)k );
}

//-----------------------------------------------------------------------------
// sg_cl_type <text> — synthesize character input for a string (supports _).
// Each character is sent via InternalKeyTyped, which writes into the focused
// text-entry panel as if the user had typed it.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_type, "Client: type a string into the focused VGUI text panel." )
{
    if ( args.ArgC() < 2 ) { Msg( "Usage: sg_cl_type <text...>\n" ); return; }
    IInputInternal *pInputInternal = (IInputInternal *)vgui::input();
    if ( !pInputInternal ) { Msg( "TYPE_FAIL no_input\n" ); return; }
    // ArgS returns everything after argv[0], with quoting intact.
    const char *pText = args.ArgS();
    int nChars = 0;
    for ( const char *p = pText; *p; ++p, ++nChars )
    {
        pInputInternal->InternalKeyTyped( (wchar_t)(unsigned char)*p );
    }
    Msg( "TYPE ok chars=%d text=%s\n", nChars, pText );
}

//-----------------------------------------------------------------------------
// sg_cl_screen_info — overall screen + cursor state.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_screen_info, "Client: screen size + cursor position." )
{
    int sw = 0, sh = 0;
    if ( surface() ) surface()->GetScreenSize( sw, sh );
    int mx = 0, my = 0;
    if ( vgui::input() ) vgui::input()->GetCursorPos( mx, my );
    Msg( "CL_SCREEN width=%d height=%d cursor=%d,%d\n", sw, sh, mx, my );
}

//-----------------------------------------------------------------------------
// sg_cl_list_missing_models — walk client entity table, log any with NULL model.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_list_missing_models, "Client: report entities whose model failed to load." )
{
    int nMissing = 0;
    int nChecked = 0;
    for ( C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity();
          pEnt;
          pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
    {
        ++nChecked;
        const model_t *pModel = pEnt->GetModel();
        const char *pModelName = STRING( pEnt->GetModelName() );
        if ( pModelName && pModelName[0] && !pModel )
        {
            Msg( "CL_MISSING_MODEL idx=%d class=%s model=%s\n",
                 pEnt->entindex(), pEnt->GetClassname(), pModelName );
            ++nMissing;
        }
    }
    Msg( "CL_MISSING_MODEL_SUMMARY checked=%d missing=%d\n", nChecked, nMissing );
}

//-----------------------------------------------------------------------------
// sg_cl_dump_render_info — log local player's render-side debugging data.
//-----------------------------------------------------------------------------
CON_COMMAND( sg_cl_dump_render_info, "Client: dump local player render info." )
{
    C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
    if ( !pPlayer )
    {
        Msg( "CL_RENDER_FAIL no_player\n" );
        return;
    }
    Vector eye = pPlayer->EyePosition();
    QAngle eyeAng = pPlayer->EyeAngles();
    Msg( "CL_RENDER eye_pos=%.1f,%.1f,%.1f eye_ang=%.1f,%.1f,%.1f "
         "view_offset=%.1f viewmodel=%d\n",
         eye.x, eye.y, eye.z,
         eyeAng.x, eyeAng.y, eyeAng.z,
         pPlayer->GetViewOffset().z,
         pPlayer->GetViewModel() ? 1 : 0 );
}

#endif // CLIENT_DLL
