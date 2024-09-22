#include <ESP32Servo.h>
#include <NewPing.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

#define PIN_ECHO_ULTRASONIK_OBJEK 18
#define PIN_TRIGGER_ULTRASONIK_OBJEK 19
#define PIN_ECHO_ULTRASONIK_SAMPAH 25
#define PIN_TRIGGER_ULTRASONIK_SAMPAH 26
#define PIN_SERVO 27
#define PIN_RELAY 14
#define PIN_LED_KONEKSI 2
#define PIN_LED_SAMPAH 15
#define MAX_DISTANCE_ULTRASONIK_OBJEK 200
#define MAX_DISTANCE_ULTRASONIK_SAMPAH 30
#define BOT_TOKEN "TOKEN_BOT_TELEGRAM"
#define CHAT_ID_ADMIN "ID_ADMIN"

Servo myservo;
NewPing deteksiObjek(PIN_TRIGGER_ULTRASONIK_OBJEK, PIN_ECHO_ULTRASONIK_OBJEK, MAX_DISTANCE_ULTRASONIK_OBJEK);
NewPing deteksiSampah(PIN_TRIGGER_ULTRASONIK_SAMPAH, PIN_ECHO_ULTRASONIK_SAMPAH, MAX_DISTANCE_ULTRASONIK_SAMPAH);
bool servoTerbuka = false;
bool peringatanPenuh = false;
String statusEnum, modeEnum;
enum Status { Kosong,
              BelumPenuh,
              Penuh,
              Terbuka,
              Tertutup };
Status status = Kosong;
enum Mode { Aktif,
            Standby };
Mode mode = Aktif;

const char* ssid[] = {"XXXXXXXX", "XXXXXXXX"};
const char* password[] = {"XXXXXXXX", "XXXXXXXX"};
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
int delayRequestBot = 1000;
unsigned long lastTimeBotRun;
unsigned long lightTimerExpires;
boolean lightTimerActive = false;
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

void setup() {
  Serial.begin(115200);
  lcd.init();                    
  lcd.backlight();
  Serial.println("ROBOBIN is Booting...");
  pinMode(PIN_LED_KONEKSI, OUTPUT);
  pinMode(PIN_LED_SAMPAH, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  WiFi.mode(WIFI_STA);
  bool connected = false;
  for (int i = 0; i < sizeof(ssid) / sizeof(ssid[0]); i++) {
    Serial.print("Menghubungkan ke ");
    Serial.println(ssid[i]);
    WiFi.begin(ssid[i], password[i]);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      Serial.print(".");
      delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\nTerhubung!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    } else {
      Serial.println("\nGagal menghubungkan.");
    }
  }
  if (!connected) {
    Serial.println("Tidak ada WiFi yang bisa terhubung.");
  }
  digitalWrite(PIN_LED_KONEKSI, HIGH);
  digitalWrite(PIN_RELAY, HIGH);
  myservo.attach(PIN_SERVO);
  myservo.write(0);
  statusEnum = "Kosong";
  modeEnum = "Aktif";
  Serial.println("WiFi is connected!");
  Serial.println("ROBOBIN ready!");
  bot.sendMessage(CHAT_ID_ADMIN, "Halo Admin👋\nROBOBIN Anda sudah siap untuk digunakan!\n\nSilahkan gunakan perintah /start untuk mulai BOT. Terimakasih😊");
}

void loop() {
  delay(1000);
  runMachine();
  handleLCD();
}

void runMachine() {
  int jarakObjek = deteksiObjek.ping_cm();
  int jarakSampah = deteksiSampah.ping_cm();
  int kapasitas = 100 - ((jarakSampah * 100) / MAX_DISTANCE_ULTRASONIK_SAMPAH);

  Serial.print("\nROBOBIN TEMPAT SAMPAH CERDAS OTOMATIS\n");
  Serial.print("\nInformasi Sistem:\n");
  Serial.print("\- IP Address\t: ");
  Serial.println(WiFi.localIP());
  Serial.println("\- Jarak Objek\t: " + String(jarakObjek) + "cm");
  Serial.println("- Jarak Sampah\t: " + String(jarakSampah) + "cm");

  if (jarakSampah == 0) {
    kapasitas = 0;
    status = Kosong;
    statusEnum = "Kosong";
    Serial.println("- Kapasitas\t: " + String(kapasitas) + "% Terpakai");
    Serial.println("- Status\t: " + statusEnum);
  } else {
    Serial.println("- Kapasitas\t: " + String(kapasitas) + "% Terpakai");
    if (jarakSampah > 0 && jarakSampah <= 10) {
      if (!servoTerbuka) {
        status = Penuh;
        statusEnum = "Sudah Penuh";
        peringatanPenuh = true;
        Serial.println("- Status\t: " + statusEnum);
        digitalWrite(PIN_LED_SAMPAH, HIGH);
        delay(250);
        digitalWrite(PIN_LED_SAMPAH, LOW);
      }
    } else {
      status = BelumPenuh;
      statusEnum = "Belum Penuh";
      Serial.println("- Status\t: " + statusEnum);
    }
  }

  if (jarakObjek > 0 && jarakObjek < 50) {
    status = Terbuka;
    statusEnum = "Terbuka";
    Serial.println("- Status\t: " + statusEnum);
    myservo.write(260);
    servoTerbuka = true;
    peringatanPenuh = false;
  } else {
    if (servoTerbuka) {
      myservo.write(0);
      servoTerbuka = false;
      status = Tertutup;
      statusEnum = "Tertutup";
      Serial.println("- Status\t: " + statusEnum);
    }
  }
  
  Serial.println("- Mode\t\t: " + modeEnum);
  Serial.println("");

  if (millis() > lastTimeBotRun + delayRequestBot) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.print("NumMsg: ");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRun = millis();
  }
}

void handleLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("-- ROBOBIN ID --");
  lcd.setCursor(0,1);
  lcd.print("Status " + modeEnum);
}

void handleNewMessages(int numNewMessages) {
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    // if (chat_id != CHAT_ID_ADMIN) {
    //   bot.sendMessage(chat_id, "Maaf, Anda tidak terdaftar didalam database ROBOBIN!");
    //   continue;
    // }
    String text = bot.messages[i].text;
    Serial.println("MsgBOT: " + text);
    String from_name = bot.messages[i].from_name;

    if (text == F("/on")) {
      mode = Aktif;
      modeEnum = "Aktif";
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(PIN_LED_KONEKSI, HIGH);
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      bot.sendMessage(chat_id, "ROBOBIN Mode Aktif");
    } else if (text == F("/off")) {
      mode = Standby;
      modeEnum = "Standby";
      digitalWrite(PIN_RELAY, LOW);
      digitalWrite(PIN_LED_KONEKSI, LOW);
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      bot.sendMessage(chat_id, "ROBOBIN Mode Standby");
    } else if (text == F("/buka")) {
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      myservo.write(260);
      bot.sendMessage(chat_id, "ROBOBIN Anda dalam Keadaan Terbuka.\nJika sudah selesai membuang sampah, silahkan gunakan perintah /tutup untuk menutup ROBOBIN.");
      delay(10000);
    } else if (text == F("/tutup")) {
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      myservo.write(0);
      bot.sendMessage(chat_id, "ROBOBIN Anda dalam Keadaan Tertutup.\nJika ingin membuka, silahkan gunakan perintah /buka untuk membuka ROBOBIN.");
    } else if (text == F("/status")) {
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      if (digitalRead(PIN_RELAY) == HIGH) {
        bot.sendMessage(chat_id, "ROBOBIN Anda sedang dalam Mode Aktif");
      } else {
        bot.sendMessage(chat_id, "ROBOBIN Anda sedang dalam Mode Standby");
      }
    } else if (text == F("/info")) {
      int jarakObjek = deteksiObjek.ping_cm();
      int jarakSampah = deteksiSampah.ping_cm();
      int kapasitas = 100 - ((jarakSampah * 100) / MAX_DISTANCE_ULTRASONIK_SAMPAH);
      long seconds = millis() / 1000;
      int hours = seconds / 3600;
      int minutes = (seconds % 3600) / 60;
      int sec = seconds % 60;
      IPAddress ipAddress = WiFi.localIP();
      String ipAddressStr = String(ipAddress[0]) + "." + String(ipAddress[1]) + "." + String(ipAddress[2]) + "." + String(ipAddress[3]);
      String infoMsg = "Hai, " + from_name + "!\nBerikut informasi mengenai ROBOBIN Anda.\n\nROBOBIN TEMPAT SAMPAH CERDAS OTOMATIS";
      infoMsg += "\n- IP Address\t: " + ipAddressStr;
      infoMsg += "\n- Jarak Objek\t: " + String(jarakObjek) + "cm";
      infoMsg += "\n- Jarak Sampah\t: " + String(jarakSampah) + "cm";
      infoMsg += "\n- Kapasitas\t: " + String(kapasitas) + "% Terpakai";
      infoMsg += "\n- Status\t: " + statusEnum;
      infoMsg += "\n- Mode\t: " + modeEnum;
      infoMsg += "\n\n* * Uptime " + String(hours) + " jam " + String(minutes) + " menit " + String(sec) + " detik * *\n";
      bot.sendChatAction(chat_id, "typing");
      delay(250);
      bot.sendMessage(chat_id, infoMsg);
    } else {
      if (text == F("/start")) {
        String welcomeMsg = "Selamat datang " + from_name + " 👋\n";
        welcomeMsg += "Saya adalah ROBOBIN, BOT Kontrol Tempat Sampah Cerdas.\n\nGunakan perintah /help untuk melihat daftar perintah yang terdapat pada bot ini.";
        bot.sendChatAction(chat_id, "typing");
        delay(250);
        bot.sendMessage(chat_id, welcomeMsg);
      }
      if (text == F("/help")) {
        String helpMsg = "Hai " + from_name + " 👋\n";
        helpMsg += "Dalam ROBOBIN ini terdapat perintah yang dapat digunakan, yaitu:\n\n/start - Memulai ROBOBIN\n/help - Menampilkan menu bantuan\n/info - Menampilkan informasi ROBOBIN\n/status - Menampilkan keadaan singkat ROBOBIN\n/buka - Membuka ROBOBIN\n/tutup - Menutup ROBOBIN\n/on - Menghidupkan ROBOBIN\n/off - Mematikan ROBOBIN";
        bot.sendChatAction(chat_id, "typing");
        delay(250);
        bot.sendMessage(chat_id, helpMsg);
      }
    }
  }
}
