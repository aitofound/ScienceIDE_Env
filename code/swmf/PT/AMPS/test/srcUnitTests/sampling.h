//sampling routined used in tests 

#ifndef _SAMPLING_CLASSES_DEF_ 
#define _SAMPLING_CLASSES_DEF_ 

  class cSampledValues {
  public:
    double val;
    int cnt;

    void Reset() {val=0.0,cnt=0;}
    cSampledValues() {Reset();}

    double GetVal() {return (cnt!=0) ? val/cnt : 0.0;}

    // Overload += operator
    cSampledValues& operator+=(double value) {
        val += value;  // Add value to 'val'
        ++cnt;         // Increment 'cnt'
        return *this;  // Return *this to allow chaining
    }

    cSampledValues& operator=(double value) {
      val=value;  // Add value to 'val'
      cnt=1;         // Increment 'cnt'
      return *this;  // Return *this to allow chaining
    }

    // Conversion operator to double
    operator double() const {
      return (cnt!=0) ? val/cnt : 0.0;  // Return the average value
    }
  };

  struct cRelativeDiff {
    double operator ()  (double a, double b) const {
      return (a+b != 0.0) ? 2.0*fabs(a - b) / (a + b) : 0.0;
    }

    double operator () (cSampledValues a, cSampledValues b) const {
      double aa=a.GetVal();
      double bb=b.GetVal();

      return (aa+bb != 0.0) ? 2.0*fabs(aa - bb) / (aa + bb) : 0.0;
    }
  };

#endif
