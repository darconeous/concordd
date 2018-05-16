NOTES
=====

This document contains random notes and quirks.

## Concord 4 Zone Data Protocol Descrepency

The automation protocol document says that the "Send Equipment List - Zone Data (03h)"
command will always have the "tripped" flag cleared to zero. This is demonstably false
for the Concord 4 panel, and `concordd` will respect the returned value.

## Concord 4 default output configuration

The following is the default output configuration for a Concord 4 panel:

Id | Code  | Trigger/Description  | ST\* | Delay | Duration
---|-------|----------------------|------|-------|---------------
1  | 01614 | Exterior Alarm       | YES  | 30s   | Siren Time
2  | 01710 | Interior Siren/Chirp | YES  | 0s    | Siren Time
3  | 01400 | Fob Star-Button      | NO   | 0s    | Momentary
4  | 00410 | Any Audible Alarm    | YES  | 0s    | Siren Time
5  | 00903 | Armed to Level 2/3   | NO   | 0s    | Sustained
6  | 01003 | Armed to Level 3     | NO   | 0s    | Sustained

All of the outputs are by default configured for partition 1.

* ST: Siren Tracking

## Concord 4 Special Lights

Light 1 will always turn on and remain on for the duration of the
entry and exit delays. Additional lights may be configured to
behave in this way via the "Entry lights" setting of the light
control menu.

Light 2 will always flash the arming level when arming the system.
For example, lights flash two times when arming to stay (Level 2),
and three times when arming to away (Level 3).

## Concord 4 undocumented key codes

* 0x26: "long lights", will blink all lights on and off again and again over and over.
  You can undo this by arming to level 1.
