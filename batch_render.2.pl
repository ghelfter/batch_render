#!/usr/bin/perl -w

# ****************************************
#              Galen Helfter
#           ghelfter@gmail.com
#            batch_render.2.pl
# ****************************************

use strict;
use warnings;
use POSIX;
use File::Spec;

# CPAN modules
use Net::OpenSSH;
use Term::ReadKey;
use JSON::XS;

# New usage will end up being something like this:
#     ./batch_render.2.pl [start] [end] [scene_file]
#                         [render_dir] [config_file]

# Command line arguments:

sub print_usage ()
{
    print "Usage: ./batch_render.pl [start] [end] [scene_file]"
          . " [render_directory] [render_layer] [config_file]\n";
}

# Arguments
#   1) Base filename
#   2) Computer name
sub create_tempfile ($$)
{
    my $tstr = $_[0];
    my ($ext) = $tstr =~ /(\.[^.]+)$/;
    my $res = $_[0];
    $res =~ s/(\.[^.]+)$//;
    $res .= '.' . $_[1] . $ext;

    return $res;
}

# Arguments
#   1) Command structure
#   2) Start frame
#   3) End frame
#   4) Render directory
#   5) Scene file
sub maya_build_command ($$$$$)
{
    my $cmd = $_[0]->{command};

    # Add the arguments
    foreach my $arg (@{$_[0]->{args}})
    {
        $cmd .= " $arg";
    }

    # Add render directory
    $cmd .= (" -rd " . $_[3]);

    # Add start number
    $cmd .= (' ' . $_[0]->{startflag} . ' ' . $_[1]);

    # Add end number
    $cmd .= (' ' . $_[0]->{endflag} . ' ' . $_[2]);

    # Add scene file
    $cmd .= (' ' . $_[4]);

    return $cmd;
}

# Arguments
#   1) Command structure
#   2) Start frame
#   3) End frame
#   4) Render directory
#   5) Scene file
sub blender_build_command ($$$$$)
{
    my $cmd = $_[0]->{command};

    # Add the arguments
    foreach my $arg (@{$_[0]->{args}})
    {
        $cmd .= " $arg";
    }

    # Add start frame
    $cmd .= (' ' . $_[0]->{startflag} . ' ' . $_[1]);

    # Add end frame
    $cmd .= (' ' . $_[0]->{endflag} . ' ' . $_[2]);

    # Add output file
    $cmd .= (' -o ' . $_[3]);

    # Add formatting information
    $cmd .= (' -F ' . $_[0]->{ext});

    # Add engine information
    $cmd .= (' ' . $_[0]->{rflag} . ' ' . $_[0]->{engine});

    return $cmd;
}

# Arguments:
#   1) Number of computers
#   2) Start
#   3) End
sub acquire_frame ($$$)
{
    my @ranges = ();
    my $step = ($_[2] - $_[1]) / $_[0];

    for (0 .. $_[0]-1)
    {
        my $n = $_+1;
        push @ranges, POSIX::floor($_[1] + ($step * $n));
    }
    push @ranges, $_[2];

    return @ranges;
}

# Arguments:
#   1) Index
#   2) Frame array
#   3) Start frame
sub acquire_start ($$$)
{
    my $ind = $_[0] - 1;
    my $res = $_[2];
    if ($ind > 0)
    {
        $res = @{$_[1]}[int($ind)] + 1;
    }

    return $res;
}

# Arguments:
#   1) Index
#   2) Frame array
#   3) End
sub acquire_end ($$)
{
    my $ind = $_[0];
    my $res = @{$_[1]}[int($ind)];

    return $res;
}

# Arguments:
#   1) Filename
sub load_json_file($)
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

