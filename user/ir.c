

#include "osapi.h"
#include "user_interface.h"
#include "gpio.h"
#include "ir_decode.h"

extern int setPwm(int on, int div, int duty);


os_timer_t time1;
uint8 buffer[128] = {0};

void ICACHE_FLASH_ATTR
	time_refresh(void)
{
    os_sprintf(buffer, "%d\r\n", NOW());
    at_port_print(buffer);
}
void ICACHE_FLASH_ATTR
	setuptimer(void)
{
     os_timer_disarm(&time1);
     os_timer_setfn(&time1,(os_timer_func_t *)time_refresh,NULL);
     os_timer_arm(&time1,500, 1);
}

int ICACHE_FLASH_ATTR
	time_test(int on)
{
	if(on)
		setuptimer();
	else
	{
		os_timer_disarm(&time1);
	}
}

// receiver states
#define STATE_IDLE     2
#define STATE_MARK     3
#define STATE_SPACE    4
#define STATE_STOP     5

#define TICKPERMS   312  // milliseconds per clock interrupt tick
#define _GAP 5 // Minimum map between transmissions
#define GAP_TICKS (_GAP*TICKPERMS)
#define RAWBUF 100 // Length of raw duration buffer

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define DATA_LEN_MIN 4

typedef struct
{
	uint32 ir_name;
	uint8  ir_id;
	uint8  ir_func;
	uint8  rcvstate;		   // state machine
	uint8  blinkflag;		   // TRUE to enable blinking of pin 13 on IR processing
	uint32 blink_name;
	uint8  blink_id;
	uint8  blink_func;
	uint8  reserved1;
	uint8  reserved2;
	uint32 status;
	uint32 timer; 	// state timer, counts 50uS ticks.
	uint32 rawbuf[RAWBUF]; // raw data
	uint8  rawlen; 		// counter of entries in rawbuf
	os_timer_t dec_timer;
}ir_t;

LOCAL ir_t ir;

void ICACHE_FLASH_ATTR
	ir_buf_reset(ir_t *ir)
{
	// Throw away and start over
	ir->rawlen = 0;
	ir->rcvstate = STATE_IDLE;
}

#define time_minus(a, b) ((a<b)? (b-a) : (b-a+0xFFFFFFFF))

LOCAL void intr_handler_fn(ir_t *ir)
{
    uint32 stat = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	uint32 irdata = GPIO_REG_READ(GPIO_IN_ADDRESS) & BIT(ir->ir_id);
	uint32 now = NOW();

//	os_sprintf(buffer, "%x:%x\r\n", stat, irdata);
//	at_port_print(buffer);

	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, stat);

	if (ir->rawlen >= RAWBUF) {
		// Buffer overflow
		ir->rcvstate = STATE_STOP;
	}

	if((ir->rcvstate == STATE_SPACE)
				&& (ir->rawlen < DATA_LEN_MIN)
				&& (time_minus(ir->timer, now) > GAP_TICKS)) {
			os_sprintf(buffer, "%d:%d %d\r\n", ir->timer, now, ir->rawlen);
			ir_buf_reset(ir);
			ir->timer = now;
			at_port_print(buffer);
		}
	switch(ir->rcvstate) {
		case STATE_IDLE: // In the middle of a gap
			if (irdata == MARK) {
				// gap just ended, record duration and start recording transmission
				ir->rawlen = 0;
				ir->rawbuf[ir->rawlen++] = now;
				ir->timer = now;
				ir->rcvstate = STATE_MARK;
			}
		break;

		case STATE_MARK: // timing MARK
			if (irdata != MARK) {   // MARK ended, record time
				ir->rawbuf[ir->rawlen++] = now;
				ir->timer = now;
				ir->rcvstate = STATE_SPACE;
			}
		break;

		case STATE_SPACE: // timing SPACE
			if (irdata == MARK) { // SPACE just ended, record it
				{
					ir->rawbuf[ir->rawlen++] = now;
					ir->timer = now;
					ir->rcvstate = STATE_MARK;
				}
			} 
			else { // SPACE // will not be here in interrupt mode, depend on the timer poll
				if (time_minus(ir->timer, now) > GAP_TICKS) {
					// big SPACE, indicates gap between codes
					// Mark current code as ready for processing
					// Switch to STOP
					// Don't reset timer; keep counting space width
					ir->rcvstate = STATE_STOP;
				}
			}
		break;

		case STATE_STOP: // waiting, measuring gap
			//do nothing just wait.
			//if (irdata == MARK)  // reset gap timer
			//	ir->timer = 0;
		break;
	}

	if (ir->blinkflag) {
		if (irdata == MARK) {
			gpio_output_set(0, GPIO_ID_PIN(ir->blink_id), GPIO_ID_PIN(ir->blink_id), 0);;	// turn blink LED on
		} 
		else {
			gpio_output_set(GPIO_ID_PIN(ir->blink_id), 0, GPIO_ID_PIN(ir->blink_id), 0);;  // turn LED off
		}
	}
	
}

