@echo off
call ..\shadercompilerreset.bat
call rsm_clean_durango.bat dev
call rsm_clean_durango.bat final
call rsm_clean_max.bat dev
call rsm_clean_max_anon.bat dev
call rsm_clean_orbis.bat dev
call rsm_clean_orbis.bat final
call rsm_clean_win32.bat dev
call rsm_clean_win32.bat final
call rsm_clean_win32_40.bat dev
call rsm_clean_win32_40.bat final
call rsm_clean_win32_50.bat dev
call rsm_clean_win32_50.bat final
call rsm_clean_win32_nvstereo.bat dev
call rsm_clean_win32_nvstereo.bat final
pause