#ifndef PARTIAL_PATH_CONTROLLER_H
#define PARTIAL_PATH_CONTROLLER_H

#include "ns3/ofswitch13-controller.h"
#include "ns3/topology.h"
#include <cstdint>
#include <unordered_map>

namespace ns3 {

using flow_id_t = std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t>;

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

  double GetMaxUsage ();
  int GetValue ();
  int GetHops ();

  FlexWeight operator+ (const FlexWeight &b) const;
  bool operator< (const FlexWeight &b) const;
  bool operator== (const FlexWeight &b) const;
};

class FlexWeightCalc : public WeightCalc<FlexWeight>
{
private:
  std::set<uint32_t> unwanted;
  std::unordered_map<uint32_t, FlexWeight> weights;

  void CalculateWeights (void);

public:
  FlexWeightCalc ();
  FlexWeightCalc (std::set<uint32_t>);

  FlexWeight GetInitialWeight () const override;
  FlexWeight GetNonViableWeight () const override;
  FlexWeight GetWeight (Edge &) const override;
  FlexWeight GetWeight (uint32_t node) const;

  void SetUnwanted (std::set<uint32_t>);
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
  std::map<flow_id_t, std::vector<Ptr<Node>>> m_state;
  std::map<Ptr<Node>, std::set<flow_id_t>> m_sw_rm_apps;

  Ipv4Address ExtractIpAddress (uint32_t oxm, struct ofl_match *match);
  flow_id_t ExtractFlow (struct ofl_match *match);
  uint32_t ExtractInPort (struct ofl_match *match);

  void AddRules (std::vector<Ptr<Node>> path, flow_id_t flow);
  void DelRules (std::set<Ptr<Node>> del_nodes, flow_id_t flow);

  void Balancer (void);
};

} // namespace ns3

#endif
