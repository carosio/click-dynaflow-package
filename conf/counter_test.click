// counter_test.click
// Act as a brigde between interfaces veth1 veth3.
//
// This Click config should test the correct function of the element TP_Counter.

require(package "travelping");

ControlSocket("TCP", 12345);

FromDevice(veth1)
  -> r::RoundRobinSwitch;

c::TP_Counter
  -> Queue
  -> ToDevice(veth3);

r[0] -> TP_SetFlowID(FLOW_ID 0) -> c;
r[1] -> TP_SetFlowID(FLOW_ID 1) -> c;
r[2] -> TP_SetFlowID(FLOW_ID 2) -> c;
r[3] -> TP_SetFlowID(FLOW_ID 3) -> c;


FromDevice(veth3)
  -> TP_SetFlowID(FLOW_ID 2)
  -> TP_Counter
  -> Queue
  -> ToDevice(veth1);
