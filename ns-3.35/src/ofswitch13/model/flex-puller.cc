#include "flex-puller.h"
#include "ns3/energy-api.h"
#include "ns3/node-energy-model.h"
#include "ns3/ofswitch13-device.h"
#include <string>

NS_LOG_COMPONENT_DEFINE ("FlexPuller");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (FlexPuller);

std::unordered_map<uint64_t, std::tuple<double, double, Time>> FlexPuller::m_readings{};

TypeId
FlexPuller::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FlexPuller").SetParent<Object> ();
  return tid;
}

void
FlexPuller::InnitNode (uint64_t node)
{
  Ptr<OFSwitch13Device> of_device = OFSwitch13Device::GetDevice (node);
  double energy = of_device->GetObject<NodeEnergyModel> ()->GetPowerDrawn ();
  m_readings[node] = std::tuple (energy, energy, Simulator::Now ());
}

void
FlexPuller::UpdateReadings ()
{
  for (auto it = m_readings.begin (); it != m_readings.end (); it++)
    {
      double last_acc = std::get<0> (m_readings[it->first]);
      double current_acc =
          OFSwitch13Device::GetDevice (it->first)->GetObject<NodeEnergyModel> ()->GetPowerDrawn ();
      it->second = std::tuple (current_acc, last_acc, Simulator::Now ());
    }
}

std::tuple<double, double, Time>
FlexPuller::GetReading (uint64_t node)
{
  return m_readings[node];
}

float
FlexPuller::GetMaxEnergy (uint64_t node)
{
  std::string emsId = Names::FindName (OFSwitch13Device::GetDevice (node)->GetObject<Node> ());
  Time now = Simulator::Now ();

  int hour = static_cast<int> (trunc (now.GetHours ()));
  int min = static_cast<int> (trunc (now.GetMinutes ()));

  int index = (hour * 4) + (min / 15);

  return EnergyAPI::GetEstimateArray (emsId)[index] + EnergyAPI::GetFlexArray (emsId)[index];
}

double
FlexPuller::GetRealFlex (uint64_t node)
{
  float max_acc_15 = GetMaxEnergy (node);
  Time current_time = Simulator::Now ();
  auto last_read = m_readings[node];

  double acc, max_acc;
  if (current_time == std::get<2> (last_read))
    {
      acc = std::get<0> (last_read) - std::get<1> (last_read);
      max_acc = max_acc_15 / 5;
    }
  else
    {
      Ptr<OFSwitch13Device> of_sw = OFSwitch13Device::GetDevice (node);
      double current_read = of_sw->GetObject<NodeEnergyModel> ()->GetPowerDrawn ();
      acc = current_read - std::get<0> (last_read);
      // verify
      max_acc = max_acc_15 / (900 / trunc ((current_time - std::get<2> (last_read)).GetSeconds ()));
    }

  return max_acc - acc;
}

} // namespace ns3
