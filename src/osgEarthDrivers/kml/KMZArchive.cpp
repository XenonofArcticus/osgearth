/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "KMZArchive"

#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/ObjectWrapper>
#include <osgEarth/HTTPClient>
#include <osgEarth/Registry>
#include <osgEarth/Threading>
#include <fstream>

#define LC "[KMZArchive] "

using namespace osgEarth;

namespace
{
    URI downloadToCache( const URI& uri )
    {
        // get a handle on the file cache. This is a temporary setup just to get things
        // working.
        static std::mutex s_fcMutex;

        static URIContext s_cache;
        if ( s_cache.empty() )
        {
            std::lock_guard<std::mutex> exclusiveLock(s_fcMutex);
            if ( s_cache.empty() )
            {
                const char* osgCacheDir = ::getenv("OSG_FILE_CACHE");
                if ( osgCacheDir )
                    s_cache = URIContext(std::string(osgCacheDir) + "/");
                else
                    s_cache = URIContext("osgearth_kmz_cache/");
            }
        }

        URI cachedFile( osgDB::getSimpleFileName(uri.full()), s_cache);

        if ( !osgDB::fileExists(cachedFile.full()) )
        {
            osgDB::makeDirectoryForFile(cachedFile.full());
            HTTPClient::download( uri.full(), cachedFile.full() );
        }

        if ( osgDB::fileExists(cachedFile.full()) )
            return cachedFile;

        return URI();
    }
}

//------------------------------------------------------------------------

KMZArchive::KMZArchive(const URI& archiveURI, const osgDB::Options* dbOptions) :
_archiveURI( archiveURI )
{
    supportsExtension( "kmz", "KMZ" );

    // download the KMZ to a local cache - cannot open zip files remotely.
    URI localURI( archiveURI );
    if ( osgDB::containsServerAddress(archiveURI.full()) )
    {
        localURI = downloadToCache( archiveURI );
    }

    osg::ref_ptr<osgDB::ReaderWriter> zipRW = osgDB::Registry::instance()->getReaderWriterForExtension("zip");
    if ( zipRW.valid() )
    {
#if 1
        auto local_options = osgEarth::Registry::cloneOrCreateOptions(dbOptions);
        local_options->setPluginStringData("ignore_extension", "true");
        ReadResult r = zipRW->openArchive(localURI.full(), osgDB::ReaderWriter::READ, 4096U, local_options);
        if (r.success())
        {
            _zip = r.takeArchive();
        }
        else
        {
            OE_WARN << LC << "Failed to open archive at \"" << localURI.full() << "\"\n";
            OE_WARN << LC << "Message = " << r.message() << "\n";
        }
#else   
        std::ifstream fin(localURI.full().c_str(), std::ios::binary|std::ios::in);
        if ( fin.is_open() )
        {
            //std::string str(
            //    (std::istreambuf_iterator<char>(fin)),
            //    std::istreambuf_iterator<char>());

            //std::istringstream in(str);

            ReadResult r = zipRW->openArchive( fin, dbOptions );
            if ( r.success() )
            {
                _zip = r.takeArchive();
            }
            else
            {
                OE_WARN << LC << "Failed to open archive at \"" << localURI.full() << "\"\n";
                OE_WARN << LC << "Message = " << r.message() << "\n";
            }
        }
        else
        {
            OE_WARN << LC << "Failed to read archive from \"" << localURI.full() << "\"\n";
        }
#endif
    }
    else
    {
        OE_WARN << LC << "Failed to locate OSG ZIP plugin\n";
    }
}

KMZArchive::~KMZArchive()
{
}

void 
KMZArchive::close()
{
    if ( _zip.valid() )
    {
        _zip->close();
    }
}

/** Get the file name which represents the archived file.*/
std::string 
KMZArchive::getArchiveFileName() const
{
    return _archiveURI.base();
}

/** Get the file name which represents the master file recorded in the Archive.*/
std::string
KMZArchive::getMasterFileName() const 
{
    return "doc.kml";
}

/** return true if file exists in archive.*/
bool
KMZArchive::fileExists(const std::string& filename) const
{
    return _zip.valid() ? _zip->fileExists(filename) : false;
}

/** return type of file. */
osgDB::FileType
KMZArchive::getFileType(const std::string& filename) const
{
    return _zip.valid() ? _zip->getFileType(filename) : osgDB::REGULAR_FILE;
}

/** Get the full list of file names available in the archive.*/
bool 
KMZArchive::getFileNames(FileNameList& fileNames) const
{
    return _zip.valid() ? _zip->getFileNames(fileNames) : false;
}

/** return the contents of a directory.
* returns an empty array on any error.*/
osgDB::DirectoryContents 
KMZArchive::getDirectoryContents(const std::string& dirName) const
{
    return _zip.valid() ? _zip->getDirectoryContents(dirName) : osgDB::DirectoryContents();
}

osgDB::ReaderWriter::ReadResult
KMZArchive::readImage(const std::string& filename, const osgDB::Options* options) const
{
    return _zip.valid() ? _zip->readImage(resolve(filename), options) : ReadResult::FILE_NOT_HANDLED;
}

osgDB::ReaderWriter::ReadResult
KMZArchive::readNode(const std::string& filename, const osgDB::Options* options) const
{
    return _zip.valid() ? _zip->readNode(resolve(filename), options) : ReadResult::FILE_NOT_HANDLED;
}

osgDB::ReaderWriter::ReadResult
KMZArchive::readObject(const std::string& filename, const osgDB::Options* options) const
{
    return _zip.valid() ? _zip->readObject(resolve(filename), options) : ReadResult::FILE_NOT_HANDLED;
}

std::string
KMZArchive::resolve(const std::string& filename) const
{
    // if the file is not in the archive AND it's the default name,
    // instead use the first file in the archive.
    if (_zip.valid() && !fileExists(filename) && filename == getMasterFileName())
    {
        FileNameList names;
        getFileNames(names);
        if (names.size() > 0)
        {
            return names.front();
        }
    }

    return filename;
}
