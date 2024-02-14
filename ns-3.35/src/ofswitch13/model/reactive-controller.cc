#include "reactive-controller.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/ofswitch13-device.h"
#include "ns3/topology.h"
#include <boost/functional/hash.hpp>

NS_LOG_COMPONENT_DEFINE ("ReactiveController");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveController);

bool
Flow::operator== (const Flow &flow) const
{
  return src_ip == flow.src_ip && dst_ip == flow.dst_ip && ip_proto == flow.ip_proto &&
         src_port == flow.src_port && dst_port == flow.dst_port;
}

bool
Flow::operator< (const Flow &flow) const
{
  return src_ip < flow.src_ip && dst_ip < flow.dst_ip && ip_proto < flow.ip_proto &&
         src_port < flow.src_port && dst_port < flow.dst_port;
}

std::size_t
Flow::hash_fn::operator() (const Flow &flow) const
{
  std::size_t seed = 0;
  boost::hash_combine (seed, flow.src_ip.Get ());
  boost::hash_combine (seed, flow.dst_ip.Get ());
  boost::hash_combine (seed, flow.ip_proto);

  boost::hash_combine (seed, flow.src_port);
  boost::hash_combine (seed, flow.dst_port);

  return seed;
}

ReactiveController::ReactiveController () : m_state ()
{
  NS_LOG_FUNCTION (this);
}

ReactiveController::~ReactiveController ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
ReactiveController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReactiveController")
                          .SetParent<OFSwitch13Controller> ()
                          .SetGroupName ("OFSwitch13")
                          .AddConstructor<ReactiveController> ();
  return tid;
}

void
ReactiveController::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_state.clear ();
  OFSwitch13Controller::DoDispose ();
}

Flow
ReactiveController::ExtractFlow (struct ofl_match *match)
{
  Flow flow;
  struct ofl_match_tlv *tlv;

  uint32_t ip;
  tlv = oxm_match_lookup (OXM_OF_IPV4_SRC, match);
  memcpy (&ip, tlv->value, OXM_LENGTH (OXM_OF_IPV4_SRC));
  flow.src_ip = Ipv4Address (ntohl (ip));

  tlv = oxm_match_lookup (OXM_OF_IPV4_DST, match);
  memcpy (&ip, tlv->value, OXM_LENGTH (OXM_OF_IPV4_DST));
  flow.dst_ip = Ipv4Address (ntohl (ip));

  tlv = oxm_match_lookup (OXM_OF_IP_PROTO, match);
  memcpy (&flow.ip_proto, tlv->value, OXM_LENGTH (OXM_OF_IP_PROTO));

  switch (flow.ip_proto)
    {
    case IP_TYPE_TCP:
      tlv = oxm_match_lookup (OXM_OF_TCP_SRC, match);
      memcpy (&flow.src_port, tlv->value, OXM_LENGTH (OXM_OF_TCP_SRC));

      tlv = oxm_match_lookup (OXM_OF_TCP_DST, match);
      memcpy (&flow.dst_port, tlv->value, OXM_LENGTH (OXM_OF_TCP_DST));

      break;
    case IP_TYPE_UDP:
      tlv = oxm_match_lookup (OXM_OF_UDP_SRC, match);
      memcpy (&flow.src_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_SRC));

      tlv = oxm_match_lookup (OXM_OF_UDP_DST, match);
      memcpy (&flow.dst_port, tlv->value, OXM_LENGTH (OXM_OF_UDP_DST));

      break;
    default:
      break;
    }

  return flow;
}

uint32_t
ReactiveController::ExtractInPort (struct ofl_match *match)
{
  uint32_t in_port;

  struct ofl_match_tlv *tlv;
  tlv = oxm_match_lookup (OXM_OF_IN_PORT, match);
  memcpy (&in_port, tlv->value, OXM_LENGTH (OXM_OF_IN_PORT));

  return in_port;
}

void
ReactiveController::FlowMatching (std::stringstream &cmd, Flow flow)
{
  cmd << "ip_proto=" << flow.ip_proto << ",ip_src=" << flow.src_ip << ",ip_dst=" << flow.dst_ip;
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

void
ReactiveController::AddRules (std::vector<Ptr<Node>> path, Flow flow)
{
  for (auto rit = std::next (path.crbegin ()); rit != path.crend (); ++rit)
    {
      Ptr<OFSwitch13Device> of_dev = (*rit)->GetObject<OFSwitch13Device> ();
      uint64_t dpid = of_dev->GetDpId ();
      uint32_t out_port = of_dev->GetPortNoConnectedTo (*(rit - 1));

      std::stringstream cmd;

      // flags OFPFF_SEND_FLOW_REM OFPFF_RESET_COUNTS (0b0101 -> flags=0x0005)
      cmd << "flow-mod cmd=add,table=0,idle=60,flags=0x0005 eth_type=0x800,";
      FlowMatching (cmd, flow);
      cmd << " apply:output=" << out_port;
      NS_LOG_DEBUG ("[" << dpid << "]: " << cmd.str ());

      DpctlExecute (dpid, cmd.str ());
    }
}

std::vector<Ptr<Node>>
ReactiveController::CalculatePath (Flow flow)
{
  return Topology::DijkstraShortestPath (flow.src_ip, flow.dst_ip);
}

void
ReactiveController::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
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
          std::ostringstream cmd;

          Ipv4Address ip = (*it)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
          // flags OFPFF_RESET_COUNTS (0b0100 -> flags=0x0004)
          cmd << "flow-mod cmd=add,table=0,flags=0x0004 eth_type=0x800,ip_dst=" << ip
              << " apply:output=" << out_port;

          NS_LOG_DEBUG ("[" << dpid << "]: " << cmd.str ());

          DpctlExecute (dpid, cmd.str ());
        }
    }
}

ofl_err
ReactiveController::HandlePacketIn (struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
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

          std::vector<Ptr<Node>> path = CalculatePath (flow);
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
ReactiveController::HandleFlowRemoved (struct ofl_msg_flow_removed *msg,
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
