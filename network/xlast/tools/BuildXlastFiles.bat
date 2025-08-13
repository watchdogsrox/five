:: This file is used to create the necessary files from the Xlast file

@echo OFF

CLS

:: Set folders
IF (%1) == () (
	set XLAST_WORK_DIR=X:\gta5\src\dev_ng\game\network\xlast
) ELSE (
	set XLAST_WORK_DIR=%1
)

set ATLAS2C_DIR=X:\rage\dev\rage\framework\deprecated\tools\bin
set XLAST2C_DIR=X:\gta5\tools_ng\bin
set PERL_DIR=C:\strawberry\perl\bin
set SCRIPT_DIR=X:\gta5\src\dev_ng\game\script_headers
set NATIVE_SCRIPT_DIR=X:\gta5\script\dev_ng\core\common\native
set BUILD_DIR=X:\gta5\titleupdate\dev_ng\common
set PC_DATA_DIR=X:\gta5\titleupdate\dev_ng\x64
set TEXT_DIR=X:\gta5\assets_ng\GameText\dev_ng\American
set XLAST_DIR=X:\gta5\xlast

set XLAST_NAME=Fuzzy_xb1_heists
set XLAST_FILE=%XLAST_NAME%.gameconfig
set SPA_FILE=Fuzzy.spa
set FUZZY_SCHEMA_RESULT_FILE=%XLAST_NAME%.schema.h
set FUZZY_SCHEMA_FILE=Fuzzy.schema.h
set PS3_ATLAS_FILE=Fuzzy.gtavps3.atlas.xml
set XBOX_ATLAS_FILE=Fuzzy.gtav360.atlas.xml
set GAME_PC_METADATA=metadata.dat

:: Specifies the Atlas Version
set ATLAS_VERSION=1

:: Specifies the Gamespy game name. When specified, an Atlas-importable XML will be created.
set GSNAMEPS3=gtavps3
set GSNAME360=gtav360

:: Specifies the .atlas.xml file for the previously released version.  The new version must have all the keys/stats of the previous version
set PREVATLAS_PS3_FILE=Fuzzy.gtavps3.atlas.xml
set PREVATLAS_360_FILE=Fuzzy.gtav360.atlas.xml

:: Generate CSV file with layouts
set ACHIEVEMENTS_ENUM_FILE=achievements_enum.sch
set AVATAR_ENUM_FILE=avatarItems_enum.sch
set GAMEMODES_ENUM_FILE=gamemodes_enum.sch
set PRESENCE_ENUM_FILE=presence_enum.sch
set PRESENCE_UTIL_FILE=presenceutil.h


echo ""
echo ""
echo This file is used to create the necessary files from the Xlast file
echo ""
echo ""
echo Setup
echo .... ATLAS_VERSION: %ATLAS_VERSION%
echo ....     GSNAMEPS3: %GSNAMEPS3%
echo ....     GSNAME360: %GSNAME360%
echo ""
echo Directories
echo ....... XLAST_WORK: %XLAST_WORK_DIR%
echo .......      XLAST: %XLAST_DIR%
echo .......    ATLAS2C: %ATLAS2C_DIR%
echo .......    XLAST2C: %XLAST2C_DIR%
echo .......       PERL: %PERL_DIR%
echo .......     SCRIPT: %SCRIPT_DIR%
echo .......     SCRIPT: %NATIVE_SCRIPT_DIR%
echo .......       TEXT: %TEXT_DIR%
echo ""
echo Files
echo .......                 XLAST: %XLAST_FILE%
echo .......                   SPA: %SPA_FILE%
echo .......          FUZZY_SCHEMA: %FUZZY_SCHEMA_FILE%
echo .......             PS3_ATLAS: %PS3_ATLAS_FILE%
echo .......            XBOX_ATLAS: %XBOX_ATLAS_FILE%
echo .......         PREVATLAS_PS3: %PREVATLAS_PS3_FILE%
echo .......         PREVATLAS_360: %PREVATLAS_360_FILE%
echo .......     ACHIEVEMENTS_ENUM: %ACHIEVEMENTS_ENUM_FILE%
echo .......           AVATAR_ENUM: %AVATAR_ENUM_FILE%
echo .......   GAMEMODES_ENUM_FILE: %GAMEMODES_ENUM_FILE%
echo .......    PRESENCE_ENUM_FILE: %PRESENCE_ENUM_FILE%
echo .......    PRESENCE_UTIL_FILE: %PRESENCE_UTIL_FILE%


