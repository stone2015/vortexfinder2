#include "VortexTransitionMatrix.h"
#include <sstream>
#include <cstdio>
#include <climits>
#include <cassert>

#if WITH_PROTOBUF
#include "Association.pb.h"
#endif

VortexTransitionMatrix::VortexTransitionMatrix() :
  _n0(INT_MAX), _n1(INT_MAX)
{
}

VortexTransitionMatrix::VortexTransitionMatrix(int t0, int t1, int n0, int n1) :
  _interval(std::make_pair<int, int>(t0, t1)), 
  _n0(n0), _n1(n1)
{
  _match.resize(_n0*_n1);
}

VortexTransitionMatrix::~VortexTransitionMatrix()
{
}

bool VortexTransitionMatrix::Serialize(std::string& buf) const
{
#if WITH_PROTOBUF
  PBAssociation pb;
  pb.set_f0(_interval.first);
  pb.set_f1(_interval.second);
  pb.set_n0(_n0);
  pb.set_n1(_n1);
  for (int i=0; i<_match.size(); i++)
    pb.add_mat(_match[i]);
  pb.SerializeToString(&buf);
  return true;
#else
  assert(false);
  return false;
#endif
}

bool VortexTransitionMatrix::Unserialize(const std::string& buf)
{
#if WITH_PROTOBUF
  PBAssociation pb;
  if (!pb.ParseFromString(buf)) return false;
  _interval.first = pb.f0();
  _interval.second = pb.f1();
  _n0 = pb.n0();
  _n1 = pb.n1();
  _match.resize(_n0 * _n1);
  for (int i=0; i<_match.size(); i++) 
    _match[i] = pb.mat(i);
  
  if (Valid()) 
    Normalize();
  return true;
#else
  assert(false);
  return false;
#endif
}

bool VortexTransitionMatrix::LoadFromFile(const std::string& filename)
{
  FILE *fp = fopen(filename.c_str(), "rb");
  if (!fp) return false;

  fread(&_interval, sizeof(int), 2, fp);
  fread(&_n0, sizeof(int), 1, fp);
  fread(&_n1, sizeof(int), 1, fp);

  _match.resize(_n0*_n1);
  fread((char*)_match.data(), sizeof(int), _n0*_n1, fp);

  fclose(fp);

  if (Valid()) 
    Normalize();

  return Valid();
}

bool VortexTransitionMatrix::SaveToFile(const std::string& filename) const
{
  if (!Valid()) return false;

  FILE *fp = fopen(filename.c_str(), "wb");
  if (!fp) return false;

  fwrite(&_interval, sizeof(int), 2, fp);
  fwrite(&_n0, sizeof(int), 1, fp);
  fwrite(&_n1, sizeof(int), 1, fp);
  fwrite((char*)_match.data(), sizeof(int), _n0*_n1, fp);

  fclose(fp);
  return true;
}

bool VortexTransitionMatrix::LoadFromFile(const std::string& dataname, int t0, int t1)
{
  return LoadFromFile(MatrixFileName(dataname, t0, t1));
}

bool VortexTransitionMatrix::SaveToFile(const std::string& dataname, int t0, int t1) const 
{
  return SaveToFile(MatrixFileName(dataname, t0, t1));
}

std::string VortexTransitionMatrix::MatrixFileName(const std::string& dataname, int t0, int t1) const
{
  std::stringstream ss;
  ss << dataname << ".match." << t0 << "." << t1;
  return ss.str();
}

int VortexTransitionMatrix::operator()(int i, int j) const
{
  return _match[i*n1() + j];
}

int& VortexTransitionMatrix::operator()(int i, int j)
{
  return _match[i*n1() + j];
}

int VortexTransitionMatrix::at(int i, int j) const
{
  return _match[i*n1() + j];
}

int& VortexTransitionMatrix::at(int i, int j)
{
  return _match[i*n1() + j];
}

int VortexTransitionMatrix::colsum(int j) const
{
  int sum = 0;
  for (int i=0; i<n0(); i++)
    sum += _match[i*n1() + j];
  return sum;
}

int VortexTransitionMatrix::rowsum(int i) const 
{
  int sum = 0;
  for (int j=0; j<n1(); j++) 
    sum += _match[i*n1() + j];
  return sum;
}

void VortexTransitionMatrix::GetModule(int i, std::set<int> &lhs, std::set<int> &rhs, int &event) const
{
  if (i>=_lhss.size()) return;

  lhs = _lhss[i];
  rhs = _rhss[i];
  event = _events[i];
}

void VortexTransitionMatrix::Normalize()
{
  if (NModules() == 0) Modularize();
  for (int i=0; i<NModules(); i++) {
    const std::set<int> &lhs = _lhss[i];
    const std::set<int> &rhs = _rhss[i];
    for (std::set<int>::const_iterator it0=lhs.begin(); it0!=lhs.end(); it0++) 
      for (std::set<int>::const_iterator it1=rhs.begin(); it1!=rhs.end(); it1++) {
        int l = *it0, r = *it1;
        at(l, r) = 1;
      }
  }
}

void VortexTransitionMatrix::Modularize()
{
  const int n = n0() + n1();

  _lhss.clear();
  _rhss.clear();
  _events.clear();

  std::set<int> unvisited; 
  for (int i=0; i<n; i++) 
    unvisited.insert(i);

  while (!unvisited.empty()) {
    std::set<int> lhs, rhs; 
    std::vector<int> Q;
    Q.push_back(*unvisited.begin());

    while (!Q.empty()) {
      int v = Q.back();
      Q.pop_back();
      unvisited.erase(v);
      if (v<n0()) lhs.insert(v);
      else rhs.insert(v-n0());

      if (v<n0()) { // left hand side
        for (int j=0; j<n1(); j++) 
          if (at(v, j)>0 && unvisited.find(j+n0()) != unvisited.end())
            Q.push_back(j+n0());
      } else { // right hand side
        for (int i=0; i<n0(); i++) 
          if (at(i, v-n0())>0 && unvisited.find(i) != unvisited.end())
            Q.push_back(i);
      }
    }

    int event; 
    if (lhs.size() == 1 && rhs.size() == 1) {
      event = VORTEX_EVENT_DUMMY;
    } else if (lhs.size() == 0 && rhs.size() == 1) {
      event = VORTEX_EVENT_BIRTH;
    } else if (lhs.size() == 1 && rhs.size() == 0) {
      event = VORTEX_EVENT_DEATH;
    } else if (lhs.size() == 1 && rhs.size() == 2) {
      event = VORTEX_EVENT_SPLIT;
    } else if (lhs.size() == 2 && rhs.size() == 1) { 
      event = VORTEX_EVENT_MERGE;
    } else if (lhs.size() == 2 && rhs.size() == 2) { 
      event = VORTEX_EVENT_RECOMBINATION;
    } else {
      event = VORTEX_EVENT_COMPOUND;
    }

    _lhss.push_back(lhs);
    _rhss.push_back(rhs);
    _events.push_back(event);
  }
}

void VortexTransitionMatrix::Print() const
{
  // fprintf(stderr, "Interval={%d, %d}, n0=%d, n1=%d\n", 
  //     _interval.first, _interval.second, _n0, _n1);
  return;

  for (int i=0; i<_n0; i++) {
    for (int j=0; j<_n1; j++) {
      fprintf(stderr, "%d\t", at(i, j));
    }
    fprintf(stderr, "\n");
  }
}
