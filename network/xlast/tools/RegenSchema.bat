set RAGE_DIR=X:\gta5\src\dev\rage\framework\deprecated
set XLAST_DIR=X:\gta5\xlast
set XLAST_FILE=Fuzzy.xlast
set XLAST_WORK_DIR=X:\gta5\src\dev\game\network\xlast

rem set XLAST_DIR=X:\gta4_japanese\src\dev_xbox360\gta\xlast
rem set XLAST_FILE=Fuzzy.xlast

cd %XLAST_WORK_DIR%
%RAGE_DIR%\tools\bin\xlast2c.exe %XLAST_DIR%\%XLAST_FILE%

rem %RAGE_DIR%\tools\base\exes\xlast2c.exe -gsgamename gta4ps3 -atlasversion 9 %XLAST_DIR%\%XLAST_FILE%

rem perl -W strings.pl %XLAST_DIR%\%XLAST_FILE% > ..\..\..\..\..\gta\build\common\text\americanXLAST.txt

pause