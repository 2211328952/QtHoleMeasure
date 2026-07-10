#include "../src/HoleMeasureCore.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool nearlyEqual(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void testParseTemplateCsv()
{
    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/template_parse_test.csv";
    {
        std::ofstream file(path.c_str(), std::ios::trunc);
        file << "Header\n";
        file << "ID,Row Label,Column Label,World X,World Y,Group,Enabled,Pin Direction,Xoffset,Yoffset\n";
        file << "1,A,1,51955,5715,0,1,0,12.5,-3.25\n";
        file << "2,B,1,51955,5815,0,0,1\n";
        file << "3,A,2,52055,5715,1,1,2,0.5,1.5\n";
    }

    const auto points = hm::loadTemplateCsv(path);
    require(points.size() == 2, "template should contain enabled points only");
    require(points.front().id == 1, "first point id");
    require(points.front().rowLabel == "A", "first point row label");
    require(points.front().columnLabel == "1", "first point column label");
    require(nearlyEqual(points.front().worldX, 51955.0), "first point world x");
    require(nearlyEqual(points.front().worldY, 5715.0), "first point world y");
    require(nearlyEqual(points.front().xOffset, 12.5), "first point x offset");
    require(nearlyEqual(points.front().yOffset, -3.25), "first point y offset");
    require(points.back().id == 3, "disabled template point should be skipped");
    require(nearlyEqual(points.back().xOffset, 0.5), "last point x offset");
    require(nearlyEqual(points.back().yOffset, 1.5), "last point y offset");
}

void testParseTemplateCsvWithoutOffsetsDefaultsToZero()
{
    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/template_parse_no_offset_test.csv";
    {
        std::ofstream file(path.c_str(), std::ios::trunc);
        file << "Header\n";
        file << "ID,Row Label,Column Label,World X,World Y,Group,Enabled,Pin Direction\n";
        file << "1,A,1,51955,5715,0,1,0\n";
    }

    const auto points = hm::loadTemplateCsv(path);

    require(points.size() == 1, "template without offsets should still load");
    require(nearlyEqual(points.front().xOffset, 0.0), "missing x offset should default to zero");
    require(nearlyEqual(points.front().yOffset, 0.0), "missing y offset should default to zero");
}

void testSortOrders()
{
    std::vector<hm::TemplatePoint> points = {
        { 1, "A", "1", 100.0, 30.0, 0, true, 0 },
        { 2, "B", "1", 100.0, 10.0, 0, true, 1 },
        { 3, "A", "2", 200.0, 20.0, 0, true, 2 },
        { 4, "B", "2", 200.0,  5.0, 0, true, 3 },
    };

    auto rowTopLeft = hm::sortForExport(points, hm::ExportOrder::RowFirstTopLeft);
    require(rowTopLeft.size() == 4, "row-first sort count");
    require(rowTopLeft[0].id == 1 && rowTopLeft[1].id == 3
        && rowTopLeft[2].id == 2 && rowTopLeft[3].id == 4,
        "row-first top-left should walk columns across each row");

    auto rowBottomLeft = hm::sortForExport(points, hm::ExportOrder::RowFirstBottomLeft);
    require(rowBottomLeft[0].id == 2 && rowBottomLeft[1].id == 4
        && rowBottomLeft[2].id == 1 && rowBottomLeft[3].id == 3,
        "row-first bottom-left should start from bottom row");

    auto rowBottomRight = hm::sortForExport(points, hm::ExportOrder::RowFirstBottomRight);
    require(rowBottomRight[0].id == 4 && rowBottomRight[1].id == 2
        && rowBottomRight[2].id == 3 && rowBottomRight[3].id == 1,
        "row-first bottom-right should reverse rows and columns");

    auto rowTopRight = hm::sortForExport(points, hm::ExportOrder::RowFirstTopRight);
    require(rowTopRight[0].id == 3 && rowTopRight[1].id == 1
        && rowTopRight[2].id == 4 && rowTopRight[3].id == 2,
        "row-first top-right should start from right column");

    auto columnTopLeft = hm::sortForExport(points, hm::ExportOrder::ColumnFirstTopLeft);
    require(columnTopLeft[0].id == 1 && columnTopLeft[1].id == 2
        && columnTopLeft[2].id == 3 && columnTopLeft[3].id == 4,
        "column-first top-left should walk rows down each column");

    auto columnBottomLeft = hm::sortForExport(points, hm::ExportOrder::ColumnFirstBottomLeft);
    require(columnBottomLeft[0].id == 2 && columnBottomLeft[1].id == 1
        && columnBottomLeft[2].id == 4 && columnBottomLeft[3].id == 3,
        "column-first bottom-left should reverse rows inside each column");

    auto columnBottomRight = hm::sortForExport(points, hm::ExportOrder::ColumnFirstBottomRight);
    require(columnBottomRight[0].id == 4 && columnBottomRight[1].id == 3
        && columnBottomRight[2].id == 2 && columnBottomRight[3].id == 1,
        "column-first bottom-right should start from bottom-right");

    auto columnTopRight = hm::sortForExport(points, hm::ExportOrder::ColumnFirstTopRight);
    require(columnTopRight[0].id == 3 && columnTopRight[1].id == 4
        && columnTopRight[2].id == 1 && columnTopRight[3].id == 2,
        "column-first top-right should start from right column");
}

void testSortTemplateImagePairs()
{
    std::vector<hm::TemplatePoint> points = {
        { 1, "A", "1", 100.0, 30.0, 0, true, 0 },
        { 2, "B", "1", 100.0, 10.0, 0, true, 1 },
        { 3, "A", "2", 200.0, 20.0, 0, true, 2 },
        { 4, "B", "2", 200.0,  5.0, 0, true, 3 },
    };
    std::vector<hm::ImagePoint> imagePoints = {
        { 101.0, 301.0 },
        { 102.0, 302.0 },
        { 103.0, 303.0 },
        { 104.0, 304.0 },
    };

    hm::sortTemplateImagePairs(points, imagePoints, hm::ExportOrder::ColumnFirstTopLeft);
    require(points[0].id == 1 && points[1].id == 2 && points[2].id == 3 && points[3].id == 4,
        "template points should be sorted before measurement");
    require(nearlyEqual(imagePoints[0].x, 101.0) && nearlyEqual(imagePoints[3].x, 104.0),
        "image points should stay paired with sorted template points");

    hm::sortTemplateImagePairs(points, imagePoints, hm::ExportOrder::ColumnFirstTopRight);
    require(points[0].id == 3 && points[1].id == 4 && points[2].id == 1 && points[3].id == 2,
        "template points should support reverse column measurement order");
    require(nearlyEqual(imagePoints[0].x, 103.0) && nearlyEqual(imagePoints[3].x, 102.0),
        "image points should stay paired after reverse column sorting");
}

