#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>

#ifdef HASMKL
#include <mkl_lapack.h>
#include <mkl_lapacke.h>
#else // HASMKL
#ifdef __MACH__
#include <clapack.h>
#include <Accelerate/Accelerate.h>
#endif // __MACH__
#endif // HASMKL

class Matrix {

public:

    Matrix(size_t nrow, size_t ncol, bool column_major)
      : m_nrow(nrow), m_ncol(ncol), m_column_major(column_major)
    {
        reset_buffer(nrow, ncol);
    }

    Matrix
    (
        size_t nrow, size_t ncol, bool column_major
      , std::vector<double> const & vec
    )
      : m_nrow(nrow), m_ncol(ncol), m_column_major(column_major)
    {
        reset_buffer(nrow, ncol);
        (*this) = vec;
    }

    Matrix & operator=(std::vector<double> const & vec)
    {
        if (size() != vec.size())
        {
            throw std::out_of_range("number of elements mismatch");
        }

        size_t k = 0;
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = vec[k];
                ++k;
            }
        }

        return *this;
    }

    Matrix(Matrix const & other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
      , m_column_major(other.m_column_major)
    {
        reset_buffer(other.m_nrow, other.m_ncol);
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
    }

    Matrix & operator=(Matrix const & other)
    {
        if (this == &other) { return *this; }
        if (m_nrow != other.m_nrow || m_ncol != other.m_ncol)
        {
            reset_buffer(other.m_nrow, other.m_ncol);
        }
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
        return *this;
    }

    Matrix(Matrix && other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
      , m_column_major(other.m_column_major)
    {
        reset_buffer(0, 0);
        std::swap(m_buffer, other.m_buffer);
    }

    Matrix & operator=(Matrix && other)
    {
        if (this == &other) { return *this; }
        reset_buffer(0, 0);
        std::swap(m_nrow, other.m_nrow);
        std::swap(m_ncol, other.m_ncol);
        std::swap(m_buffer, other.m_buffer);
        m_column_major = other.m_column_major;
        return *this;
    }

    ~Matrix()
    {
        reset_buffer(0, 0);
    }

    double   operator() (size_t row, size_t col) const
    {
        return m_buffer[index(row, col)];
    }
    double & operator() (size_t row, size_t col)
    {
        return m_buffer[index(row, col)];
    }

    size_t nrow() const { return m_nrow; }
    size_t ncol() const { return m_ncol; }

    size_t size() const { return m_nrow * m_ncol; }
    double buffer(size_t i) const { return m_buffer[i]; }
    std::vector<double> buffer_vector() const
    {
        return std::vector<double>(m_buffer, m_buffer+size());
    }

    double * data() const { return m_buffer; }

private:

    size_t index(size_t row, size_t col) const
    {
        if (m_column_major) { return row          + col * m_nrow; }
        else                { return row * m_ncol + col         ; }
    }

    void reset_buffer(size_t nrow, size_t ncol)
    {
        if (m_buffer) { delete[] m_buffer; }
        const size_t nelement = nrow * ncol;
        if (nelement) { m_buffer = new double[nelement]; }
        else          { m_buffer = nullptr; }
        m_nrow = nrow;
        m_ncol = ncol;
    }

    size_t m_nrow = 0;
    size_t m_ncol = 0;
    bool m_column_major = false;
    double * m_buffer = nullptr;

};

std::ostream & operator << (std::ostream & ostr, Matrix const & mat)
{
    for (size_t i=0; i<mat.nrow(); ++i)
    {
        ostr << std::endl << " ";
        for (size_t j=0; j<mat.ncol(); ++j)
        {
            ostr << " " << std::setw(2) << mat(i, j);
        }
    }

    ostr << std::endl << " data: ";
    for (size_t i=0; i<mat.size(); ++i)
    {
        ostr << " " << std::setw(2) << mat.data()[i];
    }

    return ostr;
}

std::ostream & operator << (std::ostream & ostr, std::vector<double> const & vec)
{
    for (size_t i=0; i<vec.size(); ++i)
    {
        std::cout << " " << vec[i];
    }

    return ostr;
}

/*
 * See references:
 * * https://software.intel.com/en-us/mkl-developer-reference-c-geev
 * * https://software.intel.com/sites/products/documentation/doclib/mkl_sa/11/mkl_lapack_examples/lapacke_dgeev_row.c.htm
 */
int main(int argc, char ** argv)
{
    const size_t n = 3;
    int status;

    std::cout << ">>> Solve Ax=lx (row major)" << std::endl;
    Matrix mat(n, n, /* column_major */ true);
    mat(0,0) = 3; mat(0,1) = 5; mat(0,2) = 2;
    mat(1,0) = 2; mat(1,1) = 1; mat(1,2) = 3;
    mat(2,0) = 4; mat(2,1) = 3; mat(2,2) = 2;
    std::vector<double> wr(n), wi(n);
    Matrix vl(n, n, /* column_major */ true), vr(n, n, /* column_major */ true);

    std::cout << "A:" << mat << std::endl;

#if !defined(HASMKL) || defined(NOMKL)
    {
        char jobvl = 'V';
        char jobvr = 'V';
        int nn = n;
        int vlnrow = vl.nrow();
        int vrnrow = vr.nrow();
        int matnrow = mat.nrow();
        int lwork = 4*n;
        std::vector<double> work(lwork);

        dgeev_( // column major.
            &jobvl
          , &jobvr
          , &nn // int * n: number of linear equation
          , mat.data() // double *: a
          , &matnrow // int *: lda
          , wr.data() // double *: wr
          , wi.data() // double *: wi
          , vl.data() // double *: vl
          , &vlnrow // int *: ldvl
          , vr.data() // double *: vr
          , &vrnrow // int *: ldvr
          , work.data() // double *: work array
          , &lwork // int *: lwork
          , &status
          // for column major matrix, ldb remains the leading dimension.
        );
    }
#else // HASMKL NOMKL
    status = LAPACKE_dgeev(
        LAPACK_COL_MAJOR // int matrix_layout
      , 'V' // char jobvl; 'V' to compute left eigenvectors, 'N' to not compute them
      , 'V' // char jobvr; 'V' to compute right eigenvectors, 'N' to not compute them
      , n // lapack_int n
      , mat.data() // double * a
      , mat.nrow() // lapack_int lda
      , wr.data() // double * wr
      , wi.data() // double * wi
      , vl.data() // double * vl
      , vl.nrow() // lapack_int ldvl
      , vr.data() // double * vr
      , vr.nrow() // lapack_int ldvr
    );
#endif // HASMKL NOMKL

    std::cout << "dgeev status: " << status << std::endl;

    std::cout << "eigenvalues:" << std::endl;
    std::cout << "      (real)      (imag)" << std::endl;
    std::cout << std::fixed;
    for (size_t i = 0 ; i < n ; ++i )
    {
        std::cout
            << "(  " << std::setw(9) << wr[i]
            << ",  " << std::setw(9) << wi[i]
            << ")" << std::endl;
    }
    std::cout << std::resetiosflags(std::ios_base::basefield);

    auto print_eigenvectors = [&wi](std::ostream & ostr, Matrix const & mat)
    {
        for (size_t i = 0 ; i < mat.nrow() ; ++i)
        {
            size_t j = 0;
            while (j < mat.ncol())
            {
                if ( wi[j] == (double)0.0 )
                {
                    ostr << "   " << std::setw(9) << mat(i, j);
                    ++j;
                }
                else
                {
                    auto eimag = mat(i, j+1);
                    ostr
                        << " ( " << std::setw(9) << mat(i, j)
                        << ", "  << std::setw(9) << eimag << ")";
                    if (eimag != 0) { eimag = -eimag; }
                    ostr
                        << " ( " << std::setw(9) << mat(i, j)
                        << ", "  << std::setw(9) << eimag << ")";
                    j += 2;
                }
            }
            ostr << std::endl;
        }
    };

    std::cout << "left eigenvectors:" << std::endl;
    print_eigenvectors(std::cout, vl);

    std::cout << "right eigenvectors:" << std::endl;
    print_eigenvectors(std::cout, vr);

    return 0;
}

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
