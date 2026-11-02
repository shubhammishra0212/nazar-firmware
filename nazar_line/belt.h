// belt.h - belt relay on GPIO26.
// RELAY_ACTIVE_HIGH (config.h, our BC547 board): HIGH = relay on = belt runs, LOW = belt stops.
// Active-LOW modules instead: LOW = run, and to stop the pin is made an INPUT (floating) so the
// module's own pull-up takes IN to its 5 V - driving 3.3 V HIGH is not always enough to release one.
#pragma once

void beltBegin();         // belt off
void beltSet(bool run);   // only touches the pin when the state changes
bool beltIsOn();
