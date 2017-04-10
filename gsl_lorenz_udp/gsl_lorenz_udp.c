#include <stdio.h>
#include <string.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rtdm/rtdm.h>

#define ORDER 3
#define LOOP_PERIOD 0.01*1e9 //ns

RT_TASK main_task;

int sys_fun(double t, const double y[], double f[], void *params)
{
    (void) t; //Avoid unused parameter warning
    double sigma = 10.0, rho = 28.0, beta = 8.0/3.0;

    f[0] = sigma*(y[1] - y[0]);
    f[1] = y[0]*(rho - y[2]) - y[1];
    f[2] = y[0]*y[1] - beta*y[2];

    return GSL_SUCCESS;
}

void main_task_proc(void *args)
{
    //Set up the integrator the way we want it
    const gsl_odeiv2_step_type *T = gsl_odeiv2_step_rk4;
    gsl_odeiv2_step *s = gsl_odeiv2_step_alloc(T, ORDER);
    gsl_odeiv2_control *c = gsl_odeiv2_control_y_new(1e-2, 0.0);
    gsl_odeiv2_evolve *e = gsl_odeiv2_evolve_alloc(ORDER);

    gsl_odeiv2_system lorenz_system = {sys_fun, NULL, ORDER, NULL};

    double t = 0.0, t1 = 60;
    double h = 0.01;

    double y[3] = {1.0, 1.0, 1.0};

    char msg[50];

    //Set up the UDP socket for transmitting the data
    static struct sockaddr_in server_addr;
    //Clear the struct
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    //Create the socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //Populate the server address details
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("10.42.0.1");
    server_addr.sin_port = htons(9751);

    //Make the task periodic
    rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);

    while(t < t1)
    {
	//Do the integration
	int status = gsl_odeiv2_evolve_apply(e, NULL, s, &lorenz_system, &t, t1, &h, y);
	//printf("%3.3f, %3.3f, %3.3f |Integrator: t=%.3f, RTClock: t=%.3f\n", 
	//       y[0], y[1], y[2], t, rt_timer_read()/1e9);
	//Send the data over the socket
	//sprintf(msg, "%3.3f %3.3f %3.3f\n", y[0], y[1], y[2]);
	sprintf(msg, "0:%.3f\n1:%.3f\n2:%.3f\n", y[0], y[1], y[2]);
	sendto(sockfd, msg, strlen(msg), 0, &server_addr, sizeof(server_addr));
	//Wait till end of the task period.
	rt_task_wait_period(NULL);
    }
	
}


int main(int argc, char **argv)
{
    char str[20];

    //Lock memory to prevent swapping
    mlockall(MCL_CURRENT | MCL_FUTURE);

    sprintf(str, "Lorenz_Task");
    rt_task_create(&main_task, str, 0, 50, 0);
    
    rt_task_start(&main_task, &main_task_proc, 0);

    pause();

    return 0;
}

    
