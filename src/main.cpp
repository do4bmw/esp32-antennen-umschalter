#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// WiFi-Zugangsdaten kommen aus secrets.ini via Build-Flags
#ifndef WIFI_SSID
  #define WIFI_SSID     "SSID_FEHLT"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "PASSWORT_FEHLT"
#endif

// ── Hardware ──────────────────────────────────────────────────
static const uint8_t RELAY_PINS[4] = {32, 33, 25, 26};
static const uint8_t LED_WIFI_PIN   = 23;

// ── Globaler Zustand ──────────────────────────────────────────
volatile int8_t activeAntenna = 0;
char antNames[4][21];   // 4 Namen, max. 20 Zeichen + '\0'

Preferences prefs;
WebServer server(80);

// ── PROGMEM: Hauptseite ───────────────────────────────────────
static const char HTML_MAIN[] PROGMEM =
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
    ".cfg{display:block;margin-top:18px;color:#666;font-size:.85em;text-decoration:none}"
    ".cfg:hover{color:#4af}"
    "</style></head><body>"
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
    "<p>Aktiv: <b id='s'>&#8230;</b></p>"
    "<div class='btns'>"
    "<button class='b' id='b1' onclick='sw(1)'>&#8230;</button>"
    "<button class='b' id='b2' onclick='sw(2)'>&#8230;</button>"
    "<button class='b' id='b3' onclick='sw(3)'>&#8230;</button>"
    "<button class='b' id='b4' onclick='sw(4)'>&#8230;</button>"
    "<button class='b off' id='b0' onclick='sw(0)'>AUS</button>"
    "</div>"
    "<a class='cfg' href='/settings'>&#9881; Einstellungen</a>"
    "<script>"
    "function sw(n){"
      "fetch('/switch?ant='+n).then(r=>r.json()).then(d=>upd(d.ant));"
    "}"
    "function upd(n){"
      "var b=document.getElementById('b'+n);"
      "document.getElementById('s').textContent=n===0?'AUS':(b?b.textContent:'?');"
      "for(var i=0;i<=4;i++){"
        "var btn=document.getElementById('b'+i);"
        "if(btn)btn.className=(i===0?'b off':'b')+(i===n?' a':'');"
      "}"
    "}"
    "fetch('/state').then(r=>r.json()).then(function(d){"
      "d.n.forEach(function(nm,i){document.getElementById('b'+(i+1)).textContent=nm;});"
      "upd(d.ant);"
    "});"
    "</script>"
    "</body></html>";

// ── PROGMEM: Einstellungsseite ────────────────────────────────
static const char HTML_SETTINGS[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Einstellungen</title>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;background:#111;color:#eee;padding:20px;margin:0}"
    "h1{color:#4af;margin-bottom:20px}"
    ".row{margin:10px auto;width:220px;text-align:left}"
    "label{display:block;margin-bottom:4px;color:#aaa;font-size:.9em}"
    "input{width:100%;box-sizing:border-box;padding:10px;border:2px solid #4af;"
    "border-radius:8px;background:#222;color:#eee;font-size:1em}"
    ".btn{margin-top:20px;width:220px;padding:14px;background:#4af;color:#000;"
    "border:none;border-radius:8px;font-size:1.1em;cursor:pointer;font-weight:bold}"
    ".btn:hover{background:#7cf}"
    ".back{display:block;margin-top:16px;color:#4af;text-decoration:none}"
    "#msg{margin-top:12px;min-height:1.2em;color:#4f4}"
    "</style></head><body>"
    "<h1>&#9881; Einstellungen</h1>"
    "<div class='row'><label>Antenne 1</label><input id='n1' maxlength='20'></div>"
    "<div class='row'><label>Antenne 2</label><input id='n2' maxlength='20'></div>"
    "<div class='row'><label>Antenne 3</label><input id='n3' maxlength='20'></div>"
    "<div class='row'><label>Antenne 4</label><input id='n4' maxlength='20'></div>"
    "<button class='btn' onclick='save()'>Speichern</button>"
    "<div id='msg'></div>"
    "<a class='back' href='/'>&#8592; Zurück</a>"
    "<script>"
    "fetch('/state').then(r=>r.json()).then(function(d){"
      "d.n.forEach(function(nm,i){document.getElementById('n'+(i+1)).value=nm;});"
    "});"
    "function save(){"
      "var b='n1='+encodeURIComponent(document.getElementById('n1').value)"
          "+'&n2='+encodeURIComponent(document.getElementById('n2').value)"
          "+'&n3='+encodeURIComponent(document.getElementById('n3').value)"
          "+'&n4='+encodeURIComponent(document.getElementById('n4').value);"
      "fetch('/settings',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})"
      ".then(r=>r.json()).then(function(d){"
        "document.getElementById('msg').textContent=d.ok?'\\u2713 Gespeichert!':'Fehler';"
      "});"
    "}"
    "</script>"
    "</body></html>";

// ── NVS: Namen laden (oder Defaults setzen) ───────────────────
static void loadNames() {
    for (int i = 0; i < 4; i++) {
        char key[3] = {'n', (char)('1' + i), '\0'};
        char def[21];
        snprintf(def, sizeof(def), "Antenne %d", i + 1);
        prefs.getString(key, antNames[i], sizeof(antNames[i]));
        if (antNames[i][0] == '\0') {
            strlcpy(antNames[i], def, sizeof(antNames[i]));
        }
    }
}

// ── NVS: Namen speichern ──────────────────────────────────────
static void saveNames() {
    for (int i = 0; i < 4; i++) {
        char key[3] = {'n', (char)('1' + i), '\0'};
        prefs.putString(key, antNames[i]);
    }
}

// ── Relais-Steuerung ──────────────────────────────────────────
static void applyRelay(int8_t ant) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(RELAY_PINS[i], (ant == i + 1) ? HIGH : LOW);
    }
}

