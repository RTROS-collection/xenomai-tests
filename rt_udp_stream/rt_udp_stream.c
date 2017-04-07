#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <rtdm/rtdm.h>

#define CLOCK_RES 1e-9 //Clock resolution is 1 ns by default
#define LOOP_PERIOD 1e9 //Expressed in ticks
//RTIME period = 1000000000;
RT_TASK loop_task;

void loop_task_proc(void *arg)
{
  RT_TASK *curtask;
  RT_TASK_INFO curtaskinfo;
  int iret = 0;
  char *msg = 'Hello World!!\n';
  int sockfd;

  RTIME tstart, now;

  curtask = rt_task_self();
  rt_task_inquire(curtask, &curtaskinfo);
  int ctr = 0;

  //Declare soscket descriptor structs
  static struct sockaddr_in local_addr;
  static struct sockaddr_in server_addr;

  //Clear their memory
  memset(&local_addr, 0, sizeof(struct sockaddr_in));
  memset(&server_addr, 0, sizeof(struct sockaddr_in));

  local_addr.sin_family = AF_INET;
  local_addr.sin_addr.s_addr = INADDR_ANY;
  local_addr.sin_port = htons(9750);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = rt_inet_aton('10.42.0.1');
  server_addr.sin_port = htons(9751);

  sockfd = rtdm_socket(AF_INET, SOCK_DGRAM, 0);

  //Connect the socket to the remote port 
  rtdm_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));

  //Send initialization message
  rtdm_send(sockfd, msg, sizeof(msg), 0); 

  //Print the info
  printf("Starting task %s with period of 10 ms ....\n", curtaskinfo.name);

  //Make the task periodic with a specified loop period
  rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);

  tstart = rt_timer_read();

  //Start the task loop
  while(ctr < 40){
    printf("Loop count: %d, Loop time: %.5f ms\n", ctr, (rt_timer_read() - tstart)/1000000.0);
    ctr++;
    rtdm_send(sockfd, msg, sizeof(msg), 0);
    rt_task_wait_period(NULL);
  }

  rtdm_close(sockfd);
}

int main(int argc, char **argv)
{
  char str[20];
  //Perform auto-init of rt_print buffers if the task does not do so
  //rt_print_auto_init(1);

  //Lock the memory to avoid memory swapping for this program
  mlockall(MCL_CURRENT | MCL_FUTURE);
    
  printf("Starting cyclic task...\n");

  //Create the real time task
  sprintf(str, "cyclic_task");
  rt_task_create(&loop_task, str, 0, 50, 0);

  //Since task starts in suspended mode, start task
  rt_task_start(&loop_task, &loop_task_proc, 0);

  //Wait until the real-time task is done
  //rt_task_join(&loop_task);

  pause();

  return 0;
}
