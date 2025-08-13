REM GTA specific SPU compile flags

REM Temporarily set the current path to the location of the batch file with a lil' trick
pushd %~dp0
set GAME_DIR=%cd%
popd

REM Add game specific force includes
set FORCE_INC=%FORCE_INC% -include %GAME_DIR%\basetypes.h -include %GAME_DIR%\game_config.h

REM Add rage include paths
set INC_DIR=%INC_DIR% -I %ABS_RAGE_DIR%\script\src -I %ABS_RAGE_DIR%\naturalmotion\include -I %ABS_RAGE_DIR%\naturalmotion\src -I %ABS_RAGE_DIR%\suite\src -I %ABS_RAGE_DIR%\framework\src 

REM Add game include paths
set INC_DIR=%INC_DIR% -I %GAME_DIR%

set WARN=%WARN% -Wno-reorder
