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
