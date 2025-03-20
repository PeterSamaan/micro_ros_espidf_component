
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
#include <std_msgs/msg/string.h> ///1.change the type of message from the includes
#include <std_msgs/msg/int32.h>
#include <rosidl_runtime_c/string_functions.h> // for changing message content

///6. trying to change the blinkinginterval from terminal
#include <std_srvs/srv/set_bool.h>/// 8.1. adding the correct service type
#include "driver/gpio.h" // for the LED commands
#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
#include <rmw_microros/rmw_microros.h>
#endif

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

#define LED_state 22
#define LED_mode 23

rcl_publisher_t publisher;
std_msgs__msg__String msg; 
bool beacon_state =0;
bool beacon_mode =0;
bool on_off =0;

static int32_t exec_timer = 10; 
int32_t counter_1 = 1; 
int32_t counter_2 = 1;


void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    char buffer[100];
    strcpy(buffer, "Beacon is OFF");

    if (beacon_state) {
        strcpy(buffer, "Beacon is ON");

        if (beacon_mode) {
            // Blinking mode: Toggle the LED every `counter_1` counts
            if (counter_2 % counter_1 == 0) {
                on_off = !on_off; // Toggle the LED state
            }
            counter_2++;
            if (counter_2 == 101) counter_2 = 1; // Reset counter_2 to avoid overflow
        } else {
            // Continuous ON mode
            on_off = 1;
        }
    } else {
        // Beacon is OFF
        strcpy(buffer, "Beacon is OFF");
        on_off = 0;
        counter_1 = 0;
        counter_2 = 0;
    }

    // Update the message and set the LED state
    rosidl_runtime_c__String__assign(&msg.data, buffer);
    rcl_ret_t ret = rcl_publish(&publisher, &msg, NULL);
    gpio_set_level(LED_state, on_off);
}




/// 8.4.
/// @brief A service to be called to change the state ofthe beacon
/// @param req of bool type.
/// @param res a confirmation of change.
void service_beacon(const void *req, void *res) {
    printf("Service callback called\n");
    const std_srvs__srv__SetBool_Request *request = (const std_srvs__srv__SetBool_Request *)req;
    std_srvs__srv__SetBool_Response *response = (std_srvs__srv__SetBool_Response *)res;

    // Initialize the response message string
    rosidl_runtime_c__String__init(&response->message);

    // Update interval based on the request data
    printf("Incoming request: %d\n", request->data);

    if (request->data) {
        if (beacon_state) {
            // If beacon is already ON, change the mode
            beacon_mode = 1; // Enable blinking mode
            counter_1++;     // Increase the blink interval
            if (counter_1 > 10) counter_1 = 1; // Reset to 1 after reaching 10
            printf("Changing Mode. Blink interval: %" PRId32 "\n", counter_1);
            response->success = true;
            rosidl_runtime_c__String__assign(&response->message, "Mode changed successfully.");
        } else {
            // Turn ON the beacon (first call)
            beacon_state = 1;
            beacon_mode = 0; // Continuous ON mode
            counter_1 = 1;   // Reset blink interval
            printf("Beacon is ON.\n");
            response->success = true;
            rosidl_runtime_c__String__assign(&response->message, "Beacon turned ON successfully.");
        }
    } else {
        // Turn OFF the beacon
        beacon_state = 0;
        beacon_mode = 0;
        counter_1 = 0;
        printf("Beacon is OFF.\n");
        response->success = true;
        rosidl_runtime_c__String__assign(&response->message, "Beacon turned OFF successfully.");
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

	// Static Agent IP and port can be used instead of autodisvery.
	RCCHECK(rmw_uros_options_set_udp_address(CONFIG_MICRO_ROS_AGENT_IP, CONFIG_MICRO_ROS_AGENT_PORT, rmw_options));
	//RCCHECK(rmw_uros_discover_agent(rmw_options));
#endif

	// create init_options
	RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));

	// create node
	rcl_node_t node;

	// 8.6. creating service
	rcl_service_t service; 

	RCCHECK(rclc_node_init_default(&node, "esp32_string_publisher", "", &support));
	


	// create publisher
	RCCHECK(rclc_publisher_init_default(
		&publisher,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), /// 4.modify the message type
		"freertos_string_publisher"));

	/// 8.8. Service initialization
	RCCHECK(rclc_service_init_default(
    			&service,
    			&node,
    			ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool),
    			"change_beacon_state"));

	// create timer,
	rcl_timer_t timer;
	const unsigned int timer_timeout = 100; ///8.5. bind the timer to a predefined interval
	RCCHECK(rclc_timer_init_default(
		&timer,
		&support,
		RCL_MS_TO_NS(timer_timeout),
		timer_callback));

	// create executor
	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator)); ///8.7.change to 2 if adding the service 
	
	 // 8.9. Adding the service to the executor
    
    std_srvs__srv__SetBool_Request request_msg;
    std_srvs__srv__SetBool_Response response_msg;
    rosidl_runtime_c__String__init(&response_msg.message);
    RCCHECK(rclc_executor_add_service(
        &executor,
        &service,
        &request_msg,
        &response_msg,
        service_beacon));
	
	
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	rosidl_runtime_c__String__assign(&msg.data, "Beacon is OFF"); ///5.Initiate the message

	while(1){
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
		usleep(10000);
	}

	// free resources
	RCCHECK(rcl_publisher_fini(&publisher, &node));
	RCCHECK(rcl_node_fini(&node));

  	vTaskDelete(NULL);
}

void app_main(void)
{
#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    ESP_ERROR_CHECK(uros_network_interface_initialize());
#endif
	gpio_reset_pin(LED_state);                      // Ensure GPIO is reset
    gpio_set_direction(LED_state, GPIO_MODE_OUTPUT); // Set GPIO as output
    gpio_reset_pin(LED_mode);                      // Ensure GPIO is reset
    gpio_set_direction(LED_mode, GPIO_MODE_OUTPUT); // Set GPIO as output

    //pin micro-ros task in APP_CPU to make PRO_CPU to deal with wifi:
    xTaskCreate(micro_ros_task,
            "uros_task",
            CONFIG_MICRO_ROS_APP_STACK,
            NULL,
            CONFIG_MICRO_ROS_APP_TASK_PRIO,
            NULL);
}