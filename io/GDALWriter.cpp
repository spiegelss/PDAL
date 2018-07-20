/******************************************************************************
* Copyright (c) 2016, Hobu Inc. <info@hobu.co>
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. nor the names of its contributors
*       may be used to endorse or promote products derived from this
*       software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include "GDALWriter.hpp"

#include <sstream>

#include <pdal/GDALUtils.hpp>
#include <pdal/PointView.hpp>

namespace pdal
{

static StaticPluginInfo const s_info
{
    "writers.gdal",
    "Write a point cloud as a GDAL raster.",
    "http://pdal.io/stages/writers.gdal.html",
    { "tif", "tiff", "vrt" }
};

CREATE_STATIC_STAGE(GDALWriter, s_info)

std::string GDALWriter::getName() const
{
    return s_info.name;
}


void GDALWriter::addArgs(ProgramArgs& args)
{
    args.add("filename", "Output filename", m_filename).setPositional();
    args.add("resolution", "Cell edge size, in units of X/Y",
        m_edgeLength).setPositional();
    m_radiusArg = &args.add("radius", "Radius from cell center to use to locate"
        " influencing points", m_radius);
    args.add("gdaldriver", "GDAL writer driver name", m_drivername, "GTiff");
    args.add("gdalopts", "GDAL driver options (name=value,name=value...)",
        m_options);
    args.add("output_type", "Statistics produced ('min', 'max', 'mean', "
        "'idw', 'count', 'stdev' or 'all')", m_outputTypeString, {"all"} );
    args.add("data_type", "Data type for output grid (\"int8\", \"uint64\", "
        "\"float\", etc.)", m_dataType, Dimension::Type::Double);
    args.add("window_size", "Cell distance for fallback interpolation",
        m_windowSize);
    // Nan is a sentinal value to say that no value was set for nodata.
    args.add("nodata", "No data value", m_noData,
        std::numeric_limits<double>::quiet_NaN());
    args.add("dimension", "Dimension to use", m_interpDimString, "Z");
    args.add("bounds", "Bounds of data.  Required in streaming mode.",
        m_bounds);
}


void GDALWriter::initialize()
{
    for (auto& ts : m_outputTypeString)
    {
       Utils::trim(ts);
        if (ts == "all")
        {
            m_outputTypes = ~0;
            break;
        }
        if (ts == "min")
            m_outputTypes |= GDALGrid::statMin;
        else if (ts == "max")
            m_outputTypes |= GDALGrid::statMax;
        else if (ts == "count")
            m_outputTypes |= GDALGrid::statCount;
        else if (ts == "mean")
            m_outputTypes |= GDALGrid::statMean;
        else if (ts == "idw")
            m_outputTypes |= GDALGrid::statIdw;
        else if (ts == "stdev")
            m_outputTypes |= GDALGrid::statStdDev;
        else
            throwError("Invalid output type: '" + ts + "'.");
    }

    gdal::registerDrivers();
}


void GDALWriter::prepared(PointTableRef table)
{
    m_interpDim = table.layout()->findDim(m_interpDimString);
    if (m_interpDim == Dimension::Id::Unknown)
        throwError("Specified dimension '" + m_interpDimString +
            "' does not exist.");
    if (!m_radiusArg->set())
        m_radius = m_edgeLength * sqrt(2.0);
    m_fixedGrid = m_bounds.to2d().valid();
}


void GDALWriter::readyFile(const std::string& filename,
    const SpatialReference& srs)
{
    m_outputFilename = filename;
    m_srs = srs;
    m_grid.reset();
    if (m_fixedGrid)
    {
        createGrid(m_bounds.to2d());
        m_expandByPoint = false;
    }
}


void GDALWriter::createGrid(BOX2D bounds)
{
    m_curBounds = bounds;
    size_t width = ((m_curBounds.maxx - m_curBounds.minx) / m_edgeLength) + 1;
    size_t height = ((m_curBounds.maxy - m_curBounds.miny) / m_edgeLength) + 1;
    try
    {
        m_grid.reset(new GDALGrid(width, height, m_edgeLength, m_radius,
            m_outputTypes, m_windowSize));
    }
    catch (GDALGrid::error& err)
    {
        throwError(err.what());
    }
}


void GDALWriter::expandGrid(BOX2D bounds)
{
    if (bounds == m_curBounds)
        return;

    bounds.grow(m_curBounds);
    size_t xshift = ceil((m_curBounds.minx - bounds.minx) / m_edgeLength);
    bounds.minx = m_curBounds.minx - (xshift * m_edgeLength);
    size_t yshift = ceil((m_curBounds.miny - bounds.miny) / m_edgeLength);
    bounds.miny = m_curBounds.miny - (yshift * m_edgeLength);

    size_t width = ((bounds.maxx - bounds.minx) / m_edgeLength) + 1;
    size_t height = ((bounds.maxy - bounds.miny) / m_edgeLength) + 1;
    try
    {
        m_grid->expand(width, height, xshift, yshift);
    }
    catch (const GDALGrid::error& err)
    {
        throwError(err.what()); // Add the stage name onto the error text.
    }
    m_curBounds = bounds;
}


void GDALWriter::writeView(const PointViewPtr view)
{
    m_expandByPoint = false;

    // When we're running in standard mode, it's better to get the bounds and
    // expand once, rather than have to do this for every point, since an
    // expansion causes data to move.
    if (!m_fixedGrid)
    {
        BOX2D bounds;
        view->calculateBounds(bounds);
        bounds.grow(m_radius);
        if (!m_grid)
            createGrid(bounds);
        else
            expandGrid(bounds);
    }

    PointRef point(*view, 0);
    for (PointId idx = 0; idx < view->size(); ++idx)
    {
        point.setPointId(idx);
        processOne(point);
    }
}


bool GDALWriter::processOne(PointRef& point)
{
    double x = point.getFieldAs<double>(Dimension::Id::X);
    double y = point.getFieldAs<double>(Dimension::Id::Y);
    double z = point.getFieldAs<double>(m_interpDim);

    if (m_expandByPoint)
    {
        if (!m_grid)
            createGrid(BOX2D(x, y, x, y).grow(m_radius));
        else if (!m_curBounds.contains(x, y))
            expandGrid(BOX2D(x, y, x, y).grow(m_radius));
    }
    x -= m_curBounds.minx;
    y -= m_curBounds.miny;
    if (!m_fixedGrid)
    {
        x += m_radius;
        y += m_radius;
    }

    m_grid->addPoint(x, y, z);
    return true;
}


void GDALWriter::doneFile()
{
    if (!m_grid)
        throw pdal_error("Unable to write GDAL data with no points "
            "for output.");

    std::array<double, 6> pixelToPos;

    pixelToPos[0] = m_curBounds.minx;
    pixelToPos[1] = m_edgeLength;
    pixelToPos[2] = 0;
    pixelToPos[3] = m_curBounds.miny + (m_edgeLength * m_grid->height());
    pixelToPos[4] = 0;
    pixelToPos[5] = -m_edgeLength;
    gdal::Raster raster(m_outputFilename, m_drivername, m_srs, pixelToPos);

    m_grid->finalize();

    gdal::GDALError err = raster.open(m_grid->width(), m_grid->height(),
        m_grid->numBands(), m_dataType, m_noData, m_options);

    if (err != gdal::GDALError::None)
        throwError(raster.errorMsg());
    int bandNum = 1;

    // Perhaps the grid should return an iterator, which would work as well.
    double *src;
    src = m_grid->data("min");
    double srcNoData = std::numeric_limits<double>::quiet_NaN();
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "min");
    src = m_grid->data("max");
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "max");
    src = m_grid->data("mean");
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "mean");
    src = m_grid->data("idw");
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "idw");
    src = m_grid->data("count");
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "count");
    src = m_grid->data("stdev");
    if (src && err == gdal::GDALError::None)
        err = raster.writeBand(src, srcNoData, bandNum++, "stdev");
    if (err != gdal::GDALError::None)
        throwError(raster.errorMsg());

    getMetadata().addList("filename", m_filename);
}

} // namespace pdal
