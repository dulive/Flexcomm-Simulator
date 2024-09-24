#ifndef ENERGY_CALCULATOR_H
#define ENERGY_CALCULATOR_H

#include "ns3/object.h"
#include <unordered_map>

namespace ns3 {

class EnergyCalculator : public Object
{
public:
  EnergyCalculator ();
  EnergyCalculator (unsigned int);
  virtual ~EnergyCalculator ();

  static TypeId GetTypeId (void);

  float GetMaxEnergy (uint64_t);
  double GetRealFlex (uint64_t);

protected:
  virtual void DoDispose ();

  virtual void NotifyConstructionCompleted (void);

private:
  std::unordered_map<uint64_t, double> m_energy;
  unsigned int m_poll_frequency;

  void CollectEnergy (void);
};

} // namespace ns3

#endif
