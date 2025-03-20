#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include <chrono>

using namespace std::chrono_literals;

class MyPublisher : public rclcpp::Node
{
public:
    MyPublisher() : Node("local_publisher"), count_(0)
    {
        publisher_ = this->create_publisher<std_msgs::msg::Int32>("publisher_topic", 10);
        timer_ = this->create_wall_timer(
            5000ms, std::bind(&MyPublisher::timer_callback, this));
    }

private:
    void timer_callback()
    {
        auto message = std_msgs::msg::Int32();
        count_ += 10;
        if (count_ > 100) count_ = 0;

        message.data = count_;
        
        RCLCPP_INFO(this->get_logger(), "Publishing: '%d'", message.data);
        publisher_->publish(message);
    }
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr publisher_;
    size_t count_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MyPublisher>());
    rclcpp::shutdown();
    return 0;
}