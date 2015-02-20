#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <climits>
#include <iostream>
#include "common/Utils.hpp"
#include "common/Lerp.hpp"
#include "common/DataInfo.pb.h"
#include "common/MeshGraphRegular3D.h"
#include "GLGPU3DDataset.h"

GLGPU3DDataset::GLGPU3DDataset()
{
  Reset();
}

GLGPU3DDataset::~GLGPU3DDataset()
{
  if (_re) free(_re);
  if (_im) free(_im);
  if (_Jx) free(_Jx);
}

void GLGPU3DDataset::Reset()
{
  for (int i=0; i<3; i++) {
    _dims[i] = 1; 
    _pbc[i] = false;
  }
  // TODO
}

void GLGPU3DDataset::BuildMeshGraph()
{
  _mg = new class MeshGraphRegular3D(_dims, _pbc);
}

void GLGPU3DDataset::ElemId2Idx(ElemIdType id, int *idx) const
{
  int s = dims()[0] * dims()[1]; 
  int k = id / s; 
  int j = (id - k*s) / dims()[0]; 
  int i = id - k*s - j*dims()[0]; 

  idx[0] = i; idx[1] = j; idx[2] = k;
}

ElemIdType GLGPU3DDataset::Idx2ElemId(int *idx) const
{
  for (int i=0; i<3; i++) 
    if (idx[i]<0 || idx[i]>=dims()[i])
      return UINT_MAX;
  
  return idx[0] + dims()[0] * (idx[1] + dims()[1] * idx[2]); 
}

void GLGPU3DDataset::Idx2Pos(const int idx[], double pos[]) const
{
  for (int i=0; i<3; i++) 
    pos[i] = idx[i] * CellLengths()[i] + Origins()[i];
}

void GLGPU3DDataset::Pos2Idx(const double pos[], int idx[]) const
{
  for (int i=0; i<3; i++)
    idx[i] = (pos[i] - Origins()[i]) / CellLengths()[i]; 
  // TODO: perodic boundary conditions
}

void GLGPU3DDataset::Pos2Grid(const double pos[], double gpos[]) const
{
  for (int i=0; i<3; i++)
    gpos[i] = (pos[i] - Origins()[i]) / CellLengths()[i]; 
}

#if 0
double GLGPU3DDataset::Flux(int face) const
{
  // TODO: pre-compute the flux
  switch (face) {
  case 0: return -dx() * dy() * Bz();
  case 1: return -dy() * dz() * Bx(); 
  case 2: return -dz() * dx() * By(); 
  case 3: return  dx() * dy() * Bz(); 
  case 4: return  dy() * dz() * Bx(); 
  case 5: return  dz() * dx() * By();
  default: assert(false);
  }
  return 0.0;
}
#endif

double GLGPU3DDataset::GaugeTransformation(const double X0[], const double X1[]) const
{
  double gx, gy, gz; 
  double dx = X1[0] - X0[0], 
         dy = X1[1] - X0[1], 
         dz = X1[2] - X0[2]; 
  double x = X0[0] + 0.5*dx, 
         y = X0[1] + 0.5*dy, 
         z = X0[2] + 0.5*dz;

  if (By()>0) { // Y-Z gauge
    gx = dx * Kex(); 
    gy =-dy * x * Bz(); // -dy*x^hat*Bz
    gz = dz * x * By(); //  dz*x^hat*By
  } else { // X-Z gauge
    gx = dx * y * Bz() + dx * Kex(); //  dx*y^hat*Bz + dx*K
    gy = 0; 
    gz =-dz * y * Bx(); // -dz*y^hat*Bx
  }

  return gx + gy + gz; 
}

#if 0
std::vector<ElemIdType> GLGPU3DDataset::GetNeighborIds(ElemIdType elem_id) const
{
  std::vector<ElemIdType> neighbors; 

  int idx[3], idx1[3];
  ElemId2Idx(elem_id, idx); 

  for (int face=0; face<6; face++) {
    switch (face) {
    case 0: idx1[0] = idx[0];   idx1[1] = idx[1];   idx1[2] = idx[2]-1; break; 
    case 1: idx1[0] = idx[0]-1; idx1[1] = idx[1];   idx1[2] = idx[2];   break;
    case 2: idx1[0] = idx[0];   idx1[1] = idx[1]-1; idx1[2] = idx[2];   break;
    case 3: idx1[0] = idx[0];   idx1[1] = idx[1];   idx1[2] = idx[2]+1; break; 
    case 4: idx1[0] = idx[0]+1; idx1[1] = idx[1];   idx1[2] = idx[2];   break;
    case 5: idx1[0] = idx[0];   idx1[1] = idx[1]+1; idx1[2] = idx[2];   break;
    default: break;
    }

#if 0 // pbc
    for (int i=0; i<3; i++) 
      if (pbc()[i]) {
        idx1[i] = idx1[i] % dims()[i]; 
        if (idx1[i]<0) idx1[i] += dims()[i];
      }
#endif
    
    neighbors.push_back(Idx2ElemId(idx1)); 
  }

  return neighbors; 
}
#endif
  
