//========= Copyright  1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

//===============================================================================

GLMRendererInfo::GLMRendererInfo( void )
{
        m_display = NULL;
        Q_memset( &m_info, 0, sizeof( m_info ) );
}

GLMRendererInfo::~GLMRendererInfo( void )
{
        SDLAPP_FUNC;

        if (m_display)
        {
                delete m_display;
                m_display = NULL;
        }
}

// !!! FIXME: sync this function with the Mac version in case anything important has changed.
void GLMRendererInfo::Init( GLMRendererInfoFields *info )
{
        SDLAPP_FUNC;

        m_info = *info;
        m_display = NULL;

        m_info.m_fullscreen = 0;
        m_info.m_accelerated = 1;
        m_info.m_windowed = 1;
        
        m_info.m_ati = true;
        m_info.m_atiNewer = true;

        m_info.m_hasGammaWrites = true;

        // If you haven't created a GL context by now (and initialized gGL), you're about to crash.

        m_info.m_hasMixedAttachmentSizes = gGL->m_bHave_GL_ARB_framebuffer_object;
        m_info.m_hasBGRA = gGL->m_bHave_GL_EXT_vertex_array_bgra;

        // !!! FIXME: what do these do on the Mac?
        m_info.m_hasNewFullscreenMode = false;
        m_info.m_hasNativeClipVertexMode = true;

        // if user disabled them
        if (CommandLine()->FindParm("-glmdisableclipplanes"))
        {
                m_info.m_hasNativeClipVertexMode = false;
        }
        
        // or maybe enabled them..
        if (CommandLine()->FindParm("-glmenableclipplanes"))
        {
                m_info.m_hasNativeClipVertexMode = true;
        }
        
        m_info.m_hasOcclusionQuery = gGL->m_bHave_GL_ARB_occlusion_query;
        m_info.m_hasFramebufferBlit = gGL->m_bHave_GL_EXT_framebuffer_blit || gGL->m_bHave_GL_ARB_framebuffer_object;

        GLint nMaxAniso = 0;
        gGL->glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &nMaxAniso );
        m_info.m_maxAniso = clamp<int>( nMaxAniso, 0, 16 );
        
        // We don't currently used bindable uniforms, but I've been experimenting with them so I might as well check this in just in case they turn out to be useful.
        m_info.m_hasBindableUniforms = gGL->m_bHave_GL_EXT_bindable_uniform;
        m_info.m_hasBindableUniforms = false;           // !!! FIXME hardwiring this path to false until we see how to accelerate it properly
        m_info.m_maxVertexBindableUniforms = 0;
        m_info.m_maxFragmentBindableUniforms = 0;
        m_info.m_maxBindableUniformSize = 0;
        
        if (m_info.m_hasBindableUniforms)
        {
                gGL->glGetIntegerv(GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT, &m_info.m_maxVertexBindableUniforms);
                gGL->glGetIntegerv(GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT, &m_info.m_maxFragmentBindableUniforms);
                gGL->glGetIntegerv(GL_MAX_BINDABLE_UNIFORM_SIZE_EXT, &m_info.m_maxBindableUniformSize);
                if ( ( m_info.m_maxVertexBindableUniforms < 1 ) || ( m_info.m_maxFragmentBindableUniforms < 1 ) || ( m_info.m_maxBindableUniformSize < ( sizeof( float ) * 4 * 256 ) ) )
                {
                        m_info.m_hasBindableUniforms = false;
                }
        }
                
        m_info.m_hasUniformBuffers =  gGL->m_bHave_GL_ARB_uniform_buffer;
        m_info.m_hasPerfPackage1 = true;  // this flag is Mac-specific. We do slower things if you don't have Mac OS X 10.x.y or later. Linux always does the fast path!

        //-------------------------------------------------------------------
        // runtime options that aren't negotiable once set

        m_info.m_hasDualShaders = CommandLine()->FindParm("-glmdualshaders") != 0;

        //-------------------------------------------------------------------
        // "can'ts "

#if defined( OSX )
        m_info.m_cantBlitReliably = m_info.m_intel;             //FIXME X3100&10.6.3 has problems blitting.. adjust this if bug fixed in 10.6.4
#else
    // m_cantBlitReliably path doesn't work right now, and the Intel path is different for us on Linux/Win7 anyway
        m_info.m_cantBlitReliably = false;