void testSortColumnLabelsForOrder()
{
    std::vector<std::string> columns = { "1", "10", "2" };

    auto first = hm::sortColumnLabelsForOrder(columns, hm::ExportOrder::RowFirstTopLeft);
    require(first[0] == "1" && first[1] == "2" && first[2] == "10",
        "column labels should sort left to right for left-start order");

    auto last = hm::sortColumnLabelsForOrder(columns, hm::ExportOrder::ColumnFirstTopRight);
    require(last[0] == "10" && last[1] == "2" && last[2] == "1",
        "column labels should sort right to left for right-start order");
}

void testProfileLabelsFollowOrderMajor()
{
    hm::TemplatePoint point;
    point.rowLabel = "B";
    point.columnLabel = "10";

    require(hm::profileLabelForOrder(point, hm::ExportOrder::ColumnFirstTopLeft) == "10",
        "column-first profile label should use column");
    require(hm::profileLabelForOrder(point, hm::ExportOrder::RowFirstTopLeft) == "B",
        "row-first profile label should use row");

    std::vector<std::string> labels = { "A", "K", "B" };
    auto topFirst = hm::sortProfileLabelsForOrder(labels, hm::ExportOrder::RowFirstTopLeft);
    require(topFirst[0] == "A" && topFirst[1] == "B" && topFirst[2] == "K",
        "row labels should sort top to bottom for top-start order");

    auto bottomFirst = hm::sortProfileLabelsForOrder(labels, hm::ExportOrder::RowFirstBottomLeft);
    require(bottomFirst[0] == "K" && bottomFirst[1] == "B" && bottomFirst[2] == "A",
        "row labels should sort bottom to top for bottom-start order");
}

void testMicronPerPixelEstimateDoesNotDependOnCurrentOrder()
{
    std::vector<hm::TemplatePoint> points = {
        { 1, "A", "1", 0.0, 0.0, 0, true, 0 },
        { 2, "B", "1", 0.0, 100.0, 0, true, 1 },
        { 3, "A", "2", 100.0, 0.0, 0, true, 2 },
        { 4, "B", "2", 100.0, 100.0, 0, true, 3 },
    };
    std::vector<hm::ImagePoint> imagePoints = {
        { 0.0, 0.0 },
        { 0.0, 50.0 },
        { 50.0, 0.0 },
        { 50.0, 50.0 },
    };

    const double first = hm::estimateMicronPerPixel(points, imagePoints, 1.0);
    hm::sortTemplateImagePairs(points, imagePoints, hm::ExportOrder::RowFirstTopRight);
    const double reordered = hm::estimateMicronPerPixel(points, imagePoints, 1.0);

    require(nearlyEqual(first, 2.0), "micron estimate should use nearest template spacing");
    require(nearlyEqual(reordered, 2.0), "micron estimate should not depend on sort order");
}

void testOffsetTransform()
{
    std::vector<hm::ImagePoint> points = {
        { 0.0, 0.0 },
        { 2.0, 0.0 },
    };
    const auto moved = hm::applyArrayOffset(points, hm::ArrayOffset{ 10.0, 20.0, 90.0 });
    require(moved.size() == 2, "offset point count");
    require(nearlyEqual(moved[0].x, 11.0), "rotated first x");
    require(nearlyEqual(moved[0].y, 19.0), "rotated first y");
    require(nearlyEqual(moved[1].x, 11.0), "rotated second x");
    require(nearlyEqual(moved[1].y, 21.0), "rotated second y");
}

void testRoiGeneration()
{
    hm::GaugeDefaults defaults;
    defaults.edgeOffsetPx = 25.0;
    defaults.roiLengthPx = 60.0;
    defaults.roiWidthPx = 12.0;
    defaults.sampleRegionWidth = 8;
    defaults.sampleRegionHeight = 16;
    defaults.sampleRegionInterval = 5;
    defaults.acceptScore = 35;

    const auto rois = hm::makeDefaultHoleRois(hm::ImagePoint{ 100.0, 200.0 }, 7, defaults);
    require(rois.size() == 4, "four rois per hole");
    require(rois[0].side == hm::RoiSide::Top, "top roi first");
    require(nearlyEqual(rois[0].center.x, 100.0) && nearlyEqual(rois[0].center.y, 175.0), "top roi center");
    require(nearlyEqual(rois[0].angleDeg, -90.0), "top roi handle points outward");
    require(rois[1].side == hm::RoiSide::Bottom, "bottom roi second");
    require(nearlyEqual(rois[1].center.y, 225.0), "bottom roi center");
    require(nearlyEqual(rois[1].angleDeg, 90.0), "bottom roi handle points outward");
    require(rois[2].side == hm::RoiSide::Left, "left roi third");
    require(nearlyEqual(rois[2].center.x, 75.0), "left roi center");
    require(nearlyEqual(rois[2].angleDeg, 180.0), "left roi handle points outward");
    require(rois[3].side == hm::RoiSide::Right, "right roi fourth");
    require(nearlyEqual(rois[3].center.x, 125.0), "right roi center");
    require(nearlyEqual(rois[3].angleDeg, 0.0), "right roi handle points outward");
    require(rois[3].params.acceptScore == 35, "gauge defaults copied");

    defaults.orientationDeg = 90.0;
    const auto rotated = hm::makeDefaultHoleRois(hm::ImagePoint{ 100.0, 200.0 }, 7, defaults);
    require(nearlyEqual(rotated[0].center.x, 125.0), "rotated top roi x");
    require(nearlyEqual(rotated[0].center.y, 200.0), "rotated top roi y");
    require(nearlyEqual(rotated[0].angleDeg, 0.0), "rotated top roi handle points outward");
    require(nearlyEqual(rotated[2].center.x, 100.0), "rotated left roi x");
    require(nearlyEqual(rotated[2].center.y, 175.0), "rotated left roi y");
    require(nearlyEqual(rotated[1].angleDeg, 180.0), "rotated bottom roi handle points outward");
    require(nearlyEqual(rotated[2].angleDeg, -90.0), "rotated left roi handle points outward");
    require(nearlyEqual(rotated[3].angleDeg, 90.0), "rotated right roi handle points outward");
}

