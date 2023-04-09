#include <NeoSWSerial.h>

String Version = "V1.03";
#define Power_Relay 2
NeoSWSerial LoRa(8, 9); //設置LoRa Module SoftwareSerial (RX,TX)

uint16_t Modbus_data[5];

uint8_t start_error_flag = 0;
uint8_t join_ok_flag = 0, rev_error_value = 0, lora_connect_ok_flag = 0;

uint32_t Task_100ms_Time, Task_1s_Time, Task_10s_Time;
uint16_t Task_100ms_Interval = 100, Task_1s_Interval = 1 * 1000, Task_10s_Interval = 10 * 1000 ;



void setup()
{
  Serial.begin(9600);
  LoRa.begin(9600);
  LoRa.listen();
  delay(2000);
  get_LoRa_data();

  uint8_t i = 0;
  do {
    i++;
    if (LoRa_Start("AT+CGMI?")) {
      start_error_flag = 0;
      Serial.println(F("LoRa Module Started Successfully"));
      read_lora_module_information();  //讀取LoRa模塊信息
      delay(1000);
      LoRa_Send_data("AT+CJOIN=1,0,10,1");
      Serial.println(get_LoRa_data());

    } else {
      start_error_flag = 1;
      if (start_error_flag && i >= 10) {
        Serial.println(F("LoRa Module Started Failed, Please Check The LoRa Module"));
      }
    }
    delay(300);
  } while (start_error_flag && i < 10);
}

void loop()
{
  //從串口監視器讀取字符串
  if (Serial.available()) {  //如果獲取到有效字節，在串口讀取模擬數據輸入
    String comdata = "";
    while (Serial.available()) {
      comdata += char(Serial.read());
      delay(2);
    }
    Serial.println(comdata);
    
      LoRa.println(comdata);
      //print_rev_data();
      String rev_data = "";
      rev_data = get_LoRa_data();
      Serial.println(rev_data);
    
  }

  unsigned long currentMillis = millis();  // 獲取當前時間
  millisBlink(currentMillis);

}

void millisBlink(unsigned long currentTime) {
  Task();

  //檢查是否到達時間間隔0.1秒運行一次
  if (currentTime - Task_100ms_Time >= Task_100ms_Interval) {
    Task_100ms();
    Task_100ms_Time = currentTime;  // 將檢查時間復位
  } else if (currentTime - Task_100ms_Time <= 0) {
    // 如果millis時間溢出,時間復位
    Task_100ms_Time = currentTime;
  }

  //檢查是否到達時間間隔1秒運行一次
  if (currentTime - Task_1s_Time >= Task_1s_Interval) {
    Task_1s();
    Task_1s_Time = currentTime;  // 將檢查時間復位
  } else if (currentTime - Task_1s_Time <= 0) {
    // 如果millis時間溢出,時間復位
    Task_1s_Time = currentTime;
  }

  //檢查是否到達時間間隔30秒運行一次
  if (currentTime - Task_10s_Time >= Task_10s_Interval) {
    Task_30s();
    Task_10s_Time = currentTime;  // 將檢查時間復位
  } else if (currentTime - Task_10s_Time <= 0) {
    // 如果millis時間溢出,時間復位
    Task_10s_Time = currentTime;
  }
}


//
void Task() {
  String rev_data = get_LoRa_data();

  if (rev_data.length() > 0) {
    Serial.print(rev_data);
    if (rev_data.indexOf("CJOIN") >= 0) {
      if (rev_data.indexOf("OK") > 0) {
        join_ok_flag = 1;
      } else {
        join_ok_flag = 0;
      }
    } else if (rev_data.indexOf("Rejion") >= 0) {
      Serial.println(F("Rejion"));
      join_ok_flag = 0;

    } else if (rev_data.indexOf("ERR") >= 0) {
      Serial.println(F("ERROR"));
      rev_error_value ++;

    } else if (rev_data.indexOf("RECV") >= 0) {
      Serial.println("RECV");
      rev_error_value = 0;

      String data = "";
      int index = rev_data.indexOf("OK+RECV:") + 14;
      data = rev_data.substring(index, index + 2);
      int Dec_1 = Hex_to_Dec(data.substring(0, 1));
      int Dec_2 = Hex_to_Dec(data.substring(1, 2));
      int Dec = Dec_1 * 16 + Dec_2;
      //Serial.println("Dec:" + String(Dec));

      if (Dec > 0) {
        data = rev_data.substring(index + 3);
        data.replace("\n", "");
        //Serial.println("data:" + data);
        data = Hex_to_String(data);
        Serial.println("rev_data:" + data);
        if (data.indexOf("RL") >= 0) {
          int index_on = data.indexOf("RL");
          data = data.substring(index_on + 4, index_on + 5);
          digitalWrite(Power_Relay, data.toInt());
          //Serial.println("data:" + String(data));
          delay(100);
          LoRa.listen();
          Send_data_Relay();

        }
      }
    }
  }
}

