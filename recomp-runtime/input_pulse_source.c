#include "input_pulse_source.h"

void recomp_input_pulse_source_init(
    RecompInputPulseSource *source,
    RecompInputSampleSource base,
    uint64_t pulse_poll)
{
    if (source == NULL) {
        return;
    }
    source->base = base;
    source->pulse_poll_count = 0u;
    source->a_poll_count = 0u;
    source->buttons_poll_count = 0u;
    source->analog_poll_count = 0u;
    source->sample_count = 0u;
    source->paused = false;
    source->pending_analog_index = 0u;
    source->pending_samples = 0u;
    source->pending_buttons_mask = 0u;
    (void)recomp_input_pulse_source_add_poll(source, pulse_poll);
}

bool recomp_input_pulse_source_add_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll)
{
    if (source == NULL || pulse_poll == 0u ||
        source->pulse_poll_count >= RECOMP_INPUT_PULSE_POLL_CAPACITY) {
        return false;
    }
    source->pulse_polls[source->pulse_poll_count] = pulse_poll;
    ++source->pulse_poll_count;
    return true;
}

bool recomp_input_pulse_source_add_a_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll)
{
    if (source == NULL || pulse_poll == 0u ||
        source->a_poll_count >= RECOMP_INPUT_A_PULSE_POLL_CAPACITY) {
        return false;
    }
    source->a_polls[source->a_poll_count] = pulse_poll;
    ++source->a_poll_count;
    return true;
}

bool recomp_input_pulse_source_add_buttons_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint16_t mask)
{
    return recomp_input_pulse_source_add_buttons_hold(
        source, pulse_poll, pulse_poll, mask);
}

bool recomp_input_pulse_source_add_buttons_hold(
    RecompInputPulseSource *source,
    uint64_t first_poll,
    uint64_t last_poll,
    uint16_t mask)
{
    if (source == NULL || first_poll == 0u || last_poll < first_poll ||
        source->buttons_poll_count >= RECOMP_INPUT_BUTTONS_PULSE_CAPACITY) {
        return false;
    }
    source->buttons_polls[source->buttons_poll_count] = first_poll;
    source->buttons_lasts[source->buttons_poll_count] = last_poll;
    source->buttons_masks[source->buttons_poll_count] = mask;
    ++source->buttons_poll_count;
    return true;
}

bool recomp_input_pulse_source_add_analog_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint8_t index,
    uint8_t value)
{
    return recomp_input_pulse_source_add_analog_hold(
        source, pulse_poll, pulse_poll, index, value);
}

bool recomp_input_pulse_source_add_analog_hold(
    RecompInputPulseSource *source,
    uint64_t first_poll,
    uint64_t last_poll,
    uint8_t index,
    uint8_t value)
{
    if (source == NULL || first_poll == 0u || last_poll < first_poll ||
        index >= RECOMP_INPUT_ANALOG_BUTTON_COUNT ||
        source->analog_poll_count >= RECOMP_INPUT_ANALOG_PULSE_CAPACITY) {
        return false;
    }
    source->analog_polls[source->analog_poll_count] = first_poll;
    source->analog_lasts[source->analog_poll_count] = last_poll;
    source->analog_indices[source->analog_poll_count] = index;
    source->analog_values[source->analog_poll_count] = value;
    ++source->analog_poll_count;
    return true;
}

bool recomp_input_pulse_source_press_analog(
    RecompInputPulseSource *source,
    uint8_t index)
{
    if (source == NULL || index >= RECOMP_INPUT_ANALOG_BUTTON_COUNT ||
        source->pending_samples != 0u) {
        return false;
    }
    source->pending_analog_index = index;
    source->pending_buttons_mask = 0u;
    source->pending_samples = 7u;
    return true;
}

bool recomp_input_pulse_source_press_buttons(
    RecompInputPulseSource *source,
    uint16_t mask)
{
    if (source == NULL || mask == 0u || (mask & 0xff00u) != 0u ||
        source->pending_samples != 0u) {
        return false;
    }
    source->pending_buttons_mask = mask;
    source->pending_samples = 7u;
    return true;
}

static void sample_pending_press(
    RecompInputPulseSource *source,
    RecompInputGamepad *gamepad)
{
    if (source->pending_samples != 0u) {
        --source->pending_samples;
        if (source->pending_buttons_mask != 0u) {
            if (source->pending_samples != 0u) {
                gamepad->buttons |= source->pending_buttons_mask;
            } else {
                gamepad->buttons &= (uint16_t)~source->pending_buttons_mask;
            }
        } else {
            gamepad->analog_buttons[source->pending_analog_index] =
                source->pending_samples != 0u
                    ? RECOMP_INPUT_ANALOG_PRESSED : 0u;
        }
    }
}

/* Applies every event armed for a poll in (after, through]; a hold is armed on
   each poll of its range. Events armed for the same poll all apply: button
   masks combine, the later analog value wins. */
static void apply_armed(
    const RecompInputPulseSource *source,
    uint64_t after,
    uint64_t through,
    RecompInputGamepad *gamepad)
{
#define ARMED(poll) ((poll) > after && (poll) <= through)
#define HELD(first, last) ((first) <= through && (last) > after)
    for (unsigned i = 0u; i < source->pulse_poll_count; ++i) {
        if (ARMED(source->pulse_polls[i])) {
            gamepad->buttons |= RECOMP_INPUT_BUTTON_START;
        }
    }
    for (unsigned i = 0u; i < source->a_poll_count; ++i) {
        if (ARMED(source->a_polls[i])) {
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] =
                RECOMP_INPUT_ANALOG_PRESSED;
        }
    }
    for (unsigned i = 0u; i < source->buttons_poll_count; ++i) {
        if (HELD(source->buttons_polls[i], source->buttons_lasts[i])) {
            gamepad->buttons |= source->buttons_masks[i];
        }
    }
    for (unsigned i = 0u; i < source->analog_poll_count; ++i) {
        if (HELD(source->analog_polls[i], source->analog_lasts[i])) {
            gamepad->analog_buttons[source->analog_indices[i]] =
                source->analog_values[i];
        }
    }
#undef HELD
#undef ARMED
}

bool recomp_input_pulse_source_sample(
    RecompInputPulseSource *source,
    RecompInputGamepad *gamepad)
{
    if (source == NULL || gamepad == NULL) {
        return false;
    }
    if (source->paused) {
        *gamepad = (RecompInputGamepad){0};
        sample_pending_press(source, gamepad);
        return true;
    }
    if (source->base == NULL) {
        *gamepad = (RecompInputGamepad){0};
    } else if (!source->base(0u, gamepad)) {
        return false;
    }
    ++source->sample_count;
    apply_armed(source, source->sample_count - 1u, source->sample_count, gamepad);
    sample_pending_press(source, gamepad);
    return true;
}

bool recomp_input_pulse_source_sample_at(
    RecompInputPulseSource *source,
    uint64_t poll,
    RecompInputGamepad *gamepad)
{
    if (source == NULL || gamepad == NULL) {
        return false;
    }
    *gamepad = (RecompInputGamepad){0};
    const uint64_t after = poll != 0u && poll == source->sample_count
        ? poll - 1u : source->sample_count;
    apply_armed(source, after, poll, gamepad);
    source->sample_count = poll;
    return true;
}
