@rem setup %GTA5_DIR%(=X:\GTA5\src\dev\game) from $(InputDir):
pushd .
 @rem cd to $(inputDir)
 @cd /D %2
 @cd ../..
 @set GTA5_DIR=%cd%
 @rem echo GTA5_DIR=%GTA5_DIR%
popd

@if /i "%RS_TOOLSROOT%"=="" (
 @echo Fatal error - "RS_TOOLSROOT" env variable not set!
 @echo Please run "X:\Gta5\tools\Install.bat" to fix this.
)

@set GTA5_SPUPM_DIR=%GTA5_DIR%\renderer\SPUPM
@set GTA5_TOOLS_DIR=%RS_TOOLSROOT%\bin
@set GTA5_SPU_DEBUG_DIR=C:\spu_debug


pushd .

@if /i "%1"=="debug" GOTO DEBUG_BUILD
@if /i "%1"=="debugopt" GOTO DEBUGOPT_BUILD
@if /i "%1"=="profile" GOTO PROFILE_BUILD
@if /i "%1"=="release" GOTO RELEASE_BUILD

:DEBUG_BUILD
@set PREPROCESSOR_DEFS=-D_DEBUG
@set COMPILE_RESP=GtaCompileSPUJobHandlerDebug
@set LINK_FLAGS="-latomic"
@goto START

:DEBUGOPT_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG -D_DEBUGOPT
@set COMPILE_RESP=GtaCompileSPUJobHandlerDebugOpt
@set LINK_FLAGS="-latomic"
@goto START

:PROFILE_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG -DSPUPM_ENABLE_TUNER_TRACE=1
@set COMPILE_RESP=GtaCompileSPUJobHandlerProfile
@set LINK_FLAGS="-lspurs -latomic -Wl,-Map=%3.sym.elf.map"
@goto START

:RELEASE_BUILD
@set PREPROCESSOR_DEFS=-DNDEBUG
@set COMPILE_RESP=GtaCompileSPUJobHandlerRelease
@set LINK_FLAGS="-latomic -Wl,-Map=%3.sym.elf.map"

:START


@set INTERMEDIATE_DIR=spu-obj

@set DIR=psn_beta
pushd .
@cd /D %RAGE_DIR%
@set ABS_RAGE_DIR=%CD%
popd



:: Create the directory if it doesn't exist
@if not exist "%INTERMEDIATE_DIR%" mkdir "%INTERMEDIATE_DIR%"

:: Compile the SPU program
gcc2vs "%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-gcc" -include %ABS_RAGE_DIR:\=/%/base/src/forceinclude/spu_%DIR%.h -DUSING_RAGE -DSN_TARGET_PS3_SPU -D_REENTRANT -D_SPU %PREPROCESSOR_DEFS% -D__GCC__ -DSPU -I"%SN_PS3_PATH%/spu/include/sn" -I"%ABS_RAGE_DIR:\=/%/base/src" -I"%ABS_RAGE_DIR:\=/%/framework/src" -I.. -c -x c++ -fpch-deps -MMD -MV @"%GTA5_SPUPM_DIR%/%COMPILE_RESP%" "%2%3.pmjob" -o "%INTERMEDIATE_DIR%/%3.spu.o"

:: Embed the elf
call "%GTA5_SPUPM_DIR%/GtaEmbedSPUJobHandler.bat" "%INTERMEDIATE_DIR%" %3 %LINK_FLAGS% %1

popd


