#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// WiFi-Zugangsdaten kommen aus secrets.ini via Build-Flags
#ifndef WIFI_SSID
  #define WIFI_SSID     "SSID_FEHLT"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "PASSWORT_FEHLT"
#endif

// ── Hardware ──────────────────────────────────────────────────
// GPIO-Pins der Relais (active HIGH laut Datenblatt RBS16467)
static const uint8_t RELAY_PINS[4] = {32, 33, 25, 26};
static const uint8_t LED_WIFI_PIN   = 23;

// ── Globaler Zustand ──────────────────────────────────────────
volatile int8_t activeAntenna = 0; // 0 = AUS, 1–4 = Antenne

WebServer server(80);

// ── PROGMEM HTML (statisch, Zustand via JS/fetch) ─────────────
static const char HTML_PAGE[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Antennen-Umschalter</title>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;background:#111;color:#eee;padding:20px;margin:0}"
    "h1{color:#4af;margin:8px 0 2px;font-size:1.4em}"
    "p{margin:4px 0 18px;font-size:1em}"
    ".btns{display:flex;flex-direction:column;align-items:center;gap:10px;margin:10px 0}"
    ".b{width:220px;padding:16px;border:2px solid #4af;border-radius:10px;"
    "font-size:1.1em;cursor:pointer;color:#eee;background:#222;transition:background .15s}"
    ".b:hover{background:#4af;color:#000}"
    ".a{background:#4af!important;color:#000!important;font-weight:bold}"
    ".off{border-color:#888;margin-top:8px}"
    ".off.a{background:#888!important}"
    "</style></head><body>"
    // Antennensymbol (inline SVG) + Titel
    "<svg width='60' height='52' viewBox='0 0 60 52' fill='none' xmlns='http://www.w3.org/2000/svg'>"
    "<line x1='30' y1='51' x2='30' y2='30' stroke='#4af' stroke-width='3' stroke-linecap='round'/>"
    "<line x1='6' y1='30' x2='54' y2='30' stroke='#4af' stroke-width='3' stroke-linecap='round'/>"
    "<line x1='16' y1='30' x2='30' y2='51' stroke='#4af' stroke-width='2' stroke-linecap='round'/>"
    "<line x1='44' y1='30' x2='30' y2='51' stroke='#4af' stroke-width='2' stroke-linecap='round'/>"
    "<path d='M22 22 Q30 13 38 22' stroke='#4af' stroke-width='2' fill='none' stroke-linecap='round'/>"
    "<path d='M15 15 Q30 3 45 15' stroke='#4af' stroke-width='2' fill='none' stroke-linecap='round'/>"
    "<path d='M8 8 Q30 -7 52 8' stroke='#4af' stroke-width='1.5' fill='none' stroke-linecap='round'/>"
    "</svg>"
    "<h1>Antennen-Umschalter</h1>"
    "<p>Aktiv: <b id='s'>…</b></p>"
    "<div class='btns'>"
    "<button class='b' id='b1' onclick='sw(1)'>Antenne 1</button>"
    "<button class='b' id='b2' onclick='sw(2)'>Antenne 2</button>"
    "<button class='b' id='b3' onclick='sw(3)'>Antenne 3</button>"
    "<button class='b' id='b4' onclick='sw(4)'>Antenne 4</button>"
    "<button class='b off' id='b0' onclick='sw(0)'>AUS</button>"
    "</div>"
    "<script>"
    "function sw(n){"
      "fetch('/switch?ant='+n).then(r=>r.json()).then(d=>upd(d.ant));"
    "}"
    "function upd(n){"
      "document.getElementById('s').textContent=n===0?'AUS':'Antenne '+n;"
      "for(var i=0;i<=4;i++){"
        "var b=document.getElementById('b'+i);"
        "if(b)b.className=(i===0?'b off':'b')+(i===n?' a':'');"
      "}"
    "}"
    "fetch('/state').then(r=>r.json()).then(d=>upd(d.ant));"
    "</script>"
    "</body></html>";

// ── Relais-Steuerung ──────────────────────────────────────────
static void applyRelay(int8_t ant) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(RELAY_PINS[i], (ant == i + 1) ? HIGH : LOW);
    }
}

static void setAntenna(int8_t ant) {
    applyRelay(ant);          // erst schalten …
    activeAntenna = ant;      // … dann State aktualisieren
    if (ant == 0) {
        Serial.println("[Relais] AUS – alle Relais offen");
    } else {
        Serial.printf("[Relais] Antenne %d aktiv (GPIO %d HIGH)\n", ant, RELAY_PINS[ant - 1]);
    }
}

// ── HTTP-Handler: Startseite ──────────────────────────────────
void handleRoot() {
    server.send_P(200, "text/html; charset=utf-8", HTML_PAGE);
}

// ── HTTP-Handler: Zustand abfragen ────────────────────────────
void handleState() {
    char buf[12];
    snprintf(buf, sizeof(buf), "{\"ant\":%d}", activeAntenna);
    server.send(200, "application/json", buf);
}

// ── HTTP-Handler: Umschalten ──────────────────────────────────
void handleSwitch() {
    if (server.hasArg("ant")) {
        int val = server.arg("ant").toInt();
        if (val >= 0 && val <= 4) {
            setAntenna((int8_t)val);
        }
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "{\"ant\":%d}", activeAntenna);
    server.send(200, "application/json", buf);
}

// ── FreeRTOS-Task: WiFi-Status-LED ────────────────────────────
static void wifiLedTask(void* /*param*/) {
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            digitalWrite(LED_WIFI_PIN, HIGH);           // dauerhaft an
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            digitalWrite(LED_WIFI_PIN, HIGH);           // 500 ms an …
            vTaskDelay(pdMS_TO_TICKS(500));
            digitalWrite(LED_WIFI_PIN, LOW);            // … 500 ms aus
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// ── FreeRTOS-Task: Relais-Watchdog alle 200 ms ────────────────
static void relayTask(void* /*param*/) {
    for (;;) {
        applyRelay(activeAntenna);          // State erneut anlegen (Glitch-Schutz)
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Alle Relais als Ausgang, initial AUS
    for (int i = 0; i < 4; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
    }

    // WiFi-Status-LED
    pinMode(LED_WIFI_PIN, OUTPUT);
    digitalWrite(LED_WIFI_PIN, LOW);

    // WLAN verbinden (max. 30 s)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Verbinde mit WiFi");
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 30000UL) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin("do4bmw-ant")) {
            MDNS.addService("http", "tcp", 80);
            Serial.println("mDNS: http://do4bmw-ant.local");
        }
    } else {
        Serial.println("\nWiFi-Verbindung fehlgeschlagen – prüfe secrets.ini!");
    }

    // Standard: Antenne 1 aktiv
    setAntenna(1);

    // HTTP-Routen registrieren und Server starten
    server.on("/",       handleRoot);
    server.on("/state",  handleState);
    server.on("/switch", handleSwitch);
    server.begin();
    Serial.println("HTTP-Server gestartet");

    // Relay-Watchdog-Task auf Core 1 starten (Stack 2 KB reicht)
    xTaskCreatePinnedToCore(relayTask,   "relayTask",   2048, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(wifiLedTask, "wifiLedTask", 2048, nullptr, 1, nullptr, 1);
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    server.handleClient();   // nicht-blockierend, kein delay()
}
