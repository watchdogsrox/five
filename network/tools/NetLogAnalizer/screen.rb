#!/usr/bin/ruby -w

#
# screen.rb
#
# Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
#

module Screen

     @MAX_DOSWINDOW_LINES = 5

    #PURPOSE
    #  Clear the screen.
    def self.clear
        puts "\n" * (@MAX_DOSWINDOW_LINES * 2 - 1)
        #puts "\a" #Make a little noise to get the player's attention
    end

    #PURPOSE
    #   Pause the display area. Execution until the player
    #   presses the Enter key.
    def self.pause( string = "Press Enter to continue" )
        puts ""
        puts ""
        puts ""
        puts string
        STDIN.gets
    end

end


 