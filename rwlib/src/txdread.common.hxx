namespace rw
{

struct texDictionaryStreamPlugin : public serializationProvider
{
    inline void Initialize( Interface *engineInterface )
    {
        txdTypeInfo = engineInterface->typeSystem.RegisterStructType <TexDictionary> ( "texture_dictionary" );

        if ( txdTypeInfo )
        {
            engineInterface->typeSystem.SetTypeInfoInheritingClass( txdTypeInfo, engineInterface->rwobjTypeInfo );
        }
    }

    inline void Shutdown( Interface *engineInterface )
    {
        if ( RwTypeSystem::typeInfoBase *txdTypeInfo = this->txdTypeInfo )
        {
            engineInterface->typeSystem.DeleteType( txdTypeInfo );
        }
    }

    // Creation functions.
    TexDictionary*  CreateTexDictionary( Interface *engineInterface ) const;
    TexDictionary*  ToTexDictionary( Interface *engineInterface, RwObject *rwObj );

    // Serialization functions.
    void        Serialize( Interface *engineInterface, BlockProvider& outputProvider, RwObject *objectToSerialize ) const;
    void        Deserialize( Interface *engineInterface, BlockProvider& inputProvider, RwObject *objectToDeserialize ) const;

    RwTypeSystem::typeInfoBase *txdTypeInfo;
};

inline void fixFilteringMode(TextureBase& inTex, uint32 mipmapCount)
{
    eRasterStageFilterMode currentFilterMode = inTex.GetFilterMode();

    eRasterStageFilterMode newFilterMode = currentFilterMode;

    if ( mipmapCount > 1 )
    {
        if ( currentFilterMode == RWFILTER_POINT )
        {
            newFilterMode = RWFILTER_POINT_POINT;
        }
        else if ( currentFilterMode == RWFILTER_LINEAR )
        {
            newFilterMode = RWFILTER_LINEAR_LINEAR;
        }
    }
    else
    {
        if ( currentFilterMode == RWFILTER_POINT_POINT ||
             currentFilterMode == RWFILTER_POINT_LINEAR )
        {
            newFilterMode = RWFILTER_POINT;
        }
        else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                  currentFilterMode == RWFILTER_LINEAR_LINEAR )
        {
            newFilterMode = RWFILTER_LINEAR;
        }
    }

    // If the texture requires a different filter mode, set it.
    if ( currentFilterMode != newFilterMode )
    {
        Interface *engineInterface = inTex.engineInterface;

        bool ignoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

        int warningLevel = engineInterface->GetWarningLevel();

        if ( ignoreSecureWarnings == false )
        {
            // Since this is a really annoying message, put it on warning level 4.
            if ( warningLevel >= 4 )
            {
                engineInterface->PushWarning( "texture " + inTex.GetName() + " has an invalid filtering mode (fixed)" );
            }
        }

        // Fix it.
        inTex.SetFilterMode( newFilterMode ); 
    }
}

// Processor of mipmap levels.
struct mipGenLevelGenerator
{
private:
    uint32 mipWidth, mipHeight;
    bool generateToMinimum;

    bool hasIncrementedWidth;
    bool hasIncrementedHeight;

public:
    inline mipGenLevelGenerator( uint32 baseWidth, uint32 baseHeight )
    {
        this->mipWidth = baseWidth;
        this->mipHeight = baseHeight;

        this->generateToMinimum = true;//false;

        this->hasIncrementedWidth = false;
        this->hasIncrementedHeight = false;
    }

    inline void setGenerateToMinimum( bool doGenMin )
    {
        this->generateToMinimum = doGenMin;
    }

    inline uint32 getLevelWidth( void ) const
    {
        return this->mipWidth;
    }

    inline uint32 getLevelHeight( void ) const
    {
        return this->mipHeight;
    }

    inline bool canIncrementDimm( uint32 dimm )
    {
        return ( dimm != 1 );
    }

    inline bool incrementLevel( void )
    {
        bool couldIncrementLevel = false;

        uint32 curMipWidth = this->mipWidth;
        uint32 curMipHeight = this->mipHeight;

        bool doGenMin = this->generateToMinimum;

        if ( doGenMin )
        {
            if ( canIncrementDimm( curMipWidth ) || canIncrementDimm( curMipHeight ) )
            {
                couldIncrementLevel = true;
            }
        }
        else
        {
            if ( canIncrementDimm( curMipWidth ) && canIncrementDimm( curMipHeight ) )
            {
                couldIncrementLevel = true;
            }
        }

        if ( couldIncrementLevel )
        {
            bool hasIncrementedWidth = false;
            bool hasIncrementedHeight = false;

            if ( canIncrementDimm( curMipWidth ) )
            {
                this->mipWidth = curMipWidth / 2;

                hasIncrementedWidth = true;
            }

            if ( canIncrementDimm( curMipHeight ) )
            {
                this->mipHeight = curMipHeight / 2;

                hasIncrementedHeight = true;
            }

            // Update feedback parameters.
            this->hasIncrementedWidth = hasIncrementedWidth;
            this->hasIncrementedHeight = hasIncrementedHeight;
        }

        return couldIncrementLevel;
    }

    inline bool isValidLevel( void ) const
    {
        return ( this->mipWidth != 0 && this->mipHeight != 0 );
    }

    inline bool didIncrementWidth( void ) const
    {
        return this->hasIncrementedWidth;
    }

    inline bool didIncrementHeight( void ) const
    {
        return this->hasIncrementedHeight;
    }
};

inline eRasterFormat getVirtualRasterFormat( bool hasAlpha, eCompressionType rwCompressionType )
{
    eRasterFormat rasterFormat = RASTER_DEFAULT;

    if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
	    if (hasAlpha)
        {
		    rasterFormat = RASTER_1555;
        }
	    else
        {
		    rasterFormat = RASTER_565;
        }
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 ||
              rwCompressionType == RWCOMPRESS_DXT3 )
    {
        rasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        rasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        rasterFormat = RASTER_4444;
    }

    return rasterFormat;
}

};