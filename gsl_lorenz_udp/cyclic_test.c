#include <stdio.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

#define ORDER 3

int sys_fun(double t, const double y[], double f[], void *params)
{
    (void) t; //Avoid unused parameter warning
    double sigma = 10.0, rho = 28.0, beta = 8.0/3.0;

    f[0] = sigma*(y[1] - y[0]);
    f[1] = y[0]*(rho - y[2]) - y[1];
    f[2] = y[0]*y[1] - beta*y[2];

    return GSL_SUCCESS;
}


int main(int argc, char **argv)
{
    const gsl_odeiv2_step_type *T = gsl_odeiv2_step_rkf45;

    gsl_odeiv2_step *s = gsl_odeiv2_step_alloc(T, ORDER);
    gsl_odeiv2_control *c = gsl_odeiv2_control_y_new(1e-3, 0.0);
    gsl_odeiv2_evolve *e = gsl_odeiv2_evolve_alloc(ORDER);

    gsl_odeiv2_system lorenz_system = {sys_fun, NULL, ORDER, NULL};

    double t = 0.0, t1 = 100;
    double h = 1e-6;
    double y[3] = {1.0, 1.0, 1.0}; //Initial conditions

    while(t < t1)
    {
	int status = gsl_odeiv2_evolve_apply(e, c, s, &lorenz_system, &t, t1, &h, y);

	if(status != GSL_SUCCESS) break;

	printf("%.5e %.5e %.5e %.5e\n", t, y[0], y[1], y[2]);
    }

    gsl_odeiv2_evolve_free (e);
    gsl_odeiv2_control_free (c);
    gsl_odeiv2_step_free (s);

    return 0;   

}

    
