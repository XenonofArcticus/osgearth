/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ViewFitter"
#include "Math"

#define LC "[ViewFitter] "

using namespace osgEarth;
using namespace osgEarth::Util;

namespace
{
    // Projected the point Pview (in camera view space) onto the far clip plane of
    // a projection matrix.
    void projectToFarPlane(osg::Vec3d& Pview, const osg::Matrix& projMatrix, const osg::Matrix& projMatrixInv)
    {
        osg::Vec4d Pclip = osg::Vec4d(Pview.x(), Pview.y(), Pview.z(), 1.0)* projMatrix;
        Pclip.z() = Pclip.w();
        osg::Vec4d Ptemp = Pclip * projMatrixInv;
        Pview.set(Ptemp.x() / Ptemp.w(), Ptemp.y() / Ptemp.w(), Ptemp.z() / Ptemp.w());
    }

    double mix(double a, double b, double t)
    {
        return a*(1.0-t) + b*t;
    }
}

ViewFitter::ViewFitter(const SpatialReference* mapSRS, const osg::Camera* camera) :
_mapSRS(mapSRS),
_camera(camera),
_vfov(30.0f),
_buffer_m(0.0)
{
    //nop
}

bool
ViewFitter::createViewpoint(const std::vector<GeoPoint>& points, Viewpoint& outVP) const
{
    OE_SOFT_ASSERT_AND_RETURN(_mapSRS.valid(), false);
    OE_SOFT_ASSERT_AND_RETURN(_camera.valid(), false);

    if (points.empty())
        return false;

    osg::Matrix projMatrix = _camera->getProjectionMatrix();
    osg::Matrix viewMatrix = _camera->getViewMatrix();

    bool isPerspective = ProjectionMatrix::isPerspective(projMatrix);

    // Convert the point set to world space:
    std::vector<osg::Vec3d> world(points.size());

    // Collect the extent so we can calculate the centroid.
    GeoExtent extent(_mapSRS.get());

    for (int i = 0; i < points.size(); ++i)
    {
        // force absolute altitude mode - we don't care about clamping here
        GeoPoint p = points[i];
        p.z() = 0;
        p.altitudeMode() = ALTMODE_ABSOLUTE;

        // transform to the map's srs and then to world coords.
        p = p.transform(_mapSRS.get());
        p.toWorld(world[i]);

        extent.expandToInclude(p.x(), p.y());
    }

    if (extent.area() == 0.0)
    {
        extent.expand(0.001, 0.001);
    }
    
    double eyeDist;
    double fovy_deg, ar;
    double zfar;

    // Calculate the centroid, which will become the focal point of the view:
    GeoPoint centroidMap = extent.getCentroid();

    osg::Vec3d centroid;
    centroidMap.toWorld(centroid);

    if (isPerspective)
    {
        // For a perspective matrix, rewrite the projection matrix so 
        // the far plane is the radius of the ellipsoid. We do this so
        // we can project our control points onto a common plane.
        double znear;
        ProjectionMatrix::getPerspective(projMatrix, fovy_deg, ar, znear, zfar);
        znear = 1.0;

        if (_mapSRS->isGeographic())
        {
            osg::Vec3d C = centroid;
            C.normalize();
            C.z() = fabs(C.z());
            double t = C * osg::Vec3d(0,0,1); // dot product

            zfar = mix(_mapSRS->getEllipsoid().getRadiusEquator(),
                       _mapSRS->getEllipsoid().getRadiusPolar(),
                       t);
            eyeDist = zfar * 2.0;
        }
        else
        {
            osg::Vec3d eye, center, up2;
            viewMatrix.getLookAt(eye, center, up2);
            eyeDist = eye.length();
            zfar = eyeDist;
        }

        ProjectionMatrix::setPerspective(projMatrix, fovy_deg, ar, znear, zfar);
    }

    else // isOrtho
    {
        fovy_deg = _vfov;
        double L, R, B, T, N, F;
        ProjectionMatrix::getOrtho(projMatrix, L, R, B, T, N, F);
        ar = (R - L) / (T - B);

        if (_mapSRS->isGeographic())
        {
            osg::Vec3d C = centroid;
            C.normalize();
            C.z() = fabs(C.z());
            double t = C * osg::Vec3d(0,0,1); // dot product

            zfar = mix(_mapSRS->getEllipsoid().getRadiusEquator(),
                       _mapSRS->getEllipsoid().getRadiusPolar(),
                       t);
            eyeDist = zfar * 2.0;
        }
        else
        {
            osg::Vec3d eye, center, up2;
            viewMatrix.getLookAt(eye, center, up2);
            eyeDist = eye.length();
            zfar = eyeDist;
        }
    }

    // Set up a new view matrix to look down on that centroid:
    osg::Vec3d lookAt, up;

    osg::Vec3d lookFrom = centroid;

    if (_mapSRS->isGeographic())
    {
        lookFrom.normalize();
        lookFrom *= eyeDist;
        lookAt = centroid;
        up.set(0,0,1);
    }
    else
    {
        lookFrom.z() = eyeDist;
        lookAt.set(lookFrom.x(), lookFrom.y(), 0);
        up.set(0,1,0);
    }
    viewMatrix.makeLookAt(lookFrom, lookAt, up);

    // Transform our control points into view space, and then project each one
    // onto our common view plane (tangent to the ellispoid).
    osg::Matrix projMatrixInv;
    projMatrixInv.invert(projMatrix);

    double Mx = -DBL_MAX, My = -DBL_MAX;
    std::vector<osg::Vec3d> view(world.size());
    for (int i = 0; i < world.size(); ++i)
    {
        // Transform into view space (camera-relative):
        view[i] = world[i] * viewMatrix;

        // For a perspective projection, we have to project each point
        // on to the far clipping plane. No need to do this in orthographic
        // since the X and Y would not change.
        if (isPerspective)
            projectToFarPlane(view[i], projMatrix, projMatrixInv);

        Mx = osg::maximum(Mx, osg::absolute(view[i].x()));
        My = osg::maximum(My, osg::absolute(view[i].y()));
    }

    // Apply the edge buffer:
    Mx += _buffer_m;
    My += _buffer_m;

    // Calculate optimal new Z (distance from view plane)
    double half_fovy_rad = osg::DegreesToRadians(fovy_deg) * 0.5;
    double half_fovx_rad = half_fovy_rad * ar;
    double Zx = Mx / tan(half_fovx_rad);
    double Zy = My / tan(half_fovy_rad);
    double Zbest = osg::maximum(Zx, Zy);

    // Calcluate the new viewpoint.
    //osg::Vec3d FPworld = centroid;

    //if (_mapSRS->isGeographic())
    //{
    //    FPworld.normalize();
    //    FPworld *= zfar;
    //}
    //else
    //{
    //    FPworld.z() = 0.0;
    //}

    // Convert to a geopoint
    GeoPoint FP;
    FP.fromWorld(_mapSRS.get(), centroid);
    outVP = Viewpoint();
    outVP.focalPoint() = FP;
    outVP.pitch() = Angle(-90, Units::DEGREES);
    outVP.range() = Distance(Zbest, Units::METERS);

    return true;
}

