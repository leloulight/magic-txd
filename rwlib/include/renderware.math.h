/*
    RenderWare math definitions.
    Every good engine needs stable math, so we provide you with it!
*/

#ifndef _RENDERWARE_MATH_GLOBALS_
#define _RENDERWARE_MATH_GLOBALS_

#pragma warning(push)
#pragma warning(disable:4520)

// TODO: add support for more compilers.
#undef _SUPPORTS_CONSTEXPR

#if defined(_MSC_VER)
#if _MSC_VER >= 1900
#define _SUPPORTS_CONSTEXPR
#endif
#else
// Unknown compiler.
#define _SUPPORTS_CONSTEXPR
#endif

#define FASTMATH __declspec(noalias)

template <typename numberType, size_t dimm>
struct complex_assign_helper
{
    template <typename valType>
    AINLINE static void AssignStuff( numberType *elems, size_t n, valType&& theVal )
    {
        elems[ n ] = (numberType)theVal;
    }

    template <size_t n, typename curVal, typename... Args>
    struct constr_helper
    {
        AINLINE static void ConstrVecElem( numberType *elems, curVal&& theVal, Args... leftArgs )
        {
            AssignStuff( elems, n, theVal );

            constr_helper <n + 1, Args...>::ConstrVecElem( elems, std::forward <Args> ( leftArgs )... );
        }
    };

    template <size_t n>
    struct constr_helper_ex
    {
        AINLINE static void ConstrVecElem( numberType *elems )
        {
            elems[ n ] = numberType();

            constr_helper_ex <n + 1>::ConstrVecElem( elems );
        }
    };

    template <>
    struct constr_helper_ex <dimm>
    {
        AINLINE static void ConstrVecElem( numberType *elems )
        {
            // Nothing.
        }
    };

    template <size_t n, typename curVal>
    struct constr_helper <n, curVal>
    {
        AINLINE static void ConstrVecElem( numberType *elems, curVal&& theVal )
        {
            AssignStuff( elems, n, theVal );

            constr_helper_ex <n + 1>::ConstrVecElem( elems );
        }
    };
};

template <typename numberType, size_t dimm>
struct Vector
{
    AINLINE FASTMATH Vector( void )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] = numberType();
        }
    }

    template <typename... Args,
        typename = typename std::enable_if<
            ( sizeof...(Args) <= dimm && sizeof...(Args) != 0 )
        >::type>
    AINLINE FASTMATH explicit Vector( Args&&... theStuff )
    {
        complex_assign_helper <numberType, dimm>::
            constr_helper <0, Args...>::
                ConstrVecElem( this->elems, std::forward <Args> ( theStuff )... );
    }

    AINLINE FASTMATH Vector( std::initializer_list <numberType> args )
    {
        size_t n = 0;

        for ( numberType iter : args )
        {
            this->elems[ n ] = iter;

            n++;
        }
    }

    AINLINE FASTMATH Vector( const Vector& right )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] = right.elems[ n ];
        }
    }
    
    AINLINE FASTMATH Vector( Vector&& right )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] = std::move( right.elems[ n ] );
        }
    }

    AINLINE FASTMATH Vector& operator = ( const Vector& right )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] = right.elems[ n ];
        }

        return *this;
    }

    AINLINE FASTMATH Vector& operator = ( Vector&& right )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] = std::move( right.elems[ n ] );
        }

        return *this;
    }

    // Basic vector math.
    AINLINE FASTMATH Vector operator + ( const Vector& right ) const
    {
        Vector newVec = *this;

        for ( size_t n = 0; n < dimm; n++ )
        {
            newVec.elems[ n ] += right.elems[ n ];
        }

        return newVec;
    }

    AINLINE FASTMATH Vector& operator += ( const Vector& right )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] += right.elems[ n ];
        }

        return *this;
    }

    AINLINE FASTMATH Vector operator * ( numberType&& val ) const
    {
        Vector newVec = *this;

        for ( size_t n = 0; n < dimm; n++ )
        {
            newVec.elems[ n ] *= val;
        }

        return newVec;
    }

    AINLINE FASTMATH Vector& operator *= ( numberType&& val )
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            this->elems[ n ] *= val;
        }

        return *this;
    }

    AINLINE FASTMATH Vector operator / ( numberType&& val ) const
    {
        Vector newVec = *this;

        for ( size_t n = 0; n < dimm; n++ )
        {
            newVec.elems[ n ] /= val;
        }

        return newVec;
    }

    AINLINE FASTMATH bool operator == ( const Vector& right ) const
    {
        for ( size_t n = 0; n < dimm; n++ )
        {
            if ( this->elems[ n ] != right.elems[ n ] )
            {
                return false;
            }
        }

        return true;
    }
    AINLINE FASTMATH bool operator != ( const Vector& right ) const
    {
        return !( *this == right );
    }

    AINLINE FASTMATH numberType& operator [] ( ptrdiff_t n )
    {
        return elems[ n ];
    }

    AINLINE FASTMATH const numberType& operator [] ( ptrdiff_t n ) const
    {
        return elems[ n ];
    }

