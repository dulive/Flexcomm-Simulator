#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/object-base.h"
#include "ns3/ofswitch13-device.h"
#include "ofl-packets.h"
#include "ofswitch13-device.h"
#include "openflow/openflow.h"
#include "oxm-match.h"
#include "packets.h"
#include "partial-path-controller.h"
#include <algorithm>
#include <boost/graph/properties.hpp>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <netinet/in.h>
#ifdef NS3_OFSWITCH13

NS_LOG_COMPONENT_DEFINE ("PartialPathController");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (PartialPathController);

/* ########## FlexWeight ########## */

FlexWeight::FlexWeight ()
{
  max_usage = 0.0;
  value = 0;
  hops = 0;
}

FlexWeight::FlexWeight (double max_usage, int value)
{
  this->max_usage = max_usage;
  this->value = value;
  this->hops = 0;
}

FlexWeight::FlexWeight (double max_usage, int value, int hops)
{
  this->max_usage = max_usage;
  this->value = value;
  this->hops = hops;
}

double
FlexWeight::GetMaxUsage () const
{
  return max_usage;
}

int
FlexWeight::GetValue () const
{
  return value;
}

int
FlexWeight::GetHops () const
{
  return hops;
}

FlexWeight
FlexWeight::operator+ (const FlexWeight &b) const
{
  return FlexWeight (std::max (max_usage, b.max_usage), value + b.value, hops + b.hops);
}

bool
FlexWeight::operator< (const FlexWeight &b) const
{
  if (max_usage >= 1 && b.max_usage < 1)
    return false;
  else if (max_usage < 1 && b.max_usage >= 1)
    return true;
  else
    return value == b.value ? hops == b.hops ? max_usage < b.max_usage : hops < b.hops
                            : value < b.value;
}

bool
FlexWeight::operator== (const FlexWeight &b) const
{
  return max_usage == b.max_usage && value == b.value && hops == b.hops;
}

/* ########## FlexWeightCalc ########## */

FlexWeightCalc::FlexWeightCalc (EnergyCalculator &calc) : weights ()
{
  CalculateWeights (calc);
}

void
FlexWeightCalc::CalculateWeights (EnergyCalculator &calc)
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
          uint64_t dpid = of_sw->GetDpId ();
          double flex = calc.GetRealFlex (dpid);
          weights.insert ({node_id, FlexWeight (of_sw->GetCpuUsage (), flex < 0 ? 1 : 0)});
        }
    }
}

FlexWeight
FlexWeightCalc::GetInitialWeight () const
{
  return FlexWeight{};
}

FlexWeight
FlexWeightCalc::GetNonViableWeight () const
{
  return FlexWeight (std::numeric_limits<double> ().max (), std::numeric_limits<int> ().max (),
                     std::numeric_limits<int> ().max ());
}

FlexWeight
FlexWeightCalc::GetWeight (Edge &e) const
{
  FlexWeight weight1 = weights.at (e.first);
  FlexWeight weight2 = weights.at (e.second);

  return FlexWeight (std::max (weight1.GetMaxUsage (), weight2.GetMaxUsage ()),
                     weight1.GetValue () + weight2.GetValue (), 1);
}

FlexWeight
FlexWeightCalc::GetWeight (uint32_t node) const
{
  return weights.at (node);
}

/* ########## PartialPathController ########## */

PartialPathController::PartialPathController () : m_state (), m_energy_calculator ()
{
  NS_LOG_FUNCTION (this);
}

PartialPathController::~PartialPathController ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
PartialPathController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PartialPathController")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<PartialPathController> ();
  return tid;
}
void

PartialPathController::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  OFSwitch13Controller::DoDispose ();
}

void
PartialPathController::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this);
  uint64_t dpid = swtch->GetDpId ();

  // table-miss rule
  DpctlExecute (dpid, "flow-mod cmd=add,table=0,prio=0 "
                      "apply:output=ctrl:0");

  // set to not send packet to controller
  DpctlExecute (dpid, "set-config miss=0");

  Ptr<OFSwitch13Device> of_device = OFSwitch13Device::GetDevice (dpid);
  NodeContainer hosts = NodeContainer::GetGlobalHosts ();

  for (auto it = hosts.Begin (); it != hosts.End (); ++it)
    {
      uint32_t out_port = of_device->GetPortNoConnectedTo (*it);
      if (out_port != -1)
        {
          Ipv4Address ip = (*it)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
          std::ostringstream cmd;
          // flags OFPFF_RESET_COUNTS (0b0100 -> flags=0x0004)
          cmd << "flow-mod cmd=add,table=0,flags=0x0004 eth_type=0x800,ip_dst=" << ip
              << " apply:output=" << out_port;

          NS_LOG_DEBUG ("[" << dpid << "]: " << cmd.str ());

          DpctlExecute (dpid, cmd.str ());
        }
    }
}