void testRoiGroupBoundsContainsRotatedRois()
{
    std::vector<hm::HoleRoi> rois;
    hm::HoleRoi first;
    first.center = hm::ImagePoint{ 100.0, 100.0 };
    first.width = 20.0;
    first.height = 10.0;
    first.angleDeg = 0.0;
    rois.push_back(first);

    hm::HoleRoi second;
    second.center = hm::ImagePoint{ 140.0, 100.0 };
    second.width = 20.0;
    second.height = 10.0;
    second.angleDeg = 90.0;
    rois.push_back(second);

    const hm::ImageRect bounds = hm::makeRoiGroupBounds(rois, 5.0);
    require(bounds.ok, "roi group bounds should be valid");
    require(nearlyEqual(bounds.left, 85.0), "roi group bounds left includes padding");
    require(nearlyEqual(bounds.top, 85.0), "roi group bounds top includes rotated roi and padding");
    require(nearlyEqual(bounds.width, 65.0), "roi group bounds width");
    require(nearlyEqual(bounds.height, 30.0), "roi group bounds height");
}

void testMeasurementAndSave()
{
    hm::TemplatePoint point{ 12, "C", "4", 0.0, 0.0, 0, true, 0, 2.0, -1.0 };
    const auto measurement = hm::makeMeasurement(point, hm::ImagePoint{ 10.0, 20.0 },
        hm::GaugeLine{ true, hm::ImagePoint{ 50.0, 40.0 }, 0.0, 90.0 },
        hm::GaugeLine{ true, hm::ImagePoint{ 50.0, 80.0 }, 0.0, 91.0 },
        hm::GaugeLine{ true, hm::ImagePoint{ 25.0, 60.0 }, 90.0, 88.0 },
        hm::GaugeLine{ true, hm::ImagePoint{ 75.0, 60.0 }, 90.0, 89.0 },
        2.5);

    require(measurement.ok, "measurement should be ok when four lines are ok");
    require(nearlyEqual(measurement.widthPx, 50.0), "width from left/right line spacing");
    require(nearlyEqual(measurement.heightPx, 40.0), "height from top/bottom line spacing");
    require(nearlyEqual(measurement.widthMicron, 125.0), "width micron scale");
    require(nearlyEqual(measurement.heightMicron, 100.0), "height micron scale");
    require(nearlyEqual(measurement.centerX, 50.0), "measurement center x should not be offset before export");
    require(nearlyEqual(measurement.centerY, 60.0), "measurement center y should not be offset before export");

    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/measurement_test.csv";
    const auto compensated = hm::applyMeasurementOffsets(std::vector<hm::HoleMeasurement>{ measurement });
    hm::saveMeasurementsCsv(path, compensated);
    std::ifstream file(path.c_str());
    std::string header;
    std::string row;
    std::getline(file, header);
    std::getline(file, row);
    require(header == "ID,Row Label,Column Label,CenterX,CenterY,HeightPx,WidthPx,HeightMicron,WidthMicron,OK,MeasuredWorldX,MeasuredWorldY,AlignedWorldX,AlignedWorldY,DeltaX,DeltaY,TaskAlignedWorldX,TaskAlignedWorldY,TaskDeltaX,TaskDeltaY",
        "measurement csv header");
    require(row.find("12,C,4,50,60,50,40,124,102,1,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999") == 0,
        "measurement csv row");
}

void testAlignmentCsvFieldsAndFailureSentinel()
{
    std::vector<hm::HoleMeasurement> measurements;

    hm::HoleMeasurement first;
    first.templateId = 1;
    first.rowLabel = "A";
    first.columnLabel = "1";
    first.templateWorldX = 0.0;
    first.templateWorldY = 0.0;
    first.measuredWorldX = 10.0;
    first.measuredWorldY = 20.0;
    first.alignedWorldX = 0.5;
    first.alignedWorldY = -0.25;
    first.deltaX = 0.5;
    first.deltaY = -0.25;
    first.ok = true;
    measurements.push_back(first);

    hm::HoleMeasurement second;
    second.templateId = 2;
    second.rowLabel = "B";
    second.columnLabel = "1";
    second.templateWorldX = 100.0;
    second.templateWorldY = 0.0;
    second.measuredWorldX = 10.0;
    second.measuredWorldY = 120.0;
    second.alignedWorldX = 99.75;
    second.alignedWorldY = 0.25;
    second.deltaX = -0.25;
    second.deltaY = 0.25;
    second.ok = true;
    measurements.push_back(second);

    hm::HoleMeasurement failed;
    failed.templateId = 3;
    failed.rowLabel = "C";
    failed.columnLabel = "1";
    failed.templateWorldX = 0.0;
    failed.templateWorldY = 100.0;
    failed.ok = false;
    measurements.push_back(failed);

    require(nearlyEqual(measurements[2].measuredWorldX, hm::InvalidMeasurementValue), "failed measured world x sentinel");
    require(nearlyEqual(measurements[2].alignedWorldX, hm::InvalidMeasurementValue), "failed aligned x sentinel");
    require(nearlyEqual(measurements[2].deltaX, hm::InvalidMeasurementValue), "failed delta x sentinel");

    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/alignment_measurement_test.csv";
    hm::saveMeasurementsCsv(path, measurements);
    std::ifstream file(path.c_str());
    std::string header;
    std::string firstRow;
    std::string secondRow;
    std::string failedRow;
    std::getline(file, header);
    std::getline(file, firstRow);
    std::getline(file, secondRow);
    std::getline(file, failedRow);

    require(header == "ID,Row Label,Column Label,CenterX,CenterY,HeightPx,WidthPx,HeightMicron,WidthMicron,OK,MeasuredWorldX,MeasuredWorldY,AlignedWorldX,AlignedWorldY,DeltaX,DeltaY,TaskAlignedWorldX,TaskAlignedWorldY,TaskDeltaX,TaskDeltaY",
        "alignment csv header");
    require(firstRow.find("1,A,1,0,0,0,0,0,0,1,10,20,0.5,-0.25,0.5,-0.25,-9999,-9999,-9999,-9999") == 0, "first alignment csv row");
    require(failedRow.find("3,C,1,0,0,0,0,0,0,0,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999") == 0,
        "failed alignment csv sentinel row");
}

