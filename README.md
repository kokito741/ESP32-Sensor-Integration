# Home Environmental Monitoring – Hardware Code

This repository contains the hardware code for the Home Environmental Monitoring System, which collects environmental data from multiple sensors using an ESP32 microcontroller.

## Features

Measures temperature, humidity, noise, UV, ambient light, and battery/Wi-Fi status.

Sends sensor data to the FIWARE platform for further processing and visualization.

Works with the corresponding dashboard configuration in Grafana.

## Files

*.ino – Arduino/ESP32 sketch for sensor reading and data transmission.

sketch.json – Project configuration file for the Arduino IDE.

## Usage

Open the .ino file in Arduino IDE.

Connect the ESP32 and the required sensors according to the wiring diagram.

Configure the Wi-Fi credentials and FIWARE endpoint in the code.

Upload the sketch to the ESP32.

Verify that data is being sent to the FIWARE platform and visualized in Grafana.
