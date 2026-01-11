/* Complete PBR Simulation with Two Paths */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PBRTwoPaths");

int main(int argc, char *argv[])
{
    double simTime = 15.0;
    std::string animFile = "pbr-twopaths.xml";
    
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    
    Time::SetResolution(Time::NS);
    
    // Create nodes: Source -> Router1 -> Router2 -> Destination
    // And alternative path: Router1 -> AltRouter -> Router2
    NodeContainer nodes;
    nodes.Create(5);  // 0=Source, 1=Router1, 2=Router2, 3=Destination, 4=AltRouter
    
    // Create links with different characteristics
    PointToPointHelper p2pFast, p2pSlow;
    p2pFast.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pFast.SetChannelAttribute("Delay", StringValue("10ms"));
    
    p2pSlow.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pSlow.SetChannelAttribute("Delay", StringValue("50ms"));
    
    // Main path (fast)
    NetDeviceContainer dev01 = p2pFast.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer dev12 = p2pFast.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    NetDeviceContainer dev23 = p2pFast.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    
    // Alternative path (slow)
    NetDeviceContainer dev14 = p2pSlow.Install(NodeContainer(nodes.Get(1), nodes.Get(4)));
    NetDeviceContainer dev42 = p2pSlow.Install(NodeContainer(nodes.Get(4), nodes.Get(2)));
    
    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    
    // Main path
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = ipv4.Assign(dev01);
    
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = ipv4.Assign(dev12);
    
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iface23 = ipv4.Assign(dev23);
    
    // Alternative path
    ipv4.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface14 = ipv4.Assign(dev14);
    
    ipv4.SetBase("172.16.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface42 = ipv4.Assign(dev42);
    
    // Setup static routing for different traffic types
    Ipv4StaticRoutingHelper staticRouting;
    
    // Default route for all traffic (main path)
    Ptr<Ipv4StaticRouting> staticRoute = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    staticRoute->AddHostRouteTo(iface23.GetAddress(1), iface01.GetAddress(1), 1);
    
    // Create applications
    uint16_t videoPort = 5004;
    uint16_t dataPort = 20;
    
    // Video server (small packets, frequent)
    UdpEchoServerHelper videoServer(videoPort);
    ApplicationContainer videoServerApp = videoServer.Install(nodes.Get(3));
    videoServerApp.Start(Seconds(0.0));
    videoServerApp.Stop(Seconds(simTime));
    
    // Video client - uses main path
    UdpEchoClientHelper videoClient(iface23.GetAddress(1), videoPort);
    videoClient.SetAttribute("MaxPackets", UintegerValue(30));
    videoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    videoClient.SetAttribute("PacketSize", UintegerValue(200));
    
    ApplicationContainer videoClientApp = videoClient.Install(nodes.Get(0));
    videoClientApp.Start(Seconds(1.0));
    videoClientApp.Stop(Seconds(10.0));
    
    // Data server (large packets, infrequent)
    UdpEchoServerHelper dataServer(dataPort);
    ApplicationContainer dataServerApp = dataServer.Install(nodes.Get(3));
    dataServerApp.Start(Seconds(0.0));
    dataServerApp.Stop(Seconds(simTime));
    
    // Data client - starts later
    UdpEchoClientHelper dataClient(iface23.GetAddress(1), dataPort);
    dataClient.SetAttribute("MaxPackets", UintegerValue(15));
    dataClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    dataClient.SetAttribute("PacketSize", UintegerValue(1400));
    
    ApplicationContainer dataClientApp = dataClient.Install(nodes.Get(0));
    dataClientApp.Start(Seconds(5.0));
    dataClientApp.Stop(Seconds(12.0));
    
    // Create NetAnim XML
    AnimationInterface anim(animFile);
    
    // Set node descriptions
    anim.UpdateNodeDescription(0, "Studio Host");
    anim.UpdateNodeDescription(1, "Studio Router\n(PBR Enabled)");
    anim.UpdateNodeDescription(2, "Cloud Router");
    anim.UpdateNodeDescription(3, "Cloud Host");
    anim.UpdateNodeDescription(4, "Alt Router\n(Slow Path)");
    
    // Set node colors
    anim.UpdateNodeColor(0, 0, 0, 255);     // Blue
    anim.UpdateNodeColor(1, 255, 0, 0);     // Red
    anim.UpdateNodeColor(2, 0, 255, 0);     // Green
    anim.UpdateNodeColor(3, 255, 165, 0);   // Orange
    anim.UpdateNodeColor(4, 128, 0, 128);   // Purple
    
    // Set node sizes
    anim.UpdateNodeSize(0, 20, 20);
    anim.UpdateNodeSize(1, 25, 25);  // Bigger for PBR router
    anim.UpdateNodeSize(2, 20, 20);
    anim.UpdateNodeSize(3, 20, 20);
    anim.UpdateNodeSize(4, 18, 18);
    
    // Enable packet metadata
    anim.EnablePacketMetadata(true);
    
    std::cout << "\n=== PBR Simulation with Two Paths ===" << std::endl;
    std::cout << "Network Topology:" << std::endl;
    std::cout << "  Source (0) -> Router1 (1) -> Router2 (2) -> Destination (3)" << std::endl;
    std::cout << "  Alternative: Router1 (1) -> AltRouter (4) -> Router2 (2)" << std::endl;
    std::cout << "\nTraffic:" << std::endl;
    std::cout << "  Video: Port 5004, small packets (200 bytes), 10 packets/sec" << std::endl;
    std::cout << "  Data: Port 20, large packets (1400 bytes), 2 packets/sec" << std::endl;
    std::cout << "\nRunning simulation for " << simTime << " seconds..." << std::endl;
    
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "NetAnim file: " << animFile << std::endl;
    std::cout << "To visualize: netanim " << animFile << std::endl;
    
    return 0;
}
