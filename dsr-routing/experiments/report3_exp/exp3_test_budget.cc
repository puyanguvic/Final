/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */


/*
Author: Xiangyu Ren
*/
// Experiment3: Test impact of delay requirements under TCP traffic
//
// Network topology
//
//
//                      
//    n0 --m3-- n3 --h3-- n6 
//    |h5       |m5       |m5
//    n1 --h3-- n4 --h3-- n7
//    |m3       |h5       |h3
//    n2 --h3-- n5 --m5-- n8
// 
//  h - high capacity link (200Mbps) 
//  m - low capacity link (100Mbps)
//  3 - Base delay = 3ms
//  5 - Base delay = 5ms
// 
// - all links are point-to-point links with indicated one-way BW/delay
// - TCP packet size of 1024 bytes, TCP-CUBIC is enabled
// - DropTail queues 



#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <sys/stat.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-dsr-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dsr-routing-module.h"

using namespace ns3;  

// ============== Auxiliary functions ===============

// Function to check queue length of Routers
std::string dir = "results/Exp3/DSR/";
std::string ExpName = "DsrExperiment3";

void InstallDSRUdpApplication (Ptr<Node> node, Address sinkAddress, double startTime, double stopTime,
                        uint32_t packetSize, uint32_t nPacket, uint32_t budget, 
                        uint32_t dataRate, bool flag)
{
  Ptr<Socket> dsrSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  Ptr<DsrUdpApplication> dsrApp = CreateObject<DsrUdpApplication> ();
  dsrApp->Setup (dsrSocket, sinkAddress, packetSize, nPacket, DataRate(std::to_string(dataRate) + "Mbps"), budget, flag);
  node->AddApplication (dsrApp);
  dsrApp->SetStartTime (Seconds (startTime));
  dsrApp->SetStopTime (Seconds (stopTime));
}

NS_LOG_COMPONENT_DEFINE (ExpName);

int 
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
#if 0 
  LogComponentEnable (ExpName, LOG_LEVEL_INFO);
