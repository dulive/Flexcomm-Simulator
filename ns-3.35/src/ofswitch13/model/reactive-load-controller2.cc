#include "reactive-load-controller2.h"
#include "ns3/log.h"
#include "ns3/ofswitch13-device.h"

NS_LOG_COMPONENT_DEFINE ("ReactiveLoadController2");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveLoadController2);

/* ########## UIntWeightCalc ########## */

UIntWeightCalc::UIntWeightCalc () : weights ()
{
  CalculateWeights ();
}

void
UIntWeightCalc::CalculateWeights ()
{
  auto node_container = NodeContainer::GetGlobal ();
  for (auto it = node_container.Begin (); it != node_container.End (); ++it)
    {
      uint32_t node_id = (*it)->GetId ();
      if ((*it)->IsHost ())
        {
          weights.insert ({node_id, GetInitialWeight ()});
        }
      else if ((*it)->IsSwitch ())
        {
          Ptr<OFSwitch13Device> of_sw = (*it)->GetObject<OFSwitch13Device> ();
          weights.insert ({node_id, of_sw->GetCpuLoad ().GetBitRate ()});
        }
    }
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
  return weights.at (e.first) + weights.at (e.second);
}

uint64_t
UIntWeightCalc::GetWeight (uint32_t node) const
{
  return weights.at (node);
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
ReactiveLoadController2::CalculatePath (Flow flow)
{
  UIntWeightCalc weight_calc;
  std::vector<Ptr<Node>> path;
  if (flow.eth_type == ETH_TYPE_IP)
    {
      path = Topology::DijkstraShortestPath<uint64_t> (flow.src_ip, flow.dst_ip, weight_calc);
    }
  else
    {
      path = Topology::DijkstraShortestPath<uint64_t> (flow.src_mac, flow.dst_mac, weight_calc);
    }
  return path;
}

} // namespace ns3
