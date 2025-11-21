/*
 * stats_update.c
 *
 * This process receives sensor data from the Central Analyzer and writes 
 * it to a dashboard.json file that is then served by an HTTP server.
 *
 *  Created on: 20-Nov-2025
 *      Author: Rohith
 */

#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "msg_def.h"

#define DASHBOARD_FILE "/home/qnxuser/home_safety_dash/dashboard.json"
#define DASHBOARD_FILE_FALLBACK "./dashboard.json"

/**
 * Update dashboard.json with latest sensor data
 * 
 * Format:
 * {
 *   "sensors": {
 *     "door": { "status": "open" | "closed" },
 *     "temperature": { "value": number },
 *     "humidity": { "value": number },
 *     "smoke": { "status": string, "alert": boolean },
 *     "motion": { "status": "detected" | "clear" },
 *     "co2": { "value": number }
 *   }
 * }
 */
static void update_dashboard(sensor_data_msg_t* data) {
    FILE* file;
    char timestamp[64];
    struct tm* tm_info;
    
    // Try primary location, fallback to current directory
    file = fopen(DASHBOARD_FILE, "w");
    if (!file) {
        file = fopen(DASHBOARD_FILE_FALLBACK, "w");
        if (!file) {
            perror("Failed to open dashboard file");
            return;
        }
    }
    
    // Format timestamp
    tm_info = localtime(&data->timestamp);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Write JSON in the specified format
    fprintf(file, "{\n");
    fprintf(file, "  \"timestamp\": \"%s\",\n", timestamp);
    fprintf(file, "  \"sensors\": {\n");
    
    // Door status
    fprintf(file, "    \"door\": {\n");
    if (data->ultrasonic_valid) {
        fprintf(file, "      \"status\": \"%s\"\n", 
                data->door_closed ? "closed" : "open");
    } else {
        fprintf(file, "      \"status\": \"unknown\"\n");
    }
    fprintf(file, "    },\n");
    
    // Temperature
    fprintf(file, "    \"temperature\": {\n");
    if (data->temp_sensor_valid) {
        fprintf(file, "      \"value\": %d\n", data->temperature);
    } else {
        fprintf(file, "      \"value\": null\n");
    }
    fprintf(file, "    },\n");
    
    // Humidity
    fprintf(file, "    \"humidity\": {\n");
    if (data->temp_sensor_valid) {
        fprintf(file, "      \"value\": %d\n", data->humidity);
    } else {
        fprintf(file, "      \"value\": null\n");
    }
    fprintf(file, "    },\n");
    
    // Smoke/Gas sensor (using gas_detected as smoke)
    fprintf(file, "    \"smoke\": {\n");
    if (data->gas_sensor_valid) {
        fprintf(file, "      \"status\": \"%s\",\n", 
                data->gas_detected ? "detected" : "clear");
        fprintf(file, "      \"alert\": %s\n", 
                data->gas_detected ? "true" : "false");
    } else {
        fprintf(file, "      \"status\": \"unknown\",\n");
        fprintf(file, "      \"alert\": false\n");
    }
    fprintf(file, "    },\n");
    
    // Motion
    fprintf(file, "    \"motion\": {\n");
    if (data->motion_sensor_valid) {
        fprintf(file, "      \"status\": \"%s\"\n", 
                data->motion_detected ? "detected" : "clear");
    } else {
        fprintf(file, "      \"status\": \"unknown\"\n");
    }
    fprintf(file, "    },\n");
    
    // CO2 (using gas sensor value as approximation)
    fprintf(file, "    \"co2\": {\n");
    if (data->gas_sensor_valid) {
        // MQ135 can detect CO2, using gas_detected as indicator
        // In a real system, this would be an analog reading
        fprintf(file, "      \"value\": %d\n", data->gas_detected ? 1000 : 400);
    } else {
        fprintf(file, "      \"value\": null\n");
    }
    fprintf(file, "    }\n");
    
    fprintf(file, "  },\n");
    
    // Add metadata
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"sequence\": %u,\n", data->sequence_num);
    fprintf(file, "    \"alert_level\": \"%s\"\n", 
            data->alert_level == ALERT_LEVEL_CRITICAL ? "critical" :
            data->alert_level == ALERT_LEVEL_WARNING ? "warning" : "info");
    fprintf(file, "  }\n");
    
    fprintf(file, "}\n");
    
    fclose(file);
}

