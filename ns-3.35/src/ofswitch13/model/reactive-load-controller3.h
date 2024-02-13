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

  LoadWeight operator+ (const LoadWeight &b) const;
  bool operator< (const LoadWeight &b) const;
  bool operator== (const LoadWeight &b) const;
};

class LoadWeightCalc : public WeightCalc<LoadWeight>
{
private:
  std::unordered_map<uint32_t, const LoadWeight> weights;

  void CalculateWeights (void);

public:
  LoadWeightCalc ();
  LoadWeightCalc (std::set<uint32_t>);

  LoadWeight GetInitialWeight () const override;
  LoadWeight GetNonViableWeight () const override;
  LoadWeight GetWeight (Edge &) const override;
  LoadWeight GetWeight (uint32_t node) const;
};

class ReactiveLoadController3 : public ReactiveController
{
public:
  ReactiveLoadController3 ();
  virtual ~ReactiveLoadController3 ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  std::vector<Ptr<Node>> CalculatePath (Flow) override;
};

} // namespace ns3

#endif
