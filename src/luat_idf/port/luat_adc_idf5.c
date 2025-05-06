
#include "luat_base.h"
#include "luat_adc.h"

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#include "luat_log.h"
#define LUAT_LOG_TAG "adc"

// Define Arduino-compatible structures and handles
static esp_adc_cal_characteristics_t adc_chars[2];
static bool adc_calibration_initialized[2] = {false, false};
static bool adc_initialized[2] = {false, false};

// For temperature sensor
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
static bool temp_sensor_initialized = false;
#endif

int luat_adc_open(int pin, void *args) {
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if (pin == LUAT_ADC_CH_CPU) {
        // Initialize temperature sensor for ESP32S3
        if (!temp_sensor_initialized) {
            // In Arduino, we'll use the internal temperature sensor
            // ESP32S3 temperature sensor uses the built-in functionality
            temp_sensor_initialized = true;
        }
        return 0;
    }
#endif

    int unit_id = -1, channel = -1;
    if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin;
    } else if (pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin-16;
    } else if (pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX) {
        unit_id = ADC_UNIT_2;
        channel = pin-32;
    } else {
        return -1;
    }

    if (!adc_initialized[unit_id]) {
        // Initialize ADC in Arduino style
        if (unit_id == ADC_UNIT_1) {
            adc1_config_width(ADC_WIDTH_BIT_12);
            adc1_config_channel_atten((adc1_channel_t)channel, ADC_ATTEN_DB_11);
        } else {
            adc2_config_channel_atten((adc2_channel_t)channel, ADC_ATTEN_DB_11);
        }
        
        // Initialize calibration
        if (!adc_calibration_initialized[unit_id]) {
            esp_adc_cal_characterize(unit_id, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars[unit_id]);
            adc_calibration_initialized[unit_id] = true;
        }
        
        adc_initialized[unit_id] = true;
    } else {
        // Configure additional channel if ADC unit already initialized
        if (unit_id == ADC_UNIT_1) {
            adc1_config_channel_atten((adc1_channel_t)channel, ADC_ATTEN_DB_11);
        } else {
            adc2_config_channel_atten((adc2_channel_t)channel, ADC_ATTEN_DB_11);
        }
    }
    
    return 0;
}

int luat_adc_read(int pin, int *val, int *val2) {
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if (pin == LUAT_ADC_CH_CPU) {
        // Read temperature sensor on ESP32S3
        // Arduino doesn't have a direct way to read temperature sensor
        // We'll use the ESP-IDF API wrapped in Arduino
        float tsens_value = temperatureRead();
        *val = (int)(tsens_value * 1000);
        *val2 = (int)(tsens_value * 1000);
        return 0;
    }
#endif

    int unit_id = -1, channel = -1;
    if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin;
    } else if (pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin-16;
    } else if (pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX) {
        unit_id = ADC_UNIT_2;
        channel = pin-32;
    } else {
        return -1;
    }

    // Read ADC value
    if (unit_id == ADC_UNIT_1) {
        *val = adc1_get_raw((adc1_channel_t)channel);
    } else {
        int read_result = 0;
        adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &read_result);
        *val = read_result;
    }

    // Convert to voltage using calibration
    *val2 = esp_adc_cal_raw_to_voltage(*val, &adc_chars[unit_id]);
    
    return 0;
}

int luat_adc_close(int pin) {
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if (pin == LUAT_ADC_CH_CPU) {
        // Disable temperature sensor
        temp_sensor_initialized = false;
        return 0;
    }
#endif

    int gpio_num, unit_id = -1, channel = -1;
    if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin;
    } else if (pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX) {
        unit_id = ADC_UNIT_1;
        channel = pin-16;
    } else if (pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX) {
        unit_id = ADC_UNIT_2;
        channel = pin-32;
    } else {
        return -1;
    }

    // Convert ADC channel to GPIO pin number
    if (unit_id == ADC_UNIT_1) {
        // Map ADC1 channels to GPIO pins based on ESP32-S3 pinout
        static const int adc1_gpio_map[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        if (channel < sizeof(adc1_gpio_map)/sizeof(adc1_gpio_map[0])) {
            gpio_num = adc1_gpio_map[channel];
        }
    } else {
        // Map ADC2 channels to GPIO pins based on ESP32-S3 pinout
        static const int adc2_gpio_map[] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        if (channel < sizeof(adc2_gpio_map)/sizeof(adc2_gpio_map[0])) {
            gpio_num = adc2_gpio_map[channel];
        }
    }

    // Reset pin in Arduino style
    pinMode(gpio_num, INPUT);
    
    return 0;
}

int luat_adc_global_config(int tp, int val) {
    return -1; // Not supported, same as original
}

// #include "driver/gpio.h"
// #include "driver/adc_types_legacy.h"
// #include "esp_adc/adc_continuous.h"
// #include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h"
// #include "esp_adc/adc_cali_scheme.h"

// #include "driver/temperature_sensor.h"

