#ifndef _CONDOR2EXTRACTOR_H
#define _CONDOR2EXTRACTOR_H

#include <string>
#include <map>
#include <libmesh/numeric_vector.h>
#include <libmesh/dof_map.h>
#include "Extractor.h"
#include "PuncturedElem.h"
#include "io/Condor2Dataset.h"

using namespace libMesh; 

/* 
 * \class   Condor2VortexExtractor
 * \author  Hanqi Guo
 * \brief   Vortex extractor for Condor2 output
*/
class Condor2VortexExtractor : public VortexExtractor
{
public:
  Condor2VortexExtractor(); 
  ~Condor2VortexExtractor();

  void SetDataset(const GLDataset* ds);
  const Condor2Dataset* Dataset() const {return _ds;}

  void Extract();

protected:
  std::vector<unsigned int> Neighbors(unsigned int elem_id) const;

private: 
  int _verbose; 
  bool _gauge; 
 
  const Condor2Dataset *_ds;
}; 

#endif
