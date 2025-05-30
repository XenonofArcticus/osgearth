/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/
#include <osgEarth/HTM>
#include <osgEarth/LabelNode>
#include <osgEarth/DrapeableNode>

using namespace osgEarth;
using namespace osgEarth::Contrib;

//-----------------------------------------------------------------------

#undef  LC
#define LC "[HTMGroup] "

bool
HTMNode::PolytopeDP::contains(const osg::Vec3d& p) const
{
    for( PlaneList::const_iterator i = _planeList.begin(); i != _planeList.end(); ++i )
    {
        if ( i->distance(p) < 0 )
            return false;
    }
    return true;
}

bool
HTMNode::PolytopeDP::containsAnyOf(const std::vector<osg::Vec3d>& points) const
{
    for( PlaneList::const_iterator i = _planeList.begin(); i != _planeList.end(); ++i )
    {
        if ( i->intersect(points) < 0 )
            return false;
    }
    return true;
}

//-----------------------------------------------------------------------

void
HTMNode::Triangle::set(const osg::Vec3d& v0, const osg::Vec3d& v1, const osg::Vec3d& v2)
{
    _v.resize(4);

    _v[0] = v0;
    _v[1] = v1;
    _v[2] = v2;
    _v[3].set(0,0,0);

    // assume verts are CCW.
    osg::Vec3d n0 = _v[0] ^ _v[1]; n0.normalize();
    osg::Vec3d n1 = _v[1] ^ _v[2]; n1.normalize();
    osg::Vec3d n2 = _v[2] ^ _v[0]; n2.normalize();

    // assemble the bounding polytope.
    _tope.add( osg::Plane(n0, _v[3]) );
    _tope.add( osg::Plane(n1, _v[3]) );
    _tope.add( osg::Plane(n2, _v[3]) );
}

void
HTMNode::Triangle::getMidpoints(osg::Vec3d* w) const
{
    w[0] = (_v[0]+_v[1]); w[0].normalize();
    w[1] = (_v[1]+_v[2]); w[1].normalize();
    w[2] = (_v[2]+_v[0]); w[2].normalize();
}

//-----------------------------------------------------------------------

HTMNode::HTMNode(HTMSettings& settings,
                 const osg::Vec3d& v0, const osg::Vec3d& v1, const osg::Vec3d& v2,
                 const std::string& id) :
_settings(settings)
{
    setName(id);

    _isLeaf = true;
    _tri.set( v0, v1, v2 );

    if (settings._debugGeom)
    {
        const double R = SpatialReference::get("wgs84")->getEllipsoid().getRadiusEquator();

        osg::Geometry* geom = new osg::Geometry();
        geom->setName(typeid(*this).name());
        geom->setUseVertexBufferObjects(true);
    
        osg::Vec3Array* verts = new osg::Vec3Array();
        for (double t=0.0; t<1.0; t+=0.05)
            verts->push_back((v0 + (v1-v0)*t) * R);
        for (double t=0.0; t<1.0; t+=0.05)
            verts->push_back((v1 + (v2-v1)*t) * R);
        for (double t=0.0; t<1.0; t+=0.05)
            verts->push_back((v2 + (v0-v2)*t) * R);
        geom->setVertexArray(verts);

        osg::Vec4Array* color = new osg::Vec4Array();
        color->push_back(osg::Vec4(1,1,0,1));
        color->setBinding(color->BIND_OVERALL);
        geom->setColorArray(color);

        geom->addPrimitiveSet(new osg::DrawArrays(GL_LINE_LOOP, 0, verts->size()));
        geom->getOrCreateStateSet()->setAttribute(new osg::Program(), osg::StateAttribute::PROTECTED);

        osgText::Text* text = new osgText::Text();
        text->setText(Stringify() << getName() << "\nS=" << geom->getBound().radius()*2.0);
        text->setPosition((v0+v1+v2)/3 * R);
        text->setCharacterSizeMode(text->SCREEN_COORDS);
        text->setCharacterSize(48);
        text->setAutoRotateToScreen(true);
        text->setAlignment(text->CENTER_CENTER);
        text->getOrCreateStateSet()->setAttribute(new osg::Program(), osg::StateAttribute::PROTECTED);

        DrapeableNode* drape = new DrapeableNode();
        drape->addChild(geom);
        drape->addChild(text);
        _debug = drape;
    }
}

void
HTMNode::traverse(osg::NodeVisitor& nv)
{
    if ( nv.getVisitorType() == nv.CULL_VISITOR )
    {
        const osg::BoundingSphere& bs = getBound();

        bool inRange = false;

        if (_settings._threshold.getUnits() == Units::PIXELS)
        {
            osg::CullStack* cs = dynamic_cast<osg::CullStack*>(&nv);
            if (cs)
            {
                float sizeInPixels = cs->clampedPixelSize(getBound()) / cs->getLODScale();
                inRange = sizeInPixels >= _settings._threshold.as(Units::PIXELS);
            }
        }
        else
        {
            float range = nv.getDistanceToViewPoint(bs.center(), true);
            inRange = range < (bs.radius() + _settings._threshold.as(Units::METERS));
        }

        if ( inRange )
        {
            osg::Group::traverse( nv );
            
            if (_debug.valid() && _isLeaf)
            {
                _debug->accept(nv);
            }
        }
        else if (_debug.valid())
        {
            _debug->accept(nv);
        }
    }
    else
    {
        if (_debug.valid())
        {
            _debug->accept(nv);
        }

        osg::Group::traverse( nv );        
    }
}

