call ..\..\spu_default_compile_flags.bat

if "%DIR%"=="psn_beta" (
	set COMPILE_SET_0_OPT_LVL=-Os
)
if "%DIR%"=="psn_bankrelease" (
	set COMPILE_SET_0_OPT_LVL=-Os
)

set WARN=%WARN% -Wno-non-virtual-dtor
