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
#ifdef _OPENMP
#include <omp.h>
#endif
//------------------------------------------------------------------------------------------------------------------------------
#include "timers.h"
#include "defines.h"
#include "level.h"
#include "operators.h"
//------------------------------------------------------------------------------------------------------------------------------
#define STENCIL_VARIABLE_COEFFICIENT
//------------------------------------------------------------------------------------------------------------------------------
#define MyPragma(a) _Pragma(#a)
//------------------------------------------------------------------------------------------------------------------------------
#if (_OPENMP>=201107) // OpenMP 3.1 supports max reductions...
  // XL C/C++ 12.01.0000.0009 sets _OPENMP to 201107, but does not support the max clause within a _Pragma().  
  // This issue was fixed by XL C/C++ 12.01.0000.0011
  // If you do not have this version of XL C/C++ and run into this bug, uncomment these macros...
  //#warning not threading norm() calculations due to issue with XL/C, _Pragma, and reduction(max:bmax)
  //#define PRAGMA_THREAD_ACROSS_BLOCKS(    level,b,nb     )    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1)                     )
  //#define PRAGMA_THREAD_ACROSS_BLOCKS_SUM(level,b,nb,bsum)    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1) reduction(  +:bsum) )
  //#define PRAGMA_THREAD_ACROSS_BLOCKS_MAX(level,b,nb,bmax)    
  #define PRAGMA_THREAD_ACROSS_BLOCKS(    level,b,nb     )    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1)                     )
  #define PRAGMA_THREAD_ACROSS_BLOCKS_SUM(level,b,nb,bsum)    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1) reduction(  +:bsum) )
  #define PRAGMA_THREAD_ACROSS_BLOCKS_MAX(level,b,nb,bmax)    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1) reduction(max:bmax) )
#elif _OPENMP // older OpenMP versions don't support the max reduction clause
  #warning Threading max reductions requires OpenMP 3.1 (July 2011).  Please upgrade your compiler.                                                           
  #define PRAGMA_THREAD_ACROSS_BLOCKS(    level,b,nb     )    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1)                     )
  #define PRAGMA_THREAD_ACROSS_BLOCKS_SUM(level,b,nb,bsum)    MyPragma(omp parallel for private(b) if(nb>1) schedule(static,1) reduction(  +:bsum) )
  #define PRAGMA_THREAD_ACROSS_BLOCKS_MAX(level,b,nb,bmax)    
#else // flat MPI should not define any threading...
  #define PRAGMA_THREAD_ACROSS_BLOCKS(    level,b,nb     )    
  #define PRAGMA_THREAD_ACROSS_BLOCKS_SUM(level,b,nb,bsum)    
  #define PRAGMA_THREAD_ACROSS_BLOCKS_MAX(level,b,nb,bmax)    
#endif
//------------------------------------------------------------------------------------------------------------------------------
#ifdef STENCIL_FUSE_BC
  #error This implementation does not support fusion of the boundary conditions with the operator
#endif
//------------------------------------------------------------------------------------------------------------------------------
void apply_BCs(level_type * level, int x_id, int shape){apply_BCs_v4(level,x_id,shape);}
//------------------------------------------------------------------------------------------------------------------------------
#define STENCIL_TWELFTH ( 0.0833333333333333333)  // 1.0/12.0;
//------------------------------------------------------------------------------------------------------------------------------
#ifdef STENCIL_VARIABLE_COEFFICIENT

//fluxes at ijk-0.5e^d (low faces of cell ijk)...
#define beta_dxdi(x,ijk)                                                                           \
(                                                                                                  \
  h2inv*STENCIL_TWELFTH*(                                                                          \
            beta_i[ijk]*( 15.0*(x[ijk]-x[ijk-1]) - x[ijk+1] + x[ijk-2] )                           \
    + 0.25*(beta_i[ijk+jStride]-beta_i[ijk-jStride]) * (+x[ijk  +jStride]                          \
                                                        -x[ijk-1+jStride]                          \
                                                        -x[ijk  -jStride]                          \
                                                        +x[ijk-1-jStride])                         \
    + 0.25*(beta_i[ijk+kStride]-beta_i[ijk-kStride]) * (+x[ijk  +kStride]                          \
                                                        -x[ijk-1+kStride]                          \
                                                        -x[ijk  -kStride]                          \
                                                        +x[ijk-1-kStride])                         \
  )                                                                                                \
)

