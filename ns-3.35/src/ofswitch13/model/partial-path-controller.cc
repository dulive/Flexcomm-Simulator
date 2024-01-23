#include "flex-puller.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/nstime.h"
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

FlexWeightCalc::FlexWeightCalc () : unwanted (), weights ()
{
  CalculateWeights ();
}

FlexWeightCalc::FlexWeightCalc (std::set<uint32_t> unwanted) : unwanted (unwanted), weights ()
{
  CalculateWeights ();
}

void
FlexWeightCalc::CalculateWeights ()
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
          double flex = FlexPuller::GetRealFlex (dpid);
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
  if (unwanted.find (e.second) != unwanted.end ())
    return GetNonViableWeight ();

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

void
FlexWeightCalc::SetUnwanted (std::set<uint32_t> unwanted)
{
  this->unwanted = unwanted;
}

/* ########## PartialPathController ########## */

PartialPathController::PartialPathController () : m_state (), m_sw_rm_apps ()
{
  NS_LOG_FUNCTION (this);
  Simulator::Schedule (Minutes (3), &PartialPathController::Balancer, this);
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

  FlexPuller::InnitNode (dpid);
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

void
PartialPathController::DelRules (std::set<Ptr<Node>> del_nodes, Flow flow)
{
  for (auto it = del_nodes.cbegin (); it != std::prev (del_nodes.cend ()); ++it)
    {
      Ptr<OFSwitch13Device> of_dev = (*it)->GetObject<OFSwitch13Device> ();
      uint64_t dpid = of_dev->GetDpId ();

      std::stringstream cmd;
      // flags OFPFF_SEND_FLOW_REM (0b0001 -> flags=0x0001)
      cmd << "flow-mod cmd=del,table=0,flags=0x0001,";
      FlowMatching (cmd, flow);

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

          FlexWeightCalc weight_calc;
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

          for (auto it = std::next (path.cbegin ()); it != std::prev (path.cend ()); ++it)
            {
              m_sw_rm_apps[*it].insert (flow);
            }
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
      m_sw_rm_apps[node].erase (flow);
      m_state.erase (flow);
    }
  ofl_msg_free_flow_removed (msg, true, 0);
  return 0;
}

void
PartialPathController::Balancer (void)
{
  NS_LOG_FUNCTION (this);

  FlexPuller::UpdateReadings ();
  FlexWeightCalc weight_calc;
  auto respects_flex = [&weight_calc] (Ptr<Node> node) {
    FlexWeight weight = weight_calc.GetWeight (node->GetId ());
    return weight.GetValue () != -1;
  };

  for (const auto &kv : m_sw_rm_apps)
    {
      Ptr<Node> node = kv.first;
      std::set<Flow> flows_rm = kv.second;

      FlexWeight weight = weight_calc.GetWeight (node->GetId ());
      if (weight.GetValue () == -1 && flows_rm.size ())
        {
          for (Flow flow : flows_rm)
            {
              std::vector<Ptr<Node>> old_path = m_state.at (flow);
              auto path_it = std::find (old_path.crbegin (), old_path.crend (), node);

              auto src_it =
                  std::find_if (std::next (path_it), std::prev (old_path.crend ()), respects_flex);
              auto dst_it =
                  std::find_if (path_it.base (), std::prev (old_path.cend ()), respects_flex);

              std::set<uint32_t> unwanted;
              std::transform (std::next (src_it), old_path.crend (),
                              std::inserter (unwanted, unwanted.begin ()),
                              [] (Ptr<Node> node) { return node->GetId (); });
              std::transform (std::next (dst_it), old_path.cend (),
                              std::inserter (unwanted, unwanted.begin ()),
                              [] (Ptr<Node> node) { return node->GetId (); });

              weight_calc.SetUnwanted (unwanted);
              std::vector<Ptr<Node>> new_path =
                  Topology::DijkstraShortestPath (*src_it, *dst_it, weight_calc);

              std::vector<Ptr<Node>> old_part (std::prev (src_it.base ()), std::next (dst_it));
              if (new_path == old_part)
                {
                  continue;
                }

              AddRules (new_path, flow);
              for (const Ptr<Node> &node : new_path)
                m_sw_rm_apps[node].insert (flow);

              std::set<Ptr<Node>> old_part_set (old_part.cbegin (), old_part.cend ());
              std::set<Ptr<Node>> new_path_set (new_path.cbegin (), new_path.cend ());
              std::set<Ptr<Node>> intersection;

              std::set_difference (old_part_set.cbegin (), old_part_set.cend (),
                                   new_path_set.cbegin (), new_path_set.cend (),
                                   std::inserter (intersection, intersection.begin ()));

              DelRules (intersection, flow);
              for (Ptr<Node> node : intersection)
                m_sw_rm_apps[node].erase (flow);

              std::size_t size = old_path.size () - old_part.size () + new_path.size ();

              std::vector<Ptr<Node>> complete_path;
              complete_path.reserve (size);
              complete_path.insert (complete_path.end (), old_path.cbegin (),
                                    std::prev (src_it.base ()));
              complete_path.insert (complete_path.end (), new_path.cbegin (), new_path.cend ());
              complete_path.insert (complete_path.end (), std::next (dst_it), old_path.cend ());

              m_state[flow] = complete_path;
            }
        }
    }
  Simulator::Schedule (Minutes (3), &PartialPathController::Balancer, this);
}

} // namespace ns3

#endif
