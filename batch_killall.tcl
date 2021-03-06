#!/usr/bin/env tclsh

# ****************************************
#              Galen Helfter
#           ghelfter@gmail.com
#            batch_killall.tcl
# ****************************************

# Script to kill all of the rendering processes on a given render queue,
# defined by the JSON file provided. Uses expect to SSH into all of the
# machines, load the local JSON files, then kill the processes.

package require json
package require Expect

proc print_usage {} {
    puts "Usage: ./batch_killall.tcl {config_file}"
}

proc print_help {} {
    puts "batch_killall:\n"
    puts "DESCRIPTION"
    puts "This program is part of a set of utility scripts making a\
          distributed render queue. These  utilities take JSON configuration\
          files, and use them to distribute rendering tasks to the set of\
          machines specified in them.\n"
    puts "This script uses expect and SSH to connect to the specified hosts,\
          given the configuration file, and kill all of the running processes\
          for the specified renderer. This will halt all remaining frames,\
          but will not delete the frames that have already been completed."
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
if {$argc < 1} {
    print_usage
    exit 1
} elseif {[string compare [lindex $argv 0] "-h"] == 0} {
    print_help
    exit 0
} elseif {[string compare [lindex $argv 0] "--help"] == 0} {
    print_help
    exit 0
}

# Assert json file exists
if {![file exists [lindex $argv 0]]} {
    puts "JSON file not found."
    exit 1
}

# Parse the JSON file
set parsed_json [load_json [lindex $argv 0]]

set renderer [dict get $parsed_json renderer]

# Build the domain names needed
set hosts [dict get $parsed_json hosts]
set clusters [dict get $parsed_json clusters]

foreach cluster $clusters {
    set prefix [dict get $cluster prefix]
    set suffix [dict get $cluster suffix]
    set start [dict get $cluster start]
    set end [dict get $cluster end]

    for {set i $start} {$i <= $end} {incr i} {
        lappend hosts [build_computer $prefix $suffix $i]
    }
}

# Prompt for username and password
stty echo
send_user "Username: "
expect_user -re "(.*)\n" {set username $expect_out(1,string)}
send_user "Password: "
stty -echo
expect_user -re "(.*)\n" {set password $expect_out(1,string)}
stty echo
send_user "\n"

log_user 0

set prompt "(%|#|\\$|%\]) $"
set prompt2 "(\S+)(\s*)\[$%\]"

# Use expect to connect to all the machines
foreach host $hosts {
    # Connect to the machine
    spawn ssh "$username@$host"

    expect {
        "assword:" {
            send -- "$password\r"
         }
         "you sure you want to continue connecting" {
             send -- "yes\r"
             expect "assword:"
             send -- "$password\r"
         }
    }

    # Parse local JSON file into memory
    set local_json_fname "/home/batch_renderer/batch_render.json"

    expect -re "$prompt"
    send -- "cat $local_json_fname\r"
    expect -re "$prompt"

    set local_json $expect_out(buffer)

    set json_list [split $local_json "\r"]
    set clean_list {}
    for {set i 1} {$i < [expr ([llength $json_list]-1)]} {incr i} {
        lappend clean_list [lindex $json_list $i]
    }

    unset local_json

    set json_dict [json::json2dict [join $clean_list ""]]

    set cmdpath [dict get $json_dict $renderer]
    set cmd "killall ${cmdpath}\r"

    send -- $cmd

    expect -re "$prompt"
    send -- "killall ${renderer}\r"
    
    # Close the connection
    expect -re "$prompt"
    send -- "exit\r"
}