void testTaskAlignmentOffsetsUseTaskBasisToTemplateBasis()
{
    hm::TaskAlignmentBasis basis;
    basis.taskCenterWorldX = 100.0;
    basis.taskCenterWorldY = 200.0;
    basis.taskAngleRad = 0.5235987755982988;
    basis.templateCenterWorldX = 0.0;
    basis.templateCenterWorldY = 0.0;
    basis.templateAngleRad = 0.0;

    hm::HoleMeasurement measurement;
    measurement.ok = true;
    measurement.templateWorldX = 10.0;
    measurement.templateWorldY = 0.0;
    measurement.measuredWorldX = 111.89230484541326;
    measurement.measuredWorldY = 203.40192378864668;

    hm::HoleMeasurement failed;
    failed.ok = false;
    failed.templateWorldX = 10.0;
    failed.templateWorldY = 0.0;

    std::vector<hm::HoleMeasurement> transformed;
    transformed.push_back(measurement);
    transformed.push_back(failed);

    hm::applyTaskAlignmentOffsets(transformed, basis);

    require(nearlyEqual(transformed[0].taskAlignedWorldX, 12.0), "task aligned x should map through task basis");
    require(nearlyEqual(transformed[0].taskAlignedWorldY, -3.0), "task aligned y should map through task basis");
    require(nearlyEqual(transformed[0].taskDeltaX, 2.0), "task delta x should compare transformed x to template x");
    require(nearlyEqual(transformed[0].taskDeltaY, 3.0), "task delta y should follow pattern TP y sign");
    require(nearlyEqual(transformed[1].taskAlignedWorldX, hm::InvalidMeasurementValue),
        "failed task aligned x sentinel");
    require(nearlyEqual(transformed[1].taskDeltaX, hm::InvalidMeasurementValue),
        "failed task delta x sentinel");
}

void testMeasurementAveragesMutualPerpendicularDistances()
{
    hm::TemplatePoint point{ 13, "D", "5", 0.0, 0.0, 0, true, 0 };
    const auto measurement = hm::makeMeasurement(point, hm::ImagePoint{ 0.0, 0.0 },
        hm::GaugeLine{ true, hm::ImagePoint{ 0.0, 0.0 }, 0.0, 90.0, hm::ImagePoint{ -10.0, 0.0 }, hm::ImagePoint{ 10.0, 0.0 } },
        hm::GaugeLine{ true, hm::ImagePoint{ 0.0, 15.0 }, 26.565051177, 91.0, hm::ImagePoint{ -10.0, 10.0 }, hm::ImagePoint{ 10.0, 20.0 } },
        hm::GaugeLine{ true, hm::ImagePoint{ 0.0, 0.0 }, 90.0, 88.0, hm::ImagePoint{ 0.0, -10.0 }, hm::ImagePoint{ 0.0, 10.0 } },
        hm::GaugeLine{ true, hm::ImagePoint{ 15.0, 0.0 }, 63.434948823, 89.0, hm::ImagePoint{ 10.0, -10.0 }, hm::ImagePoint{ 20.0, 10.0 } },
        1.0);

    const double expected = (15.0 + 300.0 / std::sqrt(500.0)) * 0.5;
    require(measurement.ok, "measurement should be ok when skewed line pairs are ok");
    require(nearlyEqual(measurement.widthPx, expected), "width should average mutual perpendicular distances");
    require(nearlyEqual(measurement.heightPx, expected), "height should average mutual perpendicular distances");
}

void testRoiMeasurementFailureStateFollowsSingleFailedLine()
{
    hm::TemplatePoint point{ 14, "E", "6", 0.0, 0.0, 0, true, 0 };
    const auto measurement = hm::makeMeasurement(point, hm::ImagePoint{ 0.0, 0.0 },
        hm::GaugeLine{ true },
        hm::GaugeLine{ false },
        hm::GaugeLine{ true },
        hm::GaugeLine{ true },
        1.0);

    require(!hm::isRoiMeasurementFailed(measurement, 1, false),
        "unmeasured roi should not be marked failed");
    require(!hm::isRoiMeasurementFailed(measurement, 0, true),
        "successful top roi should not be marked failed");
    require(hm::isRoiMeasurementFailed(measurement, 1, true),
        "failed bottom roi should be marked failed");
    require(!hm::isRoiMeasurementFailed(measurement, 2, true),
        "successful left roi should not be marked failed");
    require(!hm::isRoiMeasurementFailed(measurement, 3, true),
        "successful right roi should not be marked failed");
    require(!hm::isRoiMeasurementFailed(measurement, 4, true),
        "out-of-range roi should not be marked failed");
}

void testGaugeLineKeepsDetectedSegment()
{
    hm::GaugeLine line;
    line.ok = true;
    line.point = hm::ImagePoint{ 20.0, 30.0 };
    line.angleDeg = 12.5;
    line.score = 88.0;
    line.start = hm::ImagePoint{ 10.0, 25.0 };
    line.end = hm::ImagePoint{ 30.0, 35.0 };

    require(nearlyEqual(line.start.x, 10.0), "gauge line start x");
    require(nearlyEqual(line.start.y, 25.0), "gauge line start y");
    require(nearlyEqual(line.end.x, 30.0), "gauge line end x");
    require(nearlyEqual(line.end.y, 35.0), "gauge line end y");
}

void testCenterCrossLinesUseImageCenter()
{
    const auto lines = hm::makeCenterCrossLines(hm::ImagePoint{ 100.0, 80.0 }, 6.0);

    require(lines.size() == 2, "center cross should contain horizontal and vertical lines");
    require(lines[0].ok, "center cross horizontal line ok");
    require(nearlyEqual(lines[0].start.x, 94.0), "center cross horizontal start x");
    require(nearlyEqual(lines[0].start.y, 80.0), "center cross horizontal start y");
    require(nearlyEqual(lines[0].end.x, 106.0), "center cross horizontal end x");
    require(nearlyEqual(lines[0].end.y, 80.0), "center cross horizontal end y");
    require(lines[1].ok, "center cross vertical line ok");
    require(nearlyEqual(lines[1].start.x, 100.0), "center cross vertical start x");
    require(nearlyEqual(lines[1].start.y, 74.0), "center cross vertical start y");
    require(nearlyEqual(lines[1].end.x, 100.0), "center cross vertical end x");
    require(nearlyEqual(lines[1].end.y, 86.0), "center cross vertical end y");
}

