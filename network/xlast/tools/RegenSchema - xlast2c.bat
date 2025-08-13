set RAGE_DIR=X:\gta5\src\dev\rage\framework\deprecated
set XLAST_DIR=x:\gta5\xlast
set XLAST_FILE=Fuzzy.xlast
set XLAST_WORK_DIR=X:\gta5\src\dev\game\network\xlast

cd %XLAST_WORK_DIR%
%RAGE_DIR%\tools\bin\xlast2c.exe -gsgamename agent360 -atlasversion 1 %XLAST_DIR%\%XLAST_FILE%
rem %RAGE_DIR%\tools\bin\xlast2c.exe -gsgamename agentps3 -atlasversion 1 %XLAST_DIR%\%XLAST_FILE%

pause