// 0.1秒運行一次
void Task_100ms() {

}

// 1秒運行一次
void Task_1s() {

}

// 30秒運行一次
void Task_30s() {

  if (!join_ok_flag) {
    LoRa_Send_data("AT+CJOIN=1,0,10,1");

  } else if (rev_error_value > 3) {
    LoRa_Send_data("AT+CJOIN=1,0,10,1");
    rev_error_value = 0;

  } else if (return_lora_module_status_int() == 0) {
    lora_connect_ok_flag = 0;

    if (join_ok_flag) {
      LoRa_Send_data("AT+CJOIN=1,0,10,1");
    }
  } else {
    lora_connect_ok_flag = 1;
  }

  if (lora_connect_ok_flag) {
    String json_hex_data = "";
    json_hex_data = "{\"RL\":" + String(digitalRead(Power_Relay)) + "}";

    Serial.print(F("Json_Data:"));
    Serial.println(json_hex_data);

    json_hex_data = String_to_Hex(json_hex_data);

    String str_lengths = String(json_hex_data.length() / 2 + 2);
    json_hex_data = "AT+DTRX=1,2," + str_lengths + "," + json_hex_data;

    Serial.println(json_hex_data);
    LoRa.println(json_hex_data);

  }
}


void Send_data_Relay() {
  if (lora_connect_ok_flag) {
    String json_hex_data = "";

    json_hex_data = "{\"RL\":" + String(digitalRead(Power_Relay)) + "}";
    Serial.print(F("Json_Data:"));
    Serial.println(json_hex_data);

    json_hex_data = String_to_Hex(json_hex_data);

    String str_lengths = String(json_hex_data.length() / 2 + 2);
    json_hex_data = "AT+DTRX=1,2," + str_lengths + "," + json_hex_data;

    Serial.println(json_hex_data);
    LoRa.println(json_hex_data);

  }
}

void read_lora_module_information() {
  String JOINMODE = "",
         RXP = "",
         DEVEUI = "",
         APPEUI = "",
         APPKEY = "",
         FREQBANDMASK = "",
         ULDLMODE = "",
         CLASS = "";

  Serial.println("----------------LoRa Module Information-----------------");
  // JOINMODE
  int JOINMODE_value = (read_recv_cmd("AT+CJOINMODE?")).toInt();
  if (JOINMODE_value) {
    JOINMODE = "ABP";
  } else {
    JOINMODE = "OTAA";
  }
  Serial.print(F("JOINMODE:"));
  Serial.println(JOINMODE);

  // RXP
  RXP = read_recv_cmd("AT+CRXP?");
  Serial.print(F("RXP:"));
  Serial.println(RXP);

  // DEVEUI
  DEVEUI = read_recv_cmd("AT+CDEVEUI?");
  Serial.print(F("DEVEUI:"));
  Serial.println(DEVEUI);

  // APPEUI
  APPEUI = read_recv_cmd("AT+CAPPEUI?");
  Serial.print(F("APPEUI:"));
  Serial.println(APPEUI);

  // APPKEY
  APPKEY = read_recv_cmd("AT+CAPPKEY?");
  Serial.print(F("APPKEY:"));
  Serial.println(APPKEY);

  // FREQBANDMASK
  FREQBANDMASK = read_recv_cmd("AT+CFREQBANDMASK?");
  Serial.print(F("FREQBANDMASK:"));
  Serial.println(FREQBANDMASK);

  // ULDLMODE
  int ULDLMODE_value = (read_recv_cmd("AT+CULDLMODE?")).toInt();
  if (ULDLMODE_value == 1) {
    ULDLMODE = "Same frequency mode";
  } else {
    ULDLMODE = "Different frequency mode";
  }
  Serial.print(F("ULDLMODE:"));
  Serial.println(ULDLMODE);

  // CLASS
  int CLASS_value = (read_recv_cmd("AT+CULDLMODE?")).toInt();
  switch (CLASS_value) {
    case 0:
      CLASS = "ClassA";
      break;
    case 1:
      CLASS = "ClassB";
      break;
    case 2:
      CLASS = "ClassC";
      break;
  }
  Serial.print(F("CLASS:"));
  Serial.println(CLASS);
  Serial.println("--------------------------------------------------------");
}