void GLGPU3DDataset::SerializeDataInfoToString(std::string& buf) const
{
  PBDataInfo pb;

  pb.set_model(PBDataInfo::GLGPU);
  pb.set_name(_data_name);

  if (Lengths()[0]>0) {
    pb.set_ox(Origins()[0]); 
    pb.set_oy(Origins()[1]); 
    pb.set_oz(Origins()[2]); 
    pb.set_lx(Lengths()[0]); 
    pb.set_ly(Lengths()[1]); 
    pb.set_lz(Lengths()[2]); 
  }

  pb.set_bx(Bx());
  pb.set_by(By());
  pb.set_bz(Bz());

  pb.set_kex(Kex());

  pb.set_dx(dims()[0]);
  pb.set_dy(dims()[1]);
  pb.set_dz(dims()[2]);

  pb.set_pbc_x(pbc()[0]);
  pb.set_pbc_y(pbc()[0]);
  pb.set_pbc_z(pbc()[0]);

  pb.SerializeToString(&buf);
}

void GLGPU3DDataset::ComputeSupercurrentField()
{
  const int nvoxels = dims()[0]*dims()[1]*dims()[2];

  if (_Jx != NULL) free(_Jx);
  _Jx = (double*)malloc(3*sizeof(double)*nvoxels);
  _Jy = _Jx + nvoxels; 
  _Jz = _Jy + nvoxels;
  memset(_Jx, 0, 3*sizeof(double)*nvoxels);
 
  double u, v, rho2;
  double du[3], dv[3], dphi[3], J[3];

  // central difference
  for (int x=1; x<dims()[0]-1; x++) {
    for (int y=1; y<dims()[1]-1; y++) {
      for (int z=1; z<dims()[2]-1; z++) {
        int idx[3] = {x, y, z}; 
        double pos[3]; 
        Idx2Pos(idx, pos);

#if 1 // gradient estimation by \grad\psi or \grad\theta
        du[0] = 0.5 * (Re(x+1, y, z) - Re(x-1, y, z)) / dx();
        du[1] = 0.5 * (Re(x, y+1, z) - Re(x, y-1, z)) / dy();
        du[2] = 0.5 * (Re(x, y, z+1) - Re(x, y, z-1)) / dz();
        
        dv[0] = 0.5 * (Im(x+1, y, z) - Im(x-1, y, z)) / dx();
        dv[1] = 0.5 * (Im(x, y+1, z) - Im(x, y-1, z)) / dy();
        dv[2] = 0.5 * (Im(x, y, z+1) - Im(x, y, z-1)) / dz();

        u = Re(x, y, z); 
        v = Im(x, y, z);
        rho2 = u*u + v*v;

        J[0] = (u*dv[0] - v*du[0]) / rho2 - Ax(pos); // + Kex();
        J[1] = (u*dv[1] - v*du[1]) / rho2 - Ay(pos);
        J[2] = (u*dv[2] - v*du[2]) / rho2 - Az(pos);
// #else
        dphi[0] = 0.5 * (mod2pi(Phi(x+1, y, z) - Phi(x-1, y, z) + M_PI) - M_PI) / dx();
        dphi[1] = 0.5 * (mod2pi(Phi(x, y+1, z) - Phi(x, y-1, z) + M_PI) - M_PI) / dy();
        dphi[2] = 0.5 * (mod2pi(Phi(x, y, z+1) - Phi(x, y, z-1) + M_PI) - M_PI) / dz();

        fprintf(stderr, "J={%f, %f, %f}, ", J[0], J[1], J[2]);

        J[0] = dphi[0] - Ax(pos);
        J[1] = dphi[1] - Ay(pos);
        J[2] = dphi[2] - Az(pos);
        
        fprintf(stderr, "J'={%f, %f, %f}\n", J[0], J[1], J[2]);
#endif

        texel3D(_Jx, dims(), x, y, z) = J[0]; 
        texel3D(_Jy, dims(), x, y, z) = J[1];
        texel3D(_Jz, dims(), x, y, z) = J[2];
      }
    }
  }
}

bool GLGPU3DDataset::Psi(const double X[3], double &re, double &im) const
{
  // TODO
  return false;
}

bool GLGPU3DDataset::Supercurrent(const double X[3], double J[3]) const
{
  static const int st[3] = {0};
  double gpt[3];
  const double *j[3] = {_Jx, _Jy, _Jz};
  
  Pos2Grid(X, gpt);
  if (isnan(gpt[0]) || gpt[0]<=1 || gpt[0]>dims()[0]-2 || 
      isnan(gpt[1]) || gpt[1]<=1 || gpt[1]>dims()[1]-2 || 
      isnan(gpt[2]) || gpt[2]<=1 || gpt[2]>dims()[2]-2) return false;

  if (!lerp3D(gpt, st, dims(), 3, j, J))
    return false;
  else return true;
}
 
#if 0
bool GLGPU3DDataset::OnBoundary(ElemIdType id) const
{
  int idx[3];
  ElemId2Idx(id, idx);

  for (int i=0; i<3; i++) 
    if (idx[i] == 0 || idx[i] == dims()[i]-1) return true;

  return false;
}
#endif

