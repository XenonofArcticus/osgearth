
/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include <osgEarth/ElevationQuery>
#include <osgEarth/Map>
#include <osgSim/LineOfSight>

#define LC "[ElevationQuery] "

using namespace osgEarth;
using namespace osgEarth::Util;


ElevationQuery::ElevationQuery()
{
}

ElevationQuery::ElevationQuery(const Map* map)
{
    setMap(map);
}

void
ElevationQuery::setMap(const Map* map)
{
    _map = map;
}

void
ElevationQuery::reset()
{
    // set read callback for IntersectionVisitor
    _ivrc = new osgSim::DatabaseCacheReadCallback();

    _terrainModelLayers.clear();
    _elevationLayers.clear();

    osg::ref_ptr<const Map> map;
    if (_map.lock(map))
    {
        map->getLayers(_elevationLayers);
        
        // cache a vector of terrain patch models.
        LayerVector layers;
        map->getLayers(layers);
        for (LayerVector::const_iterator i = layers.begin(); i != layers.end(); ++i)
        {
            if (i->get()->options().terrainPatch() == true)
            {
                _terrainModelLayers.push_back(i->get());
            }
        }

        // revisions are now in sync.
        _mapRevision = map->getDataModelRevision();
    }
}
void
ElevationQuery::sync()
{
    osg::ref_ptr<const Map> map;
    if (_map.lock(map))
    {
        if (_mapRevision != map->getDataModelRevision())
        {
            reset();
        }
    }
}

void
ElevationQuery::gatherTerrainModelLayers(const Map* map)
{
    // cache a vector of terrain patch models.
    _terrainModelLayers.clear();
    LayerVector layers;
    map->getLayers(layers);
    for (LayerVector::const_iterator i = layers.begin();
        i != layers.end();
        ++i)
    {
        if (i->get()->options().terrainPatch() == true)
        {
            _terrainModelLayers.push_back(i->get());
        }
    }
}


float
ElevationQuery::getElevation(const GeoPoint& point,
                             double          desiredResolution,
                             double*         out_actualResolution)
{
    float result = NO_DATA_VALUE;

    sync();
    if ( point.altitudeMode() == ALTMODE_ABSOLUTE )
    {
        getElevationImpl( point, result, desiredResolution, out_actualResolution );
    }
    else
    {
        GeoPoint point_abs( point.getSRS(), point.x(), point.y(), 0.0, ALTMODE_ABSOLUTE );
        getElevationImpl( point_abs, result, desiredResolution, out_actualResolution );
    }

    return result;
}

bool
ElevationQuery::getElevations(std::vector<osg::Vec3d>& points,
                              const SpatialReference*  pointsSRS,
                              bool                     ignoreZ,
                              double                   desiredResolution )
{       
    if (_map->getNumTerrainPatchLayers() > 0)
    {
        sync();
        for (osg::Vec3dArray::iterator i = points.begin(); i != points.end(); ++i)
        {
            float elevation;
            double z = (*i).z();
            GeoPoint p(pointsSRS, *i, ALTMODE_ABSOLUTE);
            if (getElevationImpl(p, elevation, desiredResolution, 0L))
            {
                if (elevation == NO_DATA_VALUE)
                {
                    elevation = 0.0;
                }

                (*i).z() = ignoreZ ? elevation : elevation + z;
            }
        }
        return true;
    }
    else
    {
        // Call ElevationPool::sampleMapCoords directly if there are no terrain patches as it is significantly faster than doing
        // individual getElevationImpl queries
        if (pointsSRS != _map->getSRS())
        {
            std::vector< osg::Vec3d > mapPoints = points;
            pointsSRS->transform(mapPoints, _map->getSRS());
            int count = _map->getElevationPool()->sampleMapCoords(mapPoints.begin(), mapPoints.end(), Distance(desiredResolution, _map->getSRS()->getUnits()), nullptr, nullptr);
            for (unsigned int i = 0; i < points.size(); ++i)
            {
                points[i].z() = mapPoints[i].z();
            }
            return count > 0;
        }
        else
        {
            return _map->getElevationPool()->sampleMapCoords(points.begin(), points.end(), Distance(desiredResolution, _map->getSRS()->getUnits()), nullptr, nullptr) > 0;
        }
    }
}

