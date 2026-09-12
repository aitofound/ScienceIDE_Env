/*
 * stencil.h - Improved version
 *
 * STENCIL CONSTRUCTION LIBRARY FOR FINITE DIFFERENCE METHODS
 *
 * Original Created: Dec 9, 2017
 * Author: vtenishe
 * Improved: 2025
 *
 * ============================================================================
 * DESCRIPTION
 * ============================================================================
 * This library provides tools for constructing and manipulating numerical
 * stencils used in finite difference methods for solving partial differential
 * equations (PDEs). A stencil defines the discrete approximation of differential
 * operators by specifying coefficients at various grid points.
 *
 * The library supports:
 * - Fractional grid positions (for non-uniform or staggered grids)
 * - Arbitrary dimensional stencils (3D: i, j, k indices)
 * - Stencil arithmetic (addition, subtraction, scaling)
 * - Stencil composition and transformation
 *
 * ============================================================================
 * ALGORITHM
 * ============================================================================
 * 
 * 1. STENCIL REPRESENTATION:
 *    A stencil is represented as a collection of (position, coefficient) pairs:
 *    - Position: (i, j, k) in fractional grid coordinates
 *    - Coefficient: weight/multiplier for the value at that position
 *
 * 2. BASE POINT:
 *    Each stencil has a base point (i₀, j₀, k₀) representing the center or
 *    reference location. Stencil elements are stored relative to this base.
 *
 * 3. STENCIL OPERATIONS:
 *    - Addition: Combines two stencils, merging coefficients at same positions
 *    - Shifting: Translates all stencil points by a constant offset
 *    - Scaling: Multiplies all coefficients by a constant
 *    - Simplification: Combines duplicate positions and removes zero coefficients
 *
 * 4. FRACTION ARITHMETIC:
 *    Uses exact rational arithmetic (fractions) to avoid floating-point errors
 *    in position calculations. Fractions are automatically simplified using GCD.
 *
 * ============================================================================
 * EXAMPLE USAGE
 * ============================================================================
 *
 * Example 1: Second-order central difference for d²u/dx²
 * -------------------------------------------------------
 * Approximation: (u[i-1] - 2*u[i] + u[i+1]) / h²
 * 
 *   cStencil laplacian_x("d2u_dx2");
 *   laplacian_x.SetBase(0, 0, 0);           // Center at grid point (0,0,0)
 *   laplacian_x.add(1.0, -1, 0, 0);         // u[i-1,j,k] coefficient = 1
 *   laplacian_x.add(-2.0, 0, 0, 0);         // u[i,j,k]   coefficient = -2
 *   laplacian_x.add(1.0, 1, 0, 0);          // u[i+1,j,k] coefficient = 1
 *   laplacian_x *= 1.0/(h*h);               // Scale by 1/h²
 *
 * Example 2: First-order upwind difference for du/dx
 * ---------------------------------------------------
 * Approximation: (u[i] - u[i-1]) / h
 * 
 *   cStencil upwind("du_dx_upwind");
 *   upwind.SetBase(0, 0, 0);
 *   upwind.add(-1.0, -1, 0, 0);             // u[i-1,j,k] coefficient = -1
 *   upwind.add(1.0, 0, 0, 0);               // u[i,j,k]   coefficient = 1
 *   upwind *= 1.0/h;                        // Scale by 1/h
 *
 * Example 3: Staggered grid (fractional positions)
 * ------------------------------------------------
 * For velocity at cell face (i+1/2):
 * 
 *   cStencil vel_interp("velocity_face");
 *   vel_interp.SetBase(cFrac(1,2), 0, 0);   // Base at i=1/2
 *   vel_interp.add(0.5, cFrac(-1,2), 0, 0); // u[i=0] coefficient = 0.5
 *   vel_interp.add(0.5, cFrac(1,2), 0, 0);  // u[i=1] coefficient = 0.5
 *
 * Example 4: 3D Laplacian operator
 * --------------------------------
 * ∇²u = d²u/dx² + d²u/dy² + d²u/dz²
 * 
 *   cStencil lap_x, lap_y, lap_z;
 *   
 *   // Build x-direction Laplacian
 *   lap_x.add(1.0, -1, 0, 0);
 *   lap_x.add(-2.0, 0, 0, 0);
 *   lap_x.add(1.0, 1, 0, 0);
 *   
 *   // Build y-direction Laplacian
 *   lap_y.add(1.0, 0, -1, 0);
 *   lap_y.add(-2.0, 0, 0, 0);
 *   lap_y.add(1.0, 0, 1, 0);
 *   
 *   // Build z-direction Laplacian
 *   lap_z.add(1.0, 0, 0, -1);
 *   lap_z.add(-2.0, 0, 0, 0);
 *   lap_z.add(1.0, 0, 0, 1);
 *   
 *   // Combine into full 3D Laplacian
 *   cStencil laplacian_3d = lap_x + lap_y + lap_z;
 *   laplacian_3d.Simplify();  // Combines the three -2.0 terms at (0,0,0)
 *
 * Example 5: Exporting stencil for computation
 * --------------------------------------------
 *   cStencil::cStencilData exported;
 *   laplacian_3d.ExportStencil(&exported);
 *   
 *   // Use in finite difference computation
 *   for (int n = 0; n < exported.Data.size(); n++) {
 *       int ii = i + exported.Data[n].i;
 *       int jj = j + exported.Data[n].j;
 *       int kk = k + exported.Data[n].k;
 *       result += exported.Data[n].a * u[ii][jj][kk];
 *   }
 *
 * ============================================================================
 * KEY CLASSES
 * ============================================================================
 *
 * cFrac: Rational number (fraction) class
 *   - Exact arithmetic for grid positions
 *   - Automatic simplification using GCD
 *   - Supports +, -, comparison operators
 *
 * cStencil: Main stencil class
 *   - Stores collection of (position, coefficient) pairs
 *   - Base point (i, j, k) for reference
 *   - Methods: add(), shift(), Simplify(), Print()
 *   - Operators: +, -, *, +=, *= for stencil algebra
 *
 * cStencil::cStencilData: Exported integer-only stencil format
 *   - Converts fractional positions to absolute integer indices
 *   - Efficient format for runtime computation
 *
 * ============================================================================
 * NOTES
 * ============================================================================
 * - All fractions are automatically simplified using Euclidean GCD algorithm
 * - Zero-coefficient elements are automatically removed during Simplify()
 * - Stencils can be shifted, scaled, and combined algebraically
 * - Thread-safe for read operations after construction
 * - Uses exception handling for error conditions (no exit() calls)
 *
 */

