#include <relay_handler.h>
const int relayPin = 7;

void relaySetup() {
  pinMode(relayPin, OUTPUT);
}
void setRelay(int state){
  digitalWrite(relayPin, state);
}