// #include "luat_log.h"
// #define LUAT_LOG_TAG "adc"

// static adc_oneshot_unit_handle_t adc_handle[2] = {0};
// static adc_cali_handle_t adc_cali_handle[2] = {0};
// static uint8_t adc_init[2] = {0};

// static temperature_sensor_handle_t temp_sensor = NULL;

// int luat_adc_open(int pin, void *args){
//     adc_oneshot_chan_cfg_t config = {
//         .bitwidth = ADC_BITWIDTH_DEFAULT,
//         .atten = ADC_ATTEN_DB_11,
//     };
// #if defined(CONFIG_IDF_TARGET_ESP32C2)||defined(CONFIG_IDF_TARGET_ESP32C3)||defined(CONFIG_IDF_TARGET_ESP32S2)||defined(CONFIG_IDF_TARGET_ESP32S3)
//     if (pin==LUAT_ADC_CH_CPU && temp_sensor == NULL){
//         temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
//         temperature_sensor_install(&temp_sensor_config, &temp_sensor);
//         temperature_sensor_enable(temp_sensor);
//         return 0;
//     }
// #endif
//     int unit_id = -1,channel = -1;
//     if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin;
//     }else if(pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin-16;
//     }else if(pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX){
//         unit_id = ADC_UNIT_2;
//         channel = pin-32;
//     }else{
//         return -1;
//     }

//     if (adc_init[unit_id]==0){
//         adc_oneshot_unit_init_cfg_t init_config1 = {
//             .unit_id = unit_id,
//         };
//         adc_oneshot_new_unit(&init_config1, &adc_handle[unit_id]);
//         adc_oneshot_config_channel(adc_handle[unit_id], channel, &config);
// #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
//         adc_cali_curve_fitting_config_t cali_config = {
//             .unit_id = unit_id,
//             .atten = ADC_ATTEN_DB_11,
//             .bitwidth = ADC_BITWIDTH_DEFAULT,
//         };
//         adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle[unit_id]);
// #elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
//         adc_cali_line_fitting_config_t cali_config = {
//             .unit_id = unit_id,
//             .atten = ADC_ATTEN_DB_11,
//             .bitwidth = ADC_BITWIDTH_DEFAULT,
//         };
//         adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle[unit_id]);
// #endif
//         adc_init[unit_id] = 1;
//     }else{
//         adc_oneshot_config_channel(adc_handle[unit_id], channel, &config);
//     }
//     return 0;
// }

// int luat_adc_read(int pin, int *val, int *val2){
// #if defined(CONFIG_IDF_TARGET_ESP32C2)||defined(CONFIG_IDF_TARGET_ESP32C3)||defined(CONFIG_IDF_TARGET_ESP32S2)||defined(CONFIG_IDF_TARGET_ESP32S3)
//     if (pin == LUAT_ADC_CH_CPU){
//         float tsens_value;
//         temperature_sensor_get_celsius(temp_sensor, &tsens_value);
//         *val = (int)(tsens_value*1000);
//         *val2 = (int)(tsens_value*1000);
//         return 0;
//     }
// #endif
//     int unit_id = -1,channel = -1;
//     if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin;
//     }else if(pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin-16;
//     }else if(pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX){
//         unit_id = ADC_UNIT_2;
//         channel = pin-32;
//     }else{
//         return -1;
//     }
//     adc_oneshot_read(adc_handle[unit_id], channel, val);
//     adc_cali_raw_to_voltage(adc_cali_handle[unit_id], *val, val2);
//     return 0;
// }

// int luat_adc_close(int pin){
// #if defined(CONFIG_IDF_TARGET_ESP32C2)||defined(CONFIG_IDF_TARGET_ESP32C3)||defined(CONFIG_IDF_TARGET_ESP32S2)||defined(CONFIG_IDF_TARGET_ESP32S3)
//     if (pin == LUAT_ADC_CH_CPU){
//         temperature_sensor_disable(temp_sensor);
//         temperature_sensor_uninstall(temp_sensor);
//         temp_sensor = NULL;
//         return 0;
//     }
// #endif
//     int gpio_num,unit_id = -1,channel = -1;
//     if (pin >= 0 && pin < 16 && pin < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin;
//     }else if(pin >= 16 && pin < 32 && pin-16 < ADC1_CHANNEL_MAX){
//         unit_id = ADC_UNIT_1;
//         channel = pin-16;
//     }else if(pin >= 32 && pin < 48 && pin-32 < ADC2_CHANNEL_MAX){
//         unit_id = ADC_UNIT_2;
//         channel = pin-32;
//     }else{
//         return -1;
//     }
//     adc_oneshot_channel_to_io(unit_id, channel, &gpio_num);
//     gpio_reset_pin(gpio_num);
//     return 0;
// }

// int luat_adc_global_config(int tp, int val) {
//     return -1; // 暂时不支持配置, 放个空的函数在这里
// }
