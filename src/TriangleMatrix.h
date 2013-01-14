#ifndef INC_TRIANGLEMATRIX_H
#define INC_TRIANGLEMATRIX_H
#include <cstddef> // size_t
#include "DataSet.h"
// Class: TriangleMatrix
/// Store the upper half of a symmetric matrix
/** Useful when calculating e.g. all N^2 distances between all atoms, 
  * where the diagonal elements would be zero and element i,j == element j,i.
  * Accepts doubles, but internal storage is float to reduce memory footprint
  */
class TriangleMatrix : public DataSet {
  public :
    int Nrows()                const { return (int)nrows_;            }
    size_t Nelements()         const { return nelements_;             }
    double operator[](int idx) const { return (double)elements_[idx]; }

    TriangleMatrix();
    TriangleMatrix(size_t);
    TriangleMatrix(const TriangleMatrix&);
    ~TriangleMatrix();
    TriangleMatrix & operator=(const TriangleMatrix &);

    int Setup(int);
    int AddElement(double);
    int AddElement(float);
    void SetElement(int,int,double);
    void SetElementF(int,int,float);
    double GetElement(int,int) const;
    float GetElementF(int,int) const;
    // DataSet functions
    int Xmax() { return (int)(nrows_ - 1L); }
    int Size() { return (int)nelements_;    }
    void Write2D( CpptrajFile&, int, int);
    void GetDimensions(std::vector<int>&);
  protected:
    float *elements_;       ///< Hold all elements
    size_t nrows_;          ///< Number of elements in one row
    size_t nelements_;      ///< Total number of elements
    size_t currentElement_; ///< Current element, used in AddElement only.
  private:
    size_t calcIndex(size_t,size_t) const;
};
#endif
