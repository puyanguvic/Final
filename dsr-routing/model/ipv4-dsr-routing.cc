// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <vector>
#include <iomanip>
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/network-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/boolean.h"
#include "ns3/node.h"
#include "ipv4-dsr-routing.h"
#include "dsr-route-manager.h"
#include "budget-tag.h"
#include "flag-tag.h"
#include "timestamp-tag.h"
#include "priority-tag.h"
#include "dist-tag.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-list-routing.h"
#include "dsr-virtual-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4DSRRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4DSRRouting);

TypeId 
Ipv4DSRRouting::GetTypeId (void)
{ 
  static TypeId tid = TypeId ("ns3::dsr-routing::Ipv4DSRRouting")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("Dsr-routing")
    .AddConstructor<Ipv4DSRRouting> ()
    .AddAttribute ("RandomEcmpRouting",
                   "Set to true if packets are randomly routed among ECMP; set to false for using only one route consistently",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Ipv4DSRRouting::m_randomEcmpRouting),
                   MakeBooleanChecker ())
    .AddAttribute ("RespondToInterfaceEvents",
                   "Set to true if you want to dynamically recompute the global routes upon Interface notification events (up/down, or add/remove address)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Ipv4DSRRouting::m_respondToInterfaceEvents),
                   MakeBooleanChecker ())
  ;
  return tid;
}

Ipv4DSRRouting::Ipv4DSRRouting () 
  : m_randomEcmpRouting (false),
    m_respondToInterfaceEvents (false)
{
  NS_LOG_FUNCTION (this);

  m_rand = CreateObject<UniformRandomVariable> ();
}

Ipv4DSRRouting::~Ipv4DSRRouting ()
{
  NS_LOG_FUNCTION (this);
}

void 
Ipv4DSRRouting::AddHostRouteTo (Ipv4Address dest, 
                                   Ipv4Address nextHop, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (this << dest << nextHop << interface);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  *route = Ipv4DSRRoutingTableEntry::CreateHostRouteTo (dest, nextHop, interface);
  m_hostRoutes.push_back (route);
}

void 
Ipv4DSRRouting::AddHostRouteTo (Ipv4Address dest, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (this << dest << interface);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  *route = Ipv4DSRRoutingTableEntry::CreateHostRouteTo (dest, interface);
  m_hostRoutes.push_back (route);
}

/**
  * \author Pu Yang
  * \brief Add a host route to the global routing table with the distance 
  * between root and destination
  * \param dest The Ipv4Address destination for this route.
  * \param nextHop The next hop Ipv4Address
  * \param interface The network interface index used to send packets to the
  *  destination
  * \param distance The distance between root and destination
 */
void
Ipv4DSRRouting::AddHostRouteTo (Ipv4Address dest,
                       Ipv4Address nextHop,
                       uint32_t interface,
                       uint32_t distance)
{
  NS_LOG_FUNCTION (this << dest << nextHop << interface << distance);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  // std::cout << "add host route with the distance = " << distance;
  *route = Ipv4DSRRoutingTableEntry::CreateHostRouteTo(dest, nextHop, interface, distance);
  m_hostRoutes.push_back (route);
}



void 
Ipv4DSRRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      Ipv4Address nextHop, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  *route = Ipv4DSRRoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_networkRoutes.push_back (route);
}

void 
Ipv4DSRRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  *route = Ipv4DSRRoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        interface);
  m_networkRoutes.push_back (route);
}

void 
Ipv4DSRRouting::AddASExternalRouteTo (Ipv4Address network, 
                                         Ipv4Mask networkMask,
                                         Ipv4Address nextHop,
                                         uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface);
  Ipv4DSRRoutingTableEntry *route = new Ipv4DSRRoutingTableEntry ();
  *route = Ipv4DSRRoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_ASexternalRoutes.push_back (route);
}


