#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "driver/pcnt.h"
#include "driver/timer.h"
#include <sys/time.h>

static const int MDM_RX_BUF_SIZE = 1024;	//set the buffer size for communication with modem
uint8_t* rxData;
static const int LCD_RX_BUF_SIZE = 256;		//set the buffer size for communication with LCD

#define SENS_PON (GPIO_NUM_14) 		//Sensor's Power On pin


#define MDM_TXD_PIN  (GPIO_NUM_0) 	//ESP32 TxD pin connect to GSM RxD pin
#define MDM_RXD_PIN  (GPIO_NUM_2) 	//ESP32 RxD pin connect to GSM TxD pin
#define MDM_PON (GPIO_NUM_27) 		//Modem Power On key pin

#define LCD_TXD_PIN  (GPIO_NUM_19) 	//ESP32 TxD pin connect to LCD RxD pin
#define LCD_RXD_PIN  (GPIO_NUM_25) 	//ESP32 RxD pin connect to LCD TxD pin

#define ORP_INPUT (ADC1_CHANNEL_7)		//ORP analog input
#define PH_INPUT (ADC1_CHANNEL_5)		//ph analog input
#define TRBDTY_INPUT (ADC1_CHANNEL_4)	//Turbidity analog input

#define PCNT_UNIT			PCNT_UNIT_0		//I will use the unit 0 of PCNT module
#define PCNT_H_LIM_VAL		480				//set the maximum count value (Water-flow 
											//Formula: 1L = 480 square waves)
#define PCNT_INPUT_SIG_IO   34				//connect pulse-out pin of flow sensor

#define I2C_PORT_NUMBER		I2C_NUM_0		//I2C port number 0
#define I2C_SCL_GPIO		22				//GPIO for SCL pin
#define I2C_SDA_GPIO		21				//GPIO for SDA pin
#define I2C_MASTER_FREQ_HZ	100000			//I2C master clock frequency 100 - 400kHz
#define I2C_ADDR			0x66			//default address of EZO-RTD
#define I2C_WRITE_BIT		I2C_MASTER_WRITE
#define I2C_READ_BIT		I2C_MASTER_READ
#define I2C_ACK_CHECK_EN	0x1
#define I2C_ACK_CHECK_DIS	0x0
#define I2C_ACK_VAL			0x0
#define I2C_NACK_VAL		0x1

#define TIMER_DIVIDER       16  			//  Hardware timer clock divider
#define TIMER_SCALE         (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC (60) 			// 1 min (60 sec) interval
#define TIMER_EVENT      	1        		// timer interrupt will be done with auto reload

/*
 * A structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;

/* A structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct {
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

xQueueHandle pcnt_evt_queue;   				// A queue to handle pulse counter events
pcnt_isr_handle_t isr_handle = NULL; 		//ISR service handle


char fBase_link[45] = "https://xxxxxxx.firebaseio.com";		//where xxxxxxx your firebase project name
char fBase_key[120];
char fBase_data[120];
int upldInterval = 60;		//upload data to firebase every "upldInterval" minutes
int upldInterval_tmp = 60;	//Temporally upldInterval

char m_responce[1024];		//response from the modem in each AT command
char apn[25];				//Access Point Name (choosed automatically according to mobile operator)
char rssi[3];				//signal strength indicator
int rssi_val=-1;			//signal strength indicator (integer) must be 0-31 (-1 is initial)
char imei[16];				//IMEI of the modem (acting as system id)
char operator[9];			//mobile operator's name
int numberOfCharacters = 0;	//Number of characters in serial input buffer
int modem_error=0;			//indicates errors in communication if is > 5 show an error message in display
char error_code[20]= "Normal operation";
int connStatus;				//status of gprs connection 1 means ok, 0 means fail
int hotStart = 0;			//in first GNSS reading do "cold start" after that all readings "hot start"

//set initial values before GNSS reading
int firstTime = 0;			//variable to setup time zone (used once)
char year[5] = "2021";
int yearVal = 2021;
char month[3]  ="03";
int monthVal = 3;
char day[3] = "03";
int dayVal = 3;
char hour[3] = "00";
int hourVal = 0;
char min[3] = "00";
int minVal = 0;
char sec[3] = "01";
int secVal = 1;
char lastUpdate[22];
int timeZone = 2;				//The time zone of Greece
//struct tm timeinfo;
char latit[10] = "00.000000";
char Longi[10] = "00.000000";

char version[10] = "v 1.13";		//firmware version
char total[10];					//variable to store total water consumption
float total_val;				//variable to store total water consumption
char partial[10];				//variable to store partial water consumption
float partial_val = 0.0;		//variable to store partial water consumption

float orp_val;
float orp_offset = 49.058;
char orp[10];
float ph_val;
float ph_offset = -0.968;
char ph[10];
float trb_val;
float trb_offset = -250.0;
char trb[10];
float tempr_val;
char tempr[10];

//this counter increments per 1 min if become 60
//then trigger sensor readings and upload
//sensor data to firebase
int counter=0;

//Timer group0 ISR handler
void IRAM_ATTR timer_group0_isr(void *para)
{
    timer_spinlock_take(TIMER_GROUP_0);
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_idx);

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
if (timer_intr & TIMER_INTR_T0) {
        evt.type = TIMER_EVENT;
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_idx);

    /* Now  send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
    timer_spinlock_give(TIMER_GROUP_0);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm
 * timer_interval_sec - the interval of alarm to set
 */
static void my_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = auto_reload,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}


