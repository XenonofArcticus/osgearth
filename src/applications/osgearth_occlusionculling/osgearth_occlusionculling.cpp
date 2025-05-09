/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/MapNode>
#include <osgEarth/ScreenSpaceLayout>
#include <osgEarth/ECEF>

#include <osgEarth/EarthManipulator>
#include <osgEarth/AutoClipPlaneHandler>
#include <osgEarth/ExampleResources>

#include <osgEarth/AnnotationRegistry>
#include <osgEarth/ImageOverlay>
#include <osgEarth/ImageOverlayEditor>
#include <osgEarth/CircleNode>
#include <osgEarth/RectangleNode>
#include <osgEarth/EllipseNode>
#include <osgEarth/PlaceNode>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/StateSetManipulator>
#include <osgGA/EventVisitor>
#include <osgDB/ReadFile>


using namespace osgEarth;
using namespace osgEarth::Util;

int
usage( char** argv )
{
    OE_WARN << "Usage: " << argv[0] << " <earthfile>" << std::endl;
    return -1;
}


//------------------------------------------------------------------


int
main(int argc, char** argv)
{
    osgEarth::initialize();

    osg::Group* root = new osg::Group();

    // try to load an earth file.
    osg::ArgumentParser arguments(&argc,argv);

    osgViewer::Viewer viewer(arguments);

    unsigned int numObjects = 200;
    while (arguments.read("--count", numObjects)) {}

    bool declutter = false;
    if (arguments.read("--declutter")) declutter = true;

    // initialize the viewer:
    viewer.setCameraManipulator( new EarthManipulator() );


    // load an earth file and parse demo arguments
    auto node = MapNodeHelper().load(arguments, &viewer);
    if ( !node.valid() )
        return usage(argv);

    // find the map node that we loaded.
    MapNode* mapNode = MapNode::findMapNode(node);
    if ( !mapNode )
        return usage(argv);

    root->addChild( node );

    // Make a group for 2D items, and activate the decluttering engine. Decluttering
    // will migitate overlap between elements that occupy the same screen real estate.
    osg::Group* labelGroup = new osg::Group();
    root->addChild( labelGroup );

    // set up a style to use for placemarks:
    Style placeStyle;
    placeStyle.getOrCreate<AltitudeSymbol>()->clamping() = AltitudeSymbol::CLAMP_RELATIVE_TO_TERRAIN;

    // A lat/long SRS for specifying points.
    const SpatialReference* geoSRS = mapNode->getMapSRS()->getGeographicSRS();

    //--------------------------------------------------------------------

    //Create a bunch of placemarks around Mt Rainer so we can actually get some elevation
    {
        osg::ref_ptr<osg::Image> pin = osgDB::readRefImageFile( "../data/placemark32.png" );

        double centerLat =  46.840866;
        double centerLon = -121.769846;
        double height = 0.2;
        double width = 0.2;
        double minLat = centerLat - (height/2.0);
        double minLon = centerLon - (width/2.0);

        OE_NOTICE << "Placing " << numObjects << " placemarks" << std::endl;

        for (unsigned int i = 0; i < numObjects; i++)
        {
            double lat = minLat + height * (rand() * 1.0)/(RAND_MAX-1);
            double lon = minLon + width * (rand() * 1.0)/(RAND_MAX-1);
            PlaceNode* place = new PlaceNode("Placemark", placeStyle, pin.get());
            place->setMapNode(mapNode);
            place->setPosition(GeoPoint(geoSRS, lon, lat));
            //Enable occlusion culling.  This will hide placemarks that are hidden behind terrain.
            //This makes use of the OcclusionCullingCallback in CullingUtils.
            place->setOcclusionCulling( true );
            labelGroup->addChild( place );
        }
    }

    viewer.setSceneData( root );

    viewer.getCamera()->addCullCallback( new AutoClipPlaneCullCallback(mapNode) );
    viewer.addEventHandler(new osgViewer::StatsHandler());
    viewer.addEventHandler(new osgViewer::WindowSizeHandler());
    viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));

    return viewer.run();
}
