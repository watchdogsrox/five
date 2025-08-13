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

pushd .
cd %1
gcc2vs "%SCE_PS3_ROOT%/host-win32/spu/bin/spu-lv2-gcc" %2.spu.o @"%GTA5_SPUPM_DIR%/GtaLinkSPUJobHandler" %LINK_FLAGS% -o %2.sym.elf
@"%GTA5_TOOLS_DIR%\GtaStripSPUELF" %2.sym.elf %2.elf
"%SCE_PS3_ROOT%/host-win32/ppu/bin/ppu-lv2-objcopy" -B powerpc -I binary -O elf64-powerpc-celloslv2 --set-section-align .data=7 --set-section-pad .data=16  --rename-section .data=.spu_image.gta --redefine-sym _binary_%2_elf_start=spupm%2 %2.elf %2.ppu.o

:: Do not delete *.elf file (required for on-the-fly reload)
@rem del %2.elf

:: Do not delete the elf with symbols in a release build
@rem @if /i not %BUILD_CONFIG%=="release" @del %2.sym.elf

:: copy *.sym.elf to spu_debug folder
copy %2.sym.elf %GTA5_SPU_DEBUG_DIR%

popd
