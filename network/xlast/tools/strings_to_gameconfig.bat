set XLAST=X:\gta5\xlast\Fuzzy.gameconfig
set ASSETS_DIR=X:\gta5\assets\GameText\TitleUpdate
set SRC_DIR_EN=%ASSETS_DIR%\American
set SRC_DIR_CN=%ASSETS_DIR%\chinese
set SRC_DIR_FR=%ASSETS_DIR%\french
set SRC_DIR_DE=%ASSETS_DIR%\german
set SRC_DIR_IT=%ASSETS_DIR%\italian
set SRC_DIR_JP=%ASSETS_DIR%\japanese
set SRC_DIR_KR=%ASSETS_DIR%\korean
set SRC_DIR_ME=%ASSETS_DIR%\mexican
set SRC_DIR_PL=%ASSETS_DIR%\polish
set SRC_DIR_PT=%ASSETS_DIR%\portuguese
set SRC_DIR_RU=%ASSETS_DIR%\russian
set SRC_DIR_ES=%ASSETS_DIR%\spanish

:: Checkout p4 files
echo ""
echo Checkout p4 files
echo ""
p4 edit %XLAST%

perl -W localize.pl %SRC_DIR_EN%\americanXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%

perl -W localize_jp.pl %SRC_DIR_CN%\chineseXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%

perl -W localize_jp.pl %SRC_DIR_FR%\frenchXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%

perl -W localize_jp.pl %SRC_DIR_DE%\germanXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%

perl -W localize_jp.pl %SRC_DIR_IT%\italianXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_JP%\japaneseXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_KR%\koreanXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_ME%\mexicanXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_PL%\polishXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_PT%\portugueseXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_RU%\russianXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%
perl -W localize_jp.pl %SRC_DIR_ES%\spanishXLAST.txt %XLAST% tmp.xlast
copy /Y tmp.xlast %XLAST%

:end

pause
