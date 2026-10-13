#include "lvgl.h"

#include "wellness.h"

// Rules: second-person, warm, tiny, and never about what the person is
// feeling or how they are coping. Each stands on its own at a glance.
static const char *const k_lines[] = {
    "Take a breath.",
    "Shoulders down.",
    "One thing at a time.",
    "You've got this.",
    "Look away for a moment.",
    "Hydrate?",
    "Unclench your jaw.",
    "Roll your shoulders.",
    "Look at something far away.",
    "Five slow breaths?",
    "A short walk is a fine strategy.",
    "Stretch your neck. Slowly.",
    "Sit tall. Soften your hands.",
    "Water, then the next thing.",
    "Pause. Then decide.",
};

#define LINE_COUNT (sizeof(k_lines) / sizeof(k_lines[0]))

const char *wellness_next(void)
{
    static int s_last = -1;
    int i;
    do {
        i = (int)lv_rand(0, LINE_COUNT - 1);
    } while (i == s_last);
    s_last = i;
    return k_lines[i];
}

const char *wellness_checkin_reply(int answer)
{
    switch (answer) {
    case 0:
        return "Keep that energy.";
    case 1:
        return "One thing at a time.";
    default:
        return "Take five. The world can wait.";
    }
}
