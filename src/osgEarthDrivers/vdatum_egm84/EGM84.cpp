/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <osgEarth/VerticalDatum>
#include <osgEarth/Geoid>
#include <osgEarth/Units>
#include <osgDB/ReaderWriter>
#include <osgDB/Registry>
#include <osgDB/FileNameUtils>
#include "EGM84Grid.h"

using namespace osgEarth;

//--------------------------------------------------------------------------

namespace
{
    class EGM84VerticalDatum : public VerticalDatum
    {
    public:
        EGM84VerticalDatum() : VerticalDatum(
            "EGM84",                                  // readable name
            "egm84" )                                 // initialization string
        {
            // build a heightfield from the data.

            unsigned cols = 721, rows = 361;
            float colStep = 0.5f, rowStep = 0.5f;

            osg::HeightField* hf = new osg::HeightField();
            hf->allocate( cols, rows );
            osg::Vec3 origin(-180.f, -90.f, 0.f);
            hf->setOrigin( origin );
            hf->setXInterval( colStep );
            hf->setYInterval( rowStep );

            for( unsigned c=0; c<cols-1; ++c )
            {
                float inputLon = 0.0f + float(c) * colStep;
                if ( inputLon >= 180.0 ) inputLon -= 360.0;

                for( unsigned r=0; r<rows; ++r )
                {
                    float inputLat = 90.0f - float(r) * rowStep;

                    unsigned outc = unsigned( (inputLon-origin.x())/colStep );
                    unsigned outr = unsigned( (inputLat-origin.y())/rowStep );

                    Distance h( (double)s_egm84grid[r*cols+c], Units::CENTIMETERS );
                    hf->setHeight( outc, outr, float(h.as(Units::METERS)) );
                }
            }

            // copy the first column to the last column
            for(unsigned r=0; r<rows; ++r)
                hf->setHeight(cols-1, r, hf->getHeight(0, r));

            _geoid = new Geoid();
            _geoid->setHeightField( hf );
            _geoid->setUnits( Units::METERS );
            _geoid->setName( "EGM84" );
        }
    };
}


class EGM84VerticalDatumFactory : public osgDB::ReaderWriter
{
public:
    EGM84VerticalDatumFactory()
    {
        supportsExtension( "osgearth_vdatum_egm84", "osgEarth EGM84 vertical datum" );
    }

    virtual const char* className() const
    {
        return "osgEarth EGM84 vertical datum";
    }

    virtual ReadResult readObject(const std::string& file_name, const Options* options) const
    {
        if ( !acceptsExtension(osgDB::getLowerCaseFileExtension( file_name )))
            return ReadResult::FILE_NOT_HANDLED;

        return ReadResult( new EGM84VerticalDatum() );
    }
};

REGISTER_OSGPLUGIN(osgearth_vdatum_egm84, EGM84VerticalDatumFactory) 
