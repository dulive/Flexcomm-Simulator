#ifndef REACTIVE_FLEX_CONTROLLER_H
#define REACTIVE_FLEX_CONTROLLER_H

#include "energy-calculator.h"
#include "ns3/ipv4-address.h"
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

  FlexWeight operator+ (const FlexWeight &) const;
  bool operator< (const FlexWeight &) const;
  bool operator== (const FlexWeight &) const;
};

class FlexWeightCalc : public WeightCalc<FlexWeight>
{
private:
  EnergyCalculator &m_calc;
  std::pair<double, int> CalculateWeight (unsigned int) const;

public:
  FlexWeightCalc (EnergyCalculator &);

  FlexWeight GetInitialWeight () const override;
  FlexWeight GetNonViableWeight () const override;
  FlexWeight GetWeight (Edge &) const override;
};

class ReactiveFlexController : public ReactiveController
{
public:
  ReactiveFlexController ();
  virtual ~ReactiveFlexController ();

  static TypeId GetTypeId ();
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address) override;

private:
  EnergyCalculator m_energy_calculator;
};

} // namespace ns3

#endif
