/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2019.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include <string.h>
#include <ucs/debug/log.h>
#include <ucs/debug/memtrack.h>
#include <uct/api/uct_def.h>

#include "builtin_plan.h"

#define UCG_PLAN_CALC_CELL_UP(me, sqrt_total, total)                         \
       ((me + total - sqrt_total) % total)
#define UCG_PLAN_CALC_CELL_DOWN(me, sqrt_total, total)                       \
       ((me + sqrt_total) % total)
#define UCG_PLAN_CALC_CELL_LEFT(me, sqrt_total, total)                       \
       ((me % sqrt_total) ? (me + total - 1) % total : me + total)
#define UCG_PLAN_CALC_CELL_RIGHT(me, sqrt_total, total)                      \
       (((me + 1) % sqrt_total) ? (me + 1) % total : (me + total - sqrt_total) % total)

#define UCG_BUILTIN_NEIGHBOR_CONNECT_CELL(dir, ctx, me, dim, total, phs, ep_slot, mock)\
       ucg_builtin_connect(ctx, UCG_PLAN_CALC_CELL##dir                      \
               ((me), (dim), (total)), (phs), (ep_slot), 0, (mock))

ucs_config_field_t ucg_builtin_neighbor_config_table[] = {
    {"DIMENTION", "2", "Neighborhood dimention (e.g. 2, for a 2-D grid)",
     ucs_offsetof(ucg_builtin_neighbor_config_t, dimension), UCS_CONFIG_TYPE_UINT},

    {NULL}
};

ucs_status_t ucg_topo_neighbor_create(ucg_builtin_group_ctx_t *ctx,
        const ucg_builtin_plan_topology_t *topology,
        const ucg_builtin_config_t *config,
        const ucg_group_params_t *group_params,
        const ucg_collective_type_t *coll_type,
        ucg_builtin_plan_t **plan_p)
{
    // TODO: support more dimensions...
    if (config->neighbor.dimension != 2) {
        ucs_error("One 2D neighbor collectives are supported.");
        return UCS_ERR_UNSUPPORTED;
    }

    /* Find the size of a single dimension */
    unsigned dim_size = 1;
    unsigned proc_count = group_params->member_count;
    while (dim_size * dim_size < proc_count) {
        dim_size++;
    }

    /* Sanity check */
    if (dim_size * dim_size != proc_count) {
        ucs_error("Neighbor topology must have proc# a power of the dimension (dim %u procs %u)", 2, proc_count);
        return UCS_ERR_INVALID_PARAM;
    }

    /* Allocate memory resources */
    size_t alloc_size = sizeof(ucg_builtin_plan_t) +
            sizeof(ucg_builtin_plan_phase_t) + 4 * sizeof(uct_ep_h);
    ucg_builtin_plan_t *neighbor =
            (ucg_builtin_plan_t*)UCS_ALLOC_CHECK(alloc_size, "neighbor topology");

    ucg_group_member_index_t total = group_params->member_count;
    ucg_builtin_plan_phase_t *nbr_phs = (ucg_builtin_plan_phase_t*)(neighbor + 1);
    nbr_phs->multi_eps             = (uct_ep_h*)(nbr_phs + 1);
    nbr_phs->method                = UCG_PLAN_METHOD_NEIGHBOR;
    nbr_phs->ep_cnt                = 4;
    neighbor->phs_cnt              = 1;

    ucs_status_t status;
    ucg_group_member_index_t my_index = group_params->member_index;
    int is_mock = coll_type->modifiers & UCG_GROUP_COLLECTIVE_MODIFIER_MOCK_EPS;
    if (((status = UCG_BUILTIN_NEIGHBOR_CONNECT_CELL(_UP, ctx, my_index,
            dim_size, total, nbr_phs, 0, is_mock)) == UCS_OK) ||
        ((status = UCG_BUILTIN_NEIGHBOR_CONNECT_CELL(_DOWN, ctx, my_index,
                dim_size, total, nbr_phs, 1, is_mock)) == UCS_OK) ||
        ((status = UCG_BUILTIN_NEIGHBOR_CONNECT_CELL(_LEFT, ctx, my_index,
                dim_size, total, nbr_phs, 2, is_mock)) == UCS_OK) ||
        ((status = UCG_BUILTIN_NEIGHBOR_CONNECT_CELL(_RIGHT, ctx, my_index,
                dim_size, total, nbr_phs, 3, is_mock)) == UCS_OK)) {
        neighbor->super.my_index = my_index;
        *plan_p = neighbor;
        return UCS_OK;
    }

    return status;
}
