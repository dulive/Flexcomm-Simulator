#ifndef REACTIVE_FLEX_CONTROLLER_H
#define REACTIVE_FLEX_CONTROLLER_H

#include "energy-calculator.h"
#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class FlexWeight
{
private:
  double max_usage;
  int value;
  int hops;

public:
  FlexWeight ();
  FlexWeight (double, int);
  FlexWeight (double, int, int);

  double GetMaxUsage () const;
  int GetValue () const;
  int GetHops () const;

  FlexWeight operator+ (const FlexWeight &b) const;
  bool operator< (const FlexWeight &b) const;
  bool operator== (const FlexWeight &b) const;
};

class FlexWeightCalc : public WeightCalc<FlexWeight>
{
private:
  std::unordered_map<uint32_t, const FlexWeight> weights;

  void CalculateWeights (EnergyCalculator &calc);

public:
  FlexWeightCalc (EnergyCalculator &calc);

  FlexWeight GetInitialWeight () const override;
  FlexWeight GetNonViableWeight () const override;
  FlexWeight GetWeight (Edge &) const override;
  FlexWeight GetWeight (uint32_t node) const;
};

class ReactiveFlexController : public ReactiveController
{
public:
  ReactiveFlexController ();
  virtual ~ReactiveFlexController ();

  static TypeId GetTypeId ();
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Flow) override;

private:
  EnergyCalculator m_energy_calculator;
};

} // namespace ns3

#endif