//initialization esp32 uart1 for gsm modem
void mdm_init(void) {
    const uart_config_t mdmuart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, MDM_RX_BUF_SIZE * 2, 0, 0, NULL, 0);	//I used UART_NUM_1
    uart_param_config(UART_NUM_1, &mdmuart_config);
    uart_set_pin(UART_NUM_1, MDM_TXD_PIN, MDM_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

//initialization esp32 uart2 for LCD communication
void lcd_init(void) {
    const uart_config_t lcduart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_2, LCD_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &lcduart_config);
    uart_set_pin(UART_NUM_2, LCD_TXD_PIN, LCD_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}


int sendData(const char* logName, const char* data, int uart_number)
{
    const int len = strlen(data);
    int txBytes;
    if (uart_number == 0){
    	txBytes = uart_write_bytes(UART_NUM_1, data, len);
    }
    else{
    	txBytes = uart_write_bytes(UART_NUM_2, data, len);
    }

    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return txBytes;
}

//read data from modem
void rx_task(void *arg){
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

    rxData = (uint8_t*) malloc(MDM_RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, rxData, MDM_RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {
        	rxData[rxBytes] = 0;
            //ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, rxData);
            //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, rxData, rxBytes, ESP_LOG_INFO);
            sprintf(m_responce, "%s", rxData);
            numberOfCharacters = rxBytes;
            //printf("I receive %d", numberOfCharacters);
            //printf(" characters in my serial port\n");
            printf("I receive the responce \n%s",m_responce);
        }
    }
    free(rxData);
}

//Send an AT command and wait an appropriate response
void lcd_update(char objct[], char name[], char lcd_data[]){
	int port = 1;
	char lcd_command[20];
	if (strstr(objct, "page") != NULL){
		strcpy(lcd_command, objct);
		strcat(lcd_command,name);
		strcat(lcd_command,lcd_data);
	}

	else if (strstr(objct, "txt") != NULL){
		strcpy(lcd_command, name);
		//char field[9]= "t2.txt=\"";
		//strcpy(lcd_command, field);
		strcat(lcd_command,".txt=\"");
		strcat(lcd_command,lcd_data);
		strcat(lcd_command,"\"");
	}

	else {
		strcpy(lcd_command, name);
		strcat(lcd_command,".pic=");
		strcat(lcd_command,lcd_data);
	}

	//every lcd command must be terminated with "\xFF\xFF\xFF"
	strcat(lcd_command,"\xFF\xFF\xFF");


	static const char *TX_TASK_TAG = "UPDATE_LCD_TASK";
	esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

	sendData(TX_TASK_TAG, lcd_command, port);
	printf("LCD updated with: %s\n", lcd_command);

}

int sendAndWait(char atCommand[], char atResponse[], int timeout){
	int retry=0;
	int port = 0;		//uart1 (modem uart)
	int response=1;
	char message[20] = "Successful";
	static const char *TX_TASK_TAG = "TX_TASK";
	esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
	strcpy(m_responce," ");		//clear receive buffer
	sendData(TX_TASK_TAG, atCommand, port);
	printf("AT command= %s",atCommand);
	char *sent = m_responce;
	//loop until get the appropriate response or timeout
	while (strstr(sent, atResponse) == NULL){
		++retry;
		if (retry > timeout){
			strcpy(message, "Failed by Timeout");
			response = 0;
			break;
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}
	printf("Response to AT= %d\n",response);
	printf("AT Command= %s\n",message);
	return response;							//return 1 on success or 0 on fail
}

void modem_power_on(void){
	printf("Modem power ON\n");
	int response;

	char at_resp[3] = "OK";
	char at_cmd[20] = "AT\r\n";
	int timeout = 1;
	for (int i=0; i<3; ++i){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
		}
	}

	//if modem not responding (is OFF)
		if (response == 0){
			//Set the modem on according to datasheet
			int level = 0;
			gpio_set_level(MDM_PON, level); 					//set power on pin to 0
			printf("level= %d\n",level);
			vTaskDelay(1100 / portTICK_PERIOD_MS);
			level = !level;
			gpio_set_level(MDM_PON, level); 					//set power on pin to 1
			printf("level= %d\n",level);
			vTaskDelay(2500 / portTICK_PERIOD_MS);
			level = !level;
			gpio_set_level(MDM_PON, level); 					//set power on key pin to 0
			printf("level= %d\n",level);

			//waiting for modem to be ready
			//send AT & wait until receive AT\r\r\nOK\r\n sequence as response
			for (int i=0; i<10; ++i){
					response = sendAndWait(at_cmd, at_resp, timeout);
					if (response == 1){
						break;
					}
				}

			if (response == 0){
				//LCD go to page 0 by force
				lcd_update("page", " ", "0");
				while(1){
					printf("Modem error\n");
					strcpy(error_code, "Modem error");
					lcd_update("txt", "t0", "Error code: 1");
					vTaskDelay(1000 / portTICK_PERIOD_MS);
					lcd_update("txt", "t0", error_code);
					vTaskDelay(1000 / portTICK_PERIOD_MS);
				}

			}

			//Mode selection = (2 auto, 13 GSM only, 38 LTE only, 51 GSM and LTE only)
			strcpy(at_cmd, "AT+CNMP=13\r\n");
			strcpy(at_resp, "OK");
			response = sendAndWait(at_cmd, at_resp, timeout);
			//vTaskDelay(2000 / portTICK_PERIOD_MS);

			//Preferred Selection between CAT-M and NB-IoT (1 CAT-M, 2 NB-Iot, 3 CAT-M and NB-IoT)
			strcpy(at_cmd, "AT+CMNB=3\r\n");
			strcpy(at_resp, "OK");
			response = sendAndWait(at_cmd, at_resp, timeout);
			//vTaskDelay(2000 / portTICK_PERIOD_MS);

			//Enable RF
			timeout = 10;
			strcpy(at_cmd, "AT+CFUN=1\r\n");
			strcpy(at_resp, "OK");
			//response = sendAndWait(at_cmd, at_resp, timeout);
			while (1){
				response = sendAndWait(at_cmd, at_resp, timeout);
				if (response == 1){
					break;
					}
				}

			//wait for signal
			vTaskDelay(15000 / portTICK_PERIOD_MS);
		}
}

void modem_power_off(void){
	printf("Modem power OFF\n");
	int level = 0;
	gpio_set_level(MDM_PON, level); 					//set power on pin to 0
	vTaskDelay(1100 / portTICK_PERIOD_MS);
	level = !level;
	gpio_set_level(MDM_PON, level); 					//set power on pin to 1
	vTaskDelay(2500 / portTICK_PERIOD_MS);
	level = !level;
	gpio_set_level(MDM_PON, level); 					//set power on pin to 0
	vTaskDelay(10000 / portTICK_PERIOD_MS);				//delay 10 sec
}

void modem_get_operator(void){
	//char operator[9];
	char *operator1="COSMOTE";
	char *operator2="vodafone";
	char *operator3="WIND";
	int response;
	int timeout = 1;//5

	////get operator's name
	char at_cmd[20]="AT+COPS?\r\n";
	char at_resp[20]="+COPS: 0,0,";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
			}
		}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

	char *sent = m_responce;
	if (strstr(sent, operator1) != NULL){
		strcpy(operator, "COSMOTE");
		strcpy(apn, "\"internet\"\r\n");
	}
	else if (strstr(sent, operator2) != NULL){
		strcpy(operator, "VODAFONE");
		strcpy(apn, "\"internet.vodafone.gr\"\r\n");
	}
	else if (strstr(sent, operator3) != NULL){
		strcpy(operator, "WIND");
		strcpy(apn, "\"gint.b-online.gr\"\r\n");
	}
	else {
		//set by force the APN of Cosmote
		strcpy(operator, "UNKNOWN");
		strcpy(apn, "\"internet\"\r\n");
	}

	printf("Mobile operator= %s\n",operator);
}

