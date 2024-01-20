/****************************************************************************
 *
 * Copyright 2020 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @brief Offboard control example
 * @file offboard_control.cpp
 * @addtogroup examples
 * @author Mickey Cowden <info@cowden.tech>
 * @author Nuno Marques <nuno.marques@dronesolutions.io>
 */

#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/srv/vehicle_command.hpp>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace px4_msgs::msg;

// TODO: Try to create a class such as waypoint or the embedded control node can derive from
class OffboardControl : public rclcpp::Node {
public:
    OffboardControl() : Node("leader_control") {

        const std::string name_space{this->get_namespace()};

        // TODO: Check how to set sim_time to true
        arm_check_client_ = this->create_client<px4_msgs::srv::VehicleCommand>(
                name_space + "/fmu/vehicle_command");

        // Wait for the service to become available
        if (!arm_check_client_->wait_for_service(std::chrono::seconds(30))) {
            RCLCPP_ERROR(this->get_logger(), "Service not available");
        }
        RCLCPP_ERROR(this->get_logger(), "Vehicle command service available");
        // Set to onboard_mode
        offboard_mode();
        // Set to onboard_mode
        arm();


    }

    void arm();

    void offboard_mode();

    void disarm();

private:

    rclcpp::Client<px4_msgs::srv::VehicleCommand>::SharedPtr arm_check_client_;
    const std::size_t max_client_connexions_try{100};
    bool is_armed{false};
    bool is_offboard{false};


    bool request_vehicle_command(uint16_t command, float param1 = 0.0, float param2 = 0.0);
};

/**
 * @brief Send a command to Arm the vehicle
 */
void OffboardControl::arm() {
    is_armed = request_vehicle_command(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0);
    if (is_armed) {
        RCLCPP_INFO(this->get_logger(), "Drone armed !");
    } else {
        RCLCPP_INFO(this->get_logger(), "Didn't succeed to arm the drone");
    }
}

/**
 * @brief Send a command to switch in OffBoard Mode
 */
void OffboardControl::offboard_mode() {
    is_offboard = request_vehicle_command(VehicleCommand::VEHICLE_CMD_DO_SET_MODE, 1, 6);
    if (is_offboard) {
        RCLCPP_INFO(this->get_logger(), "Drone is in offboard mode !");
    } else {
        RCLCPP_INFO(this->get_logger(), "Didn't succeed to switch to offboard mode");
    }
}

/**
 * @brief Send a command to Disarm the vehicle
 */
void OffboardControl::disarm() {
    request_vehicle_command(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 0.0);

    RCLCPP_INFO(this->get_logger(), "Disarm command send");
}

/**
 * @brief Request the vehicle command service (Arming, switch to offboard mode...)
 */
bool OffboardControl::request_vehicle_command(uint16_t command, float param1, float param2) {
    bool request_success{false};
    for (auto call{0u}; call < max_client_connexions_try; ++call) {
        auto request{std::make_shared<px4_msgs::srv::VehicleCommand::Request>()};
        request->request.set__command(command);
        request->request.set__param1(param1);
        request->request.set__param2(param2);
        // We don't know why but target_system make the request wait a lot
//      request->request.target_system = 2;
        request->request.target_component = 1;
        request->request.source_system = 1;
        request->request.source_component = 1;
        request->request.from_external = true;
//
//            // Calculate timestamp
        request->request.timestamp = this->get_clock()->now().nanoseconds() / 1000;
        // Send the request
        // TODO: Future object can't be called in create_wall_timer... Therefore, the only way seems to increase max_client_connexions_try
        auto result_future{arm_check_client_->async_send_request(request)};

        // Wait for the response
        rclcpp::spin_until_future_complete(this->get_node_base_interface(), result_future);
        auto response{result_future.get()};
        if (response) {
            // Handle the response
            auto result{response->reply.result};
            RCLCPP_INFO(this->get_logger(), "Vehicle Command Service connexion succeeded with result: %d",
                        result);
            if (!result) {
                RCLCPP_INFO(this->get_logger(), "Command succeeded on attempt n°%d", call);
                request_success = true;
                break;
            } else {
                RCLCPP_INFO(this->get_logger(), "Command failed on attempt n°%d", call);
                continue;
            }
        } else {
            RCLCPP_ERROR(this->get_logger(), "Vehicle Command Service call failed");
        }
    }
    return request_success;
}

int main(int argc, char *argv[]) {
    std::cout << "Starting offboard control node..." << std::endl;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OffboardControl>());

    rclcpp::shutdown();
    return 0;
}
