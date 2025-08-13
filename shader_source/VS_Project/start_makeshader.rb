platform = ""
compiler = ""
waveOutput = ""
noPerfDump = ""
platformConfig = ""

platformMap = {
    "/win32_30" => "win32_30",
    "/win32_40" => "win32_40",
    "/win32_50" => "win32_50",
    "/fxl_final" => "fxl_final",
    "/psn" => "psn",
    "/fx_max" => "fx_max"
    }
    
ARGV.each do |i|
    platform = platformMap.fetch(i, platform)
    compiler = "-useATGCompiler" if i == "/ATGCOMPILER"
    waveOutput = "-noWaveLines" if i == "/noWaveLines"
	if i == "/dev"
		platformConfig = "dev"
	elsif i == "/final"
		platformConfig = "final"
	end
end

noPerfDump = "-noPerformanceDump" if ENV["XoreaxBuildContext"]

shaderPath = ENV['RS_BUILDBRANCH'] + "\\common\\shaders"
warningAsError = 0

command = "#{ENV['RS_TOOLSROOT']}\\script\\coding\\shaders\\makeshader.bat -platform #{platform} -platformConfig #{platformConfig} #{compiler} #{noPerfDump} #{waveOutput} #{ARGV[0]}"
puts command
system(command)