void modem_get_rssi(void){
	int response;
	int timeout = 1;//5

	////view RSSI
	char at_cmd[20]="AT+CSQ\r\n";
	char at_resp[20]="+CSQ:";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

	rssi[0]= m_responce[15];
	//if RSSI is below 10 ignore the 2nd place m_responce[16] (it's just a comma)
	if (m_responce[16] !=','){
		rssi[1]=m_responce[16];
		//convert rssi to integer value
		rssi_val = rssi[1]- 0x30;
		rssi_val = rssi_val + ((rssi[0]- 0x30)*10);
	}
	else{
		rssi[1]= ' ';
		rssi_val = rssi[0]- 0x30;
	}


	printf("RSSI = %d\n", rssi_val);

}

void modem_get_imei(void){
	int response;
	int timeout = 1;//5

	//get the modem IMEI
	char at_cmd[20]="AT+GSN\r\n";
	char at_resp[20]="AT+GSN";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
		}
	}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

	for (int i=9; i<24; ++i){
		imei[i-9]=m_responce[i];
	}
	printf("IMEI = %s\n", imei);
}

int gprs_connect(void){
	int response;
	int timeout = 10;

	//connect to gprs  Packet Data Protocol----------------------------------------------------
	//Disable RF
	char at_cmd[50]="AT+CFUN=0\r\n";
	char at_resp[50]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	vTaskDelay(10000 / portTICK_PERIOD_MS);



	//Set the APN manually. Some operators need to
	//set APN first when registering the network.
	timeout = 5;
	strcpy(at_cmd, "AT+CGDCONT=1,\"IP\",");
	strcat(at_cmd,apn);
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

	//Enable RF
	strcpy(at_cmd, "AT+CFUN=1\r\n");
	strcpy(at_resp,"SMS Ready");
	timeout = 10;
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	vTaskDelay(10000 / portTICK_PERIOD_MS);

	//Before activation, use AT+CNCFG to set
	//APN\user name\password if needed.
	timeout = 5;
	strcpy(at_cmd, "AT+CNCFG=0,1,");
	strcat(at_cmd,apn);
	strcpy(at_resp,"OK");
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

	//Activate network, Activate 0th PDP. !!!(WAIT UNTIL YOU RECEIVE +APP PDP: 0,ACTIVE)
	//timeout = 5;
	strcpy(at_cmd, "AT+CNACT=0,1\r\n");
	strcpy(at_resp,"0,ACTIVE");
	//char *sent = m_responce;
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		//if ((response == 1) || (strstr(sent, "ERROR") != NULL)){
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);
	return response;

}