#ifndef _GENERAL_STENCIL_H_
#define _GENERAL_STENCIL_H_

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <memory>

/*
 * Greatest Common Divisor (GCD) - Euclidean Algorithm
 * ====================================================
 * 
 * PURPOSE:
 * Computes the largest positive integer that divides both a and b without remainder.
 * Used for simplifying fractions to their lowest terms.
 * 
 * ALGORITHM (Euclidean Algorithm):
 * The GCD is found by repeatedly replacing the larger number with the remainder
 * of dividing the larger by the smaller, until one number becomes zero.
 * 
 * Mathematical basis: gcd(a, b) = gcd(b, a mod b)
 * This works because any common divisor of a and b also divides (a - kb) for any k,
 * and specifically divides the remainder (a mod b).
 * 
 * COMPLEXITY: O(log min(a,b))
 * Much faster than trial division which is O(min(a,b))
 * 
 * EXAMPLE TRACE:
 * gcd(48, 18):
 *   Step 1: a=48, b=18  →  48 % 18 = 12  →  a=18, b=12
 *   Step 2: a=18, b=12  →  18 % 12 = 6   →  a=12, b=6
 *   Step 3: a=12, b=6   →  12 % 6  = 0   →  a=6,  b=0
 *   Result: gcd(48, 18) = 6
 * 
 * Verification: 48 = 6×8, 18 = 6×3, and 6 is the largest such divisor
 * Simplified fraction: 48/18 = 8/3
 * 
 * PROPERTIES:
 * - gcd(a, 0) = |a|
 * - gcd(a, b) = gcd(b, a)  (commutative)
 * - gcd(a, b) = gcd(|a|, |b|)  (sign-independent)
 * 
 * @param a First integer
 * @param b Second integer
 * @return Greatest common divisor (always positive)
 */
