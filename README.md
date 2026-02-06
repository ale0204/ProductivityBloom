# 🌱 Productivity Bloom - Smart Productivity Cube

Un cub inteligent de productivitate bazat pe ESP32, care folosește gamification pentru a transforma sesiunile de focus într-o experiență interactivă. Planta virtuală crește pe măsură ce îți completezi task-urile!

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Arduino-orange)

---

## 📋 Cuprins

- [Descriere](#-descriere)
- [Funcționalități](#-funcționalități)
- [Bill of Materials (BOM)](#-bill-of-materials-bom)
- [Schema Electrică](#-schema-electrică)
- [Provocări Tehnice](#-provocări-tehnice)
- [Structura Codului](#-structura-codului)
- [Instalare și Configurare](#-instalare-și-configurare)
- [Utilizare](#-utilizare)
- [API Documentation](#-api-documentation)

---

## 📖 Descriere

**Productivity Bloom** este un cub fizic inteligent care te ajută să te concentrezi pe task-uri folosind tehnica Pomodoro, combinată cu gamification. 

### Conceptul
- Adaugi task-uri prin interfața web de pe telefon
- Selectezi un task și întorci cubul pentru a porni timer-ul
- În timp ce te concentrezi, o plantă virtuală crește pe display-ul OLED
- Dacă completezi toate task-urile zilnice, planta înflorește! 🌸
- Dacă nu îți atingi obiectivele, planta se ofilește... dar o poți reînvia cu lumină! 💡

---

## ✨ Funcționalități

### 🎮 Control prin Flip (MPU-6050)
- **Întoarce cubul** pentru a porni/opri timer-ul
- **OLED în jos** = Focus mode (timer-ul merge)
- **OLED în sus** = Pauză (timer-ul se oprește)
- Display-ul se rotește automat 180° pentru a fi citibil din ambele poziții

### 🌱 Sistem de Plantă Gamificat
- **4 stadii de creștere**: Sămânță → Lăstar → Creștere → Înflorit
- Planta crește cu fiecare task completat
- La miezul nopții se verifică obiectivele zilnice
- **Obiective îndeplinite** = Planta rămâne înflorită
- **Obiective ratate** = Planta se ofilește

### 💡 Revive cu Lumină (LDR Sensor)
- Planta ofilită poate fi reînviată expunând-o la lumină
- Senzorul LDR detectează lumina timp de 3 secunde
- Animație specială de "Revive!" pe OLED și web

### 🔊 Feedback Audio (Piezo Buzzer)
- **Countdown 3-2-1**: Beep-uri melodice înainte de terminarea timer-ului
- Sunetele sunt "cute" și non-intruzive

### 📱 Interfață Web Responsivă
- Funcționează pe orice dispozitiv (telefon, tablet, PC)
- **WebSocket** pentru actualizări în timp real
- Adaugă, editează și șterge task-uri
- Vezi statistici zilnice (timp focusat, task-uri completate)
- Control manual: Start, Pauză, Skip Break
- Demo mode pentru testare

### ⏰ Sincronizare NTP
- Ora se sincronizează automat de pe internet
- Fusul orar România (UTC+2 / UTC+3 DST)
- Reset automat la miezul nopții

### 📊 Analytics & Statistici
- Timp total focusat pe zi
- Număr de task-uri completate
- Număr de sesiuni de pauză
- Persistență în NVS (Non-Volatile Storage)

---

## 🛒 Bill of Materials (BOM)

| Componentă | Cantitate | Specificații | Notă |
|------------|-----------|--------------|------|
| **ESP32 WROOM-32** | 1 | DevKit V1, 38 pini | Microcontroller principal |
| **OLED Display** | 1 | Waveshare 1.5" SSD1327, 128x128px, SPI | Display grayscale |
| **MPU-6050** | 1 | Accelerometru + Giroscop 6-DOF, I2C | Detectare flip |
| **LDR (Fotorezistor)** | 1 | GL5528 sau similar | Detectare lumină |
| **Piezo Buzzer** | 1 | Pasiv, 5V | Feedback audio |
| **Rezistor 10kΩ** | 1 | 1/4W | Pull-down pentru LDR |
| **Rezistor 220Ω** | 1 | 1/4W | Limitare curent buzzer |
| **Baterii Li-Ion 18650** | 2 | 3.7V, 2000-3000mAh | **Conectate în PARALEL** |
| **Suport baterii 18650** | 1 | 2 sloturi, paralel | Pentru baterii |
| **Modul TP4056** | 1 | Cu protecție, Micro-USB | Încărcare baterii |
| **Cub transparent/translucid** | 1 | ~10cm latură | Carcasă |
| **Fire Dupont** | ~20 | M-F și M-M | Conexiuni |
| **Breadboard mini** | 1 | 170 puncte | Opțional, pentru montaj |

### ⚡ Notă despre Baterii
Am folosit **2 baterii Li-Ion 18650 de 3.7V conectate în PARALEL** pentru a obține:
- Tensiune: 3.7V (compatibilă cu ESP32 prin pinul VIN)
- Capacitate dublă: ~4000-6000mAh
- Autonomie: ~8-12 ore de funcționare continuă

---

## 🔌 Schema Electrică

### Conexiuni Pin ESP32

```
┌─────────────────────────────────────────────────────┐
│                    ESP32 WROOM-32                    │
├─────────────────────────────────────────────────────┤
│                                                      │
│  OLED SSD1327 (SPI):                                │
│    VCC  ────────── 3.3V                             │
│    GND  ────────── GND                              │
│    DIN  ────────── GPIO23 (MOSI)                    │
│    CLK  ────────── GPIO18 (SCLK)                    │
│    CS   ────────── GPIO5                            │
│    DC   ────────── GPIO16                           │
│    RST  ────────── GPIO4                            │
│                                                      │
│  MPU-6050 (I2C):                                    │
│    VCC  ────────── 3.3V                             │
│    GND  ────────── GND                              │
│    SDA  ────────── GPIO21                           │
│    SCL  ────────── GPIO22                           │
│                                                      │
│  LDR (Voltage Divider):                             │
│    LDR  ────────── 3.3V                             │
│    LDR  ────┬───── GPIO34 (ADC)                     │
│             │                                        │
│    10kΩ ────┴───── GND                              │
│                                                      │
│  Piezo Buzzer:                                      │
│    GPIO25 ──[220Ω]── Buzzer (+)                     │
│    GND  ────────── Buzzer (-)                       │
│                                                      │
│  Alimentare (Baterii):                              │
│    Baterii 3.7V (paralel) ─── TP4056 ─── VIN + GND │
│                                                      │
└─────────────────────────────────────────────────────┘
```

### Diagramă Baterii Paralel

```
    ┌─────────────┐     ┌─────────────┐
    │  Baterie 1  │     │  Baterie 2  │
    │    3.7V     │     │    3.7V     │
    │  18650      │     │  18650      │
    └──────┬──────┘     └──────┬──────┘
           │ (+)               │ (+)
           └─────────┬─────────┘
                     │
                     ▼
              ┌──────────────┐
              │   TP4056     │
              │  (Charger)   │
              └──────┬───────┘
                     │
                     ▼
              ┌──────────────┐
              │  ESP32 VIN   │
              └──────────────┘
```

---

## 🔧 Provocări Tehnice

### 1. Sincronizarea Web ↔ ESP32
Una dintre cele mai dificile provocări a fost **sincronizarea în timp real** între interfața web și ESP32:

- **WebSocket bidirectional**: Am implementat comunicare în timp real folosind WebSockets. Fiecare acțiune din web (adaugă task, start timer) se reflectă instant pe OLED și invers.
- **Race conditions**: Când mai mulți clienți se conectează simultan, trebuie gestionate corect actualizările de stare.
- **Reconnect logic**: Dacă conexiunea WebSocket cade, clientul web încearcă automat reconectarea.

### 2. Probleme cu WiFi-ul ESP32
ESP32-ul are particularități cu WiFi-ul care au cauzat multe bătăi de cap:

- **Dual-core conflicts**: WebSocket-ul rulează pe Core 0, iar logica principală pe Core 1. A fost necesar să folosesc `mutex` și `volatile` pentru variabilele partajate.
- **Memory fragmentation**: După multe conexiuni/deconectări, heap-ul se fragmenta. Am optimizat folosind buffere statice.
- **Access Point fallback**: Dacă WiFi-ul configurat nu e disponibil, ESP32-ul creează propriul Access Point cu QR code pe OLED.

### 3. Partea Fizică - Construcția Cubului
Cea mai mare provocare non-software a fost **integrarea fizică**:

- **Toate componentele în cub**: Baterii, ESP32, OLED, MPU-6050, LDR, buzzer - totul trebuia să încapă într-un cub de ~10cm.
- **Bateria în centru de greutate**: Pentru ca flip-ul să funcționeze corect, bateriile (cele mai grele) trebuiau poziționate central.
- **Fixarea componentelor**: Am folosit bandă dublu-adezivă, hot glue și suporturi printate 3D pentru a ține totul fix când cubul se întoarce.
- **Gestionarea firelor**: Cu atâtea conexiuni, firele deveneau un haos. Am folosit fire scurte și organizare pe niveluri.
- **Accesul la port USB**: TP4056 și ESP32 trebuiau poziționate pentru acces ușor la încărcare/programare.

### 4. Rotirea Display-ului
OLED-ul trebuia să fie citibil indiferent de orientarea cubului:

- Am încercat `setDisplayRotation()` la runtime, dar nu funcționa consistent pentru SSD1327.
- Soluția finală: rotația se setează în **constructor** (`U8G2_R2`) și se schimbă dinamic când MPU-ul detectează flip.

### 5. Persistența Datelor
Task-urile și starea plantei trebuiau să supraviețuiască restart-ului:

- Am folosit **NVS (Non-Volatile Storage)** pentru starea plantei și statistici.
- Task-urile se salvează în format binar optimizat.
- La miezul nopții se face reset automat cu backup al statisticilor.

---

## 📁 Structura Codului

```
productivity-bloom/
├── finall.ino              # Entry point, setup() și loop()
├── config.h                # Configurări WiFi, pini, constante
│
├── SystemState.h           # Starea globală: moduri, task-uri, plantă
├── EventQueue.h            # Coadă de evenimente thread-safe
│
├── WebServerHandler.h      # Server HTTP + WebSocket
├── MultiCoreWebServer.h    # Wrapper dual-core pentru server
├── WebContent.h            # HTML/CSS/JS compilat (generat automat)
│
├── DisplayRenderer.h       # Toate funcțiile de desenare OLED
├── QRCodeGenerator.h       # Generare QR code pentru AP mode
│
├── MPU6050Handler.h        # Detectare flip cu accelerometru
├── BuzzerHandler.h         # Melodii și sunete
├── Analytics.h             # Statistici și NTP
│
├── IntervalTimer.h         # Timere non-blocking
├── TimedScreenManager.h    # Manager pentru overlay-uri temporare
│
├── build_webcontent.py     # Script Python pentru compilare web
│
└── data/                   # Fișiere web (sursă)
    ├── index.html          # Structura paginii
    ├── style.css           # Stiluri (mobile-first)
    └── app.js              # Logica JavaScript + WebSocket
```

### Arhitectura Event-Driven

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Sensors   │────▶│ EventQueue  │────▶│   Handler   │
│ MPU, LDR    │     │  (FIFO)     │     │  (loop())   │
└─────────────┘     └─────────────┘     └─────────────┘
                           ▲
                           │
                    ┌──────┴──────┐
                    │  WebSocket  │
                    │  (Core 0)   │
                    └─────────────┘
```

---

## 🚀 Instalare și Configurare

### Cerințe
- [Arduino IDE](https://www.arduino.cc/en/software) 2.0+ sau [PlatformIO](https://platformio.org/)
- ESP32 Board Package
- Biblioteci necesare (se instalează automat):
  - `U8g2` - Display OLED
  - `WebSockets` - Comunicare WebSocket
  - `ArduinoJson` - Parsare JSON
  - `qrcode` - Generare QR code

### Pași de Instalare

1. **Clonează repository-ul**
   ```bash
   git clone https://github.com/ale0204/FinalProject.git
   cd FinalProject
   ```

2. **Configurează WiFi** - editează `config.h`:
   ```cpp
   #define WIFI_SSID "NumeleReteleiTale"
   #define WIFI_PASSWORD "ParolaTa"
   ```

3. **Generează WebContent.h** (dacă modifici fișierele din `data/`):
   ```bash
   python build_webcontent.py
   ```

4. **Încarcă pe ESP32**:
   - Selectează Board: `ESP32 Dev Module`
   - Selectează Port: `COMx` (Windows) sau `/dev/ttyUSBx` (Linux)
   - Click Upload

5. **Găsește IP-ul**:
   - Deschide Serial Monitor (115200 baud)
   - Vei vedea: `Access web interface at: http://192.168.x.x`

---

## 🎯 Utilizare

### Prima Pornire
1. ESP32-ul încearcă să se conecteze la WiFi-ul configurat
2. Dacă reușește, afișează IP-ul pe OLED și în Serial
3. Dacă nu, creează Access Point "ProductivityBloom" cu QR code

### Flux de Lucru Tipic

1. **Accesează interfața web** de pe telefon/PC
2. **Adaugă task-uri** cu numele și durata dorită
3. **Selectează un task** din listă
4. **Întoarce cubul** (OLED în jos) pentru a porni timer-ul
5. **Concentrează-te** până auzi beep-urile de countdown
6. **Întoarce cubul înapoi** când termini sau vrei pauză
7. **Marchează task-ul complet** (✓) sau continuă (✗)

### Control prin Flip
| Poziție Cub | Acțiune |
|-------------|---------|
| OLED în sus | Idle / Pauză |
| OLED în jos | Focus mode (timer merge) |

### Revive Plantă
Când planta e ofilită:
1. Expune senzorul LDR la lumină puternică
2. Ține 3 secunde
3. Planta revine la stadiul Seed

---

## 📡 API Documentation

### REST Endpoints

| Endpoint | Metodă | Descriere |
|----------|--------|-----------|
| `/` | GET | Pagina web principală |
| `/api/status` | GET | Stare curentă (mode, timer, plant) |
| `/api/tasks` | GET | Lista task-urilor |
| `/api/tasks` | POST | Adaugă task nou |
| `/api/plant` | GET | Starea plantei |
| `/api/analytics` | GET | Statistici zilnice |
| `/api/action` | POST | Acțiuni: start, pause, resume, complete, skip, kill |

### WebSocket Events

**Server → Client:**
```javascript
{ "type": "status", "mode": "focusing", "timeLeft": 1423, "totalTime": 1500 }
{ "type": "plant", "stage": 2, "isWithered": false, "wateredCount": 3 }
{ "type": "tasks", "tasks": [...] }
{ "type": "flipConfirm" }  // Arată modal de confirmare
{ "type": "flipResumed" }  // Timer-ul a fost reluat
{ "type": "revive" }       // Planta a fost reînviată
```

**Client → Server:**
```javascript
{ "action": "addTask", "task": { "name": "Study", "focusDuration": 25, "breakDuration": 5 } }
{ "action": "startTask", "taskId": 123456 }
{ "action": "confirmComplete" }
{ "action": "confirmAccidental" }
```

---

## 🙏 Credite

Proiect realizat pentru cursul de **Robotică** - Facultatea de Automatică și Calculatoare, Anul 3.

### Biblioteci Utilizate
- [U8g2](https://github.com/olikraus/u8g2) - Display OLED
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) - WebSocket
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [QRCode](https://github.com/ricmoo/QRCode) - QR code generation

---

## 📄 Licență

MIT License - folosește liber acest proiect!

---

**Made with 💚 and lots of ☕**
