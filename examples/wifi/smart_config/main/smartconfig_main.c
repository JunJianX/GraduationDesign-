/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"
#include "my_tools.h"
#include "my_sntp.h"
#include "my_uart.h"
#include "my_ota.h"
#include "lcd.h"
#include "adc.h"
#include "font.h"
#include "my_mqtt_client.h"
// #include "test.h"



/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
int WiFi_Init_Or_Not=0;
int mqtt_init_or_not=0;
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sc";

uint8_t sntp_flag =1;
uint8_t fun_left_flag = 0;
uint8_t fun_right_flag = 0;

char *passwd=NULL;
char *ssid=NULL;
// TaskHandle_t sntp_handle;
extern parse_event_struct_t my_uart_event;
extern int sntp_ok_flag;
void smartconfig_example_task(void * parm);

void clear_uart_event(void)
{
    my_uart_event.event_type=0;
}
static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
    }
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}
static void task_status(void ){
    TaskStatus_t *StatusArray;
    UBaseType_t task_num;
    int x;
    uint32_t TotalRunTime;
    UBaseType_t ArraySize;
    task_num=uxTaskGetNumberOfTasks();      //获取系统任务数量
    printf("uxTaskGetNumberOfTasks %d\r\n", task_num);

    StatusArray=pvPortMalloc(task_num*sizeof(TaskStatus_t));//申请内存
    if(StatusArray!=NULL)                   //内存申请成功
    {
        ArraySize=uxTaskGetSystemState((TaskStatus_t*   )StatusArray,   //任务信息存储数组
                                    (UBaseType_t     )task_num,  //任务信息存储数组大小
                                    (uint32_t*       )&TotalRunTime);//保存系统总的运行时间
        printf("TaskName\t\tPriority\t\tTaskNumber\t\t\r\n");
        for(x=0;x<task_num;x++)
        {
            printf("%s\t\t%d\t\t\t%d\t\t\t\r\n",                
                    StatusArray[x].pcTaskName, //任务名称
                    (int)StatusArray[x].uxCurrentPriority, //任务优先级
                    (int)StatusArray[x].xTaskNumber); //任务编号

        }
    }
    vPortFree(StatusArray); //释放内存

}
static esp_err_t My_wifi_init(void *ctx,system_event_t *event)
{
    switch(event->event_id)
    {
        case SYSTEM_EVENT_STA_START: 
            printf("SYSTEM_EVENT_STA_START\n");
            if(WiFi_Init_Or_Not==1)
            {

            }
            else if(WiFi_Init_Or_Not==0 )
            {
                xTaskCreate(&smartconfig_example_task,"smartconfig_example_task", 1024+1024, NULL, 7, NULL);
                WiFi_Init_Or_Not=1;
                break;
            }

            // xTaskCreate(&smartconfig_example_task,"smartconfig_example_task", 1024+1024, NULL, 7, NULL);break;
        
        case SYSTEM_EVENT_STA_GOT_IP:
            printf("---------------------\n");
            printf("GOT IP!\n");
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            
            // task_status();
            if(sntp_flag==1)
            {
                sntp_flag=0;
                printf("start sntp!");
                xTaskCreate(&sntp_example_task,"sntp_example_task", 1024+1024, NULL, 7, NULL);
                mqtt_init_or_not=1;
                break;

            }
            printf("---------------------\n");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("---------------------\n");
            printf("DISCONNECTED!Trying \n");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            esp_wifi_connect();
            printf("---------------------\n");
            break;
        default:break;

    }
    return ESP_OK;
}
static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_event_loop_init(My_wifi_init, NULL) );
    wifi_config_t config;
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );


    if(WiFi_Init_Or_Not==1)
    {
        memset(&config,0,(sizeof(config)));
        memcpy(config.sta.ssid,ssid,(strlen(ssid)));
        memcpy(config.sta.password,passwd,(strlen(passwd)));
    
        printf("config read is:%s\n", config.sta.ssid);
        printf("config read is:%s\n", config.sta.password);
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
    }
    
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            /*Save SSID and PASSWORD.*/
            Save_ssid_passwd( (char*)(wifi_config->sta.ssid),(char*)(wifi_config->sta.password));
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                sc_callback_data_t *sc_callback_data = (sc_callback_data_t *)pdata;
                switch (sc_callback_data->type) {
                    case SC_ACK_TYPE_ESPTOUCH:
                        ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d", sc_callback_data->ip[0], sc_callback_data->ip[1], sc_callback_data->ip[2], sc_callback_data->ip[3]);
                        ESP_LOGI(TAG, "TYPE: ESPTOUCH");
                        break;
                    case SC_ACK_TYPE_AIRKISS:
                        ESP_LOGI(TAG, "TYPE: AIRKISS");
                        break;
                    default:
                        ESP_LOGE(TAG, "TYPE: ERROR");
                        break;
                }
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            ESP_LOGI(TAG, "smartconfig task DELETE!");
            vTaskDelete(NULL);
        }
    }
}

