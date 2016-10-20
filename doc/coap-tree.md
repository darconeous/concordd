# CoAP Tree

Eventually want a CoAP server that implements something like the following:

* `p/[partition-number]/`
    * `l/[light-number]/` (Lights)
        *   `name`
        *   `desc`
        *   `type`
        *   `value`
        *   `sensor-zone`
    *   `z/[zone-number]/` (Zones)
        *   `name`
        *   `desc`
        *   `type`
        *   `tripped`
        *   `fault`
        *   `trouble`
        *   `alarm`
        *   `bypass`
        *   `is-wireless`
        *   `is-enabled`
    *   `s/[schedule-number]/` (Schedule)
        * `start-hour`
        * `start-minute`
        * `stop-hour`
        * `stop-minute`
        * `days-of-week`
    * `e/[event-number]/` (Scheduled events)
        * `type`
        * `schedules`
    *   `arm/`
        *   `level`
        *   `date`
        *   `user`
    *   `alarm/`
        *   `code`
        *   `output`
        *   `user`
        *   `is-burg`
        *   `is-trouble`
        *   `is-fault`
        *   `is-aux`
        *   `is-fire`
    * `ui/`
        * `text`
        * `keypress`
        * `siren`
