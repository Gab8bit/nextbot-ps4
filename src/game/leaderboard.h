// Leaderboard: best survival time per PNG/bot. This is the game's only saved data.
// File: <root>/leaderboard.dat, CSV text:
//     NEXTBOT_LB,1
//     <user>,<png>,<milliseconds>
// Missing, empty or corrupt files give an empty leaderboard (bad lines are skipped).
#pragma once
#include <stdint.h>

constexpr int LB_USER_MAX = 33;   // buffer sizes including the terminator
constexpr int LB_PNG_MAX = 65;
constexpr int LB_MAX_RECORDS = 256;

struct LbRecord {
    char user[LB_USER_MAX];
    char png[LB_PNG_MAX];
    uint32_t ms;
};

void leaderboard_load();
bool leaderboard_save();
// Records a finished run. Returns true if it is a new best for that PNG (or its first record);
// in that case the file is saved immediately.
bool leaderboard_submit(const char* user, const char* png, uint32_t ms);
int leaderboard_count();
const LbRecord* leaderboard_get(int i);   // sorted by time, best first
const LbRecord* leaderboard_find(const char* png);
// Empties the list and deletes the save file. Returns false if the file could not be removed.
bool leaderboard_reset();

// Copies src into dst (size dstLen) replacing characters that would break the CSV
// (commas, quotes, line breaks, control chars) with '_'. Truncates on a UTF-8 boundary.
void leaderboard_sanitize(char* dst, int dstLen, const char* src);
// "2 min 6 sec" or, under a minute, "6 sec".
void format_survival_time(uint32_t ms, char* buf, int len);