private:
    numberType elems[ dimm ];
};

// Special vectorized versions.
template <>
struct Vector <float, 4>
{
    AINLINE Vector( void ) : data() {}
    AINLINE Vector( const Vector& right ) : data( right.data )  {}
    AINLINE Vector( Vector&& right ) : data( std::move( right.data ) )  {}

    AINLINE Vector( float x, float y, float z, float w = 1.0f )
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                struct
                {
                    float a, b, c, d;
                };
                __m128 ssereg;
            };
        } tmp;

        tmp.a = x;
        tmp.b = y;
        tmp.c = z;
        tmp.d = w;

        this->data = tmp.ssereg;
    }

    AINLINE Vector& operator = ( const Vector& right )
    {
        this->data = right.data;

        return *this;
    }
    AINLINE Vector& operator = ( Vector&& right )
    {
        this->data = std::move( right.data );

        return *this;
    }

    // Addition.
    AINLINE Vector operator + ( const Vector& right ) const { Vector newVec = *this; newVec.data = _mm_add_ps( newVec.data, right.data ); return newVec; }
    AINLINE Vector operator + ( Vector&& right ) const      { Vector newVec = right; newVec.data = _mm_add_ps( newVec.data, this->data ); return newVec; }
    AINLINE Vector& operator += ( const Vector& right )     { this->data = _mm_add_ps( this->data, right.data ); return *this; }
    AINLINE Vector& operator += ( Vector&& right )          { this->data = _mm_add_ps( this->data, right.data ); return *this; }

private:
    AINLINE static __m128 loadExpanded( float val )
    { __m128 tmp = _mm_load_ss( &val ); tmp = _mm_shuffle_ps( tmp, tmp, _MM_SHUFFLE( 0, 0, 0, 0 ) ); return tmp; }

