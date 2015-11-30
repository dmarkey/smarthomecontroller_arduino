#include <sha1.h>
#include <IRremote.h>
#include <Event.h>
#include <Timer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Client.h>
//#include <EEPROM.h>
#include <WebSocketsClient.h>
#include <Ethernet.h>
#include <SPI.h>

#define DHTPIN 11
#define DHTTYPE DHT22
#define RF_DATA_PIN 12
#define IR_RECV_PIN 13

#define WS_SERVER "dmarkey.com"
#define WS_PATH "/ws"
#define WS_PORT 8080
#define CHIPID 1234567


#define PROGMEM page "<html> <body> <form action='.' method='post'>  SSID: <input type='text' name='ssid'><br>  Password: <input type='password' name='password'><br>  <input type='submit' value='Submit'></form></body></html>"


WebSocketsClient webSocket;
EthernetClient client;



DHT dht(DHTPIN, DHTTYPE);
Timer t;


//EthernetClient client;

IRrecv irrecv(IR_RECV_PIN);


#define ch1on "10110000000000010001"
#define ch1off "10110000000000000000"
#define ch2on "10110000000010010011"
#define ch2off "10110000000010000010"
#define ch3on "10110000000001010000"
#define ch3off "10110000000001000001"
#define ch4on "10110000000011010010"
#define ch4off "10110000000011000011"
#define chMon "10110000000011110000"
#define chMoff "10110000000011100001"
#define dimmUp1 "10110000000000001010"
#define dimmDn1 "10110000000000011011"
#define dimmUp2 "10110000000010001000"
#define dimmDn2 "10110000000010011001"
#define dimmUp3 "10110000000001001011"
#define dimmDn3 "10110000000001011010"
#define dimmUp4 "10110000000011001001"
#define dimmDn4 "10110000000011011000"



boolean sendCode(const char code[]) { //empfange den Code in Form eines Char[]
  sendSymbol('x');
  for (short z = 0; z < 7; z++) { //wiederhole den Code 7x
    for (short i = 0; i < 20; i++) { //ein Code besteht aus 20bits
      sendSymbol(code[i]);
    }
    sendSymbol('x'); //da der code immer mit x/sync abschliesst, brauchen wir den nicht im code und haengen es autisch immer hinten ran.
  }
  return true;
}
void sendSymbol(char i) { //Diese Funktion soll 0,1 oder x senden koennen. Wir speichern die gewuenschte Ausgabe in der Variabel i
  switch (i) { //nun gucken wir was i ist
    case '0':
      { //Der Code fuer '0'
        digitalWrite(RF_DATA_PIN, LOW);
        delayMicroseconds(600);
        digitalWrite(RF_DATA_PIN, HIGH);
        delayMicroseconds(1400);
        return;
      }
    case '1':
      { //Der Code fuer '1'
        digitalWrite(RF_DATA_PIN, LOW);
        delayMicroseconds(1300);
        digitalWrite(RF_DATA_PIN, HIGH);
        delayMicroseconds(700);
        return;
      }
    case 'x':
      { //Der Code fuer x(sync)
        digitalWrite(RF_DATA_PIN, LOW);
        delay(81);
        digitalWrite(RF_DATA_PIN, HIGH);
        delayMicroseconds(800);
      }

  }
}


int connected = false;


#define MAX_CODE_CYCLES 4

#define SHORT_DELAY       380
#define NORMAL_DELAY      500
#define SIGNAL_DELAY     1500

#define SYNC_DELAY       2650
#define EXTRASHORT_DELAY 3000
#define EXTRA_DELAY     10000

enum {
  PLUG_A = 0,
  PLUG_B,
  PLUG_C,
  PLUG_D,
  PLUG_MASTER,
  PLUG_LAST
};

unsigned long signals[PLUG_LAST][2][MAX_CODE_CYCLES] = {
  { /*A*/
    {0b101111000001000101011100, 0b101100010110110110101100, 0b101110101110010001101100, 0b101101000101010100011100},
    {0b101101010010011101111100, 0b101111100011110000101100, 0b101111110111001110001100, 0b101110111000101110111100}
  },
  { /*B*/
    {0b101101110100001000110101, 0b101101101010100111100101, 0b101110011101111000000101, 0b101100100000100011110101},
    {0b101111011001101011010101, 0b101100111011111101000101, 0b101110001100011010010101, 0b101100001111000011000101}
  },
  { /*C*/
    {0b101101010010011101111110, 0b101111100011110000101110, 0b101111110111001110001110, 0b101110111000101110111110},
    {0b101110101110010001101110, 0b101101000101010100011110, 0b101111000001000101011110, 0b101100010110110110101110}
  },
  { /*D*/
    {0b101111011001101011010111, 0b101100111011111101000111, 0b101100001111000011000111, 0b101110001100011010010111},
    {0b101101110100001000110111, 0b101100100000100011110111, 0b101101101010100111100111, 0b101110011101111000000111}
  },
  { /*MASTER*/
    {0b101111100011110000100010, 0b101110111000101110110010, 0b101101010010011101110010, 0b101111110111001110000010},
    {0b101111000001000101010010, 0b101101000101010100010010, 0b101110101110010001100010, 0b101100010110110110100010}
  },
};