void testHoleIdLabelsFollowCurrentRoiBounds()
{
    std::vector<hm::TemplatePoint> points = {
        { 10, "A", "1", 0.0, 0.0, 0, true, 0 },
        { 11, "A", "2", 0.0, 0.0, 0, true, 0 }
    };

    hm::HoleRoi firstTop;
    firstTop.center = hm::ImagePoint{ 300.0, 190.0 };
    firstTop.width = 40.0;
    firstTop.height = 10.0;
    firstTop.angleDeg = 0.0;
    hm::HoleRoi firstBottom = firstTop;
    firstBottom.center = hm::ImagePoint{ 300.0, 210.0 };

    hm::HoleRoi secondLeft;
    secondLeft.center = hm::ImagePoint{ 420.0, 280.0 };
    secondLeft.width = 10.0;
    secondLeft.height = 40.0;
    secondLeft.angleDeg = 90.0;
    hm::HoleRoi secondRight = secondLeft;
    secondRight.center = hm::ImagePoint{ 460.0, 280.0 };

    std::vector<std::vector<hm::HoleRoi> > roiGroups = {
        { firstTop, firstBottom },
        { secondLeft, secondRight }
    };

    const std::vector<hm::HoleLabel> labels = hm::makeHoleLabels(points, roiGroups);

    require(labels.size() == 2, "hole labels should only include ids");
    require(labels[0].kind == hm::HoleLabelKind::Id && labels[0].text == "1", "first id label text");
    require(nearlyEqual(labels[0].position.x, 300.0), "first id label x follows roi bounds");
    require(nearlyEqual(labels[0].position.y, 200.0), "first id label y follows roi bounds");
    require(labels[1].kind == hm::HoleLabelKind::Id && labels[1].text == "2", "second id label text");
    require(nearlyEqual(labels[1].position.x, 440.0), "second id label x follows roi bounds");
    require(nearlyEqual(labels[1].position.y, 280.0), "second id label y follows roi bounds");
}

void testStableProfileMasterPrefersExistingOrLoadedRoiMaster()
{
    std::vector<hm::TemplatePoint> points = {
        { 1, "A", "1", 0.0, 0.0, 0, true, 0 },
        { 40, "A", "40", 0.0, 0.0, 0, true, 0 },
        { 80, "B", "40", 0.0, 0.0, 0, true, 0 },
    };
    std::vector<int> profiles = { 0, 0, 0 };

    require(hm::selectStableProfileMasterIndex(points, profiles, 0, 40, {}) == 1,
        "existing profile master should survive sorting changes");
    require(hm::selectStableProfileMasterIndex(points, profiles, 0, 0, std::vector<int>{ 80, 40 }) == 2,
        "loaded roi master ids should be preferred over first sorted hole");
    require(hm::selectStableProfileMasterIndex(points, profiles, 0, 0, {}) == 0,
        "profile without saved master falls back to first sorted hole");
}

void testRoiAdjustmentsPreserveCurrentGeometryAfterRegroup()
{
    hm::GaugeDefaults defaults;
    defaults.edgeOffsetPx = 20.0;
    defaults.roiLengthPx = 50.0;
    defaults.roiWidthPx = 12.0;
    const auto masterRois = hm::makeDefaultHoleRois(hm::ImagePoint{ 100.0, 200.0 }, 10, defaults);
    auto currentRois = hm::rebaseMasterRois(
        masterRois,
        hm::ImagePoint{ 100.0, 200.0 },
        hm::ImagePoint{ 300.0, 500.0 },
        20);
    currentRois[0].center.x += 7.0;
    currentRois[0].center.y -= 3.0;
    currentRois[0].width += 4.0;

    const auto adjustments = hm::makeRoiAdjustmentsForCurrentRois(
        masterRois,
        hm::ImagePoint{ 100.0, 200.0 },
        hm::ImagePoint{ 300.0, 500.0 },
        20,
        currentRois);

    require(adjustments.size() == currentRois.size(), "regroup adjustment count");
    const auto rebuilt = hm::makeDerivedHoleRoi(
        masterRois[0],
        hm::ImagePoint{ 100.0, 200.0 },
        hm::ImagePoint{ 300.0, 500.0 },
        20,
        adjustments[0]);
    require(nearlyEqual(rebuilt.center.x, currentRois[0].center.x), "regroup adjustment preserves roi x");
    require(nearlyEqual(rebuilt.center.y, currentRois[0].center.y), "regroup adjustment preserves roi y");
    require(nearlyEqual(rebuilt.width, currentRois[0].width), "regroup adjustment preserves roi width");
}

void testLineCandidateSelectionRejectsSkewedHighScoreLine()
{
    hm::HoleRoi roi;
    roi.center = hm::ImagePoint{ 100.0, 100.0 };
    roi.width = 80.0;
    roi.height = 18.0;
    roi.angleDeg = 0.0;

    std::vector<hm::GaugeLine> candidates;
    candidates.push_back(hm::GaugeLine{
        true,
        hm::ImagePoint{ 100.0, 100.0 },
        45.0,
        99.0,
        hm::ImagePoint{ 80.0, 80.0 },
        hm::ImagePoint{ 120.0, 120.0 }
    });
    candidates.push_back(hm::GaugeLine{
        true,
        hm::ImagePoint{ 102.0, 100.0 },
        89.0,
        55.0,
        hm::ImagePoint{ 102.0, 70.0 },
        hm::ImagePoint{ 102.0, 130.0 }
    });
    candidates.push_back(hm::GaugeLine{
        true,
        hm::ImagePoint{ 130.0, 100.0 },
        91.0,
        80.0,
        hm::ImagePoint{ 130.0, 70.0 },
        hm::ImagePoint{ 130.0, 130.0 }
    });

    const int index = hm::selectLineCandidateIndex(candidates, roi, 10.0);
    require(index == 1, "line selection should prefer the centered candidate with expected angle");
    const hm::GaugeLine selected = hm::selectLineCandidate(candidates, roi, 10.0);
    require(selected.ok && nearlyEqual(selected.point.x, 102.0), "selected line should be valid expected candidate");
}

void testLineAngleDifferenceTreatsOppositeDirectionsAsSameLine()
{
    require(nearlyEqual(hm::lineAngleDifferenceDeg(90.0, -90.0), 0.0),
        "opposite line directions should have zero angle difference");
    require(nearlyEqual(hm::lineAngleDifferenceDeg(179.0, -1.0), 0.0),
        "line angle comparison should wrap at 180 degrees");
    require(nearlyEqual(hm::expectedLineAngleDeg(hm::HoleRoi{ 0, hm::RoiSide::Right, hm::ImagePoint{}, 0.0, 0.0, 0.0 }),
        90.0), "expected line angle should be perpendicular to outward roi direction");
}

