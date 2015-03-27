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

dfs :: DF_Store;

AddressInfo(
  intern        192.168.110.1   192.168.110.0/24        00:50:ba:85:84:a9,
  extern        192.168.100.1   192.168.100.0/24        00:e0:98:09:ab:af,
  gateway       192.168.100.16,
  intern_server 10.0.0.10
);

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

intern_PEF :: DF_PEFSwitch(accept, drop, deny, unknown, no_action, else);

intern_dst_classify :: DF_GetGroupIP(dfs, -1, dst, IP dst);						// Check if we know the destination IP
intern_dst_classify[0] -> DF_SaveAnno(dfs)								// Now, that we know the Src and Dst group, store the annontation
		       -> intern_PEF;									//    and apply the Policy
intern_dst_classify[1] -> Discard;

intern_src_IP :: DF_GetGroupIP(dfs, -1, src, IP src);
intern_src_IP[0] -> intern_dst_classify;								// We do know the source IP, continue with destination IP
intern_src_IP[1] -> Discard;										// We don't know the source IP, discard the packet

intern_src_MAC :: DF_GetGroupMAC(dfs, src, src);							// We don't have a flow, check if we know the source MAC
intern_src_MAC[0] -> intern_dst_classify;								// We do know the source MAC, continue with destination IP
intern_src_MAC[1] -> intern_src_IP;									// We don't know the source MAC, try to check the source IP

intern_Deny :: Discard;     										// TODO: reply with ICMP Error
intern_Forward :: DropBroadcasts     									// TODO: continue with routing
      -> gio1 :: IPGWOptions(18.26.4.24)
      -> FixIPSrc(18.26.4.24)
      -> dt1 :: DecIPTTL
      -> fr1 :: IPFragmenter(300)
      -> [0]extern_arpq
      -> extern_dev;

// DecIPTTL[1] emits packets with expired TTLs.
// Reply with ICMPs. Rate-limit them?
// dt1[1] -> ICMPError(18.26.4.24, timeexceeded) -> [0]rt;

// Send back ICMP UNREACH/NEEDFRAG messages on big packets with DF set.
// This makes path mtu discovery work.
// fr1[1] -> ICMPError(18.26.7.1, unreachable, needfrag) -> [0]rt;

// Send back ICMP Parameter Problem messages for badly formed
// IP options. Should set the code to point to the
// bad byte, but that's too hard.
// gio1[1] -> ICMPError(18.26.4.24, parameterproblem) -> [0]rt;

// Send back an ICMP redirect if required.
// cp1[1] -> ICMPError(18.26.4.24, redirect, host) -> [0]rt;

intern_noaction :: Discard;     									// TODO: WTF?

intern_PEF[0] -> intern_Forward;									// Accept   -> continue with Forwarding
intern_PEF[1] -> Discard;										// Drop     -> discard the packet
intern_PEF[2] -> intern_Deny;										// Deny     -> return ICMP Error - Admin Prohibited
intern_PEF[3] -> intern_src_MAC;									// Unknown  -> try to find client
intern_PEF[4] -> intern_noaction;									// NoAction -> WTF?
intern_PEF[5] -> Discard;										// Else

intern_arp_class[2] -> Strip(14)
        -> CheckIPHeader
	-> DF_GetFlow(dfs)
	-> intern_PEF;

// extern -> intern

extern_PEF :: DF_PEFSwitch(accept, drop, deny, unknown, no_action, else);

extern_src_classify :: DF_GetGroupIP(dfs, -1, src, IP src);							// Check if we know the source IP
extern_src_classify[0] -> DF_SaveAnno(dfs)								// Now, that we know the Src and Src group, store the annontation
		       -> extern_PEF;									//    and apply the Policy
extern_src_classify[1] -> Discard;

extern_dst_IP :: DF_GetGroupIP(dfs, -1, dst, IP dst);
extern_dst_IP[0] -> extern_src_classify;								// We do know the destination IP, continue with destination IP
extern_dst_IP[1] -> Discard;										// We don't know the destination IP, discard the packet

extern_Deny :: Discard;     										// TODO: reply with ICMP Error
extern_Forward :: DropBroadcasts     									// TODO: continue with routing
      -> gio2 :: IPGWOptions(18.26.4.24)
      -> FixIPSrc(18.26.4.24)
      -> dt2 :: DecIPTTL
      -> fr2 :: IPFragmenter(300)
      -> [0]intern_arpq
      -> intern_dev;

// DecIPTTL[1] emits packets with expired TTLs.
// Reply with ICMPs. Rate-limit them?
// dt1[1] -> ICMPError(18.26.4.24, timeexceeded) -> [0]rt;

// Send back ICMP UNREACH/NEEDFRAG messages on big packets with DF set.
// This makes path mtu discovery work.
// fr1[1] -> ICMPError(18.26.7.1, unreachable, needfrag) -> [0]rt;

// Send back ICMP Parameter Problem messages for badly formed
// IP options. Should set the code to point to the
// bad byte, but that's too hard.
// gio1[1] -> ICMPError(18.26.4.24, parameterproblem) -> [0]rt;

// Send back an ICMP redirect if required.
// cp1[1] -> ICMPError(18.26.4.24, redirect, host) -> [0]rt;

extern_noaction :: Discard;     									// TODO: WTF?

extern_PEF[0] -> extern_Forward;									// Accept   -> continue with Forwarding
extern_PEF[1] -> Discard;										// Drop     -> discard the packet
extern_PEF[2] -> extern_Deny;										// Deny     -> return ICMP Error - Admin Prohibited
extern_PEF[3] -> extern_dst_IP;										// Unknown  -> try to find client
extern_PEF[4] -> extern_noaction;									// NoAction -> WTF?
extern_PEF[5] -> Discard;										// Else

extern_arp_class[2] -> Strip(14)
        -> CheckIPHeader
	-> DF_GetFlow(dfs)
	-> intern_PEF;

