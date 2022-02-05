typedef unsigned int   uint;

#include "../public/togles/linuxwin/glmdisplay.h"
#include "../public/togles/linuxwin/glmdisplaydb.h"

void GLMDisplayDB::PopulateRenderers( void ) { }
void GLMDisplayDB::PopulateFakeAdapters( uint realRendererIndex ) { }		// fake adapters = one real adapter times however many displays are on 
void GLMDisplayDB::Populate( void ) { }
int	 GLMDisplayDB::GetFakeAdapterCount( void ) { return 1; }
bool  GLMDisplayDB::GetFakeAdapterInfo( int fakeAdapterIndex, int *rendererOut, int *displayOut, GLMRendererInfoFields *rendererInfoOut, GLMDisplayInfoFields *displayInfoOut ) { return true; }
int	 GLMDisplayDB::GetRendererCount( void ) { return 1; }
bool  GLMDisplayDB::GetRendererInfo( int rendererIndex, GLMRendererInfoFields *infoOut ) { return true; }
int	 GLMDisplayDB::GetDisplayCount( int rendererIndex ) { return 1; }
bool  GLMDisplayDB::GetDisplayInfo( int rendererIndex, int displayIndex, GLMDisplayInfoFields *infoOut ) { return true; }
int	 GLMDisplayDB::GetModeCount( int rendererIndex, int displayIndex ) { } 
bool  GLMDisplayDB::GetModeInfo( int rendererIndex, int displayIndex, int modeIndex, GLMDisplayModeInfoFields *infoOut ) { return false; }
void  GLMDisplayDB::Dump( void ) { }
