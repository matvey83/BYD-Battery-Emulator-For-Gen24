#ifndef CELLMONITOR_JS_H
#define CELLMONITOR_JS_H

// The pure helpers of the cell monitor page script, kept apart from the page so that the unit test
// (test/battery/CellMonitorJsTest.cpp) can run exactly this text under node. They touch no DOM:
// everything comes in as arguments. Plain function declarations, no exports, since the page loads
// this as its own script element ahead of the page script.
//
// m is the pack's bricks per module (the "m" of the cell JSON), or undefined when unknown.
//
// modulesRow(m): the line under Delta, empty when the grouping is unknown.
//
// columns(n, m): the graph columns for n drawn cells. A cell's column holds its number, and a module
// boundary gets a column of -1 (the gap), but only when there are at least 2 modules and they add
// up to exactly n. Otherwise it is one column per cell.
//
// cellAt(q, f): the cell number at fraction f (0 to 1) of the graph's width, or undefined/-1 over a
// gap or outside the graph.
static const char cellmonitor_logic_js[] = R"js(
function modulesRow(m){return m?'<br>Modules: '+m.length+' ('+m.join(' / ')+' bricks)':''}
function columns(n,m){const s=m&&m.length>1&&m.reduce((a,x)=>a+x)==n&&m,q=[];let u=0,w=s?s[0]:-1;for(let i=0;i<n;i++){if(i==w){q.push(-1);w+=s[++u]}q.push(i)}return q}
function cellAt(q,f){return q[f*q.length|0]}
)js";

#endif
