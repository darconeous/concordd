NOTES
=====

This document contains random notes, quirks, and undocumented
behavior.

## Concord 4 ##

### Wireless Protocol is Insecure ###

All wireless sensors, fobs, and keypads should be considered insecure
and used for non-security purposes only. The wireless protocol is
unencrypted and offers no protections against replay attacks. Such
sensors are trivial to defeat with tools which are readily available
online.

Only use wireless sensors for monitoring non-security-related things
like mouse traps, fire alarms, and medical panic buttons --- use
hard-wired sensors for everything else.

### Zone Data Protocol Descrepency ###

The automation protocol document says that the "Send Equipment List -
Zone Data (03h)" command will always have the "tripped" flag cleared
to zero. This is demonstably false for the Concord 4 panel, and
`concordd` will respect the returned value.

### Output Protocol Descrepency ###

The observed equipment list output info format doesn't match up well
with the format documented in the official protocol documentation.
I've made a best guess as to what the actual format is, and concordd
parses it accordingly.

### Undocumented keyfob behavior ###

It seems as though a keyfob will assert the trouble flag if the keyfob
hasn't been used for a period around 24 hours. It doesn't seem to
cause any user-visible supervisory condition on the panel.

Pressing both the ARM and DISARM buttons on the fob will momentarilly
cause the "alarm" zone flag to be set. "Trip" doesn't ever appear to
be set.

Contrary to the protocol documentation, the following keyfob button
combinations appear to be entirely ignored by the Concord 4 and are
not reported to the automation unit, despite clear indications that
the Fob is transmitting:

*   6: Long button press on LIGHTS
*   9: Simultaneous press of ARM and STAR
*   10: Simultaneous press of DISARM and LIGHTS

All of the other documented keyfob codes seem to be reported properly.

### Undocumented key code behavior ###

*   0x26: "long lights", will blink all lights on and off again and
    again over and over. You can undo this by arming to level 1.
*   0x2B: Direct arm to level 3 with instant alarm on doors that
    normally have an entry/exit delay
*   0x4C: Police Panic (Long Press)
*   0x4D: Aux Panic (Long Press)
*   0x4E: Fire Panic (Long Press)

### Quick reference ###

These items are technically not undocumented, but this info can be
hard to find in the official documentation so I am leaving it here for
reference purposes.

### Default output configuration ###

The following is the default output configuration for a Concord 4
panel:

Id | Code  | Trigger/Description  | ST\* | Delay | Duration
---|-------|----------------------|------|-------|---------------
1  | 01614 | Exterior Alarm       | YES  | 30s   | Siren Time
2  | 01710 | Interior Siren/Chirp | YES  | 0s    | Siren Time
3  | 01400 | Fob Star-Button      | NO   | 0s    | Momentary
4  | 00410 | Any Audible Alarm    | YES  | 0s    | Siren Time
5  | 00903 | Armed to Level 2/3   | NO   | 0s    | Sustained
6  | 01003 | Armed to Level 3     | NO   | 0s    | Sustained

> *   ST: Siren Tracking

All of the outputs are by default configured for partition 1.

#### Special Lights ####

Light 1 will always turn on and remain on for the duration of the
entry and exit delays. Additional lights may be configured to behave
in this way via the "Entry lights" setting of the light control menu.

Light 2 will always flash the arming level when arming the system. For
example, lights flash two times when arming to stay (Level 2), and
three times when arming to away (Level 3).