void display_task(void * parm)
{   
    struct tm timeinfo;
    time_t t;
    char buffer[17]="";
   
    char strftime_buf[32];
   
    uint8_t i=0;
    while(1)
    {
        i++;
        if(i%5==0)//every 5 seconds flush
        {   time(&t);
            localtime_r(&t,&timeinfo);
            sprintf(buffer,"%4d-%02d-%02d %02d:%02d",timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min);
            dsp_single_colour_x_region(0,0,128,16,GREEN);
            Display_ASCII8X16(0,0,buffer,RED);
            printf("\n%s\n",buffer);
            printf("\n%ld\n",t);
            // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            // ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
        }
        if(i==99)
        {
            i=0;
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
void test_task(void* param)
{
    char* pbuffer = (char*) malloc(2048);
    memset(pbuffer, 0x0, 2048);
    while(1) {
        printf("-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
        vTaskList(pbuffer);
        printf("%s", pbuffer);
        printf("----------------------------------------------\r\n");
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
    free(pbuffer);
}
void app_main()
{
    time_t t;
    uint8_t i=0;
    struct tm timeinfo;
    char buffer[17]="2020-04-22 21:38";
    char strftime_buf[32];
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    /****************************/
    Adc_Init();
    printf("MAIN1---------------------------------\n");
    LCD_GPIO_Init();
    printf("MAIN2---------------------------------\n");
    lcd_initial();
    printf("MAIN3---------------------------------\n");
    /****************************/
    // now_tick=xTaskGetTickCount();
    // vTaskDelayUntil(now_tick,500);
    dsp_single_colour(BLACK);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ets_delay_us(1000000);
    my_uart_init();
    //////////
    printf("-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
    xTaskCreate(&uart_event_task,"uart_event_task", 1024, NULL, 8, NULL); 
    xTaskCreate(&display_task,"display_task",1024, NULL, 12, NULL);
    printf("-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
    // xTaskCreate(&test_task, "test_task", 1024, NULL, 7, NULL);
    printf("\nStart MQTT TASK\n");
    // xTaskCreate(&My_mqtt_task,"My_mqtt_task",4096+2048+1024,NULL,6,NULL);
   
    passwd = malloc(64+1);
    ssid = malloc(32+1);

    memset(passwd,0,64+1);
    memset(ssid,0,32+1);
    

    if(Read_ssid_passwd(ssid,passwd)==0)
    {
        printf("Read passwd&&ssid sccucessful!!\n");
        WiFi_Init_Or_Not =1;

    }
    else
    {
        printf("Read passwd&&ssid failed !!\n");
        WiFi_Init_Or_Not = 0;
    }
    initialise_wifi();
    while(1)
    {
        if(mqtt_init_or_not==1&&sntp_ok_flag==1)
        {
            xTaskCreate(&My_mqtt_task,"My_mqtt_task",8192,NULL,8,NULL);
            mqtt_init_or_not=0;
            sntp_ok_flag=0;
        }

        /**/
        i++;
        if(i%10==0)//every 5 seconds flush
        {   //time(&t);
            // localtime_r(&t,&timeinfo);
            // sprintf(buffer,"%4d-%02d-%02d %02d:%02d",timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min);
            // snprintf(buffer,16,"%4d-%02d-%02d %02d:%02d",timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min);
            // dsp_single_colour_x_region(0,0,128,16,GREEN);
            // Display_ASCII8X16(0,0,buffer,RED);
            //printf("\n%s\n",buffer);
            //printf("\n%ld\n",t);
            // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            //ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
            // Display_ASCII8X16(0,0,buffer,RED);
            // printf("-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
            i=0;
        }
       
        /**/
            
        switch(my_uart_event.event_type){
        case FUN_MY_OTA: 
            printf("Execute OTA!\n");
            xTaskCreate(&ota_example_task,"ota_example_task",8192,NULL,6,NULL);
            break;
        case FUN_REBOOT: 
            esp_restart();
            break;
        case FUN_LEFT: 
            fun_left_flag=1;
            break;
        case FUN_RIGHT:
            fun_left_flag=1;
            break;
        case FUN_SET_SSID_PASSED:
            break;
        case FUN_GET_TIME:
            time(&t);
            printf("\nThe time is %ld \n",t);
            break;
            
        default :
            break;
        }
        my_uart_event.event_type=0;
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }


}