public:
    // By scalar.
    AINLINE Vector operator + ( float val ) const   { Vector newVec = *this; __m128 tmp = loadExpanded( val ); newVec.data = _mm_add_ps( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator += ( float val )        { __m128 tmp = loadExpanded( val ); this->data = _mm_add_ps( this->data, tmp ); return *this; }

    // Subtraction.
    AINLINE Vector operator - ( const Vector& right ) const { Vector newVec = *this; newVec.data = _mm_sub_ps( newVec.data, right.data ); return newVec; }
    AINLINE Vector operator - ( Vector&& right ) const      { Vector newVec = right; newVec.data = _mm_sub_ps( newVec.data, this->data ); return newVec; }
    AINLINE Vector& operator -= ( const Vector& right )     { this->data = _mm_sub_ps( this->data, right.data ); return *this; }
    AINLINE Vector& operator -= ( Vector&& right )          { this->data = _mm_sub_ps( this->data, right.data ); return *this; }

    // By scalar.
    AINLINE Vector operator - ( float val ) const   { Vector newVec = *this; __m128 tmp = loadExpanded( val ); newVec.data = _mm_sub_ps( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator -= ( float val )        { __m128 tmp = loadExpanded( val ); this->data = _mm_sub_ps( this->data, tmp ); return *this; }

    // Multiplication by scalar.
    AINLINE Vector operator * ( float val ) const   { Vector newVec = *this; __m128 tmp = loadExpanded( val ); newVec.data = _mm_mul_ps( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator *= ( float val )        { __m128 tmp = loadExpanded( val ); this->data = _mm_mul_ps( this->data, tmp ); return *this; }

    // Division by scalar.
    AINLINE Vector operator / ( float val ) const   { Vector newVec = *this; __m128 tmp = loadExpanded( val ); newVec.data = _mm_div_ps( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator /= ( float val )        { __m128 tmp = loadExpanded( val ); this->data = _mm_div_ps( this->data, tmp ); return *this; }

    AINLINE bool operator == ( const Vector& right ) const
    {
        __m128 cmpRes1 = _mm_cmpneq_ps( this->data, right.data );

        bool isSame = ( _mm_testz_ps( cmpRes1, cmpRes1 ) != 0 );

        return isSame;
    }
    AINLINE bool operator != ( const Vector& right ) const
    {
        return !( *this == right );
    }

    AINLINE float operator [] ( ptrdiff_t n ) const
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                __m128 ssereg;
                float f[4];
            };
        } tmp;

        tmp.ssereg = this->data;

        return tmp.f[ n ];
    }

    AINLINE float& operator [] ( ptrdiff_t n )
    {
        return *( (float*)this + n );
    }

private:
    __m128 data;
};

template <>
struct Vector <double, 2>
{
    AINLINE Vector( void ) : data() {}
    AINLINE Vector( const Vector& right ) : data( right.data )  {}
    AINLINE Vector( Vector&& right ) : data( std::move( right.data ) )  {}

    AINLINE Vector( double x, double y )
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                struct
                {
                    double a, b;
                };
                __m128d ssereg;
            };
        } tmp;

        tmp.a = x;
        tmp.b = y;

        this->data = tmp.ssereg;
    }

    AINLINE Vector& operator = ( const Vector& right )
    {
        this->data = right.data;

        return *this;
    }
    AINLINE Vector& operator = ( Vector&& right )
    {
        this->data = std::move( right.data );

        return *this;
    }

    // Addition.
    AINLINE Vector operator + ( const Vector& right ) const { Vector newVec = *this; newVec.data = _mm_add_pd( newVec.data, right.data ); return newVec; }
    AINLINE Vector operator + ( Vector&& right ) const      { Vector newVec = right; newVec.data = _mm_add_pd( newVec.data, this->data ); return newVec; }
    AINLINE Vector& operator += ( const Vector& right )     { this->data = _mm_add_pd( this->data, right.data ); return *this; }
    AINLINE Vector& operator += ( Vector&& right )          { this->data = _mm_add_pd( this->data, right.data ); return *this; }

private:
    AINLINE static __m128d loadExpanded( double val )
    { __m128d tmp = _mm_load_sd( &val ); tmp = _mm_shuffle_pd( tmp, tmp, _MM_SHUFFLE2(0, 0) ); return tmp; }

public:
    // By scalar.
    AINLINE Vector operator + ( double val ) const  { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data = _mm_add_pd( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator += ( double val )       { __m128d tmp = loadExpanded( val ); this->data = _mm_add_pd( this->data, tmp ); return *this; }

    // Subtraction.
    AINLINE Vector operator - ( const Vector& right ) const { Vector newVec = *this; newVec.data = _mm_sub_pd( newVec.data, right.data ); return newVec; }
    AINLINE Vector operator - ( Vector&& right ) const      { Vector newVec = right; newVec.data = _mm_sub_pd( newVec.data, this->data ); return newVec; }
    AINLINE Vector& operator -= ( const Vector& right )     { this->data = _mm_sub_pd( this->data, right.data ); return *this; }
    AINLINE Vector& operator -= ( Vector&& right )          { this->data = _mm_sub_pd( this->data, right.data ); return *this; }

    // By scalar.
    AINLINE Vector operator - ( double val ) const  { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data = _mm_sub_pd( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator -= ( double val )       { __m128d tmp = loadExpanded( val ); this->data = _mm_sub_pd( this->data, tmp ); return *this; }

    // Multiplication by scalar.
    AINLINE Vector operator * ( double val ) const  { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data = _mm_mul_pd( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator *= ( double val )       { __m128d tmp = loadExpanded( val ); this->data = _mm_mul_pd( this->data, tmp ); return *this; }

    // Division by scalar.
    AINLINE Vector operator / ( double val ) const  { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data = _mm_div_pd( newVec.data, tmp ); return newVec; }
    AINLINE Vector operator /= ( double val )       { __m128d tmp = loadExpanded( val ); this->data = _mm_div_pd( this->data, tmp ); return *this; }

    AINLINE bool operator == ( const Vector& right ) const
    {
        __m128d cmpRes1 = _mm_cmpneq_pd( this->data, right.data );

        bool isSame = ( _mm_testz_pd( cmpRes1, cmpRes1 ) != 0 );

        return isSame;
    }
    AINLINE bool operator != ( const Vector& right ) const
    {
        return !( *this == right );
    }

    AINLINE double operator [] ( ptrdiff_t n ) const
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                __m128d ssereg;
                double d[2];
            };
        } tmp;

        tmp.ssereg = this->data;

        return tmp.d[ n ];
    }

    AINLINE double& operator [] ( ptrdiff_t n )
    {
        return *( (double*)this + n );
    }

private:
    __m128d data;
};

template <>
struct Vector <double, 4>
{
    AINLINE Vector( void ) : data1(), data2() {}
    AINLINE Vector( const Vector& right ) : data1( right.data1 ), data2( right.data2 )  {}
    AINLINE Vector( Vector&& right ) : data1( std::move( right.data1 ) ), data2( std::move( right.data2 ) )  {}

    AINLINE Vector( double x, double y, double z, double w = 1.0 )
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                struct
                {
                    double a, b, c, d;
                };
                struct
                {
                    __m128d ssereg1;
                    __m128d ssereg2;
                };
            };
        } tmp;

        tmp.a = x;
        tmp.b = y;
        tmp.c = z;
        tmp.d = w;

        this->data1 = tmp.ssereg1;
        this->data2 = tmp.ssereg2;
    }

    AINLINE Vector& operator = ( const Vector& right )
    {
        this->data1 = right.data1;
        this->data2 = right.data2;

        return *this;
    }
    AINLINE Vector& operator = ( Vector&& right )
    {
        this->data1 = std::move( right.data1 );
        this->data2 = std::move( right.data2 );

        return *this;
    }

    // Addition.
    AINLINE Vector operator + ( const Vector& right ) const
    { Vector newVec = *this; newVec.data1 = _mm_add_pd( newVec.data1, right.data1 ); newVec.data2 = _mm_add_pd( newVec.data2, right.data2 ); return newVec; }
    AINLINE Vector operator + ( Vector&& right ) const
    { Vector newVec = right; newVec.data1 = _mm_add_pd( newVec.data1, this->data1 ); newVec.data2 = _mm_add_pd( newVec.data2, this->data2 ); return newVec; }
    AINLINE Vector& operator += ( const Vector& right )
    { this->data1 = _mm_add_pd( this->data1, right.data1 ); this->data2 = _mm_add_pd( this->data2, right.data2 ); return *this; }
    AINLINE Vector& operator += ( Vector&& right )
    { this->data1 = _mm_add_pd( this->data1, right.data1 ); this->data2 = _mm_add_pd( this->data2, right.data2 ); return *this; }

private:
    AINLINE static __m128d loadExpanded( double val )
    { __m128d tmp = _mm_load_sd( &val ); tmp = _mm_shuffle_pd( tmp, tmp, _MM_SHUFFLE2(0, 0) ); return tmp; }

public:
    // By scalar.
    AINLINE Vector operator + ( double val ) const
    { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data1 = _mm_add_pd( newVec.data1, tmp ); newVec.data2 = _mm_add_pd( newVec.data2, tmp ); return newVec; }
    AINLINE Vector operator += ( double val )
    { __m128d tmp = loadExpanded( val ); this->data1 = _mm_add_pd( this->data1, tmp ); this->data2 = _mm_add_pd( this->data2, tmp ); return *this; }

    // Subtraction.
    AINLINE Vector operator - ( const Vector& right ) const
    { Vector newVec = *this; newVec.data1 = _mm_sub_pd( newVec.data1, right.data1 ); newVec.data2 = _mm_sub_pd( newVec.data2, right.data2 ); return newVec; }
    AINLINE Vector operator - ( Vector&& right ) const
    { Vector newVec = right; newVec.data1 = _mm_sub_pd( newVec.data1, this->data1 ); newVec.data2 = _mm_sub_pd( newVec.data2, this->data2 ); return newVec; }
    AINLINE Vector& operator -= ( const Vector& right )
    { this->data1 = _mm_sub_pd( this->data1, right.data1 ); this->data2 = _mm_sub_pd( this->data2, right.data2 ); return *this; }
    AINLINE Vector& operator -= ( Vector&& right )
    { this->data1 = _mm_sub_pd( this->data1, right.data1 ); this->data2 = _mm_sub_pd( this->data2, right.data2 ); return *this; }

    // By scalar.
    AINLINE Vector operator - ( double val ) const
    { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data1 = _mm_sub_pd( newVec.data1, tmp ); newVec.data2 = _mm_sub_pd( newVec.data2, tmp ); return newVec; }
    AINLINE Vector operator -= ( double val )
    { __m128d tmp = loadExpanded( val ); this->data1 = _mm_sub_pd( this->data1, tmp ); this->data2 = _mm_sub_pd( this->data2, tmp ); return *this; }

    // Multiplication by scalar.
    AINLINE Vector operator * ( double val ) const
    { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data1 = _mm_mul_pd( newVec.data1, tmp ); newVec.data2 = _mm_mul_pd( newVec.data2, tmp ); return newVec; }
    AINLINE Vector operator *= ( double val )
    { __m128d tmp = loadExpanded( val ); this->data1 = _mm_mul_pd( this->data1, tmp ); this->data2 = _mm_mul_pd( this->data2, tmp ); return *this; }

    // Division by scalar.
    AINLINE Vector operator / ( double val ) const
    { Vector newVec = *this; __m128d tmp = loadExpanded( val ); newVec.data1 = _mm_div_pd( newVec.data1, tmp ); newVec.data2 = _mm_div_pd( newVec.data2, tmp ); return newVec; }
    AINLINE Vector operator /= ( double val )
    { __m128d tmp = loadExpanded( val ); this->data1 = _mm_div_pd( this->data1, tmp ); this->data2 = _mm_div_pd( this->data2, tmp ); return *this; }

    AINLINE bool operator == ( const Vector& right ) const
    {
        __m128d cmpRes1 = _mm_cmpneq_pd( this->data1, right.data1 );
        __m128d cmpRes2 = _mm_cmpneq_pd( this->data2, right.data2 );

        bool isSame = ( _mm_testz_pd( cmpRes1, cmpRes1 ) != 0 && _mm_testz_pd( cmpRes2, cmpRes2 ) != 0 );

        return isSame;
    }
    AINLINE bool operator != ( const Vector& right ) const
    {
        return !( *this == right );
    }

    AINLINE double operator [] ( ptrdiff_t n ) const
    {
        struct __declspec( align( 16 ) )
        {
            union
            {
                struct
                {
                    __m128d ssereg1;
                    __m128d ssereg2;
                };
                double d[4];
            };
        } tmp;

        tmp.ssereg1 = this->data1;
        tmp.ssereg2 = this->data2;

        return tmp.d[ n ];
    }

    AINLINE double& operator [] ( ptrdiff_t n )
    {
        return *( (double*)this + n );
    }

private:
    __m128d data1;
    __m128d data2;
};

// Include the matrix view submodule.
#include "renderware.math.matview.h"

#pragma warning(pop)

#endif //_RENDERWARE_MATH_GLOBALS_