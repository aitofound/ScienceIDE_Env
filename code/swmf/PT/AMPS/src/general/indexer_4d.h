//===================================================
// cIndexer4D: Sparse 4D Index Manager
//===================================================
//
// PURPOSE:
//   Provides efficient sparse indexing for 4D coordinate spaces with arbitrary
//   ranges. Maps 4D coordinates (i,j,k,m) to compact linear indices, allocating
//   indices only for coordinates that are actually used.
//
// ALGORITHM:
//   1. Initialization Phase:
//      - User specifies min/max ranges for each dimension: [min[d], max[d]]
//      - Allocates flat array of size: ∏ (max[d] - min[d] + 1) for all d
//      - All entries initialized to -1 (unassigned)
//      - Computes strides for fast linear index calculation
//
//   2. Index Calculation:
//      Linear_index = (i-min[0]) + (j-min[1])*stride[1] + 
//                     (k-min[2])*stride[2] + (m-min[3])*stride[3]
//      where: stride[0]=1, stride[1]=dim[0], stride[2]=dim[0]*dim[1], 
//             stride[3]=dim[0]*dim[1]*dim[2]
//
//   3. Index Assignment (Lazy Allocation):
//      - First call to get_idx(i,j,k,m) assigns next available compact index
//      - Subsequent calls return the same index
//      - Only accessed coordinates consume compact index space
//
//   4. Sparse Storage Benefits:
//      - Full array size: O(∏ dim[d]) in memory
//      - Compact indices: O(accessed coordinates) in practice
//      - Fast O(1) lookup after coordinate validation
//
// TYPICAL USE CASES:
//   - Stencil operations on structured grids with variable indices
//   - Sparse matrix assembly with multi-dimensional coordinates
//   - Mapping between physical and computational index spaces
//   - Ghost cell indexing in parallel simulations
//
// USAGE EXAMPLE 1: Basic indexing with symmetric stencil
//   cIndexer4D stencil_idx;
//   int minLimits[4] = {-2, -2, -2, 0};  // Stencil radius=2, 1 variable
//   int maxLimits[4] = { 2,  2,  2, 0};
//   stencil_idx.init(minLimits, maxLimits);
//   
//   // Build stencil pattern (e.g., 7-point stencil)
//   int center_idx = stencil_idx.get_idx(0, 0, 0, 0);  // Returns 0
//   int east_idx   = stencil_idx.get_idx(1, 0, 0, 0);  // Returns 1
//   int west_idx   = stencil_idx.get_idx(-1, 0, 0, 0); // Returns 2
//   // ... etc for other directions
//   
//   printf("Total stencil points: %d\n", stencil_idx.get_index_count());
//
// USAGE EXAMPLE 2: Multi-variable indexing
//   cIndexer4D var_idx;
//   int minLimits[4] = {-1, -1, -1, 0};  // ±1 stencil, 3 variables
//   int maxLimits[4] = { 1,  1,  1, 2};
//   var_idx.init(minLimits, maxLimits);
//   
//   // Index for variable 0 at different locations
//   int idx_v0_center = var_idx.get_idx(0, 0, 0, 0);
//   int idx_v1_center = var_idx.get_idx(0, 0, 0, 1);
//   int idx_v2_east   = var_idx.get_idx(1, 0, 0, 2);
//   
//   // Check if specific coordinate is assigned
//   if (var_idx.is_assigned(-1, 1, 0, 1)) {
//       int idx = var_idx.get_idx(-1, 1, 0, 1);
//       // Use idx...
//   }
//
// USAGE EXAMPLE 3: Efficiency analysis
//   cIndexer4D idx;
//   int minLimits[4] = {-3, -3, -3, 0};
//   int maxLimits[4] = { 3,  3,  3, 4};  // 7×7×7×5 = 1715 possible entries
//   idx.init(minLimits, maxLimits);
//   
//   // Only assign indices for face neighbors and center
//   idx.get_idx( 0, 0, 0, 0);  // center
//   idx.get_idx( 1, 0, 0, 0);  // +x face
//   idx.get_idx(-1, 0, 0, 0);  // -x face
//   idx.get_idx( 0, 1, 0, 0);  // +y face
//   idx.get_idx( 0,-1, 0, 0);  // -y face
//   idx.get_idx( 0, 0, 1, 0);  // +z face
//   idx.get_idx( 0, 0,-1, 0);  // -z face
//   // Total: 7 indices allocated vs 1715 possible
//   
//   idx.print_statistics();  // Shows ~0.4% usage
//
// USAGE EXAMPLE 4: Reset and reuse
//   cIndexer4D idx;
//   int minLimits[4] = {-1, -1, -1, 0};
//   int maxLimits[4] = { 1,  1,  1, 0};
//   idx.init(minLimits, maxLimits);
//   
//   // First use
//   int idx1 = idx.get_idx(0, 0, 0, 0);  // Returns 0
//   int idx2 = idx.get_idx(1, 0, 0, 0);  // Returns 1
//   
//   // Reset for reuse (keeps memory allocated)
//   idx.reset();
//   
//   // Second use - indices restart from 0
//   int idx3 = idx.get_idx(0, 1, 0, 0);  // Returns 0 (not 2!)
//   int idx4 = idx.get_idx(-1, 0, 0, 0); // Returns 1
//
// PERFORMANCE NOTES:
//   - Initialization: O(total_array_size) for zeroing
//   - get_idx():      O(1) - single array lookup after bounds check
//   - is_assigned():  O(1) - single array lookup
//   - Memory:         4 * ∏(max[d]-min[d]+1) bytes + overhead
//
// LIMITATIONS:
//   - Entire index space must fit in memory (even if sparsely used)
//   - Indices are assigned sequentially and cannot be freed individually
//   - Not thread-safe without external synchronization
//
//===================================================