#define beta_dxdj(x,ijk)                                                                           \
(                                                                                                  \
  h2inv*STENCIL_TWELFTH*(                                                                          \
            beta_j[ijk]*( 15.0*(x[ijk]-x[ijk-jStride]) - x[ijk+jStride] + x[ijk-jStride-jStride] ) \
    + 0.25*(beta_j[ijk+1      ]-beta_j[ijk-1      ]) * (+x[ijk        +1      ]                    \
                                                        -x[ijk-jStride+1      ]                    \
                                                        -x[ijk        -1      ]                    \
                                                        +x[ijk-jStride-1      ])                   \
    + 0.25*(beta_j[ijk+kStride]-beta_j[ijk-kStride]) * (+x[ijk        +kStride]                    \
                                                        -x[ijk-jStride+kStride]                    \
                                                        -x[ijk        -kStride]                    \
                                                        +x[ijk-jStride-kStride])                   \
  )                                                                                                \
)

#define beta_dxdk(x,ijk)                                                                           \
(                                                                                                  \
  h2inv*STENCIL_TWELFTH*(                                                                          \
            beta_k[ijk]*( 15.0*(x[ijk]-x[ijk-kStride]) - x[ijk+kStride] + x[ijk-kStride-kStride] ) \
    + 0.25*(beta_k[ijk+1      ]-beta_k[ijk-1      ]) * (+x[ijk        +1      ]                    \
                                                        -x[ijk-kStride+1      ]                    \
                                                        -x[ijk        -1      ]                    \
                                                        +x[ijk-kStride-1      ])                   \
    + 0.25*(beta_k[ijk+jStride]-beta_k[ijk-jStride]) * (+x[ijk        +jStride]                    \
                                                        -x[ijk-kStride+jStride]                    \
                                                        -x[ijk        -jStride]                    \
                                                        +x[ijk-kStride-jStride])                   \
  )                                                                                                \
)


#define Laplacian_ijk(x)                         \
(                                                \
  - beta_dxdi(x,ijk) + beta_dxdi(x,ijk+1      )  \
  - beta_dxdj(x,ijk) + beta_dxdj(x,ijk+jStride)  \
  - beta_dxdk(x,ijk) + beta_dxdk(x,ijk+kStride)  \
)

#ifdef USE_HELMHOLTZ
#define apply_op_ijk(x)  ( a*alpha[ijk]*x[ijk] - b*(Laplacian_ijk(x)) )
#else
#define apply_op_ijk(x)  (                     - b*(Laplacian_ijk(x)) )
#endif

