/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator 

   Original Version:
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov 

   See the README file in the top-level LAMMPS directory. 

   ----------------------------------------------------------------------- 

   USER-CUDA Package and associated modifications:
   https://sourceforge.net/projects/lammpscuda/ 

   Christian Trott, christian.trott@tu-ilmenau.de
   Lars Winterfeld, lars.winterfeld@tu-ilmenau.de
   Theoretical Physics II, University of Technology Ilmenau, Germany 

   See the README file in the USER-CUDA directory. 

   This software is distributed under the GNU General Public License.
------------------------------------------------------------------------- */

#include <stdio.h>

#define _lj1 MY_AP(coeff1)
#define _lj2 MY_AP(coeff2)
#define _lj3 MY_AP(coeff3)
#define _lj4 MY_AP(coeff4)
#define _lj_type MY_AP(coeff5)

    enum {CG_NOT_SET=0, CG_LJ9_6, CG_LJ12_4, CG_LJ12_6, NUM_CG_TYPES,
          CG_COUL_NONE, CG_COUL_CUT, CG_COUL_DEBYE, CG_COUL_LONG};

#include "pair_lj_sdk_cuda_cu.h"
#include "pair_lj_sdk_cuda_kernel_nc.cu"
#include <time.h>




void Cuda_PairLJSDKCuda_Init(cuda_shared_data* sdata)
{
	Cuda_Pair_Init_AllStyles(sdata, 5, false, false );
	
}




void Cuda_PairLJSDKCuda(cuda_shared_data* sdata, cuda_shared_neighlist* sneighlist, int eflag, int vflag,int eflag_atom,int vflag_atom)
{
	
	// initialize only on first call
	static  short init=0;
	if(! init)
	{
		init = 1;
		Cuda_PairLJSDKCuda_Init(sdata);
	}
	
	dim3 grid,threads;
	int sharedperproc;
	
	int maxthreads=128;

	Cuda_Pair_PreKernel_AllStyles(sdata, sneighlist, eflag, vflag, grid, threads, sharedperproc,false,maxthreads);

	cudaStream_t* streams = (cudaStream_t*) CudaWrapper_returnStreams();
	if(sdata->pair.use_block_per_atom)
		Pair_Kernel_BpA<PAIR_CG_CMM,COUL_NONE,DATA_NONE>
		<<<grid, threads,sharedperproc*sizeof(ENERGY_FLOAT)*threads.x,streams[1]>>> (eflag, vflag,eflag_atom,vflag_atom);
	else
		Pair_Kernel_TpA<PAIR_CG_CMM,COUL_NONE,DATA_NONE>
		<<<grid, threads,sharedperproc*sizeof(ENERGY_FLOAT)*threads.x,streams[1]>>> (eflag, vflag,eflag_atom,vflag_atom);
	
	Cuda_Pair_PostKernel_AllStyles(sdata, grid, sharedperproc, eflag, vflag);	
}

#undef _lj1
#undef _lj2
#undef _lj3
#undef _lj4
#undef _lj_type

