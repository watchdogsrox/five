######################################
#
######################################

import os
import os.path
import re
import sys

######################################
# GLOBAL
#

g_output_dir = "X:\\"

######################################
# USAGE
#
def get_filename(path):
	# Directory
	filename = path
	index = filename.rfind('\\')
	if index > -1:
		filename = filename[index+1:len(filename)]
	else:
		index = filename.rfind('/')
		if index > -1:
			filename = filename[index+1:len(filename)]
	
	# Extension
	#index = filename.rfind('.')
	#if index > -1:
	#	filename = filename[0:index]
		
	return filename

def printUsage():
	print 'USAGE: python ps3shaderlogextract.py <command> <parameters>'
	print 'Commands : csv <build log> <csv file> - Export LOG to CSV'
	print 'Commands : compare <build log A> <build log B> <csv file> - Merge two build log files into a single CSV, for camparison'
	print 'Commands : rdm <build log> <csv file> - Export LOG to CSV for randomiser build results'
	exit(-1)

def usage():
	if len(sys.argv) < 3:
		printUsage()
		
	global g_command
	global g_log_path
	global g_log_A_path
	global g_log_B_path
	global g_csv_path

	g_command = sys.argv[1].lower()
	if g_command == "csv" :
		g_log_path = sys.argv[2]
		g_csv_path = sys.argv[3]
	
		print "exporting", g_log_path,"to",g_csv_path
	elif g_command == "rdm" :
		g_log_path = sys.argv[2]
		g_csv_path = sys.argv[3]
		
		print "exporting randomizer results", g_log_path,"to",g_csv_path
	elif g_command == "compare":
		g_log_A_path = sys.argv[2]
		g_log_B_path = sys.argv[3]
		g_csv_path = sys.argv[4]
		print "comparing ", g_log_A_path,"with",g_log_B_path,"to",g_csv_path
	else :
	
		printUsage()
		
######################################
# PARSE
#
def get_space(line):
	spaces = 0
	for ch in line:
		if ch != ' ':
			break
		spaces = spaces + 1
	
	return spaces

def get_key(line):
	key = ""
	index = line.find('|')
	if index > -1:
		key = line[0:index + 1]
		
	return key


######################################
# UTILITY
#
def progress(value):
	if 0 == (value % 20):
		sys.stdout.write("\b|")
	elif 0 == (value % 15):
		sys.stdout.write("\b\\")
	elif 0 == (value % 10):
		sys.stdout.write("\b-")
	elif 0 == (value % 5):
		sys.stdout.write("\b/")

	
######################################
# READ
#
def findShaderPath(line):
	match = re.search('^Building: (X:.*\.fx)', line)
	if match <> None :
		return match.group(1)
	return None

def findShaderName(shaderpath):
	f = shaderpath.rfind('\\')
	return shaderpath[f+1:len(shaderpath)]

def findBuiltshaders(line):
	match = re.search('^file://(x:.+txt) Results (\d+) cycles\, (\d+).*$', line)
	if match <> None :
		return [match.group(1),match.group(2),match.group(3)]
	return None

def findBaseshaders(line):
	match = re.search('^(x:.+) Base (\d+) passes, (\d+) registers.*$', line)
	if match <> None :
		return [match.group(1),match.group(2),match.group(3)]
	return None

def findBestShaders(line):
	match = re.search('^(x:.+) Best (\d+) passes, (\d+) registers.*$', line)
	if match <> None :
		return [match.group(1),match.group(2),match.group(3)]
	return None

def findProgramName(shaderpath):
	start = shaderpath.rfind('\\')
	end = shaderpath.rfind('.perf.txt')
	if end == -1:
		end = len(shaderpath)
		
	return shaderpath[start+1:end]

def findProgramType(shaderpath):
	start = shaderpath.rfind('\\')
	end = shaderpath.rfind('.perf.txt')
	type = shaderpath[start+1:start+3].lower()
	if type <> 'ps' and type <> 'vs':
		ps = shaderpath.lower().find('ps')
		vs = shaderpath.lower().find('vs')
		if ps <> -1:
			type = 'ps'
		elif vs <> -1:
			type = 'vs'
		else:
			type = 'na'
			
	return type

