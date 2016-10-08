#include <ESP8266WiFi.h>  //https://github.com/ekstrand/ESP8266wifi
#include <WiFiUdp.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <TimeLib.h>      //https://github.com/PaulStoffregen/Time
#include <Ticker.h>

WiFiManager wifiManager;
WiFiServer server(80);
Ticker secondtick;
volatile int watchdogCount = 0;

/*
    void reset_config(void) { // apaga memora rom do esp
    wifiManager.resetSettings();
    delay(1500);
    ESP.reset();
  }
*/

//************* Função temporizada *************//
int horaLiga = 00;
int minutoLiga = 15;
int horaDesl = 00;
int minutoDesl = 20;

char hora[30];
char horaLigar[10];
char horaDesligar[10];

int Relay = 5;                // Pino Utilizado
uint8_t status_gpio = 0;      // define condição para gpio
uint8_t status_auto = false;  // define status do botão auto
#define stateRelay LOW        //estado do pino Relay

void TimedAction() {
  if (status_auto) {
    if (int(hour()) == (int)horaLiga && int(minute()) == (int)minutoLiga) {
      digitalWrite(Relay, HIGH);
      status_gpio = 1;
    } else if (int(hour()) == (int)horaDesl && int(minute()) == (int)minutoDesl) {
      digitalWrite(Relay, LOW);
      status_gpio = 0;
    }
  }
}

void ISRWatchdog(){
  watchdogCount++;
  if (watchdogCount > 30){
    Serial.println("Watchdog bite!");
    ESP.reset();
  }
}

//******************** NTP *******************//
static const char ntpServerName[] = "a.ntp.br"; //Servidor (pode ser a.ntp.br / b.ntp.br / c.ntp.br )
const int timeZone = -4; // Fuso horario

WiFiUDP Udp;
unsigned int localPort = 8888;
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  IPAddress ntpServerIP;
  while (Udp.parsePacket() > 0) ;
  Serial.println(F("Transmitindo NTP Request"));
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 3000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("Resposta recebida do NTP"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("Sem resposta do NTP");
  return 0;
}

void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
time_t prevDisplay = 0;

void RTCSoft() {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      TimedAction();
    }
  }
}

void VerifyTimeNow() {
  sprintf( hora, "%02d:%02d:%02d", hour(), minute(), second());
  sprintf( horaLigar, "%02d:%02d", horaLiga, minutoLiga);
  sprintf( horaDesligar, "%02d:%02d", horaDesl, minutoDesl);
}