#endif
                
        if (CommandLine()->FindParm("-glmenabletrustblit"))
        {
                m_info.m_cantBlitReliably = false;                      // we trust the blit, so set the cant-blit cap to false
        }
        if (CommandLine()->FindParm("-glmdisabletrustblit"))
        {
                m_info.m_cantBlitReliably = true;                       // we do not trust the blit, so set the cant-blit cap to true
        }

        // MSAA resolve issues
        m_info.m_cantResolveFlipped     = false;
        

#if defined( OSX )
        m_info.m_cantResolveScaled = true;                                                              // generally true until new extension ships     
#else
        // DON'T just slam this to false and run without first testing with -gl_debug enabled on NVidia/AMD/etc.
        // This path needs the m_bHave_GL_EXT_framebuffer_multisample_blit_scaled extension.
        m_info.m_cantResolveScaled = true;
                
        if ( gGL->m_bHave_GL_EXT_framebuffer_multisample_blit_scaled )
        {
                m_info.m_cantResolveScaled = false;
        }
#endif
        
        // gamma decode impacting shader codegen
        m_info.m_costlyGammaFlips = false;
}

void    GLMRendererInfo::PopulateDisplays()
{
        SDLAPP_FUNC;

        Assert( !m_display );
        m_display = new GLMDisplayInfo;

        // Populate display mode table.
        m_display->PopulateModes();
}


void    GLMRendererInfo::Dump( int which )
{
        SDLAPP_FUNC;

        GLMPRINTF(("\n     #%d: GLMRendererInfo @ %p, renderer-id=(%08x)  display-mask=%08x  vram=%dMB",
                which, this,
                m_info.m_rendererID,
                m_info.m_displayMask,
                m_info.m_vidMemory >> 20
        ));
        GLMPRINTF(("\n       VendorID=%04x  DeviceID=%04x  Model=%s",
                m_info.m_pciVendorID,
                m_info.m_pciDeviceID,
                m_info.m_pciModelString
        ));

        m_display->Dump( which );
}




GLMDisplayDB::GLMDisplayDB ()
{
        SDLAPP_FUNC;

        m_renderer.m_display = NULL;    
}

GLMDisplayDB::~GLMDisplayDB     ( void )
{
        SDLAPP_FUNC;

        if ( m_renderer.m_display )
        {
                delete m_renderer.m_display;
                m_renderer.m_display = NULL;
        }
}

#ifndef GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX                 0x9047
#endif

#ifndef GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#endif

#ifndef GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#endif

#ifndef GL_VBO_FREE_MEMORY_ATI
#define GL_VBO_FREE_MEMORY_ATI                                                  0x87FB
#endif

#ifndef GL_TEXTURE_FREE_MEMORY_ATI
#define GL_TEXTURE_FREE_MEMORY_ATI                                              0x87FC
#endif

#ifndef GL_RENDERBUFFER_FREE_MEMORY_ATI
#define GL_RENDERBUFFER_FREE_MEMORY_ATI                                 0x87FD
#endif
        
