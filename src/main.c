/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <SEGGER_RTT.h>
#include <SEGGER_RTT_Conf.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/retention/retention.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/zbus/zbus.h>
#include "tl_smf.h"

// TASK WATCHDOG SUBSYSTEM DEFINITIONS
//#define FORCE_TEST_TASK_WATCHDOG // Uncomment to test the task watchdog
static const struct device *const hw_wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog));
static void task_wdt_callback(int channel_id, void *user_data);

// RETENTION SYSTEM DEFINITIONS
static const struct device *retention_data = DEVICE_DT_GET(DT_ALIAS(retention_data));

// DEBUG PINS DEFINITIONS
static const struct gpio_dt_spec dbg_pin0 = GPIO_DT_SPEC_GET(DT_ALIAS(dbg_pin0), gpios);
static const struct gpio_dt_spec dbg_pin1 = GPIO_DT_SPEC_GET(DT_ALIAS(dbg_pin1), gpios);

// ONBOARD LED DEFINITIONS
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// ONBOARD BUTTON DEFINITIONS
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
#define BUTTON_ACTION_LEN (sizeof(int))
#define BUTTON_ACTION_ARRAY_SIZE (1)
K_MSGQ_DEFINE(button_action_msgq, BUTTON_ACTION_LEN, BUTTON_ACTION_ARRAY_SIZE, 4);
static struct gpio_callback button_cb_data;

// STRIP LED DEFINITIONS
#define LED_STRIP_NODE          DT_ALIAS(led_strip)
#define LED_STRIP_NUM_PIXELS    DT_PROP(DT_ALIAS(led_strip), chain_length)
#define RGB(_r, _g, _b) {.r = (_r), .g = (_g), .b = (_b)}
#define AMBIENT_HOT_THREASHOLD  (25)
#define AMBIENT_COLD_THREASHOLD (18)
#define AMBIENT_WET_THREASHOLD  (90)
#define AMBIENT_DRY_THREASHOLD  (70)
typedef enum {
    AMBIENT_HOT_WET,
    AMBIENT_HOT_NORMAL,
    AMBIENT_HOT_DRY,
    AMBIENT_NORMAL_WET,
    AMBIENT_NORMAL_NORMAL,
    AMBIENT_NORMAL_DRY,
    AMBIENT_COLD_WET,
    AMBIENT_COLD_NORMAL,
    AMBIENT_COLD_DRY,

    NUM_AMBIENT_STATES
} AMBIENT_STATE;
static const struct led_rgb default_ambient_rgb_table[NUM_AMBIENT_STATES] = {
    [AMBIENT_HOT_WET      ] RGB(0xff, 0x00, 0x00), // red
    [AMBIENT_HOT_NORMAL   ] RGB(0xff, 0x7f, 0x00), // orange
    [AMBIENT_HOT_DRY      ] RGB(0xff, 0xff, 0x00), // yellow
    [AMBIENT_NORMAL_WET   ] RGB(0x00, 0x00, 0xff), // blue
    [AMBIENT_NORMAL_NORMAL] RGB(0x00, 0xff, 0x00), // green
    [AMBIENT_NORMAL_DRY   ] RGB(0x00, 0xff, 0xff), // cyan
    [AMBIENT_COLD_WET     ] RGB(0xff, 0x7f, 0xff), // pink
    [AMBIENT_COLD_NORMAL  ] RGB(0xff, 0x00, 0xff), // purple
    [AMBIENT_COLD_DRY     ] RGB(0xff, 0xff, 0xff), // white
};
static const struct led_rgb alternate_ambient_rgb_table[NUM_AMBIENT_STATES] = {
    [AMBIENT_HOT_WET      ] RGB(0xff, 0xff, 0xff), // white
    [AMBIENT_HOT_NORMAL   ] RGB(0xff, 0x00, 0xff), // purple
    [AMBIENT_HOT_DRY      ] RGB(0xff, 0x7f, 0xff), // pink
    [AMBIENT_NORMAL_WET   ] RGB(0x00, 0xff, 0xff), // cyan
    [AMBIENT_NORMAL_NORMAL] RGB(0x00, 0xff, 0x00), // green
    [AMBIENT_NORMAL_DRY   ] RGB(0x00, 0x00, 0xff), // blue
    [AMBIENT_COLD_WET     ] RGB(0xff, 0xff, 0x00), // yellow
    [AMBIENT_COLD_NORMAL  ] RGB(0xff, 0x7f, 0x00), // orange
    [AMBIENT_COLD_DRY     ] RGB(0xff, 0x00, 0x00), // red
};
static const struct device *const strip = DEVICE_DT_GET(LED_STRIP_NODE);
static struct led_rgb ambient_rgb_table[NUM_AMBIENT_STATES];
struct led_rgb led_rgb_pixels[LED_STRIP_NUM_PIXELS];