///******************************************
void webpage() {
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println(F("Nova conexao requisitada..."));
  while (!client.available()) {
    delay(1);
  }
  Serial.println (F("Nova conexao OK..."));
  String req = client.readStringUntil('\r'); //Le a string enviada pelo cliente
  Serial.println(req);  //Mostra a string enviada
  client.flush();       //Limpa dados/buffer
  #define stateRelay LOW //estado do pino Relay

  if (req.indexOf(F("Auto_on")) != -1) {
    status_auto = true;
  } else if (req.indexOf(F("Auto_off")) != -1) {
    status_auto = false;
  } else if (req.indexOf(F("setHLu")) != -1) {
    horaLiga++; if (horaLiga > 23) {
      horaLiga = 00;
    }
  } else if (req.indexOf(F("setHLd")) != -1) {
    horaLiga--; if (horaLiga < 00) {
      horaLiga = 23;
    }
  } else if (req.indexOf(F("setMLu")) != -1) {
    minutoLiga = minutoLiga + 5;
    if (minutoLiga > 59) {
      minutoLiga = 00;
    }
  } else if (req.indexOf(F("setMLd")) != -1) {
    minutoLiga = minutoLiga - 5;
    if (minutoLiga < 0) {
      minutoLiga = 55;
    }
  } else if (req.indexOf(F("setHDu")) != -1) {
    horaDesl++;
    if (horaDesl > 23) {
      horaDesl = 00;
    }
  } else if (req.indexOf(F("setHDd")) != -1) {
    horaDesl--;
    if (horaDesl < 00) {
      horaDesl = 23;
    }
  } else if (req.indexOf(F("setMDu")) != -1) {
    minutoDesl = minutoDesl + 5;
    if (minutoDesl > 59) {
      minutoDesl = 00;
    }
  } else if (req.indexOf(F("setMDd")) != -1) {
    minutoDesl = minutoDesl - 5;
    if (minutoDesl < 00) {
      minutoDesl = 55;
    }
  } else if (req.indexOf(F("rele_on")) != -1) {
    digitalWrite(Relay, !stateRelay);
    status_gpio = 1;
  } else if (req.indexOf(F("rele_off")) != -1) {
    digitalWrite(Relay, stateRelay);
    status_gpio = 0;
  } else {
    Serial.println(F("Requisicao invalida"));
  }

  VerifyTimeNow();
  //Prepara a resposta para o cliente e carrega a pagina
  String buf = "";
  buf += "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n";
  buf += "<html lang=\"en\">";
  buf += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\r\n";
  buf += "<link href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.1/css/bootstrap.min.css' rel='stylesheet'></link>";
  buf += "<link rel=\"stylesheet\" href=\"http://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.6.3/css/font-awesome.min.css\">";
  buf += "<script src='https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js'></script>";
  buf += "<script src='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script>";
  buf += "<script src=\"https://labs.zonamaker.com.br/js/hora.js\"></script>";
  buf += "<title>WebServer ESP8266</title>";
  buf += "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:80%;} body{text-align: center;font-family:verdana;}</style>";
  buf += "</head>";
  buf += "<body onload='startTime()'><div class=\"panel panel-primary\">";
  buf += "<div class=\"panel-heading\"><h3>ESP8266 Web NTP</h3></div>";
  buf += "<div class=\"panel-body\">";
  buf += "<div id=\"txt\" style=\"font-weight:bold;\"></div></br>"; // DIV para hora
  buf += String(hora);
  //********botão lampada varanda**************
  buf += "<div class='container'>";
  buf += "<h4>Lampada da varanda</h4>";
  buf += "<div class='btn-group'>";
  //verificar como deixar automatico envia o comando
  if (status_auto)  // alterna botões on off
    buf += "<a href=\"?function=Auto_off\" class='btn btn-success'>Auto <i class=\"fa fa-toggle-on\" aria-hidden=\"true\"></i></a>";
  else
    buf += "<a href=\"?function=Auto_on\" class='btn btn-primary'>Auto <i class=\"fa fa-toggle-off\" aria-hidden=\"true\"></i></a>";
  //De acordo com o status da GPIO
  if (status_gpio)
    buf += "<a href=\"?function=rele_off\" class='btn btn-danger'><i class=\"fa fa-power-off\" aria-hidden=\"true\"></i> Desligar</a>";
  else
    buf += "<a href=\"?function=rele_on\" class='btn btn-success'><i class=\"fa fa-power-off\" aria-hidden=\"true\"></i> Ligar</a>";
  buf += "</div>";//btn group
  buf += "<p>Programado para ligar &#224;s <span class=\"label label-success\">";
  buf += String(horaLigar);
  buf += "</span> e desligar &#224;s <span class=\"label label-danger\">";
  buf += String(horaDesligar);
  buf += "</span></p></br>";
  buf += (F("<div class='btn-group'>"));
  buf += (F("<h4>Hora para ligar</h4>"));
  buf += (F("<a href=\"?function=setHLu\"><button type='button' class='btn btn-info' style='margin: 5px'>+1 h</button></a>"));
  buf += (F("<a href=\"?function=setHLd\"><button type='button' class='btn btn-info' style='margin: 5px'>-1 h</button></a>"));
  buf += (F("<a href=\"?function=setMLu\"><button type='button' class='btn btn-info' style='margin: 5px'>+5 min</button></a>"));
  buf += (F("<a href=\"?function=setMLd\"><button type='button' class='btn btn-info' style='margin: 5px'>-5 min</button></a>"));
  buf += (F("<h4>Hora para desligar</h4>"));
  buf += (F("<a href=\"?function=setHDu\"><button type='button' class='btn btn-info' style='margin: 5px'>+1 h</button></a>"));
  buf += (F("<a href=\"?function=setHDd\"><button type='button' class='btn btn-info' style='margin: 5px'>-1 h</button></a>"));
  buf += (F("<a href=\"?function=setMDu\"><button type='button' class='btn btn-info' style='margin: 5px'>+5 min</button></a>"));
  buf += (F("<a href=\"?function=setMDd\"><button type='button' class='btn btn-info' style='margin: 5px'>-5 min</button></a>"));
  buf += (F("</div> "));
  buf += "</div></div> ";//container
  //************************************
  buf += "</body>";
  buf += "</html>\n";
  VerifyTimeNow();
  client.print(buf);
  VerifyTimeNow();
  client.flush();
  client.stop();
  Serial.println(F("Cliente desconectado!"));
}

void setup() {
  WiFi.persistent(false);
  Serial.begin(115200);
  delay(250);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);
  secondtick.attach(1, ISRWatchdog);

  //Define o auto connect e o SSID do modo AP
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("ESPWebServer");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("Conectado"));
  server.begin();
  //************************hostname Config******************
  //#define HOSTNAME "ESP100"
  //String hostname(HOSTNAME);
  //wifi_station_set_hostname(strToChar(hostname));

  //********************NTP********************************
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println(F("Iniciando UDP"));
  Udp.begin(localPort);
  Serial.print(F("Porta local: "));
  Serial.println(Udp.localPort());
  Serial.println(F("Aguardando sincronia do NTP"));
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

}

void loop() {
  watchdogCount = 0;
  RTCSoft();  //sincroniza o relógio
  webpage();  //carrega o webserver
}