bool
ViewFitter::createViewpoint(const GeoExtent& extent, Viewpoint& outVP) const
{
    std::vector<GeoPoint> points;
    points.emplace_back(extent.getSRS(), extent.xMin(), extent.yMin(), 0);
    points.emplace_back(extent.getSRS(), extent.xMax(), extent.yMax(), 0);
    return createViewpoint(points, outVP);
}

bool
ViewFitter::createViewpoint(const osg::Node* node, Viewpoint& outVP) const
{
    OE_SOFT_ASSERT_AND_RETURN(node != nullptr, false);

    osg::BoundingSphere bs = node->getBound();
    if (!bs.valid())
        return false;

    osg::Matrix m = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
    bs.center() = bs.center() * m;

    osg::Vec3d c = bs.center();
    double r = bs.radius();

    std::vector<GeoPoint> points;
    GeoPoint p;
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x() + r, c.y(), c.z())); points.push_back(p);
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x() - r, c.y(), c.z())); points.push_back(p);
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x(), c.y() + r, c.z())); points.push_back(p);
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x(), c.y() - r, c.z())); points.push_back(p);
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x(), c.y(), c.z() + r)); points.push_back(p);
    p.fromWorld(_mapSRS.get(), osg::Vec3d(c.x(), c.y(), c.z() - r)); points.push_back(p);

    return createViewpoint(points, outVP);
}