void gprs_diconnect(void){
	int response;
	int timeout = 10;

	//disconnect from gprs  Packet Data Protocol----------------------------------------------------
	//Disable RF
	char at_cmd[50]="AT+CFUN=0\r\n";
	char at_resp[50]="OK";
	response = sendAndWait(at_cmd, at_resp, timeout);
	if (response == 0){
		modem_power_off();									//Modem power off
		modem_power_on();									//Modem power on
	}
	else{
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}

void setup_httpsConnecton(char link[]){
	int response;
	int timeout = 2;



	//configure http connection to a remote server--------------------------------------
	//Configure SSL/TLS version (TLS 1.2)
	char at_cmd[1024]="AT+CSSLCFG=\"sslversion\",1,3\r\n";
	char at_resp[1024]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//Set HTTP SSL
	strcpy(at_cmd,"AT+SHSSL=1,\"\"\r\n");
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//Set connect server parameter
	strcpy(at_cmd,"AT+SHCONF=\"URL\",\"");
	strcat(at_cmd,link);
	strcat(at_cmd,"\"\r\n");
	strcpy(at_resp,"OK");
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//Set max body length
	strcpy(at_cmd,"AT+SHCONF=\"BODYLEN\",1024\r\n");
	while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//Set max header length
	strcpy(at_cmd,"AT+SHCONF=\"HEADERLEN\",350\r\n");
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void patch_request(void){
	int response;
	int timeout = 20;
	int retry=0;


	//Connect HTTPS server !!!!(WAIT UNTIL YOU RECEIVE OK)
	char at_cmd[150]="AT+SHCONN\r\n";
	char at_resp[20]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		char *sent = m_responce;

		if (response == 1){
			break;
				}
		++retry;
		//if connection failed reconnect to gprs & repeat the https request
		if ((retry > 1) || (strstr(sent, "ERROR") != NULL)){
			retry = 0;
			gprs_connect();
			setup_httpsConnecton(fBase_link);
		}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//when connected to remote server
	if (response == 1) {
		modem_error=0;
		printf("I get OK - Normal operation\n");
		strcpy(error_code, "Normal operation");

		//prepare https PUT request----------------------------------------------------------------
		//Clear HTTP header content
		strcpy(at_cmd,"AT+SHCHEAD\r\n");
		timeout = 5;
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"User-Agent\",\"curl/7.47.0\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Content-Type\",\"application/json\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Cache-control\",\"no-cache\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Connection\",\"keep-alive\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Accept\",\"*/*\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Clear body content parameters
		strcpy(at_cmd,"AT+SHCPARA\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//set body content !!!(WAIT UNTIL > after that set the json contents <cr><lf>)
		//THIS TIMEOUT IS 4 sec
		strcpy(at_cmd,"AT+SHBOD=100,4000\r\n");
		strcpy(at_resp,">");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(4000 / portTICK_PERIOD_MS);

		strcpy(at_cmd,fBase_data);
		strcpy(at_resp,"OK");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Do the https request, request type is PATCH (4) BECAREFULL WITH THE KEY
		//IN THIS CASE THE KEY IS (users.json)
		//BECAREFULL THE RESPONSE MUST BE +SHREQ: "PATCH",200,X (X IS THE NUMBER OF CHARACTERS)
		strcpy(at_cmd,"AT+SHREQ=\"");
		strcat(at_cmd,fBase_link);
		strcat(at_cmd,fBase_key);
		strcat(at_cmd,"\",4\r\n");
		strcpy(at_resp,"\"PATCH\",200");
		timeout = 20;
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//if PUT request done successfully
		if (response == 1){
			//Disconnect from remote server
			strcpy(at_cmd,"AT+SHDISC\r\n");
			strcpy(at_resp,"OK");
			timeout = 5;
			while (1){
				response = sendAndWait(at_cmd, at_resp, timeout);
				if (response == 1){
					break;
						}
					}
			//response = sendAndWait(at_cmd, at_resp, timeout);
			//vTaskDelay(1000 / portTICK_PERIOD_MS);


		}
		//else retry
		else {
				//repeat the https request
				strcpy(at_cmd,"AT+SHREQ=\"");
				strcat(at_cmd,fBase_link);
				strcat(at_cmd,fBase_key);
				strcat(at_cmd,"\",4\r\n");
				strcpy(at_resp,"\"PATCH\",200");
				timeout = 20;
				while (1){
					response = sendAndWait(at_cmd, at_resp, timeout);
					if (response == 1){
						break;
							}
						}
				//response = sendAndWait(at_cmd, at_resp, timeout);
				//vTaskDelay(1000 / portTICK_PERIOD_MS);

				//Disconnect from remote server
				strcpy(at_cmd,"AT+SHDISC\r\n");
				strcpy(at_resp,"OK");
				timeout = 5;
				while (1){
					response = sendAndWait(at_cmd, at_resp, timeout);
					if (response == 1){
						break;
							}
						}
				//response = sendAndWait(at_cmd, at_resp, timeout);
				//vTaskDelay(1000 / portTICK_PERIOD_MS);
					}


	}


}

void get_request(void){
	int response;
	int timeout = 20;
	int retry=0;
	char nmbrOfChars[4];	//number of characters to be fetched from firebase
	int sPoint = 0;
	char getKey[100]="/customers/";

	//Connect HTTPS server !!!!(WAIT UNTIL YOU RECEIVE OK)
	char at_cmd[120]="AT+SHCONN\r\n";
	char at_resp[10]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		char *sent = m_responce;

		if (response == 1){
			break;
				}
		++retry;
		//if connection failed reconnect to gprs & repeat the https request
		if ((retry > 1) || (strstr(sent, "ERROR") != NULL)){
			retry = 0;
			gprs_connect();
			setup_httpsConnecton(fBase_link);
		}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);


	//when connected to remote server
	if (response == 1) {
		modem_error=0;
		printf("I get OK - Normal operation\n");
		strcpy(error_code, "Normal operation");

		//prepare https GET request----------------------------------------------------------------
		//Clear HTTP header content
		strcpy(at_cmd,"AT+SHCHEAD\r\n");
		timeout = 5;
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"User-Agent\",\"curl/7.47.0\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Content-Type\",\"application/json\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Cache-control\",\"no-cache\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Connection\",\"keep-alive\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//Add header content
		strcpy(at_cmd,"AT+SHAHEAD=\"Accept\",\"*/*\"\r\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
		//response = sendAndWait(at_cmd, at_resp, timeout);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);



		//Do the https request, request type is GET (1) BECAREFULL WITH THE KEY
		//IN THIS CASE THE KEY IS (users.json)
		//BECAREFULL THE RESPONSE MUST BE +SHREQ: "GET",200,X (X IS THE NUMBER OF CHARACTERS)
		strcat(getKey, imei);
		strcat(getKey, "/consumption/total.json");
		strcpy(at_cmd,"AT+SHREQ=\"");
		strcat(at_cmd,fBase_link);
		strcat(at_cmd,getKey);
		strcat(at_cmd,"\",1\r\n");
		strcpy(at_resp,"\"GET\",200");
		timeout = 20;
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}



		//if GET request done successfully
		if (response == 1){
			//fetch data from firebase
			char *bffr = m_responce;
			char *result = strstr(bffr, "+SHREQ: \"GET\",200,");
			sPoint = result - bffr;

			printf("sPoint = %d\n", sPoint);
			printf("m_responce[sPoint+18]= %d\n",m_responce[sPoint+18]);
			printf("m_responce[sPoint+19]= %d\n",m_responce[sPoint+19]);
			printf("m_responce[sPoint+20]= %d\n",m_responce[sPoint+20]);

			if (m_responce[sPoint+18] >= '0' && m_responce[sPoint+18] <= '9'){
				nmbrOfChars[0] = m_responce[sPoint+18];
			}

			if (m_responce[sPoint+19] >= '0' && m_responce[sPoint+19] <= '9'){
			nmbrOfChars[1] = m_responce[sPoint+19];
			}

			if (m_responce[sPoint+20] >= '0' && m_responce[sPoint+20] <= '9'){
			nmbrOfChars[2] = m_responce[sPoint+20];
			}

			printf("The number of characters I GET = %s\n", nmbrOfChars);

			//Read data
			strcpy(at_cmd,"AT+SHREAD=0,");
			strcat(at_cmd, nmbrOfChars);
			strcat(at_cmd, "\r\n");
			strcpy(at_resp,"OK");
			while (1){
				int startIndex = 0;
				int stopIndex = 0;
				response = sendAndWait(at_cmd, at_resp, timeout);
				if (response == 1){
					//total consumption data is between "" characters ("100.213")
					//responce is something like:
					//OK<CR>
					//<CR>
					//+SHREAD: xx
					//"xxxxxxxxx"<CR>
					for (int j=0; j<200; ++j){
						if (m_responce[j] == '\"'){
							startIndex = j;
							break;
						}
					}
					for (int k = startIndex+1; k<200; ++k){
						if (m_responce[k] == '\"'){
							stopIndex = k;
							break;
						}
					}
					printf("Start Index =%d\n", startIndex);
					printf("Stop Index =%d\n", stopIndex);
					//m=1 to avoid character " from "xxxxxxxxx"
					for (int m =1; m< (stopIndex - startIndex); ++m){
						total[m-1] = m_responce[m + startIndex];
					}
					//strcpy(total, m_responce);
					printf("Total consumption = %s\n", total);

					//convert total consumption to float
					sscanf(total,"%f", &total_val);
					printf("float total_val= %f\n",total_val);
					break;
						}
				}


			//Disconnect from remote server
			strcpy(at_cmd,"AT+SHDISC\r\n");
			strcpy(at_resp,"OK");
			timeout = 5;
			while (1){
				response = sendAndWait(at_cmd, at_resp, timeout);
				if (response == 1){
					break;
						}
					}
			//response = sendAndWait(at_cmd, at_resp, timeout);
			//vTaskDelay(1000 / portTICK_PERIOD_MS);


		}



	}


}

void pwerUpGnss(void){
	int response;
	int timeout = 1;

	//Turn on GNSS power
	char at_cmd[50]="AT+CGNSPWR=1\r\n";
	char at_resp[50]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);


	if (hotStart == 0){
		hotStart = 1;
		//GPS cold start
		strcpy(at_cmd, "AT+CGNSCOLD\r\n");
		printf("GNSS cold start\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
	}
	else {
		//GPS hot start
		strcpy(at_cmd, "AT+CGNSHOT\r\n");
		printf("GNSS hot start\n");
		while (1){
			response = sendAndWait(at_cmd, at_resp, timeout);
			if (response == 1){
				break;
					}
				}
	}


	//response = sendAndWait(at_cmd, at_resp, timeout);
	//vTaskDelay(1000 / portTICK_PERIOD_MS);

}

void convert2local(int yearV, int monthV, int dayV, int hourV, int minV, int secV){
	//Convert utc time to local time
	//values of year, month, day and hour
	//used when update water consumption in firebase
	struct tm timeinfo;
	timeinfo.tm_year = yearV-1900;
	timeinfo.tm_mon = monthV-1;
	timeinfo.tm_mday = dayV;
	timeinfo.tm_hour = hourV;
	timeinfo.tm_min = minV;
	timeinfo.tm_sec = secV;

	printf("local time step 1: %s", asctime(&timeinfo));

	time_t curtime;
	//printf("local time step 2: %s", asctime(&timeinfo));



	if (firstTime == 0){
		firstTime = 1;
		curtime = mktime(&timeinfo);
		printf("local time step 2: %s", asctime(&timeinfo));

		//first upload to firebase before hour value change for the first time
		//after that upload every 1 hour (60 min)
		upldInterval_tmp = 59 - minVal;

	}
	else {
		timeinfo.tm_year = yearV-1900;
		timeinfo.tm_mon = monthV-1;
		timeinfo.tm_mday = dayV;
		timeinfo.tm_hour = hourV+3;
		timeinfo.tm_min = minV;
		timeinfo.tm_sec = secV;
		curtime = mktime(&timeinfo);
		printf("local time step 2: %s", asctime(&timeinfo));
	}

	//time zone info, EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00
	//found @
	//https://remotemonitoringsystems.ca/time-zone-abbreviations.php
	setenv("TZ", "EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00", 1);
	printf("local time step 3: %s", asctime(&timeinfo));
	tzset();
	printf("local time step 4: %s", asctime(&timeinfo));


	localtime_r(&curtime, &timeinfo);
	printf("local time step 5: %s", asctime(&timeinfo));

	char buffer[12];
	yearVal =  timeinfo.tm_year + 1900;
	sprintf(buffer, "%d", yearVal);
	strcpy(year, buffer);
	monthVal = timeinfo.tm_mon + 1;
	sprintf(buffer, "%d", monthVal);
	strcpy(month, buffer);
	sprintf(buffer, "%d", timeinfo.tm_mday);
	strcpy(day, buffer);
	sprintf(buffer, "%d", timeinfo.tm_hour);
	strcpy(hour, buffer);
	sprintf(buffer, "%d", timeinfo.tm_min);
	strcpy(min, buffer);
	sprintf(buffer, "%d", timeinfo.tm_sec);
	strcpy(sec, buffer);

	strcpy(lastUpdate, day);
	strcat(lastUpdate, "/");
	strcat(lastUpdate, month);
	strcat(lastUpdate, "/");
	strcat(lastUpdate, year);
	strcat(lastUpdate, " - ");
	strcat(lastUpdate, hour);
	strcat(lastUpdate, ":");
	strcat(lastUpdate, min);
	strcat(lastUpdate, ":");
	strcat(lastUpdate, sec);

	printf("Year is: %s\n", year);
	printf("Month is: %s\n", month);
	printf("Day is: %s\n", day);
	printf("Hour is: %s\n", hour);
}

void get_gnss(void){
	int response;
	int port = 0;
	int timeout = 1;
	int successReading=1;
	char gnss_pic[2];

	/*response format
	+CGNSINF: <GNSS run status>,<Fix status>,<UTC date & Time>,<Latitude>,<Longitude>,xxxxxxxxxxxxxxxxxxxxx

	year = [27] - [30]
	month = [31] - [32]
	day = [33] - [34]
	hour = [35] - [36]
	min = [37] - [38]
	sec = [39] - [40]
	lat = [46] - [54]
	long = [56] - [64]*/



	char *ok_response="CGNSINF: 1,1,";
	static const char *TX_TASK_TAG = "TX_TASK";
	esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

	char *sent = m_responce;
	int get_timeout=0;
	char cmd[100]="AT+CGNSINF\r\n";
	while (strstr(sent, ok_response)== NULL){
		++get_timeout;
		//80 sec timeout to fix position (Fix status = 1)
		if (get_timeout < 40){
			//Read GNSS navigation information
			sendData(TX_TASK_TAG, cmd, port);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			printf("try to get GNSS data %d\n", get_timeout);
		}

		else {
			printf("GNSS timeout\n");
			successReading=0;
			break;
		}
	}

	if (successReading == 1 || strstr(m_responce, "CGNSINF: 1,,2")!= NULL){
		int base_field = 27;
		//in case of weak signal
		if (strstr(m_responce, "CGNSINF: 1,,2")!= NULL){
			base_field = 26;
		}
		year[0] = m_responce[base_field];
		year[1] = m_responce[base_field+1];
		year[2] = m_responce[base_field+2];
		year[3] = m_responce[base_field+3];
		month[0] = m_responce[base_field+4];
		month[1] = m_responce[base_field+5];
		day[0] = m_responce[base_field+6];
		day[1] = m_responce[base_field+7];
		hour[0] = m_responce[base_field+8];
		hour[1] = m_responce[base_field+9];
		min[0] = m_responce[base_field+10];
		min[1] = m_responce[base_field+11];
		sec[0] = m_responce[base_field+12];
		sec[1] = m_responce[base_field+13];

		//calculate the integer values for RTC usage
		yearVal = (year[0]- 0x30)*1000;
		yearVal = yearVal + ((year[1]- 0x30)*100);
		yearVal = yearVal + ((year[2]- 0x30)*10);
		yearVal = yearVal + (year[3]- 0x30);

		monthVal = (month[0]- 0x30)*10;
		monthVal = monthVal + (month[1]- 0x30);

		dayVal = (day[0]- 0x30)*10;
		dayVal = dayVal + (day[1]- 0x30);

		hourVal = (hour[0]- 0x30)*10;
		hourVal = hourVal + (hour[1]- 0x30);

		minVal = (min[0]- 0x30)*10;
		minVal = minVal + (min[1]- 0x30);

		secVal = (sec[0]- 0x30)*10;
		secVal = secVal + (sec[1]- 0x30);

		for (int i = 46; i< 55; ++i){
			latit[i-46] = m_responce[i];
		}

		for (int j = 56; j< 65; ++j){
			Longi[j-56] = m_responce[j];
		}

		printf("UTC Date - time = %d/%d/%d - %d:%d:%d\n", dayVal, monthVal, yearVal, hourVal, minVal, secVal);
		printf("Coordinates \"Latitude, Longitude\" = %s, %s\n",latit, Longi);

		//show gnss signal strength in lcd
		//full gnss signal
		if (strstr(m_responce, "CGNSINF: 1,1,")!= NULL){
			strcpy(gnss_pic, "8");
			lcd_update("pic", "p3", gnss_pic);
		}
		//show gnss signal strength in lcd
		//weak gnss signal
		else {
			strcpy(gnss_pic, "4");
			lcd_update("pic", "p3", gnss_pic);
		}


		//Convert utc time to local time
		//values of year, month, day and hour
		//used when update water consumption in firebase
		convert2local(yearVal, monthVal, dayVal, hourVal, minVal, secVal);


	}
	else {
		//show gnss signal strength in lcd
		//no gnss signal
		strcpy(gnss_pic, "3");
		lcd_update("pic", "p3", gnss_pic);
	}







	//Turn off GNSS power
	char at_cmd[50]="AT+CGNSPWR=0\r\n";
	char at_resp[50]="OK";
	while (1){
		response = sendAndWait(at_cmd, at_resp, timeout);
		if (response == 1){
			break;
				}
			}
	//disconnect and reconnect to gprs to avoid a modem issue
	//vTaskDelay(5000 / portTICK_PERIOD_MS);
	//gprs_connect();
	modem_power_off();									//Modem power off
	modem_power_on();									//Modem power on
	gprs_connect();										//Connect to gprs




}

//I2C initialization
void i2c_init(void){
	int i2c_master_port = I2C_PORT_NUMBER;
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_SDA_GPIO,
		.sda_pullup_en = GPIO_PULLUP_DISABLE,		//I use external pull up resistor
		.scl_io_num = I2C_SCL_GPIO,
		.scl_pullup_en = GPIO_PULLUP_DISABLE,		//I use external pull up resistor
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	i2c_param_config(i2c_master_port, &conf);
	i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);

}

//read temperature data via i2c
float i2c_transaction(void){
	esp_err_t i2c_ret = ESP_OK;

	uint8_t temp_data[7];

	float temperature = 0.0;
	int dpoint = 3;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	//Prepare to send master read from slave device command
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (I2C_ADDR << 1) | I2C_WRITE_BIT, I2C_ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0x52, I2C_ACK_CHECK_EN);		//0x52 stands for ascii "R" witch means Command "read temperature"
	i2c_master_stop(cmd);

	//Send queued commands
	i2c_ret = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, (1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);

	vTaskDelay(600 / portTICK_PERIOD_MS);

	if (i2c_ret == ESP_OK)
		{
			//Read from slave device
			printf("I2C Write OK\n");

			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (I2C_ADDR << 1) | I2C_READ_BIT, I2C_ACK_CHECK_EN);

			for (int i=0; i<8; ++i){
				i2c_master_read_byte(cmd, &temp_data[i], I2C_ACK_VAL);
			}


			//Send queued commands
			i2c_ret = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, 1000 / portTICK_RATE_MS);
			i2c_cmd_link_delete(cmd);

			if (temp_data[0] == 1){
				printf("I2C Read OK\n");
			}
			else {
				printf("I2C Read FAILED\n");
			}


			//search for the dot point
			for (int z=1; z<7; ++z){
				if (temp_data[z]==0x2e){
					dpoint = z;
				}
			}

			//temperature calculation
			//[1][a][b][.][c][d][e][0] = a*10 + b*1 + c*0.1 + d*0.01 + e*0.001
			//temp_data[7] format is:
			//0, 1, 2, 3, 4, 5, 6
			//position 0 is always 1 in normal operation
			//position 2 or 3 not used is the dot point
			if (dpoint == 3){
				if (temp_data[dpoint-2] > 0x2F && temp_data[dpoint-2] < 0x3A){
					temp_data[dpoint-2] =temp_data[dpoint-2] - 0x30;	//convert ascii character to int number (char -0x30)
					temperature = temp_data[dpoint-2] * 10;				//decades
				}
			}

			if (temp_data[dpoint-1] > 0x2F && temp_data[dpoint-1] < 0x3A){
				temp_data[dpoint-1] =temp_data[dpoint-1] - 0x30;
				temperature = temperature + temp_data[dpoint-1];
			}

			if (temp_data[dpoint+1] > 0x2F && temp_data[dpoint+1] < 0x3A){
				temp_data[dpoint+1] =temp_data[dpoint+1] - 0x30;
				temperature = temperature + (temp_data[dpoint+1] * 0.1);
			}

			if (temp_data[dpoint+2] > 0x2F && temp_data[dpoint+2] < 0x3A){
				temp_data[dpoint+2] =temp_data[dpoint+2] - 0x30;
				temperature = temperature + (temp_data[dpoint+2] * 0.01);
			}

			if (temp_data[dpoint+3] > 0x2F && temp_data[dpoint+3] < 0x3A){
				temp_data[dpoint+3] =temp_data[dpoint+3] - 0x30;
				temperature = temperature + (temp_data[dpoint+3] * 0.001);
			}


			printf("float temperature = %.3f\n", temperature);		//this is the output data---------------------


		}
		else
		{
			printf("I2C Write FAILED\n");
		}
	return temperature;
}



