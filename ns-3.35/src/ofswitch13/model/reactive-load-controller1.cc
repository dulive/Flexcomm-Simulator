#include "reactive-load-controller1.h"
#include "ns3/log.h"
#include "ns3/ofswitch13-device.h"

NS_LOG_COMPONENT_DEFINE ("ReactiveLoadController1");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveLoadController1);

/* ########## DoubleWeightCalc ########## */

DoubleWeightCalc::DoubleWeightCalc () : weights ()
{
  CalculateWeights ();
}

void
DoubleWeightCalc::CalculateWeights ()
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
          weights.insert ({node_id, of_sw->GetCpuUsage ()});
        }
    }
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
  return weights.at (e.first) + weights.at (e.second);
}

double
DoubleWeightCalc::GetWeight (uint32_t node) const
{
  return weights.at (node);
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
ReactiveLoadController1::CalculatePath (Flow flow)
{
  DoubleWeightCalc weight_calc;
  return Topology::DijkstraShortestPath<double> (flow.src_ip, flow.dst_ip, weight_calc);
}

} // namespace ns3