//檢查LoRa模塊當前的狀態返回對應的數值
int return_lora_module_status_int() {
  int STATUS_value = (read_recv_cmd("AT+CSTATUS?")).toInt();
  switch (STATUS_value) {
    case 0:
      Serial.println(F("00-No data operation"));
      break;
    case 1:
      Serial.println(F("01-Data sending"));
      break;
    case 2:
      Serial.println(F("02-Data sending failed"));
      break;
    case 3:
      Serial.println(F("03-Data sent successfu"));
      break;
    case 4:
      Serial.println(F("04-JOIN succeeded (only in the first JOIN)"));
      break;
    case 5:
      Serial.println(F("05-– JOIN fails (only during the first JOIN proces)"));
      break;
    case 6:
      Serial.println(F("06-The network may be abnormal (Link Check result)"));
      break;
    case 7:
      Serial.println(F("07-Send data successfully, no downlink"));
      break;
    case 8:
      Serial.println(F("08-Send data successfully, there is downlink"));
      break;
    default:
      Serial.println(F("Error"));
      break;
  }
  return STATUS_value;
}

//讀取和篩選LoRa模塊返回的字符串
String read_recv_cmd(String AT_cmd) {
  int int_error = 0;
  String rev_data = "";
  String return_data = "";
  do {
    LoRa.println(AT_cmd);
    //Serial.println(AT_cmd);
    rev_data = get_LoRa_data();
    rev_data.replace("\r\n", "");
    //Serial.println("rev_data" + rev_data);
    if (rev_data.indexOf("ERROR") >= 0) {
      int_error = 1;
    } else {
      int_error = 0;
      int index = rev_data.indexOf(':') + 1;
      int index_end = rev_data.indexOf("OK");
      return_data = rev_data.substring(index, index_end);
      //Serial.println(return_data) ;
    }
  } while (int_error);
  return return_data;
}

//將字符串數轉成16進制
String String_to_Hex(String str_data) {
  String string_to_hex = "";
  for (int i = 0; i < str_data.length(); i++) {
    string_to_hex += String(str_data[i], HEX);
    delay(2);
  }
  return string_to_hex;
}

//將16進制數轉成字符串
String Hex_to_String(String hex_data) {
  String hex_to_string = "";
  for (int i = 0; i < hex_data.length(); i += 2) {
    hex_to_string += (char)strtol(hex_data.substring(i, i + 2).c_str(), NULL, 16);
    delay(2);
  }
  return hex_to_string;
}

//將16進制數轉成10進賬數
int Hex_to_Dec(String hex) {
  int dec = 0;
  if (hex.toInt() < 10) {
    dec = hex.toInt();
  } else {
    char c_hex[1];
    hex.toCharArray(c_hex, 2);
    dec = (c_hex[0] - 65) + 10;
  }
  return dec;
}

// LoRa模塊啟動檢查
int LoRa_Start(String data) {
  String rev_data;
  LoRa.println(data);
  rev_data = get_LoRa_data();
  //Serial.println("rev_data" + rev_data);
  if (rev_data.indexOf("Ebyte") >= 0 || rev_data.indexOf("ASR") >= 0 ) {
    return 1;
  }  else {
    return 0;
  }
}

//通過LoRa模塊向LoRa Gateway 發送字符串
void LoRa_Send_data(String data) {
  String rev_data = "";
  LoRa.println(data);
  Serial.print(data);
}

//從LoRa模塊讀取字符串
String get_LoRa_data() {
  delay(50);
  String rev_data = "";
  if (LoRa.available()) {
    while (LoRa.available()) {
      rev_data += char(LoRa.read());
      delay(1);
    }
  }
  return rev_data;
}
