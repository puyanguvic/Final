/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "dist-tag.h"

namespace ns3 {

//----------------------------------------------------------------------
//-- DistTag
//------------------------------------------------------
TypeId
DistTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("DistTag")
    .SetParent<Tag> ()
    .AddConstructor<DistTag> ()
    .AddAttribute ("Distance",
                   "The budget time in millisecond",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&DistTag::GetDistance),
                   MakeUintegerChecker <uint32_t> ())
  ;
  return tid;
}

TypeId
DistTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
DistTag::GetSerializedSize (void) const
{
  return 4;     // 4 bytes
}

void
DistTag::Serialize (TagBuffer i) const
{
  uint32_t t = m_distance;
  i.Write ((const uint8_t *)&t, 4);
}

void
DistTag::Deserialize (TagBuffer i)
{
  uint32_t t;
  i.Read ((uint8_t *)&t, 4);
  m_distance = t;
}

void
DistTag::SetDistance (uint32_t distance)
{
  m_distance = distance;
}

uint32_t
DistTag::GetDistance (void) const
{
  return m_distance;
}

void
DistTag::Print (std::ostream &os) const
{
  os << "Distance = " << m_distance;
}

}
