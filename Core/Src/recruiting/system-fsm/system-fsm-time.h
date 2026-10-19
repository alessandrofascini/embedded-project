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

static inline bool int_system_fsm_has_passed_ms(uint32_t now_ms, uint32_t last_ms, uint32_t duration_ms) {
    /* unsigned subtraction: still correct when the tick wraps around */
    const uint32_t elapsed_ms = now_ms - last_ms;
    return elapsed_ms >= duration_ms;
}

static inline bool int_system_fsm_has_passed_ms_and_update(uint32_t now_ms, uint32_t *last_ms, uint32_t duration_ms) {
    assert(last_ms != NULL);
    if (int_system_fsm_has_passed_ms(now_ms, *last_ms, duration_ms)) {
        *last_ms = now_ms;
        return true;
    }
    return false;
}

#endif
