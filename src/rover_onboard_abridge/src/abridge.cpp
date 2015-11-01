#include <ros/ros.h>

//ROS libraries
#include <tf/transform_datatypes.h>

//ROS messages
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Quaternion.h>
#include <geometry_msgs/Twist.h>
#include <sensor_msgs/Imu.h>

//Package include
#include <usbSerial.h>

using namespace std;

void cmdHandler(const geometry_msgs::Twist::ConstPtr& message);
void serialActivityTimer(const ros::TimerEvent& e);
void publishRosTopics();
void parseData(string data);

char host[128];
char delimiter = ',';
vector<string> dataSet;

USBSerial usb;
const int baud = 115200;
char dataCmd[] = "d\n";
char moveCmd[16];

std_msgs::Float64MultiArray sonar;
sensor_msgs::Imu imu;

ros::Subscriber moveSubscriber;

ros::Publisher imuPublish;
ros::Publisher sonarPublish;
ros::Timer publishTimer;

float linearSpeed = 0.;
float turnSpeed = 0.;

const float deltaTime = 0.25; //delta t, in seconds

int main(int argc, char **argv) {

    gethostname(host, sizeof (host));
    string hostname(host);
    string publishedName;
    ros::init(argc, argv, (hostname + "_ABRIDGE"));

    ros::NodeHandle param("~");
    string devicePath;
    param.param("device", devicePath, string("/dev/ttyUSB0"));
    usb.openUSBPort(devicePath, baud);

    sleep(5);

    ros::NodeHandle aNH;
    
    if (argc >= 2) {
        publishedName = argv[1];
        cout << "Welcome to the world of tomorrow " << publishedName << "!  ABridge module started." << endl;
    } else {
        publishedName = hostname;
        cout << "No Name Selected. Default is: " << publishedName << endl;
    }

    moveSubscriber = aNH.subscribe((publishedName + "/mobility"), 10, cmdHandler);

    imuPublish = aNH.advertise<sensor_msgs::Imu>((publishedName + "/imu"), 10);    
    sonarPublish = aNH.advertise<std_msgs::Float64MultiArray>((publishedName + "/sonar"), 10);
    
    publishTimer = aNH.createTimer(ros::Duration(deltaTime), serialActivityTimer);
    
    sonar.layout.dim[0].label = "left_center_right";
    sonar.layout.dim[0].size = 3;
    sonar.layout.dim[0].stride = 3;

    ros::spin();

    return EXIT_SUCCESS;
}

void cmdHandler(const geometry_msgs::Twist::ConstPtr& message) {
    // remove artificial factor that was multiplied for simulation. this scales it back down to -1.0 to +1.0
    linearSpeed = (message->linear.x) / 1.5;
    turnSpeed = (message->angular.z) / 8;
    
    if (linearSpeed != 0.) {
        sprintf(moveCmd, "m,%d\n", (int) (linearSpeed * 255));
        usb.sendData(moveCmd);
    } else if (turnSpeed != 0.) {
        sprintf(moveCmd, "t,%d\n", (int) (turnSpeed * 255));
        usb.sendData(moveCmd);
    } else {
        sprintf(moveCmd, "s\n");
        usb.sendData(moveCmd);
    }

    memset(&moveCmd, '\0', sizeof (moveCmd));
}

void serialActivityTimer(const ros::TimerEvent& e) {
    usb.sendData(dataCmd);
    parseData(usb.readData());
    publishRosTopics();
}

void publishRosTopics() {
    sonarPublish.publish(sonar);
    imuPublish.publish(imu);
}

void parseData(string str) {
    istringstream oss(str);
    string word;
    while (getline(oss, word, delimiter)) {
        dataSet.push_back(word);
    }
    if (dataSet.size() == 10) {
        imu.orientation = tf::createQuaternionMsgFromYaw(atof(dataSet.at(0).c_str()) * M_PI / 180.0);
        imu.orientation.z = fmod(-1 * imu.orientation.z, 360.0);
        imu.linear_acceleration.x = atof(dataSet.at(1).c_str());
        imu.linear_acceleration.y = atof(dataSet.at(2).c_str());
        imu.linear_acceleration.z = atof(dataSet.at(3).c_str());
        imu.angular_velocity.x = atof(dataSet.at(4).c_str());
        imu.angular_velocity.y = atof(dataSet.at(5).c_str());
        imu.angular_velocity.z = atof(dataSet.at(6).c_str());
        sonar.data[0] = atof(dataSet.at(7).c_str()) / 100.0;
        sonar.data[1] = atof(dataSet.at(8).c_str()) / 100.0;
        sonar.data[2] = atof(dataSet.at(9).c_str()) / 100.0;
    }
    
    dataSet.clear();
}