#endif

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mb/s"));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // ------------------ build topology ---------------------------
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create(9);
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer n0n3 = NodeContainer(nodes.Get(0), nodes.Get(3));
  NodeContainer n3n4 = NodeContainer(nodes.Get(3), nodes.Get(4));
  NodeContainer n1n4 = NodeContainer(nodes.Get(1), nodes.Get(4));
  NodeContainer n4n5 = NodeContainer(nodes.Get(4), nodes.Get(5));
  NodeContainer n2n5 = NodeContainer(nodes.Get(2), nodes.Get(5));
  NodeContainer n3n6 = NodeContainer(nodes.Get(3), nodes.Get(6));
  NodeContainer n6n7 = NodeContainer(nodes.Get(6), nodes.Get(7));
  NodeContainer n4n7 = NodeContainer(nodes.Get(4), nodes.Get(7));
  NodeContainer n7n8 = NodeContainer(nodes.Get(7), nodes.Get(8));
  NodeContainer n5n8 = NodeContainer(nodes.Get(5), nodes.Get(8));

  // ------------------- install internet protocol to nodes-----------
  NS_LOG_INFO ("Enabling DSR Routing.");
  Ipv4DSRRoutingHelper dsr;

  // Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  // list.Add (staticRouting, 0);
  list.Add (dsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (nodes);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p; 
  // PointToPointHelper p2p2; 
  // PointToPointHelper p2p3; 
  // PointToPointHelper p2p4; 

  std::string channelDataRate1 = "10Mbps"; // link data rate (High)
  std::string channelDataRate2 = "5Mbps"; // link data rate (low)
  uint32_t delayInMicro1 = 3000; // transmission + propagation delay (Short)
  uint32_t delayInMicro2 = 5000; // transmission + propagation delay (Long)
  double StopTime = 10.0; // Simulation stop time
  std::vector<NetDeviceContainer> devices(12);

  // High capacity link + Short base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro1)));
  devices[10] = p2p.Install (n2n5);
  devices[5] = p2p.Install (n1n4);
  devices[9] = p2p.Install (n7n8);
  devices[1] = p2p.Install (n3n6);
  devices[6] = p2p.Install (n4n7);

  // High capacity link + Long base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro2)));
  devices[2] = p2p.Install (n0n1);
  devices[8] = p2p.Install (n4n5);


  // Low capacity link + Short base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate2)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro1)));
  devices[0] = p2p.Install (n0n3);
  devices[7] = p2p.Install (n1n2);



  // Low capacity link + Long base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate2)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro2)));
  devices[4] = p2p.Install (n6n7);
  devices[3] = p2p.Install (n3n4);
  devices[11] = p2p.Install (n5n8);

  // ------------------- install dsr-queue -----------------------------
  TrafficControlHelper tch1;

  tch1.SetRootQueueDisc ("ns3::DsrVirtualQueueDisc");

  
  std::vector<QueueDiscContainer> qdisc(12);
  for (int i = 0; i < 12; i ++)
  {
    // tch.Uninstall (devices[i]);
    qdisc[i] = tch1.Install (devices[i]);
  }

  // ------------------- IP addresses AND Link Metric ----------------------
  uint16_t Metric1 = 7000;
  uint16_t Metric2 = 6000;
  uint16_t Metric3 = 5000;
  uint16_t Metric4 = 4000;
  
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (devices[2]);
  i0i1.SetMetric (0, Metric2);
  i0i1.SetMetric (1, Metric2);

  ipv4.SetBase ("10.1.11.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = ipv4.Assign (devices[0]);
  i0i3.SetMetric (0, Metric2);
  i0i3.SetMetric (1, Metric2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (devices[7]);
  i1i2.SetMetric (0, Metric3);
  i1i2.SetMetric (1, Metric3);

  ipv4.SetBase ("10.1.12.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i4 = ipv4.Assign (devices[5]);
  i1i4.SetMetric (0, Metric4);
  i1i4.SetMetric (1, Metric4);

  ipv4.SetBase ("10.1.13.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = ipv4.Assign (devices[10]);
  i2i5.SetMetric (0, Metric4);
  i2i5.SetMetric (1, Metric4);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign (devices[3]);
  i3i4.SetMetric (0, Metric1);
  i3i4.SetMetric (1, Metric1);

  ipv4.SetBase ("10.1.14.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i6 = ipv4.Assign (devices[1]);
  i3i6.SetMetric (0, Metric4);
  i3i6.SetMetric (1, Metric4);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = ipv4.Assign (devices[8]);
  i4i5.SetMetric (0, Metric2);
  i4i5.SetMetric (1, Metric2);

  ipv4.SetBase ("10.1.15.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i7 = ipv4.Assign (devices[6]);
  i4i7.SetMetric (0, Metric4);
  i4i7.SetMetric (1, Metric4);

  ipv4.SetBase ("10.1.16.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i8 = ipv4.Assign (devices[11]); 
  i5i8.SetMetric (0, Metric1);
  i5i8.SetMetric (1, Metric1);

  ipv4.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i6i7 = ipv4.Assign (devices[4]);
  i6i7.SetMetric (0, Metric1);
  i6i7.SetMetric (1, Metric1);

  ipv4.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer i7i8 = ipv4.Assign (devices[9]);
  i7i8.SetMetric (0, Metric4);
  i7i8.SetMetric (1, Metric4);

  // ------------------------------------*** DSR Traffic Target Flow (n2 --> n6)-------------------------------------  
   //Create a dsrSink applications 
  uint16_t sinkPort = 9;
  Address sinkAddress (InetSocketAddress (i6i7.GetAddress (0), sinkPort));
  DsrSinkHelper dsrSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = dsrSinkHelper.Install (nodes.Get (6));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (20.));

  // create a dsrSender application
  // uint32_t PacketSize = 1024;
  // for test
  uint32_t PacketSize = 52;
  uint32_t NPacket = 1000;
  uint32_t budget = 30;
  // InstallDGPacketSend (nodes.Get(2), sinkAddress, BeginTime, StopTime, PacketSize, NPacket, budget, 2, true);
  for (int i=1; i<11; i++)
  {
    InstallDSRUdpApplication (nodes.Get(2), sinkAddress, (i-1)*0.5, i*0.5, PacketSize, NPacket, budget, 2, true);
  }

  // // ---------------------------------------Network Traffic C2 (n1-->n7) ------------------
  //Create a dsrSink applications 
  Address sinkAddress_2 (InetSocketAddress (i7i8.GetAddress (0), sinkPort));
  DsrSinkHelper dsrSinkHelper_2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps_2 = dsrSinkHelper_2.Install (nodes.Get (7));
  sinkApps_2.Start (Seconds (0.0));
  sinkApps_2.Stop (Seconds (20.));
  // // ---------------------------------------Network Traffic C2 (n2-->n8) ------------------
  //Create a dsrSink applications 
  Address sinkAddress_3 (InetSocketAddress (i7i8.GetAddress (1), sinkPort));
  DsrSinkHelper dsrSinkHelper_3 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps_3 = dsrSinkHelper_3.Install (nodes.Get (8));
  sinkApps_3.Start (Seconds (0.0));
  sinkApps_3.Stop (Seconds (20.));

  // create a dsrSender DG application
  uint32_t NPacket1 = 10000;
  uint32_t budget1 = 20;
   for (int i=1; i<11; i++)
  {
    InstallDSRUdpApplication (nodes.Get (0), sinkAddress, (i-1)*0.5, i*0.5, PacketSize, NPacket1, budget1, 1 + i*0.5, false);
    // InstallBEPacketSend (nodes.Get(0), sinkAddress, (i-1)*0.5, i*0.5, PacketSize, NPacket1, 1 + i*0.5, false);
    InstallDSRUdpApplication (nodes.Get(1), sinkAddress_2, (i-1)*0.5, i*0.5, PacketSize, NPacket1, budget1, 1 + i*0.5, false);
    // InstallDGPacketSend (nodes.Get(1), sinkAddress_2, (i-1)*0.5, i*0.5, PacketSize, NPacket1, budget1, 1 + i*0.5, false);
    InstallDSRUdpApplication (nodes.Get(2), sinkAddress_3, (i-1)*0.5, i*0.5, PacketSize, NPacket1, budget1, 1 + i*0.5, false);
    // InstallDGPacketSend (nodes.Get(2), sinkAddress_3, (i-1)*0.5, i*0.5, PacketSize, NPacket1, budget1, 1 + i*0.5, false);
  }
  

  // // create a dsrSender BE application
  // for (int i=1; i<11; i++)
  // {
  //   InstallBEPacketSend (nodes.Get(0), sinkAddress, (i-1)*0.2, i*0.2, PacketSize, NPacket1, i*0.5, false);
  //   InstallBEPacketSend (nodes.Get(1), sinkAddress_2, (i-1)*0.2, i*0.2, PacketSize, NPacket1, i*0.5, false);
  //   InstallBEPacketSend (nodes.Get(2), sinkAddress_3, (i-1)*0.2, i*0.2, PacketSize, NPacket1, i*0.5, false);
  // }


  // // ------------------------------------------------------------------


  // ---------------- Net Anim ---------------------
  AnimationInterface anim(ExpName + ".xml");
  anim.SetConstantPosition (nodes.Get(0), 0.0, 100.0);
  anim.SetConstantPosition (nodes.Get(1), 0.0, 130.0);
  anim.SetConstantPosition (nodes.Get(2), 0.0, 160.0);
  anim.SetConstantPosition (nodes.Get(3), 30.0, 100.0);
  anim.SetConstantPosition (nodes.Get(4), 30.0, 130.0);
  anim.SetConstantPosition (nodes.Get(5), 30.0, 160.0);
  anim.SetConstantPosition (nodes.Get(6), 60.0, 100.0);
  anim.SetConstantPosition (nodes.Get(7), 60.0, 130.0);
  anim.SetConstantPosition (nodes.Get(8), 60.0, 160.0);

  // -------------- Print the routing table ----------------
  Ipv4DSRRoutingHelper d;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
  (ExpName + ".routes", std::ios::out);
  d.PrintRoutingTableAllAt (Seconds (0), routingStream);

  // --------------- Store Recorded .dat --------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (StopTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}