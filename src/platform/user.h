// PS4 user: initializes libSceUserService and reads the logged-in user's name.
#pragma once

// Initializes the user service; returns the initial (logged-in) user id, or -1.
int user_init();
// Copies the user name (UTF-8) into buf. Falls back to "Player" if it cannot be read.
// Returns true if the real name was obtained.
bool user_get_name(int userId, char* buf, int len);
void user_shutdown();
