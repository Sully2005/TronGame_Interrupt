

	
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
 #define BOARD              "DE10-Lite"
 #define MAX_X      160
 #define MAX_Y      120
 #define YSHIFT       8
#else
 #define MAX_X      320
 #define MAX_Y      240
 #define YSHIFT       9
#endif


/* Memory */
#define SDRAM_BASE          0x00000000
#define SDRAM_END           0x03FFFFFF
#define FPGA_PIXEL_BUF_BASE     0x08000000
#define FPGA_PIXEL_BUF_END      0x0800FFFF
#define FPGA_CHAR_BASE          0x09000000
#define FPGA_CHAR_END           0x09001FFF

/* Devices */
#define LED_BASE            0xFF200000
#define LEDR_BASE           0xFF200000
#define HEX3_HEX0_BASE          0xFF200020
#define HEX5_HEX4_BASE          0xFF200030
#define SW_BASE             0xFF200040
#define KEY_BASE            0xFF200050
#define JP1_BASE            0xFF200060
#define ARDUINO_GPIO            0xFF200100
#define ARDUINO_RESET_N         0xFF200110
#define JTAG_UART_BASE          0xFF201000
#define TIMER_BASE          0xFF202000
#define TIMER_2_BASE            0xFF202020
#define MTIMER_BASE         0xFF202100
#define RGB_RESAMPLER_BASE          0xFF203010
#define PIXEL_BUF_CTRL_BASE     0xFF203020
#define CHAR_BUF_CTRL_BASE      0xFF203030
#define ADC_BASE            0xFF204000
#define ACCELEROMETER_BASE      0xFF204020

/* Nios V memory-mapped registers */
#define MTIME_BASE                  0xFF202100
#define CLOCK_RATE      100000000

#endif



typedef uint16_t pixel_t;

typedef struct{
    int x;
    int y;
} Player;



volatile pixel_t *pVGA = (pixel_t *)FPGA_PIXEL_BUF_BASE;
volatile uint32_t* jtag = (uint32_t*)JTAG_UART_BASE;
volatile uint32_t * keys = (uint32_t*)KEY_BASE;
volatile uint32_t * switches = (uint32_t*)SW_BASE;
volatile uint32_t *mtime_ptr = (uint32_t *) MTIMER_BASE;
volatile int *HEX3_HEX0 = (int*)HEX3_HEX0_BASE;
volatile int *led_base = (int*)LEDR_BASE;
volatile uint32_t *key_mask_ptr = (uint32_t *)(KEY_BASE + 0x8);

volatile uint32_t *key_edge_ptr = (uint32_t *)(KEY_BASE + 0xC);

volatile uint64_t PERIOD = (uint64_t)CLOCK_RATE; // 100M cycles (1 second). Adjust to 10M for 10fps.

const pixel_t blk = 0x0000;
const pixel_t wht = 0xffff;
const pixel_t red = 0xf800;
const pixel_t grn = 0x07e0;
const pixel_t blu = 0x001f;

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



volatile int player_score = 0;
volatile int robot_score = 0;
volatile int game_over = 0;
volatile int pdx = 1;
volatile int pdy = 0;
volatile int rdx = 1;
volatile int rdy = 0;
volatile int pending_turn;

pixel_t background_colour = blk;
pixel_t robot_colour = 0xF800; // Red
pixel_t player_colour = 0x001F; // Blue


Player player = {80, 50};
Player robot = {60, 40};


void game_state_isr(void);
void key_press_isr(void);

int control_player(Player* player, volatile int *dx, volatile int *dy);
int robot_movement(Player* robot, volatile int* dx, volatile int *dy);
void reset_field(void);
void draw_arena(void);
void display_score(int player, int robot);
void winner_winner(pixel_t colour);
void drawPixel( int y, int x, pixel_t colour );
pixel_t makePixel( uint8_t r8, uint8_t g8, uint8_t b8 );
int collision_detection(Player* p, int dx, int dy); 
void define_game_speed();



static void handler(void) __attribute__ ((interrupt ("machine")));
void handler(void)
{
    int mcause_value;
    __asm__ volatile( "csrr %0, mcause" : "=r"(mcause_value) );
    
    // Check for Machine Timer Interrupt (IRQ #7)
    // 0x80000007 means Interrupt bit (31) is set and Exception Code is 7
    if (mcause_value == 0x80000007) 
    {
        game_state_isr();
    }
	if(mcause_value == 0x80000012){
		key_press_isr();
	}
}



