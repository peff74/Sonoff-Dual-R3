/***************************************************************************
  Gets Sensor data from the Sonoff Dual R3 v2 with the bl0939 Chip.
  Display them via Telnet
  
  Written by Stefan Meier based on examples taken from:
  Tasmota, ESPHome & https://github.com/RDobrinov/bl0940
 ***************************************************************************/

/*
   More information
   BL0939 Notes_V1.1_cn.pdf
   BL0939_V1.2_cn.pdf


   How to calculate Voltage, Current , Active Power
   Vref=1.218V   |   R1=0.51kOhm   |   R2=5*390kOhm   |   RL=1mOhm

  Current(A)  = I_RMS_Reg*Vref/324004*RL
              = I_RMS_Reg / ((324004*RL)/Vref)
              = I_RMS_Reg / ((324004*1)/1,218)
              = I_RMS_Reg / 266013,14

   Voltage(V)  = V_RMS_Reg*Vref*(R2+R1)/79931*R1*1000
               = V_RMS_Reg / ((79931*R1*1000)/(1,218*(R2+R1)))
               = V_RMS_Reg / ((79931*0,51*1000)/(1,218*(5*390)+0,51))
               = V_RMS_Reg / 17158,92

   Power (W)   = WATT_Reg*Vref²*(R2+R1)/4046*RL*R1*1000
               = WATT_Reg / ((4046*RL*R1*1000)/(Vref²*(R2+R1))
               = WATT_Reg / ((4046*1*0,51*1000)/((1,218*1,218)*((5*390)+0,51)
               = WATT_Reg / 713,105

   Engergy(kWh)= CF*((1638,4*256*Vref²*(R2+R1))/(3600000*4046*RL*R1*1000))
               = CF*((1638,4*256*(1,218*1,218)*((5*390)+0,51))/(3600000*4046*1*R0,51*1000))
               = CF * 0,0001633819620262585

   TEMP (°C)   = (170/448)*(TPS1/2-32)-45
               = ((85TSP1-5440)/448)-45



  One full measurement frame from bl0939
  Byte:  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34
  Data:  55 3a 12 0  f0 2f 0  0  0  0  8c 61 3c bb 3  0  fc 10 0  0  0  0  b6 0  0  0  0  0  e2 1  0  7d 2  0  df

  Byte  Data           Function      How to calculate
  ----- -------------- ------------- ----------------------------------------------
  00    0x55           HEADER
  01-03 0x3a 0x12 0x00 I_FAST_RMS[A]
  04-06 0xf0 0x2f 0x00 I_RMS[A]      002ff0 --> 12272   --> /266013                --> 0,046A
  07-09 0x00 0x00 0x00 I_RMS[B]
  10-12 0x8c 0x61 0x3c V_RMS         3C618C --> 3957132 --> /17159                 --> 231V
  13-15 0xbb 0x03 0x00 I_FAST_RMS[B]
  16-18 0xfc 0x10 0x00 WATT[A]       0010fc --> 4348    --> /713                   --> 6,1W
  19-21 0x00 0x00 0x00 WATT[B]
  22-24 0xb6 0x00 0x00 CF_CNT[A]     0000b6 --> 182     --> *0.000163              --> 0,0296kWh
  25-27 0x00 0x00 0x00 CF_CNT[B]
  28-29 0xe2 0x01      TPS1[internal]  01e2 --> 482     --> ((85*482-5440)/448)-45 --> 34,31°C
  30    0x00           Free
  31-32 0x7d 0x02      TPS2[external]
  33    0x00           Free
  34    0xdf           CHECKSUM

*/




#include <ArduinoOTA.h>
#include <TelnetPrint.h>


// Pins where BL0939 is connected to ESP32 in Sonoff Dual R3 (V2)
#define TXD2 25
#define RXD2 26

#define PACKET_HEADER        0x55

// sending commands
#define READ_COMMAND         0x55
#define REQUEST_FULL         0xAA

// Register to setup bl039
#define WRITE_COMMAND         0xA5
#define REG_IA_FAST_RMS_CTRL  0x10
#define REG_MODE              0x18
#define REG_SOFT_RESET        0x19
#define REG_USR_WRPROT        0x1A
#define REG_TPS_CTRL          0x1B
#define REG_IB_FAST_RMS_CTRL  0x1E

