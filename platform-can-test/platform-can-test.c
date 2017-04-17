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
#define CAN_DEVICE_BAUDRATE 500000
#define BTR0_VAL 0x00 // From https://www.kvaser.com/support/calculators/bit-timing-calculator/
#define BTR1_VAL 0x32
#define CAN_CTRL_MODE 0
#define MAX_LOOP_COUNT 10
//#define CAN_MODE 

//Global defines
RT_TASK main_task;            //The main real-time task

static int can_fd = -1;                //The can socket file descriptor
static struct sockaddr_can l_addr;     //The can address for the left wheel
static struct sockaddr_can r_addr;     //The can address for the right wheel
static socklen_t lenl = sizeof(l_addr);
static socklen_t lenr = sizeof(r_addr);
static struct can_ifreq ifr;                //Holds more info about the can bus
struct can_bittime *bittime;
struct can_frame frame;

static inline void initialize_can_bus();
static inline void stop_can_bus();

static void main_task_proc(void *args)
{
    int loop_ctr = 0, nbytes=0, i;
    //Set up the the CAN Bus
    initialize_can_bus();

    //Populate can Address
    l_addr.can_ifindex = ifr.ifr_ifindex;
    l_addr.can_family = AF_CAN;
    
    //Fill up the frame
    frame.can_id = 0x00;
    frame.can_dlc = 8;
    for(i=0; i<8; i++)
	frame.data[0] = i;

    //Make the task periodic
    rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);   

    while(loop_ctr < MAX_LOOP_COUNT)
    {
	printf("Loop Count: %d", loop_ctr);
	loop_ctr++;
	printf("Sending CAN Frame...\n");
	nbytes= sendto(can_fd, &frame, sizeof(struct can_frame), 0, 
		       (struct sockaddr *)&l_addr, sizeof(l_addr));
	printf("Send %d bytes of data over CAN bus\n", nbytes); 
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

static inline void initialize_can_bus()
{
    int ret;
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
    
    //Specify the baud rate of the CAN bus
    printf("Setting bus baud rate to %d bps", CAN_DEVICE_BAUDRATE);
    ifr.ifr_ifru.baudrate = CAN_DEVICE_BAUDRATE;
    ret = ioctl(can_fd, SIOCSCANBAUDRATE, &ifr);
    if(ret){
	printf("Error setting bitrate.\n");
	stop_can_bus();
    }

    //Bit timings
    bittime = &ifr.ifr_ifru.bittime;
    bittime->type = CAN_BITTIME_BTR;
    bittime->btr.btr0 = 0x00;
    bittime->btr.btr1 = 0x32;
    printf("Setting bit timings... BTR0=0x%02x BTR1=0x%02x\n", BTR0_VAL, BTR1_VAL);
    ret = ioctl(can_fd, SIOCSCANCUSTOMBITTIME, &ifr);
    if(ret){
	printf("Error when setting the bit timings...\n");
	stop_can_bus();
    }

    //Setting control mode
    ifr.ifr_ifru.ctrlmode = CAN_CTRL_MODE;
    printf("Setting control mode...\n");
    ret = ioctl(can_fd, SIOCSCANCTRLMODE, &ifr);
    if(ret){
	printf("Error setting control mode\n");
	stop_can_bus();
    }

    //Setting can mode
    ifr.ifr_ifru.mode = CAN_MODE_START;
    printf("Setting CAN Mode\n");
    ret = ioctl(can_fd, SIOCSCANMODE, &ifr);
    if(ret){
	printf("Error setting can mode to start!\n");
	stop_can_bus();
    }

    printf("Finished initializing the CAN Bus\n");
}

static inline void stop_can_bus()
{
    int ret;
    if(can_fd > 0)
    {
	ret = close(can_fd);
	if(ret){
	    printf("Close error\n");
	}
	exit(EXIT_SUCCESS);
    }
}
