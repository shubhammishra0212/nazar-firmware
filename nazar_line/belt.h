// belt.h - belt relay on GPIO26. The module is active LOW: LOW = relay on = belt runs.
// To switch it OFF the pin is made an INPUT (floating), so the module's own pull-up
// takes IN to its 5 V. Driving 3.3 V HIGH is not always enough to turn a 5 V module off.
#pragma once

void beltBegin();         // belt off
void beltSet(bool run);   // only touches the pin when the state changes
bool beltIsOn();
