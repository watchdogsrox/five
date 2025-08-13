@echo off
pushd %~dp0
set started=%time% 
@echo STARTED : %started%

call setenv

if "%1"=="VS2010" goto VS2010

@echo 2008 Project generation started ( Old Project Generator )
@echo https://devstar.rockstargames.com/wiki/index.php/Dev:Project_builder

pushd vs_project1_lib
call rebuild_vcproj.bat VS2008
popd

pushd vs_project2_lib
call rebuild_vcproj.bat VS2008
popd

pushd vs_project3_lib
call rebuild_vcproj.bat VS2008
popd

pushd vs_project4_lib
call rebuild_vcproj.bat VS2008
popd

pushd vs_project\RageMisc\
call rebuild_vcproj.bat VS2008
popd

echo Building Vs_Project...
pushd VS_Project
call rebuild_vcproj.bat VS2008
popd

echo Building VS_Project_Network...
pushd VS_Project_network
call rebuild_vcproj.bat VS2008
popd

REM echo Building Sln...
REM pushd VS_Project
REM call rebuild_sln.bat
REM popd

@echo 2008 Project generation complete %time%

if "%1"=="VS2008" goto END

:VS2010

@echo.
@echo.
@echo.
@echo ****************************************************************************
@echo ****************************************************************************
@echo *** VS2010 Project generation started ( New Project Generator ) %time% 
@echo *** https://devstar.rockstargames.com/wiki/index.php/Dev:Project_builder3 
@echo ****************************************************************************
@echo ****************************************************************************
@echo.

call %RS_TOOLSROOT%\script\util\projGen\sync.bat


set build_script=%RS_TOOLSROOT%\script\util\projgen\rebuildGameLibrary.bat

call %build_script% 	%RS_CODEBRANCH%\game\vs_project1_lib\game1_lib.txt ^
			%RS_CODEBRANCH%\game\vs_project2_lib\game2_lib.txt^
			%RS_CODEBRANCH%\game\vs_project3_lib\game3_lib.txt ^
			%RS_CODEBRANCH%\game\vs_project4_lib\game4_lib.txt ^
			%RS_CODEBRANCH%\game\vs_project\RageMisc\ragemisc.txt ^
			%RS_CODEBRANCH%\game\vs_project_network\network.txt ^
			%RS_CODEBRANCH%\game\vs_project\make.txt ^
			%RS_CODEBRANCH%\game\vs_project\game.slndef
@echo.
@echo ****************************************************************************
@echo ****************************************************************************
@echo *** VS2010 Project generation complete %time%
@echo ****************************************************************************
@echo ****************************************************************************
@echo.
@echo.
@echo.

:END

@echo STARTED  : %started% 
@echo FINISHED : %time% 
