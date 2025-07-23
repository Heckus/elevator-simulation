The scenario used for this assignment is a simplified setup with the following rules:

Floors are labelled, either with a number (for regular levels) or with a B followed by a number (for basement levels.) Floor numbers increase as you go higher, basement level numbers increase as you go lower, and B1 is connected directly to 1. So in a building that goes from B5 to 10, the floors are: B5 B4 B3 B2 B1 1 2 3 4 5 6 7 8 9 10. Floor numbers go between B99 and 999.

Each elevator car occupies its own elevator shaft and is the only car in that shaft. Elevator cars naturally can't move between shafts.
All elevators on a particular floor are located in the same area and there is 1 call pad per floor.

The call pads use the destination dispatch system - that is, the user presses the number on the call pad indicating which level they want to go to. The call pad then tells the user which elevator has been dispatched (so they know which one to wait by).

Certain elevator cars may be configured to stop at different floors. For example, car A might be configured to go between floors 1 and 10 while car B might be configured to go between floors 1 and 20. A user asking for floor 5 could get either car, depending on what is available, but a user asking for floor 15 will always get car B.