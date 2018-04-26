/*
 * Copyright (c) 2018
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 */

#include "ROP_AbcHierarchy.h"

#include "ROP_AbcNodeInstance.h"

ROP_AbcHierarchy::Node *
ROP_AbcHierarchy::Node::setChild(
    const std::string &name,
    GABC_OError &err,
    const UT_Matrix4D &m,
    const std::string &up_vals,
    const std::string &up_meta)
{
    auto &xform = myChildren[name];
    if(!xform.myAbcNode)
    {
	std::string temp_name = name;
	myAbcNode->makeCollisionFreeName(temp_name, err);
	ROP_AbcNodeXform *child = new ROP_AbcNodeXform(temp_name);
	myAbcNode->addChild(child);
	xform.myAbcNode = child;
    }
    auto node = static_cast<ROP_AbcNodeXform *>(xform.myAbcNode);
    node->setData(m);
    node->setUserProperties(up_vals, up_meta);
    return &xform;
}

void
ROP_AbcHierarchy::Node::setShape(
    const std::string &name,
    int type,
    exint i,
    GABC_OError &err,
    const GT_PrimitiveHandle &prim,
    bool visible,
    const std::string &up_vals,
    const std::string &up_meta,
    const std::string &subd_grp)
{
    auto &shapes = myShapes[name][type];
    while(i >= shapes.entries())
    {
	std::string temp_name = name;
	myAbcNode->makeCollisionFreeName(temp_name, err);
	ROP_AbcNodeShape *child = new ROP_AbcNodeShape(temp_name);
	myAbcNode->addChild(child);
	shapes.append(child);
    }
    auto shape = shapes(i);
    shape->setData(prim, visible, subd_grp);
    shape->setUserProperties(up_vals, up_meta);
}

ROP_AbcNodeShape *
ROP_AbcHierarchy::Node::newInstanceSource(
    const std::string &name,
    int type,
    exint key2,
    GABC_OError &err,
    const GT_PrimitiveHandle &prim,
    const std::string &up_vals,
    const std::string &up_meta,
    const std::string &subd_grp)
{
    auto &shapes = myInstancedShapes[name][type];
    shapes.append(key2);

    std::string temp_name = name;
    myAbcNode->makeCollisionFreeName(temp_name, err);
    ROP_AbcNodeShape *child = new ROP_AbcNodeShape(temp_name);
    myAbcNode->addChild(child);
    child->setData(prim, true, subd_grp);
    child->setUserProperties(up_vals, up_meta);
    return child;
}

void
ROP_AbcHierarchy::Node::newInstanceRef(
    const std::string &name,
    int type,
    exint key2,
    GABC_OError &err,
    ROP_AbcNodeShape *src)
{
    auto &shapes = myInstancedShapes[name][type];
    shapes.append(key2);

    std::string temp_name = name;
    myAbcNode->makeCollisionFreeName(temp_name, err);
    ROP_AbcNodeInstance *child = new ROP_AbcNodeInstance(temp_name, src);
    myAbcNode->addChild(child);
}

void
ROP_AbcHierarchy::Node::setVisibility(bool vis)
{
    UT_ASSERT(myAbcNode->getParent());
    static_cast<ROP_AbcNodeXform *>(myAbcNode)->setVisibility(vis);
}