void setup_cpu_irqs( uint32_t new_mie_value )
{
    uint32_t mstatus_value, mtvec_value, old_mie_value;
    mstatus_value = 0b1000; // interrupt bit mask
    mtvec_value = (uint32_t) &handler; // set trap address
    
    __asm__ volatile( "csrc mstatus, %0" :: "r"(mstatus_value) ); // master irq disable
    __asm__ volatile( "csrw mtvec, %0" :: "r"(mtvec_value) );     // sets handler
    __asm__ volatile( "csrr %0, mie" : "=r"(old_mie_value) );
    __asm__ volatile( "csrc mie, %0" :: "r"(old_mie_value) );
    __asm__ volatile( "csrs mie, %0" :: "r"(new_mie_value) );     // reads old irq mask, removes old irqs, sets new irq mask
    __asm__ volatile( "csrs mstatus, %0" :: "r"(mstatus_value) ); // master irq enable
}

void set_mtimer(volatile uint32_t *time_ptr, uint64_t new_time64 )
{
    *(time_ptr+0) = (uint32_t)0;                  // prevent hi from increasing before setting lo
    *(time_ptr+1) = (uint32_t)(new_time64>>32);   // set hi part
    *(time_ptr+0) = (uint32_t)new_time64;         // set lo part
}

uint64_t get_mtimer( volatile uint32_t *time_ptr)
{
    uint32_t mtime_h, mtime_l;
    do {
        mtime_h = *(time_ptr+1); // read mtime-hi
        mtime_l = *(time_ptr+0); // read mtime-lo
    } while( mtime_h != *(time_ptr+1) );
    return ((uint64_t)mtime_h << 32) | mtime_l ;
}

void setup_mtimecmp()
{
    uint64_t mtime64 = get_mtimer( mtime_ptr );
    mtime64 = (mtime64/PERIOD+1) * PERIOD; // compute end of next time PERIOD
    set_mtimer( mtime_ptr+2, mtime64 );    // write first mtimecmp ("+2" == mtimecmp)
}



void drawPixel( int y, int x, pixel_t colour )
{
    *(pVGA + (y<<YSHIFT) + x ) = colour;
}

pixel_t makePixel( uint8_t r8, uint8_t g8, uint8_t b8 )
{
    const uint16_t r5 = (r8 & 0xf8)>>3; 
    const uint16_t g6 = (g8 & 0xfc)>>2; 
    const uint16_t b5 = (b8 & 0xf8)>>3; 
    return (pixel_t)( (r5<<11) | (g6<<5) | b5 );
}

void draw_arena(){
    for (int y = 0; y < MAX_Y; y++){
        drawPixel(y,0,wht);
        drawPixel(y,MAX_X - 1, wht);
    }
    for(int x = 0; x < MAX_X; x++){
        drawPixel(0,x,wht);
        drawPixel(MAX_Y - 1,x, wht);
    }
}

