#include "energy-calculator.h"
#include "ns3/energy-api.h"
#include "ns3/log-macros-disabled.h"
#include "ns3/node-container.h"
#include "ns3/node-energy-model.h"
#include "ns3/ofswitch13-device.h"
#include "ns3/simulator.h"
#include <string>

NS_LOG_COMPONENT_DEFINE ("EnergyCalculator");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (EnergyCalculator);

EnergyCalculator::EnergyCalculator () : m_energy (), m_poll_frequency (10)
{
}

EnergyCalculator::EnergyCalculator (unsigned int poll_frequency)
    : m_energy (), m_poll_frequency (poll_frequency)
{
}

EnergyCalculator::~EnergyCalculator ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
EnergyCalculator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EnergyCalculator")
                          .SetParent<Object> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<EnergyCalculator> ();
  return tid;
}

void
EnergyCalculator::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_energy.clear ();
}

void
EnergyCalculator::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  Simulator::ScheduleNow (&EnergyCalculator::CollectEnergy, this);

  Object::NotifyConstructionCompleted ();
}

void
EnergyCalculator::CollectEnergy ()
{
  NodeContainer switches = NodeContainer::GetGlobalSwitches ();
  for (NodeContainer::Iterator it = switches.Begin (); it != switches.End (); it++)
    {
      Ptr<OFSwitch13Device> of_sw = (*it)->GetObject<OFSwitch13Device> ();
      m_energy[of_sw->GetDatapathId ()] = of_sw->GetObject<NodeEnergyModel> ()->GetPowerDrawn ();
    }
  Simulator::Schedule (Seconds (m_poll_frequency), &EnergyCalculator::CollectEnergy, this);
}

float
EnergyCalculator::GetMaxEnergy (uint64_t node)
{
  std::string emsId = Names::FindName (OFSwitch13Device::GetDevice (node)->GetObject<Node> ());
  Time now = Simulator::Now ();

  int hour = static_cast<int> (trunc (now.GetHours ()));
  int min = static_cast<int> (trunc (now.GetMinutes ()));

  //verify
  int index = (hour * 4) + (min / 15);

  return EnergyAPI::GetEstimateArray (emsId)[index] + EnergyAPI::GetFlexArray (emsId)[index];
}

double
EnergyCalculator::GetRealFlex (uint64_t node)
{
  float max_acc_15 = GetMaxEnergy (node);
  double max_acc = max_acc_15 / (900.0 / m_poll_frequency);

  return max_acc - m_energy[node];
}

} // namespace ns3
