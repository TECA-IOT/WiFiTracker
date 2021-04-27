# WiFiTracker
wifi mac google geolocalization api sigfox low power django traccar

* en este proyecto intento crear un pequeño rastreador para mascotas (mi gato) usando un servicio de geolocalizacion basado en estaciones de wifi. Para esto use un modulo ESP8266 para leer las direcciones mac de las redes wifi cercanas al rastreador y las envio a Internet por medio de un modulo sigfox (Tinyfox). Con estas mac hice query al api de Google maps para obtener las coordenadas GPS de mi rastreador/gato/mascota y finalmente las visualice en google maps.
* Con esto ya funcionando correctamente escribí un servidor en Django y lo instale en un VPS a modo de middleware para recibir los mensajes de sigfox y solicitar las coordenadas a Google. Como paso final reenvió estas coordenadas a una instancia de traccar ejecutante en el mismo VPS.



<insertar todos de portada aquí>
### materiales:
* esp8266. En este caso usare un wemos d1
* un tinyfox (radio sigfox)
* powerbank
* regulador LDO de 3.3v (opcional)
* batería de litio pequeña (opcional)
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957307.jpg" width="400">
### servicios:
* cuenta activa de sigfox
* servicio de geolocalizacion wifi se sigfox (opcional. Si tu servicio incluye esta funcion.)
* cuenta de Google cloud con billing habilitado (puedes habilitar con una tarjeta de crédito y obtener 30 días gratis)
### software:
* arduino IDE
* postman o cURL

### ensamblado:
* Habilitar el auto-reset para poder usar el modo DeepSleep. Hacer un puente entre el reset y el D0 del esp8266
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957301.jpg" width="400">
* conectar el tinyfix al ESP8266: Rx-D7   Tx-D6   Rst-D5  gnd y 3.3v a sus respectivos pines.
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957161.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957157.jpg" width="400">
* opcionalmente agregar una batería conectada  a gnd y 5v. No conectar el cable USB y la batería al mismo tiempo!
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957148.jpg" width="400">
* <insertar fotos del ensamblado>
### Programación:
* conectamos el modulo a la PC y descargamos el siguiente scketch.
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957152.jpg" width="400">
  
```javascript
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

```

### backend sigfox:
* una ves que el modulo esta transmitiendo podemos ir al backend de sigfox y ver los mensajes.
* como podemos ver el mensaje “88de7c109d806032b150b67e” representa a las dos mac mas cercanas, es decir “88:d e:7c:10:9d:80” y “60:32:b1:50:b6:7e”.
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/backend.png" width="400">
* el siguiente paso sera usar un servicio en linea para obtener las coordenadas de esas redes wifi

existen varios servicios de este tipo como el de mozilla, here.com, google maps, etc. en este caso usare google.

### google maps api:
* https://developers.google.com/maps/documentation/geolocation/get-api-key?authuser=1
* en este enlace encontraremos información detallada del api de google maps de geolocalizacion basada en redes wifi y en torres de telefonía celular. Para hacer suo de esta API seguiremos los pasos:

crear cuenta
* https://console.cloud.google.com/home/dashboard
crearemos una cuenta de google cloud
* crear proyecto
crearemos un proyecto nuevo
<foto>

* habilitar billing  https://console.cloud.google.com/billing
debemos habilitar la facturacion obligatoriamente para usar esa api. Necesitamos ingresar datos de una tarjeta de credito activa y obtendremos unos dias gratis suficientes para realizar pruebas.
<foto>
* habilitar el geolocation api. https://console.cloud.google.com/apis/library/geolocation.googleapis.com 
buscamos la api de geolocalizacion y la habilitamos en nuestro proyecto.

* Finalmente obtener el KEY https://console.cloud.google.com/apis/credentials 
ahora solo nos falta obtener una key para poder usar el api. Copiamos la clave y la usaremos para resolver las coordenaas.

### solicitar coordenadas con curl:
* guardamos las macs en un archivo con el siguiente formato
```javascript
{
  "considerIp": false,
  "wifiAccessPoints": [
    {
      "macAddress": "88:de:7c:10:9d:80",
    },
    {
      "macAddress": "60:32:b1:50:b6:7e",
    }
  ]
}
```
* con el nombre macs.json ejecutamos un query con postman o con cURL
* $ curl -d @macs.json -H "Content-Type: application/json" -i "https://www.googleapis.com/geolocation/v1/geolocate?key=YOUR_API_KEY"
* y recibimos el resultado:
```javascript
{
  "location": {
    "lat": -12.084xxx,
    "lng": -77.0475xxx
  },
  "accuracy": 150
}
```
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/cURL_censurado.png" width="400">
## visualizar:
* reemplazamos las coordenadas en esta url de google maps y copiamos las coordenadas en el webbrowser: https://www.google.com/maps/@-12.0847800,-77.0475100,16z
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/visualizar.png" width="400">

## LowPower:
* ahora, si quiero poner este tracker en un gato debe ser ligero y la batería durar mucho tiempo por la tanto hay que mejorar el diseño del hardware hay que retirar del wemos: el chip USB, los dos transistores que se usar para programar y el regulador de voltaje para reemplazarlo por uno de mayor calidad (LDO).
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/modificaciones.jpg" width="400">
* Agregue un pulsador al gpio1 para poder re-programar el modulo de ser necesario.
* Agregar un condensador de 100uF entre gnd y 3.3v
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957280.jpg" width="400">
* pegar un modulo sobre el otro de modo que los pines queden uno frnete al otro y conectar los pines entre si
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957275.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957271.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957262.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957248.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957241.jpg" width="400">
* prepara un LDO de 3.3v con sus condensadores y una batería recargable
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957262.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957214.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957200.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957192.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957192.jpg" width="400">
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957188.jpg" width="400">
* si necesitamos reprogramarlo podemos soldar cables en tx, rx y gnd. Y mantener el pulsador presionado durante la programacion.
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/1619288957179.jpg" width="400">

## paso experto:
* Crear un middleware con python y django e integrar con traccar.
* Les dejo la parte relevante del codigo como ejemplo
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/middleware.png" width="400">
* resultado
* <img src="https://github.com/paulporto/WiFiTracker/blob/main/imagenes/traccar.png" width="400">
