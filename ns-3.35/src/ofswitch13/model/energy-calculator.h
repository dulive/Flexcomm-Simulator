#ifndef ENERGY_CALCULATOR_H
#define ENERGY_CALCULATOR_H

#include "ns3/nstime.h"
#include "ns3/object.h"
#include <unordered_map>

namespace ns3 {

class EnergyCalculator : public Object
{
public:
  EnergyCalculator ();
  virtual ~EnergyCalculator ();

  static TypeId GetTypeId (void);

  float GetMaxEnergy (uint64_t);
  double GetRealFlex (uint64_t);

protected:
  virtual void DoDispose ();

  virtual void NotifyConstructionCompleted (void);

private:
  std::unordered_map<uint64_t, std::pair<double, Time>> m_energy;

  void CollectEnergy (void);
};

} // namespace ns3

#endif
