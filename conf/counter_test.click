// counter_test.click
// Act as a brigde between interfaces veth1 veth3.
//
// This Click config should test the correct function of the element DF_Counter.

require(package "dynaflow");

ControlSocket("TCP", 12345);

FromDevice(veth1)
  -> r::RoundRobinSwitch;

c::DF_Counter
  -> Queue
  -> ToDevice(veth3);

r[0] -> DF_SetFlowID(FLOW_ID 0) -> c;
r[1] -> DF_SetFlowID(FLOW_ID 1) -> c;
r[2] -> DF_SetFlowID(FLOW_ID 2) -> c;
r[3] -> DF_SetFlowID(FLOW_ID 3) -> c;


FromDevice(veth3)
  -> DF_SetFlowID(FLOW_ID 2)
  -> DF_Counter
  -> Queue
  -> ToDevice(veth1);
