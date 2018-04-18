NOTES
=====

## Concord 4 default output configuration

Id | Code | Trigger              | ST  | Delay | Duration
---|------|----------------------|-----|-------|---------------
1  |01614 | Exterior Alarm       | YES | 30s   | Siren Time
2  |01710 | Interior Siren/Chirp | YES | 0s    | Siren Time
3  |01400 | Fob Star-Button      | NO  | 0s    | Momentary
4  |00410 | Any Audible Alarm    | YES | 0s    | Siren Time
5  |00903 | Armed to Level 2/3   | NO  | 0s    | Sustained
6  |01003 | Armed to Level 3     | NO  | 0s    | Sustained

ST: Siren Tracking

## Concord 4 undocumented key codes

* 0x26: "long lights", will blink all lights on and off again and again