void GLMDisplayDB::PopulateRenderers( void )
{
        SDLAPP_FUNC;

        Assert( !m_renderer.m_display );

        GLMRendererInfoFields   fields;
        memset( &fields, 0, sizeof(fields) );

        // Assume 512MB of available video memory
        fields.m_vidMemory = 512 * 1024 * 1024;
        
        DebugPrintf( "GL_NVX_gpu_memory_info: %s\n", gGL->m_bHave_GL_NVX_gpu_memory_info ? "AVAILABLE" : "UNAVAILABLE" );
        DebugPrintf( "GL_ATI_meminfo: %s\n", gGL->m_bHave_GL_ATI_meminfo ? "AVAILABLE" : "UNAVAILABLE" );

        if ( gGL->m_bHave_GL_NVX_gpu_memory_info )
        {
                gGL->glGetError();

                GLint nTotalDedicated = 0, nTotalAvail = 0, nCurrentAvail = 0;
                gGL->glGetIntegerv( GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &nTotalDedicated );
                gGL->glGetIntegerv( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &nTotalAvail );
                gGL->glGetIntegerv( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &nCurrentAvail );

                if ( gGL->glGetError() )
                {
                        DebugPrintf( "GL_NVX_gpu_memory_info: Failed retrieving available GPU memory\n" );
                }
                else
                {
                        DebugPrintf( "GL_NVX_gpu_memory_info: Total Dedicated: %u, Total Avail: %u, Current Avail: %u\n", nTotalDedicated, nTotalAvail, nCurrentAvail );

                        // Try to do something reasonable. Should we report dedicated or total available to the engine here?
                        // For now, just take the MAX of both.
                        uint64 nActualAvail = static_cast<uint64>( MAX( nTotalAvail, nTotalDedicated ) ) * 1024;
                        fields.m_vidMemory = static_cast< GLint >( MIN( nActualAvail, 0x7FFFFFFF ) );
                }
        }
        else if ( gGL->m_bHave_GL_ATI_meminfo )
        {
                // As of 10/8/12 this extension is only available under Linux and Windows FireGL parts.
                gGL->glGetError();

                GLint nAvail[4] = { 0, 0, 0, 0 };
                gGL->glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI, nAvail );

                if ( gGL->glGetError() )
                {
                        DebugPrintf( "GL_ATI_meminfo: Failed retrieving available GPU memory\n" );
                }
                else
                {
                        // param[0] - total memory free in the pool
                        // param[1] - largest available free block in the pool
                        // param[2] - total auxiliary memory free
                        // param[3] - largest auxiliary free block

                        DebugPrintf( "GL_ATI_meminfo: GL_TEXTURE_FREE_MEMORY_ATI: Total Free: %i, Largest Avail: %i, Total Aux: %i, Largest Aux Avail: %i\n", 
                                nAvail[0], nAvail[1], nAvail[2], nAvail[3] );

                        uint64 nActualAvail = static_cast<uint64>( nAvail[0] ) * 1024;
                        fields.m_vidMemory = static_cast< GLint >( MIN( nActualAvail, 0x7FFFFFFF ) );
                }
        }

        // Clamp the min amount of video memory to 256MB in case a query returned something bogus, or we interpreted it badly.
        fields.m_vidMemory = MAX( fields.m_vidMemory, 128 * 1024 * 1024 );
        fields.m_texMemory = fields.m_vidMemory;

        fields.m_pciVendorID = GLM_OPENGL_VENDOR_ID;
        fields.m_pciDeviceID = GLM_OPENGL_DEFAULT_DEVICE_ID;
        if ( ( gGL->m_nDriverProvider == cGLDriverProviderIntel ) || ( gGL->m_nDriverProvider == cGLDriverProviderIntelOpenSource ) )
        {
                fields.m_pciDeviceID = GLM_OPENGL_LOW_PERF_DEVICE_ID;
        }
        
/*      fields.m_colorModes = (uint)-1;
        fields.m_bufferModes = (uint)-1;
        fields.m_depthModes = (uint)-1;
        fields.m_stencilModes = (uint)-1;
        fields.m_maxAuxBuffers = (uint)128;
        fields.m_maxSampleBuffers = (uint)128;
        fields.m_maxSamples = (uint)2048;
        fields.m_sampleModes = (uint)128;
        fields.m_sampleAlpha = (uint)32;
*/

        GLint nMaxMultiSamples = 0;
        gGL->glGetIntegerv( GL_MAX_SAMPLES_EXT, &nMaxMultiSamples );
        fields.m_maxSamples = clamp<int>( nMaxMultiSamples, 0, 8 );
        DebugPrintf( "GL_MAX_SAMPLES_EXT: %i\n", nMaxMultiSamples );

        // We only have one GLMRendererInfo on Linux, unlike Mac OS X. Whatever libGL.so wants to do, we go with it.
        m_renderer.Init( &fields );

        // then go back and ask each renderer to populate its display info table.
        m_renderer.PopulateDisplays();
}



void    GLMDisplayDB::PopulateFakeAdapters( uint realRendererIndex )            // fake adapters = one real adapter times however many displays are on it
{
        SDLAPP_FUNC;

        Assert( realRendererIndex == 0 );
}

void    GLMDisplayDB::Populate(void)
{
        SDLAPP_FUNC;

        this->PopulateRenderers();
        
        this->PopulateFakeAdapters( 0 );

        #if GLMDEBUG
                this->Dump();
        #endif
}
        


