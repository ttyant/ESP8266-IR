/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/1/23, v1.0 create this file.
*******************************************************************************/

#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "driver/uart.h"

// test :AT+TEST=1,"abc"<,3>
void ICACHE_FLASH_ATTR
at_setupCmdTest(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 buffer[32] = {0};
    pPara++; // skip '='

    //get the first parameter
    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);

    // flag must be ture because there are more parameter
    if (flag == FALSE) {
        at_response_error();
        return;
    }

    if (*pPara++ != ',') { // skip ','
        at_response_error();
        return;
    }

    os_sprintf(buffer, "the first parameter:%d\r\n", result);
    at_port_print(buffer);

    //get the second parameter
    // string
    at_data_str_copy(buffer, &pPara, 10);
    at_port_print("the second parameter:");
    at_port_print(buffer);
    at_port_print("\r\n");

    if (*pPara == ',') {
        pPara++; // skip ','
        result = 0;
        //there is the third parameter
        // digit
        flag = at_get_next_int_dec(&pPara, &result, &err);
        // we donot care of flag
        os_sprintf(buffer, "the third parameter:%d\r\n", result);
        at_port_print(buffer);
    }

    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_testCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s\r\n", "at_testCmdTest");
    at_port_print(buffer);
    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_queryCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s\r\n", "at_queryCmdTest");
    at_port_print(buffer);
    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_exeCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s cmdnum=%d\r\n", "at_exeCmdTest");
    at_port_print(buffer);
    at_response_ok();
}


extern void at_exeCmdCiupdate(uint8_t id);

extern int setPwm(int on, int div, int duty);

void  ICACHE_FLASH_ATTR
	at_setupCmdPwm(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
	int on = 0, div = 1, duty = 2;

    pPara++; // skip '='

    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);
	on = result;

    if (flag) {
		if (*pPara++ != ',') { // skip ','
			at_response_error();
			return;
		}
		// digit
		flag = at_get_next_int_dec(&pPara, &result, &err);
		div = result;
		
		if (flag) {
			if (*pPara++ != ',') { // skip ','
				at_response_error();
				return;
			}
			// digit
			flag = at_get_next_int_dec(&pPara, &result, &err);
			duty = result;
		}
    }
		
	if(on) {
		uint8 buffer[32] = {0};	
		os_sprintf(buffer, "ON div:%d duty:%d\r\n", div, duty);
		at_port_print(buffer);
	}
	else
		at_port_print("OFF\r\n");

	setPwm(on, div, duty);

	at_response_ok();
}

extern int time_test(int on);

void  ICACHE_FLASH_ATTR
	at_setupCmdTime(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
	int on = 0;
    uint8 buffer[32] = {0};

    pPara++; // skip '='

    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);
	on = result;

    os_sprintf(buffer, "%s\r\n", "at_exeCmdTime");
    at_port_print(buffer);

	time_test(on);

    at_response_ok();
}

void  ICACHE_FLASH_ATTR
	at_setupCmdIR(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
	int on = 0;
    uint8 buffer[32] = {0};

    pPara++; // skip '='

    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);
	on = result;

    os_sprintf(buffer, "IR %s\r\n", on? "ON" : "OFF");
    at_port_print(buffer);

	ir_test(on);

    at_response_ok();
}

void  ICACHE_FLASH_ATTR
	at_setupCmdKey(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
	int on = 0;
    uint8 buffer[32] = {0};

    pPara++; // skip '='

    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);
	on = result;

    os_sprintf(buffer, "IR %s\r\n", on? "ON" : "OFF");
    at_port_print(buffer);

	key_test(on);

    at_response_ok();
}

#include "gpio.h"
#define INT_TEST_PIN_NAME PERIPHS_IO_MUX_MTMS_U
#define INT_TEST_PIN_ID   14
#define INT_TEST_PIN_FUNC FUNC_GPIO14

void interupt_test() {
   //gpio_pin_intr_state_set(GPIO_ID_PIN(INT_TEST_PIN_ID),   GPIO_PIN_INTR_DISABLE );
   uint32 gpio_status;
   gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
   //clear interrupt status
   GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

   ets_uart_printf("Int!\r\n");
   //gpio_pin_intr_state_set(GPIO_ID_PIN(INT_TEST_PIN_ID),  GPIO_PIN_INTR_ANYEDGE); // Interrupt on any edge
  }

void ICACHE_FLASH_ATTR intr_tst_init(int on) {
   ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
   ETS_GPIO_INTR_ATTACH(interupt_test, INT_TEST_PIN_ID); //  interrupt handler
   PIN_FUNC_SELECT(INT_TEST_PIN_NAME, INT_TEST_PIN_FUNC); // Set  function
   gpio_output_set(0, 0, 0, GPIO_ID_PIN(INT_TEST_PIN_ID)); // Set  as input
   GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(INT_TEST_PIN_ID)); // Clear  status
   gpio_pin_intr_state_set(GPIO_ID_PIN(INT_TEST_PIN_ID), GPIO_PIN_INTR_ANYEDGE); // Interrupt on any edge
   ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void  ICACHE_FLASH_ATTR
	at_setupCmdINT(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
	int on = 0;
    uint8 buffer[32] = {0};

    pPara++; // skip '='

    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);
	on = result;

    os_sprintf(buffer, "IR %s\r\n", on? "ON" : "OFF");
    at_port_print(buffer);

	intr_tst_init(on);

    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_exeCmdRst(uint8_t id)
{
  at_response_ok();
  system_restart();
}

at_funcationType at_custom_cmd[] = {
    {"+IR",       3,       NULL,            NULL, at_setupCmdIR,       NULL},
    {"+TEST",     5, at_testCmdTest, at_queryCmdTest, at_setupCmdTest, at_exeCmdTest},
    {"+RST",      4,       NULL,            NULL,            NULL,     at_exeCmdRst}/*,
    {"+CIUPDATE", 9,       NULL,            NULL,            NULL,     at_exeCmdCiupdate},
    {"+PWM",      4,       NULL,            NULL, at_setupCmdPwm,      NULL},    
    {"+TIME",     5,       NULL,            NULL, at_setupCmdTime,     NULL},	
    {"+KEY",      4,       NULL,            NULL, at_setupCmdKey,      NULL},
    {"+INT",      4,       NULL,            NULL, at_setupCmdINT,      NULL}*/
	
};

void user_init(void)
{
    char buf[64] = {0};
    //at_customLinkMax = 5;

    uart_init(BIT_RATE_115200, BIT_RATE_115200);

    wifi_set_opmode(STATION_MODE);
    wifi_station_set_auto_connect(0);
    wifi_set_sleep_type(MODEM_SLEEP_T);
    

    os_sprintf(buf,"compile time:%s %s",__DATE__,__TIME__);
    ets_uart_printf(buf);
    ets_uart_printf("\r\nready\r\n");

    at_cmd_array_regist(&at_custom_cmd[0], sizeof(at_custom_cmd)/sizeof(at_custom_cmd[0]));
    at_init();
}
