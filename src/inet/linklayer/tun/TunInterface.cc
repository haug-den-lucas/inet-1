//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/common/socket/SocketTag_m.h"
#include "inet/linklayer/tun/ITun.h"
#include "inet/linklayer/tun/TunInterface.h"

namespace inet {

Define_Module(TunInterface);

cGate *TunInterface::lookupModuleInterface(cGate *gate, const std::type_info &type, const cObject *arguments, int direction)
{
    Enter_Method("lookupModuleInterface");
    EV_TRACE << "Looking up module interface" << EV_FIELD(gate) << EV_FIELD(type, opp_typename(type)) << EV_FIELD(arguments) << EV_FIELD(direction) << EV_ENDL;
    if (gate->isName("upperLayerIn")) {
        if (type == typeid(IPassivePacketSink)) {
            auto socketInd = dynamic_cast<const SocketInd *>(arguments);
            if (socketInd != nullptr)
                return findModuleInterface(gate, type, arguments, 1);
        }
        else if (type == typeid(ITun))
            return findModuleInterface(gate, type, arguments, 1);
    }
    return NetworkInterface::lookupModuleInterface(gate, type, arguments, direction);
}

} // namespace inet