// TEMPERATURE-HUMIDITY SENSOR DEFINITIONS
static const struct device *const temp_hum_sensor = DEVICE_DT_GET(DT_ALIAS(temp_hum_sensor));

// SEGGER RTT DEFINITIONS
K_MUTEX_DEFINE(rtt_mtx);
static char g_rtt_buff[128];
#define RTT_CUSTOM_PRINT(...)                                        \
    {                                                                \
        __unused int ret;                                            \
        ret = k_mutex_lock(&rtt_mtx, K_FOREVER);                     \
        ret = snprintk(g_rtt_buff, sizeof(g_rtt_buff), __VA_ARGS__); \
        ret = SEGGER_RTT_WriteString(0, g_rtt_buff);                 \
        ret = k_mutex_unlock(&rtt_mtx);                              \
    }

// SETTINGS SUBSYSTEM DEFINITIONS
int settings_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
int settings_handle_commit(void);
int settings_handle_export(int (*cb)(const char *name, const void *value, size_t val_len));
int settings_handle_get(const char *key, char *val, int val_len_max);
const struct settings_handler settings = {.name = "settings",
                                          .h_set = settings_handle_set,
                                          .h_commit = settings_handle_commit,
                                          .h_export = settings_handle_export,
                                          .h_get = settings_handle_get};

// THREAD MAIN DEFINITIONS
#define THREAD_MAIN_SLEEPTIME 2000

// THREAD SENSOR DEFINITIONS
#define THREAD_SENSOR_SLEEPTIME 1000
#define THREAD_SENSOR_STACKSIZE 2048
#define THREAD_SENSOR_PRIORITY 7
void thread_sensor_entry_point(void *dummy1, void *dummy2, void *dummy3);
void thread_sensor_loop(const char *my_name);
K_THREAD_DEFINE(thread_sensor, 
                THREAD_SENSOR_STACKSIZE, 
                thread_sensor_entry_point, NULL, NULL, NULL,
                THREAD_SENSOR_PRIORITY, 0, 0);

// THREAD STRIP-LED DEFINITIONS
#define THREAD_STRIPLED_SLEEPTIME 1000
#define THREAD_STRIPLED_STACKSIZE 2048
#define THREAD_STRIPLED_PRIORITY 8
void thread_stripled_entry_point(void *dummy1, void *dummy2, void *dummy3);
void thread_stripled_loop(const char *my_name);
K_THREAD_DEFINE(thread_stripled, 
                THREAD_STRIPLED_STACKSIZE, 
                thread_stripled_entry_point, NULL, NULL, NULL,
                THREAD_STRIPLED_PRIORITY, 0, 0);
AMBIENT_STATE calc_ambient_state(int32_t t, int32_t h);

// ZBUS DEFINITIONS
#define SUBSCRIBER_QUEUE_SIZE 4
struct th_msg {
    int32_t temperature;
    int32_t humidity;
};
static void th_listener_cb(const struct zbus_channel *chan);
ZBUS_LISTENER_DEFINE(th_listner, th_listener_cb);
ZBUS_SUBSCRIBER_DEFINE(th_subscriber, SUBSCRIBER_QUEUE_SIZE);
ZBUS_CHAN_DEFINE(th_chan,                                       /* Name */
                 struct th_msg,                                 /* Message type */
                 NULL,                                          /* Validator */
                 NULL,                                          /* User data */
                 ZBUS_OBSERVERS(th_listner, th_subscriber),     /* observers */
                 ZBUS_MSG_INIT(.temperature = 0, .humidity = 0) /* Initial value */
);

//////////////////////////////////////////////////////////////////////////////////////////
//  SETTINGS SUBSYSTEM IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
int settings_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    size_t name_len;
    __unused int rc;
    name_len = settings_name_next(name, &next);
    if (!next) {
        if (!strncmp(name, "ambient_rgb_table", name_len)) {
            rc = read_cb(cb_arg, &ambient_rgb_table, sizeof(ambient_rgb_table));
            return 0;
        }
    }
    ARG_UNUSED(len);
    return -ENOENT;
}

int settings_handle_commit(void) {
    return 0;
}

int settings_handle_export(int (*cb)(const char *name, const void *value, size_t val_len)) {
    (void)cb("settings/ambient_rgb_table", &ambient_rgb_table, sizeof(ambient_rgb_table));
    return 0;
}

