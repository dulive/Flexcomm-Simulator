#ifndef PARTIAL_PATH_CONTROLLER_H
#define PARTIAL_PATH_CONTROLLER_H

#include "energy-calculator.h"
#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/ofswitch13-controller.h"
#include "ns3/topology.h"
#include <boost/container_hash/hash_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <unordered_map>

namespace ns3 {

struct Flow
{
  Mac48Address src_mac;
  Mac48Address dst_mac;
  uint16_t eth_type = 0;

  Ipv4Address src_ip;
  Ipv4Address dst_ip;
  uint16_t ip_proto = 0;

  uint16_t src_port = 0;
  uint16_t dst_port = 0;

  uint16_t priority = 0;

  bool
  operator== (const Flow &flow) const
  {
    return src_mac == flow.src_mac && dst_mac == flow.dst_mac && eth_type == flow.eth_type &&
           src_ip == flow.src_ip && dst_ip == flow.dst_ip && ip_proto == flow.ip_proto &&
           src_port == flow.src_port && dst_port == flow.dst_port;
  }

  bool
  operator< (const Flow &flow) const
  {
    return src_mac < flow.src_mac && dst_mac < flow.dst_mac && eth_type < flow.eth_type &&
           src_ip < flow.src_ip && dst_ip < flow.dst_ip && ip_proto < flow.ip_proto &&
           src_port < flow.src_port && dst_port < flow.dst_port;
  }
};

struct hash_fn
{
  std::size_t
  operator() (const Flow &flow) const
  {
    std::size_t seed = 0;
    uint8_t buffer[6];
    flow.src_mac.CopyTo (buffer);
    boost::hash_combine (seed, buffer);
    flow.dst_mac.CopyTo (buffer);
    boost::hash_combine (seed, buffer);
    boost::hash_combine (seed, flow.eth_type);

    boost::hash_combine (seed, flow.src_ip.Get ());
    boost::hash_combine (seed, flow.dst_ip.Get ());
    boost::hash_combine (seed, flow.ip_proto);

    boost::hash_combine (seed, flow.src_port);
    boost::hash_combine (seed, flow.dst_port);

    boost::hash_combine (seed, flow.priority);

    return seed;
  }
};

class FlexWeight
{
private:
  double max_usage;
  int value;
  int hops;

public:
  FlexWeight ();
  FlexWeight (double, int);
  FlexWeight (double, int, int);

  double GetMaxUsage () const;
  int GetValue () const;
  int GetHops () const;

  FlexWeight operator+ (const FlexWeight &b) const;
  bool operator< (const FlexWeight &b) const;
  bool operator== (const FlexWeight &b) const;
};

class FlexWeightCalc : public WeightCalc<FlexWeight>
{
private:
  std::unordered_map<uint32_t, const FlexWeight> weights;

  void CalculateWeights (EnergyCalculator &calc);

public:
  FlexWeightCalc (EnergyCalculator &calc);

  FlexWeight GetInitialWeight () const override;
  FlexWeight GetNonViableWeight () const override;
  FlexWeight GetWeight (Edge &) const override;
  FlexWeight GetWeight (uint32_t node) const;
};

class PartialPathController : public OFSwitch13Controller
{
public:
  PartialPathController ();
  virtual ~PartialPathController ();

  static TypeId GetTypeId ();
  virtual void DoDispose () override;

protected:
  void HandshakeSuccessful (Ptr<const RemoteSwitch> swtch) override;

  ofl_err HandlePacketIn (struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
                          uint32_t xid) override;

  ofl_err HandleFlowRemoved (struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch,
                             uint32_t xid) override;

  /* ofl_err HandlePortStatus (struct ofl_msg_port_status *msg, Ptr<const RemoteSwitch> swtch, */
  /*                           uint32_t xid) override; */
  /**/
  /* ofl_err HandleMultipartReply (struct ofl_msg_multipart_reply_header *msg, */
  /*                               Ptr<const RemoteSwitch> swtch, uint32_t xid) override; */

private:
  std::unordered_map<Flow, std::vector<Ptr<Node>>, hash_fn> m_state;
  EnergyCalculator m_energy_calculator;

  Flow ExtractFlow (struct ofl_match *match);
  uint32_t ExtractInPort (struct ofl_match *match);

  void FlowMatching (std::stringstream &cmd, Flow flow);
  void AddRules (std::vector<Ptr<Node>> path, Flow flow);
};

} // namespace ns3

#endif
