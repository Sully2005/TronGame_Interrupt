
	
#include <unistd.h>
#include <stdio.h>

// if using CPUlator, you should copy+paste contents of the file below instead of using #include
/*******************************************************************************
 * This file provides address values that exist in the DE10-Lite Computer
 * This file also works for DE1-SoC, except change #define DE10LITE to 0
 ******************************************************************************/

#ifndef __SYSTEM_INFO__
#define __SYSTEM_INFO__

#define DE10LITE 1 // change to 0 for CPUlator or DE1-SoC, 1 for DE10-Lite

/* do not change anything after this line */

#if DE10LITE
 #define BOARD				"DE10-Lite"
 #define MAX_X		160
 #define MAX_Y		120
 #define YSHIFT		  8
#else
 #define MAX_X		320
 #define MAX_Y		240
 #define YSHIFT		  9
#endif


/* Memory */
#define SDRAM_BASE			0x00000000
#define SDRAM_END			0x03FFFFFF
#define FPGA_PIXEL_BUF_BASE		0x08000000
#define FPGA_PIXEL_BUF_END		0x0800FFFF
#define FPGA_CHAR_BASE			0x09000000
#define FPGA_CHAR_END			0x09001FFF

/* Devices */
#define LED_BASE			0xFF200000
#define LEDR_BASE			0xFF200000
#define HEX3_HEX0_BASE			0xFF200020
#define HEX5_HEX4_BASE			0xFF200030
#define SW_BASE				0xFF200040
#define KEY_BASE			0xFF200050
#define JP1_BASE			0xFF200060
#define ARDUINO_GPIO			0xFF200100
#define ARDUINO_RESET_N			0xFF200110
#define JTAG_UART_BASE			0xFF201000
#define TIMER_BASE			0xFF202000
#define TIMER_2_BASE			0xFF202020
#define MTIMER_BASE			0xFF202100
#define RGB_RESAMPLER_BASE    		0xFF203010
#define PIXEL_BUF_CTRL_BASE		0xFF203020
#define CHAR_BUF_CTRL_BASE		0xFF203030
#define ADC_BASE			0xFF204000
#define ACCELEROMETER_BASE		0xFF204020

/* Nios V memory-mapped registers */
#define MTIME_BASE             		0xFF202100

#endif


typedef uint16_t pixel_t;

volatile pixel_t *pVGA = (pixel_t *)FPGA_PIXEL_BUF_BASE;

volatile uint32_t* jtag = (uint32_t*)JTAG_UART_BASE;

char get_jtag(volatile int * JTAG_UART_ptr)
{
int data;
data = *(JTAG_UART_ptr); // read the JTAG_UART data register
if (data & 0x00008000) // check RVALID to see if there is new data
return ((char)data & 0xFF);
else
return ('\0');
}

const pixel_t blk = 0x0000;
const pixel_t wht = 0xffff;
const pixel_t red = 0xf800;
const pixel_t grn = 0x07e0;
const pixel_t blu = 0x001f;

void delay( int N )
{
	for( int i=0; i<N; i++ ) 
		*pVGA; // read volatile memory location to waste time
}

volatile int *HEX3_HEX0 = (int*)HEX3_HEX0_BASE;

unsigned char hex_encoding[16] = {
	0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71  // F
	
};



/* STARTER CODE BELOW. FEEL FREE TO DELETE IT AND START OVER */



void drawPixel( int y, int x, pixel_t colour )
{
	*(pVGA + (y<<YSHIFT) + x ) = colour;
}

pixel_t makePixel( uint8_t r8, uint8_t g8, uint8_t b8 )
{
	// inputs: 8b of each: red, green, blue
	const uint16_t r5 = (r8 & 0xf8)>>3; // keep 5b red
	const uint16_t g6 = (g8 & 0xfc)>>2; // keep 6b green
	const uint16_t b5 = (b8 & 0xf8)>>3; // keep 5b blue
	return (pixel_t)( (r5<<11) | (g6<<5) | b5 );
}

void rect( int y1, int y2, int x1, int x2, pixel_t c )
{
	for( int y=y1; y<y2; y++ )
		for( int x=x1; x<x2; x++ )
			drawPixel( y, x, c );
}

void draw_horizontal(int y1, int x1, int x2, pixel_t c)
{
	for(int i = x1; i < x2; i++){
		drawPixel(y1,i,c);
	}
}
void draw_vertical(int x1, int y1, int y2, pixel_t c)
{
	for(int i = y1; i < y2; i++){
		drawPixel(i, x1, c);
	}
}




typedef struct{
	int x;
	int y;
	
} Player;


int collision_detection(Player* p, int dx, int dy){
	
	
	int ny = p->y + dy;
	int nx = p->x + dx;
	pixel_t moving_to = *(pVGA + (ny << YSHIFT) + nx);
	
	return moving_to != blk;
}

