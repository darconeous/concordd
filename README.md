Concord 4 Home Automation Daemon
=============================

This package implements a Linux/Unix system daemon that communicates
with the Home Automation Module for a Concord 4 alarm system.

Other programs can use the daemon to perform actions on the alarm
system using a D-Bus based API, documented [here][dbus-protoocol].

This project includes a command-line utility to allow for easy
scripted control over the alarm system. Eventually, more complicated
client programs may be implemented, such as CoAP servers.

**This project is a work-in-progress and not all documented features are
yet implemented.** Some documented features work differently than how
they are currently implemented. That being said, it does work well for
arming, disarming, and signaling of alarms.

You should make sure that the power to the computer you run this
daemon on will last for at least as long as the alarm system panel
battery. The easiest way to do this is to run the daemon on a small
low-power computer (like a Raspberry Pi) that is powered directly from
the 12v source from the panel. Otherwise, a large-ish UPS will be
needed.

Have a look at at the concordd configuration file (`concordd.conf`
in `src/concordd`) to see what sorts of configuration options and
hooks are available, as well as comments describing how to use them.

## Getting Help ##

If you are having trouble with concordd, you can [file an issue on
github](https://github.com/darconeous/concordd/issues/new).

## Dependencies ##

concordd depends on DBus and optionally libreadline. If you are
on Raspbian, you can get both by doing this:

    $ sudo apt-get install libdbus-1-dev libreadline-dev

## Getting the sources ##

Now that you've got your dependencies sorted out, you can get the
source code in one of two ways:

### Getting the sources using Git ###

Clone the repository and enter the directory:

    $ git clone git://github.com/darconeous/concordd.git
    $ cd concordd

If you have all of the autoconf tools installed, you can run the
bootstrap script to get the configure script up and running:

    $ ./bootstrap.sh

Alternatively, if you don't have autoconf installed or running that
script simply didn't work, you can skip it by doing the following
instead:

    $ git archive origin/autoconf/master | tar xvm
      # Next line is a work-around for timestamp problems
    $ touch aclocal.m4 && touch configure && touch `find . -name '*.in'`

### Getting the sources from the zip ###

You can download a ZIP file of the latest version
[here](https://github.com/darconeous/concordd/archive/full/master.zip),
which includes the configure script.

## Building, installing, and running ##

After you have obtained the sources, you can run the configure script,
make, and install:

    $ ./configure
    $ make
    $ sudo make install

You should then copy concordd.conf over from `/usr/local/etc` to
`/etc` and edit it to your liking:

    $ sudo cp /usr/local/etc/concordd.conf /etc/concordd.conf
    $ sudo nano /etc/concordd.conf

You can then start `concordd`:

    $ sudo /usr/local/bin/concordd -c /etc/concordd.conf -s /dev/ttyUSB0

Note the `sudo`. You may also need to restart DBus:

	$ sudo systemctl restart dbus

If you now open up another terminal, you should be able to use
`concordctl` to interact with the alarm system:

    $ concordctl
    concordctl:1> help
    Commands:
       partition                  Get info about partition
       system                     Get info about the system
       arm                        Read or change the partition arm level
       refresh                    Refresh alarm system state scoreboards
       panic                      Trigger a panic alarm of the specified type
       keypad                     Emulate a touchpad on this terminal
       keypad-input               Simulate button presses on the keypad
       zone                       Prints out info for one or more zones
       light                      Prints out info for one or more lights
       output                     Prints out info for one or more outputs
       log                        Prints out changes to the system as they occur
       quit                       Terminate command line mode.
       help                       Display this help.
       clear                      Clear shell.
    concordctl:1> system
    [
        "panelType" => 20
        "hwRevision" => 1793
        "swRevision" => 16530
        "serialNumber" => 21320841
        "acPowerFailure" => false
        "acPowerFailureChangedTimestamp" => 0
    ]
    concordctl:1> zone
                   Zone Name | Id |Par|Typ| Grp|Trp|Byp|Tro|Alr|Flt|Last Changed
    -------------------------+----+---+---+----+---+---+---+---+---|------------
                  FRONT DOOR |  1 | 1 | 0 | 10 |   |   |   |   |   | 37.5m ago
          LIVING ROOM MOTION |  2 | 1 | 0 | 17 |   |   |   |   |   | 1.3m ago
              KITCHEN MOTION |  3 | 1 | 0 | 17 | T |   |   |   |   | 1.0s ago
              KITCHEN WINDOW |  4 | 1 | 1 | 13 |   |   |   |   |   | 11.8h ago
              HALLWAY MOTION |  5 | 1 | 0 | 17 |   |   |   |   |   | 24.4m ago
                  PATIO DOOR |  6 | 1 | 0 | 13 |   |   |   |   |   | 2.7d ago
       MASTER BEDROOM MOTION |  7 | 1 | 0 | 17 |   |   |   |   |   | 1.1m ago
                 GARAGE DOOR |  8 | 1 | 1 | 11 |   |   |   |   |   | 1.3d ago
    8 zones total.
    concordctl:1>

Note that the interactive console shown above will only be available
if libreadline was available. Without it you will need to enter the
commands on the same command line as `concordctl`:

    $ concordctl system
    [
        "panelType" => 20
        "hwRevision" => 1234
        "swRevision" => 12345
        "serialNumber" => 12312312
        "acPowerFailure" => false
        "acPowerFailureChangedTimestamp" => 0
    ]

Setting up `concordd` to run as a daemon at startup is left as an
excercise for the reader.

## License ##

The structure of this project is loosely inspired by that of
[wpantund][], and shares the implementation for a few files, with a
few files being direct derivatives. As such, those files have retained
their original copyright notices, and this project is released under
the same license: the Apache 2.0 license. Unless otherwise marked, all
files in this project are covered under that license.

## Links ##

[wpantund]: http://wpantund.org/
[dbus-protoocol]: https://github.com/darconeous/concordd/tree/master/doc/dbus-protocol.md

 * [Home Automation Protocol Document](https://docs.google.com/file/d/0B2YZbA-Smf2WMW9udFZJUVZ4YTg/view) ([alt link](https://web.archive.org/web/20150616041642/http://www.interlogix.com/_/assets/library/Automation%20Module%20Protocol.pdf))
