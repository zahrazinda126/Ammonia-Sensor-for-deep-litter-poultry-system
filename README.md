## 🐔 Portable Ammonia Sensor for Deep-Litter Poultry Houses

This project is an **Arduino UNO R4 WiFi–based portable monitor** for poultry houses on deep litter.  
It tracks **ammonia (simulated with an MQ-2 sensor), temperature and humidity**, drives **LED + buzzer alerts**, and exposes a **beautiful web dashboard** over Wi-Fi.

 ## Problem Background

In deep-litter poultry systems, manure decomposes on the floor and silently releases **ammonia (NH₃)**.  
Farmers usually notice **only when birds start coughing, tearing, or dying**:

- Growth rate drops  
- Feed conversion worsens (feed wasted)  
- Mortality increases  
- Profits shrink  

This device gives **early warning** so action is taken **before** birds are visibly affected.

 ## Features

- **Real-time readings**
  - Temperature (°C) – DHT11  
  - Humidity (%) – DHT11  
  - “Ammonia index” – MQ-2 analog value  

- **Clear visual and audio alerts**

  | Condition                                  | LEDs                                | Buzzer |
  |-------------------------------------------|-------------------------------------|--------|
  | Ammonia **high**                          | **RED solid**                       | ON     |
  | Ammonia **moderate**                      | **YELLOW solid**                    | OFF    |
  | Ammonia normal, temp/humidity risk **high** | **RED blinking**                    | OFF    |
  | Ammonia normal, temp/humidity risk **moderate** | **YELLOW blinking**              | OFF    |
  | All safe                                  | **GREEN solid**                     | OFF    |

- **On-device LCD**
  - Line 1: `T:xx.xC H:yy%`  
  - Line 2: Status message (e.g. `NH3 HIGH! VENT`, `Env SAFE`)

- **Wi-Fi dashboard (served from the UNO R4 itself)**
  - Modern responsive UI
  - Auto-refresh every 3 seconds
  - Shows:
    - Temperature
    - Humidity
    - MQ-2 value (“ammonia index”)
    - Overall status + text recommendations


## Hardware

- **Arduino UNO R4 WiFi**
- **MQ-2 gas sensor** (used here as *ammonia index* sensor, analog output)
- **DHT11** temperature & humidity sensor
- **16x2 I2C LCD** (address `0x27`)
- **3× LEDs** (GREEN, YELLOW, RED) + suitable resistors
- **Buzzer**
- Power source (USB or battery pack)

### Pin Mapping (as used in the sketch)

| Function              | Pin         |
|-----------------------|------------|
| DHT11 data            | D2         |
| MQ-2 analog output    | A0         |
| GREEN LED             | D5         |
| YELLOW LED            | D6         |
| RED LED               | D7         |
| Buzzer                | D9         |
| LCD I2C (SDA / SCL)   | Standard I²C pins on UNO R4 WiFi |

---

## 🛠️ Software & Libraries

Board: **Arduino UNO R4 WiFi**

Required libraries:

#include <WiFiS3.h>              // For Wi-Fi on UNO R4 WiFi
#include "DHT.h"                 // DHT11 sensor
#include <LiquidCrystal_I2C.h>   // I2C LCD
