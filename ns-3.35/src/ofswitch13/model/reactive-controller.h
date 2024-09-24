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

  void AddRule (uint64_t, uint32_t, Flow);

  virtual std::vector<Ptr<Node>> CalculatePath (Ptr<Node>, Ipv4Address);

  virtual void HandshakeSuccessful (Ptr<const RemoteSwitch>) override;

  virtual ofl_err HandlePacketIn (struct ofl_msg_packet_in *, Ptr<const RemoteSwitch>,
                                  uint32_t) override;

  /* virtual ofl_err HandlePortStatus (struct ofl_msg_port_status *, Ptr<const RemoteSwitch> , */
  /*                           uint32_t xid) override; */
};

} // namespace ns3

#endif // !REACTIVE_CONTROLLER_H
