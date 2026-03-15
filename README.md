# ESP32 4-Fach Antennen-Umschalter

Webbasierter Antennen-Umschalter für vier Antennen auf Basis eines ESP32.
Steuerung über jeden Browser im lokalen Netzwerk – optimiert für Desktop und Mobilgeräte (iOS/Android).

---

## Funktionsweise

- Vier Relais (active HIGH) schalten jeweils eine Antenne aktiv
- Immer nur ein Relais gleichzeitig aktiv, alle anderen offen
- Weboberfläche erreichbar per IP oder mDNS-Name (Standard: **http://do4bmw-ant.local**)
- Buttons reagieren sofort per AJAX – kein Seiten-Reload, kein Lag auf iOS
- Relais-Watchdog (FreeRTOS-Task) stellt den Zustand alle 200 ms erneut sicher
- WiFi-Status-LED auf GPIO23: blinkt (500 ms) = sucht WLAN, dauerhaft an = verbunden

### GPIO-Belegung

| Funktion   | GPIO |
|------------|------|
| Relais 1   | 32   |
| Relais 2   | 33   |
| Relais 3   | 25   |
| Relais 4   | 26   |
| WiFi-LED   | 23   |

---

## Installation

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension oder CLI)
- ESP32 Dev Board – [exaktes Board bei Amazon bestellen](https://amzn.to/4uww4y2)

### Repository klonen

```bash
git clone https://github.com/do4bmw/esp32-antennen-umschalter.git
cd esp32-antennen-umschalter
```

### WiFi-Zugangsdaten einrichten

Die WLAN-Zugangsdaten werden **nicht** im Repository gespeichert.
Vorlage kopieren und ausfüllen:

```bash
cp secrets.ini.example secrets.ini
```

`secrets.ini` bearbeiten:

```ini
[env]
build_flags =
    -DWIFI_SSID=\"Dein-WLAN-Name\"
    -DWIFI_PASSWORD=\"Dein-WLAN-Passwort\"
```

> `secrets.ini` ist in `.gitignore` eingetragen und wird nie eingecheckt.

### Flashen

```bash
pio run --target upload
```

---

## Weboberfläche

### Hauptseite – Antennen schalten

Aufruf: `http://do4bmw-ant.local` oder per IP-Adresse (wird beim Start seriell ausgegeben).

Vier Buttons schalten die Antennen, ein AUS-Button öffnet alle Relais.
Der aktiv geschaltete Button wird farblich hervorgehoben.

### Einstellungen – Antennennamen und mDNS anpassen

Aufruf: `http://do4bmw-ant.local/settings`

Die Namen der vier Antennentasten können frei vergeben werden (max. 20 Zeichen), z.B.:

- Antenne 1 → `Yagi 40m`
- Antenne 2 → `Dipol 80m`
- Antenne 3 → `Vertikal`
- Antenne 4 → `Beam 10m`

Zusätzlich kann der **mDNS-Hostname** (max. 32 Zeichen, nur Buchstaben, Zahlen und Bindestriche) frei vergeben werden.
Das Board ist danach sofort unter dem neuen Namen erreichbar – ohne Neustart.

> Beispiel: Name `mein-esp` → Adresse `http://mein-esp.local`

Nach dem Speichern werden alle Einstellungen dauerhaft im **NVS (Non-Volatile Storage)** des ESP32 abgelegt und beim nächsten Start automatisch wiederhergestellt.

---

## Dauerhafte Speicherung (NVS)

Der ESP32 speichert folgende Daten dauerhaft im internen Flash (NVS):

| Gespeicherter Wert       | Beschreibung                                      |
|--------------------------|---------------------------------------------------|
| Antennennamen (n1–n4)    | Frei vergebene Button-Bezeichnungen                       |
| mDNS-Hostname            | Frei wählbarer `.local`-Name im Netzwerk                  |
| Zuletzt aktives Relais   | Nach Neustart wird automatisch dasselbe Relais geschaltet |

Das bedeutet: Fällt die Stromversorgung aus oder wird der ESP32 neu gestartet, schaltet er beim Hochfahren sofort wieder auf die zuletzt aktive Antenne – ohne manuellen Eingriff.

Beim Start wird der wiederhergestellte Zustand seriell ausgegeben (115200 Baud):

```
[NVS] Letzte Antenne: 2
[NVS] Name 1: Yagi 40m
[NVS] Name 2: Dipol 80m
[NVS] Name 3: Vertikal
[NVS] Name 4: Beam 10m
[NVS] mDNS-Name: do4bmw-ant
[Relais] Dipol 80m aktiv (GPIO 33 HIGH)
```

---

## mDNS

Das Board meldet sich im Netzwerk standardmäßig unter **do4bmw-ant.local**.
Der Name kann jederzeit über die Einstellungsseite (`/settings`) geändert und dauerhaft gespeichert werden – die Änderung ist sofort aktiv, kein Neustart nötig.

Funktioniert direkt unter macOS und iOS. Unter Windows 10/11 wird der mDNS-Dienst von modernen Browsern (Edge, Chrome) nativ unterstützt.

---

## Lizenz

MIT License – Copyright (c) 2026 do4bmw
Freie Nutzung, Änderung und Weitergabe erlaubt, solange der Copyright-Hinweis erhalten bleibt.
Siehe [LICENSE](LICENSE).
