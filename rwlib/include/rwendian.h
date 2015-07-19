// Endianness compatibility definitions.
namespace endian
{
    AINLINE bool is_little_endian( void )
    {
        // SHOULD be evaluating without runtime code.
        return ( *(unsigned short*)"\xFF\x00" == 0x00FF );
    }

    template <typename numberType>
    AINLINE numberType byte_swap_fast( const char *data )
    {
        char swapped_data[ sizeof(numberType) ];

        for ( unsigned int n = 0; n < sizeof(numberType); n++ )
        {
            swapped_data[ n ] = data[ ( sizeof(numberType) - 1 ) - n ];
        }

        return *(numberType*)swapped_data;
    }

    template <> AINLINE int16 byte_swap_fast( const char *data )    { return _byteswap_ushort( *(uint16*)data ); }
    template <> AINLINE uint16 byte_swap_fast( const char *data )   { return _byteswap_ushort( *(uint16*)data ); }
    template <> AINLINE int32 byte_swap_fast( const char *data )    { return _byteswap_ulong( *(uint32*)data ); }
    template <> AINLINE uint32 byte_swap_fast( const char *data )   { return _byteswap_ulong( *(uint32*)data ); }
    template <> AINLINE int64 byte_swap_fast( const char *data )    { return _byteswap_uint64( *(uint64*)data ); }
    template <> AINLINE uint64 byte_swap_fast( const char *data )   { return _byteswap_uint64( *(uint64*)data ); }

    template <typename numberType>
    struct big_endian
    {
        inline big_endian( void )
        {
            // Nothing really.
        }

        inline big_endian( const big_endian& right )
        {
            *(numberType*)this->data = *(numberType*)right.data;
        }

        inline big_endian( numberType right )
        {
            this->operator = ( right );
        }

        inline operator numberType( void ) const
        {
            if ( is_little_endian() )
            {
                return byte_swap_fast <numberType> ( this->data );
            }
            else
            {
                return *(numberType*)this->data;
            }
        }

        inline numberType operator = ( const numberType& right )
        {
            if ( is_little_endian() )
            {
                *(numberType*)data = byte_swap_fast <numberType> ( (const char*)&right );
            }
            else
            {
                *(numberType*)data = right;
            }

            return right;
        }

        inline big_endian& operator = ( const big_endian& right )
        {
            *(numberType*)this->data = *(numberType*)right.data;

            return *this;
        }

    private:
        char data[ sizeof(numberType) ];
    };

    template <typename numberType>
    struct little_endian
    {
        inline little_endian( void )
        {
            // Nothing really.
        }

        inline little_endian( const little_endian& right )
        {
            *(numberType*)this->data = *(numberType*)right.data;
        }

        inline little_endian( numberType right )
        {
            this->operator = ( right );
        }

        inline operator numberType( void ) const
        {
            if ( !is_little_endian() )
            {
                return byte_swap_fast <numberType> ( this->data );
            }
            else
            {
                return *(numberType*)this->data;
            }
        }

        inline numberType operator = ( const numberType& right )
        {
            if ( !is_little_endian() )
            {
                *(numberType*)data = byte_swap_fast <numberType> ( (const char*)&right );
            }
            else
            {
                *(numberType*)data = right;
            }

            return right;
        }

        inline little_endian& operator = ( const little_endian& right )
        {
            *(numberType*)this->data = *(numberType*)right.data;

            return *this;
        }

    private:
        char data[ sizeof(numberType) ];
    };
};