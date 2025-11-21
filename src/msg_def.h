/*
 * msg_def.h - Message definitions for inter-process communication
 */

#ifndef MSG_DEF_H
#define MSG_DEF_H

#include <stdint.h>
#include <time.h>

// Message types
#define MSG_TYPE_SENSOR_DATA    0x01
#define MSG_TYPE_ALERT          0x02
#define MSG_TYPE_PULSE          0x03
#define MSG_TYPE_LOG            0x04

// Alert levels
#define ALERT_LEVEL_INFO        0x00
#define ALERT_LEVEL_WARNING     0x01
#define ALERT_LEVEL_CRITICAL    0x02

// Sensor types
#define SENSOR_TYPE_TEMPERATURE 0x01
#define SENSOR_TYPE_HUMIDITY    0x02
#define SENSOR_TYPE_GAS         0x03
#define SENSOR_TYPE_MOTION      0x04
#define SENSOR_TYPE_ULTRASONIC  0x05

// Alert types
#define ALERT_TYPE_TEMP_HIGH    0x01
#define ALERT_TYPE_TEMP_LOW     0x02
#define ALERT_TYPE_GAS_DETECTED 0x03
#define ALERT_TYPE_MOTION       0x04
#define ALERT_TYPE_DOOR_CLOSED  0x05
#define ALERT_TYPE_DOOR_OPEN    0x06

// Pulse types (for alert manager LED/buzzer control)
#define PULSE_TYPE_NONE         0x00
#define PULSE_TYPE_SLOW         0x01  // Slow blink (low priority)
#define PULSE_TYPE_FAST         0x02  // Fast blink (medium priority)
#define PULSE_TYPE_SOLID        0x03  // Solid on (high priority)

// Aggregated sensor data message (sent to web server)
typedef struct {
    uint16_t msg_type;              // MSG_TYPE_SENSOR_DATA
    time_t timestamp;               // Time of reading
    
    // Temperature sensor data
    int temperature;                // Temperature in Celsius
    int humidity;                   // Humidity percentage
    uint8_t temp_sensor_valid;      // 1 if valid, 0 if error
    
    // Gas sensor data
    uint8_t gas_detected;           // 1 if gas detected, 0 if clean
    uint8_t gas_sensor_valid;       // 1 if valid, 0 if error
    
    // PIR motion sensor data
    uint8_t motion_detected;        // 1 if motion detected, 0 otherwise
    uint8_t motion_sensor_valid;    // 1 if valid, 0 if error
    
    // Ultrasonic sensor data (door closing detection)
    uint16_t distance_cm;           // Distance in centimeters
    uint8_t door_closed;            // 1 if door closed, 0 if open
    uint8_t ultrasonic_valid;       // 1 if valid, 0 if error
    
    // System status
    uint8_t alert_level;            // Current overall alert level
    uint32_t sequence_num;          // Sequence number for tracking
} sensor_data_msg_t;

// Alert message (sent to event logger)
typedef struct {
    uint16_t msg_type;              // MSG_TYPE_ALERT
    time_t timestamp;               // Time of alert
    uint8_t alert_type;             // Type of alert (ALERT_TYPE_*)
    uint8_t alert_level;            // Alert severity (ALERT_LEVEL_*)
    int sensor_value;               // Sensor value that triggered alert
    char description[128];          // Human-readable description
} alert_msg_t;

// Pulse message (sent to alert manager for LED/buzzer control)
typedef struct {
    uint16_t msg_type;              // MSG_TYPE_PULSE
    uint8_t pulse_type;             // Type of pulse pattern (PULSE_TYPE_*)
    uint8_t alert_level;            // Alert level causing the pulse
    uint16_t duration_ms;           // Duration of pulse (0 = continuous)
} pulse_msg_t;

// Log message (sent to event logger)
typedef struct {
    uint16_t msg_type;              // MSG_TYPE_LOG
    time_t timestamp;               // Time of log entry
    uint8_t log_level;              // Log severity level
    char message[256];              // Log message content
} log_msg_t;

// Threshold configuration
typedef struct {
    int temp_high_threshold;        // Temperature high alert threshold (°C)
    int temp_low_threshold;         // Temperature low alert threshold (°C)
    int humidity_high_threshold;    // Humidity high threshold (%)
    int humidity_low_threshold;     // Humidity low threshold (%)
    uint16_t door_closed_dist_cm;   // Distance threshold for door closed (cm)
} threshold_config_t;

#endif // MSG_DEF_H



