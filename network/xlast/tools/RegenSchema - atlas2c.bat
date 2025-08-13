set RAGE_DIR=X:\gta5\src\dev\rage\framework\deprecated
set PS3_ATLAS_FILE=Fuzzy.agentps3.atlas.xml
set XBOX_ATLAS_FILE=Fuzzy.agent360.atlas.xml
set XLAST_WORK_DIR=X:\gta5\src\dev\game\network\xlast

cd %XLAST_WORK_DIR%

%RAGE_DIR%\tools\bin\atlas2c.exe %PS3_ATLAS_FILE%
pause

%RAGE_DIR%\tools\bin\atlas2c.exe %XBOX_ATLAS_FILE% 
pause