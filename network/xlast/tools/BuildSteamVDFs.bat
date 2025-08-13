:: This file is used to create the necessary files from the Xlast file

@echo OFF

CLS

:: Set folders
IF (%1) == () (
	set XLAST_WORK_DIR=X:\gta5\src\dev_ng\game\network\xlast
) ELSE (
	set XLAST_WORK_DIR=%1
)

:: Xlast paths 
set XLAST_NAME=Fuzzy_xb1_heists
set XLAST_FILE=%XLAST_NAME%.gameconfig
set SPA_FILE=Fuzzy.spa
set FUZZY_SCHEMA_RESULT_FILE=%XLAST_NAME%.schema.h
set XLAST2C_DIR=X:\gta5\tools_ng\bin
set XLAST_DIR=X:\gta5\xlast
set STEAM_DIR=X:\gta5\xlast\Steam\271590_loc_all.vdf
set OUTPUT_ICONS=Output

:: Run the xlast2c tool with Steam
cd %XLAST_DIR%
echo xlast2c for Steam
echo Xlast Dir: %XLAST_DIR%
echo Xlast File: %XLAST_DIR%\%XLAST_FILE%
echo Steam Dir: %STEAM_DIR%
%XLAST2C_DIR%\xlast2c.exe -v %XLAST_DIR%\%XLAST_FILE% -steam %STEAM_DIR%

