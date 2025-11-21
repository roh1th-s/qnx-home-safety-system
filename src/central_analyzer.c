/*
 * central_analyzer.c
 *
 *  Central Analyzer Process:
 *  - Spawns threads for each sensor (temperature, gas, PIR motion, ultrasonic)
 *  - Aggregates sensor data
 *  - Sends required data to other processes
 *
 *  Created on: 20-Nov-2025
 *      Author: Rohith
 *
 */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

#include "alert_pulse_def.h"
#include "msg_def.h"

// Sensor modules
#include "sensors/gas_sensor.h"
#include "sensors/motion_sensor.h"
#include "sensors/temperature_sensor.h"
#include "sensors/ultrasonic_sensor.h"

// Global GPIO register pointer for ultrasonic sensor
volatile uint32_t *__RPI_GPIO_REGS = NULL;

// GPIO pin definitions
#define DHT_GPIO_PIN GPIO4     // DHT11 temperature/humidity sensor
#define MQ135_GPIO_PIN GPIO27  // MQ135 gas sensor
#define PIR_GPIO_PIN GPIO21    // PIR motion sensor
#define ULTRASONIC_TRIG_PIN 13 // Ultrasonic trigger
#define ULTRASONIC_ECHO_PIN 25 // Ultrasonic echo

// Timing configuration
#define AGGREGATION_INTERVAL_SEC 2   // Send aggregated data every 5 seconds
#define SENSOR_READ_INTERVAL_MS 1000 // Read sensors every 1 second

// Threshold configuration (can be adjusted)
static threshold_config_t thresholds = {
    .temp_high_threshold = 30,     // 30°C
    .temp_low_threshold = 15,      // 15°C
    .humidity_high_threshold = 80, // 80%
    .humidity_low_threshold = 20,  // 20%
    .door_closed_dist_cm = 10      // 10 cm or less = door closed
};

// Shared sensor data structure (protected by mutex)
typedef struct
{
    int temperature;
    int humidity;
    uint8_t temp_sensor_valid;

    uint8_t gas_detected;
    uint8_t gas_sensor_valid;

    uint8_t motion_detected;
    uint8_t motion_sensor_valid;

    uint16_t distance_cm;
    uint8_t door_closed;
    uint8_t ultrasonic_valid;

    uint8_t alert_level;
} shared_sensor_data_t;

// Global shared data
static shared_sensor_data_t g_sensor_data = {0};
static pthread_mutex_t g_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_sequence_num = 0;

// Connection IDs for message passing
static int stats_update_coid = -1;
static int event_logger_coid = -1;
static int alert_manager_coid = -1;

// Thread control
static volatile bool g_running = true;

// Function prototypes
static void *temperature_sensor_thread(void *arg);
static void *gas_sensor_thread(void *arg);
static void *motion_sensor_thread(void *arg);
static void *ultrasonic_sensor_thread(void *arg);
static void *aggregator_thread(void *arg);
static void check_thresholds_and_alert(shared_sensor_data_t *data);
static void send_alert(uint8_t alert_type, uint8_t alert_level, int sensor_value, const char *description);
static void send_pulse(uint8_t pulse_type, uint8_t alert_level);
static void send_log(const char *message);
static int connect_to_service(const char *service_name);

// Temperature sensor thread
static void *temperature_sensor_thread(void *arg)
{
    (void)arg;
    int temp, hum;

    printf("[TEMP_SENSOR] Thread started\n");
    send_log("Temperature sensor thread started");

    // Initialize DHT11 sensor
    if (temperature_sensor_init(DHT_GPIO_PIN) != 0)
    {
        printf("[TEMP_SENSOR] Failed to initialize sensor\n");
        return NULL;
    }

    while (g_running)
    {
        if (temperature_sensor_read(DHT_GPIO_PIN, &temp, &hum) == 0)
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.temperature = temp;
            g_sensor_data.humidity = hum;
            g_sensor_data.temp_sensor_valid = 1;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[TEMP_SENSOR] Temp: %d°C, Humidity: %d%%\n", temp, hum);
        }
        else
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.temp_sensor_valid = 0;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[TEMP_SENSOR] Read failed\n");
        }

        usleep(SENSOR_READ_INTERVAL_MS * 1000);
    }

    return NULL;
}

