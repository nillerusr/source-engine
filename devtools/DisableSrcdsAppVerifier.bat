@echo Created per the instructions at https://intranet.valvesoftware.com/wiki/index.php/AppVerifier_for_finding_memory_bugs_and_other_problems
@echo Disabling AppVerifier for srcds
@echo This must be run with administrator privileges

reg delete "HKLM\Software\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\srcds.exe" /f
@rem Delete from the Wow64 node as well, just to be sure.
reg delete "HKLM\Software\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\srcds.exe" /f
