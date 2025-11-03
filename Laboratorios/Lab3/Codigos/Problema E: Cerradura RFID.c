/* RFID RC522 + LCD I2C (hd44780) + EEPROM — pines según tu wiring */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <EEPROM.h>

// Reemplazo de LiquidCrystal_I2C por hd44780 
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
hd44780_I2Cexp lcd; // autodetecta PCF8574

//  Pines 
const uint8_t PIN_RFID_SS  = 10;
const uint8_t PIN_RFID_RST = 9;
const uint8_t PIN_LED_OK   = 6;
const uint8_t PIN_LED_NO   = 7;
const uint8_t PIN_BTN_ADD  = 2;
const uint8_t PIN_BTN_DEL  = 3;

//  RFID
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);

//  EEPROM
const int EEPROM_MAGIC_ADDR = 0;
const uint8_t MAGIC[4] = { 'R','F','I','D' };
const uint8_t MAX_SLOTS = 20;
const uint8_t SLOT_SIZE = 11;
const int SLOTS_START = 16;
const uint8_t EMPTY_SIZE = 0x00;

//  Botones / debounce 
struct Btn {
  uint8_t pin;
  bool lastStableHigh = true;
  bool lastReadHigh   = true;
  unsigned long lastChangeMs = 0;
  const unsigned long debounceMs = 30;
  bool pressedEdge = false;
  unsigned long pressedAtMs = 0;
};
Btn btnAdd { PIN_BTN_ADD };
Btn btnDel { PIN_BTN_DEL };

// Estado 
uint8_t lastUID[10];
uint8_t lastUIDLen = 0;
bool    lastUIDValid = false;

//  Helpers 
void lcdCenterLine(uint8_t row, const String &msg) {
  lcd.setCursor(0, row);
  String pad(16, ' ');
  int start = max(0, (16 - (int)msg.length())/2);
  lcd.print(pad);
  lcd.setCursor(start, row);
  lcd.print(msg.substring(0,16));
}

void showUIDLine(uint8_t row, const uint8_t *uid, uint8_t len) {
  String s = "";
  for (uint8_t i=0;i<len;i++) {
    if (i) s += ":";
    char b[4]; snprintf(b, sizeof(b), "%02X", uid[i]); s += b;
  }
  s = s.substring(0,16);
  lcd.setCursor(0,row);
  lcd.print("                ");
  lcd.setCursor(0,row);
  lcd.print(s);
}

void signalAccess(bool granted, unsigned long ms=800) {
  digitalWrite(granted ? PIN_LED_OK : PIN_LED_NO, HIGH);
  delay(ms);
  digitalWrite(granted ? PIN_LED_OK : PIN_LED_NO, LOW);
}

void eepromWriteByte(int addr, uint8_t v) { EEPROM.update(addr, v); }
uint8_t eepromReadByte(int addr) { return EEPROM.read(addr); }

void formatIfNeeded() {
  bool ok = true;
  for (int i=0;i<4;i++) if (eepromReadByte(EEPROM_MAGIC_ADDR+i) != MAGIC[i]) { ok=false; break; }
  if (!ok) {
    for (int i=0;i<4;i++) eepromWriteByte(EEPROM_MAGIC_ADDR+i, MAGIC[i]);
    for (int s=0; s<MAX_SLOTS; s++) {
      int base = SLOTS_START + s*SLOT_SIZE;
      eepromWriteByte(base, EMPTY_SIZE);
      for (int k=1;k<SLOT_SIZE;k++) eepromWriteByte(base+k, 0x00);
    }
  }
}

void readSlot(uint8_t s, uint8_t &len, uint8_t *buf) {
  int base = SLOTS_START + s*SLOT_SIZE;
  len = eepromReadByte(base);
  if (len == EMPTY_SIZE || len > 10) { len = 0; return; }
  for (uint8_t i=0;i<len;i++) buf[i] = eepromReadByte(base+1+i);
}

void writeSlot(uint8_t s, uint8_t len, const uint8_t *buf) {
  int base = SLOTS_START + s*SLOT_SIZE;
  if (len==0 || len>10) {
    eepromWriteByte(base, EMPTY_SIZE);
    for (int i=1;i<SLOT_SIZE;i++) eepromWriteByte(base+i, 0x00);
    return;
  }
  eepromWriteByte(base, len);
  for (uint8_t i=0;i<len;i++) eepromWriteByte(base+1+i, buf[i]);
  for (int i=1+len;i<SLOT_SIZE;i++) eepromWriteByte(base+i, 0x00);
}

int findUID(const uint8_t *uid, uint8_t len) {
  for (uint8_t s=0; s<MAX_SLOTS; s++) {
    uint8_t slen; uint8_t sbuf[10];
    readSlot(s, slen, sbuf);
    if (slen == len && slen>0) {
      bool eq=true;
      for (uint8_t i=0;i<len;i++) if (sbuf[i]!=uid[i]) { eq=false; break; }
      if (eq) return s;
    }
  }
  return -1;
}