Ptr<Ipv4Route>
Ipv4DSRRouting::LookupDSRRoute (Ipv4Address dest, Ptr<NetDevice> oif)
{
  /**
   * Get the shortest path in the routing table
  */
  NS_LOG_FUNCTION (this << dest << oif);
  NS_LOG_LOGIC ("Looking for route for destination " << dest);
  Ptr<Ipv4Route> rtentry = 0;
  // store all available routes that bring packets to their destination
  typedef std::vector<Ipv4DSRRoutingTableEntry*> RouteVec_t;
  RouteVec_t allRoutes;

  NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
  for (HostRoutesCI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i++) 
    {
      NS_ASSERT ((*i)->IsHost ());
      if ((*i)->GetDest () == dest)
        {
          if (oif != 0)
            {
              if (oif != m_ipv4->GetNetDevice ((*i)->GetInterface ()))
                {
                  NS_LOG_LOGIC ("Not on requested interface, skipping");
                  continue;
                }
            }
          allRoutes.push_back (*i);
          NS_LOG_LOGIC (allRoutes.size () << "Found dsr host route" << *i); 
        }
    }
  if (allRoutes.size () > 0 ) // if route(s) is found
    {
      uint32_t routRef = 0;
      uint32_t shortestDist = allRoutes.at(0)->GetDistance ();
      for (uint32_t i = 0; i < allRoutes.size (); i ++)
      {
        if (allRoutes.at (i)->GetDistance () <  shortestDist)
        {
          routRef = i;
        }
      }
      Ipv4DSRRoutingTableEntry* route = allRoutes.at (routRef);

      // create a Ipv4Route object from the selected routing table entry
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (route->GetDest ());
      /// \todo handle multi-address case
      rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
      rtentry->SetGateway (route->GetGateway ());
      uint32_t interfaceIdx = route->GetInterface ();
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
      return rtentry;
    }
  else 
    {
      return 0;
    }
}