void display_rssi(void){
	char rssi_pic[3];
	if (rssi_val < 4){
		strcpy(rssi_pic, "3");
	}

	else if (rssi_val > 3 && rssi_val < 12){
		strcpy(rssi_pic, "4");
	}

	else if (rssi_val > 11 && rssi_val < 16){
		strcpy(rssi_pic, "5");
	}

	else if (rssi_val > 15 && rssi_val < 20){
		strcpy(rssi_pic, "6");
	}

	else if (rssi_val > 19 && rssi_val < 28){
		strcpy(rssi_pic, "7");
	}

	else {
		strcpy(rssi_pic, "8");
	}
	lcd_update("pic", "p2", rssi_pic);
}

float read_from_adc(char adc_input[]){
	float val = 0.0;
	float res = 3.225;	//ADC resolution = 3.225mV
	float temp[10];

	//get ADC value (average of 10) and print it to the terminal
	if (strstr(adc_input, "orp") != NULL){
		for (int i=0; i<10; ++i){
			temp[i] = adc1_get_raw(ORP_INPUT);
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
		for (int j=0; j<10; ++j){
			val = val + temp[j];
		}
		val = val/10;
		printf("ADC orp value = %f\n",val);
		val = (res*val)-1500;
		val = val + orp_offset;
		printf("orp value = %f\n",val);
	}

	else if (strstr(adc_input, "ph") != NULL){
		for (int i=0; i<10; ++i){
			temp[i] = adc1_get_raw(PH_INPUT);
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
		for (int j=0; j<10; ++j){
			val = val + temp[j];
		}
		val = val/10;
		printf("ADC ph value = %f\n", val);
		val = ((-5.6548*(res*val))/1000)+15.509;
		val = val + ph_offset;
		printf("ph value = %f\n", val);
	}

	else {
		for (int i=0; i<10; ++i){
			temp[i] = adc1_get_raw(TRBDTY_INPUT);
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
		for (int j=0; j<10; ++j){
			val = val + temp[j];
		}
		val = val/10;

		printf("ADC turbidity value = %f\n", val);
		val = (-1342.282*((res*val)/1000))+4000+trb_offset;
		if (val < 0){
					val = 0.000;
				}
		printf("turbidity value = %f\n", val);
	}
	return val;
}

void get_sensor_data(void){
	gpio_set_level(SENS_PON, 1); 					//set sensors power to 1
	vTaskDelay(10000 / portTICK_PERIOD_MS);			//wait 10 sec
	float temp[11];
	tempr_val = 0.0;
	for (int i=0; i<11; ++i){
		temp[i] = i2c_transaction();			//read temperature data
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
	//some times first temperature reading was failed
	//use the 2nd to 11th readings instead (10 values)
	for (int j=1; j<11; ++j){
		tempr_val = tempr_val + temp[j];
	}
	tempr_val = tempr_val/10;
	sprintf(tempr, "%.1f", tempr_val);
	printf("tempr_val = %f\n", tempr_val);
	printf("tempr = %s\n", tempr);
	orp_val = read_from_adc("orp");
	sprintf(orp, "%.1f", orp_val);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	ph_val = read_from_adc("ph");
	sprintf(ph, "%.1f", ph_val);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	trb_val = read_from_adc("trb");
	sprintf(trb, "%.1f", trb_val);

	gpio_set_level(SENS_PON, 0);		//set sensors power to 0
}

void uload_sensors(void){
	strcpy(fBase_key, "/customers/");
	strcat(fBase_key, imei);
	strcat(fBase_key, "/sensors.json");

	strcpy(fBase_data,"{\"orp\": \"");
	strcat(fBase_data, orp);
	strcat(fBase_data, "\",\"ph\":\"");
	strcat(fBase_data, ph);
	strcat(fBase_data, "\",\"temp\":\"");
	strcat(fBase_data, tempr);
	strcat(fBase_data, "\",\"lastupdate\":\"");
	strcat(fBase_data, lastUpdate);
	strcat(fBase_data, "\",\"trb\":\"");
	strcat(fBase_data, trb);
	strcat(fBase_data, "\"}\r\n");

	setup_httpsConnecton(fBase_link);
	patch_request();

}

void uload_partial(void){
	//https://waterqlt-default-rtdb.firebaseio.com/861340049403411/consumption/yyyy/m/d.json
	strcpy(fBase_key, "/customers/");
	strcat(fBase_key, imei);
	strcat(fBase_key, "/consumption/");
	strcat(fBase_key, year);
	strcat(fBase_key, "/");
	strcat(fBase_key, month);
	strcat(fBase_key, "/");
	strcat(fBase_key, day);
	strcat(fBase_key, ".json");

	//example ->     {"19":"0.002"}
	strcpy(fBase_data,"{\"");
	strcat(fBase_data, hour);
	strcat(fBase_data, "\":\"");
	strcat(fBase_data, partial);
	strcat(fBase_data, "\"}\r\n");
	partial_val = 0.0;

	setup_httpsConnecton(fBase_link);
	patch_request();
}

void uload_total(void){
	strcpy(fBase_key, "/customers/");
	strcat(fBase_key, imei);
	strcat(fBase_key, "/consumption.json");

	//example ->     {"total":"120.002"}
	strcpy(fBase_data,"{\"total\":\"");
	strcat(fBase_data, total);
	strcat(fBase_data, "\"}\r\n");
	setup_httpsConnecton(fBase_link);
	patch_request();

}

static void timer_evt_task(void *arg)
{
	int timer2rssi=0;
    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);


        if (evt.type == TIMER_EVENT) {
            printf("Timer event\n");
            //printf("local time is: %s", asctime(&timeinfo));
            ++timer2rssi;
            printf("timer2rssi = %d\n", timer2rssi);
            //every 5 min get rssi and update lcd
            if (timer2rssi==5){
            	timer2rssi=0;
            	//get signal strength
            	modem_get_rssi();
            	//update GSM rssi icon in LCD
            	display_rssi();
            }

            ++counter;
            printf("counter = %d\n", counter);
            printf("Time until upload = %d\n", (upldInterval_tmp - counter));
            //first upload to firebase before hour value change for the first time
            //after that upload every 1 hour (60 min)
            if (counter >= (upldInterval_tmp)){
            	upldInterval_tmp = upldInterval;
            	counter = 0;

            	//get time & position
            	pwerUpGnss();
            	get_gnss();

            	printf("Reading and upload sensor data\n");

            	//upload partial volume
            	if (partial_val == 0){
            		strcpy(partial,"0.000");
            	}
            	uload_partial();

            	//upload total volume
            	uload_total();
            	if (total_val == 0){
            	    strcpy(total,"0.000");
            	}

            	//read sensor orp, ph, temperature, turbidity
            	get_sensor_data();

            	//upload sensor data to firebase
            	uload_sensors();
            }
        }
        else {
            printf("UNKNOWN EVENT TYPE\n");
        }

    }
}

static void IRAM_ATTR pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    int i;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < PCNT_UNIT_MAX; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            /* Save the PCNT event type that caused an interrupt
               to pass it to the main program */
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);
            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static void pcnt_init(void)
{
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT,
        // What to do on the positive edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // Set the maximum limit value to watch
        .counter_h_lim = PCNT_H_LIM_VAL,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_UNIT, 100);
    pcnt_filter_enable(PCNT_UNIT);

    /* Set threshold 0 and 1 values and enable events to watch */
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);

    /* Register ISR handler and enable interrupts for PCNT unit */
    pcnt_isr_register(pcnt_intr_handler, NULL, 0, &isr_handle);
    pcnt_intr_enable(PCNT_UNIT);//************************************************************************************************

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT);
}

