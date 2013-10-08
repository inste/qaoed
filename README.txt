qaoed(8)                  BSD System Manager's Manual                 qaoed(8)

NAME
     qaoed - Mulithreaded ATA over Ethernet storage target qaoed(8)

SYNOPSIS
     qaoed [-V -h -f] [-b config_file]

DESCRIPTION
     Qaoed is a multithreaded ATA over Ethernet storage target that is easy to
     use and yet higly configurable. Without any argument qaoed will try to
     read the configuration file /etc/qaoed.conf.

     NOTE: Most of qaoed's options and behaviour is controled by the configu-
     ration file. Please refer to the man-page of the configuration file as
     well as the example configuration included in the package for further
     information on how to use qaoed.

   OPTIONS
     Valid options are:

     -V          Print version and exit

     -h          Print usage and exit

     -f          Run the qaoed server in the foreground; don't fork(2) and and
                 daemonize. (The default is to demonize)

     -c config_file
                 Use an alternate config_file.

SIGNALS
     SIGHUP  Causes server to reread and reload the configuration.

FILES
     /etc/qaoed.conf  Default qaoed configuration file

SEE ALSO
     qaoed.conf(5)

     http://en.wikipedia.org/wiki/ATA-over-Ethernet

     http://www.coraid.com/documents/AoEr8.txt

     http://www.coraid.com/documents/AoEDescription.pdf

BSD                            October 24, 2006                            BSD
