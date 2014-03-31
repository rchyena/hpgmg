//------------------------------------------------------------------------------------------------------------------------------
// Samuel Williams
// SWWilliams@lbl.gov
// Lawrence Berkeley National Lab
//------------------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
//------------------------------------------------------------------------------------------------------------------------------
#include "defines.h"
#include "level.h"
#include "operators.h"
//------------------------------------------------------------------------------------------------------------------------------
#ifdef __USE_BICGSTAB
#include "solvers/bicgstab.c"
#elif  __USE_CG
#include "solvers/cg.c"
#elif  __USE_CABICGSTAB
#include "solvers/cabicgstab.c"
#elif  __USE_CACG
#include "solvers/cacg.c"
#endif
//------------------------------------------------------------------------------------------------------------------------------
#define __MODIFY_FOR_PERIODIC
//------------------------------------------------------------------------------------------------------------------------------
void IterativeSolver(level_type * level, int u_id, int f_id, double a, double b, double desired_reduction_in_norm){ 
  if(!level->active)return;
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
  double meanF;
  if(level->alpha_is_zero==-1)level->alpha_is_zero = (dot(level,__alpha,__alpha) == 0.0);  // haven't determined if alpha[] == 0
  #ifdef __MODIFY_FOR_PERIODIC
  if(level->domain_boundary_condition == __BC_PERIODIC){ // RHS should sum to zero !!!
    meanF = mean(level,f_id);
    if( (meanF!=0.0) && ((a==0.0) || (level->alpha_is_zero==1)) ){
      // Poisson with Periodic Boundary Conditions, but the RHS didn't sum to zero
      //if(level->my_rank==0)printf("coarse grid Poisson solve with periodic BC's and a RHS that doesn't sum to zero!!!\n");
      shift_grid(level,f_id,f_id,-meanF); // FIX  !!!
    }
    //if( (meanF!=0.0) && (a!=0.0) && (level->alpha_is_zero==0) ){  // FIX !!! change from alpha_is_zero to no element of alpha is zero
    //  // Helmholtz with Periodic Boundary Conditions, but the RHS didn't sum to zero
    //  // let u' = u - (meanF/a)(1/alpha)
    //  // solve a alpha u' - b div beta grad u' = f' = f - meanF + b div beta grad (meanF/a)(1/alpha)
    //  invert_grid(level,__temp,meanF/a,__alpha);  // FIX !!!  no element of alpha must ever be zero !!!
    //  residual(level,f_id,__temp,f_id,0.0,b); // f' = f - (0*__temp - b div beta grad __temp) = f + b div beta grad __temp
    //  shift_grid(level,f_id,f_id,-meanF);
    //}
  }
  #endif
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
  #ifdef __USE_BICGSTAB
    BiCGStab(level,u_id,f_id,a,b,desired_reduction_in_norm);
  #elif  __USE_CG
    CG(level,u_id,f_id,a,b,desired_reduction_in_norm);
  #elif  __USE_CABICGSTAB
    CABiCGStab(level,u_id,f_id,a,b,desired_reduction_in_norm);
  #elif  __USE_CACG
    CACG(level,u_id,f_id,a,b,desired_reduction_in_norm);
  #else // just point relaxation via multiple smooth()'s
    #if 1 
                     residual(level,__temp,u_id,f_id,a,b);
                    mul_grids(level,__temp,1.0,__temp,__Dinv); //  Using ||D^{-1}(b-Ax)||_{inf} as convergence criteria...
     double norm_of_r0 = norm(level,__temp);
    int s=0,maxSmoothsBottom=10,converged=0;
    while( (s<maxSmoothsBottom) && !converged){
      s++;
      level->Krylov_iterations++;
                       smooth(level,u_id,f_id,a,b);
                     residual(level,__temp,u_id,f_id,a,b);
                    mul_grids(level,__temp,1.0,__temp,__Dinv); //  Using ||D^{-1}(b-Ax)||_{inf} as convergence criteria...
      double norm_of_r = norm(level,__temp);
      if(norm_of_r == 0.0){converged=1;break;}
      if(norm_of_r < desired_reduction_in_norm*norm_of_r0){converged=1;break;}
    }
    #else
    int s=0;int maxSmoothsBottom=10;
    while( (s<maxSmoothsBottom) ){
      smooth(level,u_id,f_id,a,b);
      s++;
    }
    #endif
  #endif
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
  #ifdef __MODIFY_FOR_PERIODIC
  if(level->domain_boundary_condition == __BC_PERIODIC){ 
    if( (meanF!=0.0) && ((a==0.0) || (level->alpha_is_zero==1)) ){
      // Poisson with Periodic Boundary Conditions, but the RHS didn't sum to zero
      // by convention, we shift the correction to sum to zero (eliminate any constants)
      double average_value_of_e = mean(level,u_id);shift_grid(level,u_id,u_id,-average_value_of_e);
    }
    //if( (meanF!=0.0) && (a!=0.0) && (level->alpha_is_zero==0) ){
    //  // Helmholtz with Periodic Boundary Conditions, but the RHS didn't sum to zero...
    //  // u = u' + (meanF/a)(1/alpha)
    //  invert_grid(level,__temp,meanF/a,__alpha);  // FIX !!!  no element of alpha must ever be zero !!!
    //  add_grids(level,u_id,1.0,u_id,1.0,__temp);
    //}
  } 
  #endif
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
}


//------------------------------------------------------------------------------------------------------------------------------
int IterativeSolver_NumComponents(){
  // additionally number of grids required by an iterative solver...
  #ifdef __USE_BICGSTAB
  return(6);                  // BiCGStab requires additional grids r0,r,p,s,Ap,As
  #elif  __USE_CG
  return(4);                  // CG requires extra grids r0,r,p,Ap
  #elif  __USE_CABICGSTAB
  return(4+4*__CA_KRYLOV_S); // CABiCGStab requires additional grids rt,p,r,P[2s+1],R[2s].
  #elif  __USE_CACG
  return(4+2*__CA_KRYLOV_S); // CACG requires additional grids r0,p,r,P[s+1],R[s].
  #endif
  return(0);                  // simply doing multiple smooths requires no extra grids
}
//------------------------------------------------------------------------------------------------------------------------------