/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/
#include "MaterialLoader"
#include "URI"
#include "ImageUtils"

#include <osg/Texture2D>
#include <osgDB/FileNameUtils>

using namespace osgEarth;
using namespace osgEarth::Util;

#undef LC
#define LC "[MaterialUtils] "

MaterialUtils::Mangler
MaterialUtils::getDefaultNormalMapNameMangler()
{
    static Mangler getNormalMapFileName = [](const std::string& filename)
    {
        const std::string pattern = "_NML";

        std::string dot_ext = osgDB::getFileExtensionIncludingDot(filename);
        if (Strings::ciEquals(dot_ext, ".meif"))
        {
            auto underscore_pos = filename.find_last_of('_');
            if (underscore_pos != filename.npos)
            {
                return
                    filename.substr(0, underscore_pos)
                    + pattern
                    + filename.substr(underscore_pos);
            }
        }

        return
            osgDB::getNameLessExtension(filename)
            + pattern
            + dot_ext;
    };

    return getNormalMapFileName;
}

MaterialUtils::Mangler
MaterialUtils::getDefaultPBRMapNameMangler()
{
    static Mangler getPBRMapFileName = [](const std::string& filename)
    {
        const std::string pattern = "_MTL_GLS_AO";

        std::string dot_ext = osgDB::getFileExtensionIncludingDot(filename);
        if (Strings::ciEquals(dot_ext, ".meif"))
        {
            auto underscore_pos = filename.find_last_of('_');
            if (underscore_pos != filename.npos)
            {
                return
                    filename.substr(0, underscore_pos)
                    + pattern
                    + filename.substr(underscore_pos);
            }
        }

        return
            osgDB::getNameLessExtension(filename)
            + pattern
            + dot_ext;
    };

    return getPBRMapFileName;
}

MaterialUtils::Mangler
MaterialUtils::getDefaultDisplacementMapNameMangler()
{
    static Mangler getPBRMapFileName = [](const std::string& filename)
        {
            const std::string pattern = "_HGT";

            std::string dot_ext = osgDB::getFileExtensionIncludingDot(filename);
            if (Strings::ciEquals(dot_ext, ".meif"))
            {
                auto underscore_pos = filename.find_last_of('_');
                if (underscore_pos != filename.npos)
                {
                    return
                        filename.substr(0, underscore_pos)
                        + pattern
                        + filename.substr(underscore_pos);
                }
            }

            return
                osgDB::getNameLessExtension(filename)
                + pattern
                + dot_ext;
        };

    return getPBRMapFileName;
}



#undef LC
#define LC "[MaterialLoader] "

MaterialLoader::MaterialLoader()
{
    setTraversalMode(TRAVERSE_ALL_CHILDREN);
    setNodeMaskOverride(~0);
}

void
MaterialLoader::setOptions(const osgDB::Options* options)
{
    _options = options;
}

void
MaterialLoader::setMangler(
    int unit,
    MaterialLoader::Mangler mangler)
{
    _manglers[unit] = mangler;
}

void
MaterialLoader::setTextureFactory(
    int unit,
    MaterialLoader::TextureFactory factory)
{
    _factories[unit] = factory;
}

void
MaterialLoader::apply(osg::Node& node)
{
    if (node.getStateSet())
        apply(node.getStateSet());
    traverse(node);
}

void
MaterialLoader::apply(osg::StateSet* ss)
{
    OE_HARD_ASSERT(ss != nullptr);

    if (ss->getTextureAttributeList().empty())
        return;

    osg::Texture* t = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
    if (t == nullptr || t->getImage(0) == nullptr)
        return;

    std::string filename = osgDB::getSimpleFileName(t->getImage(0)->getFileName());

    for (auto& m : _manglers)
    {
        int unit = m.first;

        // if the unit is already occupied, ignore it
        // GW: removed this. Just replace it, nothing we can really do about it
        //if (ss->getTextureAttribute(unit, osg::StateAttribute::TEXTURE) != nullptr)
        //    continue;

        MaterialLoader::Mangler& mangler = m.second;
        URI materialURI(mangler(filename), URIContext(_referrer));

        // if it's already loaded re-use it
        osg::ref_ptr<osg::Texture> mat_tex;
        auto cache_iter = _cache.find(materialURI.full());
        if (cache_iter != _cache.end())
        {
            mat_tex = cache_iter->second;
        }
        else
        {
            //OE_INFO << LC << "Looking for: " << materialURI.full() << std::endl;
            osg::ref_ptr<osg::Image> image = materialURI.getImage(_options);
            if (image.valid())
            {
                auto iter = _factories.find(unit);
                if (iter != _factories.end())
                {
                    mat_tex = iter->second(image.get());
                }
                else
                {
                    mat_tex = new osg::Texture2D(image);
                }

                // match the params of the albedo texture
                mat_tex->setFilter(osg::Texture::MIN_FILTER, t->getFilter(osg::Texture::MIN_FILTER));
                mat_tex->setFilter(osg::Texture::MAG_FILTER, t->getFilter(osg::Texture::MAG_FILTER));
                mat_tex->setWrap(osg::Texture::WRAP_S, t->getWrap(osg::Texture::WRAP_S));
                mat_tex->setWrap(osg::Texture::WRAP_T, t->getWrap(osg::Texture::WRAP_T));
                mat_tex->setWrap(osg::Texture::WRAP_R, t->getWrap(osg::Texture::WRAP_R));
                mat_tex->setMaxAnisotropy(t->getMaxAnisotropy());

                _cache[materialURI.full()] = mat_tex;
                OE_DEBUG << LC << "Loaded material tex '" << materialURI.base() << "' to unit " << unit << std::endl;
            }
        }   

        if (mat_tex.valid())
        {
            ss->setTextureAttribute(unit, mat_tex, 1);
        }
    }
}
