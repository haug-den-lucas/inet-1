//
// Copyright (C) 2022 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IPV4MODULAR_H
#define __INET_IPV4MODULAR_H

#include "inet/networklayer/contract/INetworkProtocol.h"
#include "inet/networklayer/ipv4modular/INetfilterCompatibleIpv4HookManagerBase.h"

namespace inet {

class INET_API Ipv4Modular : public cModule, public INetfilterCompatibleIpv4HookManagerBase, public INetworkProtocol
{
  protected:
    opp_component_ptr<IIpv4HookManager> hookManager;

  protected:
    void chkHookManager();

  public:
    // cModule:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    // IIpv4HookManager:
    virtual void registerNetfilterHandler(Ipv4Hook::NetfilterType type, int priority, Ipv4Hook::NetfilterHandler *handler) override;
    virtual void unregisterNetfilterHandler(Ipv4Hook::NetfilterType type, int priority, Ipv4Hook::NetfilterHandler *handler) override;
    virtual void reinjectDatagram(Packet *datagram, Ipv4Hook::NetfilterResult action) override;
};

} // namespace inet

#endif
