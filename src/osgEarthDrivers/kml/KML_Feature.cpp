/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "KML_Feature"
#include "KML_Style"
#include "KML_StyleMap"
#include <osg/UserDataContainer>
#include <osg/ValueObject>
#include <osgEarth/Viewpoint>

using namespace osgEarth_kml;
using namespace osgEarth;

void
KML_Feature::scan( xml_node<>* node, KMLContext& cx )
{
    KML_Object::scan(node, cx);
    for_many( Style, scan, node, cx );
    for_many( StyleMap, scan, node, cx );
}

void
KML_Feature::scan2( xml_node<>* node, KMLContext& cx )
{
    KML_Object::scan2(node, cx);
    for_many( Style, scan2, node, cx );
    for_many( StyleMap, scan2, node, cx );
}

void
KML_Feature::build( xml_node<>* node, KMLContext& cx, osg::Node* working )
{
    KML_Object::build(node, cx, working);

    // subclass feature is built; now add feature level data if available
    if ( working )
    {
        // parse the visibility to show/hide the item by default:
		std::string visibility = getValue(node, "visibility");
        if (visibility == "1" || ciEquals(visibility, "true"))
            working->setNodeMask(~0);
        else if (visibility == "0" || ciEquals(visibility, "false"))
            working->setNodeMask(0);

        // parse a "LookAt" element (stores a viewpoint)
        AnnotationData* anno = getOrCreateAnnotationData(working);
        
        anno->setName( getValue(node, "name") );
        anno->setDescription( getValue(node, "description") );

        xml_node<>* lookat = node->first_node("lookat", 0, false);
        if ( lookat )
        {
            Viewpoint vp;

            vp.focalPoint() = GeoPoint(
                cx._srs.get(),
				as<double>(getValue(lookat, "longitude"), 0.0),
				as<double>(getValue(lookat, "latitude"), 0.0),
				as<double>(getValue(lookat, "altitude"), 0.0),
                ALTMODE_ABSOLUTE );

            vp.heading() = Angle(as<double>(getValue(lookat, "heading"), 0.0), Units::DEGREES);
            vp.pitch() = Angle(-as<double>(getValue(lookat, "tilt"), 45.0), Units::DEGREES);
            vp.range() = Distance(as<double>(getValue(lookat, "range"), 10000.0), Units::METERS);

            anno->setViewpoint( vp );
        }

        xml_node<>* timespan = node->first_node("timespan", 0, false);
        if ( timespan )
        {
            DateTimeRange range;

            std::string begin = getValue(timespan, "begin");
            if ( !begin.empty() )
            {
                range.begin() = DateTime(begin);
            }

            std::string end = getValue(timespan, "end");
            if ( !end.empty() )
            {
                range.end() = DateTime(end);
            }

            anno->setDateTimeRange( range );
        }

        xml_node<>* extdata = node->first_node("extendeddata", 0, false);
        if ( extdata )
        {
            xml_node<>* data = extdata->first_node("data", 0, false);
            if ( data )
            {
                working->setUserValue(getValue(data, "name"), getValue(data, "value"));			    
            }
        }
    }
}
