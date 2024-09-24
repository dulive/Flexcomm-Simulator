#include "energy-calculator.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ofswitch13-device.h"
#include "reactive-flex-controller2.h"

#ifdef NS3_OFSWITCH13

NS_LOG_COMPONENT_DEFINE ("ReactiveFlexController2");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveFlexController2);

/* ########## FlexWeight ########## */

FlexWeight::FlexWeight () : value (0), hops (0)
{
}

FlexWeight::FlexWeight (int value) : value (value), hops (0)
{
}

FlexWeight::FlexWeight (int value, int hops) : value (value), hops (hops)
{
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
  return FlexWeight (value + b.value, hops + b.hops);
}

bool
FlexWeight::operator< (const FlexWeight &b) const
{
  return value == b.value ? hops < b.hops : value < b.value;
}

bool
FlexWeight::operator== (const FlexWeight &b) const
{
  return value == b.value && hops == b.hops;
}

/* ########## FlexWeightCalc ########## */

FlexWeightCalc::FlexWeightCalc (EnergyCalculator &calc) : m_calc (calc)
{
}

int
FlexWeightCalc::CalculateWeight (unsigned int node_id) const
{
  Ptr<Node> node = NodeList::GetNode (node_id);
  if (node->IsSwitch ())
    {
      Ptr<OFSwitch13Device> of_sw = node->GetObject<OFSwitch13Device> ();
      double flex = m_calc.GetRealFlex (of_sw->GetDpId ());
      return flex < 0 ? 1 : 0;
    }
  return 0;
}

FlexWeight
FlexWeightCalc::GetInitialWeight () const
{
  return FlexWeight{};
}

FlexWeight
FlexWeightCalc::GetNonViableWeight () const
{
  return FlexWeight (std::numeric_limits<int> ().max (), std::numeric_limits<int> ().max ());
}

FlexWeight
FlexWeightCalc::GetWeight (Edge &e) const
{
  int weight1 = CalculateWeight (e.first);
  int weight2 = CalculateWeight (e.second);

  return FlexWeight (weight1 + weight2, 1);
}

/* ########## ReactiveFlexController2 ########## */

ReactiveFlexController2::ReactiveFlexController2 () : m_energy_calculator (60)
{
  NS_LOG_FUNCTION (this);
}

ReactiveFlexController2::~ReactiveFlexController2 ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveFlexController2::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveFlexController2")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveFlexController2> ();
  return tid;
}
void

ReactiveFlexController2::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  ReactiveController::DoDispose ();
}

std::vector<Ptr<Node>>
ReactiveFlexController2::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  FlexWeightCalc weight_calc (m_energy_calculator);
  return Topology::DijkstraShortestPath<FlexWeight> (src_node, dst_ip, weight_calc);
}

} // namespace ns3

#endif