boolean       onOff;
unsigned char plug;
unsigned char swap;


void sendSync() {
  digitalWrite(RF_DATA_PIN, HIGH);
  delayMicroseconds(SHORT_DELAY);
  digitalWrite(RF_DATA_PIN, LOW);
  delayMicroseconds(SYNC_DELAY - SHORT_DELAY);
}

void sendValue(boolean value, unsigned int base_delay) {
  unsigned long d = value ? SIGNAL_DELAY - base_delay : base_delay;
  digitalWrite(RF_DATA_PIN, HIGH);
  delayMicroseconds(d);
  digitalWrite(RF_DATA_PIN, LOW);
  delayMicroseconds(SIGNAL_DELAY - d);
}

void longSync() {
  digitalWrite(RF_DATA_PIN, HIGH);
  delayMicroseconds(EXTRASHORT_DELAY);
  digitalWrite(RF_DATA_PIN, LOW);
  delayMicroseconds(EXTRA_DELAY - EXTRASHORT_DELAY);
}

void ActivatePlug(unsigned char PlugNo, boolean On) {
  Serial.print("Activating Plug ");
  Serial.println(PlugNo);
  delayMicroseconds(1000);
  if ( PlugNo < PLUG_LAST ) {

    digitalWrite(RF_DATA_PIN, LOW);
    delayMicroseconds(1000);

    unsigned long signal = signals[PlugNo][On][swap];

    swap++;
    swap %= MAX_CODE_CYCLES;

    for (unsigned char i = 0; i < 4; i++) { // repeat 1st signal sequence 4 times
      sendSync();
      for (unsigned char k = 0; k < 24; k++) { //as 24 long and short signals, this loop sends each one and if it is long, it takes it away from total delay so that there is a short between it and the next signal and viceversa
        sendValue(bitRead(signal, 23 - k), SHORT_DELAY);
      }
    }
    for (unsigned char i = 0; i < 4; i++) { // repeat 2nd signal sequence 4 times with NORMAL DELAY
      longSync();
      for (unsigned char k = 0; k < 24; k++) {
        sendValue(bitRead(signal, 23 - k), NORMAL_DELAY);
      }
    }

  }
  Serial.print("Done: ");
  Serial.println(PlugNo);
}

void setTaskStatus(char * task_id, int status) {

  Serial.println(task_id);
  char post_data[128];

  DynamicJsonBuffer outJsonBuffer;


  JsonObject& root = outJsonBuffer.createObject();
  root["controller_id"] = CHIPID;
  root["task_id"] = task_id;
  root["status"] = status;
  root["event"] = "task_status";

  root.printTo(post_data, sizeof(post_data));
  webSocket.sendTXT(post_data);

}


void processSwitchcmd(JsonObject& obj) {

  int switch_num = obj["socketnumber"];



  bool state = obj["state"];


  if (switch_num < 5) {
    if (state) {

      switch (switch_num) {
        case 1:
          {
            sendCode(ch1on);
            Serial.println("command ch1on executed");
            break;
          }
        case 2:
          {
            sendCode(ch2on);
            Serial.println("command ch2on executed");
            break;
          }
        case 3:
          {
            sendCode(ch3on);
            Serial.println("command ch3on executed");
            break;
          }
        case 4:
          {
            sendCode(ch4on);
            Serial.println("command ch4on executed");
            break;
          }
      }

    }
    else {


      switch (switch_num) {
        case 1:
          {
            sendCode(ch1off);
            //Serial.println("command ch1off executed");
            break;
          }
        case 2:
          {
            sendCode(ch2off);
            //Serial.println("command ch2off executed");
            break;
          }
        case 3:
          {
            sendCode(ch3off);
            //Serial.println("command ch3off executed");
            break;
          }
        case 4:
          {
            sendCode(ch4off);
            //Serial.println("command ch4off executed");
            break;
          }
      }

    }
    return;
  }


  ActivatePlug(switch_num - 5, state);


}


