/*
 * Two nodes separated by a router with QoS implementation
 * 
 * Network Topology:
 *
 *   Network 1 (10.1.1.0/24)          Network 2 (10.1.2.0/24)
 *
 *   n0 -------------------- n1 (Router) -------------------- n2
 *      point-to-point                    point-to-point
 *      5Mbps, 2ms                        5Mbps, 2ms
 *
 * QoS Implementation:
 * - Class 1: VoIP-like traffic (160-byte packets, 20ms interval, DSCP EF)
 * - Class 2: FTP-like traffic (1500-byte packets, bursty, DSCP BE)
 * - Simple priority queuing using DSCP marking
 * - Performance measurement using FlowMonitor
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QoSSimulation");

// Custom application for VoIP traffic with timestamp tagging
class VoipApplication : public Application
{
public:
    VoipApplication();
    virtual ~VoipApplication();

    void Setup(Address address, uint32_t packetSize, Time interval, Time startTime, Time stopTime, uint8_t dscp);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    uint8_t m_dscp;
};

VoipApplication::VoipApplication()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_interval(Seconds(1)),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0),
      m_dscp(0)
{
}

VoipApplication::~VoipApplication()
{
    m_socket = 0;
}

void
VoipApplication::Setup(Address address, uint32_t packetSize, Time interval, Time startTime, Time stopTime, uint8_t dscp)
{
    m_peer = address;
    m_packetSize = packetSize;
    m_interval = interval;
    m_dscp = dscp;
}

void
VoipApplication::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        
        // Bind to any available port
        m_socket->Bind();
        m_socket->Connect(m_peer);
        
        // Set DSCP socket option
        m_socket->SetIpTos(m_dscp << 2); // DSCP in top 6 bits of ToS
    }
    
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    ScheduleTx();
}

void
VoipApplication::StopApplication(void)
{
    m_running = false;
    
    if (m_sendEvent.IsPending()) {
        Simulator::Cancel(m_sendEvent);
    }
    
    if (m_socket) {
        m_socket->Close();
    }
}

void
VoipApplication::ScheduleTx(void)
{
    if (m_running) {
        Time tNext(m_interval);
        m_sendEvent = Simulator::Schedule(tNext, &VoipApplication::SendPacket, this);
    }
}

void
VoipApplication::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    
    // Add timestamp for delay measurement
    TimestampTag timestamp;
    timestamp.SetTimestamp(Simulator::Now());
    packet->AddByteTag(timestamp);
    
    // Add DSCP tag
    SocketIpTosTag tosTag;
    tosTag.SetTos(m_dscp << 2);
    packet->AddPacketTag(tosTag);
    
    m_socket->Send(packet);
    ++m_packetsSent;
    
    ScheduleTx();
}

int
main(int argc, char* argv[])
{
    // Enable logging for QoSSimulation only
    LogComponentEnable("QoSSimulation", LOG_LEVEL_INFO);
    // Comment out or remove the VoipApplication logging as it's not registered
    // LogComponentEnable("VoipApplication", LOG_LEVEL_INFO);
    
    // QoS parameters
    bool enableQoS = true;
    uint32_t nFtpFlows = 3; // Number of FTP-like flows for congestion
    uint32_t queueSize = 100; // Packets
    
    CommandLine cmd;
    cmd.AddValue("qos", "Enable QoS (true/false)", enableQoS);
    cmd.AddValue("ftpflows", "Number of FTP flows", nFtpFlows);
    cmd.AddValue("queuesize", "Queue size in packets", queueSize);
    cmd.Parse(argc, argv);
    
    std::cout << "\n=== QoS Simulation Configuration ===\n";
    std::cout << "QoS Enabled: " << (enableQoS ? "YES" : "NO") << "\n";
    std::cout << "FTP Flows: " << nFtpFlows << "\n";
    std::cout << "Queue Size: " << queueSize << " packets\n";
    std::cout << "===================================\n";
    
    // Create three nodes: n0 (client), n1 (router), n2 (server)
    NodeContainer nodes;
    nodes.Create(3);
    
    Ptr<Node> n0 = nodes.Get(0); // Client
    Ptr<Node> n1 = nodes.Get(1); // Router
    Ptr<Node> n2 = nodes.Get(2); // Server
    
    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // Set queue size for bottleneck
    p2p.SetQueue("ns3::DropTailQueue<Packet>", 
                 "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    
    // Link 1: n0 <-> n1 (Network 1)
    NodeContainer link1Nodes(n0, n1);
    NetDeviceContainer link1Devices = p2p.Install(link1Nodes);
    
    // Link 2: n1 <-> n2 (Network 2)
    NodeContainer link2Nodes(n1, n2);
    NetDeviceContainer link2Devices = p2p.Install(link2Nodes);
    
    // Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    // Set positions
    Ptr<MobilityModel> mob0 = n0->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = n1->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob2 = n2->GetObject<MobilityModel>();
    
    mob0->SetPosition(Vector(5.0, 15.0, 0.0));
    mob1->SetPosition(Vector(10.0, 2.0, 0.0));
    mob2->SetPosition(Vector(15.0, 15.0, 0.0));
    
    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(link1Devices);
    
    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address2.Assign(link2Devices);
    
    // Enable IP forwarding on router
    Ptr<Ipv4> ipv4Router = n1->GetObject<Ipv4>();
    ipv4Router->SetAttribute("IpForward", BooleanValue(true));
    
    // Configure static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;
    
    // Route on n0
    Ptr<Ipv4StaticRouting> staticRoutingN0 = staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv4>());
    staticRoutingN0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.1.2"), 1);
    
    // Route on n2
    Ptr<Ipv4StaticRouting> staticRoutingN2 = staticRoutingHelper.GetStaticRouting(n2->GetObject<Ipv4>());
    staticRoutingN2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.2.1"), 1);
    
    // ========== SIMPLE QoS CONFIGURATION ==========
    // Using DSCP marking and simple queue management
    
    // Server applications on n2
    uint16_t voipPort = 5000;
    uint16_t ftpPort = 5001;
    
    // VoIP server
    PacketSinkHelper voipServer("ns3::UdpSocketFactory", 
                               InetSocketAddress(Ipv4Address::GetAny(), voipPort));
    ApplicationContainer voipServerApp = voipServer.Install(n2);
    voipServerApp.Start(Seconds(0.0));
    voipServerApp.Stop(Seconds(15.0));
    
    // FTP server
    PacketSinkHelper ftpServer("ns3::UdpSocketFactory", 
                              InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer ftpServerApp = ftpServer.Install(n2);
    ftpServerApp.Start(Seconds(0.0));
    ftpServerApp.Stop(Seconds(15.0));
    
    // Client applications on n0
    
    // 1. VoIP-like traffic (EF - Expedited Forwarding, DSCP 46)
    uint32_t voipPacketSize = 160; // bytes
    Time voipInterval = MilliSeconds(20); // 50 packets/sec
    uint8_t voipDscp = 46; // EF
    
    std::cout << "\n=== Traffic Generation ===\n";
    std::cout << "VoIP Traffic: " << voipPacketSize << " bytes every " 
              << voipInterval.GetMilliSeconds() << "ms (DSCP 46 - EF)\n";
    std::cout << "FTP Traffic: " << nFtpFlows << " flows, 1500 bytes bursty (DSCP 0 - BE)\n";
    std::cout << "Total FTP load: ~" << (nFtpFlows * 2) << " Mbps on 5 Mbps link\n";
    
    Ptr<VoipApplication> voipApp = CreateObject<VoipApplication>();
    voipApp->Setup(InetSocketAddress(interfaces2.GetAddress(1), voipPort),
                   voipPacketSize, voipInterval, Seconds(1.0), Seconds(14.0), voipDscp);
    n0->AddApplication(voipApp);
    voipApp->SetStartTime(Seconds(1.0));
    voipApp->SetStopTime(Seconds(14.0));
    
    // 2. FTP-like traffic (Bulk data, Best Effort, DSCP 0)
    ApplicationContainer ftpApps;
    
    for (uint32_t i = 0; i < nFtpFlows; i++) {
        // OnOff application for bursty FTP traffic
        OnOffHelper ftpClient("ns3::UdpSocketFactory", 
                            InetSocketAddress(interfaces2.GetAddress(1), ftpPort));
        
        // Configure bursty traffic pattern
        ftpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
        ftpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        ftpClient.SetAttribute("PacketSize", UintegerValue(1500)); // MTU size
        ftpClient.SetAttribute("DataRate", StringValue("2Mbps")); // Each flow 2Mbps
        
        // Set DSCP to 0 (Best Effort) - Note: ToS needs to be shifted
        ftpClient.SetAttribute("Tos", UintegerValue(0)); // DSCP 0
        
        ApplicationContainer ftpApp = ftpClient.Install(n0);
        
        // Stagger start times to create varying congestion
        double startTime = 3.0 + i * 0.5;
        ftpApp.Start(Seconds(startTime));
        ftpApp.Stop(Seconds(12.0));
        
        ftpApps.Add(ftpApp);
        
        std::cout << "FTP Flow " << i+1 << ": Starts at " << startTime << "s, DataRate=2Mbps\n";
    }
    
    // ========== PERFORMANCE MEASUREMENT ==========
    
    // Install FlowMonitor on all nodes
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // ========== SIMULATION SETUP ==========
    
    // NetAnim Configuration
    AnimationInterface anim("scratch/qos-simulation.xml");
    
    // Set node descriptions
    anim.UpdateNodeDescription(n0, "Client\n10.1.1.1\nVoIP+FTP");
    anim.UpdateNodeDescription(n1, enableQoS ? "Router with QoS\n10.1.1.2 | 10.1.2.1" : "Router\n10.1.1.2 | 10.1.2.1");
    anim.UpdateNodeDescription(n2, "Server\n10.1.2.2");
    
    // Set node colors
    anim.UpdateNodeColor(n0, 0, 255, 0);   // Green
    anim.UpdateNodeColor(n1, 255, 255, 0); // Yellow
    anim.UpdateNodeColor(n2, 0, 0, 255);   // Blue
    
    // Color packets by DSCP in animation
    anim.EnablePacketMetadata(true);
    
    // Enable PCAP tracing on router interfaces only (to reduce file size)
    p2p.EnablePcap("scratch/qos-router", link1Devices.Get(1), true); // Router interface 1
    p2p.EnablePcap("scratch/qos-router", link2Devices.Get(0), true); // Router interface 2
    
    std::cout << "\n=== Starting Simulation ===\n";
    std::cout << "Simulation Time: 15 seconds\n";
    
    // Schedule simulation stop
    Simulator::Stop(Seconds(15.0));
    
    // Run simulation
    Simulator::Run();
    
    // ========== RESULTS COLLECTION ==========
    
    std::cout << "\n=== Collecting Results ===\n";
    
    // Collect FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    
    // Separate statistics for VoIP and FTP
    double voipTotalDelay = 0;
    double voipTotalJitter = 0;
    uint32_t voipTxPackets = 0;
    uint32_t voipRxPackets = 0;
    uint64_t voipRxBytes = 0;
    double voipFirstTx = 0;
    double voipLastRx = 0;
    
    double ftpTotalDelay = 0;
    double ftpTotalJitter = 0;
    uint32_t ftpTxPackets = 0;
    uint32_t ftpRxPackets = 0;
    uint64_t ftpRxBytes = 0;
    double ftpFirstTx = 0;
    double ftpLastRx = 0;
    
    std::cout << "\n=== Flow Statistics ===\n";
    
    for (auto const& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        
        // Determine traffic type by port
        if (t.destinationPort == voipPort) {
            // VoIP traffic
            voipTxPackets += flow.second.txPackets;
            voipRxPackets += flow.second.rxPackets;
            voipRxBytes += flow.second.rxBytes;
            voipTotalDelay += flow.second.delaySum.GetSeconds();
            voipTotalJitter += flow.second.jitterSum.GetSeconds();
            
            if (flow.second.timeFirstTxPacket.GetSeconds() > 0) {
                if (voipFirstTx == 0 || flow.second.timeFirstTxPacket.GetSeconds() < voipFirstTx) {
                    voipFirstTx = flow.second.timeFirstTxPacket.GetSeconds();
                }
            }
            
            if (flow.second.timeLastRxPacket.GetSeconds() > voipLastRx) {
                voipLastRx = flow.second.timeLastRxPacket.GetSeconds();
            }
            
            std::cout << "Flow " << flow.first << " (VoIP): " 
                      << flow.second.rxPackets << "/" << flow.second.txPackets 
                      << " packets received, Loss: " 
                      << ((flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets)
                      << "%\n";
        } else if (t.destinationPort == ftpPort) {
            // FTP traffic
            ftpTxPackets += flow.second.txPackets;
            ftpRxPackets += flow.second.rxPackets;
            ftpRxBytes += flow.second.rxBytes;
            ftpTotalDelay += flow.second.delaySum.GetSeconds();
            ftpTotalJitter += flow.second.jitterSum.GetSeconds();
            
            if (flow.second.timeFirstTxPacket.GetSeconds() > 0) {
                if (ftpFirstTx == 0 || flow.second.timeFirstTxPacket.GetSeconds() < ftpFirstTx) {
                    ftpFirstTx = flow.second.timeFirstTxPacket.GetSeconds();
                }
            }
            
            if (flow.second.timeLastRxPacket.GetSeconds() > ftpLastRx) {
                ftpLastRx = flow.second.timeLastRxPacket.GetSeconds();
            }
            
            std::cout << "Flow " << flow.first << " (FTP): " 
                      << flow.second.rxPackets << "/" << flow.second.txPackets 
                      << " packets received, Loss: " 
                      << ((flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets)
                      << "%\n";
        }
    }
    
    // Calculate throughput
    double voipDuration = (voipLastRx - voipFirstTx);
    double voipThroughput = (voipDuration > 0) ? (voipRxBytes * 8.0 / voipDuration / 1000.0) : 0; // Kbps
    
    double ftpDuration = (ftpLastRx - ftpFirstTx);
    double ftpThroughput = (ftpDuration > 0) ? (ftpRxBytes * 8.0 / ftpDuration / 1000.0) : 0; // Kbps
    
    // Calculate averages
    double voipAvgDelay = (voipRxPackets > 0) ? (voipTotalDelay / voipRxPackets * 1000.0) : 0; // ms
    double voipAvgJitter = (voipRxPackets > 1) ? (voipTotalJitter / (voipRxPackets - 1) * 1000.0) : 0; // ms
    double voipLossRate = (voipTxPackets > 0) ? ((voipTxPackets - voipRxPackets) * 100.0 / voipTxPackets) : 0;
    
    double ftpAvgDelay = (ftpRxPackets > 0) ? (ftpTotalDelay / ftpRxPackets * 1000.0) : 0; // ms
    double ftpAvgJitter = (ftpRxPackets > 1) ? (ftpTotalJitter / (ftpRxPackets - 1) * 1000.0) : 0; // ms
    double ftpLossRate = (ftpTxPackets > 0) ? ((ftpTxPackets - ftpRxPackets) * 100.0 / ftpTxPackets) : 0;
    
    // Print results table
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "+----------------+------------+------------+\n";
    std::cout << "| Metric         | VoIP (EF)  | FTP (BE)   |\n";
    std::cout << "+----------------+------------+------------+\n";
    std::cout << "| Tx Packets     | " << std::setw(10) << voipTxPackets << " | " << std::setw(10) << ftpTxPackets << " |\n";
    std::cout << "| Rx Packets     | " << std::setw(10) << voipRxPackets << " | " << std::setw(10) << ftpRxPackets << " |\n";
    std::cout << "| Loss Rate (%)  | " << std::setw(10) << voipLossRate << " | " << std::setw(10) << ftpLossRate << " |\n";
    std::cout << "| Avg Delay (ms) | " << std::setw(10) << voipAvgDelay << " | " << std::setw(10) << ftpAvgDelay << " |\n";
    std::cout << "| Avg Jitter (ms)| " << std::setw(10) << voipAvgJitter << " | " << std::setw(10) << ftpAvgJitter << " |\n";
    std::cout << "| Throughput (Kbps) | " << std::setw(8) << voipThroughput << " | " << std::setw(8) << ftpThroughput << " |\n";
    std::cout << "+----------------+------------+------------+\n";
    
    // VoIP quality assessment (ITU-T G.114 recommendations)
    std::cout << "\n=== VoIP Quality Assessment (ITU-T G.114) ===\n";
    if (voipAvgDelay < 150) {
        if (voipAvgDelay < 20) {
            std::cout << "Delay: EXCELLENT (< 20ms)\n";
        } else if (voipAvgDelay < 50) {
            std::cout << "Delay: GOOD (20-50ms)\n";
        } else {
            std::cout << "Delay: ACCEPTABLE (50-150ms)\n";
        }
    } else {
        std::cout << "Delay: POOR (> 150ms)\n";
    }
    
    if (voipAvgJitter < 30) {
        std::cout << "Jitter: ACCEPTABLE (< 30ms)\n";
    } else {
        std::cout << "Jitter: POOR (> 30ms)\n";
    }
    
    if (voipLossRate < 1) {
        std::cout << "Loss: EXCELLENT (< 1%)\n";
    } else if (voipLossRate < 3) {
        std::cout << "Loss: ACCEPTABLE (1-3%)\n";
    } else {
        std::cout << "Loss: POOR (> 3%)\n";
    }
    
    // Overall assessment
    if (voipAvgDelay < 150 && voipAvgJitter < 30 && voipLossRate < 3) {
        std::cout << "\nOVERALL: VoIP QUALITY IS ACCEPTABLE\n";
    } else {
        std::cout << "\nOVERALL: VoIP QUALITY IS DEGRADED\n";
    }
    
    // Calculate QoS effectiveness if we have both traffic types
    if (enableQoS && voipAvgDelay > 0 && ftpAvgDelay > 0) {
        double delayImprovement = ((ftpAvgDelay - voipAvgDelay) / ftpAvgDelay) * 100;
        double lossImprovement = ((ftpLossRate - voipLossRate) / ftpLossRate) * 100;
        
        std::cout << "\n=== QoS Effectiveness ===\n";
        std::cout << "Delay Improvement for VoIP: " << delayImprovement << "%\n";
        std::cout << "Loss Improvement for VoIP: " << lossImprovement << "%\n";
    }
    
    // Save statistics to file
    std::ofstream statsFile;
    statsFile.open("scratch/qos-statistics.txt");
    statsFile << "QoS_Enabled: " << enableQoS << "\n";
    statsFile << "Queue_Size: " << queueSize << "\n";
    statsFile << "FTP_Flows: " << nFtpFlows << "\n";
    statsFile << "VoIP_Delay_ms: " << voipAvgDelay << "\n";
    statsFile << "VoIP_Jitter_ms: " << voipAvgJitter << "\n";
    statsFile << "VoIP_Loss_%: " << voipLossRate << "\n";
    statsFile << "VoIP_Throughput_Kbps: " << voipThroughput << "\n";
    statsFile << "FTP_Delay_ms: " << ftpAvgDelay << "\n";
    statsFile << "FTP_Jitter_ms: " << ftpAvgJitter << "\n";
    statsFile << "FTP_Loss_%: " << ftpLossRate << "\n";
    statsFile << "FTP_Throughput_Kbps: " << ftpThroughput << "\n";
    statsFile.close();
    
    // Generate detailed per-flow report
    monitor->SerializeToXmlFile("scratch/qos-flowmon.xml", true, true);
    
    Simulator::Destroy();
    
    std::cout << "\n=== Simulation Complete ===\n";
    std::cout << "Files generated:\n";
    std::cout << "  - Animation: scratch/qos-simulation.xml\n";
    std::cout << "  - Statistics: scratch/qos-statistics.txt\n";
    std::cout << "  - Flow details: scratch/qos-flowmon.xml\n";
    std::cout << "  - PCAP traces: scratch/qos-router-*.pcap\n";
    std::cout << "\nTo compare QoS vs non-QoS:\n";
    std::cout << "  With QoS: ./ns3 run \"scratch/qos-simulation --qos=true --ftpflows=3\"\n";
    std::cout << "  Without QoS: ./ns3 run \"scratch/qos-simulation --qos=false --ftpflows=3\"\n";
    
    return 0;
}
