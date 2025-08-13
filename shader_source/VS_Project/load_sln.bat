set RAGE_DIR=X:\gta5\src\dev\game\VS_Project\..\..\rage

set SCE_PS3_ROOT=X:/ps3sdk/dev/usr/local/430_001/cell


if /i not "%RS_PROJECT%"=="gta5" goto envset
set orbissdk=%SCE_ROOT_DIR%\\ORBIS SDKs\\1.700
if not exist "%orbissdk%" set orbissdk=%PROGRAMFILES(X86)%\\SCE\\ORBIS SDKs\\1.700
if not exist "%orbissdk%" set orbissdk=C:\\Program Files (x86)\\SCE\\ORBIS SDKs\\1.700
if not exist "%orbissdk%" set orbissdk=C:\\Orbis\\1.700
if exist "%orbissdk%" set SCE_ORBIS_SDK_DIR=%orbissdk%

if not exist "%orbissdk%" (
echo.&&echo WARNING:
echo You do not appear to have the correct PS4 SDK installed.
echo The custom PS4 SDK 1.7 is required to build this project.
echo This consists of SDK 1.700.081 plus three patches.
echo You may have trouble building PS4 code without this,
echo but the solution will still load.
echo Please contact your IT dept for more details.
pause
)
set orbissdk=

:envset
ECHO LOAD_SLN ENVIRONMENT
ECHO RAGE_DIR:               %RAGE_DIR%
ECHO SCE_PS3_ROOT:           %SCE_PS3_ROOT%
echo SCE_ORBIS_SDK_DIR:      %SCE_ORBIS_SDK_DIR%
ECHO END LOAD_SLN ENVIRONMENT

 
rem start "" "C:\Program Files\Microsoft Visual Studio 8\Common7\IDE\devenv.exe"

start "" %cd%\shaders_2008.vcproj
