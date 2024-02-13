#ifndef REACTIVE_LOAD_CONTROLLER1_H
#define REACTIVE_LOAD_CONTROLLER1_H

#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class DoubleWeightCalc : public WeightCalc<double>
{
private:
  std::unordered_map<uint32_t, const double> weights;

  void CalculateWeights (void);

public:
  DoubleWeightCalc ();
  DoubleWeightCalc (std::set<uint32_t>);

  double GetInitialWeight () const override;
  double GetNonViableWeight () const override;
  double GetWeight (Edge &) const override;
  double GetWeight (uint32_t node) const;
};

class ReactiveLoadController1 : public ReactiveController
{
public:
  ReactiveLoadController1 ();
  virtual ~ReactiveLoadController1 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Flow) override;
};

} // namespace ns3

#endif
