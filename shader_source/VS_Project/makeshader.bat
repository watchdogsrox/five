@echo off
for %%P in (%*) do (
	if /i "%%P"=="/win32_30" (
		set PLATFORM=win32_30
	) else 	if /i "%%P"=="/win32_40" (
		set PLATFORM=win32_40
	) else 	if /i "%%P"=="/win32_50" (
		set PLATFORM=win32_50
	) else if /i "%%P"=="/fxl_final" (
		set PLATFORM=fxl_final 
	) else if /i "%%P"=="/psn" (
		set PLATFORM=psn
	) else if /i "%%P"=="/fx_max" (
		set PLATFORM=fx_max
	) else if /i "%%P"=="/Randomize" (
		set COMPILER=-useRandomizer
	) else if /i "%%P"=="/noWaveLines" (
		set WAVEOUTPUT=-noWaveLines
	) 
)

if defined XoreaxBuildContext set NOPERFDUMP=-noPerformanceDump

set SHADERPATH=%RS_BUILDBRANCH%\common\shaders\
set WARNING_AS_ERROR=0
%RS_TOOLSROOT%\script\coding\shaders\makeshader.bat -platform %PLATFORM% %COMPILER% %NOPERFDUMP% %WAVEOUTPUT% %1