#ifndef CINDEXER_4D_H
#define CINDEXER_4D_H

#include <cstdio>   // printf, snprintf
#include <new>      // std::bad_alloc
#include <cstdlib>  // std::size_t (via cstdlib) and exit if needed
#include "specfunc.h" // assumes exit(__LINE__, __FILE__, msg) is declared here

class cIndexer4D {
private:
  // Flat storage of assignment table: -1 means "unassigned", otherwise compact index.
  int* index_base;
  int  index_base_cnt;   // tracks highest assigned index + 1 (not necessarily count of assigned entries)
  int  num_assigned;     // exact count of coordinates with assigned indices (index_base[idx] != -1)
  int  nVarIndexMax;     // reserved for external logic; retained for compatibility

  // Min and max limits for each dimension [i, j, k, m]
  int min_limit[4];
  int max_limit[4];

  // Dimension sizes and strides for row-major linearization
  int dim_size[4];
  int stride[4]; // stride[0]=1; stride[d]=∏_{q<d} dim_size[q]

  // Convert coordinate from [min_limit, max_limit] to [0, dim_size-1]
  inline int map_coordinate(int coord, int dim) const noexcept {
    return coord - min_limit[dim];
  }

  // Validate coordinate range and calculate linear index
  // Returns linear index if valid, -1 if invalid.
  inline int is_valid_coordinate(int i, int j, int k, int m) const noexcept {
    if (i < min_limit[0] || i > max_limit[0] ||
        j < min_limit[1] || j > max_limit[1] ||
        k < min_limit[2] || k > max_limit[2] ||
        m < min_limit[3] || m > max_limit[3]) {
      return -1;
    }

    const int i_mapped = map_coordinate(i, 0);
    const int j_mapped = map_coordinate(j, 1);
    const int k_mapped = map_coordinate(k, 2);
    const int m_mapped = map_coordinate(m, 3);

    return i_mapped + j_mapped * stride[1] + k_mapped * stride[2] + m_mapped * stride[3];
  }

public:
  // Constructor: zero-initialize state; actual allocation happens in init().
  cIndexer4D() noexcept
  : index_base(NULL), index_base_cnt(0), num_assigned(0), nVarIndexMax(0) {
    for (int d = 0; d < 4; d++) {
      min_limit[d] = 0;
      max_limit[d] = 0;
      dim_size[d]  = 0;
      stride[d]    = 0;
    }
  }

  // Non-copyable (prevents double-free of index_base).
  cIndexer4D(const cIndexer4D&)            = delete;
  cIndexer4D& operator=(const cIndexer4D&) = delete;

  // Movable: transfers ownership of index_base and metadata.
  cIndexer4D(cIndexer4D&& other) noexcept
  : index_base(other.index_base),
    index_base_cnt(other.index_base_cnt),
    num_assigned(other.num_assigned),
    nVarIndexMax(other.nVarIndexMax) {
    for (int d = 0; d < 4; d++) {
      min_limit[d] = other.min_limit[d];
      max_limit[d] = other.max_limit[d];
      dim_size[d]  = other.dim_size[d];
      stride[d]    = other.stride[d];
    }
    other.index_base = NULL;
    other.index_base_cnt = 0;
    other.num_assigned = 0;
  }

