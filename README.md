# 🔥 Auto Cut-Off System for Soldering Iron 

![1733845373900](https://github.com/user-attachments/assets/4986df6e-f328-40d9-bfc0-66eab116373d)

This **Auto Cut-Off System for Soldering Irons**, designed for simple **AC soldering irons without temperature control mechanism**, improves safety and energy efficiency. It automatically cuts off power based on user-set timers or when the iron is detected in its stand for a specific duration. This helps prevent overheating and potential hazards, effectively turning a basic soldering iron into a safer, more efficient soldering station.

---

## 🚀 Features  
- **Auto Power Cut-Off:** Automatically turns off the soldering iron based on timers or inactivity detection.  
- **Dual Timer Settings:** Adjustable on-time and off-time via a rotary encoder.  
- **Stand Detection:** Detects user inactivity when the iron is placed in the stand and cuts off power after a preset duration.  
- **EEPROM Storage:** Retains timer values even after power loss.  
- **Status Display:** 16x2 LCD shows current timer values, system status, and detection status.    

---

## ⚡ Connections
|       **Component**       |  **Arduino Nano Pin**  |              **Notes**               |
|:--------------------------|:-----------------------|:-------------------------------------|
| Relay Module              | D5                     | Controls the soldering iron power    |
| Stand Detection Pin       | D6                     | Detects when the iron is in the stand (when pull-to-ground) by connecting this pin to the stand. |
| Rotary Encoder (CLK, DT)  | D2, D3                 | For adjusting timer values          |
| Encoder Button            | D4                     | For selecting and confirming settings |
| 16x2 LCD (I2C)            | A4 (SDA), A5 (SCL)     | I2C communication with LCD          |
| Power Supply (5V)         | VIN, GND               | External 5V module recommended       |
| Soldering Iron (Earth)    | GND                    | Connect Arduino GND to the iron’s earth pin for proper stand detection. |

***Note:***
In the connection diagram, a push button is used to simulate the soldering iron-in-stand detection logic. I used two 1N4007 diodes in forward bias—one after pin D6 and another with a 1 kΩ current-limiting resistor before GND. This setup prevents false detections and ensures stable operation of the detection logic. Without these diodes, AC noise can couple into the detection circuit, causing false triggers. 

---

## 🐞 Issues  
The **system occasionally resets when the soldering iron is turned on** via the relay. This occurs due to a sudden **current surge** when the soldering iron is connected as the load, causing brief voltage dips that **resets the microcontroller**. Notably, the system operates normally without any resets when the soldering iron is not connected as the load.

#### ***Solutions:*** 
- Use a **Solid State Relay (SSR)** with at least 2A load capacity, as SSRs are more stable, efficient, faster, and durable compared to mechanical relays.

- Upgrade the power supply to a **5V, 5W module** to handle current surges more effectively than the standard 5V, 3W module.

- Add a **100μF electrolytic capacitor** across the power supply output terminals to smooth out voltage fluctuations and prevent microcontroller resets. 

---

## 🌟 Future Improvements  
- Integrate an **OLED display** to replace the 16x2 LCD, offering a smaller footprint, higher contrast, and more display flexibility.

- Introduce an adjustable stand timer, allowing users to modify the **soldering iron-in-stand timer** (currently fixed at 5 minutes) through the encoder without changing the code.

---

## 📜 License  
This project is licensed under the [MIT License](LICENSE).  
