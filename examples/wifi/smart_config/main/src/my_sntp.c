#include "my_sntp.h"

static const char *TAG = "sntp_example";
extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;
int sntp_ok_flag=0;
// int sntp_success_flag =1;//0 is failed.1 is ok.
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "pool.ntp.org");
    // sntp_setservername(1, "pool.ntp.org");
    // sntp_setservername(2, "pool.ntp.org");
    sntp_setservername(0, "ntp.aliyun.com");
    sntp_setservername(1, "s1b.time.edu.cn");
    sntp_setservername(2, "time2.aliyun.com");
    // sntp_setservername(1,"cn.ntp.org.cn");
    sntp_init();
}

static void obtain_time(void)
{
    int i = 0;
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    // int retry = 0;
    // const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) /*&& ++retry < retry_count*/) {
        // xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT , true, false, portMAX_DELAY); 
        printf( "Waiting for system time to be set... \n");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void sntp_example_task(void *arg)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    EventBits_t uxBits;
    
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
    }

    // Set timezone to Eastern Standard Time and print local time
    // setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    // tzset();

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    while (1) {
        // update 'now' variable with current time
       
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGE(TAG, "The current date/time error");
        } else {
            time_t time_stap = mktime(&timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
             ESP_LOGI(TAG, "The current STAMP  date/time in Shanghai is: %ld",time_stap);
            // printf("DELETE SNTP TASK!\n");
            ESP_LOGI(TAG, "DELETE SNTP TASK!\n");
            sntp_ok_flag=1;
            vTaskDelete(NULL);
        }
     
        ESP_LOGI(TAG, "Free heap size: %d\n", esp_get_free_heap_size());
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}