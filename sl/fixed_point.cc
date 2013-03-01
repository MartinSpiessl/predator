/*
 * Copyright (C) 2013 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "fixed_point.hh"

#include "symtrace.hh"
#include "worklist.hh"

#include <cl/storage.hh>

#include <boost/foreach.hpp>

namespace FixedPoint {

typedef const CodeStorage::Block                   *TBlock;

const THeapIdent InvalidHeap(-1, -1);

bool isTransparentInsn(const TInsn insn)
{
    const enum cl_insn_e code = insn->code;
    switch (code) {
        case CL_INSN_COND:
        case CL_INSN_JMP:
            return true;

        default:
            return false;
    }
}

typedef StateByInsn::TStateMap                      TStateMap;

typedef CleanList<LocalState>                       TStateList;
typedef CleanList<TraceEdge>                        TTraceList;
typedef std::map<TInsn, TLocIdx>                    TInsnLookup;

struct StateBuilderCtx {
    const TFnc          fnc;
    TStateList         &stateList;
    TInsnLookup         insnLookup;

    StateBuilderCtx(const TFnc fnc_, TStateList &stateList_):
        fnc(fnc_),
        stateList(stateList_)
    {
    }
};

void loadHeaps(StateBuilderCtx &ctx, const TStateMap &stateMap)
{
    typedef WorkList<TBlock> TWorkList;

    // traverse the original (block-oriented) control-flow graph
    TBlock bb = ctx.fnc->cfg.entry();
    TWorkList wl(bb);
    while (wl.next(bb)) {
        LocalState *locState;

        // go through instructions of the current basic block
        const TLocIdx insnCnt = bb->size();
        for (TLocIdx insnIdx = 0; insnIdx < insnCnt; ++insnIdx) {
            const TInsn insn = bb->operator[](insnIdx);

            BOOST_FOREACH(const TBlock bbNext, insn->targets)
                // schedule successor blocks for processing
                wl.schedule(bbNext);

            if (isTransparentInsn(insn))
                // skip instruction we do not want in the result
                continue;

            const TLocIdx locIdx = ctx.stateList.size();
            if (insnIdx)
                // update successor location of the _previous_ instruction
                locState->cfgOutEdges.push_back(locIdx);

            // allocate a new location for the current instruction
            locState = new LocalState;
            locState->insn = insn;
            ctx.stateList.append(locState);

            // store the reverse mapping from instructions to locations
            ctx.insnLookup[insn] = locIdx;

            // load heaps if a non-empty fixed-point is available for this loc
            const TStateMap::const_iterator it = stateMap.find(insn);
            if (it != stateMap.end())
                locState->heapList = it->second;
        }
    }
}

GlobalState* computeStateOf(const TFnc fnc, const TStateMap &stateByInsn)
{
    GlobalState *glState = new GlobalState;

    StateBuilderCtx ctx(fnc, glState->stateList_);

    loadHeaps(ctx, stateByInsn);

    return glState;
}

} // namespace FixedPoint
