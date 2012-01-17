/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef _IB_RULE_ENGINE_H_
#define _IB_RULE_ENGINE_H_

/**
 * @file
 * @brief IronBee - Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/types.h>
#include <ironbee/operator.h>
#include <ironbee/mpool.h>
#include <ironbee/engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeRuleEngine Rule Engine
 * @ingroup IronBee
 * @{
 */

/**
 * Rule phase number.
 */
typedef enum {
    PHASE_INVALID = -1,             /**< Invalid; used to terminate list */
    PHASE_NONE,                     /**< No phase */
    PHASE_REQUEST_HEADER,           /**< Request header available. */
    PHASE_REQUEST_BODY,             /**< Request body available. */
    PHASE_RESPONSE_HEADER,          /**< Response header available. */
    PHASE_RESPONSE_BODY,            /**< Response body available. */
    PHASE_POSTPROCESS,              /**< Post-processing phase. */
    PHASE_MAX = PHASE_POSTPROCESS,  /**< Max phase number. */
    IB_RULE_PHASE_COUNT,
} ib_rule_phase_type_t;

/* @todo */
typedef struct ib_operator_instance_t ib_operator_inst_t;

/**
 * Rule engine: Rule
 */
typedef struct ib_rule_t ib_rule_t;

/**
 * Create a rule.
 *
 * Allocates a rule for the rule engine, initializes it.
 *
 * @param ib IronBee engine
 * @param ctx Current IronBee context
 * @param prule Address which new rule is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_create(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      ib_rule_t **prule);

/**
 * Set a rule's operator.
 *
 * @param ib IronBee engine
 * @param rule Rule to operate on
 * @param op Operator instance
 * @param invert Invert operation?
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_operator(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_operator_inst_t *op,
                                            ib_num_t invert);

/**
 * Add an input field to a rule.
 *
 * @param ib IronBee engine
 * @param rule Rule to operate on
 * @param name Input field name.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_input(ib_engine_t *ib,
                                         ib_rule_t *rule,
                                         const char *name);

/**
 * Add a modifier to a rule.
 *
 * @param ib IronBee engine
 * @param rule Rule to operate on
 * @param str Modifier string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_modifier(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            const char *str);

/**
 * Register a rule.
 *
 * Register a rule for the rule engine.
 *
 * @param ib IronBee engine
 * @param ctx Context in which to execute the rule
 * @param rule Rule to register
 * @param phase Phase number in which to execute the rule
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_register(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        ib_rule_t *rule,
                                        ib_rule_phase_type_t phase);

/**
 * Get the memory pool to use for rule allocations.
 *
 * @param ib IronBee engine
 *
 * @returns Pointer to the memory pool to use.
 */
ib_mpool_t DLL_PUBLIC *ib_rule_mpool(ib_engine_t *ib);

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
