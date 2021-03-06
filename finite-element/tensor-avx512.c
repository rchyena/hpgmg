#include "tensorimpl.h"

#ifdef __AVX512F__

#include <immintrin.h>

static inline PetscErrorCode TensorContract_AVX512(PetscInt ne,PetscInt dof,PetscInt P,PetscInt Q,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[])
{

  PetscFunctionBegin;
  if (tmode == TENSOR_TRANSPOSE) {PetscInt tmp = Q; Q = P; P = tmp;}
  {
    PetscReal R[Q][P],S[Q][P],T[Q][P];
    const PetscScalar (*x)[P*P*P][ne] = (const PetscScalar(*)[P*P*P][ne])xx;
    PetscScalar       (*y)[P*P*P][ne] =       (PetscScalar(*)[Q*Q*Q][ne])yy;
    PetscScalar u[dof][Q*P*P][ne]_align,v[dof][Q*Q*P][ne]_align;

    for (PetscInt i=0; i<Q; i++) {
      for (PetscInt j=0; j<P; j++) {
        R[i][j] = tmode == TENSOR_EVAL ? Rf[i*P+j] : Rf[j*Q+i];
        S[i][j] = tmode == TENSOR_EVAL ? Sf[i*P+j] : Sf[j*Q+i];
        T[i][j] = tmode == TENSOR_EVAL ? Tf[i*P+j] : Tf[j*Q+i];
      }
    }

    // u[l,a,j,k] = R[a,i] x[l,i,j,k]
    for (PetscInt l=0; l<dof; l++) {
      for (PetscInt a=0; a<Q; a++) {
        __m512d r[P];
        for (PetscInt i=0; i<P; i++) r[i] = _mm512_set1_pd(R[a][i]);
        for (PetscInt jk=0; jk<P*P; jk++) {
          for (PetscInt e=0; e<ne; e+=8) {
            __m512d u_lajk = _mm512_setzero_pd();
            for (PetscInt i=0; i<P; i++) {
              u_lajk = _mm512_fmadd_pd(r[i],_mm512_load_pd(&x[l][i*P*P+jk][e]),u_lajk);
            }
            _mm512_store_pd(&u[l][a*P*P+jk][e],u_lajk);
          }
        }
      }
    }

    // v[l,a,b,k] = S[b,j] u[l,a,j,k]
    for (PetscInt l=0; l<dof; l++) {
      for (PetscInt b=0; b<Q; b++) {
        __m512d s[P];
        for (int j=0; j<P; j++) s[j] = _mm512_set1_pd(S[b][j]);
        for (PetscInt a=0; a<Q; a++) {
          for (PetscInt k=0; k<P; k++) {
            for (PetscInt e=0; e<ne; e+=8) {
              __m512d v_labk = _mm512_setzero_pd();
              for (PetscInt j=0; j<P; j++) {
                v_labk = _mm512_fmadd_pd(s[j],_mm512_load_pd(&u[l][(a*P+j)*P+k][e]),v_labk);
              }
              _mm512_store_pd(&v[l][(a*Q+b)*P+k][e],v_labk);
            }
          }
        }
      }
    }

    // y[l,a,b,c] = T[c,k] v[l,a,b,k]
    for (PetscInt l=0; l<dof; l++) {
      for (PetscInt c=0; c<Q; c++) {
        __m512d t[P];
        for (int k=0; k<P; k++) t[k] = _mm512_set1_pd(T[c][k]);
        for (PetscInt ab=0; ab<Q*Q; ab++) {
          for (PetscInt e=0; e<ne; e+=8) {
            __m512d y_labc = _mm512_load_pd(&y[l][ab*Q+c][e]);
            for (PetscInt k=0; k<P; k++) {
              y_labc = _mm512_fmadd_pd(t[k],_mm512_load_pd(&v[l][ab*P+k][e]),y_labc);
            }
            _mm512_store_pd(&y[l][ab*Q+c][e],y_labc);
          }
        }
      }
    }
    PetscLogFlops(dof*(Q*P*P*P+Q*Q*P*P+Q*Q*Q*P)*ne*2);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode TensorContract_AVX512_8_1_2_2(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(8,1,2,2,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_8_3_2_2(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(8,3,2,2,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_8_1_3_3(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(8,1,3,3,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_8_3_3_3(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(8,3,3,3,Rf,Sf,Tf,tmode,xx,yy);
}

static PetscErrorCode TensorContract_AVX512_16_1_2_2(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(16,1,2,2,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_16_3_2_2(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(16,3,2,2,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_16_1_3_3(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(16,1,3,3,Rf,Sf,Tf,tmode,xx,yy);
}
static PetscErrorCode TensorContract_AVX512_16_3_3_3(Tensor ten,const PetscReal Rf[],const PetscReal Sf[],const PetscReal Tf[],TensorMode tmode,const PetscScalar xx[],PetscScalar yy[]) {
  return TensorContract_AVX512(16,3,3,3,Rf,Sf,Tf,tmode,xx,yy);
}

#endif

// Choose our optimized functions if available
PetscErrorCode TensorSelect_AVX512(Tensor ten) {

  PetscFunctionBegin;
#ifdef __AVX512F__
  switch (ten->ne) {
  case 8: {
    PetscInt P = ten->P,Q = ten->Q;
    switch (ten->dof) {
    case 1: // Scalar problems with Q1 or Q2 elements
      if (P == 2 && Q == 2)      ten->Contract = TensorContract_AVX512_8_1_2_2;
      else if (P == 3 && Q == 3) ten->Contract = TensorContract_AVX512_8_1_3_3;
      break;
    case 3: // Coordinates or elasticity
      if (P == 2 && Q == 2)      ten->Contract = TensorContract_AVX512_8_3_2_2;
      else if (P == 3 && Q == 3) ten->Contract = TensorContract_AVX512_8_3_3_3;
      break;
    }
  } break;
  case 16: {
    PetscInt P = ten->P,Q = ten->Q;
    switch (ten->dof) {
    case 1: // Scalar problems with Q1 or Q2 elements
      if (P == 2 && Q == 2)      ten->Contract = TensorContract_AVX512_16_1_2_2;
      else if (P == 3 && Q == 3) ten->Contract = TensorContract_AVX512_16_1_3_3;
      break;
    case 3: // Coordinates or elasticity
      if (P == 2 && Q == 2)      ten->Contract = TensorContract_AVX512_16_3_2_2;
      else if (P == 3 && Q == 3) ten->Contract = TensorContract_AVX512_16_3_3_3;
      break;
    }
  } break;
  }
#endif
  PetscFunctionReturn(0);
}
