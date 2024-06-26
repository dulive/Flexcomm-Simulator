/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 *         Vitor M. Eichemberger <vitor.marge@gmail.com>
 *
 * Two hosts connected to different OpenFlow switches.
 * Each switch is managed by an independent default learning controller application.
 *
 *            Learning Controller   Learning Controller
 *                    |                     |
 *               +----------+          +----------+
 *    Host 0 === | Switch 0 | ======== | Switch 1 | === Host 1
 *               +----------+          +----------+
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint16_t simTime = 10;
  bool verbose = false;
  bool trace = false;

  // Configure command line parameters
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      OFSwitch13Helper::EnableDatapathLogs ();
      LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Create two host nodes
  NodeContainer hosts;
  hosts.Create (2);

  // Create two switch nodes
  NodeContainer switches;
  switches.Create (2);

  // Use the CsmaHelper to connect hosts and switches
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NodeContainer pair;
  NetDeviceContainer pairDevs;
  NetDeviceContainer hostDevices;
  NetDeviceContainer switchPorts[2];
  switchPorts[0] = NetDeviceContainer ();
  switchPorts[1] = NetDeviceContainer ();

  // Connect host 0 to first switch
  pair = NodeContainer (hosts.Get (0), switches.Get (0));
  pairDevs = csmaHelper.Install (pair);
  hostDevices.Add (pairDevs.Get (0));
  switchPorts[0].Add (pairDevs.Get (1));

  // Connect host 1 to second switch
  pair = NodeContainer (hosts.Get (1), switches.Get (1));
  pairDevs = csmaHelper.Install (pair);
  hostDevices.Add (pairDevs.Get (0));
  switchPorts[1].Add (pairDevs.Get (1));

  // Connect the switches
  pair = NodeContainer (switches.Get (0), switches.Get (1));
  pairDevs = csmaHelper.Install (pair);
  switchPorts[0].Add (pairDevs.Get (0));
  switchPorts[1].Add (pairDevs.Get (1));

  // Create two controller nodes
  NodeContainer controllers;
  controllers.Create (2);

  // Configure both OpenFlow network domains
  Ptr<OFSwitch13InternalHelper> of13Helper0 = CreateObject<OFSwitch13InternalHelper> ();
  of13Helper0->InstallController (controllers.Get (0));
  of13Helper0->InstallSwitch (switches.Get (0), switchPorts[0]);
  of13Helper0->CreateOpenFlowChannels ();

  Ptr<OFSwitch13InternalHelper> of13Helper1 = CreateObject<OFSwitch13InternalHelper> ();
  of13Helper1->InstallController (controllers.Get (1));
  of13Helper1->InstallSwitch (switches.Get (1), switchPorts[1]);
  of13Helper1->CreateOpenFlowChannels ();

  // Install the TCP/IP stack into hosts nodes
  InternetStackHelper internet;
  internet.Install (hosts);

  // Set IPv4 host addresses
  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer hostIpIfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
  hostIpIfaces = ipv4helpr.Assign (hostDevices);

  // Configure ping application between hosts
  V4PingHelper pingHelper = V4PingHelper (hostIpIfaces.GetAddress (1));
  pingHelper.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer pingApps = pingHelper.Install (hosts.Get (0));
  pingApps.Start (Seconds (1));

  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  if (trace)
    {
      of13Helper0->EnableOpenFlowPcap ("openflow-0");
      of13Helper0->EnableDatapathStats ("switch-stats");
      of13Helper1->EnableOpenFlowPcap ("openflow-1");
      of13Helper1->EnableDatapathStats ("switch-stats");
      csmaHelper.EnablePcap ("switch", switchPorts[0], true);
      csmaHelper.EnablePcap ("switch", switchPorts[1], true);
      csmaHelper.EnablePcap ("host", hostDevices);
    }

  // Run the simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}