#else
//------------------------------------------------------------------------------------------------------------------------------
#error DEFINE CONSTANT COEFFICIENT CASE!!!
#define Laplacian_ijk(x)        \
(                               \
  h2inv*STENCIL_TWELFTH*(       \
     - 1.0*(x[ijk-2*kStride] +  \
            x[ijk-2*jStride] +  \
            x[ijk-2        ] +  \
            x[ijk+2        ] +  \
            x[ijk+2*jStride] +  \
            x[ijk+2*kStride] )  \
     +16.0*(x[ijk  -kStride] +  \
            x[ijk  -jStride] +  \
            x[ijk  -1      ] +  \
            x[ijk  +1      ] +  \
            x[ijk  +jStride] +  \
            x[ijk  +kStride] )  \
     -90.0*(x[ijk          ] )  \
  )                             \
)
#define apply_op_ijk(x)  (            a*x[ijk] - b*(Laplacian_ijk(x)) )
#endif
//------------------------------------------------------------------------------------------------------------------------------
#ifdef STENCIL_VARIABLE_COEFFICIENT
int stencil_get_radius(){return(2);} // stencil reaches out 2 cells
int stencil_get_shape(){return(STENCIL_SHAPE_NO_CORNERS);} // needs faces and edges, but not corners
#else
int stencil_get_radius(){return(2);} // stencil reaches out 2 cells
int stencil_get_shape(){return(STENCIL_SHAPE_STAR);} // needs just faces
#endif
//------------------------------------------------------------------------------------------------------------------------------
void rebuild_operator(level_type * level, level_type *fromLevel, double a, double b){
  // form restriction of alpha[], beta_*[] coefficients from fromLevel
  if(fromLevel != NULL){
    restriction(level,VECTOR_ALPHA ,fromLevel,VECTOR_ALPHA ,RESTRICT_CELL  );
    restriction(level,VECTOR_BETA_I,fromLevel,VECTOR_BETA_I,RESTRICT_FACE_I);
    restriction(level,VECTOR_BETA_J,fromLevel,VECTOR_BETA_J,RESTRICT_FACE_J);
    restriction(level,VECTOR_BETA_K,fromLevel,VECTOR_BETA_K,RESTRICT_FACE_K);
  } // else case assumes alpha/beta have been set

  // extrapolate the beta's into the ghost zones (needed for mixed derivatives)
  extrapolate_betas(level);
  //initialize_problem(level,level->h,a,b); // approach used for testing smooth beta's; destroys the black box nature of the solver

  // exchange alpha/beta/...  (must be done before calculating Dinv)
  exchange_boundary(level,VECTOR_ALPHA ,STENCIL_SHAPE_BOX); // safe
  exchange_boundary(level,VECTOR_BETA_I,STENCIL_SHAPE_BOX);
  exchange_boundary(level,VECTOR_BETA_J,STENCIL_SHAPE_BOX);
  exchange_boundary(level,VECTOR_BETA_K,STENCIL_SHAPE_BOX);

  // black box rebuild of D^{-1}, l1^{-1}, dominant eigenvalue, ...
  rebuild_operator_blackbox(level,a,b,4);

  // exchange Dinv...
  exchange_boundary(level,VECTOR_DINV ,STENCIL_SHAPE_BOX); // safe
}


//------------------------------------------------------------------------------------------------------------------------------
#ifdef  USE_GSRB
#define GSRB_OOP
#define NUM_SMOOTHS      3 // RBRBRB
#include "operators.test/gsrb.flux.c"
#elif   USE_CHEBY
#warning The Chebyshev smoother is currently underperforming for 4th order.  Please use -DUSE_GSRB or -DUSE_JACOBI
#define NUM_SMOOTHS      1
#define CHEBYSHEV_DEGREE 6 // i.e. one degree-6 polynomial smoother
#include "operators/chebyshev.c"
#elif   USE_JACOBI
#define NUM_SMOOTHS      6
#include "operators/jacobi.c"
#else
#error You must compile with either -DUSE_GSRB, -DUSE_CHEBY, or -DUSE_JACOBI
#endif
#include "operators.test/residual.flux.c"
#include "operators/apply_op.c"
#include "operators/rebuild.c"
//------------------------------------------------------------------------------------------------------------------------------
#include "operators/blockCopy.c"
#include "operators/misc.c"
#include "operators/exchange_boundary.c"
#include "operators/boundary_fv.c"
#include "operators/restriction.c"
#include "operators/interpolation_v2.c"
#include "operators/interpolation_v4.c"
//------------------------------------------------------------------------------------------------------------------------------
void interpolation_vcycle(level_type * level_f, int id_f, double prescale_f, level_type *level_c, int id_c){interpolation_v2(level_f,id_f,prescale_f,level_c,id_c);}
void interpolation_fcycle(level_type * level_f, int id_f, double prescale_f, level_type *level_c, int id_c){interpolation_v4(level_f,id_f,prescale_f,level_c,id_c);}
//------------------------------------------------------------------------------------------------------------------------------
#include "operators/problem.fv.c"
//------------------------------------------------------------------------------------------------------------------------------
