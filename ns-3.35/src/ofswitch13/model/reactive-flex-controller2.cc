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

/* ########## FlexWeight2 ########## */

FlexWeight2::FlexWeight2 () : value (0), hops (0)
{
}

FlexWeight2::FlexWeight2 (int value) : value (value), hops (0)
{
}

FlexWeight2::FlexWeight2 (int value, int hops) : value (value), hops (hops)
{
}

int
FlexWeight2::GetValue () const
{
  return value;
}

int
FlexWeight2::GetHops () const
{
  return hops;
}

FlexWeight2
FlexWeight2::operator+ (const FlexWeight2 &b) const
{
  return FlexWeight2 (value + b.value, hops + b.hops);
}

bool
FlexWeight2::operator< (const FlexWeight2 &b) const
{
  return value == b.value ? hops < b.hops : value < b.value;
}

bool
FlexWeight2::operator== (const FlexWeight2 &b) const
{
  return value == b.value && hops == b.hops;
}

/* ########## FlexWeightCalc2 ########## */

FlexWeightCalc2::FlexWeightCalc2 (EnergyCalculator &calc) : m_calc (calc)
{
}

int
FlexWeightCalc2::CalculateWeight (unsigned int node_id) const
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

FlexWeight2
FlexWeightCalc2::GetInitialWeight () const
{
  return FlexWeight2{};
}

FlexWeight2
FlexWeightCalc2::GetNonViableWeight () const
{
  return FlexWeight2 (std::numeric_limits<int> ().max (), std::numeric_limits<int> ().max ());
}

FlexWeight2
FlexWeightCalc2::GetWeight (Edge &e) const
{
  int weight1 = CalculateWeight (e.first);
  int weight2 = CalculateWeight (e.second);

  return FlexWeight2 (weight1 + weight2, 1);
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
  FlexWeightCalc2 weight_calc (m_energy_calculator);
  return Topology::DijkstraShortestPath<FlexWeight2> (src_node, dst_ip, weight_calc);
}

} // namespace ns3

#endif