inline int gcd(int a, int b) {
    a = std::abs(a);  // Work with absolute values
    b = std::abs(b);
    
    // Euclidean algorithm: repeatedly replace (a,b) with (b, a mod b)
    while (b != 0) {
        int temp = b;
        b = a % b;  // Remainder when a is divided by b
        a = temp;
    }
    
    return a;  // When b=0, a contains the GCD
}

class cFrac {
public:
    int nominator;
    int denominator;
    
    // Constructors
    cFrac() : nominator(0), denominator(1) {}
    
    // Single argument constructor for implicit conversion from int
    cFrac(int n) : nominator(n), denominator(1) {}
    
    cFrac(int n, int d) : nominator(n), denominator(d) {
        if (d == 0) {
            throw std::invalid_argument("Denominator cannot be zero");
        }
        Simplify();
    }
    
    // Simplify using GCD - much faster O(log n) algorithm
    void Simplify() {
        if (denominator == 0) {
            throw std::invalid_argument("Denominator cannot be zero");
        }
        
        if (nominator == 0) {
            denominator = 1;
            return;
        }
        
        int g = gcd(nominator, denominator);
        nominator /= g;
        denominator /= g;
        
        // Keep denominator positive
        if (denominator < 0) {
            nominator = -nominator;
            denominator = -denominator;
        }
    }
    
    void Print(FILE* fout = stdout) const {
        if (nominator % denominator == 0) {
            fprintf(fout, " %i ", nominator / denominator);
        } else {
            fprintf(fout, " %i/%i ", nominator, denominator);
        }
    }
    
    void Set(int n, int d) {
        if (d == 0) {
            throw std::invalid_argument("Denominator cannot be zero");
        }
        nominator = n;
        denominator = d;
        Simplify();
    }

    int Convert2Int() const {
        if (nominator % denominator != 0) {
            throw std::runtime_error("Fraction cannot be converted to integer");
        }
        return nominator / denominator;
    }
    
    // Assignment operators
    cFrac& operator=(const cFrac& v) {
        nominator = v.nominator;
        denominator = v.denominator;
        return *this;
    }

    cFrac& operator=(int v) {
        nominator = v;
        denominator = 1;
        return *this;
    }
    
    // Arithmetic operators
    friend cFrac operator+(const cFrac& v1, const cFrac& v2) {
        cFrac v3;
        v3.nominator = v1.nominator * v2.denominator + v2.nominator * v1.denominator;
        v3.denominator = v1.denominator * v2.denominator;
        v3.Simplify();
        return v3;
    }
    
    friend cFrac& operator+=(cFrac& v1, const cFrac& v2) {
        v1.nominator = v1.nominator * v2.denominator + v2.nominator * v1.denominator;
        v1.denominator = v1.denominator * v2.denominator;
        v1.Simplify();
        return v1;
    }
    
    friend cFrac& operator-=(cFrac& v1, const cFrac& v2) {
        v1.nominator = v1.nominator * v2.denominator - v2.nominator * v1.denominator;
        v1.denominator = v1.denominator * v2.denominator;
        v1.Simplify();
        return v1;
    }
    
    friend cFrac operator-(const cFrac& v1, const cFrac& v2) {
        cFrac v3;
        v3.nominator = v1.nominator * v2.denominator - v2.nominator * v1.denominator;
        v3.denominator = v1.denominator * v2.denominator;
        v3.Simplify();
        return v3;
    }
    
    // Comparison operators
    bool operator==(const cFrac& rhs) const {
        return (nominator * rhs.denominator == denominator * rhs.nominator);
    }
    
    friend bool operator<(const cFrac& l, const cFrac& r) {
        return (l.nominator * r.denominator < l.denominator * r.nominator);
    }
    
    friend bool operator>(const cFrac& l, const cFrac& r) {
        return (l.nominator * r.denominator > l.denominator * r.nominator);
    }
};


class cStencil {
public:
    cFrac i, j, k;
    std::string symbol;
    
    struct cStencilElement {
        cFrac i, j, k;
        double a;  // coefficient used in the stencil
        
        cStencilElement() : i(0), j(0), k(0), a(0.0) {}
    };

    class cStencilData {
    public:
        class cElementData {
        public:
            int i, j, k;
            double a;

            cElementData() : i(0), j(0), k(0), a(0.0) {}
        };

