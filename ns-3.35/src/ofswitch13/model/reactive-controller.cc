#include "reactive-controller.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/ofswitch13-device.h"
#include "ns3/topology.h"
#include <boost/functional/hash.hpp>

NS_LOG_COMPONENT_DEFINE ("ReactiveController");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ReactiveController);

ReactiveController::ReactiveController ()
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
ReactiveController::AddRule (uint64_t dpId, uint32_t port, Flow flow)
{
  std::stringstream cmd;

  // flags OFPFF_SEND_FLOW_REM (0b0001 -> flags=0x0001)
  cmd << "flow-mod cmd=add,table=0,hard=10,flags=0x0001 eth_type=0x800,";
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
  cmd << " apply:output=" << port;
  NS_LOG_DEBUG ("[" << dpId << "]: " << cmd.str ());

  DpctlExecute (dpId, cmd.str ());
}

std::vector<Ptr<Node>>
ReactiveController::CalculatePath (Ptr<Node> src_node, Ipv4Address dst_ip)
{
  return Topology::DijkstraShortestPath (src_node, dst_ip);
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
}

ofl_err
ReactiveController::HandlePacketIn (struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
                                    uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  char *msgStr = ofl_structs_match_to_string ((struct ofl_match_header *) msg->match, 0);
  NS_LOG_DEBUG ("Packet in match: " << msgStr);
  free (msgStr);

  Ptr<OFSwitch13Device> of_sw = OFSwitch13Device::GetDevice (swtch->GetDpId ());
  Ptr<Node> node = of_sw->GetObject<Node> ();
  Flow flow = ExtractFlow ((struct ofl_match *) msg->match);

  std::vector<Ptr<Node>> path = CalculatePath (node, flow.dst_ip);
  uint32_t out_port = of_sw->GetPortNoConnectedTo (path[1]);

  AddRule (swtch->GetDpId (), out_port, flow);

  uint32_t in_port = ExtractInPort ((struct ofl_match *) msg->match);
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

  ofl_msg_free ((struct ofl_msg_header *) msg, 0);
  return 0;
}

} // namespace ns3
