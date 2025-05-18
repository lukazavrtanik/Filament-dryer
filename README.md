# Filament dryer
 Simple filament dryer made from RepRap 3D printer materials

My goal was to make a simple filament dryer using mainly components that were lying around from previous projects. The result is a crude but effective filament dryer.
This build was optimized for recycling old 3D printer components, so it wouldn't make much sense to make it if you don't have some old printer parts from previous projects.
_______________________________________________________

Main components:

RAMPS board + arduino mega

RepRap full graphic display

RepRap heated bed with temperature sensor

DHT11 sensor for ambient temperature and relative humidity

Computer fan

ATX power supply Misc (cables, connectors, screws...)
_______________________________________________________

Connections on the RAMPS board:

Screen: Same as on 3D printers

Heated bed: Same as on 3D printers

HEated bed thermistor: Same as on 3D printers

Computer fan: Same as extruder heater

DHT11: D57, Vcc and GND pins on AUX-1
_______________________________________________________

User interface:

The rotary knob is used to input commands:

Pressing the knob changes the active row (temperature, time, presets)

Rotating the knob changes the value of the current row

Double pressing the knob writes the values of a preset (PLA, PETC, TPU, ASA)

Long pressing the knob activates the heater and disables all the other commands

Pressing while the heater is on will turn off the heater and return to the previous mode

_______________________________________________________

![Filament dryer put together for storage](Dryer_storage.jpg)
![Filament dryer assembled and ready for use](Dryer_assembled.jpg)
