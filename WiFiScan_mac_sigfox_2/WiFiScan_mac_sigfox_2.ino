#include "ESP8266WiFi.h"
#include <iostream>
#include <string>
#include <SoftwareSerial.h>
#include <Tinyfox.h> //libreria disponible en el library manager. o en https://github.com/TECA-IOT/Tinyfox

#define debugMode 0 // poner en 1 para probar el funcionamiento. 0 para ahorrar bateria!

#if debugMode
#define sleepCycle 2 //minutos. agotara los mensajes en 4 horas.
#else
#define sleepCycle 10 //minutos
#endif

//#define RXLED  D4//este es el led del nodemcu
SoftwareSerial mySerial(D6, D7); // RX, TX//esp8266
Tiny<SoftwareSerial, HardwareSerial> wisol(&mySerial, &Serial, D5, debugMode); //esp8266, D5 es el rst

char buff[30] = "";
char ssid[100];
int fails = 0;

void setup() {
  //pinMode(RXLED, OUTPUT);
  Serial.begin(115200);
  //wisol.begin(9600);
  Serial.println("");
  Serial.println("boot");
  
#if debugMode
  wisol.begin(9600);
  wisol.RST();
  Serial.println(wisol.ID());
  Serial.println(wisol.PAC());
  /*
    mySerial.println("AT$DR=922300000");
    delay(1000);
    mySerial.println("ATS400=<00000000><F0000000><0000001F>,63");
    delay(1000);
    mySerial.println("AT$WR");
    delay(1000);
    mySerial.println("AT$RC");
    Serial.println(wisol.PAC());*/
#endif

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  
  WiFi.forceSleepWake();//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);
  //Serial.println("Setup done");
}

void loop() {
  
#if debugMode
  Serial.println("scan start");
#endif

  //escanear las redes wifi cercanas
  int n = WiFi.scanNetworks();
  
#if debugMode
  Serial.println("scan done");
  delay(1000);
#endif

  if (n < 2) { //se necesita como mininmo 2 redes wifi para solicitar ubicacion. 
    fails++;
    if (fails > 4) {      ESP.deepSleep(sleepCycle * 60e6); delay(1000);   } //si despues de 4 intento no hay suficientes redes irse a dormir
    else {      delay(5000);      return;    }
  }
  else {
    
#if debugMode
    Serial.print(n);
    Serial.println(" networks found");
#endif
    //ahora necesitamos las 2 redes con mejos señal. las mas cercanas.
    long bestSignal = -500;
    long secondBestSignal = -500;
    int  BSi = -1;//bestsignal index
    int sBSi = -1;//second better

    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
#if debugMode
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.BSSIDstr(i));
      Serial.print(" | ");
      Serial.print(WiFi.RSSI(i)); //returns a LONG
      Serial.print("  ");
      Serial.print((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      Serial.print(" | ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" | ");
      Serial.flush();
#endif
      if (validSSID(i)) { //descartar las redes provenientes de telefonos compartiendo datos.

        if (WiFi.RSSI(i) > bestSignal) { //metodo burbuja simplificado. se queda con las dos mejores señales.
          sBSi = BSi;
          secondBestSignal = bestSignal;
          bestSignal = WiFi.RSSI(i);
          BSi = i;
        }
        if ( (WiFi.RSSI(i) > secondBestSignal) and (i != BSi) ) {
          secondBestSignal = WiFi.RSSI(i);
          sBSi = i;
        }
#if debugMode
        Serial.println("valid");
      }
      else {
        Serial.println("invalid");
#endif
      }
    }

    if ( BSi == -1 or sBSi == -1 ) {//si no hay dos redes validas no transmitir.
      fails++;
      if (fails > 4) {        ESP.deepSleep(sleepCycle * 60e6);   delay(1000);   } //si despues de 4 intentos no hay redees utiles irse a ddormir
      else {        delay(5000);        return;      }
    }

#if debugMode
    Serial.print("best: ");
    Serial.print(BSi);
    Serial.print(" & ");
    Serial.println(sBSi);
    Serial.write('\t');
    sprintf(buff, "%02x%02x%02x%02x%02x%02x\n", WiFi.BSSID(BSi)[0], WiFi.BSSID(BSi)[1], WiFi.BSSID(BSi)[2], WiFi.BSSID(BSi)[3], WiFi.BSSID(BSi)[4], WiFi.BSSID(BSi)[5]);
    Serial.print(buff);
    Serial.write('\t');
    sprintf(buff, "%02x%02x%02x%02x%02x%02x\n", WiFi.BSSID(sBSi)[0], WiFi.BSSID(sBSi)[1], WiFi.BSSID(sBSi)[2], WiFi.BSSID(sBSi)[3], WiFi.BSSID(sBSi)[4], WiFi.BSSID(sBSi)[5]);
    Serial.print(buff);
    Serial.write(0x0A);
#endif

    WiFi.mode( WIFI_OFF );
    WiFi.forceSleepBegin();// una ves tenemos las redes que vamos a transmitir apagamos el wifi para ahorrar bateria.

    //digitalWrite(RXLED, LOW);
    
    
    char msg[35];
    for (int i = 0; i < 35; i++) {
      msg[i] = 0;
    }
    //preparamos el payload para el mensaje sigfox
    snprintf (msg, 35, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", WiFi.BSSID(BSi)[0], WiFi.BSSID(BSi)[1], WiFi.BSSID(BSi)[2], WiFi.BSSID(BSi)[3], WiFi.BSSID(BSi)[4], WiFi.BSSID(BSi)[5], WiFi.BSSID(sBSi)[0], WiFi.BSSID(sBSi)[1], WiFi.BSSID(sBSi)[2], WiFi.BSSID(sBSi)[3], WiFi.BSSID(sBSi)[4], WiFi.BSSID(sBSi)[5]);
    Serial.println(msg);
    wisol.begin(9600);
    wisol.RST();//despierta al radio sigfox.
    delay(1000);
    wisol.SEND( String(msg) );//transmite


    //digitalWrite(RXLED, HIGH);
    wisol.SLEEP(); //luego de transmitir poner el modulo a dormir!
    Serial.println("*");
    ESP.deepSleep(sleepCycle * 60e6);// y tambien mandar el ESP a dormir para ahorrar bateria
    delay(1000);

  }
  delay(5000);
}

boolean validSSID(int i) { //descarta las redes provenientes de un telefono o router movil.
  sprintf(ssid, WiFi.SSID(i).c_str() );
  if (strstr(ssid, "Phone"    ) != NULL) return false;
  if (strstr(ssid, "phone"    ) != NULL) return false;
  if (strstr(ssid, "ndroid"   ) != NULL) return false;
  if (strstr(ssid, "Redmi"    ) != NULL) return false;
  if (strstr(ssid, "Xiaomi"   ) != NULL) return false;
  if (strstr(ssid, "Huawei"   ) != NULL) return false;
  if (strstr(ssid, "Samsung"  ) != NULL) return false;
  if (strstr(ssid, "Galaxy"   ) != NULL) return false;
  if (strstr(ssid, "PandaWiFi") != NULL) return false;
  return true;
}
