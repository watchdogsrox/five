@echo off
call ..\shadercompilerreset.bat
call rsm_build_durango.bat dev
call rsm_build_durango.bat final
call rsm_build_max.bat dev
call rsm_build_max_anon.bat dev
call rsm_build_orbis.bat dev
call rsm_build_orbis.bat final
call rsm_build_win32.bat dev
call rsm_build_win32.bat final
call rsm_build_win32_40.bat dev
call rsm_build_win32_40.bat final
call rsm_build_win32_50.bat dev
call rsm_build_win32_50.bat final
call rsm_build_win32_nvstereo.bat dev
call rsm_build_win32_nvstereo.bat final
pause