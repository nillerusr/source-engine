

from vsdotnetxmlparser import *


#print f.GetAttribute( 'VisualStudioProject\\Configurations\\Configuration\\<2>Tool\\CommandLine' )


WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug DoD|Win32", "Release DoD|Win32"], 'cl_dll\\client_dod.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug CounterStrike|Win32", "Release CounterStrike|Win32"], 'cl_dll\\client_cs.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug HL1|Win32", "Release HL1|Win32"], 'cl_dll\\client_hl1.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug HL2|Win32", "Release HL2|Win32"], 'cl_dll\\client_hl2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug TF2|Win32", "Release TF2|Win32"], 'cl_dll\\client_tf2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug SDK|Win32", "Release SDK|Win32"], 'cl_dll\\client_temp_sdk.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug HL2MP|Win32", "Release HL2MP|Win32"], 'cl_dll\\client_hl2mp.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'cl_dll\\client.vcproj' ), ["Debug Episodic HL2|Win32", "Release Episodic HL2|Win32"], 'cl_dll\\client_episodic.vcproj' )


WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug DoD|Win32", "Release DoD|Win32"], 'dlls\\hl_dod.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug CounterStrike|Win32", "Release CounterStrike|Win32"], 'dlls\\hl_cs.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug HL1|Win32", "Release HL1|Win32"], 'dlls\\hl_hl1.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug HL2|Win32", "Release HL2|Win32"], 'dlls\\hl_hl2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug TF2|Win32", "Release TF2|Win32"], 'dlls\\hl_tf2.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug SDK|Win32", "Release SDK|Win32"], 'dlls\\hl_temp_sdk.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug HL2MP|Win32", "Release HL2MP|Win32"], 'dlls\\hl_hl2mp.vcproj' )
WriteSeparateVCProj( LoadVCProj( 'dlls\\hl.vcproj' ), ["Debug Episodic HL2|Win32", "Release Episodic HL2|Win32"], 'dlls\\hl_episodic.vcproj' )

