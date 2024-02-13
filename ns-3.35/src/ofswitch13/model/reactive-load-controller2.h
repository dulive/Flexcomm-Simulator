#ifndef REACTIVE_LOAD_CONTROLLER2_H
#define REACTIVE_LOAD_CONTROLLER2_H

#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class UIntWeightCalc : public WeightCalc<uint64_t>
{
private:
  std::unordered_map<uint32_t, const uint64_t> weights;

  void CalculateWeights (void);

public:
  UIntWeightCalc ();
  UIntWeightCalc (std::set<uint32_t>);

  uint64_t GetInitialWeight () const override;
  uint64_t GetNonViableWeight () const override;
  uint64_t GetWeight (Edge &) const override;
  uint64_t GetWeight (uint32_t node) const;
};

class ReactiveLoadController2 : public ReactiveController
{
public:
  ReactiveLoadController2 ();
  virtual ~ReactiveLoadController2 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Flow) override;
};

} // namespace ns3

#endif
