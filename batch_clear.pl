#!/usr/bin/perl -w

# ****************************************
#              Galen Helfter
#           ghelfter@gmail.com
#             batch_clear.pl
# ****************************************

use strict;
use warnings;
use POSIX;
use File::Spec;

# CPAN modules
use Net::OpenSSH;
use Term::ReadKey;
use JSON::XS;

sub print_usage ()
{
    print "Usage: ./batch_clear.pl [config_file]\n";
}

sub print_help ()
{
    print "batch_clear:\n\n";
    print "DESCRIPTION\n";
    print "This program is part of a set of utility scripts making a"
        . " distributed render queue. These utilities take JSON configuration"
        . " files, and use them to distribute rendering tasks to the set of"
        . " machines specified in them.\n\n";
    print "This script uses SSH to connect to the specified hosts, given the"
        . " configuration file, and clears the render directory of all"
        . " files. This will delete all of the frames currently stored,"
        . " and assumes that the frames have already been collected.\n";

    print "\nAUTHOR\nThese scripts were written by Galen Helfter.\n";
}

sub load_json_file ($)
{
    open FD, $_[0];
    my $res = "";

    while(<FD>)
    {
        $res .= $_;
    }

    close FD;

    return $res;
}

if (scalar(@ARGV) < 1)
{
    print_usage;
}
elsif (($ARGV[0] eq "-h") or ($ARGV[0] eq "--help"))
{
    print_help;
}
else
{
    my $cfg = $ARGV[0];

    # Assert that the configuration file is found
    die "Error - config file $cfg not found.\n" unless -e $cfg;

    my $json_str = load_json_file $cfg;
    my $parsed_json = decode_json $json_str;

    # Acquire login information
    print "Enter username: ";
    my $username = Term::ReadKey::ReadLine(0);
    chomp $username;

    print "Enter password: ";
    Term::ReadKey::ReadMode('noecho');
    my $password = Term::ReadKey::ReadLine(0);
    Term::ReadKey::ReadMode('restore');
    chomp $password;
    print "\n";

    # Load hosts and clusters
    my @hosts = ();

    foreach my $elem (@{$parsed_json->{hosts}})
    {
        push @hosts, $elem;
    }

    foreach my $elem (@{$parsed_json->{clusters}})
    {
        my $cluster_start = $elem->{start};
        my $cluster_end = $elem->{end};

        for ($cluster_start .. $cluster_end)
        {
            my $cluster_comp = $elem->{prefix} . $_ . $elem->{suffix};
            push @hosts, $cluster_comp;
        }
    }

    # Clear unneeded variables
    undef $json_str;
    undef $parsed_json;

    my $machine_count = scalar(@hosts);

    my $local_json_fname = "/home/batch_renderer/batch_render.json";

    # Connect to the machines and clear the render directories
    for my $i (0 .. $machine_count-1)
    {
        my $pid = fork;

        if (not $pid)
        {
            my $hostname = $hosts[$i];

            my $ssh = Net::OpenSSH->new($hostname, user => $username,
                                        password => $password);

            die "Error connecting to host $hostname\n" if $ssh->error;

            # Acquire local JSON file
            my $local_json = $ssh->capture("cat $local_json_fname");
            my $parsed_json = decode_json $local_json;

            my $rdir = $parsed_json->{render_directory};

            # Assert the render directory is something useful
            if ( ((length $rdir) != 0) && ($rdir ne "\/") )
            {
                if ($rdir =~ /\/$/)
                {
                    $ssh->system("rm -rf $rdir*");
                }
                else
                {
                    $ssh->system("rm -rf $rdir/*");
                }
            }

            # Disconnect ssh session
            $ssh->disconnect(0);
            exit;
        }
    }

    # Have parent process wait on child processes
    for my $i (0 .. $machine_count-1)
    {
        wait;
    }
}