void volume_mesure(void *arg){
	//static const char *RX_TASK_TAG = "RX_TASK";
	//esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
	int16_t count = 0;
	int16_t temp_count = 0;
	pcnt_evt_t evt;
	portBASE_TYPE res;
	char buffer[10] = "0.0";
	int indicator=0;
    while (1) {
        /* Wait for the event information passed from PCNT's interrupt handler.
         * Once received, decode the event type and print it on the serial monitor.
         */
        res = xQueueReceive(pcnt_evt_queue, &evt, 1000 / portTICK_PERIOD_MS);
        if (res == pdTRUE) {
        	total_val = total_val + 0.001;						//increment total volume by 1 liter (0.001m^3)
        	partial_val = partial_val + 0.001;					//increment partial volume by 1 liter (0.001m^3)
        	sprintf(buffer, "%.3f", total_val);
        	strcpy(total,buffer);
        	sprintf(buffer, "%.3f", partial_val);
        	strcpy(partial,buffer);
        	//update the total water consumption in LCD
        	lcd_update("txt", "t4", total);
        	lcd_update("pic", "p5", "9");
        	printf("Total volume = %.3f\n", total_val);
        	printf("Partial volume = %.3f\n", partial_val);
        } else {
            pcnt_get_counter_value(PCNT_UNIT, &count);
            if (temp_count != count){
            	//if there is flow, display a double arrow in lcd
            	temp_count = count;
            	if (indicator == 0){
            		indicator = 1;
            		lcd_update("pic", "p5", "9");
            		vTaskDelay(200 / portTICK_PERIOD_MS);
            	}



            }
            else {
            	//else remove the  double arrow in lcd
            	if (indicator == 1){
            		indicator = 0;
            		lcd_update("pic", "p5", "10");
            		vTaskDelay(200 / portTICK_PERIOD_MS);
            	}

            }
            //printf("Current counter value :%d\n", count);
        }
    }


}