int settings_handle_get(const char *key, char *val, int val_len_max) {
    ARG_UNUSED(key);
    ARG_UNUSED(val);
    ARG_UNUSED(val_len_max);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//  BUTTON PRESSED IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
extern struct s_object tl_s_obj;
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_msgq_put(&button_action_msgq, &pins, K_NO_WAIT);
    k_event_post(&tl_s_obj.smf_event, TL_EVENT_BTN_PRESS);
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
}

//////////////////////////////////////////////////////////////////////////////////////////
//  ZBUS LISTENER IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
static void th_listener_cb(const struct zbus_channel *chan) {
    __unused int ret;
    const struct th_msg *th = zbus_chan_const_msg(chan);
    RTT_CUSTOM_PRINT("From listener -> Temperature x=%d, Humidity=%d\n", th->temperature, th->humidity);
    ret = gpio_pin_toggle_dt(&led);
}

//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD SENSOR IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
void thread_sensor_entry_point(void *dummy1, void *dummy2, void *dummy3) {
    __unused int ret;
    ret = device_is_ready(temp_hum_sensor);
    thread_sensor_loop(__func__);
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
}

void thread_sensor_loop(const char *my_name) {
    const char *tname;
    const struct k_thread *current_thread;
    struct sensor_value temperature;
    struct sensor_value humidity;
    struct th_msg th;
    int task_wdt_id = task_wdt_add(2*THREAD_SENSOR_SLEEPTIME, task_wdt_callback, (void *)k_current_get());
    __unused int ret;

    while (1) {
        // Feed the task watchdog
        task_wdt_feed(task_wdt_id);
        current_thread = k_current_get();
        tname = k_thread_name_get((k_tid_t)current_thread);
        if (tname != NULL) {
            ret = sensor_sample_fetch(temp_hum_sensor);
            ret = sensor_channel_get(temp_hum_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
            ret = sensor_channel_get(temp_hum_sensor, SENSOR_CHAN_HUMIDITY, &humidity);
            RTT_CUSTOM_PRINT("From sensor thread -> Temperature x=%d, Humidity=%d\n", temperature.val1, humidity.val1);
            th.temperature = temperature.val1;
            th.humidity = humidity.val1;
            zbus_chan_pub(&th_chan, &th, K_SECONDS(1));
        } else {
            k_cpu_idle();
        }
        k_msleep(THREAD_SENSOR_SLEEPTIME);
    }
    ARG_UNUSED(my_name);
}

//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD STRIP-LED IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
void thread_stripled_entry_point(void *dummy1, void *dummy2, void *dummy3) {
    __unused int ret;
    ret = device_is_ready(strip);
    memset(&led_rgb_pixels, 0x00, sizeof(led_rgb_pixels));
    ret = led_strip_update_rgb(strip, led_rgb_pixels, LED_STRIP_NUM_PIXELS);
    thread_stripled_loop(__func__);
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
}

void thread_stripled_loop(const char *my_name) {
    const char *tname;
    const struct k_thread *current_thread;
    const struct zbus_channel *chan;
    AMBIENT_STATE ambient_state;
    int task_wdt_id = task_wdt_add(2*THREAD_STRIPLED_SLEEPTIME, task_wdt_callback, (void *)k_current_get());
    __unused int ret;
    while (1) {
#ifdef FORCE_TEST_TASK_WATCHDOG
        static int count = 0;
        if (count++ == 10) {
            k_sleep(K_FOREVER);
        }
#endif
        // Feed the task watchdog
        task_wdt_feed(task_wdt_id);
        current_thread = k_current_get();
        tname = k_thread_name_get((k_tid_t)current_thread);
        if (tname != NULL) {
            struct th_msg th;
            if (!zbus_sub_wait(&th_subscriber, &chan, K_NO_WAIT)) {
                if (&th_chan == chan) {
                    zbus_chan_read(&th_chan, &th, K_MSEC(THREAD_STRIPLED_SLEEPTIME));
                    RTT_CUSTOM_PRINT("From subscriber -> Temperature=%d, Humidity=%d\n", th.temperature, th.humidity);
                }
            }
            ambient_state = calc_ambient_state(th.temperature, th.humidity);
            for (int px = 0; px < LED_STRIP_NUM_PIXELS; px++) {
                memcpy(&led_rgb_pixels[px], &ambient_rgb_table[ambient_state], sizeof(struct led_rgb));
            }
            ret = led_strip_update_rgb(strip, led_rgb_pixels, LED_STRIP_NUM_PIXELS);
        } else {
            k_cpu_idle();
        }
        k_msleep(THREAD_STRIPLED_SLEEPTIME);
    }
    ARG_UNUSED(my_name);
}

AMBIENT_STATE calc_ambient_state(int32_t t, int32_t h) {
    if        (t >= AMBIENT_HOT_THREASHOLD  && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_HOT_WET;
    } else if (t >= AMBIENT_HOT_THREASHOLD  && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_HOT_NORMAL;
    } else if (t >= AMBIENT_HOT_THREASHOLD  && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_HOT_DRY;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_NORMAL_WET;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_NORMAL_NORMAL;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_NORMAL_DRY;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_COLD_WET;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_COLD_NORMAL;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_COLD_DRY;
    } else {
        return AMBIENT_NORMAL_NORMAL;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD MAIN IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
int main(void) {
    __unused int ret;
    __unused ssize_t len;
    int rstCause, rstCount, rstCountReload;

    // Setup HARDWARE WATCHDOG
    ret = device_is_ready(hw_wdt_dev);
    ret = task_wdt_init(hw_wdt_dev);
    int task_wdt_id = task_wdt_add(2*THREAD_MAIN_SLEEPTIME, NULL, NULL);

    // Setup Retention Subsystem
    ret = device_is_ready(retention_data);

    // On Cold Start Clear the Retention Data
    ret = hwinfo_get_reset_cause(&rstCause);
    if (rstCause & (RESET_BROWNOUT | RESET_POR)) {
        ret = retention_clear(retention_data);
    }
    ret = hwinfo_clear_reset_cause();

    // Reload Retention Data and Update Values
    ret = retention_read(retention_data, 10, (uint8_t *)&rstCount, sizeof(rstCount));
    rstCount++;
    ret = retention_write(retention_data, 10, (uint8_t *)&rstCount, sizeof(rstCount));
    ret = retention_read(retention_data, 10, (uint8_t *)&rstCountReload, sizeof(rstCountReload));

    // Setup onboard button in interrupt mode
    ret = gpio_is_ready_dt(&button);
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    // Setup onboard led
    ret = gpio_is_ready_dt(&led);
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    // Setup debug pins
    ret = gpio_is_ready_dt(&dbg_pin0);
    ret = gpio_pin_configure_dt(&dbg_pin0, GPIO_OUTPUT_ACTIVE);
    ret = gpio_is_ready_dt(&dbg_pin1);
    ret = gpio_pin_configure_dt(&dbg_pin1, GPIO_OUTPUT_ACTIVE);

    // Setup Settings Subsystem
    ret = settings_subsys_init();
    ret = settings_register((struct settings_handler *)&settings);
    // Dummy save to clear the flash registers
    int dummy = 0;
    ret = settings_save_one("", (const void *)&dummy, sizeof(dummy));

    // Save alternative colors map in NVS
    memcpy(ambient_rgb_table, alternate_ambient_rgb_table, sizeof(ambient_rgb_table));
    ret = settings_save();

    // Setup actual colors map in default values
    memcpy(ambient_rgb_table, default_ambient_rgb_table, sizeof(ambient_rgb_table));

    while (1) {
        // Feed the task watchdog
        task_wdt_feed(task_wdt_id);
        // Toggle debug pins just to see if the board is working
        ret = gpio_pin_toggle_dt(&dbg_pin0);
        ret = gpio_pin_toggle_dt(&dbg_pin1);

        k_msleep(THREAD_MAIN_SLEEPTIME);

        if (k_msgq_num_used_get(&button_action_msgq)) {
            struct led_rgb act_rgb_table[NUM_AMBIENT_STATES];
            struct led_rgb next_rgb_table[NUM_AMBIENT_STATES];
            uint32_t button_states;
            k_msgq_get(&button_action_msgq, &button_states, K_NO_WAIT);
            // Save actual colors map in local variable
            memcpy(act_rgb_table, ambient_rgb_table, sizeof(act_rgb_table));
            // Load the new colors map from NVS
            ret = settings_load();
            // Save new colors map in local variable
            memcpy(next_rgb_table, ambient_rgb_table, sizeof(next_rgb_table));
            // Save next colors map in NVS
            memcpy(ambient_rgb_table, act_rgb_table, sizeof(ambient_rgb_table));
            ret = settings_save();
            // Set the new colors map from local variable
            memcpy(ambient_rgb_table, next_rgb_table, sizeof(ambient_rgb_table));
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//  TASK WATCHDOG IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
static void task_wdt_callback(int channel_id, void *user_data)
{
    RTT_CUSTOM_PRINT("Task watchdog channel %d callback, thread: %s\n", channel_id, k_thread_name_get((k_tid_t)user_data));
    RTT_CUSTOM_PRINT("Resetting device...\n");
    sys_reboot(SYS_REBOOT_WARM);
}
