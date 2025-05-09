/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "KML_Geometry"
#include "KML_Point"
#include "KML_LineString"
#include "KML_LinearRing"
#include "KML_Polygon"
#include "KML_MultiGeometry"
#include "KML_Model"
#include <osgEarth/StringUtils>

using namespace osgEarth_kml;

void 
KML_Geometry::build( xml_node<>* parent, KMLContext& cx, Style& style)
{
	for (xml_node<>* node = parent->first_node(); node; node = node->next_sibling())
	{
		buildChild(node, cx, style);
	}
}

void
KML_Geometry::buildChild( xml_node<>* node, KMLContext& cx, Style& style)
{
	std::string name = toLower(node->name());
    if ( name == "point" )
    {
        KML_Point g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        g.parseStyle(node, cx, style);
    }
    else if (name == "linestring" )
    {
        KML_LineString g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        g.parseStyle(node, cx, style);
    }
    else if ( name == "linearring" || name == "gx:latlonquad" )
    {
        KML_LinearRing g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        g.parseStyle(node, cx, style);
    }
    else if ( name == "polygon" )
    {
        KML_Polygon g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        g.parseStyle(node, cx, style);
    }
    else if ( name == "multigeometry" )
    {
        KML_MultiGeometry g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        
        for( xml_node<>* n = node->first_node(); n; n = n->next_sibling())
        {
            KML_Geometry subGeom;
            subGeom.buildChild( n, cx, style ); //use single style for all subgeometries
            if ( subGeom._geom.valid() )
                dynamic_cast<MultiGeometry*>(g._geom.get())->getComponents().push_back( subGeom._geom.get() );
        }
    }
    else if ( name == "model" )
    {
        KML_Model g;
        g.parseCoords(node, cx);
        _geom = g._geom.get();
        g.parseStyle(node, cx, style);
    }
}

void
KML_Geometry::parseCoords( xml_node<>* node, KMLContext& cx )
{
    xml_node<>* coords = node->first_node("coordinates", 0, false);
    if ( coords )
    {
        xml_node<>* coord = coords->first_node();
        while (coord)
        {
            auto tuples = StringTokenizer()
                .delim(" ").delim("\n")
                .keepEmpties(false)
                .tokenize(coord->value());

            for (StringVector::const_iterator s = tuples.begin(); s != tuples.end(); ++s)
            {
                auto parts = StringTokenizer()
                    .delim(",")
                    .keepEmpties(false)
                    .tokenize(*s);

                if (parts.size() >= 2)
                {
                    osg::Vec3d point;
                    point.x() = as<double>(parts[0], 0.0);
                    point.y() = as<double>(parts[1], 0.0);
                    if (parts.size() >= 3) {
                        point.z() = as<double>(parts[2], 0.0);
                    }
                    _geom->push_back(point);
                }
            }
            coords->remove_first_node();
            coord = coords->first_node();
        }
    }
}

void
KML_Geometry::parseStyle( xml_node<>* node, KMLContext& cx, Style& style )
{
    _extrude = getValue(node, "extrude") == "1";
    _tessellate = getValue(node, "tessellate") == "1";

    std::string am = getValue(node, "altitudemode");
    if ( am.empty() )
        am = "clampToGround"; // default.

    bool isPoly = _geom.valid() && _geom->getComponentType() == Geometry::TYPE_POLYGON;
    bool isLine = _geom.valid() && _geom->getComponentType() == Geometry::TYPE_LINESTRING;

    // Resolve the correct altitude symbol. CLAMP_TO_TERRAIN is the default, but the
    // technique will depend on the geometry's type and setup.
    AltitudeSymbol* alt = style.getOrCreate<AltitudeSymbol>();
    alt->clamping() = alt->CLAMP_TO_TERRAIN;


    // Compute some info about the geometry

    // Are all of the elevations zero?
    bool zeroElev = true;
    // Are all of the the elevations the same?
    bool sameElev = true;

    double maxElevation = -DBL_MAX;

    //if ( isPoly ) //compute maxElevation also for line strings for extrusion height
    {
        bool first = true;
        double e = 0.0;
        ConstGeometryIterator gi( _geom.get(), false );
        while(gi.hasMore() )
        {
            const Geometry* g = gi.next();
            for( Geometry::const_iterator ji = g->begin(); ji != g->end(); ++ji )
            {
                if ( !osg::equivalent(ji->z(), 0.0) )
                    zeroElev = false;

                if (first)
                {
                    first = false;
                    e = ji->z();
                }
                else
                {
                    if (!osg::equivalent(e, ji->z()))
                    {
                        sameElev = false;
                    }
                }

                if (ji->z() > maxElevation) maxElevation = ji->z();
            }
        }
    }

    // clamp to ground mode:
    if ( am == "clampToGround" || am == "clampToSeaFloor" )
    {
        if ( _extrude )
        {
            alt->technique() = alt->TECHNIQUE_MAP;
        }
        else if ( isPoly )
        {
            alt->technique() = alt->TECHNIQUE_DRAPE;
        }
        else if ( isLine)
        {
            alt->technique() = alt->TECHNIQUE_SCENE;
        }
        else // point
        {
            alt->technique() = alt->TECHNIQUE_SCENE;
        }

        // extrusion is not compatible with clampToGround.
        _extrude = false;
    }

    // "relativeToGround" means the coordinates' Z values are relative to the Z of the
    // terrain at that point. NOTE: GE flattens rooftops in this mode when extrude=1,
    // which seems wrong..
    else if ( am == "relativeToGround" || am == "relativeToSeaFloor" )
    {
        alt->clamping() = alt->CLAMP_RELATIVE_TO_TERRAIN;

        if (isPoly)
        {
            // If all of the verts have the same elevation then assume that it should be clamped at the centroid and not per vertex.
            if (sameElev)
            {
                alt->binding() = AltitudeSymbol::BINDING_CENTROID;
            }

            if ( _extrude )
            {
                alt->technique() = alt->TECHNIQUE_MAP;
            }
            else
            {
                alt->technique() = alt->TECHNIQUE_SCENE;

                if ( zeroElev )
                {
                    alt->clamping()  = alt->CLAMP_TO_TERRAIN;
                    alt->technique() = alt->TECHNIQUE_DRAPE;
                }
            }
        }
    }

    // "absolute" means to treat the Z values as-is
    else if ( am == "absolute" )
    {
        alt->clamping() = AltitudeSymbol::CLAMP_ABSOLUTE;
    }

    else if (!am.empty())
    {
        OE_WARN << LC << "KML altitudeMode \"" << am << "\" is invalid" << std::endl;
    }

    if ( _extrude )
    {
        ExtrusionSymbol* es = style.getOrCreate<ExtrusionSymbol>();
        es->flatten() = false;
    }
    else
    {
        // remove polystyle since it doesn't apply to non-extruded lines and points
        if ( !isPoly )
        {
            style.remove<PolygonSymbol>();
        }
    }
}
