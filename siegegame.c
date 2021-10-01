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
#include "neslib.h"
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

byte gameover;
byte frames_per_move;

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

//function displayes text
void cputcxy(byte x, byte y, char ch) {
  vrambuf_put(NTADR_A(x,y), &ch, 1);
}

//function displayes text
void cputsxy(byte x, byte y, const char* str) {
  vrambuf_put(NTADR_A(x,y), str, strlen(str));
}

//function clears screen when called
void clrscr() {
  vrambuf_clear();
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 32*28);
  vram_adr(0x0);
  ppu_on_bg();
}

//defines values for Player object
typedef struct {
  byte x;//x coordinate
  byte y;//y coordinate
  byte dir;//direction of movement
  word score;// score 
  char head_attr; //head sprite
  char tail_attr; // tail sprite
  int collided:1; // collision value
  int token_collected:1; //token value
} Player;

//player 0 = human player
//player 1 = item

//TODO add multiple tokens
Player players[2];

#define START_SPEED 13
#define MAX_SPEED 2
#define Max_SCORE 9

//random# generator for rows ensure item spawns in play zone
int randR(){
  
  int randr = ROWS - rand() % 25;
  if(randr > ROWS-3 || randr == 2 || randr == 3)
    randR();
  else
  return randr;
}

//random# generator for columns ensure item spawns in play zone
int randC(){
  int randc = COLS - rand() % 25;
  if(randc > COLS-3 || randc == 2 || randc == 3)
    randC();
  else
  return randc;
}

//character set for box
const char BOX_CHARS[8] = {0x8D,0x8E,0x87,0x8B,0x8C,0x83,0x85,0x8A };


//function draws borders
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

//draws score and calls function to draw_box
void draw_playfield() {
  draw_box(1,2,COLS-2,ROWS-1,BOX_CHARS);
  cputcxy(18,1,players[0].score+'0');
  
    cputsxy(12,1,"Score:");
}

//function allows player to make inputs
typedef enum { D_RIGHT, D_DOWN, D_LEFT, D_UP } dir_t;
const char DIR_X[4] = { 1, 0, -1, 0 };
const char DIR_Y[4] = { 0, 1, 0, -1 };


//function initialies game with player head and tail
// and adds token sprite
void init_game() {
  memset(players, 0, sizeof(players));
  frames_per_move = START_SPEED;
  //player attributes
  players[0].head_attr = 0xB1;
  players[0].tail_attr = 0x06;
  //token attributes
  players[1].head_attr = 0x10;
  
}
//function places player at start point and places first token
void reset_players() {
  players[0].x = players[0].y = 5;
  players[0].dir = D_RIGHT;
  players[0].collided = 0;
  players[0].token_collected = 0;
}

//function draws token and player
void draw_player(Player* p) {
  cputcxy(p->x, p->y, p->head_attr);
}

//function checks for collision by comparing sprite hex values
void check_for_collision(Player* p){
  //checks if player has collided with token by checking if token 
  //is in x,y location that player is moving 
  if (getchar(p->x, p->y) == 0x10)
    p->token_collected = 1;
  //checks if player has collided with wall or tail by check if
  //wall or tail will be in x,y coordinate 
  if (getchar(p->x, p->y) == 0x83 || getchar(p->x, p->y) == 0x85 
      || getchar(p->x, p->y) == 0x8A || getchar(p->x, p->y) == 0x8C
     	|| getchar(p->x, p->y) == 0x06)
    p->collided = 1;
  draw_player(p);
}

// takes input from player to move in either x or y direction
void move_player(Player* p) {
  cputcxy(p->x, p->y, p->tail_attr);
  p->x += DIR_X[p->dir];
  p->y += DIR_Y[p->dir];
  //calls function to check for collision
  check_for_collision(p);
  //draws player if no collision is found
  draw_player(p);
}

//function spawns a token in random x y coordinate and sets player collision to 0
void spawn_item(){
  players[1].x = randC();
  players[1].y = randR();
  players[0].collided = 0;
  players[0].token_collected = 0;
  draw_player(&players[1]);
}

//function allows player to input directions
void movement(Player* p) {
  byte dir = 0xff;
  byte joy;
  joy = joy_read (JOY_1);

  //takes player input and determines which direction player will move
  if (joy & JOY_LEFT_MASK) dir = D_LEFT;
  if (joy & JOY_RIGHT_MASK) dir = D_RIGHT;
  if (joy & JOY_UP_MASK) dir = D_UP;
  if (joy & JOY_DOWN_MASK) dir = D_DOWN;
  // checks if player is attempting to move back towards tail and rejects input
  if (dir < 0x80 && dir != (p->dir ^ 2)) {
    p->dir = dir;
  }
}

