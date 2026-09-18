#ifndef _BIT_H_
#define _BIT_H_

namespace bit {
//=== The bit that represents the status of a cell/node. ===

// Example: iLevBny_ = 0. It means the 0th bit is used to represent if a
// cell/node is a boundary cell/node of a level. If the 0th bit is 1, then it is
// a boundary cell/node.

// Boundary cell/node of a level. Note: the nodes just at the boundary are not
// 'boundary'
constexpr static int iLevBny_ = 0;

// The cells/nodes that are outside the simulation domain.
constexpr static int iDomainBny_ = 1;

// The nodes at the level boundary or the physical cells just next to the level
// boundary.
constexpr static int iLevEdge_ = 2;

// The nodes at the domain boundary or the physical cells just next to the
// domain boundary.
constexpr static int iDomainEdge_ = 3;

// New active cell/node
constexpr static int iNew_ = 4;

// Node only. A node may be shared by a few blocks/boxes. Sometimes (such as the
// E field solver) only one of the boexes needs to do some operations for this
// node. This box is the 'owner' of this node.
constexpr static int iOwner_ = 5;

// If a cell/node is refined or not.
constexpr static int iRefined_ = 6;

// If a cell is a neighbour to a refined cell.
constexpr static int iRefinedNeighbour_ = 7;
//=========================================================

inline bool test_bit(const int& i, int pos) { return i & (1 << pos); }
inline void turn_on_bit(int& i, int pos) { i |= (1 << pos); }
inline void turn_off_bit(int& i, int pos) { i &= ~(1 << pos); }

// inline void flip_bit(int& i, int pos) { i ^= (1 << pos); }

//======= Lev boundary =======
inline void set_lev_boundary(int& i) { turn_on_bit(i, iLevBny_); }
inline void set_not_lev_boundary(int& i) { turn_off_bit(i, iLevBny_); }
/**
 * @brief Check if the input cell is a ghost cell on the current level.
 *
 * @param i The integer representing the cell status.
 * @return True if the cell is a ghost cell on the current level, false
 * otherwise.
 */
inline bool is_lev_boundary(const int& i) { return test_bit(i, iLevBny_); }

//======= Lev edge cell/node =======
inline void set_lev_edge(int& i) { turn_on_bit(i, iLevEdge_); }
inline void set_not_lev_edge(int& i) { turn_off_bit(i, iLevEdge_); }
/**
 * @brief Check if the input cell is an edge physical cell on the current level.
 *
 * @param i The integer representing the cell status.
 * @return True if the cell is an edge cell on the current level, false
 * otherwise.
 */
inline bool is_lev_edge(const int& i) { return test_bit(i, iLevEdge_); }

//======= Domain boundary =======
inline void set_domain_boundary(int& i) { turn_on_bit(i, iDomainBny_); }
inline void set_not_domain_boundary(int& i) { turn_off_bit(i, iDomainBny_); }
inline bool is_domain_boundary(const int& i) {
  return test_bit(i, iDomainBny_);
}

//======= Domain edge cell/node =======
inline void set_domain_edge(int& i) { turn_on_bit(i, iDomainEdge_); }
inline void set_not_domain_edge(int& i) { turn_off_bit(i, iDomainEdge_); }
inline bool is_domain_edge(const int& i) { return test_bit(i, iDomainEdge_); }

//======= New active cell/node =======
inline void set_new(int& i) { turn_on_bit(i, iNew_); }
inline void set_not_new(int& i) { turn_off_bit(i, iNew_); }
inline bool is_new(const int& i) { return test_bit(i, iNew_); }

//======= Owner node (node only) =======
inline void set_owner(int& i) { turn_on_bit(i, iOwner_); }
inline void set_not_owner(int& i) { turn_off_bit(i, iOwner_); }
inline bool is_owner(const int& i) { return test_bit(i, iOwner_); }

//======= Refined cell =======
inline void set_refined(int& i) { turn_on_bit(i, iRefined_); }
inline void set_not_refined(int& i) { turn_off_bit(i, iRefined_); }
inline bool is_refined(const int& i) { return test_bit(i, iRefined_); }

//======= Neighbour to refined cell =======
inline void set_refined_neighbour(int& i) {
  turn_on_bit(i, iRefinedNeighbour_);
}
inline void set_not_refined_neighbour(int& i) {
  turn_off_bit(i, iRefinedNeighbour_);
}
inline bool is_refined_neighbour(const int& i) {
  return test_bit(i, iRefinedNeighbour_);
}

} // namespace bit
#endif
