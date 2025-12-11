#include "JKBMSInterface.h"

// ---------------- MAX7219 ----------------
const int DIN_PIN = 23;   // MAX7219 DIN
const int CLK_PIN = 18;   // MAX7219 CLK
const int CS_PIN  = 5;    // MAX7219 CS / LOAD

// Create BMS instance using Serial2
JKBMSInterface bms(&Serial2);

// Останнє відоме значення SOC, щоб було що показувати
int lastSoc = -1;
bool summaryPrinted = false;
int frameCounter = 0;

// ---------------- MAX7219 low-level ----------------

void max7219Send(uint8_t reg, uint8_t data)
{
  digitalWrite(CS_PIN, LOW);
  shiftOut(DIN_PIN, CLK_PIN, MSBFIRST, reg);
  shiftOut(DIN_PIN, CLK_PIN, MSBFIRST, data);
  digitalWrite(CS_PIN, HIGH);
}

void max7219Init()
{
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);

  // Вимкнути тестовий режим
  max7219Send(0x0F, 0x00);

  // Вийти з shutdown (увімкнути дисплей)
  max7219Send(0x0C, 0x01);

  // Сканувати всі 8 цифр (0..7)
  max7219Send(0x0B, 0x07);

  // Увімкнути BCD Code-B decode для всіх 8 цифр
  // 0xFF = для всіх digits 0–7; 0xF7 = всі, крім 4-го справа
  max7219Send(0x09, 0xF7);

  // Яскравість (0..15)
  max7219Send(0x0A, 0x08);

  // Почистити всі цифри (0x0F у Code-B = blank)
  for (uint8_t d = 1; d <= 8; d++)
  {
    max7219Send(d, 0x0F);
  }
}

// показати тире "---" (немає даних)
void displayDash()
{
  for (uint8_t d = 5; d <= 8; d++)
    max7219Send(d, 0x0F);   // blank
  max7219Send(4, 0x00);
  
  max7219Send(1, 0x0A); // '-' на молодшому розряді
  max7219Send(2, 0x0A);
  max7219Send(3, 0x0A);
}

// Показати число 0..100 на правих трьох цифрах
// digit 1 = правий крайній, 8 = лівий крайній
void displaySoc(int soc)
{
  if (soc < 0)   soc = 0;
  if (soc > 100) soc = 100;

  int ones     = soc % 10;
  int tens     = (soc / 10) % 10;
  int hundreds = soc / 100;

  // Молодший розряд (одиниці) – digit 1
  max7219Send(1, ones);

  // Десятки – digit 2 (якщо є)
  if (soc >= 10)
    max7219Send(2, tens);
  else
    max7219Send(2, 0x0F); // blank

  // Сотні – digit 3 (тільки для 100)
  if (soc >= 100)
    max7219Send(3, hundreds);
  else
    max7219Send(3, 0x0F); // blank
}

// Показати число з плаваючою точкою на лівих 4-х розрядах дисплея
void displayFloat(float value)
{
  // Обмежимо діапазон -999.9..999.9
  if (value < -999.9) value = -999.9;
  if (value > 999.9) value = 999.9;

  // -------------------------------
  // Розміщення на дисплеї:
  // digits 8 7 6 5
  //        - 9 9 9
  //        - 9 9.9
  //        9 9.9 9
  //        - 9.9 9
  //          9.9 9
  // -------------------------------

  int position = ((value > 0.0) && (value < 10.0)) ? 7 : 8;

  if (position == 7) {
    max7219Send(8, 0x0F);  // blank
  }
  
  if (value < 0.0) {
    value = -value;
    max7219Send(position, 0x0A); // '-'
    position--;
  }
  
  // Розбиваємо на складові
  int whole = (int)value;                   // Ціла частина, наприклад 153

  int d_hundreds = (whole / 100) % 10;      // сотні   (1)
  int d_tens = (whole / 10) % 10;           // десятки (5)
  int d_ones = whole % 10;                  // одиниці (3)

  if (d_hundreds > 0) {
    max7219Send(position, d_hundreds);
    position--;
  }

  if (d_tens > 0) {
    max7219Send(position, d_tens);
    position--;
  }

  // одиниці + крапка, якщо не крайній розряд
  max7219Send(position, d_ones | ((position == 5) ? 0x00 : 0x80));
  position--;

  // десяті
  if (position > 4) {
    int tens  = (int)(value * 10) % 10;
    max7219Send(position, tens);
    position--;
  }
  
  // соті
  if (position > 4) {
    int hund = (int)(value * 100) % 10;
    max7219Send(position, hund);
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println("JK-BMS Example Starting...");

    // ---------- MAX7219 -----------
    max7219Init();

    // Показати щось на старті
    displayDash();
    
    // Initialize BMS communication (RX=16, TX=17 for ESP32 Serial2 Port)
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    bms.begin(115200);
    
    Serial.println("JK-BMS Example Started");
}

void loop() {
    // Update BMS data (call this regularly)
    bms.update();
    
    // Check if we have valid data
    if (bms.isDataValid()) {
        if (!summaryPrinted) {
            bms.printSummary();
            bms.printRawData();

          summaryPrinted = true;
        }

        // Access battery data
        float voltage = bms.getVoltage();
        uint8_t soc = bms.getSOC();
        float current = bms.getCurrent();
        
        Serial.print("Voltage: ");
        Serial.print(voltage, 2);
        Serial.print("V, SOC: ");
        Serial.print(soc);
        Serial.print("%, Current: ");
        Serial.print(current, 2);
        Serial.println("A");

        displaySoc(soc);

        // Циклічне перемикання між відображенням струму та напруги
        // кожні 3 секунди
        if (frameCounter < 3) {
          displayFloat(voltage);
          max7219Send(4, 0x1C); // 'V'
        } else {
          displayFloat(-current);
          max7219Send(4, 0x77); // 'A'
        }
        if (++frameCounter > 6) frameCounter = 0;
        
    } else {
        displayDash();
        Serial.println("Received data is not valid");
        delay(4000);
    }
    
    delay(1000);
}
