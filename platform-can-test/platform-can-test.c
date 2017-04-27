/* 
   Author: Ashwin Narayan
   Date: Thursday, April 13, 2017

   This can be used as a template when implementing new controllers on the 
   platform. It implements a real time task that samples from the CAN BUS
   in every loop to read the sensors from each ASOC.
*/

//Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <sys/mman.h>

//Xenomai includes
#include <rtdm/can.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>

#define LOOP_PERIOD 1*1e9 //ns
#define CAN_DEVICE_NAME "rtcan0"
#define CAN_DEVICE_BAUDRATE 1000000
#define BTR0_VAL 0x00 // From https://www.kvaser.com/support/calculators/bit-timing-calculator/
#define BTR1_VAL 0x1c
#define CAN_CTRL_MODE 0
#define MAX_LOOP_COUNT 10

#define LEFT_ASOC_ID 0xf 
#define RIGHT_ASOC_ID 0x10
#define MSG_ID_ESTOP 0x00 << 5
#define MSG_ID_SET_VEL 0x1 << 5
#define MSG_ID_SET_PWM 0x5 << 5
#define MSG_ID_GET_POS 0x3 << 5
#define MSG_ID_GET_ENC 0x6 << 5
#define MSG_ID_HBT 0x2 << 5
#define MSG_ID_MEN 0x4 << 5
#define PRIO_ESTOP 0x0 << 9
#define PRIO_SET 0x1 << 9
#define PRIO_GET 0x2 << 9
#define PRIO_MEN 0x2 << 9
#define PRIO_HBT 0x3 << 9

#define READ_WITH_REQ 0
#define READ_WITHOUT_REQ 1

//#define CAN_MODE 

//Global defines
RT_TASK main_task;            //The main real-time task

static int can_fd = -1;                //The can socket file descriptor
static struct sockaddr_can platform_addr; //The CAN address of the interface
static struct can_ifreq ifr;                //Holds more info about the can bus
struct can_bittime *bittime;
struct can_frame frame;
struct can_frame rx_frame;

static int vll, vlr, vrl, vrr;  //The wheel velocities
static int alpha1, alpha2;      //The absolute encoders

static inline void initialize_can_bus();
static inline void stop_can_bus();
static inline void send_estop();
static inline void enable_motors();
static inline void read_sensors(int READ_MODE);
static inline void set_velocities(int vll_d, int vlr_d, int vrl_d, int vrr_d);
//static inline void set_wheel_velocities(int vll, int vlr, int vrl, int vrr);
//static inline void request_wheel_velocities(int asoc_id);
//static inline void read_wheel_velocities()

static void main_task_proc(void *args)
{
    int loop_ctr = 0, nbytes=0;
    uint8_t i;
    //Set up the the CAN Bus
    initialize_can_bus();
    //Enable the motors
    enable_motors();

    //Fill up the frame
    frame.can_id = 0x01;
    frame.can_dlc = 8;
    for(i=0; i<8; i++)
	frame.data[i] = i;

    //Set all wheel velocities to zero
    set_velocities(0, 0, 0, 0);

    //Make the task periodic
    rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);   

    while(loop_ctr < MAX_LOOP_COUNT)
    {
	printf("Loop Count: %d\n", loop_ctr);
	loop_ctr++;
	printf("Sending CAN Frame...\n");
	
	read_sensors(READ_WITHOUT_REQ);

	//Do control and get desired velocities

	set_velocities(0, 0, 0, 0);
	
	//Wait till end of the task period.
	rt_task_wait_period(NULL);
    }
	
    //Stop the can bus
    stop_can_bus();
}


int main(int argc, char **argv)
{
    char str[20];

    //Lock memory to prevent swapping
    mlockall(MCL_CURRENT | MCL_FUTURE);

    //Create a name for the task
    sprintf(str, "Lorenz_Task");
    rt_task_create(&main_task, str, 0, 50, 0);
    
    //Start the real time task
    rt_task_start(&main_task, &main_task_proc, 0);

    //Wait for CTRL-C
    pause();

    

    return 0;
}

static inline void set_velocities(int vll_d, int vlr_d, int vrl_d, int vrr_d)
{
    frame.can_dlc = 4;
    frame.data[0] = 0xff;
    frame.data[1] = 0x00;

    //Set velocities of left ASOC
    frame.can_id = PRIO_SET | MSG_ID_SET_VEL | LEFT_ASOC_ID;
    sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	   (struct sockaddr *)&platform_addr, 
	   sizeof(platform_addr));

    //Set the velocities of the right ASOC
    frame.can_id = PRIO_SET | MSG_ID_SET_VEL | RIGHT_ASOC_ID;
    sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	   (struct sockaddr *)&platform_addr, 
	   sizeof(platform_addr));

}