Flow
PartialPathController::ExtractFlow (struct ofl_match *match)
{
  Flow flow;
  struct ofl_match_tlv *tlv;
  tlv = oxm_match_lookup (OXM_OF_ETH_TYPE, match);
  memcpy (&flow.eth_type, tlv->value, OXM_LENGTH (OXM_OF_ETH_TYPE));

  flow.priority = 500;

  if (flow.eth_type == ETH_TYPE_IP)
    {
      uint32_t ip;
      tlv = oxm_match_lookup (OXM_OF_IPV4_SRC, match);
      memcpy (&ip, tlv->value, OXM_LENGTH (OXM_OF_IPV4_SRC));
      flow.src_ip = Ipv4Address (ntohl (ip));

      tlv = oxm_match_lookup (OXM_OF_IPV4_DST, match);
      memcpy (&ip, tlv->value, OXM_LENGTH (OXM_OF_IPV4_DST));
      flow.dst_ip = Ipv4Address (ntohl (ip));

      tlv = oxm_match_lookup (OXM_OF_IP_PROTO, match);
      memcpy (&flow.ip_proto, tlv->value, OXM_LENGTH (OXM_OF_IP_PROTO));

      flow.priority += 500;

      switch (flow.ip_proto)
        {
        case IP_TYPE_TCP:
          tlv = oxm_match_lookup (OXM_OF_TCP_SRC, match);
          memcpy (&flow.src_port, tlv->value, OXM_LENGTH (OXM_OF_TCP_SRC));

          tlv = oxm_match_lookup (OXM_OF_TCP_DST, match);
          memcpy (&flow.dst_port, tlv->value, OXM_LENGTH (OXM_OF_TCP_DST));

          flow.priority += 500;
          break;
        case IP_TYPE_UDP:
          tlv = oxm_match_lookup (OXM_OF_UDP_SRC, match);
          memcpy (&flow.src_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_SRC));

          tlv = oxm_match_lookup (OXM_OF_UDP_DST, match);
          memcpy (&flow.dst_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_DST));

          flow.priority += 500;
          break;
        default:
          break;
        }
    }
  else
    {
      flow.src_mac = Mac48Address ();
      tlv = oxm_match_lookup (OXM_OF_ETH_SRC, match);
      flow.src_mac.CopyFrom (tlv->value);

      flow.dst_mac = Mac48Address ();
      tlv = oxm_match_lookup (OXM_OF_ETH_DST, match);
      flow.dst_mac.CopyFrom (tlv->value);
    }

  return flow;
}

uint32_t
PartialPathController::ExtractInPort (struct ofl_match *match)
{
  uint32_t in_port;

  struct ofl_match_tlv *tlv;
  tlv = oxm_match_lookup (OXM_OF_IN_PORT, match);
  memcpy (&in_port, tlv->value, OXM_LENGTH (OXM_OF_IN_PORT));

  return in_port;
}

void
PartialPathController::FlowMatching (std::stringstream &cmd, Flow flow)
{
  cmd << "prio=" << flow.priority;
  if (flow.eth_type != ETH_TYPE_IP)
    {
      cmd << ",eth_dst=" << flow.dst_mac << ",eth_src=" << flow.src_mac
          << ",eth_type=" << flow.eth_type;
    }
  else
    {
      cmd << ",eth_type=" << flow.eth_type << ",ip_proto=" << flow.ip_proto
          << ",ip_src=" << flow.src_ip << ",ip_dst=" << flow.dst_ip;
      switch (flow.ip_proto)
        {
        case IP_TYPE_TCP:
          cmd << ",tcp_src=" << flow.src_port << ",tcp_dst=" << flow.dst_port;
          break;
        case IP_TYPE_UDP:
          cmd << ",udp_src=" << flow.src_port << ",udp_dst=" << flow.dst_port;
          break;
        default:
          break;
        }
    }
}