        std::vector<cElementData> Data;
        int Length;  // For compatibility with original implementation

        void print(const char* msg = nullptr) const {
            if (msg != nullptr) {
                printf("%s:\n", msg);
            }
            printf("Stencil length=%i\ni\tj\tk\ta\n", Length);

            for (int it = 0; it < Length; it++) {
                printf("%i\t%i\t%i\t%e\n", Data[it].i, Data[it].j, Data[it].k, Data[it].a);
            }
        }

        cStencilData() : Length(0) {}

        void remove() {
            Data.clear();
            Length = 0;
        }

        double GetElementValue(int i, int j, int k, bool* SuccessFlag = nullptr) const {
            for (int it = 0; it < Length; it++) {
                if ((i == Data[it].i) && (j == Data[it].j) && (k == Data[it].k)) {
                    if (SuccessFlag != nullptr) *SuccessFlag = true;
                    return Data[it].a;
                }
            }

            if (SuccessFlag != nullptr) *SuccessFlag = false;
            return 0.0;
        }
    };

private:
    std::list<cStencilElement> StencilData;
    std::vector<cStencilElement> StencilCache;

public:
    // Constructors
    cStencil() : i(0), j(0), k(0), symbol("") {}

    cStencil(const char* s) : i(0), j(0), k(0), symbol(s) {}
    
    cStencil(const char* s, cFrac iIn, cFrac jIn, cFrac kIn) 
        : i(iIn), j(jIn), k(kIn), symbol(s) {}
    
    // Copy constructor
    cStencil(const cStencil& other) 
        : i(other.i), j(other.j), k(other.k), symbol(other.symbol),
          StencilData(other.StencilData), StencilCache(other.StencilCache) {}
    
    // Move constructor
    cStencil(cStencil&& other) noexcept
        : i(std::move(other.i)), j(std::move(other.j)), k(std::move(other.k)),
          symbol(std::move(other.symbol)), StencilData(std::move(other.StencilData)),
          StencilCache(std::move(other.StencilCache)) {}

    // In class cStencil
    int RadiusLinf() const noexcept {
      // ceil(|n/d|) without floating point
      const auto ceil_abs = [](const cFrac& f) noexcept -> int {
        int n = std::abs(f.nominator);
        int d = std::abs(f.denominator);
        if (n == 0) return 0;
        return (n + d - 1) / d; // ceil(n/d)
      };

      int r = 0;

      // If your base is stored as Base.i/j/k (cFrac), keep this line:
      // If instead you have Base_i/Base_j/Base_k, use:

      for (const auto& e : StencilData) {
        const int rx = ceil_abs(e.i + i);
        const int ry = ceil_abs(e.j + j);
        const int rz = ceil_abs(e.k + k);
        const int m  = (rx > ry ? (rx > rz ? rx : rz) : (ry > rz ? ry : rz));
        if (m > r) r = m;
      }
      return r;
    }


    void SetSymbol(const char* s) {
        symbol = s;
    }
    
    void MoveBase(cFrac iIn, cFrac jIn, cFrac kIn) {
        cFrac di = iIn - i;
        cFrac dj = jIn - j;
        cFrac dk = kIn - k;
        
        for (auto& elem : StencilData) {
            elem.i -= di;
            elem.j -= dj;
            elem.k -= dk;
        }
        
        i = iIn;
        j = jIn;
        k = kIn;
    }

    void SetBase(cFrac iIn, cFrac jIn, cFrac kIn) {
        i = iIn;
        j = jIn;
        k = kIn;
    }
    
    void PrintBase() const {
        i.Print();
        j.Print();
        k.Print();
        printf("\n");
    }
    
    static bool SortList(const cStencilElement& first, const cStencilElement& second) {
        if (first.i < second.i) return true;
        if (first.i > second.i) return false;
        
        if (first.j < second.j) return true;
        if (first.j > second.j) return false;
        
        if (first.k < second.k) return true;
        
        return false;
    }
    
    void Print(FILE* fout) const {
        cFrac t;
        int cnt = 0;

        // Create a copy for sorting without modifying original
        std::list<cStencilElement> sortedData = StencilData;
        sortedData.sort(SortList);

	fprintf(fout,"%s\n",symbol.c_str());
        
        for (const auto& elem : sortedData) {
            fprintf(fout, "%i ", cnt++);
            
            t = i + elem.i;
            t.Print(fout);
            
            t = j + elem.j;
            t.Print(fout);
            
            t = k + elem.k;
            t.Print(fout);
            
            fprintf(fout, " %e\n", elem.a);
        }
    }

