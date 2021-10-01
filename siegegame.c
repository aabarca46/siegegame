/*
A character-based surround-the-opponent game.
Reads from nametable RAM to determine collisions, and also
to help the AI avoid walls.
For more information, see "Making Arcade Games in C".
*/

#include <stdlib.h>
#include <string.h>
#include <nes.h>
#include <joystick.h>
#include<time.h>
#include "neslib.h"

// VRAM buffer module
#include "vrambuf.h"
//#link "vrambuf.c"

// link the pattern table into CHR ROM
//#link "chr_generic.s"

#define COLS 32
#define ROWS 27


void __fastcall__famitone_update(void);
//#link "famitone2.s"
//#link "prime_music.s"

extern char prime_music[];
// read a character from VRAM.
// this is tricky because we have to wait
// for VSYNC to start, then set the VRAM
// address to read, then set the VRAM address
// back to the start of the frame.
byte getchar(byte x, byte y) {
  // compute VRAM read address
  word addr = NTADR_A(x,y);
  // result goes into rd
  byte rd;
  // wait for VBLANK to start
  ppu_wait_nmi();
  // set vram address and read byte into rd
  vram_adr(addr);
  vram_read(&rd, 1);
  // scroll registers are corrupt
  // fix by setting vram address
  vram_adr(0x0);
  return rd;
}

void cputcxy(byte x, byte y, char ch) {
  vrambuf_put(NTADR_A(x,y), &ch, 1);
}

void cputsxy(byte x, byte y, const char* str) {
  vrambuf_put(NTADR_A(x,y), str, strlen(str));
}

void clrscr() {
  vrambuf_clear();
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 32*28);
  vram_adr(0x0);
  ppu_on_bg();
}

typedef struct {
  byte x;
  byte y;
  byte dir;
  word score;
  char head_attr;
  char tail_attr;
  int collided:1;
  int token_collected:1;
  int human:1;
} Player;

Player players[2];

byte attract;
byte gameover;
byte frames_per_move;

#define START_SPEED 5
#define MAX_SPEED 5
#define MAX_SCORE 7

///////////
int randR(){
  
  int randr = ROWS - rand() % 26;
  if(randr > ROWS-1 || randr == 1 || randr == 2)
    randR();
  else
  return randr;
}

int randC(){
  int randc = COLS - rand() % 31;
  if(randc > COLS-1 || randc == 1 || randc == 2)
    randC();
  else
  return randc;
}

const char BOX_CHARS[8] = { '+','+','+','+','-','-','!','!' };

void draw_box(byte x, byte y, byte x2, byte y2, const char* chars) {
  byte x1 = x;
  cputcxy(x, y, chars[2]);
  cputcxy(x2, y, chars[3]);
  cputcxy(x, y2, chars[0]);
  cputcxy(x2, y2, chars[1]);
  while (++x < x2) {
    cputcxy(x, y, chars[5]);
    cputcxy(x, y2, chars[4]);
  }
  while (++y < y2) {
    cputcxy(x1, y, chars[6]);
    cputcxy(x2, y, chars[7]);
  }
}

void draw_playfield() {
  draw_box(1,2,COLS-2,ROWS-1,BOX_CHARS);
  cputcxy(18,1,players[0].score+'0');
  
    cputsxy(12,1,"PLYR1:");
}

typedef enum { D_RIGHT, D_DOWN, D_LEFT, D_UP } dir_t;
const char DIR_X[4] = { 1, 0, -1, 0 };
const char DIR_Y[4] = { 0, 1, 0, -1 };

void init_game() {
  memset(players, 0, sizeof(players));
  players[0].head_attr = 0xB1;
  players[1].head_attr = 0x10;
  players[0].tail_attr = 0x06;
  frames_per_move = START_SPEED;
}

void reset_players() {
  players[0].x = players[0].y = 5;
  players[0].dir = D_RIGHT;
  players[1].x = randC();
  players[1].y = randR();
  players[0].collided = 0;
  players[0].token_collected = 0;
}
               
void draw_player(Player* p) {
  cputcxy(p->x, p->y, p->head_attr);
}

