//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Public header for panorama UI framework
//
//
//=============================================================================//
#ifndef PANORAMA_H
#define PANORAMA_H
#pragma once

namespace panorama
{

#ifndef PANORAMA_EXPORTS
#define PANORAMA_INTERFACE	
#else
#define PANORAMA_INTERFACE	DLL_EXPORT
#endif

#ifdef PANORAMA_CLIENT_EXPORTS

#if defined(PLATFORM_WINDOWS_PC32) || ( defined( _WIN32 ) && !defined( _WIN64 ) )
#define REFERENCE_PANEL_CORE( className, layoutName ) \
	__pragma( comment( linker, "/INCLUDE:?g_"#layoutName"LinkerHack@panorama@@3PAV"#className"@1@A" ) );
#elif defined(PLATFORM_WINDOWS_PC64) || defined( _WIN64 )
#define REFERENCE_PANEL_CORE( className, layoutName ) \
	__pragma( comment( linker, "/INCLUDE:?g_"#layoutName"LinkerHack@panorama@@3PEAV"#className"@1@EA" ) );
#else
#define REFERENCE_PANEL_CORE( className, layoutName ) \
	class className; \
	extern className *g_##layoutName##LinkerHack;		\
	className *g_##layoutName##PullInModule SELECTANY = g_##layoutName##LinkerHack;
#endif

#define REFERENCE_PANEL( name ) \
	REFERENCE_PANEL_CORE( C##name, name )

// Referenced internal to the framework, so this becomes unneeded
//REFERENCE_PANEL( Image )
//REFERENCE_PANEL( Panel )
REFERENCE_PANEL( Label )
REFERENCE_PANEL( Button )
REFERENCE_PANEL( ToggleButton )
REFERENCE_PANEL( Carousel )
REFERENCE_PANEL( HTML )
REFERENCE_PANEL( TextEntry )
REFERENCE_PANEL( Tooltip )
REFERENCE_PANEL( VerticalScrollList )
REFERENCE_PANEL( DebugLayout )
REFERENCE_PANEL( DebugPanelParents )
REFERENCE_PANEL( DebugAutoComplete )
REFERENCE_PANEL( DebugPanel )
REFERENCE_PANEL( DebugPanelComputed )
REFERENCE_PANEL( DebugPanelStyle )
REFERENCE_PANEL( DebugIndividualStyle )
REFERENCE_PANEL( DebugStyleAnimation )
REFERENCE_PANEL( DebugInheritedStylesHeader )
REFERENCE_PANEL( DebugStyleBlock )
REFERENCE_PANEL( DropDown )
REFERENCE_PANEL( Grid )
REFERENCE_PANEL( ProgressBar )
REFERENCE_PANEL( ContextMenu )
REFERENCE_PANEL( SimpleContextMenu )
REFERENCE_PANEL( Slider )
REFERENCE_PANEL( ListSegmentView )
REFERENCE_PANEL( AnimatedImageStrip )
REFERENCE_PANEL_CORE( CMoviePlayer, Movie )
REFERENCE_PANEL( MoviePanel )
REFERENCE_PANEL( VolumeSliderPopup )
REFERENCE_PANEL_CORE( CMovieVideoQualityPopup, VideoQualityPopup )
REFERENCE_PANEL( EdgeScroller )
#endif

} // namespace panorama

#endif // PANORAMA_H
