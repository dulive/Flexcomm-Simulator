#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ofswitch13-device.h"
#include "reactive-flex-controller.h"

#ifdef NS3_OFSWITCH13

NS_LOG_COMPONENT_DEFINE ("ReactiveFlexController");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveFlexController);

/* ########## FlexWeight ########## */

FlexWeight::FlexWeight () : max_usage (0.0), value (0), hops (0)
{
}

FlexWeight::FlexWeight (double max_usage, int value)
    : max_usage (max_usage), value (value), hops (0)
{
}

FlexWeight::FlexWeight (double max_usage, int value, int hops)
    : max_usage (max_usage), value (value), hops (hops)
{
}

double
FlexWeight::GetMaxUsage () const
{
  return max_usage;
}

int
FlexWeight::GetValue () const
{
  return value;
}

int
FlexWeight::GetHops () const
{
  return hops;
}

FlexWeight
FlexWeight::operator+ (const FlexWeight &b) const
{
  return FlexWeight (std::max (max_usage, b.max_usage), value + b.value, hops + b.hops);
}

bool
FlexWeight::operator< (const FlexWeight &b) const
{
  if (max_usage >= 1 && b.max_usage < 1)
    return false;
  else if (max_usage < 1 && b.max_usage >= 1)
    return true;
  else
    return value == b.value ? hops == b.hops ? max_usage < b.max_usage : hops < b.hops
                            : value < b.value;
}

bool
FlexWeight::operator== (const FlexWeight &b) const
{
  return max_usage == b.max_usage && value == b.value && hops == b.hops;
}

/* ########## FlexWeightCalc ########## */

FlexWeightCalc::FlexWeightCalc (EnergyCalculator &calc) : m_calc (calc)
{
}

std::pair<double, int>
FlexWeightCalc::CalculateWeight (unsigned int node_id) const
{
  Ptr<Node> node = NodeList::GetNode (node_id);
  if (node->IsSwitch ())
    {
      Ptr<OFSwitch13Device> of_sw = node->GetObject<OFSwitch13Device> ();
      double flex = m_calc.GetRealFlex (of_sw->GetDpId ());
      return std::make_pair (of_sw->GetCpuUsage (), flex < 0 ? 1 : 0);
    }
  return std::make_pair (0, 0);
}

FlexWeight
FlexWeightCalc::GetInitialWeight () const
{
  return FlexWeight{};
}

FlexWeight
FlexWeightCalc::GetNonViableWeight () const
{
  return FlexWeight (std::numeric_limits<double> ().max (), std::numeric_limits<int> ().max (),
                     std::numeric_limits<int> ().max ());
}

FlexWeight
FlexWeightCalc::GetWeight (Edge &e) const
{
  std::pair<double, int> weight1 = CalculateWeight (e.first);
  std::pair<double, int> weight2 = CalculateWeight (e.second);

  return FlexWeight (std::max (weight1.first, weight2.first), weight1.second + weight2.second, 1);
}

/* ########## ReactiveFlexController ########## */

ReactiveFlexController::ReactiveFlexController () : m_energy_calculator (60)
{
  NS_LOG_FUNCTION (this);
}

ReactiveFlexController::~ReactiveFlexController ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveFlexController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveFlexController")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveFlexController> ();
  return tid;
}
void

ReactiveFlexController::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  ReactiveController::DoDispose ();
}

std::vector<Ptr<Node>>
ReactiveFlexController::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  FlexWeightCalc weight_calc (m_energy_calculator);
  return Topology::DijkstraShortestPath<FlexWeight> (src_node, dst_ip, weight_calc);
}

} // namespace ns3

#endif
