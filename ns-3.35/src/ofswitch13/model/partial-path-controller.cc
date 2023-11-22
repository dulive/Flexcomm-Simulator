#include "flex-puller.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/nstime.h"
#include "ns3/object-base.h"
#include "ns3/ofswitch13-device.h"
#include "ns3/topology.h"
#include "ofswitch13-device.h"
#include "partial-path-controller.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
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
FlexWeight::GetMaxUsage ()
{
  return max_usage;
}

int
FlexWeight::GetValue ()
{
  return value;
}

int
FlexWeight::GetHops ()
{
  return hops;
}

FlexWeight
FlexWeight::operator+ (const FlexWeight &b) const
{
  return FlexWeight (std::max (max_usage, b.max_usage), value + b.value, hops + b.hops + 1);
}

bool
FlexWeight::operator< (const FlexWeight &b) const
{
  if (max_usage >= 1 && b.max_usage < 1)
    return false;
  else if (max_usage < 1 && b.max_usage >= 1)
    return true;
  else
    return value == b.value ? (hops == b.hops ? max_usage < b.max_usage : hops < b.hops)
                            : value > b.value;
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
          weights[node_id] = GetInitialWeight ();
        }
      else if ((*it)->IsSwitch ())
        {
          Ptr<OFSwitch13Device> of_sw = (*it)->GetObject<OFSwitch13Device> ();
          uint64_t dpid = of_sw->GetDpId ();
          double flex = FlexPuller::GetRealFlex (dpid);
          weights[node_id] = FlexWeight (of_sw->GetCpuUsage (), flex < 0 ? -1 : 0);
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

  return weights.at (e.second);
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

Ipv4Address
PartialPathController::ExtractIpAddress (uint32_t oxm, struct ofl_match *match)
{
  uint32_t ip;
  int size = OXM_LENGTH (oxm);
  struct ofl_match_tlv *tlv = oxm_match_lookup (oxm, match);
  memcpy (&ip, tlv->value, size);
  return Ipv4Address (ntohl (ip));
}

flow_id_t
PartialPathController::ExtractFlow (struct ofl_match *match)
{
  Ipv4Address src_ip, dst_ip;
  src_ip = ExtractIpAddress (OXM_OF_IPV4_SRC, match);
  dst_ip = ExtractIpAddress (OXM_OF_IPV4_DST, match);

  struct ofl_match_tlv *tlv;
  uint16_t src_port, dst_port;
  tlv = oxm_match_lookup (OXM_OF_UDP_SRC, match);
  memcpy (&src_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_SRC));
  tlv = oxm_match_lookup (OXM_OF_UDP_DST, match);
  memcpy (&dst_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_DST));

  return flow_id_t (src_ip, src_port, dst_ip, dst_port);
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
PartialPathController::AddRules (std::vector<Ptr<Node>> path, flow_id_t flow)
{

  for (auto rit = std::next (path.crbegin ()); rit != path.crend (); ++rit)
    {
      Ptr<OFSwitch13Device> of_dev = (*rit)->GetObject<OFSwitch13Device> ();
      uint64_t dpid = of_dev->GetDpId ();
      uint32_t out_port = of_dev->GetPortNoConnectedTo (*(rit - 1));

      std::stringstream cmd;
      // flags OFPFF_SEND_FLOW_REM OFPFF_RESET_COUNTS (0b0101 -> flags=0x0005)
      cmd << "flow-mod cmd=add,table=0,idle=60,flags=0x0005,prio=1000"
          << " eth_type=0x800,ip_proto=17"
          << ",ip_src=" << get<0> (flow) << ",ip_dst=" << get<2> (flow)
          << ",udp_src=" << get<1> (flow) << ",udp_dst=" << get<3> (flow)
          << " apply:output=" << out_port;
      NS_LOG_DEBUG ("[" << dpid << "]: " << cmd.str ());

      DpctlExecute (dpid, cmd.str ());
    }
}

void
PartialPathController::DelRules (std::set<Ptr<Node>> del_nodes, flow_id_t flow)
{
  for (auto it = del_nodes.cbegin (); it != std::prev (del_nodes.cend ()); ++it)
    {
      Ptr<OFSwitch13Device> of_dev = (*it)->GetObject<OFSwitch13Device> ();
      uint64_t dpid = of_dev->GetDpId ();

      std::stringstream cmd;
      // flags OFPFF_SEND_FLOW_REM (0b0001 -> flags=0x0001)
      cmd << "flow-mod cmd=del,table=0,flags=0x0001"
          << " eth_type=0x800,ip_proto=17,ip_src=" << get<0> (flow) << ",ip_dst=" << get<2> (flow)
          << ",udp_src=" << get<1> (flow) << ",udp_dst=" << get<3> (flow);

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
      flow_id_t flow = ExtractFlow ((struct ofl_match *) msg->match);
      uint32_t in_port = ExtractInPort ((struct ofl_match *) msg->match);

      auto it = m_state.find (flow);
      if (it == m_state.end ())
        {

          FlexWeightCalc weight_calc;
          std::vector<Ptr<Node>> path = Topology::DijkstraShortestPath<FlexWeight> (
              std::get<0> (flow), std::get<2> (flow), weight_calc);
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
      flow_id_t flow = ExtractFlow ((struct ofl_match *) msg->stats->match);
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
      std::set<flow_id_t> flows_rm = kv.second;

      FlexWeight weight = weight_calc.GetWeight (node->GetId ());
      if (weight.GetValue () == -1 && flows_rm.size ())
        {
          for (flow_id_t flow : flows_rm)
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
