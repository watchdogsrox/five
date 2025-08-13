#!/usr/bin/ruby -w

#
# parse_command_line.rb
#
# Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
#

# It uses the optparse library, which is included with ruby 1.8
# It handles classic unix style and gnu style options
begin
    require 'optparse'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

begin
    require 'screen.rb'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

module ParseCommandLine

    #Number of obj names
    NUMOBJNAMES = 5

    #Extra debug information on tty
    attr_reader :debugmode

    #Extra debug information on tty
    attr_reader :verbose

    #Current script name being executed
    attr_reader :base

    #Current working directory
    attr_reader :dir

    # Input file that will be parsed
    attr_reader :inputfile

    # Output file were the results are written
    attr_reader :outputfile

    # Objects names
    attr_reader :objnames

    #PURPOSE
    # Initialize
    def initialize

        @inputfile = ""
        @outputfile = ""
        @debugmode = false
        @verbose = false
        @base = File.basename($0)
        @dir  = File.dirname($0)
        @objnames = Array.new(NUMOBJNAMES)

        # ruby has no fileparse equivalent
        #dir, base = File.split($0)
        #ext = base.scan(/\..*$/).to_s

        # Output all params retrieved
        #output_params

        # Parse arguments
        parse_arguments

        #output defined arguments
        output_arguments

    end

    #PURPOSE
    # Output to tty all params from the user input
    def output_params
        puts ""
        ARGV.each { |param|
            puts " .. Got parameter #{param}"
        }
        puts ""
    end

    #PURPOSE
    #  Output to tty available command arguments help.
    def output_arguments
        puts ""
        puts "-v Verbose is on" if @verbose
        puts "-d Debugmode is on" if @debugmode
        puts "-i Inpufile is #{@inputfile}" if not inputfile.empty?
        puts "-o Outfile is #{@outputfile}" if not outputfile.empty?

        for i in 0..NUMOBJNAMES
          puts "-a Object Names is #{@objnames[i]}" if not objnames[i].nil?
        end

        #puts " is #{$count}" if defined? $count
        puts ""
    end

    #PURPOSE
    # Parse command line arguments.
    def parse_arguments

        num = 0

        ARGV.options do |opts|

            opts.banner = "Usage: ruby #{@base} [OPTIONS] INPUTFILES"

            opts.on("-h", "--help", "show this message") {
                puts opts
                Screen.pause("Press Enter to Exit")
                exit
            }

            # The OptionParser#on method is called with a specification of short
            # options, of long options, a data type spezification and user help
            # messages for this option.
            # The method analyses the given parameter and decides what it is,
            # so you can leave out the long option if you don't need it
            opts.on("-v", "--[no-]verbose=[FLAG]", TrueClass, "run verbosly") {
                |@verbose|   # sets @verbose to true or false
            }

            opts.on("-D", "--DEBUG", TrueClass, "turns on debug mode" ){
                |@debugmode|   # sets $debugmode to true
            }

            # opts.on("-c", "--count=NUMBER", Integer, "how many times we do it" ){
                # |$count|      # sets @count to given integer
            # }

            opts.on("-i", "--input=FILE", String, "file to read input from"){
                |@inputfile|   # sets @inputfile to given string
            }

            opts.on("-o", "--output=FILE", String, "file to write output to"){
                |@outputfile|   # sets @outputfile to given string
            }

            if NUMOBJNAMES > num
              opts.on("-a", "--objectNames=FILE", String, "object names to look for"){
                  |@objnames[num]| num +=1# sets @outputfile to given string
              }
            end

            opts.parse!

            if inputfile.empty? || outputfile.empty?
                puts opts
                Screen.pause("Parser - Press Enter to Exit")
                exit
            end
        end
    end
 end


 class CommandParser
    include ParseCommandLine
 end
