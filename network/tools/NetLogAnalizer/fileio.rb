# To change this template, choose Tools | Templates
# and open the template in the editor.

#!/usr/bin/ruby -w

#
# fileio.rb
#
# Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
#

begin
    require 'fileutils'
rescue LoadError
    puts "Couldn't load #{$!}"  # $! contains the last error string
end

module FileIO

    FRAMEDELIMITER = '-------------------------------------------------------------------------------------------------------------'

    #PURPOSE
    #  Check to see if a file exists.
    def self.file_exists (filename)
      File.exists?(filename)
    end

    #PURPOSE
    #  Check to see if it is a file.
    def self.file_isfile (filename)
      FileUtils.touch(filename)
      File.file?(filename)
    end

    #PURPOSE
    #  Check to see if it is a directory.
    def self.file_isdir (dir)
      File.directory?(dir)
    end

    #PURPOSE
    #  Check to see if it is readable.
    def self.file_readable (filename)
      File.readable?(filename)
    end

    #PURPOSE
    #  Check to see if it is writable.
    def self.file_writable (filename)
      File.writable?(filename)
    end

    #PURPOSE
    #  Check to see if it is readable.
    def self.file_search (inputfile, outputfile, *objectname)

      object_name = Screen.pause("Start Parsing... Press enter to continue")

      last_frame = ""

      open(outputfile, 'w') do |w|
        w.write("")

        open(inputfile, 'r') do |f|

          # Look for next occurance
          f.each(FRAMEDELIMITER) do |l|

            if not l.empty?

              # Get Frame number, System Time and Network time
              if l.include?("Frame") && l.include?("System time") && l.include?("Network time") then
                last_frame = l
              end

              objectname.each do |words|
                if words.empty?
                  puts ">> #{words} is Empty"
                  Screen.pause("Press Enter to continue")
                elsif l.include?(words)
                   puts "#{last_frame}"
                   puts "#{l}"
                   w << FRAMEDELIMITER << last_frame << l
                   #Screen.pause("Press Enter to continue")
                end
              end
            end
          end

          #Screen.pause("Press Enter to continue")

        end
    end
    end
end
