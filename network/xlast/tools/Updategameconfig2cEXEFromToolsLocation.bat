set GAMECONFIG2C_DIR=X:\gta5\tools\bin\scgameconfig2c
set DEBUG_PATH=X:\gta5\src\dev\rage\base\tools\cli\scgameconfig2c\bin\Debug

p4 edit %GAMECONFIG2C_DIR%\scgameconfig2c_2010.exe
p4 edit %GAMECONFIG2C_DIR%\LeaderboardGameConfigModel.dll

copy %DEBUG_PATH%\scgameconfig2c_2010.exe %GAMECONFIG2C_DIR%
copy %DEBUG_PATH%\LeaderboardGameConfigModel.dll %GAMECONFIG2C_DIR%

pause