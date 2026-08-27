# RTG Fuel Telemetry System

## Overview

The RTG Fuel Telemetry System is an Industrial IoT prototype developed for remote monitoring of diesel fuel levels on Rubber Tyred Gantry (RTG) cranes.

The system was developed and tested on **RTG14** at APM Terminals (WEST AFRICA CONTAINER TERMINAL), Onne, Nigeria.

The main objective is to eliminate the need for manual fuel-level checks by providing remote access to the RTG fuel level through a mobile dashboard and SMS notifications.

## Current Prototype

The current prototype uses direct cellular communication and does not include LoRa.

### System Architecture

Fuel Level Sensor
        ↓
      ESP32
        ↓
     A7670G
      ↙   ↘
   Blynk   SMS

## Hardware

- ESP32 development board
- 0–5 V DC fuel level sensor
- A7670G LTE Cat-1 communication module
- 24 V DC equipment supply
- DC-DC buck converter
- Voltage divider for ESP32 ADC input
- Diesel fuel tank

## Software

- PlatformIO
- Visual Studio Code
- Arduino framework
- Blynk IoT
- TinyGSM
- C/C++ firmware

## Fuel Monitoring

The RTG fuel tank has a capacity of approximately **2,000 litres**.

The fuel-level sensor produces a 0–5 V DC signal. Because the ESP32 ADC input operates at a lower voltage range, a voltage-divider circuit is used to interface the sensor with the ESP32.

The ESP32 reads the sensor signal, converts the ADC reading into a fuel-level value, and transmits the data through the A7670G cellular module.

## Data Transmission

The prototype provides two methods of reporting:

### Blynk

Fuel-level information is transmitted to a Blynk dashboard for remote monitoring.

The dashboard can display:

- Fuel level in litres
- Fuel level percentage
- RTG identification

### SMS

The system can also transmit fuel-level information through SMS.

SMS reporting is intended to provide fuel-level information even when the Blynk dashboard is not being actively monitored.

## Prototype Deployment

The system was initially developed and tested on **RTG14**.

The prototype was used to validate:

- Fuel sensor interfacing
- ESP32 ADC measurement
- Fuel-level conversion
- Cellular communication
- Blynk data transmission
- SMS reporting
- System reliability under operating conditions

## Project Objective

The long-term objective is to scale the working prototype from a single RTG to the RTG fleet.

The terminal has multiple RTG cranes that require fuel monitoring. A scalable communication architecture will therefore be evaluated after successful validation of the current prototype.

## Future Development

Future versions may investigate alternative communication architectures, including **LoRa-based communication**, to reduce the cellular hardware required across multiple RTGs.

The LoRa/SX1262 architecture is **not part of the current prototype** documented in this version of the repository.

## Project Status

**Current status: Prototype operational**

- [x] Fuel sensor interface
- [x] ESP32 fuel-level measurement
- [x] Fuel-level conversion
- [x] A7670G cellular communication
- [x] Blynk integration
- [x] SMS reporting
- [x] RTG14 prototype testing
- [ ] Fleet-wide deployment
- [ ] Scalable communication architecture
- [ ] Multi-RTG dashboard

## Author

Developed as an Industrial IoT / Kaizen project for improving RTG fuel monitoring and reducing manual inspection activities.
