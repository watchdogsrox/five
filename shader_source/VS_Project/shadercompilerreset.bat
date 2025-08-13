
tasklist | find /I "wave.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM wave.exe /T 
)
tasklist | find /I "fx2cg_win32_debug.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM fx2cg_win32_debug.exe /T 
)
tasklist | find /I "fxc.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM fxc.exe /T 
)
tasklist | find /I "xsd.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM xsd.exe /T 
)
tasklist | find /I "sce-cgc.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM sce-cgc.exe /T 
)
tasklist | find /I "sce-cgcdisasm.exe" > NUL
IF %ERRORLEVEL% EQU 0 (
	taskkill /F /IM sce-cgcdisasm.exe /T 
)