static void print_dashboard_update(sensor_data_msg_t* data) {
    char timestamp[64];
    struct tm* tm_info = localtime(&data->timestamp);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("\n┌─────────────────────────────────────────┐\n");
    printf("│    Dashboard Update #%-6u          │\n", data->sequence_num);
    printf("│    %s              │\n", timestamp);
    printf("├─────────────────────────────────────────┤\n");
    
    if (data->temp_sensor_valid) {
        printf("│ 🌡️  Temperature: %-5d°C            │\n", data->temperature);
        printf("│ 💧 Humidity:     %-5d%%             │\n", data->humidity);
    } else {
        printf("│ 🌡️  Temperature: INVALID             │\n");
    }
    
    if (data->gas_sensor_valid) {
        printf("│ 🔥 Smoke/Gas:    %-20s│\n", 
               data->gas_detected ? "⚠️  DETECTED" : "✓ Clear");
    } else {
        printf("│ 🔥 Smoke/Gas:    INVALID             │\n");
    }
    
    if (data->motion_sensor_valid) {
        printf("│ 👁️  Motion:       %-20s│\n", 
               data->motion_detected ? "⚠️  DETECTED" : "✓ Clear");
    } else {
        printf("│ 👁️  Motion:       INVALID             │\n");
    }
    
    if (data->ultrasonic_valid) {
        printf("│ 🚪 Door:         %-20s│\n", 
               data->door_closed ? "✓ CLOSED" : "⚠️  OPEN");
        printf("│    Distance:     %-5d cm            │\n", data->distance_cm);
    } else {
        printf("│ 🚪 Door:         INVALID             │\n");
    }
    
    printf("├─────────────────────────────────────────┤\n");
    printf("│ Alert Level: %-23s│\n", 
           data->alert_level == ALERT_LEVEL_CRITICAL ? "🔴 CRITICAL" :
           data->alert_level == ALERT_LEVEL_WARNING ? "🟡 WARNING" : "🟢 INFO");
    printf("└─────────────────────────────────────────┘\n\n");
}

int main(void) {
    name_attach_t* attach;
    sensor_data_msg_t sensor_msg;
    int rcvid;
    
    printf("===========================================\n");
    printf("  Stats Update - Dashboard JSON Generator\n");
    printf("===========================================\n\n");
    
    // Create a channel and attach a name
    attach = name_attach(NULL, "stats_update", 0);
    if (attach == NULL) {
        fprintf(stderr, "Failed to attach name: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    printf("Stats Update Server ready at /dev/name/stats_update\n");
    printf("Dashboard file: %s\n", DASHBOARD_FILE);
    printf("Fallback file: %s\n", DASHBOARD_FILE_FALLBACK);
    printf("Waiting for sensor data from central analyzer...\n\n");
    
    while (1) {
        // Receive message from Central Analyzer
        rcvid = MsgReceive(attach->chid, &sensor_msg, sizeof(sensor_msg), NULL);
        
        if (rcvid == -1) {
            fprintf(stderr, "MsgReceive error: %s\n", strerror(errno));
            break;
        }
        
        if (rcvid == 0) {
            // Pulse received (not used)
            continue;
        }
        
        // Process sensor data message
        if (sensor_msg.msg_type == MSG_TYPE_SENSOR_DATA) {
            // Update dashboard.json file
            update_dashboard(&sensor_msg);
            
            // Print formatted update to console
            print_dashboard_update(&sensor_msg);
            
            // Reply to sender (required for MsgSend to complete)
            MsgReply(rcvid, EOK, NULL, 0);
        } else {
            printf("Received unknown message type: 0x%02X\n", sensor_msg.msg_type);
            MsgReply(rcvid, EINVAL, NULL, 0);
        }
    }
    
    name_detach(attach, 0);
    return EXIT_SUCCESS;
}
