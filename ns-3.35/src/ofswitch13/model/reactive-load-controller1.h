#ifndef REACTIVE_LOAD_CONTROLLER1_H
#define REACTIVE_LOAD_CONTROLLER1_H

#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class DoubleWeightCalc : public WeightCalc<double>
{
private:
  double CalculateWeight (unsigned int) const;

public:
  DoubleWeightCalc ();

  double GetInitialWeight () const override;
  double GetNonViableWeight () const override;
  double GetWeight (Edge &) const override;
};

class ReactiveLoadController1 : public ReactiveController
{
public:
  ReactiveLoadController1 ();
  virtual ~ReactiveLoadController1 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address) override;
};

} // namespace ns3

#endif
