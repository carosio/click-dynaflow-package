// ipc_test.click
// Act as a brigde between interfaces veth1 veth3.
//
// This Click config helps me to understand how to use the IPClassifier element,

ControlSocket("TCP", 12345);

ps :: PaintSwitch
  -> Queue
  -> ToDevice(veth3);

ps[1]
  -> Queue
  -> ToDevice(veth1);

cl :: Classifier(12/0800, -)
  -> Strip(14)
  -> CheckIPHeader
  -> ipc :: IPClassifier((icmp type echo) or (icmp type echo-reply),
                         proto icmp, -);

ipc[0] -> c0 :: Counter -> us :: Unstrip(14) -> ps;
ipc[1] -> c1 :: Counter -> us;
ipc[2] -> c2 :: Counter -> us;

cl[1] -> ps;

FromDevice(veth1)
  -> Paint(0) -> cl;

FromDevice(veth3)
  -> Paint(1) -> cl;