Ptr<Ipv4Route>
Ipv4DSRRouting::LookupDSRRoute (Ipv4Address dest, Ptr<Packet> p, Ptr<NetDevice> oif)
{
  /**
   * Lookup a Route to forward the DSR packets.
  */
  NS_LOG_FUNCTION (this << dest << oif);
  NS_LOG_LOGIC ("Looking for route for destination " << dest);
  Ptr<Ipv4Route> rtentry = 0;
  // store all available routes that bring packets to their destination
  typedef std::vector<Ipv4DSRRoutingTableEntry*> RouteVec_t;
  RouteVec_t allRoutes;

  NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
  for (HostRoutesCI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i++) 
    {
      NS_ASSERT ((*i)->IsHost ());
      if ((*i)->GetDest () == dest)
        {
          if (oif != 0)
            {
              if (oif != m_ipv4->GetNetDevice ((*i)->GetInterface ()))
                {
                  NS_LOG_LOGIC ("Not on requested interface, skipping");
                  continue;
                }
            }
          allRoutes.push_back (*i);
          NS_LOG_LOGIC (allRoutes.size () << "Found dsr host route" << *i << " with Cost: " << (*i)->GetDistance ()); 
        }
    }
  if (allRoutes.size () > 0 ) // if route(s) is found
    {
      FlagTag flagTag;
      BudgetTag budgetTag;
      TimestampTag timestampTag;
      if (!(p->PeekPacketTag (flagTag) 
            && p->PeekPacketTag (budgetTag) 
            && p->PeekPacketTag (timestampTag)))
      {
        NS_LOG_INFO ("No Tags in the packet");
        return 0;
      }
      // Time out drop
      if (budgetTag.GetBudget () + timestampTag.GetMicroSeconds () < Simulator::Now().GetMicroSeconds ())
      {
        NS_LOG_INFO ("TIMEOUT DROP !!!");
        return 0;
      }
      uint32_t budget = budgetTag.GetBudget () + timestampTag.GetMicroSeconds () - Simulator::Now().GetMicroSeconds (); // in Microseconds
      // std::cout << "print budget : " << budget << std::endl;
      DistTag distTag;
      RouteVec_t cRouts;
      // p->PrintPacketTags (std::cout);
      if (p->PeekPacketTag (distTag))
      {
        uint32_t dist = distTag.GetDistance ();
        // std::cout << dist << std::endl;
        budget = (budget < dist)? budget : dist;
      }

      for (uint32_t i = 0; i < allRoutes.size (); i ++)
      {
        if (allRoutes.at (i)->GetDistance () < budget)
        {
          // check queue status in current node
          // Get the netdevice
          Ptr <NetDevice> dev = m_ipv4->GetNetDevice (allRoutes.at (i)->GetInterface ());
          //get the queue disc on the device
          Ptr<QueueDisc> disc = m_ipv4->GetNetDevice (0)->GetNode ()->GetObject<TrafficControlLayer> ()->GetRootQueueDiscOnDevice (dev);
          Ptr<DsrVirtualQueueDisc> i_dvq = DynamicCast <DsrVirtualQueueDisc> (disc);
          uint32_t i_fastSize = i_dvq->GetInternalQueue (0) ->GetCurrentSize ().GetValue ();
          uint32_t i_slowSize = i_dvq->GetInternalQueue (1) ->GetCurrentSize ().GetValue ();
          if (i_fastSize >= (i_dvq->GetInternalQueue (0)->GetMaxSize ().GetValue ()-1) 
              || (i_slowSize >= i_dvq->GetInternalQueue (1)->GetMaxSize ().GetValue ()-1))
            {
              continue;
            }
          
          Ptr<Channel> channel = dev->GetChannel ();
          PointToPointChannel *p2pchannel = dynamic_cast <PointToPointChannel *> (PeekPointer (channel));
          if (p2pchannel != 0)
          {
            Ptr<PointToPointNetDevice> d_Dev = p2pchannel->GetDestination (1);
            Ptr<Node> d_node = d_Dev->GetNode ();
            // find the out put route
            Ptr<Ipv4> d_ipv4 = d_node->GetObject<Ipv4> ();
            if (d_ipv4 != 0 && d_ipv4->GetRoutingProtocol () != 0)
            {
              Ptr<Ipv4Route> route;
              if (d_ipv4->GetRoutingProtocol () != 0)
              {
                Ptr<Ipv4RoutingProtocol> routingProtocol = d_ipv4->GetRoutingProtocol ();
                Ipv4ListRouting *listRoutingProtocol = dynamic_cast <Ipv4ListRouting *> (PeekPointer (routingProtocol));
                if (listRoutingProtocol != 0)
                {
                  int16_t tempprio;
                  int16_t  *ptr = &tempprio;
                  Ptr <Ipv4RoutingProtocol> rpt = listRoutingProtocol->GetRoutingProtocol (0, *ptr);
                  // dynamic_cast routing protocol to Ipv4DSRRouting
                  Ipv4DSRRouting *dsrRoutingProtocol = dynamic_cast <Ipv4DSRRouting *> (PeekPointer (rpt));
                  if (dsrRoutingProtocol != 0)
                  {
                    route = dsrRoutingProtocol->LookupDSRRoute (dest, oif);
                    if (route != 0)
                    {
                      Ptr<NetDevice> o_dev = route->GetOutputDevice ();
                      Ptr<QueueDisc> o_disc = o_dev->GetNode ()->GetObject<TrafficControlLayer> ()->GetRootQueueDiscOnDevice (o_dev);
                      Ptr<DsrVirtualQueueDisc> o_dvq = DynamicCast <DsrVirtualQueueDisc> (o_disc);
                      // std::cout << "queue disc name : " << o_dvq->GetTypeId ().GetName () << std::endl;
                      if (o_dvq != 0)
                      {
                        uint32_t o_fastSize = o_dvq->GetInternalQueue (0) ->GetCurrentSize ().GetValue ();
                        uint32_t o_slowSize = o_dvq->GetInternalQueue (1) ->GetCurrentSize ().GetValue ();
                        // std::cout << "current size : " << o_dvq->GetInternalQueue (0)->GetMaxSize ().GetValue () << std::endl;
                        // uint32_t o_normalSize = o_dvq->GetInternalQueue (2) ->GetCurrentSize ().GetValue ();
                        cRouts.push_back (allRoutes.at (i));
                        if (o_fastSize < (o_dvq->GetInternalQueue (0)->GetMaxSize ().GetValue ()-1) || o_slowSize < (o_dvq->GetInternalQueue (1)->GetMaxSize ().GetValue ()-1))
                        {
                          cRouts.push_back (allRoutes.at (i));
                        }
                        else
                        {
                          std::cout << "route overloaded" << std::endl;
                        }
                      }
                      else
                      {
                        NS_LOG_INFO ("Could not find the DSRVirtualQueue, drop this route.");
                      }
                    }
                  }
                  
                }
              }
            }
          }
        }
      }
      if (cRouts.size () > 0)
      {
        uint32_t routRef = 0;
        uint32_t shortestDist = cRouts.at(0)->GetDistance ();
        for (uint32_t i = 0; i < cRouts.size (); i ++)
        {
          if (cRouts.at (i)->GetDistance () <  shortestDist)
          {
            routRef = i;
          }
        }
        Ipv4DSRRoutingTableEntry* route = cRouts.at (routRef);
        DistTag distTag;
        p->PeekPacketTag (distTag);
        distTag.SetDistance (route->GetDistance ());
        p->ReplacePacketTag (distTag);

        PriorityTag priorityTag;
        p->PeekPacketTag (priorityTag);
        uint32_t bgt = budgetTag.GetBudget () + timestampTag.GetMicroSeconds () - Simulator::Now().GetMicroSeconds ();
        if (bgt > (route->GetDistance () + 10))
          {
            priorityTag.SetPriority (1);
          }
        else
          {
            priorityTag.SetPriority (0);
          }
        p->ReplacePacketTag (priorityTag);
        // create a Ipv4Route object from the selected routing table entry
        rtentry = Create<Ipv4Route> ();
        rtentry->SetDestination (route->GetDest ());
        rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
        rtentry->SetGateway (route->GetGateway ());
        uint32_t interfaceIdx = route->GetInterface ();
        rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
        return rtentry;
      }
      else
      {
        NS_LOG_INFO ("No Route available");
        return 0;
      }
    }
  else 
    {
      return 0;
    }
}

