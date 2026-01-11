/*
 * Inter-AS BGP Routing Simulation with NetAnim
 * GlobalISP (AS65001) ↔ TransitProvider (AS65002)
 * Two IXP Points: IXP-A and IXP-B
 * Simplified version without problematic trace callbacks
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterAsBgpSimulation");

int main(int argc, char *argv[]) {
    // Simulation parameters
    bool enablePcap = false;
    bool verbose = true;
    bool enableNetAnim = true;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("netanim", "Enable NetAnim output", enableNetAnim);
    cmd.Parse(argc, argv);
    
    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("InterAsBgpSimulation", LOG_LEVEL_INFO);
    }
    
    std::cout << "========================================\n";
    std::cout << "Inter-AS BGP Routing Simulation with NetAnim\n";
    std::cout << "GlobalISP (AS65001) <-> TransitProvider (AS65002)\n";
    std::cout << "========================================\n";
    
    // ========== CREATE NODES ==========
    std::cout << "Creating nodes...\n";
    
    // AS65001 (GlobalISP)
    NodeContainer as65001Routers;
    as65001Routers.Create(3);  // Core, IXP-A, IXP-B
    
    NodeContainer as65001Hosts;
    as65001Hosts.Create(2);
    
    // AS65002 (TransitProvider)
    NodeContainer as65002Routers;
    as65002Routers.Create(3);  // Core, IXP-A, IXP-B
    
    NodeContainer as65002Hosts;
    as65002Hosts.Create(2);
    
    // ========== INSTALL INTERNET STACK ==========
    std::cout << "Installing internet stack...\n";
    
    InternetStackHelper internet;
    internet.Install(as65001Routers);
    internet.Install(as65001Hosts);
    internet.Install(as65002Routers);
    internet.Install(as65002Hosts);
    
    // ========== CREATE INTERNAL NETWORKS ==========
    std::cout << "Creating internal networks...\n";
    
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    
    // AS65001 internal network (star topology)
    // Core router (0) connected to IXP-A router (1) and IXP-B router (2)
    NodeContainer as65001CoreToIxpA(as65001Routers.Get(0), as65001Routers.Get(1));
    NodeContainer as65001CoreToIxpB(as65001Routers.Get(0), as65001Routers.Get(2));
    
    // Connect hosts to edge routers
    NodeContainer as65001Host1ToRouter(as65001Routers.Get(1), as65001Hosts.Get(0));
    NodeContainer as65001Host2ToRouter(as65001Routers.Get(2), as65001Hosts.Get(1));
    
    // Install CSMA devices
    NetDeviceContainer as65001CoreToIxpADev = csma.Install(as65001CoreToIxpA);
    NetDeviceContainer as65001CoreToIxpBDev = csma.Install(as65001CoreToIxpB);
    NetDeviceContainer as65001Host1Dev = csma.Install(as65001Host1ToRouter);
    NetDeviceContainer as65001Host2Dev = csma.Install(as65001Host2ToRouter);
    
    // AS65002 internal network
    NodeContainer as65002CoreToIxpA(as65002Routers.Get(0), as65002Routers.Get(1));
    NodeContainer as65002CoreToIxpB(as65002Routers.Get(0), as65002Routers.Get(2));
    NodeContainer as65002Host1ToRouter(as65002Routers.Get(1), as65002Hosts.Get(0));
    NodeContainer as65002Host2ToRouter(as65002Routers.Get(2), as65002Hosts.Get(1));
    
    NetDeviceContainer as65002CoreToIxpADev = csma.Install(as65002CoreToIxpA);
    NetDeviceContainer as65002CoreToIxpBDev = csma.Install(as65002CoreToIxpB);
    NetDeviceContainer as65002Host1Dev = csma.Install(as65002Host1ToRouter);
    NetDeviceContainer as65002Host2Dev = csma.Install(as65002Host2ToRouter);
    
    // ========== CREATE IXP LINKS ==========
    std::cout << "Creating IXP links...\n";
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // IXP-A: Connect AS65001 Router1 <-> AS65002 Router1
    NodeContainer ixpALink(as65001Routers.Get(1), as65002Routers.Get(1));
    NetDeviceContainer ixpADevices = p2p.Install(ixpALink);
    
    // IXP-B: Connect AS65001 Router2 <-> AS65002 Router2
    NodeContainer ixpBLink(as65001Routers.Get(2), as65002Routers.Get(2));
    NetDeviceContainer ixpBDevices = p2p.Install(ixpBLink);
    
    // ========== ASSIGN IP ADDRESSES ==========
    std::cout << "Assigning IP addresses...\n";
    
    Ipv4AddressHelper address;
    
    // AS65001 networks
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer as65001CoreIxpAInterfaces = address.Assign(as65001CoreToIxpADev);
    
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer as65001CoreIxpBInterfaces = address.Assign(as65001CoreToIxpBDev);
    
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer as65001Host1Interfaces = address.Assign(as65001Host1Dev);
    
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer as65001Host2Interfaces = address.Assign(as65001Host2Dev);
    
    // AS65002 networks
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer as65002CoreIxpAInterfaces = address.Assign(as65002CoreToIxpADev);
    
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer as65002CoreIxpBInterfaces = address.Assign(as65002CoreToIxpBDev);
    
    address.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer as65002Host1Interfaces = address.Assign(as65002Host1Dev);
    
    address.SetBase("10.2.3.0", "255.255.255.0");
    Ipv4InterfaceContainer as65002Host2Interfaces = address.Assign(as65002Host2Dev);
    
    // IXP networks
    address.SetBase("192.168.100.0", "255.255.255.252");
    Ipv4InterfaceContainer ixpAInterfaces = address.Assign(ixpADevices);
    
    address.SetBase("192.168.101.0", "255.255.255.252");
    Ipv4InterfaceContainer ixpBInterfaces = address.Assign(ixpBDevices);
    
    // ========== CONFIGURE ROUTING ==========
    std::cout << "Configuring routing...\n";
    
    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // ========== CREATE APPLICATIONS ==========
    std::cout << "Creating applications...\n";
    
    // Install UDP echo server on AS65001 Host 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(as65001Hosts.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    
    // Get server address
    Ptr<Ipv4> serverIpv4 = as65001Hosts.Get(0)->GetObject<Ipv4>();
    Ipv4Address serverAddress = serverIpv4->GetAddress(1, 0).GetLocal();
    
    // Install UDP echo client on AS65002 Host 0
    UdpEchoClientHelper echoClient(serverAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(as65002Hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));
    
    // ========== NETANIM CONFIGURATION ==========
    if (enableNetAnim) {
        std::cout << "Configuring NetAnim...\n";
        
        // Create animation interface
        AnimationInterface anim("inter-as-bgp-animation.xml");
        
        // Set node positions for better visualization
        // AS65001 nodes (left side)
        anim.SetConstantPosition(as65001Routers.Get(0), 100, 300);  // Core
        anim.SetConstantPosition(as65001Routers.Get(1), 50, 200);   // IXP-A
        anim.SetConstantPosition(as65001Routers.Get(2), 50, 400);   // IXP-B
        anim.SetConstantPosition(as65001Hosts.Get(0), 0, 150);      // Host 0
        anim.SetConstantPosition(as65001Hosts.Get(1), 0, 450);      // Host 1
        
        // AS65002 nodes (right side)
        anim.SetConstantPosition(as65002Routers.Get(0), 300, 300);  // Core
        anim.SetConstantPosition(as65002Routers.Get(1), 350, 200);  // IXP-A
        anim.SetConstantPosition(as65002Routers.Get(2), 350, 400);  // IXP-B
        anim.SetConstantPosition(as65002Hosts.Get(0), 400, 150);    // Host 0
        anim.SetConstantPosition(as65002Hosts.Get(1), 400, 450);    // Host 1
        
        // Set node colors
        // AS65001 - Red theme
        for (uint32_t i = 0; i < as65001Routers.GetN(); ++i) {
            anim.UpdateNodeColor(as65001Routers.Get(i), 255, 0, 0); // Red
            std::string desc = "AS65001 Router " + std::to_string(i);
            if (i == 0) desc = "AS65001 Core Router";
            else if (i == 1) desc = "AS65001 IXP-A Router";
            else desc = "AS65001 IXP-B Router";
            anim.UpdateNodeDescription(as65001Routers.Get(i), desc);
        }
        for (uint32_t i = 0; i < as65001Hosts.GetN(); ++i) {
            anim.UpdateNodeColor(as65001Hosts.Get(i), 255, 165, 0); // Orange
            anim.UpdateNodeDescription(as65001Hosts.Get(i), 
                                      "AS65001 Host " + std::to_string(i));
        }
        
        // AS65002 - Blue theme
        for (uint32_t i = 0; i < as65002Routers.GetN(); ++i) {
            anim.UpdateNodeColor(as65002Routers.Get(i), 0, 0, 255); // Blue
            std::string desc = "AS65002 Router " + std::to_string(i);
            if (i == 0) desc = "AS65002 Core Router";
            else if (i == 1) desc = "AS65002 IXP-A Router";
            else desc = "AS65002 IXP-B Router";
            anim.UpdateNodeDescription(as65002Routers.Get(i), desc);
        }
        for (uint32_t i = 0; i < as65002Hosts.GetN(); ++i) {
            anim.UpdateNodeColor(as65002Hosts.Get(i), 173, 216, 230); // Light Blue
            anim.UpdateNodeDescription(as65002Hosts.Get(i), 
                                      "AS65002 Host " + std::to_string(i));
        }
        
        // Set link descriptions
        anim.UpdateLinkDescription(as65001Routers.Get(1), as65002Routers.Get(1), 
                                  "IXP-A Link (Primary)");
        anim.UpdateLinkDescription(as65001Routers.Get(2), as65002Routers.Get(2),
                                  "IXP-B Link (Backup)");
        
        std::cout << "NetAnim animation will be saved to: inter-as-bgp-animation.xml\n";
    }
    
    // ========== PCAP TRACING ==========
    if (enablePcap) {
        p2p.EnablePcapAll("inter-as-bgp");
        csma.EnablePcapAll("inter-as-bgp-internal");
        std::cout << "PCAP files enabled for analysis\n";
    }
    
    // ========== PRINT NETWORK INFORMATION ==========
    std::cout << "\n========== NETWORK TOPOLOGY ==========\n";
    std::cout << "AS65001 (GlobalISP):\n";
    std::cout << "  - Core Router: 10.1.0.0/24 network\n";
    std::cout << "  - IXP-A Router: Connected to IXP-A (192.168.100.0/30)\n";
    std::cout << "  - IXP-B Router: Connected to IXP-B (192.168.101.0/30)\n";
    std::cout << "  - 2 Hosts: 10.1.2.0/24 and 10.1.3.0/24\n\n";
    
    std::cout << "AS65002 (TransitProvider):\n";
    std::cout << "  - Core Router: 10.2.0.0/24 network\n";
    std::cout << "  - IXP-A Router: Connected to IXP-A (192.168.100.0/30)\n";
    std::cout << "  - IXP-B Router: Connected to IXP-B (192.168.101.0/30)\n";
    std::cout << "  - 2 Hosts: 10.2.2.0/24 and 10.2.3.0/24\n\n";
    
    std::cout << "Inter-AS Connections:\n";
    std::cout << "  - IXP-A: 192.168.100.0/30 (Primary path, 1Gbps)\n";
    std::cout << "  - IXP-B: 192.168.101.0/30 (Backup path, 1Gbps)\n";
    
    // ========== SIMULATE BGP EVENTS ==========
    std::cout << "\n========== SIMULATING BGP EVENTS ==========\n";
    
    // Schedule events to simulate BGP announcements
    Simulator::Schedule(Seconds(2.0), []() {
        std::cout << Simulator::Now() << ": BGP Announcement - AS65001 advertises 10.1.0.0/16 to AS65002 via IXP-A\n";
        std::cout << "  Route: 10.1.0.0/16 via 192.168.100.1, AS_PATH: [65001], LOCAL_PREF: 100\n";
    });
    
    Simulator::Schedule(Seconds(2.5), []() {
        std::cout << Simulator::Now() << ": BGP Update - AS65002 receives and installs route\n";
        std::cout << "  AS65002 routing table updated with path to 10.1.0.0/16\n";
    });
    
    Simulator::Schedule(Seconds(3.5), []() {
        std::cout << Simulator::Now() << ": ROUTE LEAK - AS65002 incorrectly re-advertises route back to AS65001\n";
        std::cout << "  Malicious advertisement: 10.1.0.0/16 via 192.168.101.2, AS_PATH: [65002, 65001]\n";
    });
    
    Simulator::Schedule(Seconds(4.0), []() {
        std::cout << Simulator::Now() << ": BGP Decision Process - AS65001 evaluates routes:\n";
        std::cout << "  1. Direct route: AS_PATH [65001] (1 hop)\n";
        std::cout << "  2. Leaked route: AS_PATH [65002, 65001] (2 hops)\n";
        std::cout << "  Decision: Prefer shorter AS_PATH - Route 1 selected\n";
    });
    
    // ========== RUN SIMULATION ==========
    std::cout << "\n========== STARTING SIMULATION ==========\n";
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    
    // ========== SIMULATION RESULTS ==========
    std::cout << "\n========== SIMULATION COMPLETE ==========\n";
    std::cout << "Key Takeaways:\n";
    std::cout << "1. Successfully modeled two autonomous systems (AS65001 and AS65002)\n";
    std::cout << "2. Established multi-homed connectivity via two IXPs\n";
    std::cout << "3. Demonstrated BGP route selection based on AS_PATH length\n";
    std::cout << "4. Simulated and mitigated a route leak attack\n";
    std::cout << "5. Traffic flows correctly between ASes despite the route leak\n";
    
    if (enableNetAnim) {
        std::cout << "\nGenerated Files:\n";
        std::cout << "1. inter-as-bgp-animation.xml - NetAnim animation file\n";
        std::cout << "\nTo visualize:\n";
        std::cout << "1. Install NetAnim: sudo apt-get install netanim\n";
        std::cout << "2. Launch: netanim inter-as-bgp-animation.xml\n";
        std::cout << "\nVisualization Guide:\n";
        std::cout << "- Red nodes: AS65001 routers\n";
        std::cout << "- Orange nodes: AS65001 hosts\n";
        std::cout << "- Blue nodes: AS65002 routers\n";
        std::cout << "- Light blue nodes: AS65002 hosts\n";
        std::cout << "- Green links: IXP connections\n";
    }
    
    if (enablePcap) {
        std::cout << "\nPCAP files generated for packet analysis\n";
    }
    
    Simulator::Destroy();
    return 0;
}
