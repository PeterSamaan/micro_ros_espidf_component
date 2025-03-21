![banner](.images/banner-dark-theme.png#gh-dark-mode-only)
![banner](.images/banner-light-theme.png#gh-light-mode-only)

# micro-ROS component for ESP-IDF

This component has been tested in ESP-IDF v4.4, and v5.2 with ESP32, ESP32-S2, ESP32-S3 and ESP32-C3.

## Dependencies

This component needs `colcon` and other Python 3 packages inside the IDF virtual environment in order to build micro-ROS packages:

```bash
. $IDF_PATH/export.sh
pip3 install catkin_pkg lark-parser colcon-common-extensions empy==3.3.4
```

## Middlewares available

This package support the usage of micro-ROS on top of two different middlewares:
- [eProsima Micro XRCE-DDS](https://micro-xrce-dds.docs.eprosima.com/en/latest/): the default micro-ROS middleware.
- [embeddedRTPS](https://github.com/embedded-software-laboratory/embeddedRTPS): an experimental implementation of a RTPS middleware compatible with ROS 2.

In order to select it, use `idf.py menuconfig` and go to `micro-ROS Settings > micro-ROS middleware`
## Usage

You can clone this repo directly in the `components` folder of your project.

If you encounter issues during the build process, ensure that you are running in a clean shell environment _without_ the ROS 2 setup script sourced.

## Example

In order to test a int32_publisher example:

```bash
. $IDF_PATH/export.sh
cd examples/int32_publisher
# Set target board [esp32|esp32s2|esp32s3|esp32c3]
idf.py set-target esp32
idf.py menuconfig
# Set your micro-ROS configuration and WiFi credentials under micro-ROS Settings
idf.py build
idf.py flash
idf.py monitor
```

To clean and rebuild all the micro-ROS library:

```bash
idf.py clean-microros
```

Is possible to use a micro-ROS Agent just with this docker command:

```bash
# UDPv4 micro-ROS Agent
docker run -it --rm --net=host microros/micro-ros-agent:foxy udp4 --port 8888 -v6
```

## Build and Flash in one step with docker container

It's possible to build this example application using preconfigured docker container. Execute this line to build an example app using docker container:

```bash
docker run -it --rm --user root     --volume="/etc/timezone:/etc/timezone:ro"     -v $(pwd):/micro_ros_espidf_component     -v /dev:/dev --privileged     --workdir /micro_ros_espidf_component/examples/beacon_subs     microros/esp-idf-microros:latest /bin/bash -c "idf.py menuconfig build flash monitor"
```
Mind the part *examples/beacon_subs* indicates the desied example to be run.

Dockerfile for this container is provided in the ./docker directory and available in dockerhub. This approach uses ESP-IDF v5.

## Using serial transport

By default, micro-ROS component uses UDP transport, but is possible to enable UART transport or any other custom transport setting the `colcon.meta` like:

```json
...
"rmw_microxrcedds": {
    "cmake-args": [
        ...
        "-DRMW_UXRCE_TRANSPORT=custom",
        ...
    ]
},
...
```

An example on how to implement this external transports is available in `examples/int32_publisher_custom_transport`.

Available ports are `0`, `1` and `2` corresponding `UART_NUM_0`, `UART_NUM_1` and `UART_NUM_2`.

Is possible to use a micro-ROS Agent just with this docker command:

```bash
# Serial micro-ROS Agent
docker run -it --rm -v /dev:/dev --privileged --net=host microros/micro-ros-agent:foxy serial --dev [YOUR BOARD PORT] -v6
```

## Purpose of the Project

The iitial software examples were not meant for production use. It had neither been developed nor
tested for a specific use case. However, the two examples added:
* beacon_control
* beacon_flicker

Were designed to control  beacon of model *RS 907-5965* 

 ### beacon_control
 The project has a publisher and a binary service, the publisher indicates the 
 case of the beacon (on / off), while calling the service with TRUE will 
 switch it to ON or change its mode is it is already ON, calling the service with OFF willswitch off the beacon. 

 ### beacon_flicker
 The project has a publisher and a binary service, the publisher indicates the 
 case of the beacon (on / off), while calling the service with TRUE will 
 switch it to ON or change its flickering frequency (0 to 10 times per second). 
 For this project it is essential to pre-fix the beacon mode at Constant light mode.

### beacon_subs
 The project uses thetopic of the *local_publisher* (a ros2 publisher included in the examples) to control the fliskering intervals of the beacon according to the published values, curently the values published are from 0 to 100, incrementing by 10 each 10 seconds to allow for testing and monitoring the final behavior.

### Inportant Documents
 Please follow both *"Installing a Micro-Ros Agent on a ESP32 (blinking and LED)"* and *"ESP32 controlling RS 907-5965 via Micro-Ros Agent over WiFi"* documents for step-by-step dlasing and running the codes, along with important comments on the code for further modifications.

## License

This repository is modified for and ow managed by Saxion University for Applied Science.

