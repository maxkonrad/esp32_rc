# ESP32 Sony IR Remote Controller with HTTP API

This ESP32 project creates a WiFi-connected IR remote controller for Sony devices. It exposes an HTTP API to send various IR commands remotely.

## Features

- WiFi connectivity for remote control
- HTTP server with RESTful endpoints
- Sony IR protocol implementation
- Modular RMT-based IR transmission

## Hardware Requirements

- ESP32 development board
- IR LED connected to GPIO 18 (with appropriate transistor driver)
- WiFi network access

## Software Requirements

- ESP-IDF v5.5 or later

## Setup

1. **Configure WiFi**:
   ```
   idf.py menuconfig
   ```
   Navigate to `WiFi Configuration` and set your SSID and password.

2. **Build the project**:
   ```
   idf.py build
   ```

3. **Flash to ESP32**:
   ```
   idf.py flash
   ```

4. **Monitor output**:
   ```
   idf.py monitor
   ```

The ESP32 will connect to WiFi and start the HTTP server. Note the IP address from the logs.

## HTTP API

Send GET requests to the ESP32's IP address on port 80:

| Endpoint | Command | Hex Code |
|----------|---------|----------|
| `/power` | Power On/Off | 0x2215 |
| `/volume_up` | Volume + | 0x2212 |
| `/volume_down` | Volume - | 0x2213 |
| `/function` | Function | 0x2247 |
| `/play` | Play | 0x3232 |
| `/pause` | Pause | 0x3239 |
| `/stop` | Stop | 0x3238 |
| `/tune_up` | Tune + | 0x3273 |
| `/tune_down` | Tune - | 0x3274 |
| `/fast_forward` | Fast Forward | 0x3231 |
| `/fast_backward` | Fast Backward | 0x3230 |

### Example Usage

```bash
# Power on/off
curl http://192.168.1.100/power

# Volume up
curl http://192.168.1.100/volume_up

# Play
curl http://192.168.1.100/play
```

Each request will return a confirmation message and transmit the corresponding IR command.

## Troubleshooting

- **WiFi Connection Issues**: Verify SSID/password and network availability
- **No IR Output**: Check GPIO 18 connection and IR LED circuit
- **HTTP 404**: Ensure correct endpoint URLs
- **ESP-IDF Version**: Update to latest stable version if issues persist
## Circuit Schema
You chould build the following (its gemini generated in this case) circuit for this project. Connecting signal to 18th pin.
<img width="1024" height="559" alt="image" src="https://github.com/user-attachments/assets/2fdaf960-1958-4bb7-a14d-40790b7b1643" />

## Technical Details

- Uses RMT peripheral for precise IR timing (40kHz carrier, Sony protocol)
- HTTP server runs on port 80
- IR commands are transmitted 3 times with 45ms gaps (Sony standard)
I (1325) example: Turning the LED ON!
I (2325) example: Turning the LED OFF!
I (3325) example: Turning the LED ON!
I (4325) example: Turning the LED OFF!
I (5325) example: Turning the LED ON!
I (6325) example: Turning the LED OFF!
I (7325) example: Turning the LED ON!
I (8325) example: Turning the LED OFF!