void
PartialPathController::AddRules (std::vector<Ptr<Node>> path, Flow flow)
{

  for (auto rit = std::next (path.crbegin ()); rit != path.crend (); ++rit)
    {
      Ptr<OFSwitch13Device> of_dev = (*rit)->GetObject<OFSwitch13Device> ();
      uint64_t dpid = of_dev->GetDpId ();
      uint32_t out_port = of_dev->GetPortNoConnectedTo (*(rit - 1));

      std::stringstream cmd;

      // flags OFPFF_SEND_FLOW_REM OFPFF_RESET_COUNTS (0b0101 -> flags=0x0005)
      cmd << "flow-mod cmd=add,table=0,idle=60,flags=0x0005,";
      FlowMatching (cmd, flow);
      cmd << " apply:output=" << out_port;
      NS_LOG_DEBUG ("[" << dpid << "]: " << cmd.str ());

      DpctlExecute (dpid, cmd.str ());
    }
}

ofl_err
PartialPathController::HandlePacketIn (struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
                                       uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  char *msgStr = ofl_structs_match_to_string ((struct ofl_match_header *) msg->match, 0);
  NS_LOG_DEBUG ("Packet in match: " << msgStr);
  free (msgStr);

  if (msg->reason == OFPR_NO_MATCH)
    {
      uint32_t out_port;

      Ptr<OFSwitch13Device> of_sw = OFSwitch13Device::GetDevice (swtch->GetDpId ());
      Flow flow = ExtractFlow ((struct ofl_match *) msg->match);
      uint32_t in_port = ExtractInPort ((struct ofl_match *) msg->match);

      auto it = m_state.find (flow);
      if (it == m_state.end ())
        {

          FlexWeightCalc weight_calc (m_energy_calculator);
          std::vector<Ptr<Node>> path;
          if (flow.eth_type == ETH_TYPE_IP)
            {
              path = Topology::DijkstraShortestPath<FlexWeight> (flow.src_ip, flow.dst_ip,
                                                                 weight_calc);
            }
          else
            {
              path = Topology::DijkstraShortestPath<FlexWeight> (flow.src_mac, flow.dst_mac,
                                                                 weight_calc);
            }
          path.pop_back ();
          path.erase (path.begin ());

          AddRules (path, flow);

          m_state[flow] = path;

          out_port = of_sw->GetPortNoConnectedTo (path[1]);
        }
      else
        {
          Ptr<Node> node = of_sw->GetObject<Node> ();
          std::vector<Ptr<Node>> path = (*it).second;
          auto path_it = std::find (path.cbegin (), path.cend (), node);
          out_port = of_sw->GetPortNoConnectedTo (*std::next (path_it));
        }

      struct ofl_msg_packet_out reply;
      reply.header.type = OFPT_PACKET_OUT;
      reply.buffer_id = msg->buffer_id;
      reply.in_port = in_port;
      reply.data_length = 0;
      reply.data = 0;

      struct ofl_action_output *a =
          (struct ofl_action_output *) xmalloc (sizeof (struct ofl_action_output));
      a->header.type = OFPAT_OUTPUT;
      a->port = out_port;
      a->max_len = 0;

      reply.actions_num = 1;
      reply.actions = (struct ofl_action_header **) &a;

      SendToSwitch (swtch, (struct ofl_msg_header *) &reply, xid);
    }

  ofl_msg_free ((struct ofl_msg_header *) msg, 0);
  return 0;
}

ofl_err
PartialPathController::HandleFlowRemoved (struct ofl_msg_flow_removed *msg,
                                          Ptr<const RemoteSwitch> swtch, uint32_t xid)
{
  NS_LOG_FUNCTION (this);
  if (msg->reason == OFPRR_IDLE_TIMEOUT)
    {
      char *msgStr = ofl_structs_match_to_string ((struct ofl_match_header *) msg->stats->match, 0);
      NS_LOG_DEBUG ("Packet in match: " << msgStr);
      free (msgStr);

      Ptr<Node> node = OFSwitch13Device::GetDevice (swtch->GetDpId ())->GetObject<Node> ();
      Flow flow = ExtractFlow ((struct ofl_match *) msg->stats->match);
      m_state.erase (flow);
    }
  ofl_msg_free_flow_removed (msg, true, 0);
  return 0;
}

} // namespace ns3

#endif