static inline void read_sensors(int READ_MODE)
{
    int nbytes;
    socklen_t len = sizeof(platform_addr);

    frame.can_dlc = 0;
    //Request wheel encoders from left ASOC
    if(READ_MODE == READ_WITH_REQ){
	frame.can_id = PRIO_GET | MSG_ID_GET_POS | LEFT_ASOC_ID | CAN_RTR_FLAG;
	sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	       (struct sockaddr *)&platform_addr, 
	       sizeof(platform_addr));
    }
    //Get the data and write to global variable
    nbytes = recvfrom(can_fd, &rx_frame, sizeof(struct can_frame),
		      0, (struct sockaddr*)&platform_addr, &len);
    
    
    //Request wheel encoders from the right ASOC
    if(READ_MODE == READ_WITH_REQ){
	frame.can_id = PRIO_GET | MSG_ID_GET_POS | RIGHT_ASOC_ID | CAN_RTR_FLAG;
	sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	       (struct sockaddr *)&platform_addr, 
	       sizeof(platform_addr));
    }

    //Get the data and write to global variable
    nbytes = recvfrom(can_fd, &rx_frame, sizeof(struct can_frame),
		      0, (struct sockaddr*)&platform_addr, &len);

    //Request absolute encoders from the left ASOC
    if(READ_MODE == READ_WITH_REQ){
	frame.can_id = PRIO_GET | MSG_ID_GET_ENC | LEFT_ASOC_ID | CAN_RTR_FLAG;
	sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	       (struct sockaddr *)&platform_addr, 
	       sizeof(platform_addr));
    }

    //Get the data and write to global variable
    nbytes = recvfrom(can_fd, &rx_frame, sizeof(struct can_frame),
		      0, (struct sockaddr*)&platform_addr, &len);
    
    //Request absolute encoders from the right ASOC
    if(READ_MODE == READ_WITH_REQ){
	frame.can_id = PRIO_GET | MSG_ID_GET_ENC | RIGHT_ASOC_ID | CAN_RTR_FLAG;
	sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	       (struct sockaddr *)&platform_addr, 
	       sizeof(platform_addr));
    }

    //Get the data and write to global variable
    nbytes = recvfrom(can_fd, &rx_frame, sizeof(struct can_frame),
		      0, (struct sockaddr*)&platform_addr, &len);
    
}

static inline void enable_motors()
{
    frame.can_id = PRIO_MEN | MSG_ID_MEN | LEFT_ASOC_ID;
    frame.can_dlc = 1;
    
    frame.data[0] = 1;

    sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	   (struct sockaddr *)&platform_addr, 
	   sizeof(platform_addr));

    frame.can_id = PRIO_MEN | MSG_ID_MEN | RIGHT_ASOC_ID;
    sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	   (struct sockaddr *)&platform_addr, 
	   sizeof(platform_addr));
}

static inline void send_estop()
{
    uint8_t i;
    frame.can_id = 0x00 | PRIO_ESTOP | MSG_ID_ESTOP;
    frame.can_dlc = 8;
    
    for(i=0; i<8; i++)
	frame.data[i] = 0;

    sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
	   (struct sockaddr *)&platform_addr, 
	   sizeof(platform_addr));
}

static inline void initialize_can_bus()
{
    int ret;
    char com[200];
    printf("Initializing the can bus...\n");
    //Initialize the socket
    printf("Initializing socket....\n");
    can_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(can_fd < 0){
	printf("Cannot open RTDM CAN Socket. %d\n", can_fd);
	stop_can_bus();
    }
    
    //Specify the name of the can device
    printf("Getting interface %s\n", CAN_DEVICE_NAME);
    strncpy(ifr.ifr_name, CAN_DEVICE_NAME, IFNAMSIZ);
    ret = ioctl(can_fd, SIOCGIFINDEX, &ifr);
    if(ret){
	printf("Can't get interface index for %s, ERRNO: %d\n",
	       CAN_DEVICE_NAME, ret);
	stop_can_bus();
    }
    
    //Set the correct socket options for using sendto()
    ret = setsockopt(can_fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
    if(ret){
	printf("Error setting sockopts!\n");
    }

    //Populate can Address
    printf("Populating the can address struct...\n");
    platform_addr.can_ifindex = ifr.ifr_ifindex;
    platform_addr.can_family = AF_CAN;

    //Calling rtcanconfig to do the rest
    printf("Calling rtcanconfig to do the rest...\n");
    sprintf(com, "sudo /usr/xenomai/sbin/rtcanconfig %s -b %d -v start",
	    CAN_DEVICE_NAME, CAN_DEVICE_BAUDRATE);
    printf("%s \n", com);
    system(com);

    printf("Finished initializing the CAN Bus\n");
}

static inline void stop_can_bus()
{
    char com[200];
    int ret;
    if(can_fd > 0)
    {
	ret = close(can_fd);
	if(ret){
	    printf("Close error\n");
	}
    }
    sprintf(com, "sudo /usr/xenomai/sbin/rtcanconfig %s -b %d -v stop",
	    CAN_DEVICE_NAME, CAN_DEVICE_BAUDRATE);
    printf("Calling rtcanconfig to stop the can bus\n");
    printf("%s \n", com);
    system(com);
}
