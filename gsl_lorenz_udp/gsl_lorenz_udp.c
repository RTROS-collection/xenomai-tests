#include <stdio.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <math.h>

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
    const gsl_odeiv2_step_type *T = gsl_odeiv2_step_rk4;
    gsl_odeiv2_step *s = gsl_odeiv2_step_alloc(T, ORDER);
    gsl_odeiv2_control *c = gsl_odeiv2_control_y_new(1e-2, 0.0);
    gsl_odeiv2_evolve *e = gsl_odeiv2_evolve_alloc(ORDER);

    gsl_odeiv2_system lorenz_system = {sys_fun, NULL, ORDER, NULL};

    double t = 0.0, t1 = 10;
    double h = 0.01;

    double y[3] = {1.0, 1.0, 1.0};

    rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);

    while(t < t1)
    {
	int status = gsl_odeiv2_evolve_apply(e, NULL, s, &lorenz_system, &t, t1, &h, y);
	printf("Integrator: t=%.3f, RTClock: t=%.3f\n", t, rt_timer_read()/1e9);
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

    
