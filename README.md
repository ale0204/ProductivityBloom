# Productivity Bloom

## A Smart Embedded System for Focus Session Management

Productivity Bloom is a physical cube built around an ESP32 microcontroller that transforms focus sessions into an interactive, gamified experience. The system combines hardware sensors (accelerometer, light sensor), a grayscale OLED display, audio feedback, and a real-time web interface to create a tangible productivity tool that responds to physical gestures.

Unlike traditional software-based Pomodoro timers, this project embodies the concept of "calm technology" - technology that exists in the physical world, requires minimal attention, and provides ambient feedback. The user interacts with the system by physically flipping the cube to start/stop focus sessions, while a virtual plant grows on the display as tasks are completed, creating an emotional connection to productivity habits.

The system operates as a standalone embedded device with its own web server, eliminating dependency on external services or internet connectivity for core functionality. Time synchronization via NTP ensures accurate daily goal tracking, with automatic midnight resets and persistent storage of statistics across power cycles.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Features](#features)
3. [Hardware Components](#hardware-components)
4. [Electrical Schematic](#electrical-schematic)
5. [Software Architecture](#software-architecture)
6. [Installation](#installation)
7. [Usage Guide](#usage-guide)
8. [API Reference](#api-reference)
9. [Technical Challenges](#technical-challenges)
10. [Design Questions](#design-questions)

---

## System Overview

The Productivity Bloom cube serves as a physical interface for the Pomodoro technique, enhanced with gamification elements. The core interaction model is:

1. **Task Creation**: Users add tasks via a mobile-responsive web interface served directly from the ESP32
2. **Task Selection**: Selecting a task on the web interface marks it as "ready to start"
3. **Focus Initiation**: Physically flipping the cube (OLED facing down) starts the focus timer
4. **Focus Session**: Timer counts down on both OLED and web interface; the virtual plant grows
5. **Session End**: Audio countdown at 3-2-1 seconds; flipping cube back triggers completion dialog
6. **Break Period**: Optional break timer with automatic transition
7. **Daily Goals**: At midnight, the system evaluates if daily goals were met; plant withers or blooms accordingly
8. **Recovery Mechanism**: Withered plants can be revived by exposing the light sensor to bright light

The system maintains state across power cycles using the ESP32's Non-Volatile Storage (NVS), ensuring that tasks, plant state, and statistics persist.

---

## Features

### Physical Interaction
- **Flip-based Control**: MPU-6050 accelerometer detects cube orientation; flipping initiates or pauses timers
- **Automatic Display Rotation**: OLED content rotates 180 degrees based on cube orientation for readability
- **Light-based Revival**: LDR sensor enables plant revival through 3-second light exposure

### Visual Feedback
- **128x128 Grayscale OLED**: Displays timer, plant growth stages, and status information
- **4-Stage Plant Growth**: Seed, Sprout, Growing, Bloomed - visual progress tied to task completion
- **QR Code Display**: In Access Point mode, displays QR code for easy connection

### Audio Feedback
- **Countdown Beeps**: Melodic warnings at 3, 2, 1 seconds before timer completion
- **Passive Buzzer**: PWM-driven for frequency control

### Web Interface
- **Real-time Updates**: WebSocket connection provides instant synchronization
- **Mobile-first Design**: Responsive CSS optimized for smartphone use
- **Task Management**: Add, edit, delete tasks with customizable durations
- **Statistics Dashboard**: Daily focus time, tasks completed, session history

### Network Capabilities
- **Dual-mode WiFi**: Station mode for home network; Access Point fallback with captive portal
- **NTP Synchronization**: Automatic time sync for accurate midnight resets
- **RESTful API**: Full HTTP API for integration possibilities

### Data Persistence
- **NVS Storage**: Plant state, statistics, and configuration survive power cycles
- **Task Persistence**: Active tasks maintained across restarts
- **Analytics History**: Daily statistics with proper session recording

---

## Hardware Components

### Bill of Materials

| Component | Quantity | Specifications | Purpose |
|-----------|----------|----------------|---------||
| ESP32 WROOM-32 | 1 | DevKit V1, 38 pins | Main microcontroller |
| OLED Display | 1 | Waveshare 1.5", SSD1327, 128x128, SPI | Visual output |
| MPU-6050 | 1 | 6-DOF Accelerometer/Gyroscope, I2C | Flip detection |
| LDR Photoresistor | 1 | GL5528 or equivalent | Light detection |
| Piezo Buzzer | 1 | Passive, 5V rated | Audio feedback |
| Resistor 10k Ohm | 1 | 1/4W | LDR voltage divider |
| Resistor 220 Ohm | 1 | 1/4W | Buzzer current limiting |
| Li-Ion 18650 Battery | 2 | 3.7V, 2000-3000mAh each | Power supply |
| Battery Holder | 1 | 2-slot, parallel configuration | Battery mounting |
| TP4056 Module | 1 | With protection circuit | Battery charging |
| Boost Converter | 1 | MT3608 or similar, adjustable output | Step-up to 5V |
| Capacitors | As needed | 100uF, 10uF, 0.1uF | Filtering for regulator, ESP32, OLED |
| Enclosure | 1 | Cube form factor, approx. 10cm | Housing |
| Dupont Wires | As needed | Male-Female and Male-Male | Connections |

### Power Configuration

The system uses two 18650 Li-Ion batteries connected in parallel, with a boost converter to provide stable 5V:

- Battery Output: 3.7V nominal (3.0V-4.2V range)
- Boost Converter: Steps up to 5V for ESP32 VIN
- Combined Capacity: 4000-6000mAh
- Runtime: Approximately 8-12 hours continuous operation
- Charging: Via TP4056 module with micro-USB input

The boost converter output was calibrated to exactly 5V using a separate adjustable power supply module with LCD display for precision voltage measurement during the tuning process.

---

## Electrical Schematic

### Pin Connections

```
ESP32 WROOM-32 Pin Assignments
===============================

OLED Display (SPI):
  VCC  --> 3.3V
  GND  --> GND
  DIN  --> GPIO23 (MOSI)
  CLK  --> GPIO18 (SCLK)
  CS   --> GPIO5
  DC   --> GPIO16
  RST  --> GPIO4

MPU-6050 (I2C):
  VCC  --> 3.3V
  GND  --> GND
  SDA  --> GPIO21
  SCL  --> GPIO22

LDR Circuit (Voltage Divider):
  LDR  --> 3.3V
  LDR  --+--> GPIO34 (ADC input)
         |
  10k   -+--> GND

Piezo Buzzer:
  GPIO25 --[220 Ohm]--> Buzzer (+)
  GND                --> Buzzer (-)

Power:
  Battery Pack (+) --> TP4056 B+ --> Boost IN+ --> Boost OUT+ (5V) --> ESP32 VIN
  Battery Pack (-) --> TP4056 B- --> Boost IN- --> Boost OUT-      --> ESP32 GND
  
  Note: Boost converter adjusted to 5V output using external 
        regulated power supply with display for calibration
```

### Battery and Power Regulation

```
  +--------+     +--------+
  | Cell 1 |     | Cell 2 |
  | 3.7V   |     | 3.7V   |
  +---+----+     +----+---+
      |    (+)       |
      +------+-------+
             | 3.7V
             v
      +------+------+
      |   TP4056    |
      |  Charger    |
      +------+------+
             | 3.7V
             v
      +------+------+
      |    Boost    |
      |  Converter  |
      |  (set 5V)   |
      +------+------+
             | 5V
             v
      +------+------+
      |  ESP32 VIN  |
      +-------------+
```

---

## Software Architecture

### Project Structure

```
ProductivityBloom/
|-- README.md
|-- src/
    |-- finall.ino              # Main entry point
    |-- config.h                # Configuration constants
    |
    |-- SystemState.h           # Global state management
    |-- EventQueue.h            # Thread-safe event queue
    |
    |-- WebServerHandler.h      # HTTP server + WebSocket
    |-- MultiCoreWebServer.h    # Dual-core wrapper
    |-- WebContent.h            # Compiled HTML/CSS/JS
    |
    |-- DisplayRenderer.h       # OLED drawing functions
    |-- QRCodeGenerator.h       # QR code generation
    |
    |-- MPU6050Handler.h        # Accelerometer driver
    |-- BuzzerHandler.h         # Audio output control
    |-- Analytics.h             # Statistics and NTP
    |
    |-- IntervalTimer.h         # Non-blocking timers
    |-- TimedScreenManager.h    # Overlay management
    |
    |-- build_webcontent.py     # Web asset compiler
    |
    |-- data/
        |-- index.html          # Web interface structure
        |-- style.css           # Styles (mobile-first)
        |-- app.js              # Client-side logic
```

### Dual-Core Architecture

The ESP32's dual-core capability is leveraged for responsive operation:

- **Core 0**: WebSocket server, HTTP request handling, WiFi management
- **Core 1**: Main application loop, sensor reading, display updates, timer logic

Inter-core communication is handled through a thread-safe event queue with proper mutex protection for shared state variables.

### Event-Driven Design

```
+-------------+     +-------------+     +-------------+
|   Sensors   |---->| EventQueue  |---->|   Handler   |
| MPU, LDR    |     |   (FIFO)    |     |   loop()    |
+-------------+     +-------------+     +-------------+
                          ^
                          |
                    +-----+-----+
                    | WebSocket |
                    |  Core 0   |
                    +-----------+
```

Events include: TIMER_TICK, STATE_CHANGED, PLANT_WATERED, PLANT_BLOOMED, PLANT_WITHERED, PLANT_REVIVED, FLIP_CONFIRM_NEEDED, FLIP_RESUMED, WEB_BROADCAST

---

## Installation

### Prerequisites

- Arduino IDE 2.0+ or PlatformIO
- ESP32 Board Package installed
- Required libraries: U8g2, WebSockets, ArduinoJson, QRCode

### Setup Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/ale0204/ProductivityBloom.git
   cd ProductivityBloom/src
   ```

2. Configure WiFi credentials in `config.h`:
   ```cpp
   #define WIFI_SSID "YourNetworkName"
   #define WIFI_PASSWORD "YourPassword"
   ```

3. If modifying web files, regenerate WebContent.h:
   ```bash
   python build_webcontent.py
   ```

4. Upload to ESP32:
   - Board: ESP32 Dev Module
   - Port: Appropriate COM port
   - Upload Speed: 921600

5. Find the device IP:
   - Open Serial Monitor at 115200 baud
   - Look for: "Access web interface at: http://192.168.x.x"

---

## Usage Guide

### Initial Setup

1. Power on the device
2. ESP32 attempts WiFi connection (configured network)
3. If successful: IP address shown on OLED and Serial
4. If failed: Creates "ProductivityBloom" access point with QR code

### Basic Workflow

1. Access web interface from phone or computer
2. Add tasks with name and duration
3. Select a task from the list
4. Flip cube (OLED facing down) to start timer
5. Focus until countdown beeps
6. Flip cube back when finished
7. Mark task complete or continue

### Cube Orientation Reference

| Cube Position | System Action |
|---------------|---------------|
| OLED visible (up) | Idle / Paused state |
| OLED hidden (down) | Focus mode active |

### Plant Revival

When plant is withered:
1. Expose LDR sensor to bright light
2. Maintain exposure for 3 seconds
3. Plant revives to Seed stage

---

## API Reference

### REST Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main web interface |
| `/api/status` | GET | Current system state |
| `/api/tasks` | GET | Task list |
| `/api/tasks` | POST | Add new task |
| `/api/plant` | GET | Plant state |
| `/api/analytics` | GET | Statistics |
| `/api/action` | POST | Control actions |

### WebSocket Protocol

Connection: `ws://[device-ip]:81`

Server-to-Client Messages:
```json
{"type": "status", "mode": "focusing", "timeLeft": 1423, "totalTime": 1500}
{"type": "plant", "stage": 2, "isWithered": false, "wateredCount": 3}
{"type": "tasks", "tasks": [...]}
{"type": "flipConfirm"}
{"type": "flipResumed"}
{"type": "revive"}
```

Client-to-Server Messages:
```json
{"action": "addTask", "task": {"name": "Study", "focusDuration": 25, "breakDuration": 5}}
{"action": "startTask", "taskId": 123456}
{"action": "confirmComplete"}
{"action": "confirmAccidental"}
```

---

## Technical Challenges

### 1. Real-time Synchronization

Maintaining consistency between the physical device and multiple web clients required careful engineering:

- WebSocket broadcasts ensure all clients receive updates simultaneously
- Race conditions between Core 0 (network) and Core 1 (logic) required mutex locks
- Event queue decouples sensor inputs from state processing

### 2. ESP32 WiFi Stability

The ESP32's WiFi stack presented several challenges:

- Memory fragmentation after prolonged operation required static buffer allocation
- Dual-core access to WiFi resources needed careful synchronization
- Fallback to Access Point mode required complete network stack reconfiguration

### 3. Physical Integration

Building a functional cube with all components presented non-software challenges:

- Weight distribution: Batteries (heaviest) centered for stable flip detection
- Wire management: Minimal wire lengths to fit within enclosure
- Component mounting: Combination of adhesive and 3D-printed brackets
- Access ports: USB charging port positioning for practical use

### 4. Display Rotation at Runtime

The SSD1327 OLED controller has limited runtime rotation support:

- `setDisplayRotation()` unreliable after initialization
- Solution: Rotation set in U8G2 constructor, changed on flip detection
- Requires full buffer redraw after orientation change

### 5. Accurate Time Tracking

Focus session timing needed to survive pauses and interruptions:

- Accumulated time tracking across pause/resume cycles
- Proper rounding rules (30+ seconds rounds up to next minute)
- NTP synchronization for midnight detection
- Timezone handling for Romanian locale

---

## Design Questions

### Q1: What is the system boundary?

The system boundary encompasses the physical cube device and its self-hosted web interface. The ESP32 acts as both the embedded controller and a web server, creating a self-contained system that requires no external servers or cloud services for core functionality.

External dependencies cross the boundary only for:
- NTP time synchronization (optional, graceful degradation)
- Initial WiFi network connection
- Web browser on user's device for interface access

The boundary explicitly excludes:
- Cloud storage or processing
- External APIs or services
- Database servers
- Authentication services

This design choice ensures the system remains functional in offline environments and eliminates privacy concerns about productivity data leaving the local network.

### Q2: Where does intelligence live?

Intelligence is distributed across three layers:

**Embedded Layer (ESP32)**: Contains the core decision-making logic:
- Timer state machine management
- Flip gesture interpretation with debouncing
- Plant growth algorithm based on task completion
- Midnight goal evaluation and plant withering logic
- Light sensor threshold detection for revival

**Protocol Layer (WebSocket/HTTP)**: Provides the communication intelligence:
- Real-time state synchronization
- Conflict resolution for concurrent actions
- Graceful reconnection handling

**Client Layer (JavaScript)**: Handles presentation intelligence:
- UI state management and animations
- Optimistic updates with server reconciliation
- Offline detection and queuing

The critical intelligence (what determines plant state, timer behavior, goal evaluation) resides entirely on the ESP32, ensuring the system behaves correctly even if the web interface is unavailable.

### Q3: What is the hardest technical problem?

The hardest technical problem was achieving reliable bidirectional real-time synchronization between the ESP32 and multiple web clients while maintaining responsive physical interactions.

Specific challenges included:

1. **Dual-core Race Conditions**: The WebSocket server runs on Core 0 while the main loop runs on Core 1. Shared state (tasks, timers, plant) required mutex protection without blocking the time-critical timer updates.

2. **State Consistency**: When a user flips the cube, the state change must propagate to all connected web clients within 100ms to feel responsive. Simultaneously, web-initiated actions must immediately reflect on the OLED.

3. **Memory Constraints**: WebSocket connections consume significant heap memory. With multiple clients and the JSON serialization overhead, heap fragmentation became a stability issue requiring static buffer allocation.

4. **Timing Accuracy**: Focus sessions must be accurately timed even through pause/resume cycles, WiFi reconnections, and NTP synchronization events.

The solution involved an event-driven architecture with a thread-safe queue, careful memory management, and a clear separation between state mutation (single point of truth in SystemState) and state broadcasting (triggered by events).

### Q4: What is the minimum demo?

The minimum viable demo consists of:

1. **Hardware**: ESP32 + OLED display + MPU-6050 accelerometer (3 components minimum)
2. **Software**: Basic timer with flip detection, simple plant visualization
3. **Interface**: Serial monitor output (no web interface required)

Demo flow (2 minutes):
1. Power on, show plant on OLED (5 sec)
2. Flip cube to start 1 minute focus timer (show countdown)
3. Flip back and confirm you finished the task, so you can "water" your plant
4. Press the button to water it, and watch it grow
5. Press the button Demo Kill to demonstrate the withered state and how to revive it using light

This demonstrates the core value proposition: physical interaction controlling a timer with visual gamification feedback. The web interface, WiFi, NTP, persistence, and audio are enhancements that can be incrementally added.

### Q5: Why is this not just a tutorial project?

This project extends beyond tutorial-level complexity in several dimensions:

**Architectural Complexity**: 
- Dual-core concurrent execution with proper synchronization
- Event-driven architecture with decoupled components
- Multiple communication protocols (SPI, I2C, WiFi, WebSocket)

**Integration Depth**:
- 6 distinct hardware components working in coordination
- Physical form factor constraints driving software decisions
- Power management for battery operation

**Real-world Considerations**:
- Persistent storage across power cycles
- Graceful degradation when WiFi unavailable
- Timezone-aware time handling
- State machine with complex transitions (idle/focusing/paused/break/withered)

**Production Qualities**:
- Mobile-responsive web interface
- Error handling and recovery mechanisms
- Debug output for troubleshooting
- Configuration management

**Novel Interaction Design**:
- Flip-based input paradigm (not commonly covered in tutorials)
- Gamification layer with goal tracking
- Multi-modal feedback (visual, audio, haptic through physical manipulation)

Tutorial projects typically demonstrate a single concept (e.g., "ESP32 web server" or "MPU6050 orientation"). This project integrates multiple concepts into a cohesive product that solves a real problem (focus management) with a novel interaction model (physical cube manipulation).

---

### Libraries Used

- U8g2 - OLED display driver
- arduinoWebSockets - WebSocket implementation
- ArduinoJson - JSON serialization
- QRCode - QR code generation
