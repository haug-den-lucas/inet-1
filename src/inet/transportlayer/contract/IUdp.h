//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IUDP_H
#define __INET_IUDP_H

#include "inet/networklayer/common/L3Address.h"

namespace inet {

class INET_API IUdp
{
  public:
    virtual void bind(int socketId, const L3Address& localAddr, int localPort) = 0;
    virtual void connect(int socketId, const L3Address& remoteAddr, int remotePort) = 0;
    virtual void joinMulticastGroups(int socketId, const std::vector<L3Address>& multicastAddresses, const std::vector<int> interfaceIds) = 0;
};

} // namespace inet

#endif