void reset_field(){
    pixel_t reset_colour = makePixel(0,0,0);
    pdx = 1;
    pdy = 0;
    rdx = 1;
    rdy = 0;
	pending_turn = 0;

    // Clear Screen
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

void display_score(int player, int robot){
    unsigned int value = 0;
    value|= hex_encoding[player & 0xF] << 0; // Added & 0xF to prevent overflow
    value|= hex_encoding[robot & 0xF] << 8;
    *HEX3_HEX0 = value;
}

void define_game_speed(){
   
    uint32_t sw = switches[0];
    
   
    if((sw & 0x1)){
        PERIOD = 100000000;
    }
    else if ((sw & 0x2)){
        PERIOD = 600000;
    }
    else if ((sw & 0x4)){
        PERIOD = 700000;
    }
    else if ((sw & 0x8)){
        PERIOD = 800000;
    }
    else if ((sw & 0x10)){
        PERIOD = 900000;
    }
    else if ((sw & 0x20)){
        PERIOD = 1000000;
    }
    else if ((sw & 0x40)){
        PERIOD = 2000000;
    }
    else if ((sw & 0x80)){
        PERIOD = 30000000;
    }
    else {
        PERIOD = 4000000;
    }
       
    
}



int collision_detection(Player* p, int dx, int dy){
    int ny = p->y + dy;
    int nx = p->x + dx;
    
    if (nx < 0 || nx >= MAX_X || ny < 0 || ny >= MAX_Y) return 1; 

    pixel_t moving_to = *(pVGA + (ny << YSHIFT) + nx);
    return moving_to != blk;
}


int control_player(Player* player, volatile int *dx, volatile int *dy){
    pixel_t function_colour = makePixel(0,0,255);
    static uint32_t prev_key_state = 0;
    
    int initial_dx = *dx;
    int initial_dy = *dy;
    
    
    uint32_t key_state = keys[0];
    
    
    if((key_state & 0x1) && !(prev_key_state & 0x1)){
        *dx = initial_dy;
        *dy = -initial_dx;
    }
   
    else if((key_state & 0x2) && !(prev_key_state & 0x2)){
        *dx = -initial_dy;
        *dy = initial_dx;
    }
    
    prev_key_state = key_state;

    
    if (collision_detection(player, *dx, *dy) == 0) {
        player->x += *dx;
        player->y += *dy;
    } else {
        return 0; // Collision detected
    }
    
    drawPixel(player->y, player->x, function_colour);
    return 1;
}


int robot_movement(Player* robot, volatile int* dx, volatile int *dy) {
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
                    return 0; // Trapped
                }
            }
        }
    }
    
    robot->x += *dx;
    robot->y += *dy;

    drawPixel(robot->y, robot->x, function_colour);
    return 1;
}

void setup_key_irq(void)
{
    
    *key_edge_ptr = 0xF; 

    
    *key_mask_ptr = 0x3; 
}

void key_press_isr(void){
	volatile uint32_t *key_edge_ptr = (uint32_t *)(KEY_BASE + 0xC);
	uint32_t edge_capture = *key_edge_ptr;
	*key_edge_ptr = edge_capture;
	
	
	
	
	
	
	if(edge_capture & 0x1){
		if(pending_turn == 1){
		pending_turn = 0;
		*led_base &= ~0x1;
		}
		else {
			pending_turn = 1;
			*led_base |= 0x1;
			*led_base &= ~0x2;
		}
	}
	else if((edge_capture & 0x2)){
		if(pending_turn ==2){
		pending_turn = 0; //cancel turn
		*led_base &= ~0x2;
			
		}
		else{
			pending_turn = 2;
			*led_base |= 0x2;
			*led_base &= ~0x1;
		}
	}
	
	
	
}

void game_state_isr(void){

    int initial_x = pdx;
	int initial_y = pdy;
    uint64_t mtimecmp64 = get_mtimer( mtime_ptr+2 );
    mtimecmp64 += PERIOD;
    set_mtimer( mtime_ptr+2, mtimecmp64 );
	
	if(pending_turn == 1){
		pdx = initial_y;
		pdy = -initial_x;
		*led_base = 0;
		pending_turn = 0;
	}
	
	if(pending_turn == 2){
		pdx = -initial_y;
		pdy = initial_x;
		
		*led_base = 0;
		pending_turn = 0;
		
	}
	
		

    
    if(!control_player(&player, &pdx, &pdy)){
        robot_score += 1;
        reset_field();
        draw_arena();
        
        player.x = 80; player.y = 50;
        robot.x = 60; robot.y = 40;
    }

    
    if(!robot_movement(&robot, &rdx, &rdy)){
        player_score += 1;
        reset_field();
        draw_arena();
        
        player.x = 80; player.y = 50;
        robot.x = 60; robot.y = 40;
    }

    
    if(player_score > 9 || robot_score > 9){
        game_over = 1;
    }
    
    display_score(player_score, robot_score);
	define_game_speed();
}



int main()
{
    
    
    
    setup_mtimecmp();
	setup_key_irq();
    setup_cpu_irqs( 0x40080 );

   
    reset_field(); 
    draw_arena();

    
    while(1){
        if(game_over && player_score > robot_score){
            winner_winner(player_colour);
            break;
        }
        if(game_over && robot_score > player_score){
            winner_winner(robot_colour);
            break;
        }
    }
    
    printf( "Done\n" );
    return 0;
}