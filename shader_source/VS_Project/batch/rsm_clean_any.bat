@echo off
set SHADER_PLATFORMCONFIG_ARG=
if NOT [%2]==[] (set SHADER_PLATFORMCONFIG_ARG=-platformConfig %2)
@echo on
%RS_TOOLSROOT%\bin\RageShaderManager\RAGEShaderManager -config=%RS_TOOLSROOT%\etc\RageShaderManager\shaderbuild.xml -action=clean -buildfromdirectory -incredibuild -platform=%1 %SHADER_PLATFORMCONFIG_ARG%