pause
cls

move /Y  X:\gta5\src\dev_ng\rage\base\tools\cli\xlast2c\bin\Release\xlast2c.exe X:\gta5\src\dev_ng\game\network\xlast\tools

:: Compile the xlast file.
:: "%XEDK%\bin\win32\spac" -o Fuzzy.spa -h Fuzzy.spa.h Fuzzy.gameconfig
:: PS3-specific trophy hack
:: REM t:\rdr2\tools\bin\TrophyVoodoo.exe 35 t:\rdr2\assets\content\scripting\AchievementsEnum\AchievementsEnum.sch ACHIEVEMENT_PS3_SPECIFIC


:: Set our working directory
cd %XLAST_WORK_DIR%


:: Checkout p4 files
echo ""
echo Checkout p4 files
echo ""
p4 edit %TEXT_DIR%\americanXLAST.txt
p4 edit %TEXT_DIR%\americanXLAST_DE.txt
p4 edit %TEXT_DIR%\americanXLAST_ES.txt
p4 edit %TEXT_DIR%\americanXLAST_FR.txt
p4 edit %TEXT_DIR%\americanXLAST_IT.txt
p4 edit %PC_DATA_DIR%\%GAME_PC_METADATA%
p4 edit X:\gta5\src\dev_ng\game\network\xlast\Fuzzy.schema.h
p4 edit X:\gta5\src\dev_ng\game\network\xlast\atlas_gtav360_v1.h
p4 edit X:\gta5\src\dev_ng\game\network\xlast\atlas_gtav360_v1.cpp
p4 edit X:\gta5\src\dev_ng\game\network\xlast\atlas_gtavps3_v1.h
p4 edit X:\gta5\src\dev_ng\game\network\xlast\atlas_gtavps3_v1.cpp
p4 edit X:\gta5\src\dev_ng\game\network\xlast\presenceutil.h

p4 edit X:\gta5\src\dev_ng\game\script_headers\avatarItems_enum.sch
p4 edit X:\gta5\src\dev_ng\game\script_headers\achievements_enum.sch
p4 edit X:\gta5\src\dev_ng\game\script_headers\presence_enum.sch
p4 edit X:\gta5\src\dev_ng\game\script_headers\gamemodes_enum.sch

p4 edit X:\gta5\script\dev_ng\core\common\native\avatarItems_enum.sch
p4 edit X:\gta5\script\dev_ng\core\common\native\achievements_enum.sch
p4 edit X:\gta5\script\dev_ng\core\common\native\presence_enum.sch
p4 edit X:\gta5\script\dev_ng\core\common\native\gamemodes_enum.sch

pause
cls

:: Generate american xlast file that will be translated
echo ""
echo Generate american xlast 
echo ""
%PERL_DIR%\perl.exe -W %XLAST_WORK_DIR%\tools\strings.pl %XLAST_DIR%\%XLAST_FILE% en-US > %TEXT_DIR%\americanXLAST.txt

if not "%1" == "all" goto end

%PERL_DIR%\perl.exe -W %XLAST_WORK_DIR%\tools\strings.pl %XLAST_DIR%\%XLAST_FILE% de-DE > %TEXT_DIR%\americanXLAST_DE.txt
%PERL_DIR%\perl.exe -W %XLAST_WORK_DIR%\tools\strings.pl %XLAST_DIR%\%XLAST_FILE% es-ES > %TEXT_DIR%\americanXLAST_ES.txt
%PERL_DIR%\perl.exe -W %XLAST_WORK_DIR%\tools\strings.pl %XLAST_DIR%\%XLAST_FILE% fr-FR > %TEXT_DIR%\americanXLAST_FR.txt
%PERL_DIR%\perl.exe -W %XLAST_WORK_DIR%\tools\strings.pl %XLAST_DIR%\%XLAST_FILE% it-IT > %TEXT_DIR%\americanXLAST_IT.txt

:end


pause
cls

:: Generate xlast2c files
cd %XLAST_DIR%

