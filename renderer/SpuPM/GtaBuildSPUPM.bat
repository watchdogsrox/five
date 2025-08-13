@rem setup %GTA5_DIR%(=X:\GTA5\src\dev\game) from $(InputDir):
pushd .
 @rem cd to $(inputDir)
 @cd /D %2
 @cd ../..
 @set GTA5_DIR=%cd%
 @rem echo GTA5_DIR=%GTA5_DIR%
popd

@set GTA5_SPUPM_DIR=%GTA5_DIR%\renderer\SPUPM

pushd .

@if /i "%1"=="debug" GOTO DEBUG_BUILD
@if /i "%1"=="debugopt" GOTO DEBUGOPT_BUILD
@if /i "%1"=="profile" GOTO PROFILE_BUILD
@if /i "%1"=="release" GOTO RELEASE_BUILD

:DEBUG_BUILD
@set PREPROCESSOR_DEFS=-D_DEBUG
@set COMPILE_RESP=CompileSPUJobHandlerDebug
@goto START

:DEBUGOPT_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG -D_DEBUGOPT
@set COMPILE_RESP=CompileSPUJobHandlerDebugOpt
@goto START

:PROFILE_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG -DSPUPM_ENABLE_TUNER_TRACE=1
@set COMPILE_RESP=CompileSPUJobHandlerProfile
@set LINK_FLAGS="-Wl,-Map=%3.sym.elf.map"
@goto START

:RELEASE_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG
@set COMPILE_RESP=CompileSPUJobHandlerRelease
@set LINK_FLAGS="-Wl,-Map=%3.sym.elf.map"

:START

@set INTERMEDIATE_DIR=spu-obj

REM IF %4=="GTA4 PS3 Beta" set DIR=psn_beta
@set DIR=psn_beta


:: Create the directory if it doesn't exist
@if not exist "%INTERMEDIATE_DIR%" mkdir "%INTERMEDIATE_DIR%"

@set GCC2VS="%SN_COMMON_PATH%\vsi\bin\gcc2vs.exe"
IF NOT EXIST %GCC2VS% (
	echo gcc2vs not found, error messages won't be pretty.
	set GCC2VS=
)

pushd .
rem if exist %3.bat CALL %3.bat
rem echo cd is %CD%, RAGE_DIR is %RAGE_DIR%
@cd /D %RAGE_DIR%
@set ABS_RAGE_DIR=%CD%
popd


:: Compile the SPU program
%GCC2VS% "%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-gcc" -include %ABS_RAGE_DIR:\=/%/base/src/forceinclude/spu_%DIR%.h -DUSING_RAGE -DSN_TARGET_PS3_SPU -D_REENTRANT -D_SPU %PREPROCESSOR_DEFS% -D__GCC__ -DSPU -I"%ABS_RAGE_DIR:\=/%/base/src" -I"%SN_PS3_PATH%/spu/include/sn" -I.. -c -fpch-deps -MMD -MV @"%GTA5_SPUPM_DIR%\GtaCompileSPUJobHandlerDebug" "%2SpuPmMgr_pm_spursentry.cpp" -o "%INTERMEDIATE_DIR%/SpuPmMgr_pm_spursentry.spu.o"
%GCC2VS% "%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-gcc" -include %ABS_RAGE_DIR:\=/%/base/src/forceinclude/spu_%DIR%.h -DUSING_RAGE -DSN_TARGET_PS3_SPU -D_REENTRANT -D_SPU %PREPROCESSOR_DEFS% -D__GCC__ -DSPU -I"%ABS_RAGE_DIR:\=/%/base/src" -I"%SN_PS3_PATH%/spu/include/sn" -I.. -c -fpch-deps -MMD -MV @"%GTA5_SPUPM_DIR%\GtaCompileSPUJobHandlerDebug" "%2%3.cpp"                     -o "%INTERMEDIATE_DIR%/%3.spu.o"


:: Embed the elf
call "%GTA5_SPUPM_DIR%\GtaEmbedSPUPM.bat" "%INTERMEDIATE_DIR%" %3 %LINK_FLAGS% %1

:END


popd

