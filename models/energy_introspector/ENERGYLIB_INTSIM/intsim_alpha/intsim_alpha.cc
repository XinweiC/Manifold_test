/*
	This simulator is C/C++ version of IntSim.

	Original copy was developed by D. Sekar, R. Venkatesan, R. Sarvari, K. Shakeri,
	J. Davis, and Prof. J. Meindl from Microelectronics Research Center,
	Georgia Tech, Atlanta, GA.
	
	Reference: D. Sekar, "Optimal Signal, Power, Clock, and Thermal Interconnect
	Networks for High-Performance 2D and 3D Integrated Circuits," Ph.D. Dissertation,
	School of ECE, Georgia Tech, Atlanta, GA, 2008.

	Copyright 2012
	William Song and Sudhakar Yalamanchili
	Georgia Institute of Technology, Atlanta, GA 30332
*/

#include "intsim.h"

int main(void)
{
  intsim_chip_t *chip = new intsim_chip_t();
  intsim_param_t *param = new intsim_param_t();

  param->setup(/*use_default_parameters*/true);

  intsim(chip,param,(char*)"intsim.out");

  chip->exit();

  return 0;
}

