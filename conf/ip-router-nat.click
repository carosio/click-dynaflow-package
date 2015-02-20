// a pure IP router + NAT
// veth1 interface in the private network
// veth3 interface in the public network

ControlSocket("TCP", 12345);

AddressInfo(veth1 10.10.0.1 a6:c3:77:97:2d:c9, veth3 10.20.0.1 52:ab:65:f3:49:6c);

c0 :: Classifier(12/0806 20/0001,
                  12/0806 20/0002,
                  12/0800,
                  -);
 
c1 :: Classifier(12/0806 20/0001,
                  12/0806 20/0002,
                  12/0800,
                  -);

FromDevice(veth1) -> [0]c0;
FromDevice(veth3) -> [0]c1;
out0 :: Queue(200) -> ToDevice(veth1);
out1 :: Queue(200) -> ToDevice(veth3);

ar0 :: ARPResponder(veth1);
ar1 :: ARPResponder(veth3);
aq0 :: ARPQuerier(veth1);
aq1 :: ARPQuerier(veth3);

c0[0] -> ar0 -> out0;
c1[0] -> ar1 -> out1;

c0[1] -> [1]aq0 -> out0;
c1[1] -> [1]aq1 -> out1;

rt :: RangeIPLookup(10.10.0.0/16 0, 10.20.0.0/16 1);

// NAT
IPRewriterPatterns(to_ext_pat veth3 1024-65535 - -); // use all unprivileged ports
nat :: IPClassifier((icmp type echo) or (icmp type echo-reply), proto icmp, -);

nat[0] -> ipr :: ICMPPingRewriter(pattern to_ext_pat 0 1)
ipr[0] -> [0]aq1; // intern -> extern, after rounting
ipr[1] -> [0]rt; // extern -> intern, before routing

nat[2] -> ipr2 :: IPRewriter(pattern to_ext_pat 0 1);
ipr2[0] -> [0]aq1; // intern -> extern, after rounting
ipr2[1] -> [0]rt; // extern -> intern, before routing

nat[1] -> ipp :: ICMPRewriter(ipr2 ipr); // -> ps :: PaintSwitch;
//ipp[1] -> ps;
ipp[0] -> [0]aq1;
ipp[1] -> [0]rt;
//ps[1] -> [0]aq1; // intern -> extern, after routing
//ps[2] -> [0]rt; // extern -> intern, before routing
//ps[0] -> Discard; // make click happy, 0 is not used as color


c0[2] -> Paint(1)
      -> Strip(14)
      -> CheckIPHeader(INTERFACES veth1)
      -> [0]rt;
c1[2] -> Paint(2)
      -> Strip(14)
      -> CheckIPHeader(INTERFACES veth3)
      -> IPClassifier(dst host veth3) // only IP packets with veth3 dst address, else drop it
      -> nat;

c0[3] -> Discard;
c1[3] -> Discard;

rt[0] // go to the private network
      -> DropBroadcasts
      -> pt0 :: PaintTee(1)
      -> ipgwo0 :: IPGWOptions(veth1)
      -> FixIPSrc(veth1)
      -> dipt0 :: DecIPTTL
      -> ipf0 :: IPFragmenter(1500)
      -> [0]aq0;

pt0[1] -> ICMPError(veth1, redirect) -> rt;
ipgwo0[1] -> ICMPError(veth1, parameterproblem) -> rt;
dipt0[1] -> ICMPError(veth1, timeexceeded) -> rt;
ipf0[1] -> ICMPError(veth1, unreachable, needfrag) -> rt;

rt[1] // go to the public network
      -> DropBroadcasts
      -> pt1 :: PaintTee(2)
      -> ipgwo1 :: IPGWOptions(veth3)
      -> FixIPSrc(veth3)
      -> dipt1 :: DecIPTTL
      -> ipf1 :: IPFragmenter(1500) // before aq1 add NAT
      -> nat;

pt1[1] -> ICMPError(veth3, redirect)
      -> p :: Paint(1) // needed for the NAT
      -> rt;
ipgwo1[1] -> ICMPError(veth3, parameterproblem) -> p;
dipt1[1] -> ICMPError(veth3, timeexceeded) -> p;
ipf1[1] -> ICMPError(veth3, unreachable, needfrag) -> p;