int             GLMDisplayDB::GetFakeAdapterCount( void )
{
        SDLAPP_FUNC;

        return 1;
}

bool    GLMDisplayDB::GetFakeAdapterInfo( int fakeAdapterIndex, int *rendererOut, int *displayOut, GLMRendererInfoFields *rendererInfoOut, GLMDisplayInfoFields *displayInfoOut )
{
        SDLAPP_FUNC;

        if (fakeAdapterIndex >= GetFakeAdapterCount() )
        {
                *rendererOut = 0;
                *displayOut = 0;
                return true;            // fail
        }

        *rendererOut = 0;
        *displayOut = 0;

        bool rendResult = GetRendererInfo( *rendererOut, rendererInfoOut );
        bool dispResult = GetDisplayInfo( *rendererOut, *displayOut, displayInfoOut );
        
        return rendResult || dispResult;
}
        

int             GLMDisplayDB::GetRendererCount( void )
{
        SDLAPP_FUNC;

        return 1;
}

bool    GLMDisplayDB::GetRendererInfo( int rendererIndex, GLMRendererInfoFields *infoOut )
{
        SDLAPP_FUNC;

        memset( infoOut, 0, sizeof( GLMRendererInfoFields ) );

        if (rendererIndex >= GetRendererCount())
                return true; // fail
        
        *infoOut = m_renderer.m_info;

        return false;
}

int             GLMDisplayDB::GetDisplayCount( int rendererIndex )
{
        SDLAPP_FUNC;

        if (rendererIndex >= GetRendererCount())
        {
                Assert( 0 );
                return 0; // fail
        }
        
        return 1;
}

bool    GLMDisplayDB::GetDisplayInfo( int rendererIndex, int displayIndex, GLMDisplayInfoFields *infoOut )
{
        SDLAPP_FUNC;

        memset( infoOut, 0, sizeof( GLMDisplayInfoFields ) );
        
        if (rendererIndex >= GetRendererCount())
                return true; // fail
        
        if (displayIndex >= GetDisplayCount(rendererIndex))
                return true; // fail
        
        *infoOut = m_renderer.m_display->m_info;

        return false;
}

int             GLMDisplayDB::GetModeCount( int rendererIndex, int displayIndex )
{
        SDLAPP_FUNC;

        if (rendererIndex >= GetRendererCount())
                return 0; // fail
        
        if (displayIndex >= GetDisplayCount(rendererIndex))
                return 0; // fail
                
        return m_renderer.m_display->m_modes->Count();
}

bool    GLMDisplayDB::GetModeInfo( int rendererIndex, int displayIndex, int modeIndex, GLMDisplayModeInfoFields *infoOut )
{
        SDLAPP_FUNC;

        memset( infoOut, 0, sizeof( GLMDisplayModeInfoFields ) );
        
        if ( rendererIndex >= GetRendererCount())
                return true; // fail
        
        if (displayIndex >= GetDisplayCount( rendererIndex ) )
                return true; // fail
        
        if ( modeIndex >= GetModeCount( rendererIndex, displayIndex ) )
                return true; // fail
        
        if ( modeIndex >= 0 )
        {
                GLMDisplayMode *displayModeInfo = m_renderer.m_display->m_modes->Element( modeIndex );

                *infoOut = displayModeInfo->m_info;
        }
        else
        {
                const GLMDisplayInfoFields &info = m_renderer.m_display->m_info;

                infoOut->m_modePixelWidth = info.m_displayPixelWidth;
                infoOut->m_modePixelHeight = info.m_displayPixelHeight;
                infoOut->m_modeRefreshHz = 0;

                //return true; // fail
        }

        return false;
}


void    GLMDisplayDB::Dump( void )
{
        SDLAPP_FUNC;

        GLMPRINTF(("\n GLMDisplayDB @ %p ",this ));

        m_renderer.Dump( 0 );
}

//===============================================================================

GLMDisplayInfo::GLMDisplayInfo()
{
        SDLAPP_FUNC;

        m_modes = NULL;

        int Width, Height;
        GetLargestDisplaySize( Width, Height );

        m_info.m_displayPixelWidth = ( uint )Width;
        m_info.m_displayPixelHeight = ( uint )Height;
}

GLMDisplayInfo::~GLMDisplayInfo( void )
{
        SDLAPP_FUNC;
}

