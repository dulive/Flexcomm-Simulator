#ifndef REACTIVE_LOAD_CONTROLLER2_H
#define REACTIVE_LOAD_CONTROLLER2_H

#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class UIntWeightCalc : public WeightCalc<uint64_t>
{
private:
  uint64_t CalculateWeight (unsigned int) const;

public:
  UIntWeightCalc ();

  uint64_t GetInitialWeight () const override;
  uint64_t GetNonViableWeight () const override;
  uint64_t GetWeight (Edge &) const override;
};

class ReactiveLoadController2 : public ReactiveController
{
public:
  ReactiveLoadController2 ();
  virtual ~ReactiveLoadController2 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address) override;
};

} // namespace ns3

#endif