:: xlast2c for ps3
:: -prevatlasxml %XLAST_WORK_DIR%\%PREVATLAS_PS3_FILE%
:: echo xlast2c for ps3
:: %XLAST2C_DIR%\xlast2c.exe -v -gsgamename %GSNAMEPS3% -atlasversion %ATLAS_VERSION% %XLAST_DIR%\%XLAST_FILE%
:: move /Y %XLAST_DIR%\Fuzzy.atlas.xml %XLAST_WORK_DIR%\%PS3_ATLAS_FILE%
:: pause

:: xlast2c for 360
:: -prevatlasxml %XLAST_WORK_DIR%\%PREVATLAS_360_FILE%
:: echo xlast2c for 360
:: %XLAST2C_DIR%\xlast2c.exe -v -gsgamename %GSNAME360% -atlasversion %ATLAS_VERSION%  %XLAST_DIR%\%XLAST_FILE%
:: move /Y %XLAST_DIR%\Fuzzy.atlas.xml %XLAST_WORK_DIR%\%XBOX_ATLAS_FILE%

:: xlast2c for PC
:: echo xlast2c for PC
%XLAST2C_DIR%\xlast2c.exe -pc -v %XLAST_DIR%\%XLAST_FILE%

:: xlast2c for script enums
echo xlast2c for script enums
rem %XLAST2C_DIR%\xlast2c.exe -v -rsAvatarItems %XLAST_WORK_DIR%\%AVATAR_ENUM_FILE% %XLAST_DIR%\%XLAST_FILE%
%XLAST2C_DIR%\xlast2c.exe -v -achievements_enum %XLAST_WORK_DIR%\%ACHIEVEMENTS_ENUM_FILE% -gamemodes_enum %XLAST_WORK_DIR%\%GAMEMODES_ENUM_FILE% -presence_enum %XLAST_WORK_DIR%\%PRESENCE_ENUM_FILE% -presence_util %XLAST_WORK_DIR%\%PRESENCE_UTIL_FILE% -suppress_presence_toCstring -suppress_leaderboard_toCstring %XLAST_DIR%\%XLAST_FILE%
pause

:: :: Generate atlas2c files
:: cd %XLAST_WORK_DIR%
:: echo ""
:: echo Generate atlas2c files
:: echo ""
:: %ATLAS2C_DIR%\atlas2c.exe %XLAST_WORK_DIR%\%PS3_ATLAS_FILE%
:: %ATLAS2C_DIR%\atlas2c.exe %XLAST_WORK_DIR%\%XBOX_ATLAS_FILE% 
:: pause


cls

:: Move files to their proper locations
echo ""
echo Move files to their proper locations
echo ""

move /Y %XLAST_DIR%\%FUZZY_SCHEMA_RESULT_FILE% %XLAST_WORK_DIR%\%FUZZY_SCHEMA_FILE%
move /Y %XLAST_DIR%\%PLAYER_FUZZY_SCHEMA_FILE% %XLAST_WORK_DIR%\%PLAYER_FUZZY_SCHEMA_FILE%

copy /Y %XLAST_WORK_DIR%\%ACHIEVEMENTS_ENUM_FILE% %SCRIPT_DIR%\
copy /Y %XLAST_WORK_DIR%\%AVATAR_ENUM_FILE% %SCRIPT_DIR%\
copy /Y %XLAST_WORK_DIR%\%GAMEMODES_ENUM_FILE% %SCRIPT_DIR%\
copy /Y %XLAST_WORK_DIR%\%PRESENCE_ENUM_FILE% %SCRIPT_DIR%\

move /Y %XLAST_WORK_DIR%\%ACHIEVEMENTS_ENUM_FILE% %NATIVE_SCRIPT_DIR%\
move /Y %XLAST_WORK_DIR%\%AVATAR_ENUM_FILE% %NATIVE_SCRIPT_DIR%\
move /Y %XLAST_WORK_DIR%\%GAMEMODES_ENUM_FILE% %NATIVE_SCRIPT_DIR%\
move /Y %XLAST_WORK_DIR%\%PRESENCE_ENUM_FILE% %NATIVE_SCRIPT_DIR%\

copy /Y %XLAST_DIR%\%GAME_PC_METADATA% %PC_DATA_DIR%\%GAME_PC_METADATA%

rem mv -f -v -u %XLAST_DIR%\%SPA_FILE% %SCRIPT_DIR%\

pause
exit
