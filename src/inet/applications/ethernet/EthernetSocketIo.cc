//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/applications/ethernet/EthernetSocketIo.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet {

Define_Module(EthernetSocketIo);

void EthernetSocketIo::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        const char *protocolAsString = par("protocol");
        if (!opp_isempty(protocolAsString))
            protocol = Protocol::getProtocol(protocolAsString);
        trafficSink.reference(gate("trafficOut"), false);
        numSent = 0;
        numReceived = 0;
        WATCH(numSent);
        WATCH(numReceived);
    }
    else if (stage == INITSTAGE_NETWORK_INTERFACE_CONFIGURATION + 1) {
        setSocketOptions();
        socket.setOutputGate(gate("socketOut"));
        const char *localAddressString = par("localAddress");
        if (*localAddressString != '\0') {
            L3Address l3Address;
            L3AddressResolver addressResolver;
            addressResolver.tryResolve(localAddressString, l3Address, L3AddressResolver::ADDR_MAC);
            if (l3Address.getType() == L3Address::MAC)
                localAddress = l3Address.toMac();
            else
                localAddress = MacAddress(localAddressString);
        }
        const char *remoteAddressString = par("remoteAddress");
        if (*remoteAddressString != '\0') {
            L3Address l3Address;
            L3AddressResolver addressResolver;
            addressResolver.tryResolve(remoteAddressString, l3Address, L3AddressResolver::ADDR_MAC);
            if (l3Address.getType() == L3Address::MAC)
                remoteAddress = l3Address.toMac();
            else
                remoteAddress = MacAddress(remoteAddressString);
        }
    }
}

void EthernetSocketIo::handleMessageWhenUp(cMessage *message)
{
    if (socket.belongsToSocket(message))
        socket.processMessage(message);
    else {
        auto packet = check_and_cast<Packet *>(message);
        if (packet->findTag<PacketProtocolTag>() == nullptr) {
            auto packetProtocolTag = packet->addTag<PacketProtocolTag>();
            packetProtocolTag->setProtocol(&Protocol::unknown);
        }
        auto& macAddressReq = packet->addTag<MacAddressReq>();
        macAddressReq->setDestAddress(remoteAddress);
        socket.send(packet);
        numSent++;
        emit(packetSentSignal, packet);
    }
}

void EthernetSocketIo::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    ApplicationBase::finish();
}

void EthernetSocketIo::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();
    char buf[100];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void EthernetSocketIo::setSocketOptions()
{
    socket.setCallback(this);
    const char *interface = par("interface");
    if (interface[0] != '\0') {
        auto interfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        networkInterface = interfaceTable->findInterfaceByName(interface);
        if (networkInterface == nullptr)
            throw cRuntimeError("Cannot find network interface");
        if (!localAddress.isUnspecified())
            networkInterface->addMulticastMacAddress(localAddress);
        socket.setNetworkInterface(networkInterface);
    }
}

void EthernetSocketIo::socketDataArrived(EthernetSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);
    numReceived++;
    packet->removeTag<SocketInd>();
    trafficSink.pushPacket(packet);
}

void EthernetSocketIo::socketErrorArrived(EthernetSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring Ethernet error report " << indication->getName() << endl;
    delete indication;
}

void EthernetSocketIo::socketClosed(EthernetSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void EthernetSocketIo::pushPacket(Packet *packet, const cGate *gate)
{
    Enter_Method("pushPacket");
    take(packet);
    if (gate->isName("trafficIn")) {
        if (packet->findTag<PacketProtocolTag>() == nullptr) {
            auto packetProtocolTag = packet->addTag<PacketProtocolTag>();
            packetProtocolTag->setProtocol(&Protocol::unknown);
        }
        auto& macAddressReq = packet->addTag<MacAddressReq>();
        macAddressReq->setDestAddress(remoteAddress);
        emit(packetSentSignal, packet);
        socket.send(packet);
        numSent++;
    }
    else {
        if (socket.belongsToSocket(packet))
            socket.processMessage(packet);
    }
}

void EthernetSocketIo::handleStartOperation(LifecycleOperation *operation)
{
    if (!localAddress.isUnspecified())
        socket.bind(localAddress, remoteAddress, protocol, par("steal"));
}

void EthernetSocketIo::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void EthernetSocketIo::handleCrashOperation(LifecycleOperation *operation)
{
    socket.destroy();
}

cGate* EthernetSocketIo::lookupModuleInterface(cGate *gate, const std::type_info &type, const cObject *arguments, int direction)
{
    Enter_Method("lookupModuleInterface");
    EV_TRACE << "Looking up module interface" << EV_FIELD(gate) << EV_FIELD(type, opp_typename(type)) << EV_FIELD(arguments) << EV_FIELD(direction) << EV_ENDL;
    if (gate->isName("trafficIn")) {
        if (type == typeid(IPassivePacketSink))
            return gate;
    }
    else if (gate->isName("socketIn")) {
        if (type == typeid(IPassivePacketSink)) {
            auto socketInd = dynamic_cast<const SocketInd *>(arguments);
            if (socketInd != nullptr && socketInd->getSocketId() == socket.getSocketId())
                return gate;
        }
    }
    return nullptr;
}

} // namespace inet