  cIndexer4D& operator=(cIndexer4D&& other) noexcept {
    if (this == &other) return *this;
    free(); // release current
    
    // Copy metadata
    index_base_cnt  = other.index_base_cnt;
    num_assigned    = other.num_assigned;
    nVarIndexMax    = other.nVarIndexMax;
    for (int d = 0; d < 4; d++) {
      min_limit[d] = other.min_limit[d];
      max_limit[d] = other.max_limit[d];
      dim_size[d]  = other.dim_size[d];
      stride[d]    = other.stride[d];
    }
    
    // Deep copy index_base array if source is initialized
    if (other.index_base != NULL) {
      const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
      try {
        index_base = new int[total_size];
      } catch (std::bad_alloc&) {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Error: Failed to allocate %zu bytes for cIndexer4D move assignment",
                      static_cast<size_t>(total_size) * sizeof(int));
        exit(__LINE__, __FILE__, msg);
      }
      
      // Copy all indices
      for (int idx = 0; idx < total_size; idx++) {
        index_base[idx] = other.index_base[idx];
      }
    } else {
      index_base = NULL;
    }
    
    // Do NOT modify source (other remains unchanged)
    return *this;
  }

  // Destructor
  ~cIndexer4D() {
    free();
  }

  // Explicitly release memory (keeps limits intact but clears storage).
  void free() noexcept {
    if (index_base != NULL) {
      delete[] index_base;
      index_base = NULL;
    }
    index_base_cnt = 0;
    num_assigned = 0;
  }

  // Check if the indexer has been initialized (i.e., storage allocated).
  bool isInitialized() const noexcept {
    return (index_base != NULL);
  }

  // Initialize the indexer with min/max limits for each dimension
  // minLimits[4]: minimum values for [i, j, k, m]
  // maxLimits[4]: maximum values for [i, j, k, m]
  void init(int minLimits[4], int maxLimits[4]) {
    if (index_base != NULL) {
      exit(__LINE__, __FILE__, "Error: cIndexer4D already initialized");
    }

    // Validate and store limits; compute dimension sizes.
    for (int d = 0; d < 4; d++) {
      if (minLimits[d] > maxLimits[d]) {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Error: minLimits[%d] (%d) > maxLimits[%d] (%d)",
                      d, minLimits[d], d, maxLimits[d]);
        exit(__LINE__, __FILE__, msg);
      }
      min_limit[d] = minLimits[d];
      max_limit[d] = maxLimits[d];
      dim_size[d]  = maxLimits[d] - minLimits[d] + 1;
    }

    // Compute strides (row-major layout).
    stride[0] = 1;
    stride[1] = dim_size[0];
    stride[2] = dim_size[0] * dim_size[1];
    stride[3] = dim_size[0] * dim_size[1] * dim_size[2];

    // Allocate and initialize the assignment table.
    const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];

    try {
      index_base = new int[total_size];
    } catch (std::bad_alloc&) {
      char msg[256];
      // Use %zu for size_t; cast to size_t for clarity.
      std::snprintf(msg, sizeof(msg),
                    "Error: Failed to allocate %zu bytes for cIndexer4D",
                    static_cast<size_t>(total_size) * sizeof(int));
      exit(__LINE__, __FILE__, msg);
    }

    // Initialize entries to -1 (unassigned).
    for (int idx = 0; idx < total_size; idx++) index_base[idx] = -1;

    index_base_cnt = 0;
    num_assigned = 0;
  }

  // Add an index entry for the given coordinates (no-op if already assigned).
  // Returns the compact index.
  int add_index(int i, int j, int k, int m) {
    if (index_base == NULL) {
      exit(__LINE__, __FILE__, "Error: cIndexer4D not initialized. Call init() first");
    }

    const int linear_idx = is_valid_coordinate(i, j, k, m);
    if (linear_idx == -1) {
      char msg[512];
      std::snprintf(msg, sizeof(msg),
                    "Error: Coordinates (%d, %d, %d, %d) out of range\n"
                    "Valid ranges: i[%d,%d], j[%d,%d], k[%d,%d], m[%d,%d]",
                    i, j, k, m,
                    min_limit[0], max_limit[0],
                    min_limit[1], max_limit[1],
                    min_limit[2], max_limit[2],
                    min_limit[3], max_limit[3]);
      exit(__LINE__, __FILE__, msg);
    }

    int& slot = index_base[linear_idx];
    if (slot == -1) {
      slot = index_base_cnt++;
      num_assigned++;
    }
    return slot;
  }

  // Get index for the given coordinates; lazily assigns if unassigned.
  int get_idx(int i, int j, int k, int m) {
    if (index_base == NULL) {
      exit(__LINE__, __FILE__, "Error: cIndexer4D not initialized. Call init() first");
    }

    const int linear_idx = is_valid_coordinate(i, j, k, m);
    if (linear_idx == -1) {
      char msg[512];
      std::snprintf(msg, sizeof(msg),
                    "Error: Coordinates (%d, %d, %d, %d) out of range\n"
                    "Valid ranges: i[%d,%d], j[%d,%d], k[%d,%d], m[%d,%d]",
                    i, j, k, m,
                    min_limit[0], max_limit[0],
                    min_limit[1], max_limit[1],
                    min_limit[2], max_limit[2],
                    min_limit[3], max_limit[3]);
      exit(__LINE__, __FILE__, msg);
    }

    int& slot = index_base[linear_idx];
    if (slot == -1) {
      slot = index_base_cnt++;
      num_assigned++;
    }
    return slot;
  }

  // Manually set a specific index value for the given coordinates.
  // This allows direct control over index assignment, useful for remapping indices
  // or integrating with external indexing schemes. Unlike get_idx(), this does NOT
  // automatically assign sequential indices; it sets the exact value specified.
  // 
  // Parameters:
  //   new_idx: The compact index value to assign (should be non-negative)
  //   i, j, k, m: The 4D coordinates
  //
  // Notes:
  //   - If new_idx >= index_base_cnt, index_base_cnt is updated to new_idx + 1
  //   - This can create gaps in the index sequence
  //   - Setting new_idx to -1 effectively "unassigns" the coordinate
  //   - Does not validate that new_idx is unique; caller must ensure no duplicates
  //
  // Usage Example:
  //   idx.set_idx(100, 0, 0, 0, 0);  // Manually assign index 100 to center
  //   idx.set_idx(200, 1, 0, 0, 0);  // Manually assign index 200 to east
  void set_idx(int new_idx, int i, int j, int k, int m) {
    if (index_base == NULL) {
      exit(__LINE__, __FILE__, "Error: cIndexer4D not initialized. Call init() first");
    }

    const int linear_idx = is_valid_coordinate(i, j, k, m);
    if (linear_idx == -1) {
      char msg[512];
      std::snprintf(msg, sizeof(msg),
                    "Error: Coordinates (%d, %d, %d, %d) out of range\n"
                    "Valid ranges: i[%d,%d], j[%d,%d], k[%d,%d], m[%d,%d]",
                    i, j, k, m,
                    min_limit[0], max_limit[0],
                    min_limit[1], max_limit[1],
                    min_limit[2], max_limit[2],
                    min_limit[3], max_limit[3]);
      exit(__LINE__, __FILE__, msg);
    }

    // Check if this is a new assignment
    if (index_base[linear_idx] == -1) {
      num_assigned++;
    }
    
    index_base[linear_idx] = new_idx;

    // Update index_base_cnt if necessary to maintain count of highest index + 1
    if (new_idx >= index_base_cnt) {
      index_base_cnt = new_idx + 1;
    }
  }

  // Safely check and set an index for the given coordinates with conflict detection.
  // This function ensures consistent index assignment by either setting a new index
  // or verifying that an existing index matches the expected value. It prevents
  // accidental index conflicts that could lead to data corruption.
  //
  // Parameters:
  //   new_idx: The desired compact index value to assign
  //   i, j, k, m: The 4D coordinates
  //
  // Behavior:
  //   1. If coordinates are unassigned (index == -1):
  //      - Assigns new_idx to the coordinates
  //      - Updates index_base_cnt if necessary
  //      - Returns true
  //   2. If coordinates are already assigned:
  //      a. If existing index matches new_idx: Returns true (no-op)
  //      b. If existing index differs from new_idx: Exits with error message
  //
  // Returns:
  //   true if assignment succeeded or index already matches
  //
  // Usage Example:
  //   // Safe assignment with conflict detection
  //   idx.check_and_set(10, 0, 0, 0, 0);  // Assigns 10 to center
  //   idx.check_and_set(10, 0, 0, 0, 0);  // OK: already 10, returns true
  //   idx.check_and_set(20, 0, 0, 0, 0);  // ERROR: conflict detected, exits
  //
  // Use case:
  //   Useful when rebuilding index mappings from external data where
  //   consistency must be verified, such as reading from checkpoint files
  //   or merging index assignments from multiple sources.
  bool check_and_set(int new_idx, int i, int j, int k, int m) {
    if (index_base == NULL) {
      exit(__LINE__, __FILE__, "Error: cIndexer4D not initialized. Call init() first");
    }

    const int linear_idx = is_valid_coordinate(i, j, k, m);
    if (linear_idx == -1) {
      char msg[512];
      std::snprintf(msg, sizeof(msg),
                    "Error: Coordinates (%d, %d, %d, %d) out of range\n"
                    "Valid ranges: i[%d,%d], j[%d,%d], k[%d,%d], m[%d,%d]",
                    i, j, k, m,
                    min_limit[0], max_limit[0],
                    min_limit[1], max_limit[1],
                    min_limit[2], max_limit[2],
                    min_limit[3], max_limit[3]);
      exit(__LINE__, __FILE__, msg);
    }

    int& slot = index_base[linear_idx];
    
    if (slot == -1) {
      // Coordinate not yet assigned - set it to new_idx
      slot = new_idx;
      num_assigned++;
      
      // Update index_base_cnt if necessary
      if (new_idx >= index_base_cnt) {
        index_base_cnt = new_idx + 1;
      }
      return true;
    } else if (slot == new_idx) {
      // Already assigned to the expected value - no conflict
      return true;
    } else {
      // Conflict detected - existing index differs from new_idx
      // Search through index_base to find where new_idx is currently assigned (if anywhere)
      const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
      int found_linear_idx = -1;
      for (int idx = 0; idx < total_size; idx++) {
        if (index_base[idx] == new_idx) {
          found_linear_idx = idx;
          break;
        }
      }
      
      char msg[768];
      if (found_linear_idx != -1) {
        // new_idx is already assigned to different coordinates - compute those coordinates
        int found_i = found_linear_idx % dim_size[0];
        int temp = found_linear_idx / dim_size[0];
        int found_j = temp % dim_size[1];
        temp = temp / dim_size[1];
        int found_k = temp % dim_size[2];
        int found_m = temp / dim_size[2];
        
        // Convert from mapped [0, dim_size-1] back to [min_limit, max_limit]
        found_i += min_limit[0];
        found_j += min_limit[1];
        found_k += min_limit[2];
        found_m += min_limit[3];
        
        std::snprintf(msg, sizeof(msg),
                      "Error: Index conflict at coordinates (%d, %d, %d, %d)\n"
                      "  Existing index: %d\n"
                      "  Attempted to set: %d\n"
                      "  Index %d is already assigned to coordinates (%d, %d, %d, %d)",
                      i, j, k, m, slot, new_idx, new_idx, found_i, found_j, found_k, found_m);
      } else {
        // new_idx is not currently assigned anywhere
        std::snprintf(msg, sizeof(msg),
                      "Error: Index conflict at coordinates (%d, %d, %d, %d)\n"
                      "  Existing index: %d\n"
                      "  Attempted to set: %d\n"
                      "  Index %d is not currently assigned to any coordinates",
                      i, j, k, m, slot, new_idx, new_idx);
      }
      exit(__LINE__, __FILE__, msg);
    }

    return true;
  }

  // Check that all assigned indices in index_base are unique.
  // This validates the integrity of the index mapping by ensuring no duplicate
  // index values exist across different coordinates (except for -1 which means unassigned).
  //
  // Returns:
  //   true if all assigned indices are unique (or if indexer is not initialized)
  //
  // Side effects:
  //   If duplicates are found, exits with a detailed error message showing:
  //   - The duplicate index value
  //   - All coordinates that share this index
  //
  // Performance:
  //   O(capacity * assigned_count) in worst case
  //   For each assigned entry, checks if its index appears elsewhere
  //
  // Usage:
  //   idx.check_unique();  // Validates index integrity, exits on error
  //
  // Typical use cases:
  //   - After manual index manipulation with set_idx()
  //   - After loading index mappings from external sources
  //   - Debugging index assignment algorithms
  //   - Validating correctness before saving checkpoint data
  bool check_unique() const {
    if (index_base == NULL) {
      return true;  // Not initialized, trivially unique
    }
    
    const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
    
    // For each assigned entry, verify its index doesn't appear elsewhere
    for (int idx1 = 0; idx1 < total_size; idx1++) {
      const int value1 = index_base[idx1];
      
      // Skip unassigned entries
      if (value1 == -1) continue;
      
      // Check if this value appears anywhere else
      for (int idx2 = idx1 + 1; idx2 < total_size; idx2++) {
        const int value2 = index_base[idx2];
        
        if (value1 == value2) {
          // Found duplicate - decode both linear indices to coordinates
          
          // Decode idx1
          int i1 = idx1 % dim_size[0];
          int temp = idx1 / dim_size[0];
          int j1 = temp % dim_size[1];
          temp = temp / dim_size[1];
          int k1 = temp % dim_size[2];
          int m1 = temp / dim_size[2];
          
          // Convert from mapped [0, dim_size-1] back to [min_limit, max_limit]
          i1 += min_limit[0];
          j1 += min_limit[1];
          k1 += min_limit[2];
          m1 += min_limit[3];
          
          // Decode idx2
          int i2 = idx2 % dim_size[0];
          temp = idx2 / dim_size[0];
          int j2 = temp % dim_size[1];
          temp = temp / dim_size[1];
          int k2 = temp % dim_size[2];
          int m2 = temp / dim_size[2];
          
          // Convert from mapped [0, dim_size-1] back to [min_limit, max_limit]
          i2 += min_limit[0];
          j2 += min_limit[1];
          k2 += min_limit[2];
          m2 += min_limit[3];
          
          // Report error with full details
          char msg[768];
          std::snprintf(msg, sizeof(msg),
                        "Error: Duplicate index detected during uniqueness check\n"
                        "  Index value: %d\n"
                        "  Assigned to coordinates: (%d, %d, %d, %d)\n"
                        "  Also assigned to:        (%d, %d, %d, %d)\n"
                        "  All assigned indices must be unique",
                        value1, i1, j1, k1, m1, i2, j2, k2, m2);
          exit(__LINE__, __FILE__, msg);
        }
      }
    }
    
    return true;  // All indices are unique
  }

  // Find the coordinates (i,j,k,m) that are assigned to a given index value.
  // This performs a reverse lookup: given a compact index, find which coordinates
  // map to that index.
  //
  // Parameters:
  //   idx: The compact index value to search for
  //   i, j, k, m: Output parameters - will be set to the coordinates if found
  //
  // Returns:
  //   true if the index was found and coordinates were written to i,j,k,m
  //   false if the index was not found (i,j,k,m remain unchanged)
  //
  // Performance:
  //   O(capacity) - must scan the entire index_base array
  //
  // Notes:
  //   - If multiple coordinates have the same index (integrity violation),
  //     this returns the first occurrence found
  //   - Searching for idx == -1 will always return false (unassigned marker)
  //
  // Usage Example:
  //   int i, j, k, m;
  //   if (indexer.find_idx(10, i, j, k, m)) {
  //       printf("Index 10 is at coordinates (%d, %d, %d, %d)\n", i, j, k, m);
  //   } else {
  //       printf("Index 10 not found\n");
  //   }
  //
  // Use cases:
  //   - Reverse mapping from compact storage back to coordinate space
  //   - Debugging: verify which coordinates correspond to specific indices
  //   - Translating between index-based and coordinate-based representations
  bool find_idx(int idx, int& i, int& j, int& k, int& m) const {
    if (index_base == NULL) {
      return false;  // Not initialized
    }
    
    // Don't search for the unassigned marker
    if (idx == -1) {
      return false;
    }
    
    const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
    
    // Search for the index value
    for (int linear_idx = 0; linear_idx < total_size; linear_idx++) {
      if (index_base[linear_idx] == idx) {
        // Found it - decode linear index to coordinates
        int i_mapped = linear_idx % dim_size[0];
        int temp = linear_idx / dim_size[0];
        int j_mapped = temp % dim_size[1];
        temp = temp / dim_size[1];
        int k_mapped = temp % dim_size[2];
        int m_mapped = temp / dim_size[2];
        
        // Convert from mapped [0, dim_size-1] back to [min_limit, max_limit]
        i = i_mapped + min_limit[0];
        j = j_mapped + min_limit[1];
        k = k_mapped + min_limit[2];
        m = m_mapped + min_limit[3];
        
        return true;
      }
    }
    
    return false;  // Index not found
  }

  // Overloaded version: find and print the coordinates for a given index.
  // Convenience function that searches for an index and prints the result
  // to stdout instead of using output parameters.
  //
  // Parameters:
  //   idx: The compact index value to search for
  //
  // Returns:
  //   true if the index was found and printed
  //   false if the index was not found
  //
  // Output format:
  //   If found:     "Index <idx> found at coordinates: (i, j, k, m)"
  //   If not found: "Index <idx> not found in index mapping"
  //
  // Usage Example:
  //   indexer.find_idx(10);  // Prints result directly
  //
  // Use cases:
  //   - Quick debugging and inspection
  //   - Interactive exploration of index mappings
  //   - Logging and diagnostics
  bool find_idx(int idx) const {
    int i, j, k, m;
    if (find_idx(idx, i, j, k, m)) {
      std::printf("Index %d found at coordinates: (%d, %d, %d, %d)\n", idx, i, j, k, m);
      return true;
    } else {
      std::printf("Index %d not found in index mapping\n", idx);
      return false;
    }
  }

  // Get the current total count of assigned indices.
  // Returns the exact number of coordinates with assigned compact indices (index_base[idx] != -1).
  // This is O(1) as it returns a counter maintained during index assignments.
  int get_index_count() const noexcept {
    return num_assigned;
  }

  // Get the highest assigned index value + 1.
  // This represents the minimum array size needed to store all assigned indices.
  // If indices are assigned sequentially (0, 1, 2, ...), this equals get_index_count().
  // If indices have gaps (e.g., 0, 10, 50), this will be larger than get_index_count().
  int get_max_index_bound() const noexcept { return index_base_cnt; }

  // Total capacity (number of addressable entries in the dense table).
  int capacity() const noexcept { 
    return dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
  }

  // Check if a specific entry has been assigned.
  bool is_assigned(int i, int j, int k, int m) const noexcept {
    if (index_base == NULL) return false;
    const int linear_idx = is_valid_coordinate(i, j, k, m);
    if (linear_idx == -1)  return false;
    return index_base[linear_idx] != -1;
  }

  // Get the min/max limits.
  void get_limits(int minOut[4], int maxOut[4]) const noexcept {
    for (int d = 0; d < 4; d++) { minOut[d] = min_limit[d]; maxOut[d] = max_limit[d]; }
  }

  // Get dimension sizes.
  void get_dim_sizes(int sizes[4]) const noexcept {
    for (int d = 0; d < 4; d++) sizes[d] = dim_size[d];
  }

  // Reset all indices to -1 without deallocating (keeps capacity/limits).
  void reset() noexcept {
    if (index_base != NULL) {
      const int total_size = dim_size[0] * dim_size[1] * dim_size[2] * dim_size[3];
      for (int idx = 0; idx < total_size; idx++) index_base[idx] = -1;
      index_base_cnt = 0;
      num_assigned = 0;
    }
  }

  // Print statistics about index usage.
  void print_statistics() const {
    if (index_base == NULL) {
      std::printf("cIndexer4D not initialized\n");
      return;
    }
    const int total_entries = capacity();
    const int assigned_count = get_index_count();
    const double usage_percent = (total_entries > 0)
      ? (100.0 * static_cast<double>(assigned_count) / static_cast<double>(total_entries))
      : 0.0;

    std::printf("cIndexer4D Statistics:\n");
    std::printf("  Dimension ranges:\n");
    std::printf("    i: [%d, %d] (size=%d)\n", min_limit[0], max_limit[0], dim_size[0]);
    std::printf("    j: [%d, %d] (size=%d)\n", min_limit[1], max_limit[1], dim_size[1]);
    std::printf("    k: [%d, %d] (size=%d)\n", min_limit[2], max_limit[2], dim_size[2]);
    std::printf("    m: [%d, %d] (size=%d)\n", min_limit[3], max_limit[3], dim_size[3]);
    std::printf("  Total possible entries: %d\n", total_entries);
    std::printf("  Assigned indices: %d\n", assigned_count);
    std::printf("  Max index bound: %d\n", index_base_cnt);
    std::printf("  Usage: %.2f%%\n", usage_percent);
  }
};

#endif // CINDEXER_4D_H