if(scalar(@ARGV) < 6)
{
    print_usage();
}
else
{
    # Acquire start and end frames
    my $start_frame = $ARGV[0];
    my $end_frame = $ARGV[1];

    # Acquire scene file and top level directory from command line arguments
    my $scene_file = $ARGV[2];
    my $top_rdir = $ARGV[3];
    my $rl = $ARGV[4];

    my $cfg = $ARGV[5];

    # Acquire absolute filepath
    my $scene_abs = File::Spec->rel2abs($scene_file);

    # Login information
    print "Enter username: ";
    my $username = Term::ReadKey::ReadLine(0);
    chomp $username;

    my $cmd_struct = {
        renderer  => "",
        command   => "",
        ext       => "",
        rflag     => "",
        startflag => "",
        endflag   => "",
        engine    => "",
        hosts     => [],
        args      => []
    };

    my $json_str = load_json_file $cfg;
    print $json_str . "\n";

    my $parsed_json = decode_json $json_str;

    # Load json into the structure
    $cmd_struct->{renderer}  = $parsed_json->{renderer};
    $cmd_struct->{command}   = $parsed_json->{$cmd_struct->{renderer}}->{path};
    $cmd_struct->{startflag} = $parsed_json->{$cmd_struct->{renderer}}
                                                 ->{startflag};
    $cmd_struct->{endflag} = $parsed_json->{$cmd_struct->{renderer}}
                                               ->{endflag};

    my $rd_abs = '';
    # See whether there are renderer specific arguments
    if ($cmd_struct->{renderer} eq 'maya')
    {
        $rd_abs = File::Spec->rel2abs($top_rdir);
    }
    elsif($cmd_struct->{renderer} eq 'blender')
    {
        # Acquire imagetype, engine and renderflag
        $cmd_struct->{rflag} = $parsed_json->{$cmd_struct->{renderer}}
                                                ->{renderflag};
        $cmd_struct->{engine} = $parsed_json->{$cmd_struct->{renderer}}
                                                 ->{engine};
        $cmd_struct->{ext} = $parsed_json->{$cmd_struct->{renderer}}
                                              ->{imagetype};
        $rd_abs = '//' . $top_rdir;
    }

    # Load the arguments
    foreach my $elem (@{$parsed_json->{$cmd_struct->{renderer}}->{args}})
    {
        push @{$cmd_struct->{args}}, $elem;
    }

    # Load the singular hosts
    foreach my $elem (@{$parsed_json->{hosts}})
    {
        push @{$cmd_struct->{hosts}}, $elem;
    }

    # Load any clusters
    foreach my $elem (@{$parsed_json->{clusters}})
    {
        my $cluster_start = $elem->{start};
        my $cluster_end = $elem->{end};

        for ($cluster_start .. $cluster_end)
        {
            my $cluster_comp = $elem->{prefix} . $_ . $elem->{suffix};
            push @{$cmd_struct->{hosts}}, $cluster_comp;
        }
    }

    my $machine_count = scalar(@{$cmd_struct->{hosts}});
    #print $machine_count . "\n";

    #foreach my $elem (@{$cmd_struct->{hosts}})
    #{
    #    print $elem . "\n";
    #}

    print "Enter password: ";
    Term::ReadKey::ReadMode('noecho');
    my $password = Term::ReadKey::ReadLine(0);
    Term::ReadKey::ReadMode('restore');
    chomp $password;
    print "\n";

    # Acquire frame ranges
    my @ranges = acquire_frame($machine_count, $start_frame, $end_frame);

    my @files = ();
    # Acquire array of maya files and copy them
    foreach my $elem (@{$cmd_struct->{hosts}})
    {
        my $tfile = create_tempfile($scene_abs, $elem);
        #system 'cp', '-f', $scene_abs, $tfile;
        push @files, $tfile;
    }

    for my $i (0 .. $machine_count-1)
    {
        my $pid = fork ();
        if (not $pid)
        {
            my $n_start = acquire_start($i, \@ranges, $start_frame);
            my $n_end = acquire_end($i, \@ranges);

            # Acquire hostname
            my $hostname = @{$cmd_struct->{hosts}}[$i];
            my $file = $files[$i];

            # Construct new render directory
            my $cmd = '';
            my $n_rdir = '';

            if ($cmd_struct->{renderer} eq 'maya')
            {
                $n_rdir = $rd_abs . '/' . $hostname;

                $cmd = maya_build_command($cmd_struct, $n_start, $n_end,
                                          $n_rdir, $file);
            }
            elsif ($cmd_struct->{renderer} eq 'blender')
            {
                $n_rdir = $rd_abs;
                $cmd = blender_build_command($cmd_struct, $n_start, $n_end,
                                             $n_rdir, $file);
            }
            print $cmd . "\n";

            # Can add more information, such as a config file there for
            # storing render directories and its own copy of the command.
            # This hasn't been done yet, and would be a useful extension
            # to get it to actually do its job.
            #
            # Will need to also add an scp command in order to bring the
            # file over to there

            # Open up the ssh connection, then perform the actions needed
            my $ssh = Net::OpenSSH->new($hostname, user => $username,
                                        password => $password);

            # ssh->scp_put
            #$ssh->system("mkdir -p $n_rdir");
            #$ssh->system($cmd);
            # Disconnect ssh session
            $ssh->disconnect(0);
            exit;
        }
    }

    # Parent process waits for the child processes to finish
    for my $i (0 .. $machine_count-1)
    {
        wait();
    }
}