bool addUID(const uint8_t *uid, uint8_t len) {
  if (findUID(uid, len) >= 0) return false;
  for (uint8_t s=0; s<MAX_SLOTS; s++) {
    uint8_t slen; uint8_t sbuf[10];
    readSlot(s, slen, sbuf);
    if (slen == 0) { writeSlot(s, len, uid); return true; }
  }
  return false;
}

bool removeUID(const uint8_t *uid, uint8_t len) {
  int idx = findUID(uid, len);
  if (idx < 0) return false;
  writeSlot(idx, 0, nullptr);
  return true;
}

void clearAll() { for (uint8_t s=0;s<MAX_SLOTS;s++) writeSlot(s, 0, nullptr); }
bool isAuthorized(const uint8_t *uid, uint8_t len) { return findUID(uid, len) >= 0; }

bool btnUpdate(Btn &b, bool &fallingEdge, bool &longPress2s) {
  bool rawHigh = digitalRead(b.pin);
  unsigned long now = millis();
  if (rawHigh != b.lastReadHigh) { b.lastReadHigh = rawHigh; b.lastChangeMs = now; }
  fallingEdge = false; longPress2s = false;

  if ((now - b.lastChangeMs) > b.debounceMs) {
    if (b.lastStableHigh != b.lastReadHigh) {
      b.lastStableHigh = b.lastReadHigh;
      if (!b.lastStableHigh) { b.pressedEdge = true; b.pressedAtMs = now; fallingEdge = true; }
      else {
        if (b.pressedEdge && (now - b.pressedAtMs) >= 2000) longPress2s = true;
        b.pressedEdge = false;
      }
    }
  }
  return !b.lastStableHigh;
}

void printShortStatus(const char *l1, const char *l2, unsigned long ms=1200) {
  lcd.clear(); lcdCenterLine(0, l1); lcdCenterLine(1, l2); delay(ms);
}

// ----------------- Setup -----------------
void setup() {
  pinMode(PIN_LED_OK, OUTPUT);
  pinMode(PIN_LED_NO, OUTPUT);
  digitalWrite(PIN_LED_OK, LOW);
  digitalWrite(PIN_LED_NO, LOW);

  pinMode(PIN_BTN_ADD, INPUT_PULLUP);
  pinMode(PIN_BTN_DEL, INPUT_PULLUP);

  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(100000);

  // hd44780: inicializa y autodetecta backpack I2C (PCF8574). 
  // Si devuelve status!=0 igual suele funcionar.
  (void)lcd.begin(16, 2);
  lcd.clear();
  lcdCenterLine(0, "Control RFID");
  lcdCenterLine(1, "Inicializando...");

  SPI.begin();
  rfid.PCD_Init();

  formatIfNeeded();

  delay(600);
  lcd.clear();
  lcdCenterLine(0, "Aproxime tarjeta");
  lcdCenterLine(1, "o pulse un boton");
}

//  Loop
void loop() {
  bool edgeAdd=false, longAdd=false;
  bool edgeDel=false, longDel=false;

  btnUpdate(btnAdd, edgeAdd, longAdd);
  btnUpdate(btnDel, edgeDel, longDel);

  if (edgeAdd) {
    if (lastUIDValid) {
      if (addUID(lastUID, lastUIDLen)) printShortStatus("Tarjeta agregada","OK");
      else if (isAuthorized(lastUID,lastUIDLen)) printShortStatus("Ya estaba","autorizada");
      else printShortStatus("Sin espacio","en memoria");
    } else printShortStatus("No hay UID","Lea una tarjeta");
  }

  static bool delWasPressed = false;
  bool delPressedNow = (digitalRead(PIN_BTN_DEL) == LOW);
  if (delWasPressed && !delPressedNow) {
    unsigned long now = millis();
    if ((now - btnDel.pressedAtMs) >= 2000) longDel = true;
  }
  delWasPressed = delPressedNow;

  if (longDel) { clearAll(); printShortStatus("Memoria","BORRADA"); }
  else if (edgeDel) {
    if (lastUIDValid) {
      if (removeUID(lastUID,lastUIDLen)) printShortStatus("Tarjeta","ELIMINADA");
      else printShortStatus("Tarjeta no","autorizada");
    } else printShortStatus("No hay UID","Lea una tarjeta");
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    lastUIDLen = rfid.uid.size; if (lastUIDLen > 10) lastUIDLen = 10;
    for (uint8_t i=0;i<lastUIDLen;i++) lastUID[i] = rfid.uid.uidByte[i];
    lastUIDValid = true;

    lcd.clear(); lcdCenterLine(0, "UID detectada"); showUIDLine(1, lastUID, lastUIDLen);
    bool ok = isAuthorized(lastUID, lastUIDLen); delay(500);

    lcd.clear();
    if (ok) { lcdCenterLine(0,"ACCESO"); lcdCenterLine(1,"AUTORIZADO"); signalAccess(true,800); }
    else    { lcdCenterLine(0,"ACCESO"); lcdCenterLine(1,"DENEGADO");   signalAccess(false,800); }

    digitalWrite(PIN_LED_OK, LOW); digitalWrite(PIN_LED_NO, LOW);
    delay(200);
    lcd.clear();
    lcdCenterLine(0, "Aproxime tarjeta");
    lcdCenterLine(1, "o pulse un boton");

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  delay(5);
}