    void Print(const char* fname = nullptr) const {
        if (fname == nullptr) {
            Print(stdout);
        } else {
            FILE* fout = fopen(fname, "w");
            if (fout == nullptr) {
                throw std::runtime_error("Could not open file for writing");
            }
            Print(fout);
            fclose(fout);
        }
    }
    
    // Remove duplicate elements and combine coefficients
    void Simplify() {
        auto l0 = StencilData.begin();
        
        while (l0 != StencilData.end()) {
            auto l1 = l0;
            ++l1;
            
            while (l1 != StencilData.end()) {
                if ((l0->i == l1->i) && (l0->j == l1->j) && (l0->k == l1->k)) {
                    l0->a += l1->a;
                    l1 = StencilData.erase(l1);
                } else {
                    ++l1;
                }
            }
            ++l0;
        }
        
        // Remove elements with zero coefficients
        StencilData.remove_if([](const cStencilElement& elem) {
            return std::abs(elem.a) < 1e-15;
        });
    }

    // Get stencil as vector for faster access
    const std::vector<cStencilElement>& GetStencil() {
        Simplify();
        StencilCache.clear();
        StencilCache.reserve(StencilData.size());
        
        for (const auto& elem : StencilData) {
            StencilCache.push_back(elem);
        }
        
        return StencilCache;
    }

    int GetStencilLength() const {
        return static_cast<int>(StencilData.size());
    }

    void ExportStencil(cStencilData* s,double scale=1.0) {
        if (s == nullptr) {
            throw std::invalid_argument("Null pointer passed to ExportStencil");
        }

        Simplify();
        
        s->remove();
        s->Length = static_cast<int>(StencilData.size());
        s->Data.resize(s->Length);

        int it = 0;
        for (const auto& elem : StencilData) {
            s->Data[it].a = elem.a*scale;
            // Add fractions first, then convert to ensure correct handling
            // of fractional positions (e.g., i=1/2 + elem.i=1/2 = 1)
            s->Data[it].i = (i + elem.i).Convert2Int();
            s->Data[it].j = (j + elem.j).Convert2Int();
            s->Data[it].k = (k + elem.k).Convert2Int();
            ++it;
        }
    }

    // Multiply entire stencil by a constant
    cStencil& operator*=(double t) {
        for (auto& elem : StencilData) {
            elem.a *= t;
        }
        return *this;
    }

    // Add a new element to the stencil
    void add(double a, cFrac iIn, cFrac jIn, cFrac kIn) {
        cStencilElement NewElement;
        NewElement.a = a;
        NewElement.i = iIn;
        NewElement.j = jIn;
        NewElement.k = kIn;
        StencilData.push_back(NewElement);
    }

    void add(double a, int iIn, int jIn, int kIn) {
        add(a, cFrac(iIn, 1), cFrac(jIn, 1), cFrac(kIn, 1));
    }

    // Shift the entire stencil
    void shift(cFrac di, cFrac dj, cFrac dk) {
        i += di;
        j += dj;
        k += dk;
    }

    void shift(int di, int dj, int dk) {
        shift(cFrac(di, 1), cFrac(dj, 1), cFrac(dk, 1));
    }

    // Copy assignment operator
    cStencil& operator=(const cStencil& v) {
        if (this != &v) {
            i = v.i;
            j = v.j;
            k = v.k;
            symbol = v.symbol;
            StencilData = v.StencilData;
            StencilCache = v.StencilCache;
        }
        return *this;
    }
    
    // Move assignment operator
    cStencil& operator=(cStencil&& v) noexcept {
        if (this != &v) {
            i = std::move(v.i);
            j = std::move(v.j);
            k = std::move(v.k);
            symbol = std::move(v.symbol);
            StencilData = std::move(v.StencilData);
            StencilCache = std::move(v.StencilCache);
        }
        return *this;
    }

    friend void copy_shifted(cStencil* target, cStencil* source, int di, int dj, int dk) {
        if (target == nullptr || source == nullptr) {
            throw std::invalid_argument("Null pointer passed to copy_shifted");
        }
        *target = *source;
        target->shift(di, dj, dk);
    }

