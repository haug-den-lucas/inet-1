//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_RFC5681RECOVERY_H
#define __INET_RFC5681RECOVERY_H

#include "inet/transportlayer/tcp/flavours/TcpTahoeRenoFamily.h"
#include "inet/transportlayer/tcp/TcpConnection.h"
#include "inet/transportlayer/tcp/ITcpRecovery.h"

namespace inet {
namespace tcp {

/**
 * Implements RFC 5681: TCP Congestion Control.
 */
class INET_API Rfc5681Recovery : public ITcpRecovery
{
  protected:
    TcpTahoeRenoFamilyStateVariables *state = nullptr;
    TcpConnection *conn = nullptr;

  public:
    Rfc5681Recovery(TcpStateVariables *state, TcpConnection *conn) : state(check_and_cast<TcpTahoeRenoFamilyStateVariables *>(state)), conn(conn) { }

    virtual bool isDuplicateAck(Packet *tcpSegment, TcpHeader *tcpHeader);

    virtual void receivedDataAck(uint32_t numBytesAcked) override;

    virtual void receivedDuplicateAck() override;
};

} // namespace tcp
} // namespace inet

#endif

