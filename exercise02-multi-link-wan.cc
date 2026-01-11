/*
 * Triangular WAN topology with redundant paths and static routing
 *
 * Network Topology (Full Mesh):
 *
 *           HQ (n0)
 *            /   \
 *           /     \
 *  10.1.1.0/24  10.1.2.0/24
 *         /         \
 *        /           \
 *  Branch (n1)---10.1.3.0/24---DC (n2)
 *
 * - HQ (n0): Connects to both Branch and DC
 * - Branch (n1): Connects to both HQ and DC
 * - DC (n2): Connects to both HQ and Branch
 * - All links: 5Mbps, 2ms
 * - Static routes configured for redundant paths
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TriangularWAN");

// Helper function to disable an interface on a node
void DisableInterface(Ptr<Node> node, uint32_t interfaceIndex)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4)
    {
        ipv4->SetDown(interfaceIndex);
        std::cout << "Interface " << interfaceIndex << " on node " << node->GetId() 
                  << " disabled at t=" << Simulator::Now().GetSeconds() << "s\n";
    }
}

int
main(int argc, char* argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: HQ, Branch, and Data Center
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0); // HQ
    Ptr<Node> n1 = nodes.Get(1); // Branch
    Ptr<Node> n2 = nodes.Get(2); // Data Center

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create THREE links (full mesh triangle)
    // Link 1: HQ (n0) <-> Branch (n1)
    NodeContainer linkHQ_Branch(n0, n1);
    NetDeviceContainer devicesHQ_Branch = p2p.Install(linkHQ_Branch);

    // Link 2: HQ (n0) <-> DC (n2)
    NodeContainer linkHQ_DC(n0, n2);
    NetDeviceContainer devicesHQ_DC = p2p.Install(linkHQ_DC);

    // Link 3: Branch (n1) <-> DC (n2)
    NodeContainer linkBranch_DC(n1, n2);
    NetDeviceContainer devicesBranch_DC = p2p.Install(linkBranch_DC);

    // Install mobility model to keep nodes at fixed positions
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set the positions for each node (triangular layout)
    Ptr<MobilityModel> mob0 = n0->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = n1->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob2 = n2->GetObject<MobilityModel>();

    mob0->SetPosition(Vector(10.0, 20.0, 0.0));  // HQ (top)
    mob1->SetPosition(Vector(5.0, 10.0, 0.0));   // Branch (bottom-left)
    mob2->SetPosition(Vector(15.0, 10.0, 0.0));  // DC (bottom-right)

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to all three networks
    // Network 1: HQ-Branch (10.1.1.0/24)
    Ipv4AddressHelper addressHQ_Branch;
    addressHQ_Branch.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesHQ_Branch = addressHQ_Branch.Assign(devicesHQ_Branch);
    // HQ: 10.1.1.1, Branch: 10.1.1.2

    // Network 2: HQ-DC (10.1.2.0/24)
    Ipv4AddressHelper addressHQ_DC;
    addressHQ_DC.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesHQ_DC = addressHQ_DC.Assign(devicesHQ_DC);
    // HQ: 10.1.2.1, DC: 10.1.2.2

    // Network 3: Branch-DC (10.1.3.0/24)
    Ipv4AddressHelper addressBranch_DC;
    addressBranch_DC.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBranch_DC = addressBranch_DC.Assign(devicesBranch_DC);
    // Branch: 10.1.3.1, DC: 10.1.3.2

    // *** Configure Static Routing ***

    // Enable IP forwarding on all nodes (all are routers in this topology)
    nodes.Get(0)->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    nodes.Get(1)->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    nodes.Get(2)->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));

    // Get static routing protocol helper
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // Configure routing on HQ (n0)
    Ptr<Ipv4StaticRouting> staticRoutingHQ = 
        staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv4>());
    
    // Route to Branch-DC network via Branch (primary path to 10.1.3.0/24)
    staticRoutingHQ->AddNetworkRouteTo(
        Ipv4Address("10.1.3.0"),   // Destination network
        Ipv4Mask("255.255.255.0"), // Network mask
        Ipv4Address("10.1.1.2"),   // Next hop: Branch (10.1.1.2)
        1,                         // Interface index to HQ-Branch link
        0                          // Metric (lower = higher priority)
    );
    // Alternative route via DC (backup path to 10.1.3.0/24)
    staticRoutingHQ->AddNetworkRouteTo(
        Ipv4Address("10.1.3.0"),
        Ipv4Mask("255.255.255.0"),
        Ipv4Address("10.1.2.2"),   // Next hop: DC (10.1.2.2)
        2,                         // Interface index to HQ-DC link
        10                         // Higher metric (lower priority)
    );

    // Configure routing on Branch (n1)
    Ptr<Ipv4StaticRouting> staticRoutingBranch = 
        staticRoutingHelper.GetStaticRouting(n1->GetObject<Ipv4>());
    
    // Route to HQ-DC network via HQ (primary path to 10.1.2.0/24)
    staticRoutingBranch->AddNetworkRouteTo(
        Ipv4Address("10.1.2.0"),
        Ipv4Mask("255.255.255.0"),
        Ipv4Address("10.1.1.1"),   // Next hop: HQ (10.1.1.1)
        1,                         // Interface index to HQ-Branch link
        0                          // Metric
    );
    // Alternative route via DC (backup path to 10.1.2.0/24)
    staticRoutingBranch->AddNetworkRouteTo(
        Ipv4Address("10.1.2.0"),
        Ipv4Mask("255.255.255.0"),
        Ipv4Address("10.1.3.2"),   // Next hop: DC (10.1.3.2)
        2,                         // Interface index to Branch-DC link
        10                         // Higher metric
    );

    // Configure routing on DC (n2)
    Ptr<Ipv4StaticRouting> staticRoutingDC = 
        staticRoutingHelper.GetStaticRouting(n2->GetObject<Ipv4>());
    
    // Route to HQ-Branch network via HQ (primary path to 10.1.1.0/24)
    staticRoutingDC->AddNetworkRouteTo(
        Ipv4Address("10.1.1.0"),
        Ipv4Mask("255.255.255.0"),
        Ipv4Address("10.1.2.1"),   // Next hop: HQ (10.1.2.1)
        1,                         // Interface index to HQ-DC link
        0                          // Metric
    );
    // Alternative route via Branch (backup path to 10.1.1.0/24)
    staticRoutingDC->AddNetworkRouteTo(
        Ipv4Address("10.1.1.0"),
        Ipv4Mask("255.255.255.0"),
        Ipv4Address("10.1.3.1"),   // Next hop: Branch (10.1.3.1)
        2,                         // Interface index to Branch-DC link
        10                         // Higher metric
    );

    // Print routing tables for verification
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>("scratch/triangular-wan.routes", std::ios::out);
    staticRoutingHelper.PrintRoutingTableAllAt(Seconds(1.0), routingStream);

    std::cout << "\n=== Network Configuration ===\n";
    std::cout << "HQ (n0):\n";
    std::cout << "  Interface 1 (to Branch): " << interfacesHQ_Branch.GetAddress(0) << "\n";
    std::cout << "  Interface 2 (to DC): " << interfacesHQ_DC.GetAddress(0) << "\n";
    std::cout << "\nBranch (n1):\n";
    std::cout << "  Interface 1 (to HQ): " << interfacesHQ_Branch.GetAddress(1) << "\n";
    std::cout << "  Interface 2 (to DC): " << interfacesBranch_DC.GetAddress(0) << "\n";
    std::cout << "\nDC (n2):\n";
    std::cout << "  Interface 1 (to HQ): " << interfacesHQ_DC.GetAddress(1) << "\n";
    std::cout << "  Interface 2 (to Branch): " << interfacesBranch_DC.GetAddress(1) << "\n";
    std::cout << "=============================\n\n";

    // Create UDP Echo Servers on ALL THREE nodes
    uint16_t port = 9;
    
    // Server on HQ
    UdpEchoServerHelper echoServerHQ(port);
    ApplicationContainer serverAppsHQ = echoServerHQ.Install(n0);
    serverAppsHQ.Start(Seconds(1.0));
    serverAppsHQ.Stop(Seconds(15.0));
    
    // Server on Branch
    UdpEchoServerHelper echoServerBranch(port);
    ApplicationContainer serverAppsBranch = echoServerBranch.Install(n1);
    serverAppsBranch.Start(Seconds(1.0));
    serverAppsBranch.Stop(Seconds(15.0));
    
    // Server on DC
    UdpEchoServerHelper echoServerDC(port);
    ApplicationContainer serverAppsDC = echoServerDC.Install(n2);
    serverAppsDC.Start(Seconds(1.0));
    serverAppsDC.Stop(Seconds(15.0));

    // Create UDP Echo Clients for communication between ALL PAIRS
    // Client 1: HQ -> DC (tests primary HQ-DC link)
    UdpEchoClientHelper echoClientHQtoDC(interfacesHQ_DC.GetAddress(1), port); // DC's IP on HQ-DC network
    echoClientHQtoDC.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientHQtoDC.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientHQtoDC.SetAttribute("PacketSize", UintegerValue(1024));

    // Client 2: HQ -> Branch (tests HQ-Branch link)
    UdpEchoClientHelper echoClientHQtoBranch(interfacesHQ_Branch.GetAddress(1), port); // Branch's IP on HQ-Branch network
    echoClientHQtoBranch.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientHQtoBranch.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientHQtoBranch.SetAttribute("PacketSize", UintegerValue(1024));

    // Client 3: Branch -> DC (tests Branch-DC link)
    UdpEchoClientHelper echoClientBranchtoDC(interfacesBranch_DC.GetAddress(1), port); // DC's IP on Branch-DC network
    echoClientBranchtoDC.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientBranchtoDC.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientBranchtoDC.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    // HQ sends to DC
    ApplicationContainer clientAppsHQtoDC = echoClientHQtoDC.Install(n0);
    clientAppsHQtoDC.Start(Seconds(2.0));
    clientAppsHQtoDC.Stop(Seconds(15.0));
    
    // HQ sends to Branch
    ApplicationContainer clientAppsHQtoBranch = echoClientHQtoBranch.Install(n0);
    clientAppsHQtoBranch.Start(Seconds(2.1)); // Slightly offset
    clientAppsHQtoBranch.Stop(Seconds(15.0));
    
    // Branch sends to DC
    ApplicationContainer clientAppsBranchtoDC = echoClientBranchtoDC.Install(n1);
    clientAppsBranchtoDC.Start(Seconds(2.2)); // Slightly offset
    clientAppsBranchtoDC.Stop(Seconds(15.0));

    // Schedule link failure at t=6 seconds (disable primary HQ-DC link)
    // Disable the interface on HQ (interface 2) and DC (interface 1)
    Simulator::Schedule(Seconds(6.0), &DisableInterface, n0, 2);
    Simulator::Schedule(Seconds(6.0), &DisableInterface, n2, 1);

    // Install FlowMonitor for performance analysis
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // *** NetAnim Configuration ***
    AnimationInterface anim("scratch/triangular-wan.xml");

    // Set node descriptions
    anim.UpdateNodeDescription(n0, "HQ\n10.1.1.1 | 10.1.2.1");
    anim.UpdateNodeDescription(n1, "Branch\n10.1.1.2 | 10.1.3.1");
    anim.UpdateNodeDescription(n2, "DC\n10.1.2.2 | 10.1.3.2");

    // Set node colors
    anim.UpdateNodeColor(n0, 0, 255, 0);   // Green for HQ
    anim.UpdateNodeColor(n1, 255, 165, 0); // Orange for Branch
    anim.UpdateNodeColor(n2, 0, 0, 255);   // Blue for DC

    // Track packet flows
    anim.EnablePacketMetadata(true);

    // Enable PCAP tracing on all devices for Wireshark analysis
    p2p.EnablePcapAll("scratch/triangular-wan");

    // Run simulation
    Simulator::Stop(Seconds(16.0));
    Simulator::Run();

    // Analyze FlowMonitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n=== Flow Analysis ===\n";
    std::cout << "Note: Link failure occurs at t=6 seconds\n\n";
    
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " 
                  << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Packet Loss: " 
                  << ((flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets) 
                  << "%\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: " 
                      << flow.second.delaySum.GetSeconds() / flow.second.rxPackets * 1000 
                      << " ms\n";
            std::cout << "  Mean Jitter: " 
                      << flow.second.jitterSum.GetSeconds() / (flow.second.rxPackets - 1) * 1000 
                      << " ms\n";
            
            // Simple analysis based on timing
            // Packets sent every second, link fails at 6 seconds
            // So packets at t=2,3,4,5,6,7,8,9,10,11 (10 packets total)
            // Link fails at t=6, so packets 1-4 before failure, packets 5-10 after
            std::cout << "  Expected pattern:\n";
            std::cout << "    - 4 packets before link failure (t=2,3,4,5)\n";
            std::cout << "    - 6 packets after link failure (t=6,7,8,9,10,11)\n";
            if (t.sourceAddress == "10.1.1.1" && t.destinationAddress == "10.1.2.2")
            {
                std::cout << "  [HQ->DC Flow: Should use backup path after t=6s]\n";
            }
        }
        std::cout << "\n";
    }

    Simulator::Destroy();

    std::cout << "\n=== Simulation Complete ===\n";
    std::cout << "Three communication flows established:\n";
    std::cout << "  1. HQ -> DC (tests primary/backup path)\n";
    std::cout << "  2. HQ -> Branch (tests direct link)\n";
    std::cout << "  3. Branch -> DC (tests direct link)\n";
    std::cout << "\nPrimary HQ-DC link disabled at t=6 seconds\n";
    std::cout << "HQ->DC traffic should failover to backup path: HQ->Branch->DC\n";
    std::cout << "Other flows (HQ->Branch, Branch->DC) should continue unaffected\n";
    std::cout << "\nAnimation trace saved to: scratch/triangular-wan.xml\n";
    std::cout << "Routing tables saved to: scratch/triangular-wan.routes\n";
    std::cout << "PCAP traces saved to: scratch/triangular-wan-*.pcap\n";
    std::cout << "Open the XML file with NetAnim to visualize the simulation.\n";

    return 0;
}