//function calls movement function and moves player
void make_move() {
  byte i;
  for (i=0; i<frames_per_move; i++) {
    movement(&players[0]);
    vrambuf_flush();
  }
  move_player(&players[0]);
}


//defines atribute tables
#define AE(tl,tr,bl,br) (((tl)<<0)|((tr)<<2)|((bl)<<4)|((br)<<6))

// this is attribute table data, 
// each 2 bits defines a color palette
// for a 16x16 box
const unsigned char Attrib_Table[0x40]={
AE(1,3,1,1),AE(0,0,1,1),AE(0,0,1,1),AE(0,0,1,1), AE(0,0,1,1),AE(0,0,1,1),AE(0,0,1,1),AE(0,0,1,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,0,1,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0), AE(0,0,0,0),AE(0,0,0,0),AE(0,0,0,0),AE(0,1,0,1),
AE(1,3,1,1),AE(0,0,1,1),AE(0,0,1,1),AE(0,0,1,1), AE(0,0,1,1),AE(0,0,1,1),AE(0,0,1,1),AE(0,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
};

// this is attribute table data for game over screen, 
// each 2 bits defines a color palette
// for a 16x16 box
const unsigned char Attrib_Table_over[0x40]={
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(0,0,0,0),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1), AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),AE(1,1,1,1),
};



//determines pallete colors
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
// put 8x8 grid of palette entries into the PPU for game over screen
void setup_attrib_table_game_over(){
  vram_adr(0x23c0);
  vram_write(Attrib_Table_over, 0x40);
}

//sets pallet colors
void setup_palette() {
  int i;
  for (i=0; i<16; i++)
    pal_col(i, Palette_Table[i]);
}

//function creates starting screen and takes player input to begin game
void title_screen(){
  ppu_off();
  setup_attrib_table();
  setup_palette();
  clrscr();
  draw_box(1,2,COLS-2,ROWS-1,BOX_CHARS);
  vrambuf_flush();
  cputsxy(11,9,"Tron-o-Thon");
  cputsxy(5,13,"Press Any key to Start!");
  cputsxy(2,19,"Created by Alessandro Abarca");
  cputsxy(7,21,"and Anthony Moreno");
  vrambuf_flush();
  music_stop();
}

//displays game over screen by clearing play screen
// and creating a box that moves to the center
// displays final score
void game_over(byte gameover) {
  byte i;
  clrscr();
  vrambuf_flush();
  setup_attrib_table_game_over();
  for (i=0; i<ROWS/2-3; i++) {
    draw_box(i,i,COLS-1-i,ROWS-1-i,BOX_CHARS);
    vrambuf_flush();
  }
  cputsxy(11,12,"Game Over");
  cputsxy(11,14,"Score:");
  cputcxy(18,14, '0'+gameover);
  vrambuf_flush();
  delay(75);
  gameover = 1;
  players[0].score = 0;
  frames_per_move = START_SPEED;
}

//transitions title screen to play screen by clearing 
//title screen and setting attribute table and pallete
void play() {
  vrambuf_flush();
  ppu_off();
  setup_attrib_table();
  setup_palette();
  clrscr();
  draw_playfield();
  reset_players();
  spawn_item();
  //creates a loop that can only be closed when player collides
  //with wall on any side, or tail
  // if player grabs token, screen refreshes and player enters
  // new round
  while (1) {
    make_move();
    
    //if player collects token reset screen and decrease frames
    // per move redraw play field
    if (players[0].token_collected){
      	players[0].score++;
      	frames_per_move--;
      	ppu_off();
  	setup_attrib_table();
  	setup_palette();
  	clrscr();
      	vrambuf_flush();
  	draw_playfield();
      //calls function to spawn token
       	spawn_item();
    }
    //ends game if player collides with wall or tail
    if(players[0].collided) break;
    //ends game if player reaches max score
    if(players[0].score == Max_SCORE) break;
  }
      game_over(players[0].score);
}

//initializes game start
void start_game() {
  vrambuf_clear();
  
  gameover = 0;
  init_game();

  while (!gameover) {
    play();
  }
}

void main() {
  famitone_init(prime_music);
  nmi_set_callback(famitone_update);
  music_play(0);
  // allows user to input movement commands
  joy_install (joy_static_stddrv);
  //clear screen and display title screen
  vrambuf_clear();
  set_vram_update(updbuf);
  
  //calls title screen function
  title_screen();
  
  //creates loop that only ends if player closes game
  while (1) {
    while(1){
    	byte joy;
  	joy = joy_read (JOY_1);
      if(joy)
        break;
    }
    start_game();
  }
}
