#ifndef REACTIVE_LOAD_CONTROLLER3_H
#define REACTIVE_LOAD_CONTROLLER3_H

#include "ns3/topology.h"
#include "reactive-controller.h"

namespace ns3 {

class LoadWeight
{
private:
  double max_usage;
  DataRate load;
  int hops;

public:
  LoadWeight ();
  LoadWeight (double, DataRate);
  LoadWeight (double, DataRate, int);

  double GetMaxUsage () const;
  DataRate GetLoad () const;
  int GetHops () const;

  LoadWeight operator+ (const LoadWeight &) const;
  bool operator< (const LoadWeight &) const;
  bool operator== (const LoadWeight &) const;
};

class LoadWeightCalc : public WeightCalc<LoadWeight>
{
private:
  std::pair<double, DataRate> CalculateWeight (unsigned int) const;

public:
  LoadWeightCalc ();

  LoadWeight GetInitialWeight () const override;
  LoadWeight GetNonViableWeight () const override;
  LoadWeight GetWeight (Edge &) const override;
};

class ReactiveLoadController3 : public ReactiveController
{
public:
  ReactiveLoadController3 ();
  virtual ~ReactiveLoadController3 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address) override;
};

} // namespace ns3

#endif
