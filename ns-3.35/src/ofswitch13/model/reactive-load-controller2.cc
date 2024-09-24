#include "reactive-load-controller2.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/ofswitch13-device.h"

NS_LOG_COMPONENT_DEFINE ("ReactiveLoadController2");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveLoadController2);

/* ########## UIntWeightCalc ########## */

UIntWeightCalc::UIntWeightCalc ()
{
}

uint64_t
UIntWeightCalc::CalculateWeight (unsigned int node_id) const
{
  Ptr<Node> node = NodeList::GetNode (node_id);
  if (node->IsSwitch ())
    {
      Ptr<OFSwitch13Device> of_sw = node->GetObject<OFSwitch13Device> ();
      return of_sw->GetCpuLoad ().GetBitRate ();
    }
  return 0;
}

uint64_t
UIntWeightCalc::GetInitialWeight () const
{
  return 0;
}

uint64_t
UIntWeightCalc::GetNonViableWeight () const
{
  return std::numeric_limits<uint64_t> ().max ();
}

uint64_t
UIntWeightCalc::GetWeight (Edge &e) const
{
  uint64_t weight1 = CalculateWeight (e.first);
  uint64_t weight2 = CalculateWeight (e.second);

  return weight1 + weight2;
}

/* ########## ReactiveLoadController2 ########## */

ReactiveLoadController2::ReactiveLoadController2 ()
{
  NS_LOG_FUNCTION (this);
}

ReactiveLoadController2::~ReactiveLoadController2 ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveLoadController2::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveLoadController2")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveLoadController2> ();
  return tid;
}
void

ReactiveLoadController2::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  ReactiveController::DoDispose ();
}

std::vector<Ptr<Node>>
ReactiveLoadController2::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  UIntWeightCalc weight_calc;
  return Topology::DijkstraShortestPath<uint64_t> (src_node, dst_ip, weight_calc);
}

} // namespace ns3