// Gas sensor thread
static void *gas_sensor_thread(void *arg)
{
    (void)arg;
    bool gas_detected;

    printf("[GAS_SENSOR] Thread started\n");
    send_log("Gas sensor thread started");

    // Initialize MQ135 sensor
    if (gas_sensor_init(MQ135_GPIO_PIN) != 0)
    {
        printf("[GAS_SENSOR] Failed to initialize sensor\n");
        return NULL;
    }

    while (g_running)
    {
        if (gas_sensor_read(MQ135_GPIO_PIN, &gas_detected) == 0)
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.gas_detected = gas_detected ? 1 : 0;
            g_sensor_data.gas_sensor_valid = 1;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[GAS_SENSOR] Gas: %s\n", gas_detected ? "DETECTED" : "Clean");
        }
        else
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.gas_sensor_valid = 0;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[GAS_SENSOR] Read failed\n");
        }

        usleep(SENSOR_READ_INTERVAL_MS * 1000);
    }

    return NULL;
}

// PIR motion sensor thread
static void *motion_sensor_thread(void *arg)
{
    (void)arg;
    bool motion_detected;

    printf("[MOTION_SENSOR] Thread started\n");
    send_log("Motion sensor thread started");

    // Initialize PIR sensor
    if (motion_sensor_init(PIR_GPIO_PIN) != 0)
    {
        printf("[MOTION_SENSOR] Failed to initialize sensor\n");
        return NULL;
    }

    while (g_running)
    {
        if (motion_sensor_read(PIR_GPIO_PIN, &motion_detected) == 0)
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.motion_detected = motion_detected ? 1 : 0;
            g_sensor_data.motion_sensor_valid = 1;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[MOTION_SENSOR] Motion: %s\n", motion_detected ? "DETECTED" : "None");
        }
        else
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.motion_sensor_valid = 0;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[MOTION_SENSOR] Read failed\n");
        }

        usleep(SENSOR_READ_INTERVAL_MS * 1000);
    }

    return NULL;
}

// Ultrasonic sensor thread (door closing detection)
static void *ultrasonic_sensor_thread(void *arg)
{
    (void)arg;
    uint16_t distance;

    printf("[ULTRASONIC_SENSOR] Thread started\n");
    send_log("Ultrasonic sensor thread started");

    // Initialize ultrasonic sensor
    if (ultrasonic_sensor_init(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN) != 0)
    {
        printf("[ULTRASONIC_SENSOR] Failed to initialize sensor\n");
        return NULL;
    }

    while (g_running)
    {
        if (ultrasonic_sensor_read(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN, &distance) == 0)
        {
            bool door_closed = (distance <= thresholds.door_closed_dist_cm);

            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.distance_cm = distance;
            g_sensor_data.door_closed = door_closed ? 1 : 0;
            g_sensor_data.ultrasonic_valid = 1;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[ULTRASONIC_SENSOR] Distance: %d cm, Door: %s\n", distance,
                   door_closed ? "CLOSED" : "OPEN");
        }
        else
        {
            pthread_mutex_lock(&g_data_mutex);
            g_sensor_data.ultrasonic_valid = 0;
            pthread_mutex_unlock(&g_data_mutex);

            printf("[ULTRASONIC_SENSOR] Read failed\n");
        }

        usleep(SENSOR_READ_INTERVAL_MS * 1000);
    }

    return NULL;
}

