/******************************************************************************
* Copyright (c) 2011, Michael P. Gerlek (mpg@flaxen.com)
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
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
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

#include <pdal/pdal_test_main.hpp>

#include <pdal/PipelineManager.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/util/FileUtils.hpp>
#include <pdal/util/Utils.hpp>
#include <io/LasReader.hpp>
#include "Support.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace
{

std::string appName()
{
    const std::string app = Support::binpath(Support::exename("pdal") +
        " pipeline");
    return app;
}

// most pipelines (those with a writer) will be invoked via `pdal pipeline`
void run_pipeline(std::string const& pipelineFile,
    const std::string options = std::string(), const std::string lookFor = "")
{
    const std::string cmd = appName();

    std::string output;
    std::string file(Support::configuredpath(pipelineFile));
    int stat = pdal::Utils::run_shell_command(cmd + " " + file + " " +
        options + " 2>&1", output);
    EXPECT_EQ(0, stat) << "Failure running '" << pipelineFile << "' with "
        "options '" << options << "'.";
    if (stat)
        std::cerr << output << std::endl;
    if (lookFor.size())
    {
        EXPECT_NE(output.find(lookFor), std::string::npos);
    }
}

// most pipelines (those with a writer) will be invoked via `pdal pipeline`
void run_pipeline_stdin(std::string const& pipelineFile)
{
    const std::string cmd = appName();

    std::string output;
    std::string file(Support::configuredpath(pipelineFile));
    int stat = pdal::Utils::run_shell_command(cmd + " --stdin < " + file,
        output);
    EXPECT_EQ(0, stat);
    if (stat)
        std::cerr << output << std::endl;
}

} // unnamed namespace

namespace pdal
{

TEST(pipelineBaseTest, no_input)
{
    const std::string cmd = appName();

    std::string output;
    int stat = pdal::Utils::run_shell_command(cmd, output);
    EXPECT_NE(0, stat);

    const std::string expected = "usage: pdal pipeline [options] [input]";
    EXPECT_EQ(expected, output.substr(0, expected.length()));
}


TEST(pipelineBaseTest, common_opts)
{
    const std::string cmd = appName();

    std::string output;
    int stat = Utils::run_shell_command(cmd + " -h", output);
    EXPECT_EQ(stat, 0);
}


TEST(pipelineBaseTest, progress)
{
    std::string cmd = appName();
    std::string progressOut = Support::temppath("progress.out");
    FileUtils::deleteFile(progressOut);
    auto handle = FileUtils::createFile(progressOut);
    FileUtils::closeFile(handle);

    cmd += " --progress " + progressOut + " "  +
        Support::configuredpath("pipeline/bpf2las.json");

    std::string output;
    EXPECT_EQ(Utils::run_shell_command(cmd, output), 0);

    std::string progress = FileUtils::readFileIntoString(progressOut);
    EXPECT_NE(progress.find("READYFILE"), std::string::npos);
    EXPECT_NE(progress.find("DONEFILE"), std::string::npos);
}

class json : public testing::TestWithParam<const char*> {};

// TEST_P is run for each of the values in INSTANTIATE_TEST_CASE below.
TEST_P(json, pipeline)
{
    run_pipeline(GetParam());
}

INSTANTIATE_TEST_CASE_P(base, json,
                        testing::Values(
                            // "autzen/autzen-interpolate.json",
                            "pipeline/assign.json",
                            "pipeline/bpf2las.json",
                            "pipeline/chipper.json",
                            "pipeline/colorize-multi.json",
                            "pipeline/colorize.json",
                            "pipeline/crop-hole.json",
                            "pipeline/crop_wkt.json",
                            "pipeline/crop_wkt_2d.json",
                            "pipeline/decimate.json",
                            "pipeline/ferry-reproject.json",
                            "pipeline/las2csv.json",
                            "pipeline/las2csv-with-unicode-paths.json",
                            "pipeline/las2geojson.json",
                            "pipeline/las2space-delimited.json",
                            "pipeline/merge.json",
                            "pipeline/metadata_reader.json",
                            "pipeline/metadata_writer.json",
                            "pipeline/mississippi.json",
                            "pipeline/mississippi_reverse.json",
                            "pipeline/overlay.json",
                            // "pipeline/qfit2las.json",
                            "pipeline/range_z.json",
                            "pipeline/range_z_classification.json",
                            "pipeline/range_classification.json",
                            "pipeline/sbet2txt.json",
                            "pipeline/sort.json",
                            "pipeline/splitter.json",
                            "pipeline/stats.json",
                            "pipeline/transformation.json"
                        ));

TEST(json, pipeline_stdin)
{
    run_pipeline_stdin("pipeline/las2csv.json");
    run_pipeline_stdin("pipeline/bpf2las.json");
}

TEST(json, pipeline_verify)
{
    run_pipeline("pipeline/streamable.json", "--validate",
        "\"streamable\" : true");
    run_pipeline("pipeline/nonstreamable.json", "--validate",
        "\"streamable\" : false");
    run_pipeline("pipeline/invalid1.json", "--validate",
        "Unable to parse");
    run_pipeline("pipeline/invalid2.json", "--validate",
        "Unexpected argument");
    run_pipeline("pipeline/streamable.json", "-v Debug",
        "stream mode");
    run_pipeline("pipeline/streamable.json", "-v Debug --nostream",
        "standard mode");
}


class jsonWithNITF : public testing::TestWithParam<const char*> {};

TEST_P(jsonWithNITF, pipeline)
{
    pdal::StageFactory f;
    pdal::Stage* s1 = f.createStage("readers.nitf");
    pdal::Stage* s2 = f.createStage("writers.nitf");
    if (s1 && s2)
        run_pipeline(GetParam());
    else
        std::cerr << "WARNING: could not create readers.nitf or writers.nitf, skipping test" << std::endl;
}


#if defined(PDAL_HAVE_LASZIP) || defined(PDAL_HAVE_LAZPERF)
INSTANTIATE_TEST_CASE_P(plugins, jsonWithNITF,
                        testing::Values(
                            "pipeline/bpf2nitf.json",
                            "pipeline/las2nitf.json",
                            "pipeline/las2nitf-crop-with-options.json",
                            "pipeline/las2nitf-2.json",
                            "pipeline/nitf2las.json"
                        ));
#else
INSTANTIATE_TEST_CASE_P(plugins, jsonWithNITF,
                        testing::Values(
                            "pipeline/bpf2nitf.json",
                            "pipeline/las2nitf.json",
                            "pipeline/las2nitf-crop-with-options.json",
                            "pipeline/nitf2las.json"
                        ));
#endif

class jsonWithP2G : public testing::TestWithParam<const char*> {};

TEST_P(jsonWithP2G, pipeline)
{
    pdal::StageFactory f;
    pdal::Stage* s = f.createStage("writers.p2g");
    if (s)
        run_pipeline(GetParam());
    else
        std::cerr << "WARNING: could not create writers.p2g, skipping test" << std::endl;
}

INSTANTIATE_TEST_CASE_P(plugins, jsonWithP2G,
                        testing::Values(
                            "pipeline/p2g-writer.json"
                        ));

class jsonWithHexer : public testing::TestWithParam<const char*> {};

TEST_P(jsonWithHexer, pipeline)
{
    pdal::StageFactory f;
    pdal::Stage* s = f.createStage("filters.hexbin");
    if (s)
        run_pipeline(GetParam());
    else
        std::cerr << "WARNING: could not create filters.hexbin, skipping test" << std::endl;
}

INSTANTIATE_TEST_CASE_P(plugins, jsonWithHexer,
                        testing::Values(
                            "pipeline/hexbin-info.json",
                            "pipeline/hexbin.json"
                        ));

class jsonWithLAZ : public testing::TestWithParam<const char*> {};

TEST_P(jsonWithLAZ, pipeline)
{
#if defined PDAL_HAVE_LASZIP || defined PDAL_HAVE_LAZPERF
    run_pipeline(GetParam());
#else
    std::cerr << "WARNING: no LAZ support, skipping test" << std::endl;
#endif
}

INSTANTIATE_TEST_CASE_P(plugins, jsonWithLAZ,
                        testing::Values(
                            "pipeline/crop.json",
                            "pipeline/crop-stats.json"
                        ));

TEST(json, tags)
{
    PipelineManager manager;

    manager.readPipeline(Support::configuredpath("pipeline/tags.json"));
    std::vector<Stage *> stages = manager.m_stages;

    EXPECT_EQ(stages.size(), 4u);
    size_t totalInputs(0);
    for (Stage *s : stages)
    {
        std::vector<Stage *> inputs = s->getInputs();
        if (inputs.empty())
            EXPECT_EQ(s->getName(), "readers.las");
        if (inputs.size() == 1 || inputs.size() == 2)
            EXPECT_EQ(s->getName(), "writers.las");
        totalInputs += inputs.size();
    }
    EXPECT_EQ(totalInputs, 3U);
}

TEST(json, issue1417)
{
    std::string options = "--readers.las.filename=" +
        Support::datapath("las/utm15.las");
    run_pipeline("pipeline/issue1417.json", options);
}

// Make sure we handle repeated options properly
TEST(json, issue1941)
{
    PipelineManager manager;
    std::string file;

    file = Support::configuredpath("pipeline/range_multi_limits.json");
    manager.readPipeline(file);
    EXPECT_EQ(manager.execute(), (point_count_t)5);
    const PointViewSet& s = manager.views();
    EXPECT_EQ(s.size(), 1U);
    PointViewPtr view = *s.begin();
    EXPECT_EQ(view->getFieldAs<int>(Dimension::Id::X, 0), 3);
    EXPECT_EQ(view->getFieldAs<int>(Dimension::Id::X, 1), 4);
    EXPECT_EQ(view->getFieldAs<int>(Dimension::Id::X, 2), 5);
    EXPECT_EQ(view->getFieldAs<int>(Dimension::Id::X, 3), 8);
    EXPECT_EQ(view->getFieldAs<int>(Dimension::Id::X, 4), 9);

    PipelineManager manager2;

    file = Support::configuredpath("pipeline/range_bad_limits.json");
    EXPECT_THROW(manager2.readPipeline(file), pdal_error);
}


// Test that stage options passed via --stage.<tagname>.<option> work.
TEST(json, stagetags)
{
    auto checkValue = [](const std::string filename, double value)
    {
        Options o;
        o.add("filename", filename);
        LasReader r;
        r.setOptions(o);

        PointTable t;
        r.prepare(t);
        PointViewSet s = r.execute(t);
        EXPECT_EQ(s.size(), 1u);
        PointViewPtr v = *s.begin();

        for (PointId i = 0; i < v->size(); ++i)
            EXPECT_DOUBLE_EQ(value,
                v->getFieldAs<double>(Dimension::Id::Z, i));

        FileUtils::deleteFile(filename);
    };

    std::string outFilename(Support::temppath("assigned.las"));
    std::string base(appName() + " " +
        Support::configuredpath("pipeline/options.json"));
    std::string output;
    int stat;

    stat = Utils::run_shell_command(base, output);
    EXPECT_EQ(stat, 0);
    checkValue(outFilename, 25);

    stat = Utils::run_shell_command(base +
        " --filters.assign.assignment=Z[:]=101",
        output);
    EXPECT_EQ(stat, 0);
    checkValue(outFilename, 101);

    stat = Utils::run_shell_command(base +
        " --stage.assigner.assignment=Z[:]=1987",
        output);
    EXPECT_EQ(stat, 0);
    checkValue(outFilename, 1987);

    // Make sure that tag options override stage options.
    stat = Utils::run_shell_command(base +
        " --filters.assign.assignment=Z[:]=25 "
        "--stage.assigner.assignment=Z[:]=555", output);
    EXPECT_EQ(stat, 0);
    checkValue(outFilename, 555);
    stat = Utils::run_shell_command(base +
        " --stage.assigner.assignment=Z[:]=555 "
        "--filters.assign.assignment=Z[:]=25 ", output);
    EXPECT_EQ(stat, 0);
    checkValue(outFilename, 555);

    // Check that bad tag fails.
    stat = Utils::run_shell_command(base +
        " --stage.foobar.assignment=Z[:]=1987",
        output);
    EXPECT_NE(stat, 0);

    // Check that bad option name fails.
    stat = Utils::run_shell_command(base +
        " --stage.assigner.blah=Z[:]=1987",
        output);
    EXPECT_NE(stat, 0);

    // Check that multiply specified option fails.
    stat = Utils::run_shell_command(base +
        " --stage.reader.compression=laszip "
        "--stage.reader.compression=lazperf", output);
    EXPECT_NE(stat, 0);
}

} // namespace pdal
