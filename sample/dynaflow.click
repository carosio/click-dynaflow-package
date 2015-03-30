// dynaflow.click

require(dynaflow)

// This is a minimal DynaFlow Click configuration.
//
// Clients are L2 attached on the 'intern' side, the Internat
// is on the 'extern' side.
//
// Know Bugs:
//   * DHCP request from unknown Clients are Discarded
//   * ARP Requests are handled before we have a policy decission - should we delay that?
//
// Run with 'click dynaflow.click'

// ControlSocket(unix, /tmp/clicksocket);

dfs :: DF_Store;

AddressInfo(
  intern        192.168.110.1   192.168.110.0/24        00:50:ba:85:84:a9,
  extern        192.168.100.1   192.168.100.0/24        00:e0:98:09:ab:af,
  gateway       192.168.100.16,
  intern_server 10.0.0.10
);

// IP routing table. Outputs:
// 0: packets for this machine.
// 1: packets for intern (192.168.110.0/24)
// 2: packets for extern (192.168.100.0/24)
// All other packets are sent to output 2, with 192.168.100.16 as the gateway.
rt :: StaticIPLookup(intern:ip/32 0,
                     extern:ip/32 0,
                     intern:ipnet 1,
                     extern:ipnet 2,
                     0.0.0.0/0 gateway 2);

// DEVICE SETUP

elementclass GatewayDevice {
  $device |
  from :: FromDevice($device)
	-> output;
  input -> q :: Queue(1024)
	-> to :: ToDevice($device);
  ScheduleInfo(from .1, to 1);
}

extern_dev :: GatewayDevice(extern);
intern_dev :: GatewayDevice(intern);

// ARP MACHINERY

// ARP requests are sent to output 0, ARP replies are sent to output 1, IP packets to output 2, and all others to output 3.
extern_arp_class, intern_arp_class
        :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);

intern_arpq :: ARPQuerier(intern);
extern_arpq :: ARPQuerier(extern);

extern_dev -> extern_arp_class;
extern_arp_class[0] -> ARPResponder(extern)     // ARP queries
        	    -> extern_dev;
extern_arp_class[1] -> [1]extern_arpq;          // ARP responses
extern_arp_class[3] -> Discard;

intern_dev -> intern_arp_class;
intern_arp_class[0] -> ARPResponder(intern)     // ARP queries
        	    -> intern_dev;
intern_arp_class[1] -> [1]intern_arpq;          // ARP responses
intern_arp_class[3] -> Discard;

// DynaFlow

// intern -> extern

intern_src_MAC :: DF_GetGroupMAC(dfs, src, src);
intern_src_IP :: DF_GetGroupIP(dfs, -1, src, true, IP src);
intern_src_IP[1] -> Discard;										// We don't know the source IP, discard the packet

policy_from_intern :: DF_GetFlow(dfs)
      -> intern_PEF_1st :: DF_PEFSwitch(unknown, else)
      -> intern_src_MAC => (										// We don't have a flow, check if we know the source MAC
       	 input[0] -> output;
	 input[1] -> intern_src_IP -> output								// We don't know the source MAC, try to check the source IP
      )
      -> intern_dst_classify :: DF_GetGroupIP(dfs, -1, dst, IP dst)					// Check if we know the destination IP
      -> DF_SetAction(dfs)
      -> DF_SaveAnno(dfs)										// Now, that we know the Src and Dst group, store the annontation
      -> intern_PEF :: DF_PEFSwitch(accept, drop, deny, unknown, no_action, else);			//    and apply the Policy

intern_PEF_1st[1] -> intern_PEF;									// Else

intern_dst_classify[1] -> Discard;

intern_Deny :: Discard;     										// TODO: reply with ICMP Error

intern_noaction :: Discard;     									// TODO: WTF?
intern_unknown :: Discard;     										// TODO: WTF?

