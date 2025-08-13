set XLAST=X:\gta5\xlast\Fuzzy.gameconfig
set ASSETS_DIR=X:\gta5\assets\GameText\TitleUpdate
set DEST_DIR_EN=%ASSETS_DIR%\American
set DEST_DIR_CN=%ASSETS_DIR%\chinese
set DEST_DIR_FR=%ASSETS_DIR%\french
set DEST_DIR_DE=%ASSETS_DIR%\german
set DEST_DIR_IT=%ASSETS_DIR%\italian
set DEST_DIR_JP=%ASSETS_DIR%\japanese
set DEST_DIR_KR=%ASSETS_DIR%\korean
set DEST_DIR_ME=%ASSETS_DIR%\mexican
set DEST_DIR_PL=%ASSETS_DIR%\polish
set DEST_DIR_PT=%ASSETS_DIR%\portuguese
set DEST_DIR_RU=%ASSETS_DIR%\russian
set DEST_DIR_ES=%ASSETS_DIR%\spanish

:: Checkout p4 files
echo ""
echo Checkout p4 files
echo ""
p4 edit %DEST_DIR_EN%\americanXLAST.txt
p4 edit %DEST_DIR_CN%\chineseXLAST.txt
p4 edit %DEST_DIR_FR%\frenchXLAST.txt
p4 edit %DEST_DIR_DE%\germanXLAST.txt
p4 edit %DEST_DIR_IT%\italianXLAST.txt
p4 edit %DEST_DIR_JP%\japaneseXLAST.txt
p4 edit %DEST_DIR_KR%\koreanXLAST.txt
p4 edit %DEST_DIR_ME%\mexicanXLAST.txt
p4 edit %DEST_DIR_PL%\polishXLAST.txt
p4 edit %DEST_DIR_PT%\portugueseXLAST.txt
p4 edit %DEST_DIR_RU%\russianXLAST.txt
p4 edit %DEST_DIR_ES%\spanishXLAST.txt


:: Export strings
perl -W strings.pl %XLAST% en-US > %DEST_DIR_EN%\americanXLAST.txt

:: if not "%1" == "all" goto end

perl -W strings.pl %XLAST% zh-CHT > %DEST_DIR_CN%\chineseXLAST.txt
perl -W strings.pl %XLAST% fr-FR > %DEST_DIR_FR%\frenchXLAST.txt
perl -W strings.pl %XLAST% de-DE > %DEST_DIR_DE%\germanXLAST.txt
perl -W strings.pl %XLAST% it-IT > %DEST_DIR_IT%\italianXLAST.txt
perl -W strings.pl %XLAST% ja-JP > %DEST_DIR_JP%\japaneseXLAST.txt
perl -W strings.pl %XLAST% ko-KR > %DEST_DIR_KR%\koreanXLAST.txt
perl -W strings.pl %XLAST% es-ES > %DEST_DIR_ME%\mexicanXLAST.txt
perl -W strings.pl %XLAST% pl-PL > %DEST_DIR_PL%\polishXLAST.txt
perl -W strings.pl %XLAST% pt-PT > %DEST_DIR_PT%\portugueseXLAST.txt
perl -W strings.pl %XLAST% ru-RU > %DEST_DIR_RU%\russianXLAST.txt
perl -W strings.pl %XLAST% es-ES > %DEST_DIR_ES%\spanishXLAST.txt



:end

pause