int ICACHE_FLASH_ATTR
	ir_io_setup(ir_t *ir, uint32 name, uint8 id, uint8 func)
{
	ir->ir_name = name;
	ir->ir_id = id;
	ir->ir_func = func;

	//disable int
    ETS_GPIO_INTR_DISABLE();
	//set isr	
    ETS_GPIO_INTR_ATTACH(intr_handler_fn, ir);
	//set function
	PIN_FUNC_SELECT(ir->ir_name, ir->ir_func);
	//set input
	gpio_output_set(0, 0, 0, GPIO_ID_PIN(ir->ir_id));
	//config gpio
//    gpio_register_set(GPIO_PIN_ADDR(ir->ir_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
//                          | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
//                          | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
    //clear gpio status
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(ir->ir_id));
    //set int mode
    gpio_pin_intr_state_set(GPIO_ID_PIN(ir->ir_id), GPIO_PIN_INTR_ANYEDGE);
	//enable int
    ETS_GPIO_INTR_ENABLE();
}

int ICACHE_FLASH_ATTR
	ir_blink_setup(ir_t *ir, uint32 name, uint8 id, uint8 func)
{
	ir->blinkflag = 1;

	ir->blink_name = name;
	ir->blink_id = id;
	ir->blink_func = func;

	PIN_FUNC_SELECT(ir->blink_name, ir->blink_func);
	
	gpio_output_set(GPIO_ID_PIN(ir->blink_id), 0, GPIO_ID_PIN(ir->blink_id), 0);

    gpio_register_set(GPIO_PIN_ADDR(ir->blink_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                          | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)
                          | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
}


void ICACHE_FLASH_ATTR
	ir_try_decode(void *arg)
{
	ir_t *ir = (ir_t*)arg;

	decode_results results;

	if ((ir->rcvstate == STATE_SPACE) && (ir->rawlen >= DATA_LEN_MIN)) {
		uint32 now = NOW();
		if (time_minus(ir->timer, now) > GAP_TICKS) {
			// big SPACE, indicates gap between codes
			// Mark current code as ready for processing
			// Switch to STOP
			// space timeout
			ir->rawbuf[ir->rawlen++] = now;
			ir->timer = now;
			ir->rcvstate = STATE_STOP;
		}
	}

	//os_sprintf(buffer, "try %d:%d\r\n", ir->rcvstate, ir->rawlen);
	//at_port_print(buffer);

	if (ir->rcvstate != STATE_STOP)
		return;

	//convert to intervals
	int i;
	for(i=0; i<(ir->rawlen-1); i++)
		ir->rawbuf[i] = ir->rawbuf[i+1] - ir->rawbuf[i];

	{
		int i;
		os_sprintf(buffer, "len:%d\r\n", ir->rawlen-1);
	    at_port_print(buffer);
		for(i=0; i<10/*(ir->rawlen-1)*/; i++) {
			os_sprintf(buffer, "%d ", ir->rawbuf[i]);
		    at_port_print(buffer);
		}
		at_port_print("\r\n");
	}

	if(ir_decode(ir->rawbuf, ir->rawlen-1, &results)==DECODED) {
		os_sprintf(buffer, "decode: bit[%d] val[%08x] type[%d]\r\n",
			results.bits, results.value, results.decode_type);
		at_port_print(buffer);
	}

	// Throw away and start over
	ir_buf_reset(ir);
}

int ICACHE_FLASH_ATTR
	ir_timer_setup(ir_t *ir, int on)
{
	if(on) {
		os_timer_disarm(&ir->dec_timer);
		os_timer_setfn(&ir->dec_timer,(os_timer_func_t *)ir_try_decode, ir);
		os_timer_arm(&ir->dec_timer, 100, 1);
	}
	else {
		os_timer_disarm(&ir->dec_timer);
	}
}

int ICACHE_FLASH_ATTR
	ir_test(int on)
{
	ir_buf_reset(&ir);
//	ir_io_setup(&ir, PERIPHS_IO_MUX_GPIO4_U, 4, FUNC_GPIO4);
//	ir_io_setup(&ir, PERIPHS_IO_MUX_GPIO5_U, 5, FUNC_GPIO5);
	ir_io_setup(&ir, PERIPHS_IO_MUX_MTMS_U, 14, FUNC_GPIO14);
//	ir_blink_setup(&ir, PERIPHS_IO_MUX_GPIO4_U, 4, FUNC_GPIO4);	
	ir_timer_setup(&ir, 1);

	return 0;
}