static void setAntenna(int8_t ant) {
    applyRelay(ant);
    activeAntenna = ant;
    prefs.putChar("active", ant);   // dauerhaft speichern
    if (ant == 0) {
        Serial.println("[Relais] AUS – alle Relais offen");
    } else {
        Serial.printf("[Relais] %s aktiv (GPIO %d HIGH)\n",
                      antNames[ant - 1], RELAY_PINS[ant - 1]);
    }
}

// ── HTTP-Handler: Hauptseite ──────────────────────────────────
void handleRoot() {
    server.send_P(200, "text/html; charset=utf-8", HTML_MAIN);
}

// ── HTTP-Handler: Zustand + Namen ────────────────────────────
void handleState() {
    char buf[140];
    snprintf(buf, sizeof(buf),
        "{\"ant\":%d,\"n\":[\"%s\",\"%s\",\"%s\",\"%s\"]}",
        activeAntenna,
        antNames[0], antNames[1], antNames[2], antNames[3]);
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

// ── HTTP-Handler: Einstellungsseite (GET) ─────────────────────
void handleSettings() {
    server.send_P(200, "text/html; charset=utf-8", HTML_SETTINGS);
}

// ── HTTP-Handler: Einstellungen speichern (POST) ──────────────
void handleSettingsSave() {
    bool changed = false;
    for (int i = 0; i < 4; i++) {
        char key[3] = {'n', (char)('1' + i), '\0'};
        if (server.hasArg(key)) {
            String val = server.arg(key);
            val.trim();
            if (val.length() == 0) {
                snprintf(antNames[i], sizeof(antNames[i]), "Antenne %d", i + 1);
            } else {
                strlcpy(antNames[i], val.c_str(), sizeof(antNames[i]));
            }
            changed = true;
        }
    }
    if (changed) saveNames();
    server.send(200, "application/json", "{\"ok\":true}");
}

// ── FreeRTOS-Task: WiFi-Status-LED ────────────────────────────
static void wifiLedTask(void* /*param*/) {
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            digitalWrite(LED_WIFI_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            digitalWrite(LED_WIFI_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(500));
            digitalWrite(LED_WIFI_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// ── FreeRTOS-Task: Relais-Watchdog alle 200 ms ────────────────
static void relayTask(void* /*param*/) {
    for (;;) {
        applyRelay(activeAntenna);
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

    // NVS öffnen und gespeicherte Werte laden
    prefs.begin("ant", false);
    loadNames();
    int8_t lastAnt = prefs.getChar("active", 1);
    Serial.printf("[NVS] Letzte Antenne: %d\n", lastAnt);
    for (int i = 0; i < 4; i++) {
        Serial.printf("[NVS] Name %d: %s\n", i + 1, antNames[i]);
    }

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

    // Letzte Antenne wiederherstellen
    applyRelay(lastAnt);
    activeAntenna = lastAnt;
    Serial.printf("[Relais] Wiederhergestellt: %s\n",
                  lastAnt == 0 ? "AUS" : antNames[lastAnt - 1]);

    // HTTP-Routen registrieren und Server starten
    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/state",   HTTP_GET,  handleState);
    server.on("/switch",  HTTP_GET,  handleSwitch);
    server.on("/settings",HTTP_GET,  handleSettings);
    server.on("/settings",HTTP_POST, handleSettingsSave);
    server.begin();
    Serial.println("HTTP-Server gestartet");

    xTaskCreatePinnedToCore(relayTask,   "relayTask",   2048, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(wifiLedTask, "wifiLedTask", 2048, nullptr, 1, nullptr, 1);
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    server.handleClient();
}
