#include "gameboy_spoof.h"

int volatile CLOCK_PIN = 2;
int volatile MISO_PIN = 3;
int volatile MOSI_PIN = 4;
int volatile data = 0;
int volatile val = 0;
int ledStatus = 13;

unsigned volatile long t_last_rec = 0;
volatile byte out_buff = 0x00;
volatile int clock_cycle = 0;

volatile int data_counter = 0;
volatile connection_state_t connection_state = NOT_CONNECTED;
volatile int trade_center_state = INIT;


struct pokemon pk_test = {
  //Index Number
  PKI_MEWTWO,
  //HP
  300,
  //PC Level (Redundant, same as Level
  1,
  //Status Flags
  STA_BURNED,
  //Type 1
  TYP_PSYCHIC,
  //Type 2
  TYP_PSYCHIC,
  //Catch/Held Item
  0,
  //Move 1
  MOV_HYPER_BEAM,
  //Move 2
  MOV_CLAMP,
  //Move 3
  MOV_SPORE,
  //Move 4
  MOV_BLIZZARD,
  //Original Trainer ID Number
  256,
  //EXP
  100,
  //HP EV Data
  0xFFFF,
  //Attack EV Data
  0xFFFF,
  //Defense EV Data
  0xFFFF,
  //Speed EV Data
  0xFFFF,
  //Special EV Data
  0xFFFF,
  //IV Data
  0xFFFF,
  //Move 1 PP
  0,
  //Move 2 PP
  0,
  //Move 3 PP
  0,
  //Move 4 PP
  0,
  //Level
  5,
  //Max HP
  40,
  //Attack
  0xFFFF,
  //Defense
  0xFFFF,
  //Speed
  0xFFFF,
  //Special
  0xFFFF
};

struct trade trade_test = {
  "Grant     ",
  1,
  PKI_MEWTWO,
  PKI_MEWTWO,
  PKI_MEWTWO,
  PKI_MEWTWO,
  PKI_MEWTWO,
  PKI_MEWTWO,
  PKI_MEWTWO,
  pk_test,
  "Mewtwo    ",
  "Mewtwo    ",
  "Mewtwo    ",
  "Mewtwo    ",
  "Mewtwo    ",
  "Mewtwo    ",
  "Mewtwo    ",
  "Grant     ",
  "Grant     ",
  "Grant     ",
  "Grant     ",
  "Grant     ",
  "Grant     ",
  "Grant     "
};

void setup() 
{
  pinMode(MOSI_PIN, INPUT);
  digitalWrite( MOSI_PIN, HIGH);  
  pinMode(MISO_PIN, OUTPUT);
  pinMode(ledStatus, OUTPUT);
  digitalWrite(ledStatus, LOW);
  digitalWrite(MISO_PIN, LOW);
  pinMode(CLOCK_PIN, INPUT);
  digitalWrite(CLOCK_PIN, HIGH);
  Serial.begin(9600);
  attachInterrupt( 0, clock_interrupt, RISING );
}