void handle_task(byte* payload, unsigned int length)
{
  //Serial.println(millis());
  //Serial.println("Incoming command");
  //Serial.println((char *)payload);
  //Serial.println(ESP.getFreeHeap(), DEC);
  char task_id[40];
  //Serial.println(ESP.getFreeHeap(), DEC);
  DynamicJsonBuffer inJsonBuffer;
  JsonObject& root = inJsonBuffer.parseObject((char *)payload);
  if (!root.success()) {
    Serial.print("Problem parsing this message:");
    Serial.println((char *)payload);
    return;
  }
  const char * command = root["name"];
  snprintf(task_id, 40, "%s", (const char *)root["task_id"]);
  Serial.println("Setting task status");
  if (strcmp(command, "sockettoggle") != -1) {
    processSwitchcmd(root);
    setTaskStatus(task_id, 3);
  }

}



void webSocketEvent(WStype_t type, uint8_t * payload, size_t lenght) {


  switch (type) {
    case WStype_DISCONNECTED:
      //Serial.print("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      {
        //Serial.print("[WSc] Connected to url: %s\n",  (char *)payload);
        sendBeacon();
      }
      break;
    case WStype_TEXT:
      //Serial.print("[WSc] get text: %s\n", payload);
      handle_task(payload, lenght);

      break;

  }

}

void readTemp() {
  StaticJsonBuffer<128> tempJsonBuffer;
  char post_data[128];
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  //Serial.print("Temperature: ");
  //Serial.println(t);

  JsonObject& root = tempJsonBuffer.createObject();
  root["controller_id"] = CHIPID;
  root["event"] = "Temp";
  root["route"] = "Temperature";
  root["value"] = t;
  root["hum"] = h;

  root.printTo(post_data, sizeof(post_data));
  webSocket.sendTXT(post_data);
  delay(10);

}



void setup(void) {
  //EEPROM.begin();
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting..");
  pinMode(IR_RECV_PIN, OUTPUT);
  pinMode(RF_DATA_PIN, OUTPUT);

  irrecv.enableIRIn();
  //StaticJsonBuffer<1000> json;
  //JsonObject& root = json.parseObject(eeprom_read());
  Serial.print("Getting an IP...");

  uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
  Ethernet.begin(mac);

  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("subnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("gatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());
  
  Ethernet.begin(mac);
  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());

  webSocket.onEvent(webSocketEvent);
  webSocket.begin(WS_SERVER, WS_PORT, WS_PATH);

  t.every(10000, readTemp);

}


int attempt = 0;


void sendBeacon() {
  char buf[128];
  StaticJsonBuffer<128> BeaconBuff;
  JsonObject& root = BeaconBuff.createObject();
  root["controller_id"] = CHIPID;
  root["event"] = "BEACON";
  root["route"] = "All";
  root["model"] = "SmartController-8RFT";
  root.printTo(buf, sizeof(buf));
  webSocket.sendTXT(buf, strlen(buf));
}


const char *  encoding (decode_results *results)
{
  switch (results->decode_type) {
    default:
    case UNKNOWN:      return NULL;       break ;
    case NEC:          return "NEC";           break ;
    case SONY:         return "SONY";          break ;
    case RC5:          return "RC5";           break ;
    case RC6:          return "RC6";           break ;
    case DISH:         return "DISH";          break ;
    case SHARP:        return "SHARP";         break ;
    case JVC:          return "JVC";           break ;
    case SANYO:        return "SANYO";         break ;
    case MITSUBISHI:   return "MITSUBISHI";    break ;
    case SAMSUNG:      return "SAMSUNG";       break ;
    case LG:           return "LG";            break ;
    case WHYNTER:      return "WHYNTER";       break ;
    case AIWA_RC_T501: return "AIWA_RC_T501";  break ;
    case PANASONIC:    return "PANASONIC";     break ;
  }
}


void  dumpInfo (decode_results *results)
{

  char buf[200];
  StaticJsonBuffer<256> IRJsonBuffer;
  JsonObject& root = IRJsonBuffer.createObject();
  root["controller_id"] = CHIPID;
  root["event"] = "IRIN";
  root["route"] = "RemoteControl";
  const char * encoding_str = encoding(results);
  if (encoding_str == NULL || results->value == -1) {

    return;

  }
  root["encoding"] = encoding_str;
  root["code"] = results->value;
  root.printTo(buf, sizeof(buf));
  webSocket.sendTXT(buf, strlen(buf));
}



void loop(void) {


    decode_results  results;        // Somewhere to store the results
    if (irrecv.decode(&results)) {  // Grab an IR code
      dumpInfo(&results);           // Output the results
      irrecv.resume();              // Prepare for the next value
    }
    t.update();
    webSocket.loop();


}

