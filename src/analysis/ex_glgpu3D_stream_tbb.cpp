#include <iostream>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <vector>
#include <queue>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <tbb/flow_graph.h>
#include <tbb/concurrent_unordered_map.h>
#include "io/GLGPU3DDataset.h"
#include "extractor/Extractor.h"

enum {
  VFGPU_MSG_PF = 0,
  VFGPU_MSG_PE = 1
};

enum {
  VFGPU_MESH_HEX = 0,
  VFGPU_MESH_TET,
  VFGPU_MESH_2D
};

enum {
  VFGPU_GAUGE_YZ = 0,
  VFGPU_GAUGE_XZ = 1
};

typedef struct {
  unsigned int fid_and_chirality; 
  float pos[3];
} vfgpu_pf_t; // punctured faces from GPU output, 16 bytes

typedef unsigned int vfgpu_pe_t; 

typedef struct {
  int frame;
  int d[3];
  unsigned int count; // d[0]*d[1]*d[2];
  bool pbc[3];
  float origins[3];
  float lengths[3];
  float cell_lengths[3];
  float B[3];
  float Kx;
} vfgpu_hdr_t;

tbb::concurrent_unordered_map<int, vfgpu_hdr_t> hdrs_all;
tbb::concurrent_unordered_map<int, std::vector<vfgpu_pf_t> > pfs_all;
tbb::concurrent_unordered_map<std::pair<int, int>, std::vector<vfgpu_pe_t> > pes_all;

/////////////////
struct extract {
  int frame;
  extract(int frame_) : frame(frame_) {}

  void operator()(tbb::flow::continue_msg) const {
    const vfgpu_hdr_t& hdr = hdrs_all[frame];
    const std::vector<vfgpu_pf_t>& pfs = pfs_all[frame];

    GLHeader h;
    h.ndims = 3;
    memcpy(h.dims, hdr.d, sizeof(int)*3);
    memcpy(h.pbc, hdr.pbc, sizeof(int)*3);
    memcpy(h.lengths, hdr.lengths, sizeof(float)*3);
    memcpy(h.origins, hdr.origins, sizeof(float)*3);
    memcpy(h.cell_lengths, hdr.cell_lengths, sizeof(float)*3);

    GLGPU3DDataset *ds = new GLGPU3DDataset;
    ds->SetHeader(h);
    ds->SetMeshType(GLGPU3D_MESH_HEX); // TODO
    ds->BuildMeshGraph();

    VortexExtractor *ex = new VortexExtractor;
    ex->SetDataset(ds);
    
    ex->Clear();
    for (int i=0; i<pfs.size(); i++) {
      const vfgpu_pf_t &pf = pfs[i];
      int chirality = pf.fid_and_chirality & 0x80000000 ? 1 : -1;
      int fid = pf.fid_and_chirality & 0x7fffffff;
      ex->AddPuncturedFace(fid, 0, chirality, pf.pos);
    }
    ex->TraceOverSpace(0);

    std::vector<VortexLine> vlines = ex->GetVortexLines();

    std::stringstream ss;
    ss << "vlines-" << hdr.frame << ".vtk";
    SaveVortexLinesVTK(vlines, ss.str());

    delete ds;
    delete ex;
    
    fprintf(stderr, "frame=%d, #pfs=%d, #vlines=%d\n", 
        hdr.frame, (int)pfs.size(), (int)vlines.size());
  }
}; 

/////////////////
struct track {
  std::pair<int, int> frames;
  track(const std::pair<int, int> f) : frames(f) {}

  void operator()(tbb::flow::continue_msg) const {
    // TODO
  }
};

/////////////////
int main(int argc, char **argv)
{
  using namespace tbb::flow;
  graph g;

  int type_msg;
  vfgpu_hdr_t hdr;
  int pfcount, pecount;

  FILE *fp = stdin;
  while (!feof(fp)) {
    size_t count = fread(&type_msg, sizeof(int), 1, fp);
    if (count != 1) break;

    if (type_msg == VFGPU_MSG_PF) {
      fread(&hdr, sizeof(vfgpu_hdr_t), 1, fp);
      fread(&pfcount, sizeof(int), 1, fp);
      
      hdrs_all[hdr.frame] = hdr;
      std::vector<vfgpu_pf_t> &pfs = pfs_all[hdr.frame];
      pfs.resize(pfcount);
      fread(pfs.data(), sizeof(vfgpu_pf_t), pfcount, fp);
      
      continue_node<continue_msg> *e = new continue_node<continue_msg>(g, extract(hdr.frame));
      e->try_put(continue_msg());
    } else if (type_msg == VFGPU_MSG_PE) {
      std::pair<int, int> frames;
      fread(&frames, sizeof(int), 2, fp);
      fread(&pecount, sizeof(int), 1, fp);

      std::vector<vfgpu_pe_t> &pes = pes_all[frames];
      pes.resize(pecount);
      fread(pes.data(), sizeof(vfgpu_pe_t), pecount, fp);
    
      // continue_node<continue_msg> *t = new continue_node<continue_msg>(g, track(frames));
      // make_edge(*e, *t);
      // make_edge(*e, *t);
    }
  }

  fclose(fp);

  g.wait_for_all();
  fprintf(stderr, "exiting...\n");
  return 0;
}
