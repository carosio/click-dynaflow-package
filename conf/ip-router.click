
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

ip ::   Strip(14)
     -> CheckIPHeader(INTERFACES veth1 veth3)
     -> [0]rt;
c0[2] -> Paint(1) -> ip;
c1[2] -> Paint(2) -> ip;

c0[3] -> Discard;
c1[3] -> Discard;

rt[0]
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

rt[1]
      -> DropBroadcasts
      -> pt1 :: PaintTee(2)
      -> ipgwo1 :: IPGWOptions(veth3)
      -> FixIPSrc(veth3)
      -> dipt1 :: DecIPTTL
      -> ipf1 :: IPFragmenter(1500)
      -> [0]aq1;

pt1[1] -> ICMPError(veth3, redirect) -> rt;
ipgwo1[1] -> ICMPError(veth3, parameterproblem) -> rt;
dipt1[1] -> ICMPError(veth3, timeexceeded) -> rt;
ipf1[1] -> ICMPError(veth3, unreachable, needfrag) -> rt;

