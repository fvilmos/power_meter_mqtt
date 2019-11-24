# Power Meter - for devices with pulse output


## ESP32 arduino project, for power meters with pulse output

#About

ESP32 devices are simple but powerfull for IOT applications.
This project presents a solution using a cheap energy monitor, with pulse output, connected to
an ESP32 WROOM 32 board, which over WiFI, using mqtt protocol can be easely connected to a home automation system.

In addtion, a DS18d20 temperature sensor is connected to the board, supplying eith the measured Watts, kWh, the instant temperature in Celsius.

# Hardware Components

## ESP 32 board
## The Power Meter

The power meter (adeleq_02-553_DIG) operating range is 0.05A - 45A, and (the most important part) provide 2000 impulses / kWh.
The outpus is open collector, operateing woltage, from 5-24V. In this case the ESP32 supply provides 3.3V, but this is not a limitation.

<h1 align="center">
  <a name="Pulse Output" href=""><img src="images/adeleq_02-553_DIG.jpg" alt="" width="192"></a>
</h1>

Below a sample picture for the output, where a light blow (~53W) is connected to the power meter.
Measured pulse between two detections is about 30.71s, which leads to a value of ~58.61 Watts.

<h1 align="center">
  <a name="Pulse Output" href=""><img src="images/pulse_measurement.png" alt="" width="192"></a>
</h1>

## The temperature sensor
## Connection diagramm
## Final integration
# How to use