    // Add another stencil (FIXED BUG: was using v2.i for all three coordinates)
    cStencil& operator+=(const cStencil& v2) {
        for (const auto& elem : v2.StencilData) {
            cStencilElement NewElement = elem;
            
            NewElement.i += v2.i - i;
            NewElement.j += v2.j - j;  // FIXED: was v2.i - j
            NewElement.k += v2.k - k;  // FIXED: was v2.i - k
      
            StencilData.push_back(NewElement);
        }
        return *this;
    }

    cStencil& operator-=(const cStencil& rhs) {
      cStencil tmp = rhs;
      tmp*=-1.0;
      return (*this += tmp);
    }

    //Clear
    void Clear() {
      // Remove all coefficient entries and any cached view
      StencilData.clear();
      StencilCache.clear();
      // (Deliberately do NOT reset i, j, k, or symbol.)
      // If you ever need a full reset, add a separate Reset() that also zeros i,j,k and clears symbol.
    }

    //Reset
    void Reset() {
      // 1) Drop all coefficient entries/caches.
      Clear();  // calls StencilData.clear(); StencilCache.clear();

      // 2) Restore reference offsets (corner/center shift) to zero.
      //    cFrac(0,1) keeps exact zero without changing your rational arithmetic semantics.
      i = cFrac(0,1);
      j = cFrac(0,1);
      k = cFrac(0,1);

      // 3) Reset any identifying tag for the stencil (e.g., "curlBx", "curlBy", "curlBz", etc.).
      symbol.clear();
    }

    void SwitchAxes(int Axis0, int Axis1) {
        if (Axis0 < 0 || Axis0 > 2 || Axis1 < 0 || Axis1 > 2) {
            throw std::invalid_argument("Axis indices must be 0, 1, or 2");
        }
        
        if (Axis0 == Axis1) return;

        for (auto& elem : StencilData) {
            switch (3 * Axis0 + Axis1) {
            case 3 * 0 + 1:  // Swap i and j
                std::swap(elem.i, elem.j);
                break;
            case 3 * 0 + 2:  // Swap i and k
                std::swap(elem.i, elem.k);
                break;
            case 3 * 1 + 0:  // Swap j and i
                std::swap(elem.i, elem.j);
                break;
            case 3 * 1 + 2:  // Swap j and k
                std::swap(elem.j, elem.k);
                break;
            case 3 * 2 + 0:  // Swap k and i
                std::swap(elem.i, elem.k);
                break;
            case 3 * 2 + 1:  // Swap k and j
                std::swap(elem.j, elem.k);
                break;
            }
        }
    }

    void AddShifted(const cStencil& v, cFrac di, cFrac dj, cFrac dk, double c = 1.0) {
        for (const auto& elem : v.StencilData) {
            cStencilElement NewElement = elem;
            NewElement.a *= c;
            
            NewElement.i += v.i - i + di;
            NewElement.j += v.j - j + dj;
            NewElement.k += v.k - k + dk;

            StencilData.push_back(NewElement);
        }
    }

    void AddShifted(const cStencil& v, int di, int dj, int dk, double c = 1.0) {
        AddShifted(v, cFrac(di, 1), cFrac(dj, 1), cFrac(dk, 1), c);
    }

    void SubstractShifted(const cStencil& v, cFrac di, cFrac dj, cFrac dk, double c = 1.0) {
        AddShifted(v, di, dj, dk, -c);
    }

    void SubstractShifted(const cStencil& v, int di, int dj, int dk, double c = 1.0) {
        AddShifted(v, di, dj, dk, -c);
    }

    // Friend operators
    friend cStencil operator*(cStencil v1, double t) {
        v1 *= t;
        return v1;
    }

    friend cStencil operator*(double t, cStencil v1) {
        return v1 * t;
    }

    friend cStencil operator+(cStencil v1, const cStencil& v2) {
        v1 += v2;
        return v1;
    }

    friend cStencil operator-(cStencil v1, const cStencil& v2) {
        v1.AddShifted(v2, 0, 0, 0, -1.0);
        return v1;
    }
};

#endif /* _GENERAL_STENCIL_H_ */
