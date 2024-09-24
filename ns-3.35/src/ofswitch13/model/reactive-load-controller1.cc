#include "reactive-load-controller1.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/ofswitch13-device.h"

NS_LOG_COMPONENT_DEFINE ("ReactiveLoadController1");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveLoadController1);

/* ########## DoubleWeightCalc ########## */

DoubleWeightCalc::DoubleWeightCalc ()
{
}

double
DoubleWeightCalc::CalculateWeight (unsigned int node_id) const
{
  Ptr<Node> node = NodeList::GetNode (node_id);
  if (node->IsSwitch ())
    {
      Ptr<OFSwitch13Device> of_sw = node->GetObject<OFSwitch13Device> ();
      return of_sw->GetCpuUsage ();
    }
  return 0.0;
}

double
DoubleWeightCalc::GetInitialWeight () const
{
  return 0.0;
}

double
DoubleWeightCalc::GetNonViableWeight () const
{
  return std::numeric_limits<double> ().max ();
}

double
DoubleWeightCalc::GetWeight (Edge &e) const
{
  double weight1 = CalculateWeight (e.first);
  double weight2 = CalculateWeight (e.second);

  return weight1 + weight2;
}

/* ########## ReactiveLoadController1 ########## */

ReactiveLoadController1::ReactiveLoadController1 ()
{
  NS_LOG_FUNCTION (this);
}

ReactiveLoadController1::~ReactiveLoadController1 ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveLoadController1::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveLoadController1")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveLoadController1> ();
  return tid;
}
void

ReactiveLoadController1::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  ReactiveController::DoDispose ();
}

std::vector<Ptr<Node>>
ReactiveLoadController1::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  DoubleWeightCalc weight_calc;
  return Topology::DijkstraShortestPath<double> (src_node, dst_ip, weight_calc);
}

} // namespace ns3
