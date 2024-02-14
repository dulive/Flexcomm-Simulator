#ifndef REACTIVE_CONTROLLER_H
#define REACTIVE_CONTROLLER_H

#include "ofswitch13-controller.h"
namespace ns3 {

struct Flow
{
  Ipv4Address src_ip;
  Ipv4Address dst_ip;
  uint16_t ip_proto = 0;

  uint16_t src_port = 0;
  uint16_t dst_port = 0;

  bool operator== (const Flow &) const;

  bool operator< (const Flow &) const;

  struct hash_fn
  {
    std::size_t operator() (const Flow &flow) const;
  };
};

class ReactiveController : public OFSwitch13Controller
{
public:
  ReactiveController ();
  virtual ~ReactiveController ();

  static TypeId GetTypeId (void);
  virtual void DoDispose () override;

protected:
  Flow ExtractFlow (struct ofl_match *);
  uint32_t ExtractInPort (struct ofl_match *);

  void FlowMatching (std::stringstream &, Flow);

  void AddRules (std::vector<Ptr<Node>>, Flow);

  virtual std::vector<Ptr<Node>> CalculatePath (Flow);

  virtual void HandshakeSuccessful (Ptr<const RemoteSwitch>) override;

  virtual ofl_err HandlePacketIn (struct ofl_msg_packet_in *, Ptr<const RemoteSwitch>,
                                  uint32_t) override;

  virtual ofl_err HandleFlowRemoved (struct ofl_msg_flow_removed *, Ptr<const RemoteSwitch>,
                                     uint32_t) override;

  /* virtual ofl_err HandlePortStatus (struct ofl_msg_port_status *, Ptr<const RemoteSwitch> , */
  /*                           uint32_t xid) override; */

private:
  std::unordered_map<Flow, std::vector<Ptr<Node>>, Flow::hash_fn> m_state;
};

} // namespace ns3

#endif // !REACTIVE_CONTROLLER_H
