#include <Arduino.h>

/*
Se estiver configurado para 400 no dip switch (ON ON ON ON _ _ _ _) e o valor em sw_conf for mantido em 400, teremos 1 volta completa. 
Se a configuração for 800 (OFF ON ON ON _ _ _ _) e for mantido 400, será dada meia volta. Para uma volta completa na metade da velocidade inicial, use 800 e OFF ON ON ON _ _ _ _.
Lembre-se: A tabela mostra pulsos por revolução, então podemos controlar velocidade e avanço.


WIRING (UNO Leonardo etc) e Control Signal:
Arduino / Drive
D5     ---> ENA-   (bit 0 no PCF8575)
VCC    ---> ENA+
GND    ---> DIR-    
D6     ---> DIR+   (bit 1 no PCF8575)
D7     ---> PUL-   (bit 2 no PCF8575)
VCC    ---> PUL+

-------------- PCF8575 ----------------------
Bits   |   Driver
0      |   ENA-
1      |   DIR+
2      |   PUL-

3      | SIGNAL para EOC
4      | Leitura do EOC  - TODO: Deixar leitura no segundo byte?

Da MCU:
VCC    |   ENA+
GND    |   DIR-
VCC    |   PUL+


SERIAL 2: 16,17 Mega2560

definição de mensagem:

*/


/*
TODO: analisar a direcao para saber se deve ignorar o fim de curso, senão não voltará na direção oposta
TODO: colocar HC12 pra se comunicar com RPi
*/
//String opts[3] = {"left", "right", "stop"};

/*
msg example:
0x5e 0x01 0x07 0x01 0x0e 0x30 0x0a
Results in:
start: ^
addr: 1
sum: total bytes, including next byte ('\n')
steps: (1<<8) | (14<<0)
side:  left
end:   \n
270 steps left.
*/

struct {
  const uint16_t addr    = 0x01; //receiver addr
  const uint16_t start   = 0x5e; //start char  message
  const uint16_t end     = 0x0a; //end char message
  const uint16_t left    = 0x30; //run left char message
  const uint16_t right   = 0x31; //run right char message
  const uint16_t stop    = 0x32; //stop char message
  const uint16_t msg_len = 0x05; //message lenght (5 bytes - 2 for steps, 1 for direction or stop, start byte, end byte)
  uint16_t steps         = 0x00; //read from HC12 always
  int command            = 0x32; //initial value is 'stop'
} hc12attr;

enum {EN = 5, DIR = 6, PUL = 7};
enum {hc12_start, hc12_addr, hc12_sum, hc12_msb_step, hc12_lsb_step, hc12_comm, hc12_end};
enum {ramp_from,ramp_to};

int i                  = 0;
uint8_t debug          = 1;
uint16_t msg_array[10] = {0};
uint16_t msg_len       = 0;
uint32_t RAMP_FROM     = 20000;
uint32_t RAMP_TO       = 50;
uint32_t sw_conf[2]    = {RAMP_FROM,RAMP_TO}; //Configuração de pulsos, conforme tabela sobre o drive. No exemplo, estamos dando 1 volta completa.
bool ends              = false;

void setup(){
    Serial.begin(9600);
    Serial2.begin(9600);
    pinMode(EN,OUTPUT);
    pinMode(DIR,OUTPUT);
    pinMode(PUL,OUTPUT);
}

void clearMsgArray(){
  memset(msg_array,0,sizeof(msg_array));
  msg_len = 0;
}

void ramp(){
  sw_conf[ramp_from] = sw_conf[ramp_from] - sw_conf[ramp_to] > sw_conf[ramp_to] ? sw_conf[ramp_from] - sw_conf[ramp_to] : sw_conf[ramp_to];
}

void turnLeft(){
    if (debug == 1 && msg_array[hc12_comm] == hc12attr.left){
      Serial.println("left");
      Serial2.println("LEFT");
    }
    if (hc12attr.steps > 0){
      for (uint16_t k=1;k<hc12attr.steps;k++){
        digitalWrite(EN,HIGH);
        digitalWrite(DIR,HIGH);
        digitalWrite(PUL,HIGH);
        delayMicroseconds(sw_conf[ramp_from]);
        digitalWrite(PUL,LOW);
        delayMicroseconds(sw_conf[ramp_from]);
        Serial.print("<- ");
      }
      clearMsgArray();
    }
}

void turnRight(){
    if (debug == 1 && msg_array[hc12_comm] == hc12attr.right){
      Serial.println("right");
      Serial2.println("right");
    }
    if (hc12attr.steps > 0){
      for (uint16_t k=1;k<hc12attr.steps;k++){
        digitalWrite(EN,HIGH);
  
        digitalWrite(DIR,LOW);
        digitalWrite(PUL,HIGH);
        delayMicroseconds(sw_conf[ramp_from]);
        digitalWrite(PUL,LOW);
        delayMicroseconds(sw_conf[ramp_from]);
        Serial.print("-> ");
      }
      clearMsgArray();
    }       
}

/*
msg example:
0x5e 0x01 0x07 0x01 0x0e 0x30 0x0a
Results in:
start: ^
addr: 1
sum: total bytes, including next byte ('\n')
steps: (1<<8) | (14<<0)
side:  left
end:   \n
270 steps left.
*/

void setStepsAndCommand(){
  hc12attr.steps   = (msg_array[3]<<8)|(msg_array[4]<<0);
  hc12attr.command = msg_array[5];
  
  Serial.println(hc12attr.steps);
}

void hc12decodeBytes(){
  if (Serial2.available()){
    i = 0;
    memset(msg_array,0,sizeof(msg_array));
  }
  
  while (Serial2.available()){
    uint16_t buf = Serial2.read();    

    if (buf != hc12attr.start && i == 0){
      Serial.println("noise");
      break;
    }
    
    msg_array[i] = buf;
    msg_len     += 1;
    
    Serial.print("buf - msg_array ");
    Serial.print(buf);
    Serial.print(" ");
    Serial.println(msg_array[i]);

    i+=1;
  } 
  
  if (msg_array[hc12_sum] == msg_len && msg_array[hc12_sum] != 0){
    Serial.println("tamanho correto");
    setStepsAndCommand();  
  }
  delay(1000);
}

void moveMotor(){
  if (msg_array[hc12_comm] == hc12attr.left){
    ramp();
    turnLeft();
    Serial.println("<-");
  }  
  else if (msg_array[hc12_comm] == hc12attr.right){
    ramp(); 
    turnRight();
    Serial.println("->");
  }
}

void run(){
  hc12decodeBytes();

  if (msg_array[hc12_comm] != hc12attr.left && msg_array[hc12_comm] != hc12attr.right){
    digitalWrite(EN,LOW);
    if (debug == 1 && msg_array[hc12_comm] != 0){
      Serial2.println("STOP");     
    }
    sw_conf[ramp_from] = RAMP_FROM;
    clearMsgArray();
  }
  moveMotor();
}

void loop(){
    run();
}