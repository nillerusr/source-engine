
from vsdotnetxmlparser import *


WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug SDK|Win32", "Release SDK|Win32"], 'cl_dll\\client_sdk.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug HL2|Win32", "Release HL2|Win32"], 'cl_dll\\client_sdk_hl2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug HL2MP|Win32", "Release HL2MP|Win32"], 'cl_dll\\client_sdk_hl2mp.vcproj' )

WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug SDK|Win32", "Release SDK|Win32"], 'dlls\\hl_sdk.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug HL2|Win32", "Release HL2|Win32"], 'dlls\\hl_sdk_hl2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug HL2MP|Win32", "Release HL2MP|Win32"], 'dlls\\hl_sdk_hl2mp.vcproj' )