uint32_t 
Ipv4DSRRouting::GetNRoutes (void) const
{
  NS_LOG_FUNCTION (this);
  uint32_t n = 0;
  n += m_hostRoutes.size ();
  n += m_networkRoutes.size ();
  n += m_ASexternalRoutes.size ();
  return n;
}

Ipv4DSRRoutingTableEntry *
Ipv4DSRRouting::GetRoute (uint32_t index) const
{
  NS_LOG_FUNCTION (this << index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesCI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              return *i;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  if (index < m_networkRoutes.size ())
    {
      for (NetworkRoutesCI j = m_networkRoutes.begin (); 
           j != m_networkRoutes.end ();
           j++)
        {
          if (tmp == index)
            {
              return *j;
            }
          tmp++;
        }
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesCI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end (); 
       k++) 
    {
      if (tmp == index)
        {
          return *k;
        }
      tmp++;
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}
void 
Ipv4DSRRouting::RemoveRoute (uint32_t index)
{
  NS_LOG_FUNCTION (this << index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_hostRoutes.size ());
              delete *i;
              m_hostRoutes.erase (i);
              NS_LOG_LOGIC ("Done removing host route " << index << "; host route remaining size = " << m_hostRoutes.size ());
              return;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j++) 
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_networkRoutes.size ());
          delete *j;
          m_networkRoutes.erase (j);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());
          return;
        }
      tmp++;
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end ();
       k++)
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_ASexternalRoutes.size ());
          delete *k;
          m_ASexternalRoutes.erase (k);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());
          return;
        }
      tmp++;
    }
  NS_ASSERT (false);
}

int64_t
Ipv4DSRRouting::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}

void
Ipv4DSRRouting::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  for (HostRoutesI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i = m_hostRoutes.erase (i)) 
    {
      delete (*i);
    }
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j = m_networkRoutes.erase (j)) 
    {
      delete (*j);
    }
  for (ASExternalRoutesI l = m_ASexternalRoutes.begin (); 
       l != m_ASexternalRoutes.end ();
       l = m_ASexternalRoutes.erase (l))
    {
      delete (*l);
    }

  Ipv4RoutingProtocol::DoDispose ();
}

// Formatted like output of "route -n" command
void
Ipv4DSRRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  NS_LOG_FUNCTION (this << stream);
  std::ostream* os = stream->GetStream ();

  *os << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
      << ", Time: " << Now().As (unit)
      << ", Local time: " << m_ipv4->GetObject<Node> ()->GetLocalTime ().As (unit)
      << ", Ipv4DSRRouting table" << std::endl;

  if (GetNRoutes () > 0)
    {
      *os << "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface" << std::endl;
      for (uint32_t j = 0; j < GetNRoutes (); j++)
        {
          /**
           * \author Pu Yang
           * \brief print the metric in routing table
          */
          std::ostringstream dest, gw, mask, flags, metric;
          Ipv4DSRRoutingTableEntry route = GetRoute (j);
          dest << route.GetDest ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << dest.str ();
          gw << route.GetGateway ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << gw.str ();
          mask << route.GetDestNetworkMask ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << mask.str ();
          flags << "U";
          if (route.IsHost ())
            {
              flags << "H";
            }
          else if (route.IsGateway ())
            {
              flags << "G";
            }
          *os << std::setiosflags (std::ios::left) << std::setw (6) << flags.str ();
          // // Metric not implemented
          // *os << "-" << "      ";
          metric << route.GetDistance ();
          *os << std::setiosflags (std::ios::left) <<std::setw(16) << metric.str();
          
          // Ref ct not implemented
          *os << "-" << "      ";
          // Use not implemented
          *os << "-" << "   ";
          if (Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ())) != "")
            {
              *os << Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ()));
            }
          else
            {
              *os << route.GetInterface ();
            }
          *os << std::endl;
        }
    }
  *os << std::endl;
}



