// ipc_test.click
// Act as a brigde between interfaces veth1 veth3.
//
// This Click config 

ControlSocket("TCP", 12345);

FromDevice(veth1)
  -> ipc :: IPClassifier(icmp type echo, proto icmp, -);

q :: Queue
  -> ToDevice(veth3);

ipc[0] -> c0 :: Counter -> q;
ipc[1] -> c1 :: Counter -> q;
ipc[2] -> c2 :: Counter -> q;

//FromDevice(veth1)
//  -> Queue
//  -> ToDevice(veth3);

FromDevice(veth3)
  -> Queue
  -> ToDevice(veth1);
