#pragma once

#include "members.h"

// Manage Running Threads
void start_background(member* m);
void stop_background(member* m);
void wait_background(member* m);
void stop_wait_all_background();