void move_player(Player* p) {
  cputcxy(p->x, p->y, p->tail_attr);
  p->x += DIR_X[p->dir];
  p->y += DIR_Y[p->dir];
  
  if (getchar(p->x, p->y) == 0x10)
    p->token_collected = 1;
  if (getchar(p->x, p->y) == '-' || getchar(p->x, p->y) == '!' 
      || getchar(p->x, p->y) == 0x06)
    p->collided = 1;
  draw_player(p);
}

void spawn_item(){
  players[1].x = randC();
  players[1].y = randR();
  players[0].collided = 0;
  players[0].token_collected = 0;
  draw_player(&players[1]);
}
               
void movement(Player* p) {
  byte dir = 0xff;
  byte joy;
  joy = joy_read (JOY_1);
  // start game if attract mode
  if (attract && (joy & JOY_START_MASK))
    gameover = 1;
  // do not allow movement unless human player
  if (!p->human) return;
  if (joy & JOY_LEFT_MASK) dir = D_LEFT;
  if (joy & JOY_RIGHT_MASK) dir = D_RIGHT;
  if (joy & JOY_UP_MASK) dir = D_UP;
  if (joy & JOY_DOWN_MASK) dir = D_DOWN;
  // don't let the player reverse
  if (dir < 0x80 && dir != (p->dir ^ 2)) {
    p->dir = dir;
  }
}

void make_move() {
  byte i;
  for (i=0; i<frames_per_move; i++) {
    movement(&players[0]);
    vrambuf_flush();
  }
  move_player(&players[0]);
}

void declare_winner(byte winner) {
  byte i;
  clrscr();
  for (i=0; i<ROWS/2-3; i++) {
    draw_box(i,i,COLS-1-i,ROWS-1-i,BOX_CHARS);
    vrambuf_flush();
  }
  music_stop();
  cputsxy(12,10,"Score:");
  cputcxy(12+7, 10, '0'+winner);
  vrambuf_flush();
  delay(75);
  gameover = 1;
}

#define AE(tl,tr,bl,br) (((tl)<<0)|((tr)<<2)|((bl)<<4)|((br)<<6))

// this is attribute table data, 
// each 2 bits defines a color palette
// for a 16x16 box
const unsigned char Attrib_Table[0x40]={
AE(3,3,1,0),AE(3,3,0,0),AE(3,3,0,0),AE(3,3,0,0), AE(3,3,0,0),AE(3,3,0,0),AE(3,3,0,0),AE(3,3,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
};

/*{pal:"nes",layout:"nes"}*/
const unsigned char Palette_Table[16]={ 
  0x00,
  0x01,0x28,0x31,0x00,
  0x04,0x24,0x34,0x00,
  0x09,0x29,0x39,0x00,
  0x06,0x26,0x36
};

// put 8x8 grid of palette entries into the PPU
void setup_attrib_table() {
  vram_adr(0x23c0);
  vram_write(Attrib_Table, 0x40);
}

void setup_palette() {
  int i;
  // only set palette entries 0-15 (background only)
  for (i=0; i<16; i++)
    pal_col(i, Palette_Table[i] ^ attract);
}

void play_round() {
  ppu_off();
  setup_attrib_table();
  setup_palette();
  clrscr();
  draw_playfield();
  reset_players();
  draw_player(&players[1]);
  while (1) {
    make_move();
    if (gameover) return; // attract mode -> start
    if (players[0].token_collected){
      	players[0].score++;
      	frames_per_move--;
      	ppu_off();
  	setup_attrib_table();
  	setup_palette();
  	clrscr();
      	vrambuf_flush();
  	draw_playfield();
      
       	spawn_item();
    }
    if(players[0].collided) break;
  }
      declare_winner(players[0].score);

}

void play_game() {
  gameover = 0;
  init_game();
  if (!attract)
    players[0].human = 1;
  while (!gameover) {
    play_round();
  }
}

void main() {
  famitone_init(prime_music);
  nmi_set_callback(famitone_update);
  music_play(0);
  joy_install (joy_static_stddrv);
  vrambuf_clear();
  set_vram_update(updbuf);
  while (1) {
    //attract = 1;
    //play_game();
    attract = 0;
    play_game();
  }
}