int control_player(Player* player, int *dx, int *dy){

	pixel_t function_colour = makePixel(0,0,255);
	
	
	int initial_dx = *dx;
	int initial_dy = *dy;
	int move = get_jtag(jtag);

	if(move == 'a'){
		*dx = initial_dy;
		*dy = -initial_dx;
	}
	if(move == 'd'){
		*dx = -initial_dy;
		*dy = initial_dx;
	}
	if(move == 's') {
		*dx = -(*dx);
		*dy = -(*dy);
	}

	
	if (collision_detection(player, *dx, *dy) == 0) {
        player->x += *dx;
        player->y += *dy;
    } else {
        return 0;
    }
	
	drawPixel(player->y, player->x, function_colour);
	return 1;
	
	
	

	
}

int robot_movement(Player* robot, int* dx, int *dy) {
    
    

    pixel_t function_colour = makePixel(255,0,0);

    

        
        if (collision_detection(robot, *dx, *dy) != 0) {
            
            
            int left_dx = *dy;
            int left_dy = -(*dx);

            if (collision_detection(robot, left_dx, left_dy) == 0) {
                *dx = left_dx;
                *dy = left_dy;
            }
            else {
                
                int right_dx = -(*dy);
                int right_dy = *dx;

                if (collision_detection(robot, right_dx, right_dy) == 0) {
                    *dx = right_dx;
                    *dy = right_dy;
                }
                else {
                    
                    int back_dx = -(*dx);
                    int back_dy = -(*dy);

                    if (collision_detection(robot, back_dx, back_dy) == 0) {
                        *dx = back_dx;
                        *dy = back_dy;
                    }
                    else {
                      return 0;
                        
                    }
                }
            }
		}
        

        
        robot->x += *dx;
        robot->y += *dy;

        drawPixel(robot->y, robot->x, function_colour);
		return 1;
		
        
    
	
}

void reset_field(){
	
	pixel_t reset_colour = makePixel(0,0,0);
	for(int y = 0; y< MAX_Y; y++){
		for(int x = 0; x<MAX_X; x++){
			drawPixel(y,x,reset_colour);
			
		}
		
	}
	
}
void winner_winner(pixel_t colour){
	
	
	for(int y = 0; y< MAX_Y; y++){
		for(int x = 0; x<MAX_X; x++){
			drawPixel(y,x,colour);
			
		}
		
	}
	
}


void display_score(player, robot){
	unsigned int value = 0;
	
	value|= hex_encoding[player] << 0;
	value|= hex_encoding[robot] << 8;
	
	*HEX3_HEX0 = value;
	
	
}

void draw_arena(){

for (int y = 0; y < MAX_Y; y++){
		
		drawPixel(y,0,wht);
		drawPixel(y,MAX_X - 1a, wht);
	}
	for(int x = 0; x < MAX_X; x++){
		 
		drawPixel(0,x,wht);
		drawPixel(MAX_Y - 1,x, wht);
	}
	


}


int main()
{
	pixel_t colour;
	pixel_t function_colour;
	pixel_t background_colour;
	const int half_y = MAX_Y/2;
	const int half_x = MAX_X/2;
	printf( "start\n" );
	int player_score = 0;
	int robot_score = 0;
	rect( 0, MAX_Y, 0, MAX_X, blk );
	
	
	//Unclaimed pixel colour//
	background_colour = blk;
	pixel_t robot_colour = makePixel(255,0,0);
	pixel_t player_colour = makePixel(0,0,255);
	
	function_colour = makePixel(255,255,255);
	//draw_horizontal(20,20,35,function_colour);
	//draw_horizontal(30,40,70,function_colour);
	//draw_vertical(45,50,65,function_colour);
	//draw_vertical(55,70,85,function_colour);
	//rect(90,110,130,140,function_colour);
	//rect(110,25,50,60,function_colour);
	//draw_vertical(55,130,150,function_colour);
	//draw_vertical(15,60,90,function_colour);
	//rect(110,25,120,135,function_colour);
	//draw_horizontal(20,40,70,function_colour);
	//draw_horizontal(40,80,90, function_colour);
	//draw_horizontal(30,100,110,function_colour);
	//draw_vertical(100,5,25,function_colour);
	//draw_vertical(140,130,140,function_colour);
	
	
	
	draw_arena();
	Player robot;
	Player player;
	player.x = 80;
	player.y = 50;
	int pdx = 1;
	int pdy = 0;
	
	robot.x = 60;
	robot.y = 40;
	int rdx = 1;
	int rdy = 0;
	while(player_score < 9 && robot_score < 9){
	
	if(!control_player(&player, &pdx, &pdy)){
		robot_score++;
		reset_field();
		draw_arena();
		player.x = 80;
		player.y = 50;
		robot.x = 60;
		robot.y = 40;
		pdx = 1;
		pdy = 0;
		rdx = 1;
		rdy = 0;
		
	}
	if(!robot_movement(&robot, &rdx, &rdy)){
		player_score++;
		reset_field();
		draw_arena();
		robot.x = 60;
		robot.y = 40;
		player.x = 80;
		player.y = 50;
		pdx = 1;
		pdy = 0;
		rdx = 1;
		rdy = 0;
	}
	//control_player(&player, &pdx,&pdy);
	//robot_movement(&robot, &rdx, &rdy);
	display_score(player_score, robot_score);
	delay(500000);
	}
	
	if(robot_score > player_score){
		winner_winner(robot_colour);
	}
	else{
		winner_winner(player_colour);
	}
	//control_player(player);
	
	
	
	printf( "done\n" );

}