extern "C" int DisplayModeSortFunction( GLMDisplayMode * const *A, GLMDisplayMode * const *B )
{
        SDLAPP_FUNC;

        int bigger = -1;
        int smaller = 1;        // adjust these for desired ordering

        // check refreshrate - higher should win
        if ( (*A)->m_info.m_modeRefreshHz > (*B)->m_info.m_modeRefreshHz )
        {       
                return bigger;
        }
        else if ( (*A)->m_info.m_modeRefreshHz < (*B)->m_info.m_modeRefreshHz )
        {
                return smaller;
        }

        // check area - larger mode should win
        int areaa = (*A)->m_info.m_modePixelWidth * (*A)->m_info.m_modePixelHeight;
        int areab = (*B)->m_info.m_modePixelWidth * (*B)->m_info.m_modePixelHeight;

        if ( areaa > areab )
        {       
                return bigger;
        }
        else if ( areaa < areab )
        {
                return smaller;
        }
        
        return 0;       // equal rank
}


void    GLMDisplayInfo::PopulateModes( void )
{
        SDLAPP_FUNC;

        Assert( !m_modes );
        m_modes = new CUtlVector< GLMDisplayMode* >;

        int nummodes = SDL_GetNumVideoDisplays();

        for ( int i = 0; i < nummodes; i++ )
        {
                SDL_Rect rect = { 0, 0, 0, 0 };

                if ( !SDL_GetDisplayBounds( i, &rect ) && rect.w && rect.h )
                {
                        m_modes->AddToTail( new GLMDisplayMode( rect.w, rect.h, 0 ) );
                }
        }

        // Add a big pile of window resolutions.
        static const struct
        {
                uint w;
                uint h;
        } s_Resolutions[] =
        {
                { 640, 480 },   // 4x3
                { 800, 600 },
                { 1024, 768 },
                { 1152, 864 },
                { 1280, 960 },
                { 1600, 1200 },
                { 1920, 1440 },
                { 2048, 1536 },

                { 1280, 720 },  // 16x9
                { 1366, 768 },
                { 1600, 900 },
                { 1920, 1080 },

                { 720, 480 },   // 16x10
                { 1280, 800 },
                { 1680, 1050 },
                { 1920, 1200 },
                { 2560, 1600 },
        };

        for ( int i = 0; i < ARRAYSIZE( s_Resolutions ); i++ )
        {
                uint w = s_Resolutions[ i ].w;
                uint h = s_Resolutions[ i ].h;

                if ( ( w <= m_info.m_displayPixelWidth ) && ( h <= m_info.m_displayPixelHeight ) )
                {
                        m_modes->AddToTail( new GLMDisplayMode( w, h, 0 ) );

                        if ( ( w * 2 <= m_info.m_displayPixelWidth ) && ( h * 2 < m_info.m_displayPixelHeight ) )
                        {
                                // Add double of everything also - Retina proofing hopefully.
                                m_modes->AddToTail( new GLMDisplayMode( w * 2, h * 2, 0 ) );
                        }
                }
        }

        m_modes->Sort( DisplayModeSortFunction );

        // remove dupes.
        nummodes = m_modes->Count();
        int i = 1;  // not zero!
        while (i < nummodes)
        {
                GLMDisplayModeInfoFields& info0 = m_modes->Element( i - 1 )->m_info;
                GLMDisplayModeInfoFields& info1 = m_modes->Element( i )->m_info;

                if ( ( info0.m_modePixelWidth == info1.m_modePixelWidth ) &&
                     ( info0.m_modePixelHeight == info1.m_modePixelHeight ) &&
                     ( info0.m_modeRefreshHz == info1.m_modeRefreshHz ) )
                {
                        m_modes->Remove(i);
                        nummodes--;
                }
                else
                {
                        i++;
                }
        }
}


void    GLMDisplayInfo::Dump( int which )
{
        SDLAPP_FUNC;

        GLMPRINTF(("\n         #%d: GLMDisplayInfo @ %08x, pixwidth=%d  pixheight=%d",
                           which, (int)this,  m_info.m_displayPixelWidth,  m_info.m_displayPixelHeight ));

        FOR_EACH_VEC( *m_modes, i )
        {
                ( *m_modes )[i]->Dump(i);
        }
}
