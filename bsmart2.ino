#define TINY_GSM_MODEM_SIM7600

#include <ArduinoJson.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT21

#define SIM_RESET_PIN 15
#define SIM_AT_BAUDRATE 115200
#define GSM_PIN ""

// GPRS config
const char apn[] = "m-wap";
// MQTT config
#define PORT_MQTT 1883
const char* broker = "bsmart2.cloud.shiftr.io";
const char* mqtt_client_name = "loc4atnt Nek";
const char* mqtt_user = "bsmart2";
const char* mqtt_pass = "34DOBprzncvolzM3";
const char* topic_sensor = "farm/fff";

TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);
PubSubClient gsmMqtt(gsmClient);

DHT dht(DHTPIN, DHTTYPE);
float temp = 0;
float hum = 0;

// Hàm dùng để thiết lập modem SIM tạo kết nối GPRS
bool connectToGPRS() {
  // Unlock SIM (nếu có)
  if (GSM_PIN && modem.getSimStatus() != 3)
    modem.simUnlock(GSM_PIN);

  Serial.println("Ket noi toi nha mang...");
  while (!modem.waitForNetwork(60000L)) {
    Serial.println("._.");
    delay(5000);
  }

  // Nếu không thấy sóng từ nhà mạng thì thoát hàm
  if (!modem.isNetworkConnected())
    return false;

  Serial.println("Ket noi GPRS");
  if (!modem.gprsConnect(apn)) {// Hàm kết nối tới gprs trả về true/false cho biết có kết nối được hay chưa
    Serial.print("Khong the ket noi GPRS - ");
    Serial.println(apn);
    return false;
  }

  // Kiểm tra lại lần nữa để chắc cú
  if (modem.isGprsConnected()){
    Serial.println("Da ket noi duoc GPRS!");
    return true;
  }
  return false;
}

// Hàm khởi động module SIM
bool initModemSIM() {
  Serial.println("Bat dau khoi dong module SIM");

  // Đặt chân reset của module xuống LOW để chạy
  pinMode(SIM_RESET_PIN, OUTPUT);
  digitalWrite(SIM_RESET_PIN, LOW);
  delay(5000);

  Serial2.begin(SIM_AT_BAUDRATE);// Module SIM giao tiếp với ESP qua cổng Serial2 bằng AT cmds

  if (!modem.restart()) {
    Serial.println("Khong the khoi dong lai module SIM => Co loi");
    return false;
  }
  return true;
}

// Hàm kết nối tới MQTT Broker
boolean connectMQTT(PubSubClient *mqtt) {
  Serial.print("Ket noi broker ");
  Serial.print(broker);
  boolean status = mqtt->connect(mqtt_client_name, mqtt_user, mqtt_pass);
  if (status == false) {
    Serial.println(" khong thanh cong!");
    return false;
  }
  Serial.println(" thanh cong!");

  return mqtt->connected();
}

// Hàm xử lý trong loop() dành cho MQTT Client dùng GPRS
void mqttLoopWithGPRS() {
  if (!gsmMqtt.connected()) { // Nếu chưa có kết nối tới MQTT Broker
    Serial.println("Ket noi MQTT voi GPRS");
    while ( (!gsmMqtt.connected()) && modem.isGprsConnected()) {
      connectMQTT(&gsmMqtt);
      delay(5000);
    }
  } else { // Nếu đã có kết nối tới MQTT Broker rồi
    String sendingStr = "";
    StaticJsonDocument<32> doc;
    doc["t"] = temp;
    doc["h"] = hum;
    serializeJson(doc, sendingStr);
    
    gsmMqtt.publish(topic_sensor,sendingStr.c_str());
    gsmMqtt.loop();// Hàm xử lý của thư viện mqtt
  }
}

void getSensorData(){
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(temp)) temp = 0;
  if (isnan(hum)) hum = 0;
}

void setup() {
  Serial.begin(115200);

  if (initModemSIM()){
    while(!connectToGPRS()) delay(100);
  }

  gsmMqtt.setServer(broker, PORT_MQTT);

  dht.begin();
}

void loop() {
  if (modem.isGprsConnected()) {
    getSensorData();
    mqttLoopWithGPRS();
  } else {
    Serial.println("Khong co ket noi GRPS!");
    delay(2000);
  }
}