// Aggregator thread - collects data and sends to stats_update server
static void *aggregator_thread(void *arg)
{
    (void)arg;
    sensor_data_msg_t msg;

    printf("[AGGREGATOR] Thread started\n");
    send_log("Aggregator thread started");

    while (g_running)
    {
        sleep(AGGREGATION_INTERVAL_SEC);

        // Collect all sensor data
        pthread_mutex_lock(&g_data_mutex);

        msg.msg_type = MSG_TYPE_SENSOR_DATA;
        msg.timestamp = time(NULL);

        msg.temperature = g_sensor_data.temperature;
        msg.humidity = g_sensor_data.humidity;
        msg.temp_sensor_valid = g_sensor_data.temp_sensor_valid;

        msg.gas_detected = g_sensor_data.gas_detected;
        msg.gas_sensor_valid = g_sensor_data.gas_sensor_valid;

        msg.motion_detected = g_sensor_data.motion_detected;
        msg.motion_sensor_valid = g_sensor_data.motion_sensor_valid;

        msg.distance_cm = g_sensor_data.distance_cm;
        msg.door_closed = g_sensor_data.door_closed;
        msg.ultrasonic_valid = g_sensor_data.ultrasonic_valid;

        msg.alert_level = g_sensor_data.alert_level;
        msg.sequence_num = g_sequence_num++;

        // Check thresholds and generate alerts if needed
        check_thresholds_and_alert(&g_sensor_data);

        pthread_mutex_unlock(&g_data_mutex);

        // Send aggregated data to stats_update server
        if (stats_update_coid != -1)
        {
            if (MsgSend(stats_update_coid, &msg, sizeof(msg), NULL, 0) == -1)
            {
                printf("[AGGREGATOR] Failed to send to stats_update: %s\n", strerror(errno));
            }
            else
            {
                printf("[AGGREGATOR] Sent data packet #%u to stats_update (dashboard.json updated)\n",
                       msg.sequence_num);
            }
        }
        else
        {
            printf("[AGGREGATOR] Stats Update not connected (simulated send)\n");
            printf("[AGGREGATOR] Data packet #%u: Temp=%d°C, Hum=%d%%, Gas=%s, Motion=%s, Door=%s\n",
                   msg.sequence_num, msg.temperature, msg.humidity, msg.gas_detected ? "DETECTED" : "Clean",
                   msg.motion_detected ? "YES" : "NO", msg.door_closed ? "CLOSED" : "OPEN");
        }
    }

    return NULL;
}

// Check sensor thresholds and send alerts
static void check_thresholds_and_alert(shared_sensor_data_t *data)
{
    static uint8_t last_alert_level = ALERT_LEVEL_INFO;
    uint8_t current_alert_level = ALERT_LEVEL_INFO;

    // Check temperature thresholds
    if (data->temp_sensor_valid)
    {
        if (data->temperature > thresholds.temp_high_threshold)
        {
            send_alert(ALERT_TYPE_TEMP_HIGH, ALERT_LEVEL_WARNING, data->temperature,
                       "Temperature above threshold");
            send_pulse(HIGH_TEMP, ALERT_LEVEL_WARNING);
            current_alert_level = ALERT_LEVEL_WARNING;
        }
        else if (data->temperature < thresholds.temp_low_threshold)
        {
            send_alert(ALERT_TYPE_TEMP_LOW, ALERT_LEVEL_WARNING, data->temperature,
                       "Temperature below threshold");
            current_alert_level = ALERT_LEVEL_WARNING;
        }
    }

    // Check gas sensor
    if (data->gas_sensor_valid && data->gas_detected)
    {
        send_alert(ALERT_TYPE_GAS_DETECTED, ALERT_LEVEL_CRITICAL, 1, "Gas detected - potential hazard!");
        send_pulse(HIGH_CO2, ALERT_LEVEL_CRITICAL);
        current_alert_level = ALERT_LEVEL_CRITICAL;
    }

    // Check motion sensor
    if (data->motion_sensor_valid && data->motion_detected)
    {
        static uint8_t last_motion = 0;
        // if (!last_motion) {
        send_alert(ALERT_TYPE_MOTION, ALERT_LEVEL_INFO, 1, "Motion detected");
        send_pulse(MOTION_DETECTED, ALERT_LEVEL_INFO);
        //}
        last_motion = data->motion_detected;
        if (current_alert_level < ALERT_LEVEL_INFO)
        {
            current_alert_level = ALERT_LEVEL_INFO;
        }
    }

    // Check door status
    if (data->ultrasonic_valid)
    {
        static uint8_t last_door_state = 0;
        if (data->door_closed && !last_door_state)
        {
            send_alert(ALERT_TYPE_DOOR_CLOSED, ALERT_LEVEL_INFO, data->distance_cm, "Door closed");
            send_pulse(DOOR_OPEN, ALERT_LEVEL_INFO);
        }
        else if (!data->door_closed && last_door_state)
        {
            send_alert(ALERT_TYPE_DOOR_OPEN, ALERT_LEVEL_INFO, data->distance_cm, "Door opened");
            send_pulse(DOOR_OPEN, ALERT_LEVEL_INFO);
        }
        last_door_state = data->door_closed;
    }

    // Update alert level
    data->alert_level = current_alert_level;
    last_alert_level = current_alert_level;
}