void clock_interrupt(void) {
  byte in;
  unsigned long t = 0;
  if(t_last_rec > 0) {
    if( micros() - t_last_rec > 120 ) {
      clock_cycle = 0;
      val = 0;
      in = 0x00;
    }
  }
  data = digitalRead(MOSI_PIN);
  if(data == HIGH){
    val |= ( 1 << (7-clock_cycle) );
    in |= ( 1 << (7-clock_cycle) );
  }
  if(clock_cycle == 7) {
    out_buff = handle_in_byte((byte)val);
    val = 0;
    in = 0x00;
    clock_cycle = -1;
  }
  
  clock_cycle++;
  t_last_rec = micros();
  while( ((digitalRead(CLOCK_PIN) | CLOCK_PIN) & CLOCK_PIN)  == 0);
  digitalWrite(MISO_PIN, out_buff & 0x80 ? MISO_PIN : 0);
  out_buff = out_buff << 1;
}
byte handle_in_byte(byte in) {
  byte send = 0x00;
  switch(connection_state) {
  case NOT_CONNECTED:
    if(in == PKMN_MASTER)
      send = PKMN_SLAVE;
    else if(in == PKMN_BLANK)
      send = PKMN_BLANK;
    else if(in == PKMN_CONNECTED) {
      send = PKMN_CONNECTED;
      connection_state = CONNECTED;
                        digitalWrite(ledStatus, HIGH);
    }
    break;

  case CONNECTED:
    if(in == PKMN_CONNECTED)
      send = PKMN_CONNECTED;
    else if(in == PKMN_TRADE_CENTER)
      connection_state = TRADE_CENTER;
    else if(in == PKMN_COLOSSEUM)
      connection_state = COLOSSEUM;
    else if(in == PKMN_BREAK_LINK || in == PKMN_MASTER) {
      connection_state = NOT_CONNECTED;
      send = PKMN_BREAK_LINK;
                        digitalWrite(ledStatus, LOW);
    } else {
      send = in;
    }
    break;

  case TRADE_CENTER:
    if(trade_center_state == INIT && in == 0x00) {
      if(data_counter++ == 5) {
        trade_center_state = READY_TO_GO;
      }
      send = in;
    } else if(trade_center_state == READY_TO_GO && (in & 0xF0) == 0xF0) {
      trade_center_state = SEEN_FIRST_WAIT;
      send = in;
    } else if(trade_center_state == SEEN_FIRST_WAIT && (in & 0xF0) != 0xF0) {
      send = in;
      data_counter = 0;
      trade_center_state = SENDING_RANDOM_DATA;
    } else if(trade_center_state == SENDING_RANDOM_DATA && (in & 0xF0) == 0xF0) {
      if(data_counter++ == 5) {
        trade_center_state = WAITING_TO_SEND_DATA;
      }
      send = in;
    } else if(trade_center_state == WAITING_TO_SEND_DATA && (in & 0xF0) != 0xF0) {
      data_counter = 0;
      send = PKMN_DATA[data_counter++];
      trade_center_state = SENDING_DATA;
    } else if(trade_center_state == SENDING_DATA) {
      send = PKMN_DATA[data_counter++];
      if(data_counter == 415) {
        trade_center_state = DATA_SENT;
      }
    } else {
      send = in;
    }
    break;

  case COLOSSEUM:
    send = in;
    break;

  default:
    send = in;
    break;
  }

  return send;
}

void loop() 
{ 
  struct trade* trade_test_ptr = &trade_test;
  uint8_t* trade = 0;
  trade = gen_trade(trade_test_ptr);
  dump_trade(trade);
  while(1)
  {
    delay(1);
  }
}


void dump_trade(uint8_t* trade)
{
  for(int i = 0; i < 415; i++)
  {
    Serial.print(i);
    Serial.print(":");
    Serial.println(trade[i]);
  }
}

uint8_t* gen_pokemon(struct pokemon *in_poke)
{
  //The final buffer to hold the whole pokemon
  uint8_t* data; 
  data = (uint8_t*)malloc(44);

  uint32_t tmp = 0;
  uint8_t byte_1 = 0;
  uint8_t byte_2 = 0;
  uint8_t byte_3 = 0;
    
  data[0x00] = in_poke->index;
  
  tmp = in_poke->hp;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF); 
  data[0x01] = byte_1;
  data[0x02] = byte_2;
  
  data[0x03] = in_poke->pc_lvl;
  data[0x04] = in_poke->status_cond;
  data[0x05] = in_poke->type1;
  data[0x06] = in_poke->type2;
  data[0x07] = in_poke->catch_or_held_item;
  data[0x08] = in_poke->mov1;
  data[0x09] = in_poke->mov2;
  data[0x0A] = in_poke->mov3;
  data[0x0B] = in_poke->mov4;

  tmp = in_poke->trainer_id;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x0C] = byte_1;
  data[0x0D] = byte_2;

  tmp = in_poke->exp_pts;
  byte_3 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x0E] = byte_1;
  data[0x0F] = byte_2;
  data[0x10] = byte_3;

  tmp = in_poke->hp_ev;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x11] = byte_1;
  data[0x12] = byte_2;

  tmp = in_poke->attack_ev;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x13] = byte_1;
  data[0x14] = byte_2;

  tmp = in_poke->defense_ev;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x15] = byte_1;
  data[0x16] = byte_2;

  tmp = in_poke->speed_ev;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x17] = byte_1;
  data[0x18] = byte_2;

  tmp = in_poke->special_ev;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x19] = byte_1;
  data[0x1A] = byte_2;

  tmp = in_poke->iv_data;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x1B] = byte_1;
  data[0x1C] = byte_2;
  
  data[0x1D] = in_poke->mov1_pp;
  data[0x1E] = in_poke->mov2_pp;
  data[0x1F] = in_poke->mov3_pp;
  data[0x20] = in_poke->mov4_pp;
  data[0x21] = in_poke->lvl;

  tmp = in_poke->max_hp;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x22] = byte_1;
  data[0x23] = byte_2;

  tmp = in_poke->attack_stat;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x24] = byte_1;
  data[0x25] = byte_2;

  tmp = in_poke->defense_stat;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x26] = byte_1;
  data[0x27] = byte_2;

  tmp = in_poke->speed_stat;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x28] = byte_1;
  data[0x29] = byte_2;

  tmp = in_poke->special_stat;
  byte_2 = (tmp & 0x000000FF); 
  tmp >>= 8;
  byte_1 = (tmp & 0x000000FF);
  data[0x2A] = byte_1;
  data[0x2B] = byte_2;

  return data;
}

