/*!
 * \file system-fsm-time.h
 * \author Alessandro Fascini
 * \brief Private elapsed-time helpers of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_TIME_H
#define RECRUITING_SYSTEM_FSM_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/*!
 * \brief Check whether a duration has elapsed since a timestamp.
 *
 * Correct across a wrap-around of the tick, as long as the real elapsed time
 * is below 2^32 ms (about 49 days).
 *
 * \param now_ms      The current tick.
 * \param last_ms     The timestamp to measure from.
 * \param duration_ms The duration to check.
 * \return true if at least duration_ms have passed since last_ms.
 */
static inline bool int_system_fsm_has_passed_ms(uint32_t now_ms, uint32_t last_ms, uint32_t duration_ms) {
    /* unsigned subtraction: still correct when the tick wraps around */
    const uint32_t elapsed_ms = now_ms - last_ms;
    return elapsed_ms >= duration_ms;
}

/*!
 * \brief Periodic check: like int_system_fsm_has_passed_ms(), and restart the period when it fires.
 * \param now_ms      The current tick.
 * \param last_ms     The start of the current period; set to now_ms when the duration has passed. Must not be NULL.
 * \param duration_ms The period.
 * \return true once per period, when at least duration_ms have passed since *last_ms.
 */
static inline bool int_system_fsm_has_passed_ms_and_update(uint32_t now_ms, uint32_t *last_ms, uint32_t duration_ms) {
    assert(last_ms != NULL);
    if (int_system_fsm_has_passed_ms(now_ms, *last_ms, duration_ms)) {
        *last_ms = now_ms;
        return true;
    }
    return false;
}

#endif