// Send alert message to event logger
static void send_alert(uint8_t alert_type, uint8_t alert_level, int sensor_value, const char *description)
{
    struct
    {
        uint16_t type;
        char text[128];
    } msg;

    msg.type = alert_type;
    snprintf(msg.text, sizeof(msg.text), "[%s] %s (value=%d)",
             alert_level == ALERT_LEVEL_CRITICAL  ? "CRITICAL"
             : alert_level == ALERT_LEVEL_WARNING ? "WARNING"
                                                  : "INFO",
             description, sensor_value);

    if (event_logger_coid != -1)
    {
        if (MsgSend(event_logger_coid, &msg, sizeof(msg), NULL, 0) == -1)
        {
            printf("[ALERT] Failed to send to event logger: %s\n", strerror(errno));
        }
        else
        {
            printf("[ALERT] Logged: %s\n", msg.text);
        }
    }
    else
    {
        printf("[ALERT] Event logger not connected: %s\n", msg.text);
    }
}

// Send pulse command to alert manager
static void send_pulse(uint8_t pulse_type, uint8_t alert_level)
{
    (void)alert_level;

    if (alert_manager_coid != -1)
    {
        if (MsgSendPulse(alert_manager_coid, -1, pulse_type, 0) == -1)
        {
            printf("[PULSE] Failed to send pulse to alert manager: %s\n", strerror(errno));
        }
        else
        {
            printf("[PULSE] Sent pulse code: %d\n", pulse_type);
        }
    }
    else
    {
        printf("[PULSE] Alert manager not connected (simulated pulse: %d)\n", pulse_type);
    }
}

// Send log message to event logger
static void send_log(const char *message)
{
    struct
    {
        uint16_t type;
        char text[128];
    } msg;

    msg.type = 0;
    snprintf(msg.text, sizeof(msg.text), "[LOG] %s", message);

    if (event_logger_coid != -1)
    {
        MsgSend(event_logger_coid, &msg, sizeof(msg), NULL, 0);
    }
}

// Connect to a service (returns -1 if service not available)
static int connect_to_service(const char *service_name)
{
    int coid = name_open(service_name, 0);
    if (coid == -1)
    {
        printf("[CONNECT] Could not connect to %s (running in standalone mode)\n", service_name);
    }
    else
    {
        printf("[CONNECT] Connected to %s\n", service_name);
    }
    return coid;
}

int main(void)
{
    pthread_t temp_thread, gas_thread, motion_thread, ultrasonic_thread, agg_thread;

    printf("=================================================\n");
    printf("    Central Analyzer - Sensor Aggregation System\n");
    printf("=================================================\n");

    // Attempt to connect to other processes (optional)
    stats_update_coid = connect_to_service("stats_update");
    event_logger_coid = connect_to_service("event_logger");
    alert_manager_coid = connect_to_service("alert_manager");

    printf("\nStarting sensor threads...\n");

    // Create sensor threads
    if (pthread_create(&temp_thread, NULL, temperature_sensor_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create temperature sensor thread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&gas_thread, NULL, gas_sensor_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create gas sensor thread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&motion_thread, NULL, motion_sensor_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create motion sensor thread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&ultrasonic_thread, NULL, ultrasonic_sensor_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create ultrasonic sensor thread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&agg_thread, NULL, aggregator_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create aggregator thread\n");
        return EXIT_FAILURE;
    }

    printf("\nAll threads started. Central Analyzer running...\n");
    printf("Press Ctrl+C to stop.\n\n");

    // Main thread waits for all sensor threads
    pthread_join(temp_thread, NULL);
    pthread_join(gas_thread, NULL);
    pthread_join(motion_thread, NULL);
    pthread_join(ultrasonic_thread, NULL);
    pthread_join(agg_thread, NULL);

    // Cleanup
    if (stats_update_coid != -1)
    {
        name_close(stats_update_coid);
    }
    if (event_logger_coid != -1)
    {
        name_close(event_logger_coid);
    }
    if (alert_manager_coid != -1)
    {
        name_close(alert_manager_coid);
    }

    rpi_gpio_cleanup();

    printf("\nCentral Analyzer shut down.\n");
    return EXIT_SUCCESS;
}