uint8_t* gen_trade(struct trade *in_trade)
{
  //The final buffer to hold the whole trade
  uint8_t* data; 
  data = (uint8_t*)malloc(415);
  for(int i = 0; i < 10; i++)
  {
    data[i] = char_to_sprite_char(in_trade->trader_name[i]);
  }
  data[10] = 0;
  data[11] = in_trade->num_of_pokemon;
  data[12] = in_trade->pki1;
  data[13] = in_trade->pki2;
  data[14] = in_trade->pki3;
  data[15] = in_trade->pki4;
  data[16] = in_trade->pki5;
  data[17] = in_trade->pki6;
  data[18] = in_trade->pki7;
  uint8_t* pokemon1_data = gen_pokemon(&in_trade->poke1);
  for(int i = 0; i < 44; i++)
  {
    data[19+i] = pokemon1_data[i];
  }
  free(pokemon1_data);
  //Right now I don't want to try and trade 7 pokemon at once, so I'm just going to fill the rest of this with garbage data
  for(int i = 63; i < 283; i++)
  {
    data[i] = 85;
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+283] = char_to_sprite_char(in_trade->trainer_name1[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+294] = char_to_sprite_char(in_trade->trainer_name2[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+305] = char_to_sprite_char(in_trade->trainer_name3[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+316] = char_to_sprite_char(in_trade->trainer_name4[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+327] = char_to_sprite_char(in_trade->trainer_name5[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+338] = char_to_sprite_char(in_trade->trainer_name6[i]);
  }
  for(int i = 0; i < 10; i++)
  {
    data[i+349] = char_to_sprite_char(in_trade->poke_name1[i]);
  }
  data[359] = 0x50;
  //Again, don't want to deal with the rest of the pokemon so I'm zeroing this out with terminators
  for(int i = 360; i < 415; i++)
  {
    data[i] = 0x50;
  }
}

byte char_to_sprite_char(char in)
{
  //All the numbers
  if(in >= '1' && in <= '2')
  {
    return 0xF6 - ('1' - in);
  }
  //All the uppercase letters
  if(in >= 'A' && in <= 'Z')
  {
    return 0x80 - ('A' - in);
  }
  //All the lower case letters
  if(in >= 'a' && in <= 'z')
  {
    return 0xA0 - ('a' - in);
  }
  //All the other characters
  if(in == ' ')
  {
    return 0x7F;
  }
  if(in == '-')
  {
    return 0xE3;
  }
  if(in == '?')
  {
    return 0xE6;
  }
  if(in == '!')
  {
    return 0xE7;
  }
  if(in == '.')
  {
    return 0xE8;
  }
  if(in == ',')
  {
    return 0xF4;
  }
  //If there's bad input, return a question mark
  else
  {
    return 0xE6;
  }
}

//There is NO out of bounds checking on the data_block being modified in this function. Be careful!
void string_to_sprite_chars(char *in, byte *data_block)
{
  for(int i = 0; in[i] != '\0'; i++)
  {
    data_block[i] = char_to_sprite_char(in[i]);
  }
  return;
}
