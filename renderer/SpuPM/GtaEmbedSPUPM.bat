
@set LINK_FLAGS=""
@set BUILD_CONFIG=""

@if /i "%4"=="" ( GOTO NO_BUILD_CONFIG ) else ( GOTO HAS_BUILD_CONFIG )

:NO_BUILD_CONFIG
@set BUILD_CONFIG=%3
@set LINK_FLAGS=%LINK_FLAGS:"=%
@goto BUILD

:HAS_BUILD_CONFIG
@set BUILD_CONFIG=%4
@set LINK_FLAGS=%3
@set LINK_FLAGS=%LINK_FLAGS:"=% 

:BUILD


set GCC2VS="%SN_COMMON_PATH%\vsi\bin\gcc2vs.exe"
IF NOT EXIST %GCC2VS% (
	echo gcc2vs not found, error messages won't be pretty.
	set GCC2VS=
)


pushd .
cd %1
%GCC2VS% "%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-gcc" SpuPmMgr_pm_spursentry.spu.o %2.spu.o @"%GTA5_SPUPM_DIR%/GtaLinkSPUPM" %LINK_FLAGS% -o %2.sym.elf -ldma -lspurs
"%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-strip" %2.sym.elf -o %2.elf
"%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-objcopy" -O binary %2.elf %2.bin
@del %2.elf

:: Do not delete the elf with symbols in a release build
@if /i not "%BUILD_CONFIG%"=="release" @del %2.sym.elf
"%SCE_PS3_ROOT%/host-win32/ppu/bin/ppu-lv2-objcopy" -B powerpc -I binary -O elf64-powerpc-celloslv2 --set-section-align .data=7 --set-section-pad .data=16  --rename-section .data=.spu_image.gta --redefine-sym _binary_%2_bin_start=_%2 --redefine-sym _binary_%2_bin_size=_%2_size %2.bin %2.ppu.o
popd