// Divisor / Multiplier for conversion to get real values
#define I_Conversion                266013
#define V_Conversion                17159
#define W_Conversion                713
#define CF_Conversion               0.000163


uint32_t voltage = 0;
float current_A  = 0;
float current_B  = 0;
int32_t power_A  = 0;
int32_t power_B  = 0;
float energy_A   = 0;
float energy_B   = 0;
float tps1       = 0;


uint8_t rx_cache[35];
uint8_t serial_in_byte = 0;
uint8_t buffer_size    = 35;
uint8_t byte_counter   = 0;
uint8_t checksum       = 0;
bool found_header      = false;
bool received          = false;
bool crc_checked       = false;
bool decoded           = false;




const uint8_t  bl0939_init[6][4] = {
  { REG_SOFT_RESET,       0x5A, 0x5A, 0x5A },   // Reset to default
  { REG_USR_WRPROT,       0x55, 0x00, 0x00 },   // Enable User Operation Write
  { REG_MODE,             0x00, 0x10, 0x00 },   // 0x0100 = CF_UNABLE energy pulse, AC_FREQ_SEL 50Hz, RMS_UPDATE_SEL 800mS
  { REG_TPS_CTRL,         0xFF, 0x47, 0x00 },   // 0x47FF = Over-current and leakage alarm on, Automatic temperature measurement, Interval 100mS
  { REG_IA_FAST_RMS_CTRL, 0x1C, 0x18, 0x00 },   // 0x181C = Half cycle, Fast RMS threshold 6172 on Channel 0
  { REG_IB_FAST_RMS_CTRL, 0x1C, 0x18, 0x00 }    // 0x181C = Half cycle, Fast RMS threshold 6172 on Channel 0
};



void BL0939_setup() {
  for (uint32_t i = 0; i < 6; i++) {
    uint8_t crc, byte;
    crc = byte = WRITE_COMMAND;
    Serial2.write(byte);
    for (uint32_t j = 0; j < 4; j++) {
      crc += byte = bl0939_init[i][j];
      Serial2.write(byte);
    }
    Serial2.write(0xFF ^ crc);
    delay(1);
  }
  Serial2.flush();
}


void Request_Data() {
  Serial2.flush();
  Serial2.write(READ_COMMAND);
  Serial2.write(REQUEST_FULL);
  TelnetPrint.println("Request Data");
}


void Read_Data() {
  while (Serial2.available()) {
    yield();
    serial_in_byte = Serial2.read();
    // TelnetPrint.print("Serial Byte: " + String(serial_in_byte, HEX));

    if (serial_in_byte == PACKET_HEADER) {
      byte_counter = 0;
      found_header = true;
      TelnetPrint.println("Paket Header found");
    }
    if (found_header) {
      rx_cache[byte_counter++] = serial_in_byte;
      // TelnetPrint.print("Byte_Counter: " + String (byte_counter));

      if (buffer_size == byte_counter) {
        received = true;
        TelnetPrint.println("Paket received");

        for (byte i = 0; i < byte_counter; i = i + 1) {
          TelnetPrint.print(String (rx_cache[i], HEX) + " ");
        }
        TelnetPrint.println("");
      }
    }
  }
}


void CRC_Check() {

  if (rx_cache[0] == PACKET_HEADER) {  // Check Header

    checksum = READ_COMMAND;
    for (uint32_t i = 0; i < buffer_size - 1; i++) {
      checksum += rx_cache[i];
    }
    checksum ^= 0xFF;
    TelnetPrint.println("CHECKSUM Data: " + String(rx_cache[buffer_size - 1]) + "\tChecksum: " + String(checksum));
    if (checksum == rx_cache[buffer_size - 1]) {
      crc_checked = true;
    } else {
      TelnetPrint.println("CHECKSUM_FAILURE!");
      do {
        memmove(rx_cache, rx_cache + 1, buffer_size - 1);
        byte_counter--;
      } while ((byte_counter > 1) && (PACKET_HEADER != rx_cache[0]));
      if (PACKET_HEADER != rx_cache[0]) {
        found_header = false;
        received     = false;
        byte_counter = 0;
      }
    }
  }
}