def create_base_data(line_array):
	if len(line_array) == 0:
		return {}
	
	shader_map = {}
	
	i = 0
	shader_map[0] = ["shaderName","programName","type","cycleCount","registerCount"]
	i+=1
	parsingShader = 0
	shaderName = 0
	
	for line in line_array:
			shaderPath = findShaderPath(line)
			if shaderPath <> None:
				shaderName = findShaderName(shaderPath)
			else:
				shaderBuild = findBuiltshaders(line)
				if shaderBuild <> None:
					programName = findProgramName(shaderBuild[0])
					type = findProgramType(shaderBuild[0])
					cycleCount = shaderBuild[1]
					registerCount = shaderBuild[2]
					shader_map[i] = [shaderName,programName,type,cycleCount,registerCount]
					i+=1
				

	return shader_map

def create_rdm_data(line_array):
	if len(line_array) == 0:
		return {}
	
	shader_map = {}
	
	i = 0
	shader_map[0] = ["shaderName","programName","type","baseCycleCount","baseRegisterCount","bestCycleCount","bestRegisterCount"]
	i+=1
	
	parsingShader = 0
	shaderName = 0
	baseShader = None
	bestShader = None
	for line in line_array:
			shaderPath = findShaderPath(line)
			if shaderPath <> None:
				shaderName = findShaderName(shaderPath)
				baseShader = None
				bestShader = None
			else:
				curBaseShader = findBaseshaders(line)
				curBestShader = findBestShaders(line)

				if curBaseShader <> None:
					baseShader = curBaseShader
					
				if curBestShader <> None:
					bestShader = curBestShader
					if baseShader[0] == bestShader[0] :
						programName = findProgramName(baseShader[0])
						type = findProgramType(baseShader[0])
						baseCycleCount = baseShader[1]
						baseRegisterCount = baseShader[2]
						bestCycleCount = bestShader[1]
						bestRegisterCount = bestShader[2]
						shader_map[i] = [shaderName,programName,type,baseCycleCount,baseRegisterCount,bestCycleCount,bestRegisterCount]
						i+=1
					baseShader = None
					bestShader = None

	return shader_map

def export_csv(data):
	fout = open(g_csv_path, "w")
	for programSet in data:
		for value in data[programSet]:
			fout.write(value+",")
		fout.write('\n')
	fout.close()
		
def read():
	global main_log_data
	main_log_data = {}
	
	if g_command == 'csv' :
		fin = open(g_log_path, "r")
		log_lines = fin.readlines()
		main_log_data = create_base_data(log_lines)
		fin.close()
	elif g_command == 'rdm' :
		fin = open(g_log_path, "r")
		log_lines = fin.readlines()
		main_log_data = create_rdm_data(log_lines)
		fin.close()
		return
	elif g_command == 'compare':
		global main_log_A_data
		global main_log_B_data
		
		fin = open(g_log_A_path, "r")
		log_lines = fin.readlines()
		main_log_A_data = create_base_data(log_lines)
		fin.close()
		fin = open(g_log_B_path, "r")
		log_lines = fin.readlines()
		main_log_B_data = create_base_data(log_lines)
		fin.close()
	

######################################
# WRITE
#
def write():
	export_csv(main_log_data)
		

######################################
# COMPARE
#

def compare():
	if g_command == 'compare' :
		logAMap = {}
		logBMap = {}
		for programSet in main_log_A_data:
			shaderName = main_log_A_data[programSet][0]
			programName = main_log_A_data[programSet][1]
			type = main_log_A_data[programSet][2]
			cycleCount = main_log_A_data[programSet][3]
			registerCount = main_log_A_data[programSet][4]
			logAMap[shaderName,programName,type] = [cycleCount,registerCount]

		for programSet in main_log_B_data:
			shaderName = main_log_B_data[programSet][0]
			programName = main_log_B_data[programSet][1]
			type = main_log_B_data[programSet][2]
			cycleCount = main_log_B_data[programSet][3]
			registerCount = main_log_B_data[programSet][4]
			logBMap[shaderName,programName,type] = [cycleCount,registerCount]

		i=0
		main_log_data[i] = ["shaderName","programName","type","cycleCountA","registerCountA","cycleCountB","registerCountB"]
		i+=1
		for programSet in main_log_A_data:
				shaderName = main_log_A_data[programSet][0]
				programName = main_log_A_data[programSet][1]
				if shaderName <> 'shaderName' and programName <> 'programName':
					type = main_log_A_data[programSet][2]
					cycleCountA = logAMap[shaderName,programName,type][0]
					registerCountA = logAMap[shaderName,programName,type][1]
					cycleCountB = logBMap[shaderName,programName,type][0]
					registerCountB = logBMap[shaderName,programName,type][1]
						
					main_log_data[i] = [shaderName,programName,type,cycleCountA,registerCountA,cycleCountB,registerCountB]
					i += 1
	
######################################
# MAIN
#
def main():
	usage()
	read()
	compare()
	write()
	
main()
















