#!/usr/bin/env tclsh

# ****************************************
#              Galen Helfter
#           ghelfter@gmail.com
#            batch_collect.tcl
# ****************************************

# Script to collect all of the rendered frames out of the render directory,
# using the JSON file provided. Uses expect to SSH into all the machines
# and recover their JSON information, then uses SCP to transfer the frames
# back on the original computer.

package require json
package require Expect

proc print_usage {} {
    puts "Usage: ./batch_collect.tcl {config_file} {final_directory}"
}

proc print_help {} {
    puts "batch_collect:\n"
    puts "DESCRIPTION"
    puts "This program is part of a set of utility scripts making a\
          distributed render queue. These  utilities take JSON configuration\
          files, and use them to distribute rendering tasks to the set of\
          machines specified in them.\n"
    puts "This script uses expect and SSH to connect to the specified hosts,\
          given the configuration file, and collect all of the frames in\
          their render directory onto the current machine using the SCP\
          command."
    puts "\nAUTHOR\nThese scripts were written by Galen Helfter."
}

proc build_computer {prefix suffix n} {
    return "${prefix}${n}${suffix}"
}

proc load_json {filename} {
    set fp [open $filename]
    set json_data [read $fp]
    close $fp
    return [json::json2dict $json_data]
}

# Check arguments
if {$argc < 2} {
    if {[string compare [lindex $argv 0] "-h"] == 0} {
        print_help
        exit 0
    } elseif {[string compare [lindex $argv 0] "--help"] == 0} {
        print_help
        exit 0
    } else {
        print_usage
        exit 1
    }
}

foreach arg $argv {
    if {[string compare $arg "-h"] == 0} {
        print_help
        exit 0
    } elseif {[string compare $arg "--help"] == 0} {
        print_help
        exit 0
    }
}

# Assert JSON file exists
if {![file exists [lindex $argv 0]]} {
    puts "JSON file not found."
    exit 1
}
