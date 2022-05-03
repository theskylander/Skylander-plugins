
# Controller Plugin for the Dreadbox Nymphes

This plugin is a midi controller for the Dreadbox Nymphes. It receives and sends CC messages to reflect the actual settings of the Nymphes 6-voice synthesizer.
It is set-up for the latest v2.1 version of the firmware. Note that it will not work correctly with v1 as the mappings have been changed.

In addition to the standard controls there is also program change which can be controlled by CV inside of Rack. There is also the option of saving and loading patches according to a human-readable text format. In the future I might add Sysex support to follow the standard patch save and load method of the Nymphes, but for now it is independent.

All settings can be controlled by CV within Rack, which makes for some interesting possibilities for modulator the already extensive modulators. Most controls are one per knob, except for the modulation destinations for which the slider display and control are switchable.

See [skylander website](https://skylander.ch) for contact and more info.

This plugin is released into the public domain ([CC0](https://creativecommons.org/publicdomain/zero/1.0/)).