void
HTMNode::insert(osg::Node* node)
{
    if ( _isLeaf )
    {
        bool roomForMoreObjects = (getNumChildren() < _settings._maxObjectsPerCell);
        bool underMaxCellSize = getBound().radius()*2.0 < _settings._maxCellSize;
        bool reachedMinCellSize = getBound().radius()*2.0 <= _settings._minCellSize;

        if ((underMaxCellSize && roomForMoreObjects) || reachedMinCellSize)
        {
            addChild( node );
        }

        else
        {
            split();
            insert( node );
        }
    }

    else
    {
        const osg::Vec3d& p = node->getBound().center();

        // last four children are the subcells
        for (int i = _children.size() - 1; i >= _children.size()-4; --i)
        {
            HTMNode* child = dynamic_cast<HTMNode*>(_children[i].get());
            if ( child && child->contains(p) )
            {
                child->insert(node);
                break;
            }
        }
    }
}


void
HTMNode::split()
{
    // find the midpoints of each side of the triangle
    osg::Vec3d w[3];
    _tri.getMidpoints( w );

    // split into four children, each wound CCW
    HTMNode* c[4];
    c[0] = new HTMNode(_settings, _tri._v[0], w[0], w[2], Stringify()<< getName() << "0");
    c[1] = new HTMNode(_settings, _tri._v[1], w[1], w[0], Stringify() << getName() << "1");
    c[2] = new HTMNode(_settings, _tri._v[2], w[2], w[1], Stringify() << getName() << "2");
    c[3] = new HTMNode(_settings, w[0], w[1], w[2], Stringify() << getName() << "3");
    
    if (_settings._storeObjectsInLeavesOnly == true)
    {
        // distribute the data amongst the children
        for(osg::NodeList::iterator i = _children.begin(); i != _children.end(); ++i)
        {
            osg::Node* node = i->get();        
            const osg::Vec3d& p = node->getBound().center();

            for(unsigned j=0; j<4; ++j)
            {
                if ( c[j]->contains(p) )
                {
                    c[j]->insert( node );
                    break;
                }
            }
        }

        // remove the leaves from this node
        osg::Group::removeChildren(0, getNumChildren());
    }

    // add the new subnodes to this node.
    for(unsigned i=0; i<4; ++i)
    {
        osg::Group::addChild( c[i] );
    }

    _isLeaf = false;
}

//-----------------------------------------------------------------------

HTMGroup::HTMGroup()
{
    _settings._maxObjectsPerCell = 128;
    _settings._threshold = Distance(50.0f, Units::PIXELS);
    _settings._debugGeom = false;
    _settings._minCellSize = 10000;
    _settings._maxCellSize = 500000;
    _settings._storeObjectsInLeavesOnly = false;

    // hopefully prevent the OSG optimizer from altering this graph:
    setDataVariance( osg::Object::DYNAMIC );

    reinitialize();
}

void
HTMGroup::reinitialize()
{
    _children.clear();

    double rx = 1.0;
    double ry = 1.0;
    double rz = 1.0;

    // assemble the base manifold of 8 triangles.
    osg::Vec3d v0( 0,   0,   rz);     // lat= 90  long=  0
    osg::Vec3d v1( rx,  0,   0);      // lat=  0  long=  0
    osg::Vec3d v2( 0,   ry,  0);      // lat=  0  long= 90
    osg::Vec3d v3(-rx,  0,   0);      // lat=  0  long=180
    osg::Vec3d v4( 0,  -ry,  0);      // lat=  0  long=-90
    osg::Vec3d v5( 0,   0,  -rz);     // lat=-90  long=  0

    // CCW triangles.
    osg::Group::addChild( new HTMNode(_settings, v0, v1, v2, "0") );
    osg::Group::addChild( new HTMNode(_settings, v0, v2, v3, "1") );
    osg::Group::addChild( new HTMNode(_settings, v0, v3, v4, "2") );
    osg::Group::addChild( new HTMNode(_settings, v0, v4, v1, "3") );
    osg::Group::addChild( new HTMNode(_settings, v5, v1, v4, "4") );
    osg::Group::addChild( new HTMNode(_settings, v5, v4, v3, "5") );
    osg::Group::addChild( new HTMNode(_settings, v5, v3, v2, "6") );
    osg::Group::addChild( new HTMNode(_settings, v5, v2, v1, "7") );
}

bool
HTMGroup::insert(osg::Node* node)
{
    osg::Vec3d p = node->getBound().center();
    p.normalize(); // need?

    bool inserted = false;

    for(unsigned i=0; i<_children.size(); ++i)
    {
        HTMNode* child = static_cast<HTMNode*>(_children[i].get());
        if ( child->contains(p) )
        {
            child->insert(node);
            inserted = true;
            break;
        }
    }

    return inserted;
}
bool 
HTMGroup::addChild(osg::Node* child)
{
    return insert( child );
}

bool 
HTMGroup::insertChild(unsigned index, osg::Node* child)
{
    return insert( child );
}

bool 
HTMGroup::removeChildren(unsigned pos, unsigned numChildrenToRemove)
{
    OE_WARN << LC << "removeChildren() not implemented for HTM" << std::endl;
    return false;
}

bool 
HTMGroup::replaceChild(osg::Node* origChild, osg::Node* newChild)
{
    OE_WARN << LC << "replaceChild() not implemented for HTM" << std::endl;
    return false;
}

bool 
HTMGroup::setChild(unsigned index, osg::Node* node)
{
    OE_WARN << LC << "setChild() not implemented for HTM" << std::endl;
    return false;
}
