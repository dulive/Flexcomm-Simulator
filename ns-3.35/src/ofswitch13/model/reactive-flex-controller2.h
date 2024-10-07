#ifndef REACTIVE_FLEX_CONTROLLER2_H
#define REACTIVE_FLEX_CONTROLLER2_H

#include "energy-calculator.h"
#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class FlexWeight2
{
private:
  int value;
  int hops;

public:
  FlexWeight2 ();
  FlexWeight2 (int);
  FlexWeight2 (int, int);

  int GetValue () const;
  int GetHops () const;

  FlexWeight2 operator+ (const FlexWeight2 &) const;
  bool operator< (const FlexWeight2 &) const;
  bool operator== (const FlexWeight2 &) const;
};

class FlexWeightCalc2 : public WeightCalc<FlexWeight2>
{
private:
  EnergyCalculator &m_calc;
  int CalculateWeight (unsigned int) const;

public:
  FlexWeightCalc2 (EnergyCalculator &);

  FlexWeight2 GetInitialWeight () const override;
  FlexWeight2 GetNonViableWeight () const override;
  FlexWeight2 GetWeight (Edge &) const override;
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
