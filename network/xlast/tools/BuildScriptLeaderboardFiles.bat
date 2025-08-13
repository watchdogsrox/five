
:: Set folders
IF (%1) == () (
	set XLAST_WORK_DIR=X:\gta5\src\dev_ng\game\network\xlast
) ELSE (
	set XLAST_WORK_DIR=%1
)

set XLAST_DIR=X:\gta5\xlast
set CODE_SCRIPT_DIR=X:\gta5\src\dev_ng\game\script_headers
set NATIVE_SCRIPT_DIR=X:\gta5\script\dev_ng\core\common\native
set LB_ENUM_FILE=leaderboards_enum.sch
set GAME_CONFIG2C_TOOL_PATH=X:\gta5\tools\bin\scgameconfig2c\scgameconfig2c_2010.exe
set GAME_PLAYER_XLAST_FILE=playerxlast.xml
set BUILD_DIR=X:\gta5\titleupdate\dev_ng\common
set GAME_CONFIG_FILE=NewLeaderboards.tu.ros.xml

:: Checkout p4 files
echo ""
echo Checkout p4 files
echo ""
p4 edit %CODE_SCRIPT_DIR%\%LB_ENUM_FILE%
p4 edit %NATIVE_SCRIPT_DIR%\%LB_ENUM_FILE%
p4 edit %XLAST_DIR%\%GAME_CONFIG_FILE%
p4 edit %BUILD_DIR%\data\%GAME_PLAYER_XLAST_FILE%

:: Set our working directory
cd %XLAST_WORK_DIR%

%GAME_CONFIG2C_TOOL_PATH% -nogui -file=%XLAST_DIR%\%GAME_CONFIG_FILE% -leaderboards_enum=%XLAST_WORK_DIR%\%LB_ENUM_FILE%

copy /Y %XLAST_WORK_DIR%\%LB_ENUM_FILE% %CODE_SCRIPT_DIR%\%LB_ENUM_FILE%
copy /Y %XLAST_WORK_DIR%\%LB_ENUM_FILE% %NATIVE_SCRIPT_DIR%\%LB_ENUM_FILE%
copy /Y %XLAST_DIR%\%GAME_CONFIG_FILE% %BUILD_DIR%\data\%GAME_PLAYER_XLAST_FILE%

pause