# 8x8x8 LED Cube Powered by ESP32 & Blynk 🧊💡

## 📌 Overview
This project is an 8x8x8 LED matrix containing 512 LEDs, driven by an ESP32 microcontroller. The system is controlled wirelessly via the **Blynk** IoT platform. To overcome the physical limitations of microcontroller I/O pins, the hardware utilizes Time-Division Multiplexing (TDM) and nine cascaded 74HC595 shift registers. This reduces the required data lines to just three physical pins. 

By leveraging the ESP32's 240 MHz dual-core architecture, the system achieves a flawless 100 Hz refresh rate for smooth Persistence of Vision (POV) 3D animations.


https://github.com/user-attachments/assets/ece2a988-989e-4ad9-9f22-1e67aad0af72


## ✨ Features
* **Blynk IoT Integration:** Fully controllable via smartphone using the Blynk app to switch between the pre-programmed 3D animations.
* **Dual-Core Processing (FreeRTOS):** * **Core 1:** Strictly dedicated to the high-speed LED multiplexing loop to prevent any flickering or dropped frames.
  * **Core 0:** Manages Wi-Fi connections and the Blynk protocol stack independently.
* **15 Built-in Animations:** Includes animations such as Rain, 3D Wave, Spiral, Expanding Box, and Firework.
* **Hardware Safety:** Uses 220Ω current-limiting resistors. This ensures the maximum total current draw remains safely under 5A, even in absolute maximum load conditions.

## 🛠️ Hardware Requirements (BOM)
* **Microcontroller:** 1x ESP32 Development Board
* **LEDs:** 512x 5mm LEDs (Green was used in the original build)
* **Shift Registers:** 9x 74HC595 ICs
* **Transistors:** 16x NPN Transistors for layer switching
* **Resistors:** 64x 220Ω Resistors
* **Power Supply:** 5V / 5A DC Power Supply
* **Miscellaneous:** IC Sockets (16 pins), Perfboard/PCB, and jumper wires

## 🔌 Pin Configuration
The ESP32 communicates with the shift registers using custom software SPI mapping to ensure signal stability:
* **DATA (SER):** GPIO 23
* **CLOCK (SRCLK):** GPIO 18
* **LATCH (RCLK):** GPIO 5
<img width="940" height="668" alt="image" src="https://github.com/user-attachments/assets/c0cb2c84-473f-4db2-a620-056e0bbd3ee8" />

## 🚀 Installation & Setup

1. **Clone the repository:**
```bash
git clone https://github.com/ithmod/Led-Light-Cube-8x8x8-using-ESP32.git
```

2. **Blynk Configuration:**
   * Open the `.ino` / `.cpp` file in your IDE.
   * Insert your `BLYNK_AUTH_TOKEN`, `WIFI_SSID`, and `WIFI_PASSWORD`.
   * Set up a Datastream in your Blynk Web Dashboard (e.g., a Virtual Pin) to send animation commands.

3. **Upload the Code:** * Compile and upload using PlatformIO or the Arduino IDE. Make sure the ESP32 board package is installed.

4. **Power Up:** * Connect the external 5V/5A power supply. **Do not power the full cube directly from the ESP32 5V pin to avoid hardware damage!**

## 👨‍💻 Author
**Ahmed Diaa Al-Zuhairi** *Electrical Engineer* Let's connect! 
* **LinkedIn:** [https://www.linkedin.com/in/ahmed-diaa-759376371/]
* **GitHub:** [@ithmod](https://github.com/ithmod)

## 🤝 Contributing
Contributions, issues, and feature requests are welcome!

## 📄 License
This project is open-source and available under the [MIT License](LICENSE).
