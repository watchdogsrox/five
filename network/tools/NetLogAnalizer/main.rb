#!/usr/bin/ruby -w

#
# main.rb
#
# Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
#

begin
    require 'parse_command_line.rb'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

begin
    require 'screen.rb'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

begin
    require 'fileio.rb'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

#######################
### MAIN LOOP
#######################

begin
  #clear screen
  Screen.clear

  puts "##########################"
  puts "  network_logs_analizer  "
  puts "##########################"
  puts ""

  #parse all command arguments
  parser = CommandParser.new

  puts ">> Dir (#{parser.dir}) exists." if FileIO.file_exists(parser.dir)
  puts ">> Dir (#{parser.dir}) does not exists." if not FileIO.file_exists(parser.dir)

  puts ">> Inpufile (#{parser.inputfile}) exists." if FileIO.file_exists(parser.inputfile)
  puts ">> Inpufile (#{parser.inputfile}) does not exists." if not FileIO.file_exists(parser.inputfile)

  puts ">> Outputfile (#{parser.outputfile}) exists." if FileIO.file_exists(parser.outputfile)
  puts ">> Outputfile (#{parser.outputfile}) does not exists." if not FileIO.file_exists(parser.outputfile)

  #FileIO.file_open(parser.inputfile)
  #FileIO.file_output(parser.inputfile)

  if not parser.objnames[1].nil?
      FileIO.file_search(parser.inputfile, parser.outputfile, parser.objnames[0], parser.objnames[1])
    elsif not parser.objnames[0].nil?
      FileIO.file_search(parser.inputfile, parser.outputfile, parser.objnames[0])
    else
      object_name = Screen.pause("Enter object name")
      object_name.gsub!(/ /,'')
      object_name.gsub!(/\n/,'')
      if not object_name.empty?
        FileIO.file_search(parser.inputfile, parser.outputfile, object_name)
      else
        puts ">> No object name found"
      end
    end if FileIO.file_exists(parser.inputfile)

  #pause on exit
  Screen.pause("Press Enter to Exit")
  exit

rescue 
    puts "Couldn't load #{$!}"  # $! contains the last error string
end
