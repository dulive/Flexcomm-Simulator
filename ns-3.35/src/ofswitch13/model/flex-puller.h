#ifndef FLEX_PULLER_H
#define FLEX_PULLER_H

#include "ns3/nstime.h"
#include "ns3/object.h"
#include <unordered_map>

namespace ns3 {

class FlexPuller : public Object
{

private:
  static std::unordered_map<uint64_t, std::tuple<double, double, Time>> m_readings;

public:
  FlexPuller () = delete;

  static TypeId GetTypeId (void);

  static void InnitNode (uint64_t);
  static void UpdateReadings ();
  static std::tuple<double, double, Time> GetReading (uint64_t);

  static float GetMaxEnergy (uint64_t);
  static double GetRealFlex (uint64_t);
};

} // namespace ns3

#endif
