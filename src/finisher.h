// The device a machine states for ending with a status (finisher.cpp, 0.10.1).
#pragma once

// Reads the device tree the firmware handed the program, once, at its start.
void okbsbi_find_finisher(const void* fdt);
// Ends the machine with `status' where the tree stated the device; returns where it stated none.
void okbsbi_finish(int status);