void testRoiConfigSaveLoad()
{
    hm::HoleRoi roi;
    roi.templateId = 9;
    roi.side = hm::RoiSide::Right;
    roi.center = hm::ImagePoint{ 123.5, 456.25 };
    roi.width = 77.0;
    roi.height = 18.0;
    roi.angleDeg = -12.5;
    roi.params.kernelSize = 5;
    roi.params.polarity = 1;
    roi.params.acceptScore = 44;
    roi.params.normScore = true;
    roi.params.sampleRegionWidth = 11;
    roi.params.sampleRegionHeight = 22;
    roi.params.sampleRegionInterval = 6;
    roi.params.maxSamplePointCount = 2;
    roi.params.fitDistThreshold = 1.25;
    roi.params.fitCountThreshold = 3;

    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/roi_config_test.csv";
    hm::saveRoisCsv(path, std::vector<hm::HoleRoi>{ roi });
    const auto loaded = hm::loadRoisCsv(path);

    require(loaded.size() == 1, "roi config load count");
    require(loaded[0].templateId == 9, "roi config template id");
    require(loaded[0].side == hm::RoiSide::Right, "roi config side");
    require(nearlyEqual(loaded[0].center.x, 123.5), "roi config center x");
    require(nearlyEqual(loaded[0].center.y, 456.25), "roi config center y");
    require(nearlyEqual(loaded[0].angleDeg, -12.5), "roi config angle");
    require(loaded[0].params.polarity == 1, "roi config polarity");
    require(loaded[0].params.normScore, "roi config norm score");
    require(loaded[0].params.sampleRegionWidth == 11, "roi config sample width");
    require(nearlyEqual(loaded[0].params.fitDistThreshold, 1.25), "roi config fit dist");
}

void testAppParamsSaveLoad()
{
    hm::AppParams params;
    params.offsetX = -12.5;
    params.offsetY = 34.25;
    params.angleDeg = 91.5;
    params.micronPerPixel = 2.75;
    params.pointOrder = hm::ExportOrder::RowFirstBottomRight;
    params.lineFindMethod = hm::LineFindMethod::LineDetector;
    params.taskPath = "D:/tasks/chip_align.task";
    params.ibPath = "D:/IBService";
    params.gauge.edgeOffsetPx = 31.0;
    params.gauge.roiLengthPx = 42.0;
    params.gauge.roiWidthPx = 53.0;
    params.gauge.kernelSize = 7;
    params.gauge.polarity = 4;
    params.gauge.acceptScore = 66;
    params.gauge.normScore = true;
    params.gauge.sampleRegionWidth = 11;
    params.gauge.sampleRegionHeight = 22;
    params.gauge.sampleRegionInterval = 33;
    params.gauge.maxSamplePointCount = 3;
    params.gauge.fitDistThreshold = 1.75;
    params.gauge.fitCountThreshold = 5;
    params.roiProfiles.push_back(hm::RoiProfileRange{ 0, 1, 3 });
    params.roiProfiles.push_back(hm::RoiProfileRange{ 1, 4, 99 });
    params.columnProfiles.push_back(hm::RoiColumnProfile{ "1", 0 });
    params.columnProfiles.push_back(hm::RoiColumnProfile{ "5", 2 });

    const std::string path = "C:/Users/22113/Desktop/files/QtHoleMeasure/tests/app_params_test.ini";
    hm::saveAppParams(path, params);

    hm::AppParams loaded;
    const bool ok = hm::loadAppParams(path, loaded);

    require(ok, "app params load should succeed");
    require(nearlyEqual(loaded.offsetX, -12.5), "app params offset x");
    require(nearlyEqual(loaded.offsetY, 34.25), "app params offset y");
    require(nearlyEqual(loaded.angleDeg, 91.5), "app params angle");
    require(nearlyEqual(loaded.micronPerPixel, 2.75), "app params micron scale");
    require(loaded.pointOrder == hm::ExportOrder::RowFirstBottomRight, "app params point order");
    require(loaded.lineFindMethod == hm::LineFindMethod::LineDetector, "app params line find method");
    require(loaded.taskPath == "D:/tasks/chip_align.task", "app params task path");
    require(loaded.ibPath == "D:/IBService", "app params ib path");
    require(nearlyEqual(loaded.gauge.edgeOffsetPx, 31.0), "app params edge offset");
    require(nearlyEqual(loaded.gauge.roiLengthPx, 42.0), "app params roi length");
    require(nearlyEqual(loaded.gauge.roiWidthPx, 53.0), "app params roi width");
    require(loaded.gauge.kernelSize == 7, "app params kernel size");
    require(loaded.gauge.polarity == 4, "app params polarity");
    require(loaded.gauge.acceptScore == 66, "app params accept score");
    require(loaded.gauge.normScore, "app params norm score");
    require(loaded.gauge.sampleRegionWidth == 11, "app params sample width");
    require(loaded.gauge.sampleRegionHeight == 22, "app params sample height");
    require(loaded.gauge.sampleRegionInterval == 33, "app params sample interval");
    require(loaded.gauge.maxSamplePointCount == 3, "app params max sample count");
    require(nearlyEqual(loaded.gauge.fitDistThreshold, 1.75), "app params fit dist");
    require(loaded.gauge.fitCountThreshold == 5, "app params fit count");
    require(loaded.roiProfiles.size() == 2, "app params roi profile count");
    require(loaded.roiProfiles[0].startColumn == 1 && loaded.roiProfiles[0].endColumn == 3,
        "app params first roi profile range");
    require(loaded.roiProfiles[1].profileIndex == 1 && loaded.roiProfiles[1].startColumn == 4,
        "app params second roi profile range");
    require(loaded.columnProfiles.size() == 2, "app params column profile count");
    require(loaded.columnProfiles[0].columnLabel == "1" && loaded.columnProfiles[0].profileIndex == 0,
        "app params first column profile");
    require(loaded.columnProfiles[1].columnLabel == "5" && loaded.columnProfiles[1].profileIndex == 2,
        "app params second column profile");
}