bool
ElevationQuery::getElevations(const std::vector<osg::Vec3d>& points,
                              const SpatialReference*        pointsSRS,
                              std::vector<float>&            out_elevations,
                              double                         desiredResolution )
{
    if (_map->getNumTerrainPatchLayers() > 0)
    {
        sync();
        for (osg::Vec3dArray::const_iterator i = points.begin(); i != points.end(); ++i)
        {
            float elevation;
            GeoPoint p(pointsSRS, *i, ALTMODE_ABSOLUTE);

            if (getElevationImpl(p, elevation, desiredResolution, 0L))
            {
                out_elevations.push_back(elevation);
            }
            else
            {
                out_elevations.push_back(0.0);
            }
        }
        return true;
    }
    else
    {
        // Call ElevationPool::sampleMapCoords directly if there are no terrain patches as it is significantly faster than doing
        // individual getElevationImpl queries
        std::vector< osg::Vec3d > mapPoints = points;
        if (pointsSRS != _map->getSRS())
        {
            pointsSRS->transform(mapPoints, _map->getSRS());
        }
        int count = _map->getElevationPool()->sampleMapCoords(mapPoints.begin(), mapPoints.end(), Distance(desiredResolution, _map->getSRS()->getUnits()), nullptr, nullptr);
        for (unsigned int i = 0; i < points.size(); ++i)
        {
            out_elevations.push_back(mapPoints[i].z());
        }
        return count > 0;
    }
}

bool
ElevationQuery::getElevationImpl(const GeoPoint& point,
                                 float&          out_elevation,
                                 double          desiredResolution,
                                 double*         out_actualResolution)
{
    // assertion.
    if ( !point.isAbsolute() )
    {
        OE_WARN << LC << "Assertion failure; input must be absolute" << std::endl;
        return false;
    }

    osg::Timer_t begin = osg::Timer::instance()->tick();

    // first try the terrain patches.
    if ( _terrainModelLayers.size() > 0 )
    {
        osgUtil::IntersectionVisitor iv;

        if ( _ivrc.valid() )
            iv.setReadCallback(_ivrc.get());

        for(LayerVector::iterator i = _terrainModelLayers.begin(); i != _terrainModelLayers.end(); ++i)
        {
            // find the scene graph for this layer:
            Layer* layer = i->get();
            osg::Node* node = layer->getNode();
            if ( node )
            {
                // configure for intersection:
                osg::Vec3d surface;
                point.toWorld( surface );

                // trivial bounds check:
                if ( node->getBound().contains(surface) )
                {
                    osg::Vec3d nvector;
                    point.createWorldUpVector(nvector);

                    osg::Vec3d start( surface + nvector*5e5 );
                    osg::Vec3d end  ( surface - nvector*5e5 );

                    // first time through, set up the intersector on demand
                    if ( !_lsi.valid() )
                    {
                        _lsi = new osgUtil::LineSegmentIntersector(start, end);
                        _lsi->setIntersectionLimit( _lsi->LIMIT_NEAREST );
                    }
                    else
                    {
                        _lsi->reset();
                        _lsi->setStart( start );
                        _lsi->setEnd  ( end );
                    }

                    // try it.
                    iv.setIntersector( _lsi.get() );
                    node->accept( iv );

                    // check for a result!!
                    if ( _lsi->containsIntersections() )
                    {
                        osg::Vec3d isect = _lsi->getIntersections().begin()->getWorldIntersectPoint();

                        // transform back to input SRS:
                        GeoPoint output;
                        output.fromWorld( point.getSRS(), isect );
                        out_elevation = (float)output.z();
                        if ( out_actualResolution )
                            *out_actualResolution = 0.0;

                        return true;
                    }
                }
                else
                {
                    //OE_INFO << LC << "Trivial rejection (bounds check)" << std::endl;
                }
            }
        }
    } 

    if (_elevationLayers.empty())
    {
        // this means there are no heightfields.
        out_elevation = NO_DATA_VALUE;
        return true;
    }

    // secure map pointer:
    osg::ref_ptr<const Map> map;
    if (!_map.lock(map))
    {
        return false;
    } 

    // Set the query resolution:
    Distance resolution(desiredResolution, map->getSRS()->getUnits());

    // Sample the pool
    ElevationSample sample = map->getElevationPool()->getSample(point, resolution, &_workingSet);
    out_elevation = sample.elevation().as(Units::METERS);
    if (out_actualResolution)
        *out_actualResolution = sample.resolution().as(map->getSRS()->getUnits());

    return out_elevation != NO_DATA_VALUE;
}
