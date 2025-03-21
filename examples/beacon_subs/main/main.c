#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include <uros_network_interfaces.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>
#include <rosidl_runtime_c/string_functions.h>

#include <std_srvs/srv/set_bool.h>
#include "driver/gpio.h"
#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
#include <rmw_microros/rmw_microros.h>
#endif

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

#define LED_state 22



rcl_publisher_t publisher; // the main publisher to announce the beacon is off/on
std_msgs__msg__String msg; 

rcl_subscription_t subscriber; // the subscriber to listen to a local publisher publishing an integer
std_msgs__msg__Int32 int_msg;

// Add a new publisher for the echo message
rcl_publisher_t echo_publisher; // an internal publisher inside the subscriber to echo messages for debugging
std_msgs__msg__String echo_msg;

bool beacon_state = 0; // indicator for beacon is off or on in general (constant on or flickering)
bool on_off = 0; // to control the actual on/off state
int ret_value = 0; //the value received from the remote publisher
int counter = 0; //counting cycles for flickering intervals


static int32_t exec_timer = 10; 


void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    char buffer[100];
    strcpy(buffer, "Beacon is OFF");

    if (beacon_state) { 
        strcpy(buffer, "Beacon is ON");
		counter+=10;
		if (counter > 1000) counter = 0;
		if (ret_value ==100) on_off = 1; //If ret_value is 100 ==> beacon is constant on
        // the beacon changes state (on/off) eatch n executor cycles, n is (ret_value/10): for ret_value 40 ==> becaon changes state each 4 cycles
		else if (counter%ret_value ==0) on_off = !on_off; 
    } else {
        strcpy(buffer, "Beacon is OFF");
		counter = 0;
		on_off = 0;
    }

    rosidl_runtime_c__String__assign(&msg.data, buffer);
    rcl_ret_t ret = rcl_publish(&publisher, &msg, NULL);
	gpio_set_level(LED_state, on_off);
}

void subscription_callback(const void *msgin)
{
    const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;
	if (msg->data > 0) {
		beacon_state = 1;
		
	}
	else beacon_state = 0;

	ret_value = msg->data;
    

    // Create an echo message
    char echo_buffer[100];
    snprintf(echo_buffer, sizeof(echo_buffer), "Received: %"PRId32, msg->data);
    rosidl_runtime_c__String__assign(&echo_msg.data, echo_buffer);

    // Publish the echo message
    rcl_ret_t ret = rcl_publish(&echo_publisher, &echo_msg, NULL);
    if (ret != RCL_RET_OK) {
        printf("Failed to publish echo message: %"PRId32"\n", ret);
    }
}

void micro_ros_task(void * arg)
{
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, allocator));

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
    rmw_init_options_t* rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    RCCHECK(rmw_uros_options_set_udp_address(CONFIG_MICRO_ROS_AGENT_IP, CONFIG_MICRO_ROS_AGENT_PORT, rmw_options));
#endif

    RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));

    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, "esp32_string_publisher", "", &support));

    // Initialize the original publisher
    RCCHECK(rclc_publisher_init_default(
        &publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
        "freertos_string_publisher"));

    // Initialize the subscriber
    RCCHECK(rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "publisher_topic")); //this topic is to be the same as of the local  publisher's in the system

    // Initialize the echo publisher
    RCCHECK(rclc_publisher_init_default(
        &echo_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
        "subscriber_echo_topic"));

    rcl_timer_t timer;
    const unsigned int timer_timeout = 100;
    RCCHECK(rclc_timer_init_default(
        &timer,
        &support,
        RCL_MS_TO_NS(timer_timeout),
        timer_callback));

    rclc_executor_t executor;
    RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator)); // 2 noddes are needed in the executor, the main publisher and sbscriber

    RCCHECK(rclc_executor_add_subscription(
        &executor, 
        &subscriber, 
        &int_msg, 
        &subscription_callback, 
        ON_NEW_DATA));

    RCCHECK(rclc_executor_add_timer(&executor, &timer));

    rosidl_runtime_c__String__assign(&msg.data, "Beacon is OFF");

    while (1) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        usleep(10000);
    }

    RCCHECK(rcl_publisher_fini(&publisher, &node));
    RCCHECK(rcl_publisher_fini(&echo_publisher, &node));
    RCCHECK(rcl_node_fini(&node));

    vTaskDelete(NULL);
}

void app_main(void)
{
#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    ESP_ERROR_CHECK(uros_network_interface_initialize());
#endif
    gpio_reset_pin(LED_state);
    gpio_set_direction(LED_state, GPIO_MODE_OUTPUT);


    xTaskCreate(micro_ros_task,
                "uros_task",
                CONFIG_MICRO_ROS_APP_STACK,
                NULL,
                CONFIG_MICRO_ROS_APP_TASK_PRIO,
                NULL);
}