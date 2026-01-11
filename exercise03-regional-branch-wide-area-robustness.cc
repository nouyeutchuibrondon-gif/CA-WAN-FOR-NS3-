/*
 * RegionalBank WAN Resilience Simulation
 * NS-3.46.1 Compatible Version (No NetAnim)
 * 
 * Network Topology:
 *
 *                     [Backup Router] - 10.1.5.0/24
 *                           |           (Backup Path)
 *                           |
 *                   10.1.3.0/24 (Backup Link)
 *                           |
 *                           |  (Initially idle)
 *                           |
 *        10.1.1.0/24    10.1.4.0/24 (PRIMARY LINK)
 *           |                |
 *           |                |
 *     [Branch-C]━━━━━▶[ DC-A ]━━━━━▶[ DR-B ]
 *     10.1.1.1       10.1.1.2/     10.1.4.2
 *     (Client)      10.1.4.1/      (Server)
 *                   10.1.3.1
 *
 * Simulation tests static routing with manual failover vs
 * Ipv4GlobalRouting (simulates dynamic routing behavior)
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RegionalBankWAN");

// Global variables for statistics
uint32_t totalPacketsSent = 0;
uint32_t totalPacketsReceived = 0;
Time convergenceStartTime;
Time convergenceEndTime;
bool failureOccurred = false;
bool backupRouteActivated = false;

// Callback function for Tx trace
void
TxTrace(std::string context, Ptr<const Packet> packet)
{
    totalPacketsSent++;
    if (Simulator::Now().GetSeconds() > 1.9) { // Avoid initial setup noise
        std::cout << Simulator::Now().GetSeconds() << "s: " 
                  << context << " TX packet " << packet->GetSize() << " bytes" 
                  << " [Total Sent: " << totalPacketsSent << "]" << std::endl;
    }
}

// Callback function for Rx trace - correct signature for UdpEchoServer
void
RxTrace(std::string context, Ptr<const Packet> packet)
{
    totalPacketsReceived++;
    if (Simulator::Now().GetSeconds() > 1.9) {
        std::cout << Simulator::Now().GetSeconds() << "s: " 
                  << context << " RX packet " << packet->GetSize() << " bytes" 
                  << " [Total Received: " << totalPacketsReceived << "]" << std::endl;
    }
}

// Function to simulate link failure by making the link extremely slow
void
SimulateLinkFailure(Ptr<PointToPointNetDevice> device)
{
    std::cout << "\n=== LINK FAILURE EVENT at " << Simulator::Now().GetSeconds() << "s ===" << std::endl;
    std::cout << "Simulating primary network link failure..." << std::endl;
    
    // Make the data rate extremely slow to simulate link failure
    // This will cause packets to be dropped due to buffer overflow
    device->SetDataRate(DataRate("1bps")); // Extremely slow rate
    
    failureOccurred = true;
    convergenceStartTime = Simulator::Now();
    
    std::cout << "Link severely degraded. Traffic will now be severely impacted." << std::endl;
}

// Function to manually activate backup route (simulating dynamic routing)
void
ActivateBackupRoute(Ptr<Node> node, Ipv4Address destNetwork, Ipv4Mask mask, 
                    Ipv4Address nextHop, uint32_t interface)
{
    std::cout << "\n=== MANUAL FAILOVER at " << Simulator::Now().GetSeconds() << "s ===" << std::endl;
    std::cout << "Activating backup route for " << destNetwork << "/" << mask.GetPrefixLength() << std::endl;
    std::cout << "Next hop: " << nextHop << " via interface " << interface << std::endl;
    
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
    Ipv4StaticRouting* staticRouting = dynamic_cast<Ipv4StaticRouting*>(PeekPointer(routing));
    
    if (staticRouting) {
        // Find and remove the old route (if it exists)
        for (uint32_t i = 0; i < staticRouting->GetNRoutes(); i++) {
            Ipv4RoutingTableEntry route = staticRouting->GetRoute(i);
            if (route.GetDestNetwork() == destNetwork && route.GetDestNetworkMask() == mask) {
                staticRouting->RemoveRoute(i);
                break;
            }
        }
        
        // Add the backup route
        staticRouting->AddNetworkRouteTo(destNetwork, mask, nextHop, interface, 50);
        
        backupRouteActivated = true;
        convergenceEndTime = Simulator::Now();
        
        Time convergenceTime = convergenceEndTime - convergenceStartTime;
        std::cout << "Failover completed in " << convergenceTime.GetSeconds() << " seconds" << std::endl;
    }
}

int
main(int argc, char* argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("RegionalBankWAN", LOG_LEVEL_INFO);
    
    // Simulation parameters
    std::string routingType = "static"; // "static", "manual-failover", or "global"
    double simulationTime = 20.0;
    double failureTime = 5.0;
    std::string dataRate = "10Mbps";
    uint32_t packetSize = 1024;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("routing", "Routing type (static/manual-failover/global)", routingType);
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.AddValue("failure", "Link failure time", failureTime);
    cmd.AddValue("rate", "Data rate of primary link", dataRate);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.Parse(argc, argv);
    
    std::cout << "\n==============================================" << std::endl;
    std::cout << "RegionalBank WAN Resilience Simulation" << std::endl;
    std::cout << "NS-3 Version: 3.46.1" << std::endl;
    std::cout << "Routing Type: " << routingType << std::endl;
    std::cout << "Simulation Time: " << simulationTime << " seconds" << std::endl;
    std::cout << "Link Failure at: " << failureTime << " seconds" << std::endl;
    std::cout << "==============================================\n" << std::endl;
    
    // Create 4 nodes: Branch-C (client), DC-A (router), DR-B (server), Backup-Router
    NodeContainer nodes;
    nodes.Create(4);
    
    Ptr<Node> branchC = nodes.Get(0); // Client (Branch-C)
    Ptr<Node> dcA = nodes.Get(1);     // Primary Router (DC-A)
    Ptr<Node> drB = nodes.Get(2);     // Server (DR-B)
    Ptr<Node> backupRouter = nodes.Get(3); // Backup Router
    
    // Create point-to-point links with different characteristics
    PointToPointHelper p2pPrimary, p2pBackup, p2pAccess, p2pBackupPath;
    
    // Network 1: Branch-C ↔ DC-A (Access Network)
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("2ms"));
    NodeContainer net1Nodes(branchC, dcA);
    NetDeviceContainer net1Devices = p2pAccess.Install(net1Nodes);
    
    // Network 4: DC-A ↔ DR-B (Primary WAN Link)
    p2pPrimary.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pPrimary.SetChannelAttribute("Delay", StringValue("5ms"));
    NodeContainer net4Nodes(dcA, drB);
    NetDeviceContainer net4Devices = p2pPrimary.Install(net4Nodes);
    
    // Network 3: DC-A ↔ DR-B (Backup Direct Link)
    p2pBackup.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    p2pBackup.SetChannelAttribute("Delay", StringValue("10ms"));
    NodeContainer net3Nodes(dcA, drB);
    NetDeviceContainer net3Devices = p2pBackup.Install(net3Nodes);
    
    // Network 5: DC-A ↔ Backup-Router ↔ DR-B (Alternative Backup Path)
    p2pBackupPath.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBackupPath.SetChannelAttribute("Delay", StringValue("20ms"));
    NodeContainer net5Nodes(dcA, backupRouter);
    NodeContainer net6Nodes(backupRouter, drB);
    NetDeviceContainer net5Devices = p2pBackupPath.Install(net5Nodes);
    NetDeviceContainer net6Devices = p2pBackupPath.Install(net6Nodes);
    
    // Install Internet stack with appropriate routing
    InternetStackHelper stack;
    stack.Install(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper address;
    
    // Network 1: 10.1.1.0/24 (Branch-C ↔ DC-A)
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(net1Devices);
    
    // Network 4: 10.1.4.0/24 (DC-A ↔ DR-B Primary)
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(net4Devices);
    
    // Network 3: 10.1.3.0/24 (DC-A ↔ DR-B Backup)
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(net3Devices);
    
    // Network 5: 10.1.5.0/24 (DC-A ↔ Backup-Router)
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces5 = address.Assign(net5Devices);
    
    // Network 6: 10.1.6.0/24 (Backup-Router ↔ DR-B)
    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces6 = address.Assign(net6Devices);
    
    // Print network configuration
    std::cout << "\n=== NETWORK CONFIGURATION ===" << std::endl;
    std::cout << "Branch-C (Client): " << interfaces1.GetAddress(0) << std::endl;
    std::cout << "DC-A (Router):" << std::endl;
    std::cout << "  Interface to Branch-C: " << interfaces1.GetAddress(1) << std::endl;
    std::cout << "  Interface to DR-B (Primary): " << interfaces4.GetAddress(0) << std::endl;
    std::cout << "  Interface to DR-B (Backup): " << interfaces3.GetAddress(0) << std::endl;
    std::cout << "  Interface to Backup-Router: " << interfaces5.GetAddress(0) << std::endl;
    std::cout << "DR-B (Server):" << std::endl;
    std::cout << "  Interface to DC-A (Primary): " << interfaces4.GetAddress(1) << std::endl;
    std::cout << "  Interface to DC-A (Backup): " << interfaces3.GetAddress(1) << std::endl;
    std::cout << "  Interface to Backup-Router: " << interfaces6.GetAddress(1) << std::endl;
    std::cout << "Backup-Router: " << interfaces5.GetAddress(1) << " / " 
              << interfaces6.GetAddress(0) << std::endl;
    
    // Configure static routing for non-global routing types
    if (routingType != "global") {
        std::cout << "\n=== CONFIGURING STATIC ROUTES ===" << std::endl;
        
        // Enable IP forwarding on router
        Ptr<Ipv4> ipv4DCA = dcA->GetObject<Ipv4>();
        ipv4DCA->SetAttribute("IpForward", BooleanValue(true));
        
        // Get static routing helper
        Ipv4StaticRoutingHelper staticRoutingHelper;
        
        // Configure Branch-C routes
        Ptr<Ipv4StaticRouting> routingBranchC = 
            staticRoutingHelper.GetStaticRouting(branchC->GetObject<Ipv4>());
        
        // Route to DR-B via DC-A (primary path)
        routingBranchC->AddNetworkRouteTo(
            Ipv4Address("10.1.4.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.1.2"), // DC-A interface
            1, // Interface index
            10 // Metric
        );
        
        // Configure DC-A routes
        Ptr<Ipv4StaticRouting> routingDCA = 
            staticRoutingHelper.GetStaticRouting(dcA->GetObject<Ipv4>());
        
        // Route to DR-B via primary link
        routingDCA->AddNetworkRouteTo(
            Ipv4Address("10.1.4.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.4.2"), // DR-B interface
            2, // Interface index (primary link)
            10 // Metric (lowest for primary)
        );
        
        if (routingType == "static") {
            // For pure static routing - no backup route pre-configured
            std::cout << "Pure static routing - no backup routes configured" << std::endl;
        } else if (routingType == "manual-failover") {
            // For manual failover - pre-configure backup routes with higher metrics
            std::cout << "Manual failover mode - backup routes pre-configured" << std::endl;
            
            // Route to DR-B via backup link (higher metric)
            routingDCA->AddNetworkRouteTo(
                Ipv4Address("10.1.4.0"),
                Ipv4Mask("255.255.255.0"),
                Ipv4Address("10.1.3.2"), // DR-B backup interface
                3, // Interface index (backup link)
                50 // Higher metric
            );
            
            // Route to DR-B via backup router path (highest metric)
            routingDCA->AddNetworkRouteTo(
                Ipv4Address("10.1.4.0"),
                Ipv4Mask("255.255.255.0"),
                Ipv4Address("10.1.5.2"), // Backup router
                4, // Interface index
                100 // Highest metric
            );
        }
        
        // Configure DR-B routes
        Ptr<Ipv4StaticRouting> routingDRB = 
            staticRoutingHelper.GetStaticRouting(drB->GetObject<Ipv4>());
        
        // Route to Branch-C via primary link
        routingDRB->AddNetworkRouteTo(
            Ipv4Address("10.1.1.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.4.1"), // DC-A primary interface
            1, // Interface index
            10
        );
        
        if (routingType == "manual-failover") {
            // Backup route for DR-B
            routingDRB->AddNetworkRouteTo(
                Ipv4Address("10.1.1.0"),
                Ipv4Mask("255.255.255.0"),
                Ipv4Address("10.1.3.1"), // DC-A backup interface
                2, // Interface index
                50
            );
        }
        
        // Configure Backup-Router routes
        Ptr<Ipv4StaticRouting> routingBackup = 
            staticRoutingHelper.GetStaticRouting(backupRouter->GetObject<Ipv4>());
        
        // Route from DC-A to DR-B via backup router
        routingBackup->AddNetworkRouteTo(
            Ipv4Address("10.1.4.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.6.2"), // DR-B interface
            2, // Interface index
            10
        );
        
        std::cout << "Static routes configured" << std::endl;
        if (routingType == "manual-failover") {
            std::cout << "Backup routes pre-configured with metrics:" << std::endl;
            std::cout << "  Primary path: metric 10" << std::endl;
            std::cout << "  Backup direct: metric 50" << std::endl;
            std::cout << "  Backup via router: metric 100" << std::endl;
        }
    }
    
    // Populate routing tables for global routing
    if (routingType == "global") {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }
    
    // Create UDP Echo Server on DR-B
    uint16_t port = 50000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(drB);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));
    
    // Create UDP Echo Client on Branch-C
    UdpEchoClientHelper echoClient(interfaces4.GetAddress(1), port); // Primary DR-B IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000)); // Large number for continuous flow
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // 10 packets per second
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    ApplicationContainer clientApps = echoClient.Install(branchC);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 1));
    
    // Install trace callbacks for detailed monitoring
    Config::Connect("/NodeList/0/ApplicationList/*/Tx", MakeCallback(&TxTrace));
    Config::Connect("/NodeList/2/ApplicationList/*/Rx", MakeCallback(&RxTrace));
    
    // Schedule link failure at specified time
    std::cout << "\n=== SCHEDULING LINK FAILURE ===" << std::endl;
    std::cout << "Primary link (Network 4) will fail at t=" << failureTime << "s" << std::endl;
    
    // Get the PointToPointNetDevice objects
    Ptr<PointToPointNetDevice> dcAPrimaryDev = DynamicCast<PointToPointNetDevice>(net4Devices.Get(0));
    Ptr<PointToPointNetDevice> drBPrimaryDev = DynamicCast<PointToPointNetDevice>(net4Devices.Get(1));
    
    if (dcAPrimaryDev && drBPrimaryDev) {
        Simulator::Schedule(Seconds(failureTime), &SimulateLinkFailure, dcAPrimaryDev);
        Simulator::Schedule(Seconds(failureTime), &SimulateLinkFailure, drBPrimaryDev);
    } else {
        std::cout << "WARNING: Could not get PointToPointNetDevice for link failure simulation" << std::endl;
    }
    
    // Schedule appropriate recovery based on routing type
    if (routingType == "manual-failover") {
        // Simulate manual failover after 2 seconds (like an admin intervention)
        std::cout << "Manual failover scheduled 2 seconds after failure" << std::endl;
        Simulator::Schedule(Seconds(failureTime + 2.0), &ActivateBackupRoute, 
                           dcA, Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"),
                           Ipv4Address("10.1.3.2"), 3);
    } else if (routingType == "global") {
        // Global routing will automatically recalculate
        std::cout << "Global routing will automatically recalculate routes" << std::endl;
        // Force routing recalculation after link failure
        Simulator::Schedule(Seconds(failureTime + 0.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
    }
    
    // Install FlowMonitor for comprehensive statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    
    // Enable PCAP tracing on all devices
    p2pPrimary.EnablePcapAll("scratch/regionalbank-primary");
    p2pBackup.EnablePcapAll("scratch/regionalbank-backup");
    p2pAccess.EnablePcapAll("scratch/regionalbank-access");
    
    // Run simulation
    std::cout << "\n=== STARTING SIMULATION ===" << std::endl;
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // Collect and display results
    std::cout << "\n=== SIMULATION RESULTS ===" << std::endl;
    std::cout << "Routing Type: " << routingType << std::endl;
    std::cout << "Total packets sent: " << totalPacketsSent << std::endl;
    std::cout << "Total packets received: " << totalPacketsReceived << std::endl;
    
    double packetDeliveryRatio = (totalPacketsSent > 0) ? 
        (100.0 * totalPacketsReceived / totalPacketsSent) : 0;
    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << "%" << std::endl;
    
    if (backupRouteActivated && convergenceEndTime.IsPositive()) {
        Time convergenceTime = convergenceEndTime - convergenceStartTime;
        std::cout << "Failover Time: " << convergenceTime.GetSeconds() << " seconds" << std::endl;
    }
    
    // Generate FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(
        flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    std::cout << "\n=== DETAILED FLOW STATISTICS ===" << std::endl;
    
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        
        std::cout << "\nFlow ID: " << iter->first << std::endl;
        std::cout << "  Source:      " << t.sourceAddress << ":" << t.sourcePort << std::endl;
        std::cout << "  Destination: " << t.destinationAddress << ":" << t.destinationPort << std::endl;
        std::cout << "  Protocol:    " << (int)t.protocol << std::endl;
        std::cout << "  Tx Packets:  " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets:  " << iter->second.rxPackets << std::endl;
        
        if (iter->second.txPackets > 0) {
            double lossRate = 100.0 * (iter->second.txPackets - iter->second.rxPackets) / 
                              iter->second.txPackets;
            std::cout << "  Packet Loss: " << lossRate << "%" << std::endl;
            
            if (iter->second.rxPackets > 0) {
                std::cout << "  Mean Delay:  " << 
                    iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
                std::cout << "  Mean Jitter: " << 
                    iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets - 1) << " s" << std::endl;
                std::cout << "  Throughput:  " << 
                    iter->second.rxBytes * 8.0 / (simulationTime - 2.0) / 1000000.0 << " Mbps" << std::endl;
            }
        }
    }
    
    // Generate XML output for further analysis
    std::string flowmonFile = "scratch/regionalbank-flowmon.xml";
    monitor->SerializeToXmlFile(flowmonFile, true, true);
    std::cout << "\nFlowMonitor statistics saved to: " << flowmonFile << std::endl;
    
    // Generate network configuration file for visualization
    std::ofstream configFile("scratch/network-config.json");
    if (configFile.is_open()) {
        configFile << "{\n";
        configFile << "  \"network\": {\n";
        configFile << "    \"name\": \"RegionalBank WAN\",\n";
        configFile << "    \"routing_type\": \"" << routingType << "\",\n";
        configFile << "    \"simulation_time\": " << simulationTime << ",\n";
        configFile << "    \"failure_time\": " << failureTime << ",\n";
        configFile << "    \"data_rate\": \"" << dataRate << "\",\n";
        configFile << "    \"packet_size\": " << packetSize << ",\n";
        configFile << "    \"convergence_time\": " << 
            (backupRouteActivated && convergenceEndTime.IsPositive() ? 
             (convergenceEndTime.GetSeconds() - failureTime) : 0) << ",\n";
        configFile << "    \"packet_delivery_ratio\": " << packetDeliveryRatio << "\n";
        configFile << "  },\n";
        
        configFile << "  \"nodes\": [\n";
        configFile << "    {\"id\": 0, \"name\": \"Branch-C\", \"type\": \"client\", \"ip\": \"10.1.1.1\"},\n";
        configFile << "    {\"id\": 1, \"name\": \"DC-A\", \"type\": \"router\", \"ip\": [\"10.1.1.2\", \"10.1.4.1\", \"10.1.3.1\", \"10.1.5.1\"]},\n";
        configFile << "    {\"id\": 2, \"name\": \"DR-B\", \"type\": \"server\", \"ip\": [\"10.1.4.2\", \"10.1.3.2\", \"10.1.6.2\"]},\n";
        configFile << "    {\"id\": 3, \"name\": \"Backup-Router\", \"type\": \"backup\", \"ip\": [\"10.1.5.2\", \"10.1.6.1\"]}\n";
        configFile << "  ],\n";
        
        configFile << "  \"links\": [\n";
        configFile << "    {\"from\": 0, \"to\": 1, \"name\": \"Access Link\", \"bandwidth\": \"100Mbps\", \"delay\": \"2ms\", \"type\": \"access\"},\n";
        configFile << "    {\"from\": 1, \"to\": 2, \"name\": \"Primary WAN\", \"bandwidth\": \"" << dataRate << "\", \"delay\": \"5ms\", \"type\": \"primary\"},\n";
        configFile << "    {\"from\": 1, \"to\": 2, \"name\": \"Backup Direct\", \"bandwidth\": \"2Mbps\", \"delay\": \"10ms\", \"type\": \"backup\"},\n";
        configFile << "    {\"from\": 1, \"to\": 3, \"name\": \"Backup Path 1\", \"bandwidth\": \"5Mbps\", \"delay\": \"20ms\", \"type\": \"backup-path\"},\n";
        configFile << "    {\"from\": 3, \"to\": 2, \"name\": \"Backup Path 2\", \"bandwidth\": \"5Mbps\", \"delay\": \"20ms\", \"type\": \"backup-path\"}\n";
        configFile << "  ],\n";
        
        configFile << "  \"statistics\": {\n";
        configFile << "    \"total_packets_sent\": " << totalPacketsSent << ",\n";
        configFile << "    \"total_packets_received\": " << totalPacketsReceived << ",\n";
        configFile << "    \"packet_delivery_ratio\": " << packetDeliveryRatio << ",\n";
        configFile << "    \"failure_occurred\": " << (failureOccurred ? "true" : "false") << ",\n";
        configFile << "    \"backup_activated\": " << (backupRouteActivated ? "true" : "false") << "\n";
        configFile << "  }\n";
        configFile << "}\n";
        configFile.close();
        std::cout << "Network configuration saved to: scratch/network-config.json" << std::endl;
    }
    
    // Calculate and display business impact
    std::cout << "\n=== BUSINESS IMPACT ANALYSIS ===" << std::endl;
    
    double downtime = 0;
    if (routingType == "static") {
        downtime = simulationTime - failureTime; // Permanent outage
    } else if (routingType == "manual-failover") {
        downtime = (backupRouteActivated && convergenceEndTime.IsPositive()) ? 
                   (convergenceEndTime.GetSeconds() - failureTime) : 
                   (simulationTime - failureTime);
    } else if (routingType == "global") {
        // Global routing typically converges quickly
        downtime = 1.0; // Estimate for global routing
    }
    
    std::cout << "Routing Type: " << routingType << std::endl;
    std::cout << "Estimated Downtime: " << downtime << " seconds" << std::endl;
    
    // Business impact calculations
    double transactionsPerSecond = 10.0;
    double downtimeCostPerMinute = 10000.0;
    
    double lostTransactions = downtime * transactionsPerSecond;
    double estimatedCost = (downtime / 60.0) * downtimeCostPerMinute;
    
    std::cout << "Lost Transactions: " << lostTransactions << std::endl;
    std::cout << "Estimated Cost: $" << estimatedCost << std::endl;
    
    // SLA Compliance Check
    std::cout << "\n=== SLA COMPLIANCE CHECK ===" << std::endl;
    
    bool slaRTO = (downtime <= 2.0); // Recovery Time Objective: 2 seconds
    bool slaAvailability = (packetDeliveryRatio >= 99.95); // 99.95% availability
    
    std::cout << "RTO (2 seconds): " << (slaRTO ? "PASS" : "FAIL") 
              << " (" << downtime << "s)" << std::endl;
    std::cout << "Availability (99.95%): " << (slaAvailability ? "PASS" : "FAIL") 
              << " (" << packetDeliveryRatio << "%)" << std::endl;
    
    // Generate summary report
    std::cout << "\n=== SIMULATION SUMMARY ===" << std::endl;
    std::cout << "Output files generated in 'scratch/' directory:" << std::endl;
    std::cout << "  1. regionalbank-flowmon.xml (FlowMonitor statistics)" << std::endl;
    std::cout << "  2. regionalbank-*.pcap (PCAP traces for Wireshark)" << std::endl;
    std::cout << "  3. network-config.json (Network configuration for visualization)" << std::endl;
    std::cout << "\nTo generate visualizations, run: python3 visualize-wan.py" << std::endl;
    
    std::cout << "\n=== RECOMMENDATIONS ===" << std::endl;
    if (routingType == "static") {
        std::cout << "STATIC ROUTING: Causes complete outage (" << downtime << "s downtime)" << std::endl;
        std::cout << "Recommend implementing failover mechanism or dynamic routing" << std::endl;
    } else if (routingType == "manual-failover") {
        std::cout << "MANUAL FAILOVER: Recovery in " << downtime << " seconds" << std::endl;
        if (downtime > 2.0) {
            std::cout << "Consider automated monitoring systems for faster failover" << std::endl;
        }
    } else if (routingType == "global") {
        std::cout << "GLOBAL ROUTING: Automatic recovery with " << downtime << "s downtime" << std::endl;
        std::cout << "Most resilient option for WAN environments" << std::endl;
    }
    
    Simulator::Destroy();
    
    std::cout << "\n=== SIMULATION COMPLETE ===" << std::endl;
    
    return 0;
}