void uload_location(void){
	strcpy(fBase_key, "/customers/");
	strcat(fBase_key, imei);
		strcat(fBase_key, "/location.json");

		strcpy(fBase_data,"{\"lati\": \"");
		strcat(fBase_data, latit);
		strcat(fBase_data, "\",\"logni\":\"");
		strcat(fBase_data, Longi);
		strcat(fBase_data, "\"}\r\n");

		setup_httpsConnecton(fBase_link);
		patch_request();
}

void app_main(void)
{
	gpio_set_direction(MDM_PON, GPIO_MODE_OUTPUT);	//modem power on key pin set as output

	/* Configure the IOMUX register for pad BLINK_GPIO (some pads are
	       muxed to GPIO on reset already, but some default to other
	       functions and need to be switched to GPIO. Consult the
	       Technical Reference for a list of pads and their default
	       functions.)
	    */
	gpio_pad_select_gpio(SENS_PON);
	gpio_set_direction(SENS_PON, GPIO_MODE_OUTPUT);	//modem power on key pin set as output

	gpio_set_level(SENS_PON, 0); 					//set sensors power to 0


	//ADC configuration. Read from channels 4, 5 & 7 of ADC1 with 10bit resolution and 11db attenuation
	//attenuation is critical because without attenuation I get value 1024 for every input voltage
	adc1_config_width(ADC_WIDTH_BIT_10);
	adc1_config_channel_atten(ORP_INPUT, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(PH_INPUT, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(TRBDTY_INPUT, ADC_ATTEN_DB_11);

	/* Initialize PCNT event queue and PCNT functions */
	pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
	pcnt_init();

	//uart0 & 1 initialization
	mdm_init();
	lcd_init();



	//LCD go to page 0 by force
	lcd_update("page", " ", "0");

	//display firmware version in lcd
	lcd_update("txt", "t2", version);

	//I2C initialization
	i2c_init();

	//xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
	//read data from modem task
	xTaskCreate(rx_task, "uart0_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);

	modem_power_on();									//Modem power on
	modem_get_imei();									//get modem IMEI (this is the system id)
	modem_get_operator();								//get operator's name
	//RSSI must be 0 - 31
	while ((rssi_val < 0) || (rssi_val > 31)){
		modem_get_rssi();									//get signal strength
	}


	connStatus = gprs_connect();						//Connect to gprs
	printf("Connection Status is: %d\n", connStatus);
	//retry on error
	if (connStatus == 0){
	    modem_power_off();									//Modem power off
	    modem_power_on();									//Modem power on
	    connStatus = gprs_connect();						//Connect to gprs
	    if (connStatus == 0){
	    	printf("gprs communication error\n");
	    		strcpy(error_code, "Communication error");
	    		while(1){
	    			lcd_update("txt", "t0", "Error code: 2");
	    			vTaskDelay(1000 / portTICK_PERIOD_MS);
	    			lcd_update("txt", "t0", error_code);
	    			vTaskDelay(1000 / portTICK_PERIOD_MS);
	    		}
	    }

	}
	else {
		timer_queue = xQueueCreate(10, sizeof(timer_event_t));
		my_timer_init(TIMER_0, TIMER_EVENT, TIMER_INTERVAL0_SEC);
		xTaskCreate(timer_evt_task, "timer_evt_task", 4096*2, NULL, configMAX_PRIORITIES-1, NULL);



		//get stored total consumption from firebase
		setup_httpsConnecton(fBase_link);
		get_request();

		//LCD go to page 1
		lcd_update("page", " ", "1");

		//update GSM rssi icon in LCD
		display_rssi();

		//update operator's name in LCD
		lcd_update("txt", "t3", operator);

		//update serial number in LCD
		lcd_update("txt", "t2", imei);

		//update the total water consumption in LCD
		lcd_update("txt", "t4", total);

		//get time & position
		pwerUpGnss();
		get_gnss();
		uload_location();
		xTaskCreate(volume_mesure, "volume_mesure_task", 1024*2, NULL, configMAX_PRIORITIES-2, NULL);

		//read sensor orp, ph, temperature, turbidity
		get_sensor_data();
		//upload sensor data to firebase
		uload_sensors();


	}

}

