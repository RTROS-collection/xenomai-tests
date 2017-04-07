#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <math.h>
//#include <rtdm>

RT_TASK demo_task;

void demo(void *arg)
{
  RT_TASK *curtask;
  RT_TASK_INFO curtaskinfo;

  double i = 3.1415/4;
  double s = sin(i);
  //Get the info of the current task
  printf("Hello World!\n");
  printf("The sine of pi/4 is %f",s);
  curtask = rt_task_self();
  rt_task_inquire(curtask, &curtaskinfo);

  //Print the info
  printf("Task name: %s \n", curtaskinfo.name);
}

int main(int argc, char **argv)
{
  char str[10];
  //Perform auto-init of rt_print buffers if the task does not do so
  //rt_print_auto_init(1);

  //Lock the memory to avoid memory swapping for this program
  mlockall(MCL_CURRENT | MCL_FUTURE);
    
  printf("Starting task...\n");

  //Start the real time task
  sprintf(str, "hello");
  rt_task_create(&demo_task, str, 0, 50, 0);

  rt_task_start(&demo_task, &demo, 0);
}