void Decode() {

  tps1       =                      rx_cache[29] << 8 | rx_cache[28];     // TPS1        unsigned
  voltage    = rx_cache[12] << 16 | rx_cache[11] << 8 | rx_cache[10];     // V_RMS       unsigned
  energy_A   = rx_cache[24] << 16 | rx_cache[23] << 8 | rx_cache[22];     // CF_CNT[A]   unsigned
  energy_B   = rx_cache[27] << 16 | rx_cache[26] << 8 | rx_cache[25];     // CF_CNT[B]   unsigned
  current_A  = rx_cache[6]  << 16 | rx_cache[5]  << 8 | rx_cache[4];      // I_RMS[A]    unsigned
  current_B  = rx_cache[9]  << 16 | rx_cache[8]  << 8 | rx_cache[7];      // I_RMS[B]    unsigned
  power_A    = rx_cache[18] << 16 | rx_cache[17] << 8 | rx_cache[16];     // WATT[A]      signed
  if (bitRead(power_A, 23)) {
    power_A |= 0xFF000000;  // Extend sign bit
  }
  power_B    = rx_cache[21] << 16 | rx_cache[20] << 8 | rx_cache[19];     // WATT[B]      signed
  if (bitRead(power_B, 23)) {
    power_B |= 0xFF000000;  // Extend sign bit
  }
  decoded = true;
}


void Display_data() {
  TelnetPrint.println ("Current Voltage: " + String(voltage    / V_Conversion) + "V");
  TelnetPrint.println ("Power Channel 0: " + String(current_A  / I_Conversion) + "A");
  TelnetPrint.println ("Power Channel 1: " + String(current_B  / I_Conversion) + "A");
  TelnetPrint.println ("Power Channel 0: " + String(power_A    / W_Conversion) + "W");
  TelnetPrint.println ("Power Channel 1: " + String(power_B    / W_Conversion) + "W");
  TelnetPrint.println ("Energy CNT CN 0: " + String(energy_A   * CF_Conversion) + "kWh");
  TelnetPrint.println ("Energy CNT CN 0: " + String(energy_B   * CF_Conversion) + "kWh");
  // float e = (energy_A * CF_Conversion);
  // TelnetPrint.println ("Energy CNT CN 0: " + String(e, 5) + "kWh");
  // float e = (energy_B * CF_Conversion);
  // TelnetPrint.println ("Energy CNT CN 0: " + String(e, 5) + "kWh");
  TelnetPrint.println ("Curr. Chip Temp: " + String(((85 * tps1 - 5440) / 448) - 45)  + "C");
}

void ideOTASetup()
{
  ArduinoOTA.setHostname("ESP32-BL0939");    // IDE OTA give a name to your ESP for the Arduino IDE
  ArduinoOTA.onStart([]() {
    TelnetPrint.println("[otaide] OTA started");
  })
  .onEnd([]() {
    TelnetPrint.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    TelnetPrint.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    TelnetPrint.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)  TelnetPrint.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)  TelnetPrint.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)  TelnetPrint.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)  TelnetPrint.println("Receive Failed");
    else if (error == OTA_END_ERROR)  TelnetPrint.println("End Failed");
  });
}




void setup() {

  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);

  byte i = 0;
  WiFi.mode(WIFI_STA);
  WiFi.hostname("ESP32-BL0939");
  WiFi.begin("XXX", "XXX");
  Serial.print("Connecting WiFi.");
  while  (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay (500);
    ++i;
    Serial.print(".");
    if (i > 20) {
      ESP.restart();
    }
  }
  TelnetPrint.begin();
  Serial.println(WiFi.localIP());
  ideOTASetup();
  ArduinoOTA.begin();
  BL0939_setup();
  pinMode (27, OUTPUT);    // Relay A
  pinMode (14, OUTPUT);    // Relay B
  digitalWrite(27, HIGH);  // must be active to measure current, power and energy
  digitalWrite(14, HIGH);  // must be active to measure current, power and energy
}

void loop() {
  ArduinoOTA.handle();
  if (!received) {
    Request_Data();
    delay(1000);
    Read_Data();
  }

  if (received) {
    CRC_Check();
  }

  if (crc_checked) {
    Decode();
  }

  if (decoded) {
    Display_data();
    found_header = false;
    received    = false;
    crc_checked = false;
    decoded     = false;
  }
}