void testZoomViewAtCursorKeepsImagePointUnderCursor()
{
    const int imageWidth = 1000;
    const int imageHeight = 500;
    const int viewWidth = 800;
    const int viewHeight = 600;
    const hm::ViewState state{ 1.5, 40.0, -30.0 };
    const double cursorX = 320.0;
    const double cursorY = 260.0;

    const double fitZoom = 0.8;
    const double oldZoom = fitZoom * state.scale;
    const double oldPanX = (viewWidth - imageWidth * oldZoom) * 0.5 + state.panX;
    const double oldPanY = (viewHeight - imageHeight * oldZoom) * 0.5 + state.panY;
    const double imageX = (cursorX - oldPanX) / oldZoom;
    const double imageY = (cursorY - oldPanY) / oldZoom;

    const hm::ViewState zoomed = hm::zoomViewAtCursor(
        imageWidth, imageHeight, viewWidth, viewHeight,
        state, cursorX, cursorY, 1.25, 0.05, 80.0);

    const double newZoom = fitZoom * zoomed.scale;
    const double newPanX = (viewWidth - imageWidth * newZoom) * 0.5 + zoomed.panX;
    const double newPanY = (viewHeight - imageHeight * newZoom) * 0.5 + zoomed.panY;

    require(nearlyEqual((cursorX - newPanX) / newZoom, imageX), "zoom should preserve cursor image x");
    require(nearlyEqual((cursorY - newPanY) / newZoom, imageY), "zoom should preserve cursor image y");
}

void testZoomViewAtViewportCursorKeepsImagePointUnderCursor()
{
    const int imageWidth = 1000;
    const int imageHeight = 500;
    const int viewWidth = 796;
    const int viewHeight = 596;
    const double viewportX = 2.0;
    const double viewportY = 2.0;
    const hm::ViewState state{ 1.5, 40.0, -30.0 };
    const double cursorX = 320.0;
    const double cursorY = 260.0;

    const double fitZoom = 0.796;
    const double oldZoom = fitZoom * state.scale;
    const double oldPanX = viewportX + (viewWidth - imageWidth * oldZoom) * 0.5 + state.panX;
    const double oldPanY = viewportY + (viewHeight - imageHeight * oldZoom) * 0.5 + state.panY;
    const double imageX = (cursorX - oldPanX) / oldZoom;
    const double imageY = (cursorY - oldPanY) / oldZoom;

    const hm::ViewState zoomed = hm::zoomViewAtViewportCursor(
        imageWidth, imageHeight, viewWidth, viewHeight, viewportX, viewportY,
        state, cursorX, cursorY, 1.25, 0.05, 80.0);

    const double newZoom = fitZoom * zoomed.scale;
    const double newPanX = viewportX + (viewWidth - imageWidth * newZoom) * 0.5 + zoomed.panX;
    const double newPanY = viewportY + (viewHeight - imageHeight * newZoom) * 0.5 + zoomed.panY;

    require(nearlyEqual((cursorX - newPanX) / newZoom, imageX),
        "viewport zoom should preserve cursor image x");
    require(nearlyEqual((cursorY - newPanY) / newZoom, imageY),
        "viewport zoom should preserve cursor image y");
}

void testViewTransformsSeparateDrawingFromWidgetEvents()
{
    const int imageWidth = 1000;
    const int imageHeight = 500;
    const int viewWidth = 796;
    const int viewHeight = 596;
    const double viewportX = 2.0;
    const double viewportY = 2.0;
    const hm::ViewState state{ 1.5, 40.0, -30.0 };

    const hm::ViewTransform draw = hm::makeViewTransform(
        imageWidth, imageHeight, viewWidth, viewHeight, state);
    const hm::ViewTransform event = hm::makeViewportViewTransform(
        imageWidth, imageHeight, viewWidth, viewHeight, viewportX, viewportY, state);

    require(nearlyEqual(draw.zoomX, 1.194), "draw transform zoom x");
    require(nearlyEqual(draw.panX, -159.0), "draw transform should be content-local");
    require(nearlyEqual(draw.panY, -30.5), "draw transform y should be content-local");
    require(nearlyEqual(event.panX, draw.panX + viewportX), "event transform x should include viewport offset");
    require(nearlyEqual(event.panY, draw.panY + viewportY), "event transform y should include viewport offset");
}

void testDerivedRoiFollowsMasterWithPerHoleAdjustment()
{
    hm::HoleRoi master;
    master.templateId = 1;
    master.side = hm::RoiSide::Right;
    master.center = hm::ImagePoint{ 110.0, 205.0 };
    master.width = 70.0;
    master.height = 16.0;
    master.angleDeg = 5.0;
    master.params.polarity = 2;
    master.params.acceptScore = 41;

    const hm::ImagePoint masterCenter{ 100.0, 200.0 };
    const hm::ImagePoint targetCenter{ 150.0, 260.0 };

    const hm::HoleRoi followed = hm::makeDerivedHoleRoi(
        master, masterCenter, targetCenter, 9, hm::RoiAdjustment{});

    require(followed.templateId == 9, "derived roi target id");
    require(followed.side == hm::RoiSide::Right, "derived roi side");
    require(nearlyEqual(followed.center.x, 160.0), "derived roi follows x offset");
    require(nearlyEqual(followed.center.y, 265.0), "derived roi follows y offset");
    require(nearlyEqual(followed.width, 70.0), "derived roi follows width");
    require(nearlyEqual(followed.height, 16.0), "derived roi follows height");
    require(nearlyEqual(followed.angleDeg, 5.0), "derived roi follows angle");
    require(followed.params.polarity == 2, "derived roi follows params");

    hm::HoleRoi edited = followed;
    edited.center.x += 3.0;
    edited.center.y -= 4.0;
    edited.width += 5.0;
    edited.height += 2.0;
    edited.angleDeg -= 7.0;
    edited.params.polarity = 4;
    edited.params.acceptScore = 55;

    const hm::RoiAdjustment adjustment = hm::makeRoiAdjustment(followed, edited);
    const hm::HoleRoi adjusted = hm::makeDerivedHoleRoi(
        master, masterCenter, targetCenter, 9, adjustment);

    require(adjustment.enabled, "roi adjustment should be enabled");
    require(adjustment.paramsOverridden, "roi adjustment should override params");
    require(nearlyEqual(adjusted.center.x, edited.center.x), "adjusted roi center x");
    require(nearlyEqual(adjusted.center.y, edited.center.y), "adjusted roi center y");
    require(nearlyEqual(adjusted.width, edited.width), "adjusted roi width");
    require(nearlyEqual(adjusted.height, edited.height), "adjusted roi height");
    require(nearlyEqual(adjusted.angleDeg, edited.angleDeg), "adjusted roi angle");
    require(adjusted.params.polarity == 4, "adjusted roi polarity");
    require(adjusted.params.acceptScore == 55, "adjusted roi score");
}

