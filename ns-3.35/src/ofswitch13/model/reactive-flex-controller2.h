#ifndef REACTIVE_FLEX_CONTROLLER2_H
#define REACTIVE_FLEX_CONTROLLER2_H

#include "energy-calculator.h"
#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class FlexWeight
{
private:
  int value;
  int hops;

public:
  FlexWeight ();
  FlexWeight (int);
  FlexWeight (int, int);

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
  int CalculateWeight (unsigned int) const;

public:
  FlexWeightCalc (EnergyCalculator &);

  FlexWeight GetInitialWeight () const override;
  FlexWeight GetNonViableWeight () const override;
  FlexWeight GetWeight (Edge &) const override;
};

class ReactiveFlexController2 : public ReactiveController
{
public:
  ReactiveFlexController2 ();
  virtual ~ReactiveFlexController2 ();

  static TypeId GetTypeId ();
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address) override;

private:
  EnergyCalculator m_energy_calculator;
};

} // namespace ns3

#endif