intern_PEF[0] -> [0]rt;											// Accept   -> continue with Forwarding
intern_PEF[1] -> Discard;										// Drop     -> discard the packet
intern_PEF[2] -> intern_Deny;										// Deny     -> return ICMP Error - Admin Prohibited
intern_PEF[3] -> intern_unknown;									// Unknown  -> try to find client
intern_PEF[4] -> intern_noaction;									// NoAction -> WTF?
intern_PEF[5] -> Discard;										// Else

intern_arp_class[2] -> Strip(14)
        -> CheckIPHeader
	-> policy_from_intern;

// extern -> intern

extern_Deny :: Discard;     										// TODO: reply with ICMP Error

// output on interface INTERN
out1 :: DropBroadcasts
      -> gio1 :: IPGWOptions(intern)
      -> FixIPSrc(intern)
      -> dt1 :: DecIPTTL
      -> fr1 :: IPFragmenter(1500)
      -> [0]intern_arpq
      -> intern_dev;

// output on interface EXTERN
out2 :: DropBroadcasts
      -> gio2 :: IPGWOptions(extern)
      -> FixIPSrc(extern)
      -> dt2 :: DecIPTTL
      -> SetIPAddress(gateway)
      -> fr2 :: IPFragmenter(1500)
      -> [0]extern_arpq
      -> extern_dev;

policy_to_intern :: DF_GetFlow(dfs)
      -> extern_PEF_1st :: DF_PEFSwitch(unknown, else)
      -> extern_dst_IP :: DF_GetGroupIP(dfs, -1, dst, true, IP dst)
      -> extern_src_classify :: DF_GetGroupIP(dfs, -1, src, IP src)					// Check if we know the source IP
      -> DF_SaveAnno(dfs)										// Now, that we know the Src and Src group, store the annontation
      -> extern_PEF :: DF_PEFSwitch(accept, drop, deny, unknown, no_action, else);			//    and apply the Policy

rt[0] -> Discard;											// TODO: deliver to localhost
rt[1] -> policy_to_intern;
rt[2] -> out2;

// DecIPTTL[1] emits packets with expired TTLs.
// Reply with ICMPs. Rate-limit them?
dt1[1] -> ICMPError(intern, timeexceeded) -> [0]rt;
dt2[1] -> ICMPError(extern, timeexceeded) -> [0]rt;

// Send back ICMP UNREACH/NEEDFRAG messages on big packets with DF set.
// This makes path mtu discovery work.
fr1[1] -> ICMPError(extern, unreachable, needfrag) -> [0]rt;
fr2[1] -> ICMPError(intern, unreachable, needfrag) -> [0]rt;

// Send back ICMP Parameter Problem messages for badly formed
// IP options. Should set the code to point to the
// bad byte, but that's too hard.
gio1[1] -> ICMPError(intern, parameterproblem) -> [0]rt;
gio2[1] -> ICMPError(extern, parameterproblem) -> [0]rt;

// Send back an ICMP redirect if required.
// cp1[1] -> ICMPError(extern, redirect, host) -> [0]rt;
// cp2[1] -> ICMPError(intern, redirect, host) -> [0]rt;

extern_PEF_1st[1] -> extern_PEF;									// Else

extern_dst_IP[1] -> Discard;										// We don't know the destination IP, discard the packet
extern_src_classify[1] -> Discard;									// We don't know the source IP, discard the packet

extern_noaction :: Discard;     									// TODO: WTF?
extern_unknown :: Discard;

extern_PEF[0] -> out1;											// Accept   -> continue with Forwarding
extern_PEF[1] -> Discard;										// Drop     -> discard the packet
extern_PEF[2] -> extern_Deny;										// Deny     -> return ICMP Error - Admin Prohibited
extern_PEF[3] -> extern_unknown;										// Unknown  -> try to find client
extern_PEF[4] -> extern_noaction;									// NoAction -> WTF?
extern_PEF[5] -> Discard;										// Else

extern_arp_class[2] -> Strip(14)
        -> CheckIPHeader
	-> [0]rt;