void testRoiAdjustmentCanKeepGeometryWhileClearingGaugeParams()
{
    hm::HoleRoi master;
    master.templateId = 1;
    master.side = hm::RoiSide::Top;
    master.center = hm::ImagePoint{ 100.0, 100.0 };
    master.width = 40.0;
    master.height = 10.0;
    master.angleDeg = -90.0;
    master.params.sampleRegionWidth = 8;
    master.params.acceptScore = 40;
    master.params.polarity = 0;

    const hm::ImagePoint masterCenter{ 100.0, 100.0 };
    const hm::ImagePoint targetCenter{ 200.0, 100.0 };
    const hm::HoleRoi base = hm::makeDerivedHoleRoi(
        master, masterCenter, targetCenter, 2, hm::RoiAdjustment{});

    hm::HoleRoi edited = base;
    edited.center.x += 5.0;
    edited.width += 3.0;
    edited.params.sampleRegionWidth = 15;
    edited.params.acceptScore = 70;
    edited.params.polarity = 4;

    hm::RoiAdjustment adjustment = hm::makeRoiAdjustment(base, edited);
    require(adjustment.paramsOverridden, "adjustment starts with param override");

    adjustment = hm::withoutGaugeParamOverride(adjustment);
    master.params.sampleRegionWidth = 22;
    master.params.acceptScore = 66;
    master.params.polarity = 2;

    const hm::HoleRoi updated = hm::makeDerivedHoleRoi(
        master, masterCenter, targetCenter, 2, adjustment);

    require(!adjustment.paramsOverridden, "param override cleared");
    require(nearlyEqual(updated.center.x, edited.center.x), "geometry adjustment still applies");
    require(nearlyEqual(updated.width, edited.width), "size adjustment still applies");
    require(updated.params.sampleRegionWidth == 22, "global sample width should apply");
    require(updated.params.acceptScore == 66, "global score should apply");
    require(updated.params.polarity == 2, "global polarity should apply");
}

void testRebaseMasterRoisToNewMasterCenter()
{
    hm::GaugeDefaults defaults;
    defaults.edgeOffsetPx = 20.0;
    defaults.roiLengthPx = 50.0;
    defaults.roiWidthPx = 12.0;
    const auto masterRois = hm::makeDefaultHoleRois(hm::ImagePoint{ 100.0, 200.0 }, 10, defaults);

    const auto rebased = hm::rebaseMasterRois(
        masterRois,
        hm::ImagePoint{ 100.0, 200.0 },
        hm::ImagePoint{ 300.0, 500.0 },
        99);

    require(rebased.size() == masterRois.size(), "rebased master roi count");
    require(rebased[0].templateId == 99, "rebased roi template id");
    require(nearlyEqual(rebased[0].center.x - 300.0, masterRois[0].center.x - 100.0),
        "rebased roi x offset should be preserved");
    require(nearlyEqual(rebased[0].center.y - 500.0, masterRois[0].center.y - 200.0),
        "rebased roi y offset should be preserved");
    require(nearlyEqual(rebased[2].angleDeg, masterRois[2].angleDeg),
        "rebased roi angle should be preserved");
}

void testRoiProfileSelectionByColumnRange()
{
    std::vector<hm::RoiProfileRange> profiles;
    profiles.push_back(hm::RoiProfileRange{ 0, 1, 3 });
    profiles.push_back(hm::RoiProfileRange{ 1, 4, 99 });

    require(hm::selectRoiProfileIndex(profiles, "1") == 0, "column 1 uses first profile");
    require(hm::selectRoiProfileIndex(profiles, "3") == 0, "column 3 uses first profile");
    require(hm::selectRoiProfileIndex(profiles, "4") == 1, "column 4 uses second profile");

    profiles.push_back(hm::RoiProfileRange{ 2, 6, 8 });
    require(hm::selectRoiProfileIndex(profiles, "7") == 2, "later matching profile overrides earlier range");
    require(hm::selectRoiProfileIndex(profiles, "abc") == 0, "invalid column falls back to first profile");
}

void testRoiProfileSelectionByExplicitColumn()
{
    std::vector<hm::RoiColumnProfile> assignments;
    assignments.push_back(hm::RoiColumnProfile{ "1", 0 });
    assignments.push_back(hm::RoiColumnProfile{ "5", 2 });

    require(hm::selectRoiProfileIndex(assignments, "1") == 0, "explicit column 1 profile");
    require(hm::selectRoiProfileIndex(assignments, "5") == 2, "explicit column 5 profile");
    require(hm::selectRoiProfileIndex(assignments, "7") == 0, "unassigned column falls back to profile 1");
}
}

int main()
{
    testParseTemplateCsv();
    testParseTemplateCsvWithoutOffsetsDefaultsToZero();
    testSortOrders();
    testSortTemplateImagePairs();
    testSortColumnLabelsForOrder();
    testProfileLabelsFollowOrderMajor();
    testMicronPerPixelEstimateDoesNotDependOnCurrentOrder();
    testOffsetTransform();
    testRoiGeneration();
    testRoiGroupBoundsContainsRotatedRois();
    testMeasurementAndSave();
    testAlignmentCsvFieldsAndFailureSentinel();
    testTaskAlignmentOffsetsUseTaskBasisToTemplateBasis();
    testMeasurementAveragesMutualPerpendicularDistances();
    testRoiMeasurementFailureStateFollowsSingleFailedLine();
    testGaugeLineKeepsDetectedSegment();
    testCenterCrossLinesUseImageCenter();
    testHoleIdLabelsFollowCurrentRoiBounds();
    testStableProfileMasterPrefersExistingOrLoadedRoiMaster();
    testRoiAdjustmentsPreserveCurrentGeometryAfterRegroup();
    testLineCandidateSelectionRejectsSkewedHighScoreLine();
    testLineAngleDifferenceTreatsOppositeDirectionsAsSameLine();
    testRoiConfigSaveLoad();
    testAppParamsSaveLoad();
    testZoomViewAtCursorKeepsImagePointUnderCursor();
    testZoomViewAtViewportCursorKeepsImagePointUnderCursor();
    testViewTransformsSeparateDrawingFromWidgetEvents();
    testDerivedRoiFollowsMasterWithPerHoleAdjustment();
    testRoiAdjustmentCanKeepGeometryWhileClearingGaugeParams();
    testRebaseMasterRoisToNewMasterCenter();
    testRoiProfileSelectionByColumnRange();
    testRoiProfileSelectionByExplicitColumn();
    std::cout << "CoreTests passed" << std::endl;
    return 0;
}
