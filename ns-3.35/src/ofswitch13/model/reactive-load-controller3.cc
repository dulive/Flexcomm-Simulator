#include "reactive-load-controller3.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/ofswitch13-device.h"

NS_LOG_COMPONENT_DEFINE ("ReactiveLoadController3");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveLoadController3);

/* ########## LoadWeight ########## */

LoadWeight::LoadWeight () : max_usage (0.0), load (0), hops (0)
{
}

LoadWeight::LoadWeight (double max_usage, DataRate load)
    : max_usage (max_usage), load (load), hops (0)
{
}

LoadWeight::LoadWeight (double max_usage, DataRate load, int hops)
    : max_usage (max_usage), load (load), hops (hops)
{
}

double
LoadWeight::GetMaxUsage () const
{
  return max_usage;
}

DataRate
LoadWeight::GetLoad () const
{
  return load;
}

int
LoadWeight::GetHops () const
{
  return hops;
}

LoadWeight
LoadWeight::operator+ (const LoadWeight &b) const
{
  return LoadWeight (std::max (max_usage, b.max_usage), this->GetLoad () + b.GetLoad (),
                     hops + b.hops);
}

bool
LoadWeight::operator< (const LoadWeight &b) const
{
  if (max_usage >= 1 && b.max_usage < 1)
    return false;
  else if (max_usage < 1 && b.max_usage >= 1)
    return true;
  else
    return load == b.load ? hops == b.hops ? max_usage < b.max_usage : hops < b.hops
                          : load < b.load;
}

bool
LoadWeight::operator== (const LoadWeight &b) const
{
  return max_usage == b.max_usage && load == b.load && hops == b.hops;
}

/* ########## LoadWeightCalc ########## */

LoadWeightCalc::LoadWeightCalc ()
{
}

std::pair<double, DataRate>
LoadWeightCalc::CalculateWeight (unsigned int node_id) const
{
  Ptr<Node> node = NodeList::GetNode (node_id);
  if (node->IsSwitch ())
    {
      Ptr<OFSwitch13Device> of_sw = node->GetObject<OFSwitch13Device> ();
      return std::make_pair (of_sw->GetCpuUsage (), of_sw->GetCpuLoad ());
    }
  return std::make_pair (0, DataRate (0));
}

LoadWeight
LoadWeightCalc::GetInitialWeight () const
{
  return LoadWeight ();
}

LoadWeight
LoadWeightCalc::GetNonViableWeight () const
{
  return LoadWeight (std::numeric_limits<double> ().max (),
                     DataRate (std::numeric_limits<uint64_t> ().max ()),
                     std::numeric_limits<int> ().max ());
}

LoadWeight
LoadWeightCalc::GetWeight (Edge &e) const
{
  std::pair<double, DataRate> weight1 = CalculateWeight (e.first);
  std::pair<double, DataRate> weight2 = CalculateWeight (e.second);

  return LoadWeight (std::max (weight1.first, weight2.first), weight1.second + weight2.second, 1);
}

/* ########## ReactiveLoadController3 ########## */

ReactiveLoadController3::ReactiveLoadController3 ()
{
  NS_LOG_FUNCTION (this);
}

ReactiveLoadController3::~ReactiveLoadController3 ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveLoadController3::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveLoadController3")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveLoadController3> ();
  return tid;
}
void

ReactiveLoadController3::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  ReactiveController::DoDispose ();
}

std::vector<Ptr<Node>>
ReactiveLoadController3::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  LoadWeightCalc weight_calc;
  return Topology::DijkstraShortestPath<LoadWeight> (src_node, dst_ip, weight_calc);
}

} // namespace ns3