Ptr<Ipv4Route>
Ipv4DSRRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << p << &header << oif << &sockerr);
//
// First, see if this is a multicast packet we have a route for.  If we
// have a route, then send the packet down each of the specified interfaces.
//
  if (header.GetDestination ().IsMulticast ())
    {
      NS_LOG_LOGIC ("Multicast destination-- returning false");
      return 0; // Let other routing protocols try to handle this
    }
//
// See if this is a unicast packet we have a route for.
//
  NS_LOG_LOGIC ("Unicast destination- looking up");
  Ptr<Ipv4Route> rtentry;
  BudgetTag bugetTag;
  if (p != nullptr && p->GetSize () != 0 && p->PeekPacketTag (bugetTag) && bugetTag.GetBudget () != 0)
    { 
      rtentry = LookupDSRRoute (header.GetDestination (), p, oif);
    }
  else
  {
    rtentry = LookupDSRRoute (header.GetDestination (), oif);
  }
  if (rtentry)
    {
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}



bool 
Ipv4DSRRouting::RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                                UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                LocalDeliverCallback lcb, ErrorCallback ecb)
{ 
  Ptr <Packet> p_copy = p->Copy();
  NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev << &lcb << &ecb);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  if (m_ipv4->IsDestinationAddress (header.GetDestination (), iif))
    {
      if (!lcb.IsNull ())
        {
          NS_LOG_LOGIC ("Local delivery to " << header.GetDestination ());
          // std::cout << "Local delivery to " << header.GetDestination () << std::endl;
          lcb (p, header, iif);
          return true;
        }
      else
        {
          // The local delivery callback is null.  This may be a multicast
          // or broadcast packet, so return false so that another
          // multicast routing protocol can handle it.  It should be possible
          // to extend this to explicitly check whether it is a unicast
          // packet, and invoke the error callback if so
          // std::cout << "ERROR !!!!" << std::endl;
          return false;
        }
    }
  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      // std::cout << "RI: Forwarding disabled for this interface" << std::endl;
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }
  // Next, try to find a route
  NS_LOG_LOGIC ("Unicast destination- looking up global route");
  Ptr<Ipv4Route> rtentry; 
  BudgetTag budgetTag;
  
  if (p->PeekPacketTag (budgetTag) && budgetTag.GetBudget () != 0)
  {
    rtentry = LookupDSRRoute (header.GetDestination (), p_copy); 
  }
  else
  {
    rtentry = LookupDSRRoute (header.GetDestination ());
  }
  if (rtentry != 0)
    {
      const Ptr <Packet> p_c = p_copy->Copy();
      NS_LOG_LOGIC ("Found unicast destination- calling unicast callback");
      ucb (rtentry, p_c, header);
      return true; 
    }
  else
    {
      NS_LOG_LOGIC ("Did not find unicast destination- returning false");
      return false; // Let other routing protocols try to handle this
                    // route request.
    }
}

void 
Ipv4DSRRouting::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      DSRRouteManager::DeleteDSRRoutes ();
      DSRRouteManager::BuildDSRRoutingDatabase ();
      DSRRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4DSRRouting::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      DSRRouteManager::DeleteDSRRoutes ();
      DSRRouteManager::BuildDSRRoutingDatabase ();
      DSRRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4DSRRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      DSRRouteManager::DeleteDSRRoutes ();
      DSRRouteManager::BuildDSRRoutingDatabase ();
      DSRRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4DSRRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      DSRRouteManager::DeleteDSRRoutes ();
      DSRRouteManager::BuildDSRRoutingDatabase ();
      DSRRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4DSRRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  m_ipv4 = ipv4;
}

} // namespace ns3
