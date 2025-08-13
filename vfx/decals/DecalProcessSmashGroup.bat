call ..\..\spu_default_compile_flags.bat

if "%DIR%"=="psn_beta" (
	set COMPILE_SET_0_OPT_LVL=-Os
)

set WARN=%WARN% -Wno-non-virtual-dtor
