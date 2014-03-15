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
#include "fixed_point_proxy.hh"

#include "adt_op.hh"
#include "adt_op_def.hh"
#include "adt_op_match.hh"
#include "adt_op_replace.hh"
#include "cont_shape_seq.hh"
#include "cont_shape_var.hh"
#include "fixed_point.hh"
#include "symplot.hh"

#include <cl/cl_msg.hh>
#include <cl/cldebug.hh>
#include <cl/storage.hh>

#include <fstream>
#include <iomanip>
#include <map>

#include <boost/foreach.hpp>

namespace FixedPoint {

typedef const struct cl_loc                        *TLoc;
typedef int                                         TFncUid;
typedef std::map<TFncUid, TFnc>                     TFncMap;

typedef const CodeStorage::Block                   *TBlock;

struct StateByInsn::Private {
    TFncMap             visitedFncs;
    TStateMap           stateByInsn;
};

StateByInsn::StateByInsn():
    d(new Private)
{
}

StateByInsn::~StateByInsn()
{
    delete d;
}

bool /* any change */ StateByInsn::insert(const TInsn insn, const SymHeap &sh)
{
    SymStateWithJoin &state = d->stateByInsn[insn];

    if (!state.size()) {
        // update the map of visited functions
        const TFnc fnc = fncByCfg(insn->bb->cfg());
        const TFncUid uid = uidOf(*fnc);
        d->visitedFncs[uid] = fnc;
    }

    return state.insert(sh, /* allowThreeWay */ false);
}

const StateByInsn::TStateMap& StateByInsn::stateMap() const
{
    return d->stateByInsn;
}

struct PlotData {
    std::ostream                   &out;
    StateByInsn::TStateMap         &stateByInsn;
    std::string                     name;