double GLGPU3DDataset::QP(const double X0[], const double X1[]) const 
{
  const double *L = Lengths();
  double d[3] = {X1[0] - X0[0], X1[1] - X0[1], X1[2] - X0[2]};
  int p[3] = {0}; // 0: not crossed; 1: positive; -1: negative

  for (int i=0; i<3; i++) {
    d[i] = X1[i] - X0[i];
    if (d[i]>L[i]/2) {d[i] -= L[i]; p[i] = 1;}
    else if (d[i]<-L[i]/2) {d[i] += L[i]; p[i] = -1;}
  }

  if (pbc()[0] && p[0]!=0) { // By>0
    return p[0] * (L[0]*Bz()*X0[1] - L[2]*By()*X0[2]); // FIXME: the cross point
  } else if (pbc()[1] && p[1]!=0) {
    return p[1] * (-L[2]*Bz()*X0[0] + L[2]*Bx()*X0[2]);
  } else return 0.0;
}

#if 0
bool GLGPU3DDataset::GetFace(ElemIdType id, int face, double X[][3], double A[][3], double re[], double im[]) const
{
  int idx0[3]; 
  ElemId2Idx(id, idx0);
  int idx1[3] = {(idx0[0]+1)%dims()[0], (idx0[1]+1)%dims()[1], (idx0[2]+1)%dims()[2]}; 
  
  int V[4][3];

  switch (face) {
  case 0: // XY0
    V[0][0] = idx0[0]; V[0][1] = idx0[1]; V[0][2] = idx0[2]; 
    V[1][0] = idx0[0]; V[1][1] = idx1[1]; V[1][2] = idx0[2]; 
    V[2][0] = idx1[0]; V[2][1] = idx1[1]; V[2][2] = idx0[2]; 
    V[3][0] = idx1[0]; V[3][1] = idx0[1]; V[3][2] = idx0[2]; 
    break; 
  
  case 1: // YZ0
    V[0][0] = idx0[0]; V[0][1] = idx0[1]; V[0][2] = idx0[2]; 
    V[1][0] = idx0[0]; V[1][1] = idx0[1]; V[1][2] = idx1[2]; 
    V[2][0] = idx0[0]; V[2][1] = idx1[1]; V[2][2] = idx1[2]; 
    V[3][0] = idx0[0]; V[3][1] = idx1[1]; V[3][2] = idx0[2]; 
    break; 
  
  case 2: // ZX0
    V[0][0] = idx0[0]; V[0][1] = idx0[1]; V[0][2] = idx0[2]; 
    V[1][0] = idx1[0]; V[1][1] = idx0[1]; V[1][2] = idx0[2]; 
    V[2][0] = idx1[0]; V[2][1] = idx0[1]; V[2][2] = idx1[2]; 
    V[3][0] = idx0[0]; V[3][1] = idx0[1]; V[3][2] = idx1[2]; 
    break; 
  
  case 3: // XY1
    V[0][0] = idx0[0]; V[0][1] = idx0[1]; V[0][2] = idx1[2]; 
    V[1][0] = idx1[0]; V[1][1] = idx0[1]; V[1][2] = idx1[2]; 
    V[2][0] = idx1[0]; V[2][1] = idx1[1]; V[2][2] = idx1[2]; 
    V[3][0] = idx0[0]; V[3][1] = idx1[1]; V[3][2] = idx1[2]; 
    break; 

  case 4: // YZ1
    V[0][0] = idx1[0]; V[0][1] = idx0[1]; V[0][2] = idx0[2]; 
    V[1][0] = idx1[0]; V[1][1] = idx1[1]; V[1][2] = idx0[2]; 
    V[2][0] = idx1[0]; V[2][1] = idx1[1]; V[2][2] = idx1[2]; 
    V[3][0] = idx1[0]; V[3][1] = idx0[1]; V[3][2] = idx1[2]; 
    break; 

  case 5: // ZX1
    V[0][0] = idx0[0]; V[0][1] = idx1[1]; V[0][2] = idx0[2]; 
    V[1][0] = idx0[0]; V[1][1] = idx1[1]; V[1][2] = idx1[2]; 
    V[2][0] = idx1[0]; V[2][1] = idx1[1]; V[2][2] = idx1[2]; 
    V[3][0] = idx1[0]; V[3][1] = idx1[1]; V[3][2] = idx0[2]; 
    break; 

  default: return false; 
  }

  for (int i=0; i<4; i++) {
    Idx2Pos(V[i], X[i]); 
    re[i] = Re(V[i][0], V[i][1], V[i][2]);
    im[i] = Im(V[i][0], V[i][1], V[i][2]);
    GLGPU3DDataset::A(X[i], A[i]);
  }

  return true;
}

bool GLGPU3DDataset::GetSpaceTimeEdgeValues(const Edge* e, double X[][3], double A[][3], double re[], double im[]) const
{
  // TODO
  return false;
}
#endif
