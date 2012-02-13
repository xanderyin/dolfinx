// Copyright (C) 2006-2010 Anders Logg
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
// First added:  2006-06-02
// Last changed: 2011-11-15

#include <set>
#include <vector>
#include <boost/unordered_set.hpp>

#include <dolfin/common/Timer.h>
#include <dolfin/common/Timer.h>
#include <dolfin/common/utils.h>
#include <dolfin/log/dolfin_log.h>
#include "CellType.h"
#include "Mesh.h"
#include "MeshConnectivity.h"
#include "MeshEntity.h"
#include "MeshEntityIterator.h"
#include "MeshTopology.h"
#include "TopologyComputation.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
dolfin::uint TopologyComputation::compute_entities(Mesh& mesh, uint dim)
{
  // Generating an entity of topological dimension dim is equivalent
  // to generating the connectivity dim - 0 (connections to vertices)
  // and the connectivity mesh.topology().dim() - dim (connections from cells).
  //
  // We generate entities by iterating over all cells and generating a
  // new entity only on its first occurence. Entities also contained
  // in a previously visited cell are not generated. The new entities
  // are computed in three steps:
  //
  //   1. Iterate over cells and count new entities
  //
  //   2. Allocate memory / prepare data structures
  //
  //   3. Iterate over cells and add new entities

  // Get mesh topology and connectivity
  MeshTopology& topology = mesh.topology();
  MeshConnectivity& ce = topology(topology.dim(), dim);
  MeshConnectivity& ev = topology(dim, 0);

  uint current_entity = 0;

  // Check if entities have already been computed
  if (topology.size(dim) > 0)
  {
    // Make sure we really have the connectivity
    if ((ce.size() == 0 && dim != topology.dim()) || (ev.size() == 0 && dim != 0))
      dolfin_error("TopologyComputation.cpp",
                   "compute topological entities",
                   "Entities of topological dimension %d exist but connectivity is missing", dim);

    return topology.size(dim);
  }

  // Make sure connectivity does not already exist
  if (ce.size() > 0 || ev.size() > 0)
  {
      dolfin_error("TopologyComputation.cpp",
                   "compute topological entities",
                   "Connectivity for topological dimension %d exists but entities are missing", dim);
  }

  //info("Computing mesh entities of topological dimension %d.", dim);

  // Compute connectivity dim - dim if not already computed
  compute_connectivity(mesh, mesh.topology().dim(), mesh.topology().dim());

  // Start timer
  //info("Creating mesh entities of dimension %d.", dim);
  Timer timer("compute entities dim = " + to_string(dim));

  // Get cell type
  const CellType& cell_type = mesh.type();

  // Initialize local array of entities
  const uint m = cell_type.num_entities(dim);
  const uint n = cell_type.num_vertices(dim);
  std::vector<std::vector<uint> > entities(m, std::vector<uint>(n, 0));

  // Count the number of entities
  /*
  uint num_entities = 0;
  for (MeshEntityIterator c(mesh, mesh.topology().dim()); !c.end(); ++c)
  {
    // Get vertices from cell
    const uint* vertices = c->entities(0);
    dolfin_assert(vertices);

    // Create entities
    cell_type.create_entities(entities, dim, vertices);

    // Count new entities
    num_entities += count_entities(mesh, *c, entities, dim);
  }

  // Initialize the number of entities and connections
  topology.init(dim, num_entities);
  ce.init(mesh.num_cells(), m);
  ev.init(num_entities, n);

  // Add new entities
  current_entity = 0;
  for (MeshEntityIterator c(mesh, mesh.topology().dim()); !c.end(); ++c)
  {
    // Get vertices from cell
    const uint* vertices = c->entities(0);
    dolfin_assert(vertices);

    // Create entities
    cell_type.create_entities(entities, dim, vertices);

    // Add new entities to the mesh
    add_entities(mesh, *c, entities, dim, ce, ev, current_entity);
  }

  return num_entities;
  */

  // New code

  // List of entity e indices connected to cell
  std::vector<std::vector<uint> > connectivity_ce(mesh.num_cells());

  // List of vertces indices connected to entity e
  std::vector<std::vector<uint> > connectivity_ev;

  // List entities e (uint index, std::vector<uint> vertex_list) connected to each cell
  std::vector<std::vector<std::pair<uint, std::vector<uint> > > > ce_list(mesh.num_cells());

  current_entity = 0;
  for (MeshEntityIterator c(mesh, mesh.topology().dim()); !c.end(); ++c)
  {
    // Get vertices from cell
    const uint* vertices = c->entities(0);
    dolfin_assert(vertices);

    // Create entities
    cell_type.create_entities(entities, dim, vertices);

    // Iterate over the given list of entities
    //std::vector<std::vector<uint> >::const_iterator entity;
    std::vector<std::vector<uint> >::iterator entity;
    for (entity = entities.begin(); entity != entities.end(); ++entity)
    {
      // Sort entities (so that we can use equality testing later)
      std::sort(entity->begin(), entity->end());

      // Iterate over connected cells and look for entity
      for (MeshEntityIterator c0(*c, mesh.topology().dim()); !c0.end(); ++c0)
      {
        // Check only previously visited cells
        if (c0->index() >= c->index())
          continue;

        // Entities connected to c0
        const std::vector<std::pair<uint, std::vector<uint> > >& c0_list = ce_list[c0->index()];

        std::vector<std::pair<uint, std::vector<uint> > >::const_iterator other_entity;
        for (other_entity = c0_list.begin(); other_entity != c0_list.end(); ++other_entity)
        {
          // FIXME: Comparison relies on order of other_entity->second
          //        and *entity. We sort *entity to get this.
          //        Can we rely on the order and avoid sorting?
          if (other_entity->second == *entity)
          {
            // Entity already exists, so pick the index
            connectivity_ce[c->index()].push_back(other_entity->first);

            goto found;
          }
        }
      }

      // Add (index, list of vertices) pair to ce_list for cell
      ce_list[c->index()].push_back(std::make_pair(current_entity, *entity));

      // Add new entity index to cell - e connectivity
      connectivity_ce[c->index()].push_back(current_entity);

      // Add lst of new entity vertices
      connectivity_ev.push_back(*entity);

      // Increase counter
      current_entity++;

      // Entity found, don't need to create
      found:
      ;
    }
  }

  // Initialise connectivity data structure
  topology.init(dim, connectivity_ev.size());

  // Copy connectivity data into static MeshTopology data structures
  ce.set(connectivity_ce);
  ev.set(connectivity_ev);

  return connectivity_ev.size();
}
//-----------------------------------------------------------------------------
void TopologyComputation::compute_connectivity(Mesh& mesh, uint d0, uint d1)
{
  // This is where all the logic takes place to find a stragety for
  // the connectivity computation. For any given pair (d0, d1), the
  // connectivity is computed by suitably combining the following
  // basic building blocks:
  //
  //   1. compute_entities():     d  - 0  from dim - 0
  //   2. compute_transpose():    d0 - d1 from d1 - d0
  //   3. compute_intersection(): d0 - d1 from d0 - d' - d1
  //
  // Each of these functions assume a set of preconditions that we
  // need to satisfy.

  log(TRACE, "Requesting connectivity %d - %d.", d0, d1);

  // Get mesh topology and connectivity
  MeshTopology& topology = mesh.topology();
  MeshConnectivity& connectivity = topology(d0, d1);

  // Check if connectivity has already been computed
  if (connectivity.size() > 0)
    return;

  //info("Computing mesh connectivity %d - %d.", d0, d1);

  // Compute entities if they don't exist
  if (topology.size(d0) == 0)
    compute_entities(mesh, d0);
  if (topology.size(d1) == 0)
    compute_entities(mesh, d1);

  // Check is mesh has entities
  if (topology.size(d0) == 0 && topology.size(d1) == 0)
    return;

  // Check if connectivity still needs to be computed
  if (connectivity.size() > 0)
    return;

  // Start timer
  //info("Computing mesh connectivity %d - %d.", d0, d1);
  Timer timer("compute connectivity " + to_string(d0) + " - " + to_string(d1));

  // Decide how to compute the connectivity
  if (d0 < d1)
  {
    // Compute connectivity d1 - d0 and take transpose
    compute_connectivity(mesh, d1, d0);
    compute_from_transpose(mesh, d0, d1);
  }
  else
  {
    // These connections should already exist
    dolfin_assert(!(d0 > 0 && d1 == 0));

    // Choose how to take intersection
    uint d = 0;
    if (d0 == 0 && d1 == 0)
      d = mesh.topology().dim();

    // Compute connectivity d0 - d - d1 and take intersection
    compute_connectivity(mesh, d0, d);
    compute_connectivity(mesh, d, d1);
    compute_from_intersection(mesh, d0, d1, d);
  }
}
//----------------------------------------------------------------------------
void TopologyComputation::compute_from_transpose(Mesh& mesh, uint d0, uint d1)
{
  // The transpose is computed in three steps:
  //
  //   1. Iterate over entities of dimension d1 and count the number
  //      of connections for each entity of dimension d0
  //
  //   2. Allocate memory / prepare data structures
  //
  //   3. Iterate again over entities of dimension d1 and add connections
  //      for each entity of dimension d0

  log(TRACE, "Computing mesh connectivity %d - %d from transpose.", d0, d1);

  // Get mesh topology and connectivity
  MeshTopology& topology = mesh.topology();
  MeshConnectivity& connectivity = topology(d0, d1);

  // Need connectivity d1 - d0
  dolfin_assert(topology(d1, d0).size() > 0);

  // Temporary array
  std::vector<uint> tmp(topology.size(d0), 0);

  // Count the number of connections
  for (MeshEntityIterator e1(mesh, d1); !e1.end(); ++e1)
    for (MeshEntityIterator e0(*e1, d0); !e0.end(); ++e0)
      tmp[e0->index()]++;

  // Initialize the number of connections
  connectivity.init(tmp);

  // Reset current position for each entity
  std::fill(tmp.begin(), tmp.end(), 0);

  // Add the connections
  for (MeshEntityIterator e1(mesh, d1); !e1.end(); ++e1)
    for (MeshEntityIterator e0(*e1, d0); !e0.end(); ++e0)
      connectivity.set(e0->index(), e1->index(), tmp[e0->index()]++);
}
//----------------------------------------------------------------------------
void TopologyComputation::compute_from_intersection(Mesh& mesh,
                                                    uint d0, uint d1, uint d)
{
  log(TRACE, "Computing mesh connectivity %d - %d from intersection %d - %d - %d.",
      d0, d1, d0, d, d1);

  // Get mesh topology
  MeshTopology& topology = mesh.topology();

  // Check preconditions
  dolfin_assert(d0 >= d1);
  dolfin_assert(topology(d0, d).size() > 0);
  dolfin_assert(topology(d, d1).size() > 0);

  // Temporary dynamic storage, later copied into static storage
  std::vector<std::vector<uint> > connectivity(topology.size(d0));

  // Iterate over all entities of dimension d0
  std::size_t max_size = 1;
  for (MeshEntityIterator e0(mesh, d0); !e0.end(); ++e0)
  {
    // Get set of connected entities for current entity
    std::vector<uint>& entities = connectivity[e0->index()];

    // Reserve space
    entities.reserve(max_size);

    // Iterate over all connected entities of dimension d
    for (MeshEntityIterator e(*e0, d); !e.end(); ++e)
    {
      // Iterate over all connected entities of dimension d1
      for (MeshEntityIterator e1(*e, d1); !e1.end(); ++e1)
      {
        if (d0 == d1)
        {
          // An entity is not a neighbor to itself
          if (e0->index() != e1->index() && std::find(entities.begin(), entities.end(), e1->index()) == entities.end())
            entities.push_back(e1->index());
        }
        else
        {
          // Entity e1 must be completely contained in e0
          if (contains(*e0, *e1) && std::find(entities.begin(), entities.end(), e1->index()) == entities.end())
            entities.push_back(e1->index());
        }
      }
    }

    // Store maximum size
    max_size = std::max(entities.size(), max_size);
  }

  // Copy to static storage
  topology(d0, d1).set(connectivity);
}
//-----------------------------------------------------------------------------
dolfin::uint TopologyComputation::count_entities(Mesh& mesh, MeshEntity& cell,
                                     const std::vector<std::vector<uint> >& entities,
                                     uint dim)
{
  // For each entity, we iterate over connected and previously visited
  // cells to see if the entity has already been counted.

  // Needs to be a cell
  dolfin_assert(cell.dim() == mesh.topology().dim());

  // Iterate over the given list of entities
  uint num_entities = 0;
  for (uint i = 0; i < entities.size(); i++)
  {
    // Iterate over connected cells and look for entity
    for (MeshEntityIterator c(cell, mesh.topology().dim()); !c.end(); ++c)
    {
      // Check only previously visited cells
      if (c->index() >= cell.index())
        continue;

      // Check for vertices
      if (contains(c->entities(0), c->num_entities(0), &entities[i][0], entities[i].size()))
        goto found;
    }

    // Increase counter
    num_entities++;

    // Entity found, don't need to count
    found:
    ;
  }

  return num_entities;
}
//----------------------------------------------------------------------------
void TopologyComputation::add_entities(Mesh& mesh, MeshEntity& cell,
				 std::vector<std::vector<uint> >& entities, uint dim,
				 MeshConnectivity& ce,
				 MeshConnectivity& ev,
				 uint& current_entity)
{
  // We repeat the same algorithm as in count_entities() but this time
  // we add any entities that are new.

  // Needs to be a cell
  dolfin_assert(cell.dim() == mesh.topology().dim());

  // Iterate over the given list of entities
  for (uint i = 0; i < entities.size(); i++)
  {
    // Iterate over connected cells and look for entity
    for (MeshEntityIterator c(cell, mesh.topology().dim()); !c.end(); ++c)
    {
      // Check only previously visited cells
      if (c->index() >= cell.index())
        continue;

      // Check all entities of dimension dim in connected cell
      uint num_other_entities = c->num_entities(dim);
      const uint* other_entities = c->entities(dim);
      for (uint j = 0; j < num_other_entities; j++)
      {
        // Can't use iterators since connectivity has not been computed
        MeshEntity e(mesh, dim, other_entities[j]);
        if (contains(e.entities(0), e.num_entities(0), &entities[i][0], entities[i].size()))
        {
          // Entity already exists, so pick the index
          ce.set(cell.index(), e.index(), i);
          goto found;
        }
      }
    }

    // Entity does not exist, so create it
    ce.set(cell.index(), current_entity, i);
    ev.set(current_entity, entities[i]);

    // Increase counter
    current_entity++;

    // Entity found, don't need to create
    found:
    ;
  }
}
//----------------------------------------------------------------------------
bool TopologyComputation::contains(MeshEntity& e0, MeshEntity& e1)
{
  // Check vertices
  return contains(e0.entities(0), e0.num_entities(0),
                  e1.entities(0), e1.num_entities(0));
}
//----------------------------------------------------------------------------
bool TopologyComputation::contains(const uint* v0, uint n0,
                                   const uint* v1, uint n1)
{
  dolfin_assert(v0);
  dolfin_assert(v1);

  for (uint i1 = 0; i1 < n1; i1++)
  {
    bool found = false;
    for (uint i0 = 0; i0 < n0; i0++)
    {
      if (v0[i0] == v1[i1])
      {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}
//----------------------------------------------------------------------------
bool TopologyComputation::contains(const std::vector<uint>& v0,
                                   const std::vector<uint>& v1)
{
  for (uint i1 = 0; i1 < v1.size(); i1++)
  {
    bool found = false;
    for (uint i0 = 0; i0 < v0.size(); i0++)
    {
      if (v0[i0] == v1[i1])
      {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}
//----------------------------------------------------------------------------
