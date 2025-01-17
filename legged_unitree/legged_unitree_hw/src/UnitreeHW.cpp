//
// Created by qiayuan on 1/24/22.
// Adapted for Ylo2 robot - vincent foucault - 2023-02
//

#include "legged_unitree_hw/UnitreeHW.h"
#include "moteus_driver/YloTwoPcanToMoteus.hpp" // ylo2 library
#include "sensor_msgs/Imu.h"

YloTwoPcanToMoteus command; // instance of class YloTwoPcanToMoteus

namespace legged {

bool UnitreeHW::startup_routine()
{
  //command.peak_fdcan_board_initialization();
  //usleep(200);// TODO : test and reduce pause !
  //command.check_initial_ground_pose();
  std::cout << "--- startup_routine Done. ---" << std::endl;
  //usleep(200);// TODO : test and reduce pause !
  return true;
}

float imuorientx = 0.0;

// the imu callback
void UnitreeHW::ImuCallback(const sensor_msgs::Imu::ConstPtr& imu_message){
  
  ROS_INFO("****************** Imu Orientation x: [%f], y: [%f], z: [%f], w: [%f]", imu_message->orientation.x,
          imu_message->orientation.y,imu_message->orientation.z,imu_message->orientation.w);
  float imuorientx = imu_message->orientation.x;
}


bool UnitreeHW::init(ros::NodeHandle& root_nh, ros::NodeHandle& robot_hw_nh) {
  if (!LeggedHW::init(root_nh, robot_hw_nh)) {
    return false;
  }

  robot_hw_nh.getParam("power_limit", powerLimit_);

  // Imu subscriber 
  ros::Subscriber sub = root_nh.subscribe("imu/data", 1000, &UnitreeHW::ImuCallback, this);

  setupJoints();
  setupImu();
  setupContactSensor(robot_hw_nh);

  std::string robot_type;
  root_nh.getParam("robot_type", robot_type);
  if (robot_type == "a1") {
    std::cout << "robot is : " << robot_type << std::endl;
  } else if (robot_type == "aliengo") {
    std::cout << "robot is : " << robot_type << std::endl;
  } else {
    ROS_FATAL("Unknown robot type: %s", robot_type.c_str());
    return false;
  }
  return true;
}

void UnitreeHW::read(const ros::Time& /*time*/, const ros::Duration& /*period*/) {
  //ROS_INFO("read function");

  // read the 12 joints, and store values into legged controller
  for (int i = 0; i < 12; ++i) {

    // Reset values
    RX_pos = 0.0;
    RX_vel = 0.0;
    RX_tor = 0.0;
    RX_volt = 0.0;
    RX_temp = 0.0;
    RX_fault = 0.0;

    auto ids  = command.motor_adapters_[i].getIdx();
    int port  = command.motor_adapters_[i].getPort();
    auto sign = command.motor_adapters_[i].getSign();

    // call ylo2 moteus lib
  /*
    command.read_moteus_RX_queue(ids, port, 
                                  RX_pos, RX_vel, RX_tor, 
                                  RX_volt, RX_temp, RX_fault);  // query values;
*/
    //usleep(10); // TODO : test and reduce pause !

    // ex : jointData_ = [(pos_, vel_, tau_, posDes_, velDes_, kp_, kd_, ff_), (pos_, vel_, tau_, posDes_, velDes_, kp_, kd_, ff_),...]
    jointData_[i].pos_ = static_cast<double>(sign*(RX_pos*2*M_PI)); // joint turns to radians
    jointData_[i].vel_ = static_cast<double>(RX_vel);   // measured in revolutions / s
    jointData_[i].tau_ = static_cast<double>(RX_tor);   // measured in N*m

    // TODO read volt, temp, faults for Diagnostics

    //usleep(200); // TODO : test and reduce pause !
  }

  // TODO
  // imu_message contains the real imu topic msg
  // read imu message, and store values into legged controller
  //ROS_INFO("read imu => [%f]", imuorientx);
  /*
  imu_message = imu_callback(); _// read imu full message_

  imuData_.ori_[0]        = imu_message->orientation.x;
  imuData_.ori_[1]        = imu_message->orientation.y;
  imuData_.ori_[2]        = imu_message->orientation.z;
  imuData_.ori_[3]        = imu_message->orientation.w; // TODO : is this 4rd index w, or x ?

  imuData_.angularVel_[0] = imu_message->angular_velocity.x;
  imuData_.angularVel_[1] = imu_message->angular_velocity.y;
  imuData_.angularVel_[2] = imu_message->angular_velocity.z;
  imuData_.linearAcc_[0]  = imu_message->linear_acceleration.x;
  imuData_.linearAcc_[1]  = imu_message->linear_acceleration.y;
  imuData_.linearAcc_[2]  = imu_message->linear_acceleration.z;
  */
  imuData_.ori_[0] = 0.0000001;
  imuData_.ori_[1] = 0.0000001;
  imuData_.ori_[2] = 0.0000001;
  imuData_.ori_[3] = 1.0;
  imuData_.angularVel_[0] = 0.0000001;
  imuData_.angularVel_[1] = 0.0000001;
  imuData_.angularVel_[2] = 0.0000001;
  imuData_.linearAcc_[0] = 0.0000001;
  imuData_.linearAcc_[1] = 0.0000001;
  imuData_.linearAcc_[2] = 0.0000001;

  // Set feedforward and velocity cmd to zero to avoid for safety when not controller setCommand
  std::vector<std::string> names = hybridJointInterface_.getNames();
  for (const auto& name : names) {
    HybridJointHandle handle = hybridJointInterface_.getHandle(name);
    handle.setFeedforward(0.);
    handle.setVelocityDesired(0.);
    handle.setKd(3.);
  }
}

void UnitreeHW::write(const ros::Time& /*time*/, const ros::Duration& /*period*/) {
  //ROS_INFO("write function");
  for (int i = 0; i < 12; ++i) {
    auto ids  = command.motor_adapters_[i].getIdx(); // moteus controller id
    int port  = command.motor_adapters_[i].getPort(); // select correct port on Peak canfd board
    auto sign = command.motor_adapters_[i].getSign(); // in case of joint reverse rotation
    
    joint_position = static_cast<float>(jointData_[i].posDes_);;
    joint_velocity = static_cast<float>(jointData_[i].velDes_);
    joint_fftorque = static_cast<float>(jointData_[i].ff_);
    joint_kp       = static_cast<float>(jointData_[i].kp_);
    joint_kd       = static_cast<float>(jointData_[i].kd_);

    // call ylo2 moteus lib
/*
    command.send_moteus_TX_frame(ids, port, 
                                joint_position, joint_velocity, joint_fftorque, joint_kp, joint_kd); 
    //usleep(120); // TODO : test and reduce pause !
*/
  }
}

bool UnitreeHW::setupJoints() {
  for (const auto& joint : urdfModel_->joints_) {
    int leg_index = 0;
    int joint_index = 0;
    if (joint.first.find("RF") != std::string::npos) {
      leg_index = 0;
    } else if (joint.first.find("LF") != std::string::npos) {
      leg_index = 1;
    } else if (joint.first.find("RH") != std::string::npos) {
      leg_index = 2;
    } else if (joint.first.find("LH") != std::string::npos) {
      leg_index = 3;
    } else {
      continue;
    }

    if (joint.first.find("HAA") != std::string::npos) {
      joint_index = 0; // ABAD
    } else if (joint.first.find("HFE") != std::string::npos) {
      joint_index = 1; // UPPER LEG
    } else if (joint.first.find("KFE") != std::string::npos) {
      joint_index = 2; // LOWER LEG
    } else {
      continue;
    }

    // FOR LEGGED HARDWARE
    int index = leg_index * 3 + joint_index;
    hardware_interface::JointStateHandle state_handle(joint.first, &jointData_[index].pos_, &jointData_[index].vel_,
                                                      &jointData_[index].tau_);
    jointStateInterface_.registerHandle(state_handle);
    hybridJointInterface_.registerHandle(HybridJointHandle(state_handle, &jointData_[index].posDes_, &jointData_[index].velDes_,
                                                           &jointData_[index].kp_, &jointData_[index].kd_, &jointData_[index].ff_));
  }
  return true;
}

bool UnitreeHW::setupImu() {
  imuSensorInterface_.registerHandle(hardware_interface::ImuSensorHandle("unitree_imu", "unitree_imu", imuData_.ori_, imuData_.oriCov_,
                                                                         imuData_.angularVel_, imuData_.angularVelCov_, imuData_.linearAcc_,
                                                                         imuData_.linearAccCov_));
  imuData_.oriCov_[0] = 0.0012;
  imuData_.oriCov_[4] = 0.0012;
  imuData_.oriCov_[8] = 0.0012;

  imuData_.angularVelCov_[0] = 0.0004;
  imuData_.angularVelCov_[4] = 0.0004;
  imuData_.angularVelCov_[8] = 0.0004;

  return true;
}

bool UnitreeHW::setupContactSensor(ros::NodeHandle& nh) {
  nh.getParam("contact_threshold", contactThreshold_);
  for (size_t i = 0; i < CONTACT_SENSOR_NAMES.size(); ++i) {
    contactSensorInterface_.registerHandle(ContactSensorHandle(CONTACT_SENSOR_NAMES[i], &contactState_[i]));
  }
  return true;
}

}  // namespace legged
