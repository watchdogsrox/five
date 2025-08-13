:: This file is used to create the necessary files from the Xlast file

@echo OFF

CLS

:: Set folders
IF (%1) == () (
	set XLAST_WORK_DIR=X:\gta5\src\dev\game\network\xlast
) ELSE (
	set XLAST_WORK_DIR=%1
)

:: Xlast paths 
set XLAST_FILE=Fuzzy.gameconfig
set XLAST2C_DIR=X:\gta5\xlast\ps4
set XLAST_DIR=X:\gta5\xlast
set OUTPUT_ICONS=Output

:: TROPHY file paths and PS4 communication ID
set PS4_COM_ID=NPWR04523_00
set XLAST_TROPHY_DIR=%XLAST_DIR%\ps4
set TROPHY_FILE_CONFIG=Config.sfn
set TROPHY_FILE_DEFAULT=Default Language.sfm
set TROPHY_FILE_ENGLISH=English.sfm
set TROPHY_FILE_FRENCH=French.sfm
set TROPHY_FILE_GERMAN=German.sfm
set TROPHY_FILE_ITALIAN=Italian.sfm
set TROPHY_FILE_JAPANESE=Japanese.sfm
set TROPHY_FILE_SPANISH=Spanish.sfm
set TROPHY_FILE_RUSSIAN=Russian.sfm
set TROPHY_FILE_POLISH=Polish.sfm
set TROPHY_FILE_PORTUGUESE=Portuguese.sfm

:: Run the xlast2c tool with PS4 trophies
cd %XLAST_DIR%
echo xlast2c for PS4
%XLAST2C_DIR%\xlast2c.exe -v -ps4 -trophies %PS4_COM_ID% %XLAST_DIR%\%XLAST_FILE%

:: Make sure the directories we are moving to exist
mkdir %XLAST_TROPHY_DIR%
mkdir %XLAST_TROPHY_DIR%\%OUTPUT_ICONS%

:: Move config and language files
move /Y %XLAST_DIR%\%TROPHY_FILE_CONFIG% %XLAST_TROPHY_DIR%
move /Y "%XLAST_DIR%\%TROPHY_FILE_DEFAULT%" %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_ENGLISH% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_FRENCH% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_GERMAN% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_ITALIAN% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_JAPANESE% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_SPANISH% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_RUSSIAN% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_POLISH% %XLAST_TROPHY_DIR%
move /Y %XLAST_DIR%\%TROPHY_FILE_PORTUGUESE% %XLAST_TROPHY_DIR%

:: Move TROP000.PNG through TROP050.PNG to output
move /Y %XLAST_DIR%\TROP* %XLAST_TROPHY_DIR%\%OUTPUT_ICONS%