    PlotData(
            std::ostream           &out_,
            StateByInsn::TStateMap &stateByInsn_,
            const std::string      &name_):
        out(out_),
        stateByInsn(stateByInsn_),
        name(name_)
    {
    }
};

#define QUOT(what) "\"" << what << "\""
#define LOC_NODE(locIdx) QUOT("loc" << locIdx)
#define SH_NODE(sh) QUOT("loc" << (sh.first) << "-sh" << (sh.second))
#define DOT_LINK(to) "\"" << to << ".svg\""
#define STD_SETW(n) std::fixed << std::setfill('0') << std::setw(n)

typedef std::set<THeapIdent>                        THeapSet;
typedef AdtOp::TShapeVarByShape                     TVarMap;

void plotInsn(
        THeapSet                   *pHeapSet,
        PlotData                   &plot,
        const TVarMap              &varMap,
        const TLocIdx               locIdx,
        const LocalState           &locState)
{
    const TInsn insn = locState.insn;

    // open cluster
    plot.out << "subgraph \"cluster" << locIdx
        << "\" {\n\tlabel=\"loc #" << locIdx << "\";\n";

    // plot the root node
    plot.out << LOC_NODE(locIdx) << " [label=" << QUOT(*insn)
        << ", tooltip=" << QUOT(insn->loc)
        << ", shape=box, color=blue, fontcolor=blue];\n";

    const SymState &state = locState.heapList;

    const THeapIdx cntHeaps = state.size();
    for (THeapIdx shIdx = 0; shIdx < cntHeaps; ++shIdx) {
        const THeapIdent shIdent(locIdx, shIdx);
        const SymHeap &sh = state[shIdx];
        const TShapeList &shapeList = locState.shapeListByHeapIdx[shIdx];

        TIdSet contShapeIds;
        BOOST_FOREACH(const Shape &shape, shapeList) {
            TObjSet contShapeObjs;
            objSetByShape(&contShapeObjs, sh, shape);
            BOOST_FOREACH(const TObjId obj, contShapeObjs)
                contShapeIds.insert(static_cast<int>(obj));
        }

        std::ostringstream nameStr;
        nameStr << plot.name
            << "-loc" << STD_SETW(4) << locIdx
            << "-sh" << STD_SETW(4) << shIdx;

        // plot the shape graph
        std::string shapeName;
        plotHeap(sh, nameStr.str(), /* loc */ 0, &shapeName, &contShapeIds);

        // plot the link to shape
        plot.out << SH_NODE(shIdent) << " [label=\"sh #" << shIdx;
        const TShapeIdent csIdent(shIdent, /* XXX */ 0);
        const TVarMap::const_iterator it = varMap.find(csIdent);
        if (varMap.end() != it)
            plot.out << " C" << it->second;
        plot.out << "\", URL=" << DOT_LINK(shapeName);

        if (1U == pHeapSet->erase(shIdent))
            // template instance mached on this heap!
            plot.out << ", color=chartreuse2, penwidth=4";
        else {
            const unsigned csCnt = shapeList.size();
            if (csCnt)
                plot.out << ", color=red, penwidth=" << (2U * csCnt);
        }

        plot.out << "];\n";
    }

    // close cluster
    plot.out << "}\n";
}

// XXX
AdtOp::OpCollection adtOps;

void summarizeInsnReplace(
        const AdtOp::TMatchList    &matchList,
        const GlobalState          &progState)
{
    using namespace AdtOp;

    // collect instructions to be replaced
    TInsnListByTplIdx insnsToBeReplaced;
    collectReplacedInsns(&insnsToBeReplaced, matchList, progState);

    const TTemplateIdx tplCnt = insnsToBeReplaced.size();
    for (TTemplateIdx tplIdx = 0; tplIdx < tplCnt; ++tplIdx) {
        const TInsnList &toReplace = insnsToBeReplaced[tplIdx];
        if (toReplace.empty())
            continue;

        const OpTemplate &tpl = adtOps[tplIdx];
        std::ostringstream str;
        str << "template " << tpl.name() << "() would replace locations:";
        BOOST_FOREACH(const TLocIdx insn, toReplace)
            str << " #" << insn;

        const std::string msg = str.str();
        CL_NOTE("[ADT] " << msg);
    }
}

void plotFncCore(PlotData &plot, const GlobalState &fncState)
{
    // match templates
    using namespace AdtOp;
    TMatchList matchList;
    matchTemplates(&matchList, adtOps, fncState);
    selectApplicableMatches(&matchList, fncState);
    summarizeInsnReplace(matchList, fncState);

    TOpList opList;
    if (!collectOpList(&opList, matchList))
        CL_ERROR("[ADT] failed to detect container operations");

    // assign shape variables
    TShapeVarByShape varByShape;
    if (!assignShapeVariables(&varByShape, matchList, opList, adtOps, fncState))
        CL_ERROR("[ADT] failed to assign shape variables");

    // replace container operations
    if (!replaceAdtOps(matchList, opList, adtOps, varByShape, fncState))
        CL_ERROR("[ADT] failed to replace container operations");

    // remove matched heaps not representing any instructions to be replaced
    BOOST_FOREACH(FootprintMatch &fm, matchList) {
        CL_BREAK_IF(fm.matchedHeaps.empty());
        fm.matchedHeaps.pop_back();
    }

    // compute set of matched heaps
    THeapSet heapSet;
    BOOST_FOREACH(FootprintMatch &fm, matchList) {
        CL_BREAK_IF(fm.matchedHeaps.empty());
        BOOST_FOREACH(const THeapIdent heap, fm.matchedHeaps)
            heapSet.insert(heap);
    }

    const TLocIdx locCnt = fncState.size();
    for (TLocIdx locIdx = 0; locIdx < locCnt; ++locIdx) {
        const LocalState &locState = fncState[locIdx];
        plotInsn(&heapSet, plot, varByShape, locIdx, locState);

        // plot trace edges
        BOOST_FOREACH(const TTraceEdgeList &tList, locState.traceOutEdges) {
            BOOST_FOREACH(const TraceEdge *te, tList) {
                plot.out << SH_NODE(te->src) << " -> " << SH_NODE(te->dst);

                const unsigned cnt = te->csMap.size();
                if (cnt) {
                    // we have non-empty container shape mapping for this edge
                    plot.out << " [color=red, fontcolor=red";
                    if (1U < cnt)
                        plot.out << ", label=" << cnt << ", penwidth=" << cnt;
                    plot.out << "]";
                }

                plot.out << ";\n";
            }
        }

        const unsigned cntTargets = locState.cfgOutEdges.size();
        for (unsigned i = 0; i < cntTargets; ++i) {
            const CfgEdge &edge = locState.cfgOutEdges[i];

            const char *color = "blue";
            if (edge.closesLoop)
                // loop-closing edge
                color = "chartreuse2";

            const char *label = "";
            if (2U == cntTargets)
                // assume CL_INSN_COND
                label = (!i) ? "T" : "F";

            plot.out << LOC_NODE(locIdx) << " -> " << LOC_NODE(edge.targetLoc)
                << " [label=" << QUOT(label)
                << ", color=" << color
                << ", fontcolor=" << color << "];\n";
        }
    }
}

void plotFnc(const TFnc fnc, StateByInsn::TStateMap &stateByInsn)
{
    const std::string fncName = nameOf(*fnc);
    std::string plotName("fp-");
    plotName += fncName;

    // create a dot file
    const std::string fileName(plotName + ".dot");
    std::fstream out(fileName.c_str(), std::ios::out);
    if (!out) {
        CL_ERROR("unable to create file '" << fileName << "'");
        return;
    }

    // open graph
    out << "digraph " << QUOT(plotName)
        << " {\n\tlabel=<<FONT POINT-SIZE=\"36\">" << fncName
        << "()</FONT>>;\n\tclusterrank=local;\n\tlabelloc=t;\n";

    // plot the body
    PlotData plot(out, stateByInsn, plotName);
    const GlobalState *fncState = computeStateOf(fnc, stateByInsn);
    plotFncCore(plot, *fncState);
    delete fncState;

    // close graph
    out << "}\n";
    if (!out)
        CL_ERROR("unable to write file '" << fileName << "'");
    out.close();
}

void StateByInsn::plotAll()
{
    if (d->visitedFncs.empty())
        // nothing to plot
        return;

    // obtain a reference to CodeStorage::Storage
    TStorRef stor = *d->visitedFncs.begin()->second->stor;

    // XXX
    AdtOp::loadDefaultOperations(&adtOps, stor);

    BOOST_FOREACH(TFncMap::const_reference fncItem, d->visitedFncs) {
        const TFnc fnc = fncItem.second;
        const TLoc loc = locationOf(*fnc);
        CL_NOTE_MSG(loc, "plotting fixed-point of " << nameOf(*fnc) << "()...");

        plotFnc(fnc, d->stateByInsn);
    }
}

} // namespace FixedPoint
