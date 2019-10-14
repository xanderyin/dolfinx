// Copyright (C) 2019 Jorgen S. Dokken
//
// This file is part of DOLFIN (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

# pragma once


#include <Eigen/Dense>
#include <array>
#include <map>
#include <vector>
#include <dolfin/mesh/cell_types.h>

namespace dolfin
{
namespace mesh
{

/// Map from DOLFIN node ordering to VTK/XDMF ordering
std::vector<std::uint8_t> vtk_cell_permutation(CellType type, int num_nodes);

/// Map from VTK ordering of a cell to tensor-product ordering.
/// This map returns the identity map for all other cells than
/// quadrilaterals and hexahedrons.
/// @param type Celltype to map
/// @param num_nodes Number of nodes in cell
/// @return The map
std::vector<std::uint8_t> vtk_to_tp(CellType type, int num_nodes);


/// Map from the mapping of lexicographic nodes to a tensor product ordering
 /// @param type Celltype to map
/// @param num_nodes Number of nodes in cell
/// @return The map
 std::vector<std::uint8_t> lexico_to_tp(CellType type, int num_nodes);


// Permutation for VTK ordering to FENICS ordering
std::vector<std::uint8_t> vtk_to_fenics(CellType type, int num_nodes);


// Convert gmsh cell ordering to FENICS cell ordering
Eigen::Array<std::int64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
gmsh_to_dolfin_ordering(Eigen::Array<std::int64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> cells,
						CellType type);

/// Default map of DOLFIN/UFC node ordering to the cell input ordering
std::vector<std::uint8_t> default_cell_permutation(CellType type,
                                                   std::int32_t degree);

} // namespace mesh
} // namespace dolfin