void
ROP_AbcHierarchy::merge(
    GABC_OError &err,
    const ROP_AbcHierarchySample &src,
    const ROP_AbcHierarchySample::InstanceMap &instance_map)
{

    // assign existing instances if possible
    UT_Map<std::tuple<int, std::string, exint>, exint> mapping;
    UT_Map<exint, std::tuple<int, std::string, exint> > rev_mapping;

    UT_Array<std::tuple<const ROP_AbcHierarchySample *, Node *> > work;
    work.append(std::make_tuple(&src, &myRoot));
    while(work.entries())
    {
	// get work item
	auto &item = work.last();
	auto n1 = std::get<0>(item);
	auto n2 = std::get<1>(item);
	work.removeLast();

	// for each child instance
	auto &named_shapes2 = n2->getInstancedShapes();
	for(auto &it : n1->getInstancedShapes())
	{
	    const std::string &name = it.first;
	    auto it2 = named_shapes2.find(name);
	    if(it2 == named_shapes2.end())
		continue;

	    // for each primitive type
	    for(auto &it3 : it.second)
	    {
		int type = it3.first;
		auto it4 = it2->second.find(type);
		if(it4 == it2->second.end())
		    continue;

		auto &keys2 = it4->second;
		exint i2 = 0;

		// find unprocessed instance
		auto &keys1 = it3.second;
		for(exint i = 0; i < keys1.entries(); ++i)
		{
		    auto &key = keys1(i);
		    auto key1 = std::make_tuple(type, std::get<0>(key), std::get<1>(key));

		    if(mapping.find(key1) != mapping.end())
			continue;

		    // look for next unassigned instance
		    for(; i2 < keys2.entries(); ++i2)
		    {
			auto key2 = keys2(i2);
			if(rev_mapping.find(key2) == rev_mapping.end())
			{
			    // assign instance
			    mapping.emplace(key1, key2);
			    rev_mapping.emplace(key2, key1);
			    break;
			}
		    }
		}
	    }
	}

	// add children to work list
	auto &children2 = n2->getChildren();
	for(auto &it : n1->getChildren())
	{
	    auto it2 = children2.find(it.first);
	    if(it2 == children2.end())
		continue;

	    work.append(std::make_tuple(&it.second, &it2->second));
	}
    }

    // process each node
    bool root_vis_warned = false;
    UT_Set<exint> updated_instances;
    UT_Array<std::tuple<const ROP_AbcHierarchySample *, Node *, bool> > work2;
    work2.append(std::make_tuple(&src, &myRoot, true));
    while(work2.entries())
    {
	// get work item
	auto &item = work2.last();
	auto n1 = std::get<0>(item);
	auto n2 = std::get<1>(item);
	bool visible = std::get<2>(item);
	work2.removeLast();

	bool should_be_visible = false;
	bool should_be_invisible = !visible;

	UT_SortedSet<std::string> names;
	if(n1)
	{
	    // export shapes
	    for(auto &it : n1->getShapes())
	    {
		auto &name = it.first;
		for(auto &it2 : it.second)
		{
		    int type = it2.first;
		    auto &shapes = it2.second;

		    exint n = shapes.entries();
		    for(exint i = 0; i < n; ++i)
		    {
			auto &shape = shapes(i);
			bool vis = std::get<1>(shape);
			auto &up_vals = std::get<2>(shape);
			auto &up_meta = std::get<3>(shape);
			auto &subd_grp = std::get<4>(shape);
			if(vis)
			    should_be_visible = true;

			n2->setShape(name, type, i, err, std::get<0>(shape),
				     vis, up_vals, up_meta, subd_grp);
		    }
		}
	    }

	    for(auto &it : n1->getInstancedShapes())
		names.insert(it.first);
	}

	for(auto &it : n2->getInstancedShapes())
	    names.insert(it.first);

	// export instances
	auto &inst_shapes2 = n2->getInstancedShapes();
	for(auto &name : names)
	{
	    UT_SortedSet<int> types;

	    const UT_Map<int, UT_Array<std::tuple<std::string, exint, bool,
		std::string, std::string,std::string> > > *n1data = nullptr;
	    if(n1)
	    {
		auto &inst_shapes = n1->getInstancedShapes();
		auto it = inst_shapes.find(name);
		if(it != inst_shapes.end())
		{
		    n1data = &it->second;
		    for(auto &it2 : it->second)
			types.insert(it2.first);
		}
	    }

	    const UT_Map<int, UT_Array<exint> > *n2data = nullptr;
	    {
		auto it = inst_shapes2.find(name);
		if(it != inst_shapes2.end())
		{
		    n2data = &it->second;
		    for(auto &it2 : it->second)
			types.insert(it2.first);
		}
	    }

	    for(int type : types)
	    {
		const UT_Array<std::tuple<std::string, exint, bool,
		    std::string, std::string, std::string> > *n1data2 = nullptr;
		if(n1data)
		{
		    auto it = n1data->find(type);
		    if(it != n1data->end())
			n1data2 = &it->second;
		}

		const UT_Array<exint> *n2data2 = nullptr;
		if(n2data)
		{
		    auto it = n2data->find(type);
		    if(it != n2data->end())
			n2data2 = &it->second;
		}

		UT_Map<exint, exint> unvisited;
		if(n2data2)
		{
		    exint n = n2data2->entries();
		    for(exint i = 0; i < n; ++i)
			unvisited[(*n2data2)(i)] += 1;
		}

		if(n1data2)
		{
		    exint n = n1data2->entries();
		    for(exint i = 0; i < n; ++i)
		    {
			auto &inst = (*n1data2)(i);
			auto &key1 = std::get<0>(inst);
			exint idx1 = std::get<1>(inst);
			bool vis = std::get<2>(inst);
			auto &up_vals = std::get<3>(inst);
			auto &up_meta = std::get<4>(inst);
			auto &subd_grp = std::get<5>(inst);
			auto &prim = instance_map.find(key1)->second.find(type)->second(idx1);

			if(vis)
			    should_be_visible = true;
			else
			    should_be_invisible = true;

			auto key = std::make_tuple(type, key1, idx1);

			auto it = mapping.find(key);
			if(it == mapping.end())
			{
			    // allocate new instance
			    exint key2 = myNextInstanceId++;
			    myInstancedShapes[type][key2] = 
				    n2->newInstanceSource(name, type, key2,
							  err, prim,
							  up_vals, up_meta,
							  subd_grp);
			    updated_instances.insert(key2);

			    mapping.emplace(key, key2);
			    rev_mapping.emplace(key2, key);
			}
			else
			{
			    exint key2 = it->second;

			    if(!updated_instances.contains(key2))
			    {
				// update instance
				auto shape = myInstancedShapes[type][key2];
				shape->setData(prim, true, subd_grp);
				shape->setUserProperties(up_vals, up_meta);
				updated_instances.insert(key2);
			    }

			    auto it2 = unvisited.find(key2);
			    if(it2 == unvisited.end())
			    {
				// create new reference
				n2->newInstanceRef(name, type, key2, err,
					myInstancedShapes[type][key2]);
			    }
			    else if(--it2->second == 0)
				unvisited.erase(key2);
			}
		    }
		}

		if(!unvisited.empty())
		    should_be_invisible = true;
	    }
	}

	if(should_be_visible && should_be_invisible)
	{
	    if(!root_vis_warned)
	    {
		err.warning("Cannot set hide some instanced geometry.");
		root_vis_warned = true;
	    }
	}

	if(!should_be_visible && should_be_invisible)
	    visible = false;

	if(n1 != &src)
	    n2->setVisibility(visible);
	else if(!visible)
	{
	    if(!root_vis_warned)
	    {
		err.warning("Cannot set hide some instanced geometry.");
		root_vis_warned = true;
	    }
	}

	// add children to work list
	auto &children1 = n1->getChildren();
	auto &children2 = n2->getChildren();
	for(auto it = children1.rbegin(); it != children1.rend(); ++it)
	{
	    auto &node = it->second;
	    auto child =
		n2->setChild(it->first, err,
			     node.getPreXform() * node.getXform(),
			     node.getUserPropsVals(), node.getUserPropsMeta());
	    work2.append(std::make_tuple(&node, child, visible));
	}

	// hide all non-exported children
	for(auto &it : children2)
	{
	    if(children1.find(it.first) == children1.end())
		it.second.setVisibility(false);
	}
    }
}

void
ROP_AbcHierarchy::getNodes(UT_Set<ROP_AbcNode *> &nodes)
{
    nodes.clear();

    // logs the children
    UT_Array<Node *> work3;
    Node *node = &myRoot;

    work3.append(node);
    while(work3.entries())
    {
	node = work3.last();
	work3.removeLast();

	nodes.insert(node->getAbcNode());

	for(auto &it : node->getShapes())
	    for(auto &it2 : it.second)
		for(auto *shape : it2.second)
		    nodes.insert(shape);

	for(auto &it : node->getChildren())
	    work3.append(&it.second);
    }
}