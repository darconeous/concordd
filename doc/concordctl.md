# `concordctl`

The command line tool will eventually have the following commands:

## `help <command>`

Prints out syntax information for a specific command. If you leave out the command,
it will print out all of the supported commands.

## `log [--json]`

Prints out log messages for events that occur. Cancel with CTRL-C.

## `speaker [--sound=bel|beep|alsa] [--volume=0-255]`

Emulates alarm speaker/siren. It is limited to making status noises and audible alarms. Tries to use a reasonable sound output mechanism by default, but allows you to specify how the sound is output with the `--sound` argument:

* `bel` - Uses the ASCII `BEL` (0x07) character to try to roughly emulate status beeps and alarms. Available on all platforms.
* `beep` - Uses Linux `ioctl` calls to modulate the PC speaker.
* `alsa` - Outputs sound via Linux's ALSA interface.

## `keypad-emu [--text=ascii|ansi] [--sound=off|bel|beep|alsa] [--volume=0-255]`

Emulates an LCD keypad on the terminal. Typing in numbers and letters works just like they would on a real keypad. LCD text is shown on the screen and updated as necessary. Confirmation and alarm noises can also be emulated with the `--sound` argument.

## `keypad-input [key-string]... [--entry-speed=ms-per-key]`

Enter the given string of keys into the virtual keypad.

## `keypad-text [--json] [--text=ascii|ansi|markdown|html]`

Prints out the current text that appears on the LCD to `stdout`.

## `arm [--json] <arm-level>`

Change the arming level of the panel. If no arm-level is specified, prints out the current arm level.

## `bypass <zone-number>...`

Bypass one or more zones.

## `unbypass <zone-number>...`

Unbypass one or more zones.

## `zone [--json] [--filter=tripped|bypassed|trouble|alarm] [zone-number]...`

Prints out the status of the given zones. If no zones are given, all configured zones are printed.

## `light [--json] [light-number] [light-state]`

If no arguments are given, prints out the status of all lights.
If only a light number is given, prints out the status of that light.
If both a light number and a light state are given, the light is changed to the given state.

State values can be:

* `0` or `off`: The light is turned off.
* `1` or `on`: The light is turned on.
* `t` or `toggle`: The light is toggled.

Additionally, the identifier `all` can be used for the light number to change the value of all lights.
