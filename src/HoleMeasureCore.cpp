#include "HoleMeasureCore.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

namespace hm
{
namespace
{
double normalizeAngleDeg(double angle)
{
    while (angle > 180.0) {
        angle -= 360.0;
    }
    while (angle <= -180.0) {
        angle += 360.0;
    }
    return angle;
}

bool nearlyEqual(double a, double b)
{
    return std::fabs(a - b) <= 1e-9;
}

bool sameGaugeParams(const GaugeParams& a, const GaugeParams& b)
{
    return a.kernelSize == b.kernelSize
        && a.polarity == b.polarity
        && a.acceptScore == b.acceptScore
        && a.normScore == b.normScore
        && a.sampleRegionWidth == b.sampleRegionWidth
        && a.sampleRegionHeight == b.sampleRegionHeight
        && a.sampleRegionInterval == b.sampleRegionInterval
        && a.maxSamplePointCount == b.maxSamplePointCount
        && nearlyEqual(a.fitDistThreshold, b.fitDistThreshold)
        && a.fitCountThreshold == b.fitCountThreshold;
}

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    std::stringstream ss(line);
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }
    while (!line.empty() && line.back() == ',') {
        fields.push_back(std::string());
        break;
    }
    return fields;
}

RoiSide sideFromIndex(int index)
{
    switch (index) {
    case 1: return RoiSide::Bottom;
    case 2: return RoiSide::Left;
    case 3: return RoiSide::Right;
    default: return RoiSide::Top;
    }
}

int sideIndex(RoiSide side)
{
    switch (side) {
    case RoiSide::Bottom: return 1;
    case RoiSide::Left: return 2;
    case RoiSide::Right: return 3;
    case RoiSide::Top:
    default: return 0;
    }
}

int columnIndex(const TemplatePoint& point)
{
    try {
        return std::stoi(point.columnLabel);
    }
    catch (...) {
        return point.id;
    }
}

int rowIndex(const TemplatePoint& point)
{
    const std::string& label = point.rowLabel;
    int value = 0;
    for (char ch : label) {
        if (ch >= 'A' && ch <= 'Z') {
            value = value * 26 + (ch - 'A' + 1);
        }
        else if (ch >= 'a' && ch <= 'z') {
            value = value * 26 + (ch - 'a' + 1);
        }
    }
    return value > 0 ? value : point.id;
}

int rowLabelIndex(const std::string& label)
{
    int value = 0;
    for (char ch : label) {
        if (ch >= 'A' && ch <= 'Z') {
            value = value * 26 + (ch - 'A' + 1);
        }
        else if (ch >= 'a' && ch <= 'z') {
            value = value * 26 + (ch - 'a' + 1);
        }
    }
    return value;
}

bool exportOrderIsRowFirst(ExportOrder order)
{
    return order == ExportOrder::RowFirstTopLeft
        || order == ExportOrder::RowFirstBottomLeft
        || order == ExportOrder::RowFirstBottomRight
        || order == ExportOrder::RowFirstTopRight;
}

bool exportOrderStartsTop(ExportOrder order)
{
    return order == ExportOrder::RowFirstTopLeft
        || order == ExportOrder::RowFirstTopRight
        || order == ExportOrder::ColumnFirstTopLeft
        || order == ExportOrder::ColumnFirstTopRight;
}

bool pointBefore(const TemplatePoint& a, const TemplatePoint& b, ExportOrder order)
{
    const int ca = columnIndex(a);
    const int cb = columnIndex(b);
    const int ra = rowIndex(a);
    const int rb = rowIndex(b);

    const bool rowFirst = exportOrderIsRowFirst(order);
    const bool topStart = exportOrderStartsTop(order);
    const bool leftStart = order == ExportOrder::RowFirstTopLeft
        || order == ExportOrder::RowFirstBottomLeft
        || order == ExportOrder::ColumnFirstTopLeft
        || order == ExportOrder::ColumnFirstBottomLeft;

    if (rowFirst) {
        if (ra != rb) {
            return topStart ? ra < rb : ra > rb;
        }
        if (ca != cb) {
            return leftStart ? ca < cb : ca > cb;
        }
    }
    else {
        if (ca != cb) {
            return leftStart ? ca < cb : ca > cb;
        }
        if (ra != rb) {
            return topStart ? ra < rb : ra > rb;
        }
    }
    return a.id < b.id;
}

bool exportOrderStartsLeft(ExportOrder order)
{
    return order == ExportOrder::RowFirstTopLeft
        || order == ExportOrder::RowFirstBottomLeft
        || order == ExportOrder::ColumnFirstTopLeft
        || order == ExportOrder::ColumnFirstBottomLeft;
}

ExportOrder exportOrderFromInt(int value, ExportOrder fallback)
{
    switch (value) {
    case 0: return ExportOrder::ColumnFirstTopLeft;
    case 1: return ExportOrder::ColumnFirstTopRight;
    case 2: return ExportOrder::RowFirstTopLeft;
    case 3: return ExportOrder::RowFirstBottomLeft;
    case 4: return ExportOrder::RowFirstBottomRight;
    case 5: return ExportOrder::RowFirstTopRight;
    case 6: return ExportOrder::ColumnFirstBottomLeft;
    case 7: return ExportOrder::ColumnFirstBottomRight;
    default: return fallback;
    }
}

double pointLineDistance(const ImagePoint& point, const GaugeLine& line)
{
    double dx = line.end.x - line.start.x;
    double dy = line.end.y - line.start.y;
    double length = std::sqrt(dx * dx + dy * dy);
    ImagePoint origin = line.start;

    if (length <= 1e-9) {
        const double radians = line.angleDeg * 3.14159265358979323846 / 180.0;
        dx = std::cos(radians);
        dy = std::sin(radians);
        length = 1.0;
        origin = line.point;
    }

    const double px = point.x - origin.x;
    const double py = point.y - origin.y;
    return std::fabs(dx * py - dy * px) / length;
}

bool lineIntersection(const GaugeLine& first, const GaugeLine& second, ImagePoint& intersection)
{
    double ax = first.end.x - first.start.x;
    double ay = first.end.y - first.start.y;
    double bx = second.end.x - second.start.x;
    double by = second.end.y - second.start.y;
    ImagePoint a = first.start;
    ImagePoint b = second.start;

    if (std::sqrt(ax * ax + ay * ay) <= 1e-9) {
        const double radians = first.angleDeg * 3.14159265358979323846 / 180.0;
        ax = std::cos(radians);
        ay = std::sin(radians);
        a = first.point;
    }
    if (std::sqrt(bx * bx + by * by) <= 1e-9) {
        const double radians = second.angleDeg * 3.14159265358979323846 / 180.0;
        bx = std::cos(radians);
        by = std::sin(radians);
        b = second.point;
    }

    const double denom = ax * by - ay * bx;
    if (std::fabs(denom) <= 1e-9) {
        return false;
    }

    const double cx = b.x - a.x;
    const double cy = b.y - a.y;
    const double t = (cx * by - cy * bx) / denom;
    intersection = ImagePoint{ a.x + t * ax, a.y + t * ay };
    return true;
}

bool measuredCenterFromLines(const GaugeLine& top, const GaugeLine& bottom,
    const GaugeLine& left, const GaugeLine& right, ImagePoint& center)
{
    ImagePoint points[4];
    if (!lineIntersection(top, left, points[0])
        || !lineIntersection(top, right, points[1])
        || !lineIntersection(bottom, left, points[2])
        || !lineIntersection(bottom, right, points[3])) {
        return false;
    }

    center = ImagePoint{
        (points[0].x + points[1].x + points[2].x + points[3].x) * 0.25,
        (points[0].y + points[1].y + points[2].y + points[3].y) * 0.25
    };
    return true;
}

void setInvalidAlignment(HoleMeasurement& measurement)
{
    measurement.measuredWorldX = InvalidMeasurementValue;
    measurement.measuredWorldY = InvalidMeasurementValue;
    measurement.alignedWorldX = InvalidMeasurementValue;
    measurement.alignedWorldY = InvalidMeasurementValue;
    measurement.deltaX = InvalidMeasurementValue;
    measurement.deltaY = InvalidMeasurementValue;
}
}

std::vector<TemplatePoint> loadTemplateCsv(const std::string& path)
{
    std::ifstream file(path.c_str());
    std::vector<TemplatePoint> points;
    std::string line;

    bool inTable = false;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        if (!inTable) {
            inTable = line.find("ID,Row Label,Column Label") == 0;
            continue;
        }

        const auto fields = splitCsvLine(line);
        if (fields.size() < 8) {
            continue;
        }

        TemplatePoint point;
        point.id = std::stoi(fields[0]);
        point.rowLabel = fields[1];
        point.columnLabel = fields[2];
        point.worldX = std::stod(fields[3]);
        point.worldY = std::stod(fields[4]);
        point.groupId = std::stoi(fields[5]);
        point.enabled = std::stoi(fields[6]) != 0;
        point.pinDirection = std::stoi(fields[7]);
        if (fields.size() >= 10) {
            point.xOffset = fields[8].empty() ? 0.0 : std::stod(fields[8]);
            point.yOffset = fields[9].empty() ? 0.0 : std::stod(fields[9]);
        }
        if (point.enabled) {
            points.push_back(point);
        }
    }

    return points;
}

std::vector<TemplatePoint> sortForExport(const std::vector<TemplatePoint>& points, ExportOrder order)
{
    std::vector<TemplatePoint> sorted = points;
    std::sort(sorted.begin(), sorted.end(), [order](const TemplatePoint& a, const TemplatePoint& b) {
        return pointBefore(a, b, order);
    });
    return sorted;
}

std::vector<std::string> sortColumnLabelsForOrder(const std::vector<std::string>& columns, ExportOrder order)
{
    std::vector<std::string> sorted = columns;
    std::sort(sorted.begin(), sorted.end(), [order](const std::string& a, const std::string& b) {
        int ai = 0;
        int bi = 0;
        try {
            ai = std::stoi(a);
        }
        catch (...) {
            ai = 0;
        }
        try {
            bi = std::stoi(b);
        }
        catch (...) {
            bi = 0;
        }
        if (ai != bi) {
            return exportOrderStartsLeft(order) ? ai < bi : ai > bi;
        }
        return exportOrderStartsLeft(order) ? a < b : a > b;
    });
    return sorted;
}

std::string profileLabelForOrder(const TemplatePoint& point, ExportOrder order)
{
    return exportOrderIsRowFirst(order) ? point.rowLabel : point.columnLabel;
}

std::vector<std::string> sortProfileLabelsForOrder(const std::vector<std::string>& labels, ExportOrder order)
{
    if (!exportOrderIsRowFirst(order)) {
        return sortColumnLabelsForOrder(labels, order);
    }

    std::vector<std::string> sorted = labels;
    std::sort(sorted.begin(), sorted.end(), [order](const std::string& a, const std::string& b) {
        const int ai = rowLabelIndex(a);
        const int bi = rowLabelIndex(b);
        if (ai != bi) {
            return exportOrderStartsTop(order) ? ai < bi : ai > bi;
        }
        return exportOrderStartsTop(order) ? a < b : a > b;
    });
    return sorted;
}

void sortTemplateImagePairs(std::vector<TemplatePoint>& points, std::vector<ImagePoint>& imagePoints, ExportOrder order)
{
    if (points.size() != imagePoints.size()) {
        return;
    }

    std::vector<int> indices;
    indices.reserve(points.size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        indices.push_back(i);
    }

    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return pointBefore(points[a], points[b], order);
    });

    std::vector<TemplatePoint> sortedPoints;
    std::vector<ImagePoint> sortedImagePoints;
    sortedPoints.reserve(points.size());
    sortedImagePoints.reserve(imagePoints.size());
    for (int index : indices) {
        sortedPoints.push_back(points[index]);
        sortedImagePoints.push_back(imagePoints[index]);
    }

    points.swap(sortedPoints);
    imagePoints.swap(sortedImagePoints);
}

double estimateMicronPerPixel(const std::vector<TemplatePoint>& points,
    const std::vector<ImagePoint>& imagePoints, double fallback)
{
    if (points.size() != imagePoints.size() || points.size() < 2) {
        return fallback;
    }

    double bestWorldDist = 0.0;
    double bestImageDist = 0.0;
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(points.size()); ++j) {
            const double worldDx = points[j].worldX - points[i].worldX;
            const double worldDy = points[j].worldY - points[i].worldY;
            const double imageDx = imagePoints[j].x - imagePoints[i].x;
            const double imageDy = imagePoints[j].y - imagePoints[i].y;
            const double worldDist = std::sqrt(worldDx * worldDx + worldDy * worldDy);
            const double imageDist = std::sqrt(imageDx * imageDx + imageDy * imageDy);
            if (worldDist <= 1e-9 || imageDist <= 1e-9) {
                continue;
            }
            if (bestWorldDist <= 0.0 || worldDist < bestWorldDist) {
                bestWorldDist = worldDist;
                bestImageDist = imageDist;
            }
        }
    }

    return bestWorldDist > 0.0 && bestImageDist > 0.0 ? bestWorldDist / bestImageDist : fallback;
}

std::vector<ImagePoint> applyArrayOffset(const std::vector<ImagePoint>& points, const ArrayOffset& offset)
{
    if (points.empty()) {
        return {};
    }

    ImagePoint pivot;
    for (const auto& point : points) {
        pivot.x += point.x;
        pivot.y += point.y;
    }
    pivot.x /= points.size();
    pivot.y /= points.size();

    const double radians = offset.angleDeg * 3.14159265358979323846 / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);

    std::vector<ImagePoint> result;
    result.reserve(points.size());
    for (const auto& point : points) {
        const double x = point.x - pivot.x;
        const double y = point.y - pivot.y;
        result.push_back(ImagePoint{
            pivot.x + x * c - y * s + offset.dx,
            pivot.y + x * s + y * c + offset.dy
        });
    }
    return result;
}

std::vector<HoleRoi> makeDefaultHoleRois(const ImagePoint& center, int templateId, const GaugeDefaults& defaults)
{
    std::vector<HoleRoi> rois;
    rois.reserve(4);
    const double radians = defaults.orientationDeg * 3.14159265358979323846 / 180.0;
    const double ux = std::cos(radians);
    const double uy = std::sin(radians);
    const double vx = -std::sin(radians);
    const double vy = std::cos(radians);

    auto makeRoi = [&](RoiSide side, double cx, double cy, double angle) {
        HoleRoi roi;
        roi.templateId = templateId;
        roi.side = side;
        roi.center = ImagePoint{ cx, cy };
        roi.width = defaults.roiLengthPx;
        roi.height = defaults.roiWidthPx;
        roi.angleDeg = angle;
        roi.params = defaults;
        rois.push_back(roi);
    };

    makeRoi(RoiSide::Top, center.x - vx * defaults.edgeOffsetPx, center.y - vy * defaults.edgeOffsetPx,
        normalizeAngleDeg(defaults.orientationDeg - 90.0));
    makeRoi(RoiSide::Bottom, center.x + vx * defaults.edgeOffsetPx, center.y + vy * defaults.edgeOffsetPx,
        normalizeAngleDeg(defaults.orientationDeg + 90.0));
    makeRoi(RoiSide::Left, center.x - ux * defaults.edgeOffsetPx, center.y - uy * defaults.edgeOffsetPx,
        normalizeAngleDeg(defaults.orientationDeg + 180.0));
    makeRoi(RoiSide::Right, center.x + ux * defaults.edgeOffsetPx, center.y + uy * defaults.edgeOffsetPx,
        normalizeAngleDeg(defaults.orientationDeg));
    return rois;
}

ImageRect makeRoiGroupBounds(const std::vector<HoleRoi>& rois, double paddingPx)
{
    ImageRect bounds;
    if (rois.empty()) {
        return bounds;
    }

    bool hasPoint = false;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    for (const auto& roi : rois) {
        const double halfWidth = roi.width * 0.5;
        const double halfHeight = roi.height * 0.5;
        const double radians = roi.angleDeg * 3.14159265358979323846 / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        const double xs[4] = { -halfWidth, halfWidth, halfWidth, -halfWidth };
        const double ys[4] = { -halfHeight, -halfHeight, halfHeight, halfHeight };

        for (int i = 0; i < 4; ++i) {
            const double x = roi.center.x + xs[i] * c - ys[i] * s;
            const double y = roi.center.y + xs[i] * s + ys[i] * c;
            if (!hasPoint) {
                minX = maxX = x;
                minY = maxY = y;
                hasPoint = true;
            }
            else {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }

    if (!hasPoint) {
        return bounds;
    }
    const double padding = std::max(0.0, paddingPx);
    bounds.left = minX - padding;
    bounds.top = minY - padding;
    bounds.width = maxX - minX + padding * 2.0;
    bounds.height = maxY - minY + padding * 2.0;
    bounds.ok = bounds.width > 0.0 && bounds.height > 0.0;
    return bounds;
}

HoleRoi makeDerivedHoleRoi(const HoleRoi& masterRoi, const ImagePoint& masterCenter,
    const ImagePoint& targetCenter, int targetTemplateId, const RoiAdjustment& adjustment)
{
    HoleRoi roi = masterRoi;
    roi.templateId = targetTemplateId;
    roi.center.x = masterRoi.center.x + targetCenter.x - masterCenter.x;
    roi.center.y = masterRoi.center.y + targetCenter.y - masterCenter.y;

    if (adjustment.enabled) {
        roi.center.x += adjustment.centerDelta.x;
        roi.center.y += adjustment.centerDelta.y;
        roi.width += adjustment.widthDelta;
        roi.height += adjustment.heightDelta;
        roi.angleDeg = normalizeAngleDeg(roi.angleDeg + adjustment.angleDelta);
    }
    if (adjustment.paramsOverridden) {
        roi.params = adjustment.params;
    }
    return roi;
}

std::vector<HoleRoi> rebaseMasterRois(const std::vector<HoleRoi>& masterRois,
    const ImagePoint& oldMasterCenter, const ImagePoint& newMasterCenter, int newMasterTemplateId)
{
    std::vector<HoleRoi> rebased;
    rebased.reserve(masterRois.size());
    for (const auto& roi : masterRois) {
        rebased.push_back(makeDerivedHoleRoi(
            roi,
            oldMasterCenter,
            newMasterCenter,
            newMasterTemplateId,
            RoiAdjustment{}));
    }
    return rebased;
}

RoiAdjustment makeRoiAdjustment(const HoleRoi& baseRoi, const HoleRoi& editedRoi)
{
    RoiAdjustment adjustment;
    adjustment.centerDelta.x = editedRoi.center.x - baseRoi.center.x;
    adjustment.centerDelta.y = editedRoi.center.y - baseRoi.center.y;
    adjustment.widthDelta = editedRoi.width - baseRoi.width;
    adjustment.heightDelta = editedRoi.height - baseRoi.height;
    adjustment.angleDelta = normalizeAngleDeg(editedRoi.angleDeg - baseRoi.angleDeg);
    adjustment.paramsOverridden = !sameGaugeParams(baseRoi.params, editedRoi.params);
    adjustment.params = editedRoi.params;
    adjustment.enabled = !nearlyEqual(adjustment.centerDelta.x, 0.0)
        || !nearlyEqual(adjustment.centerDelta.y, 0.0)
        || !nearlyEqual(adjustment.widthDelta, 0.0)
        || !nearlyEqual(adjustment.heightDelta, 0.0)
        || !nearlyEqual(adjustment.angleDelta, 0.0)
        || adjustment.paramsOverridden;
    return adjustment;
}

RoiAdjustment withoutGaugeParamOverride(const RoiAdjustment& adjustment)
{
    RoiAdjustment result = adjustment;
    result.paramsOverridden = false;
    return result;
}

std::vector<RoiAdjustment> makeRoiAdjustmentsForCurrentRois(const std::vector<HoleRoi>& masterRois,
    const ImagePoint& masterCenter, const ImagePoint& targetCenter, int targetTemplateId,
    const std::vector<HoleRoi>& currentRois)
{
    const int count = static_cast<int>(std::min(masterRois.size(), currentRois.size()));
    std::vector<RoiAdjustment> adjustments;
    adjustments.reserve(count);
    for (int i = 0; i < count; ++i) {
        const HoleRoi base = makeDerivedHoleRoi(
            masterRois[i],
            masterCenter,
            targetCenter,
            targetTemplateId,
            RoiAdjustment{});
        adjustments.push_back(makeRoiAdjustment(base, currentRois[i]));
    }
    return adjustments;
}

int selectRoiProfileIndex(const std::vector<RoiProfileRange>& profiles, const std::string& columnLabel)
{
    int column = 0;
    try {
        column = std::stoi(columnLabel);
    }
    catch (...) {
        return profiles.empty() ? 0 : profiles.front().profileIndex;
    }

    int selected = profiles.empty() ? 0 : profiles.front().profileIndex;
    for (const auto& profile : profiles) {
        const int startColumn = std::min(profile.startColumn, profile.endColumn);
        const int endColumn = std::max(profile.startColumn, profile.endColumn);
        if (column >= startColumn && column <= endColumn) {
            selected = profile.profileIndex;
        }
    }
    return selected;
}

int selectRoiProfileIndex(const std::vector<RoiColumnProfile>& profiles, const std::string& columnLabel)
{
    for (const auto& profile : profiles) {
        if (profile.columnLabel == columnLabel) {
            return profile.profileIndex;
        }
    }
    return 0;
}

double lineSpacing(const GaugeLine& a, const GaugeLine& b)
{
    return (pointLineDistance(a.point, b) + pointLineDistance(b.point, a)) * 0.5;
}

double lineAngleDifferenceDeg(double firstAngleDeg, double secondAngleDeg)
{
    double diff = std::fabs(normalizeAngleDeg(firstAngleDeg - secondAngleDeg));
    if (diff > 90.0) {
        diff = 180.0 - diff;
    }
    return diff;
}

double expectedLineAngleDeg(const HoleRoi& roi)
{
    return normalizeAngleDeg(roi.angleDeg + 90.0);
}

int selectLineCandidateIndex(const std::vector<GaugeLine>& candidates, const HoleRoi& roi, double maxAngleDeg)
{
    const double expectedAngle = expectedLineAngleDeg(roi);
    int bestIndex = -1;
    double bestDistance = 0.0;
    double bestScore = 0.0;

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const GaugeLine& line = candidates[i];
        if (!line.ok) {
            continue;
        }
        if (lineAngleDifferenceDeg(line.angleDeg, expectedAngle) > maxAngleDeg) {
            continue;
        }

        const double distance = pointLineDistance(roi.center, line);
        const double score = std::fabs(line.score);
        if (bestIndex < 0
            || distance < bestDistance - 1e-6
            || (nearlyEqual(distance, bestDistance) && score > bestScore)) {
            bestIndex = i;
            bestDistance = distance;
            bestScore = score;
        }
    }
    return bestIndex;
}

GaugeLine selectLineCandidate(const std::vector<GaugeLine>& candidates, const HoleRoi& roi, double maxAngleDeg)
{
    const int index = selectLineCandidateIndex(candidates, roi, maxAngleDeg);
    return index >= 0 ? candidates[index] : GaugeLine{};
}

std::vector<GaugeLine> makeCenterCrossLines(const ImagePoint& center, double radiusPx)
{
    GaugeLine horizontal;
    horizontal.ok = true;
    horizontal.point = center;
    horizontal.angleDeg = 0.0;
    horizontal.start = ImagePoint{ center.x - radiusPx, center.y };
    horizontal.end = ImagePoint{ center.x + radiusPx, center.y };

    GaugeLine vertical;
    vertical.ok = true;
    vertical.point = center;
    vertical.angleDeg = 90.0;
    vertical.start = ImagePoint{ center.x, center.y - radiusPx };
    vertical.end = ImagePoint{ center.x, center.y + radiusPx };

    std::vector<GaugeLine> lines;
    lines.push_back(horizontal);
    lines.push_back(vertical);
    return lines;
}

std::vector<HoleLabel> makeHoleLabels(const std::vector<TemplatePoint>& points,
    const std::vector<std::vector<HoleRoi> >& roiGroups)
{
    std::vector<HoleLabel> labels;
    const int count = static_cast<int>(std::min(points.size(), roiGroups.size()));
    labels.reserve(count);

    for (int i = 0; i < count; ++i) {
        const ImageRect bounds = makeRoiGroupBounds(roiGroups[i], 0.0);
        if (!bounds.ok) {
            continue;
        }
        labels.push_back(HoleLabel{
            HoleLabelKind::Id,
            std::to_string(i + 1),
            ImagePoint{ bounds.left + bounds.width * 0.5, bounds.top + bounds.height * 0.5 }
        });
    }
    return labels;
}

int selectStableProfileMasterIndex(const std::vector<TemplatePoint>& points,
    const std::vector<int>& profileIndexes, int profileIndex, int currentMasterId,
    const std::vector<int>& preferredMasterIds)
{
    if (points.size() != profileIndexes.size()) {
        return -1;
    }

    auto findById = [&](int templateId) {
        if (templateId <= 0) {
            return -1;
        }
        for (int i = 0; i < static_cast<int>(points.size()); ++i) {
            if (profileIndexes[i] == profileIndex && points[i].id == templateId) {
                return i;
            }
        }
        return -1;
    };

    const int currentIndex = findById(currentMasterId);
    if (currentIndex >= 0) {
        return currentIndex;
    }

    for (int templateId : preferredMasterIds) {
        const int preferredIndex = findById(templateId);
        if (preferredIndex >= 0) {
            return preferredIndex;
        }
    }

    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        if (profileIndexes[i] == profileIndex) {
            return i;
        }
    }
    return -1;
}

HoleMeasurement makeMeasurement(const TemplatePoint& point, const ImagePoint& center, const GaugeLine& top,
    const GaugeLine& bottom, const GaugeLine& left, const GaugeLine& right, double micronPerPixel)
{
    HoleMeasurement measurement;
    measurement.templateId = point.id;
    measurement.rowLabel = point.rowLabel;
    measurement.columnLabel = point.columnLabel;
    measurement.templateWorldX = point.worldX;
    measurement.templateWorldY = point.worldY;
    measurement.centerX = center.x;
    measurement.centerY = center.y;
    measurement.xOffset = point.xOffset;
    measurement.yOffset = point.yOffset;
    measurement.top = top;
    measurement.bottom = bottom;
    measurement.left = left;
    measurement.right = right;
    measurement.ok = top.ok && bottom.ok && left.ok && right.ok;
    if (!measurement.ok) {
        setInvalidAlignment(measurement);
        return measurement;
    }

    ImagePoint measuredCenter;
    if (measuredCenterFromLines(top, bottom, left, right, measuredCenter)) {
        measurement.centerX = measuredCenter.x;
        measurement.centerY = measuredCenter.y;
    }
    measurement.heightPx = lineSpacing(top, bottom);
    measurement.widthPx = lineSpacing(left, right);
    measurement.widthMicron = measurement.widthPx * micronPerPixel;
    measurement.heightMicron = measurement.heightPx * micronPerPixel;
    return measurement;
}

bool isRoiMeasurementFailed(const HoleMeasurement& measurement, int roiIndex, bool measurementAvailable)
{
    if (!measurementAvailable) {
        return false;
    }
    switch (roiIndex) {
    case 0: return !measurement.top.ok;
    case 1: return !measurement.bottom.ok;
    case 2: return !measurement.left.ok;
    case 3: return !measurement.right.ok;
    default: return false;
    }
}

std::vector<HoleMeasurement> applyMeasurementOffsets(const std::vector<HoleMeasurement>& measurements)
{
    std::vector<HoleMeasurement> compensated = measurements;
    for (auto& measurement : compensated) {
        measurement.widthMicron += measurement.yOffset;
        measurement.heightMicron += measurement.xOffset;
    }
    return compensated;
}

void saveMeasurementsCsv(const std::string& path, const std::vector<HoleMeasurement>& measurements)
{
    std::ofstream file(path.c_str(), std::ios::trunc);
    file << "ID,Row Label,Column Label,CenterX,CenterY,HeightPx,WidthPx,HeightMicron,WidthMicron,OK,"
         << "MeasuredWorldX,MeasuredWorldY,AlignedWorldX,AlignedWorldY,DeltaX,DeltaY\n";
    for (const auto& item : measurements) {
        file << item.templateId << ','
             << item.rowLabel << ','
             << item.columnLabel << ','
             << item.centerX << ','
             << item.centerY << ','
             << item.widthPx << ','
             << item.heightPx << ','
             << item.widthMicron << ','
             << item.heightMicron << ','
             << (item.ok ? 1 : 0) << ','
             << item.measuredWorldX << ','
             << item.measuredWorldY << ','
             << item.alignedWorldX << ','
             << item.alignedWorldY << ','
             << item.deltaX << ','
             << item.deltaY << '\n';
    }
}

void saveRoisCsv(const std::string& path, const std::vector<HoleRoi>& rois)
{
    std::ofstream file(path.c_str(), std::ios::trunc);
    file << "TemplateID,Side,CenterX,CenterY,Width,Height,Angle,KernelSize,Polarity,AcceptScore,NormScore,SampleWidth,SampleHeight,SampleInterval,MaxSamplePointCount,FitDistThreshold,FitCountThreshold\n";
    for (const auto& roi : rois) {
        file << roi.templateId << ','
             << sideName(roi.side) << ','
             << roi.center.x << ','
             << roi.center.y << ','
             << roi.width << ','
             << roi.height << ','
             << roi.angleDeg << ','
             << roi.params.kernelSize << ','
             << roi.params.polarity << ','
             << roi.params.acceptScore << ','
             << (roi.params.normScore ? 1 : 0) << ','
             << roi.params.sampleRegionWidth << ','
             << roi.params.sampleRegionHeight << ','
             << roi.params.sampleRegionInterval << ','
             << roi.params.maxSamplePointCount << ','
             << roi.params.fitDistThreshold << ','
             << roi.params.fitCountThreshold << '\n';
    }
}

std::vector<HoleRoi> loadRoisCsv(const std::string& path)
{
    std::ifstream file(path.c_str());
    std::vector<HoleRoi> rois;
    std::string line;
    bool header = true;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        if (header) {
            header = false;
            continue;
        }
        const auto fields = splitCsvLine(line);
        if (fields.size() < 17) {
            continue;
        }

        HoleRoi roi;
        roi.templateId = std::stoi(fields[0]);
        roi.side = sideFromName(fields[1]);
        roi.center.x = std::stod(fields[2]);
        roi.center.y = std::stod(fields[3]);
        roi.width = std::stod(fields[4]);
        roi.height = std::stod(fields[5]);
        roi.angleDeg = std::stod(fields[6]);
        roi.params.kernelSize = std::stoi(fields[7]);
        roi.params.polarity = std::stoi(fields[8]);
        roi.params.acceptScore = std::stoi(fields[9]);
        roi.params.normScore = std::stoi(fields[10]) != 0;
        roi.params.sampleRegionWidth = std::stoi(fields[11]);
        roi.params.sampleRegionHeight = std::stoi(fields[12]);
        roi.params.sampleRegionInterval = std::stoi(fields[13]);
        roi.params.maxSamplePointCount = std::stoi(fields[14]);
        roi.params.fitDistThreshold = std::stod(fields[15]);
        roi.params.fitCountThreshold = std::stoi(fields[16]);
        rois.push_back(roi);
    }
    return rois;
}

void saveAppParams(const std::string& path, const AppParams& params)
{
    std::ofstream file(path.c_str(), std::ios::trunc);
    file << "OffsetX=" << params.offsetX << '\n'
         << "OffsetY=" << params.offsetY << '\n'
         << "AngleDeg=" << params.angleDeg << '\n'
         << "MicronPerPixel=" << params.micronPerPixel << '\n'
         << "PointOrder=" << static_cast<int>(params.pointOrder) << '\n'
         << "LineFindMethod=" << static_cast<int>(params.lineFindMethod) << '\n'
         << "EdgeOffsetPx=" << params.gauge.edgeOffsetPx << '\n'
         << "RoiLengthPx=" << params.gauge.roiLengthPx << '\n'
         << "RoiWidthPx=" << params.gauge.roiWidthPx << '\n'
         << "KernelSize=" << params.gauge.kernelSize << '\n'
         << "Polarity=" << params.gauge.polarity << '\n'
         << "AcceptScore=" << params.gauge.acceptScore << '\n'
         << "NormScore=" << (params.gauge.normScore ? 1 : 0) << '\n'
         << "SampleRegionWidth=" << params.gauge.sampleRegionWidth << '\n'
         << "SampleRegionHeight=" << params.gauge.sampleRegionHeight << '\n'
         << "SampleRegionInterval=" << params.gauge.sampleRegionInterval << '\n'
         << "MaxSamplePointCount=" << params.gauge.maxSamplePointCount << '\n'
         << "FitDistThreshold=" << params.gauge.fitDistThreshold << '\n'
         << "FitCountThreshold=" << params.gauge.fitCountThreshold << '\n'
         << "RoiProfileCount=" << params.roiProfiles.size() << '\n';
    for (int i = 0; i < static_cast<int>(params.roiProfiles.size()); ++i) {
        const auto& profile = params.roiProfiles[i];
        file << "RoiProfile" << i << '='
             << profile.profileIndex << ','
             << profile.startColumn << ','
             << profile.endColumn << '\n';
    }
    file << "ColumnProfileCount=" << params.columnProfiles.size() << '\n';
    for (int i = 0; i < static_cast<int>(params.columnProfiles.size()); ++i) {
        const auto& profile = params.columnProfiles[i];
        file << "ColumnProfile" << i << '='
             << profile.columnLabel << ','
             << profile.profileIndex << '\n';
    }
}

bool loadAppParams(const std::string& path, AppParams& params)
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }
        values[line.substr(0, equalPos)] = line.substr(equalPos + 1);
    }

    auto getDouble = [&](const char* key, double fallback) {
        const auto it = values.find(key);
        return it == values.end() ? fallback : std::stod(it->second);
    };
    auto getInt = [&](const char* key, int fallback) {
        const auto it = values.find(key);
        return it == values.end() ? fallback : std::stoi(it->second);
    };

    params.offsetX = getDouble("OffsetX", params.offsetX);
    params.offsetY = getDouble("OffsetY", params.offsetY);
    params.angleDeg = getDouble("AngleDeg", params.angleDeg);
    params.micronPerPixel = getDouble("MicronPerPixel", params.micronPerPixel);
    params.pointOrder = exportOrderFromInt(getInt("PointOrder", static_cast<int>(params.pointOrder)), params.pointOrder);
    params.lineFindMethod = getInt("LineFindMethod", static_cast<int>(params.lineFindMethod)) == 1
        ? LineFindMethod::LineDetector
        : LineFindMethod::LineGauge;
    params.gauge.edgeOffsetPx = getDouble("EdgeOffsetPx", params.gauge.edgeOffsetPx);
    params.gauge.roiLengthPx = getDouble("RoiLengthPx", params.gauge.roiLengthPx);
    params.gauge.roiWidthPx = getDouble("RoiWidthPx", params.gauge.roiWidthPx);
    params.gauge.kernelSize = getInt("KernelSize", params.gauge.kernelSize);
    params.gauge.polarity = getInt("Polarity", params.gauge.polarity);
    params.gauge.acceptScore = getInt("AcceptScore", params.gauge.acceptScore);
    params.gauge.normScore = getInt("NormScore", params.gauge.normScore ? 1 : 0) != 0;
    params.gauge.sampleRegionWidth = getInt("SampleRegionWidth", params.gauge.sampleRegionWidth);
    params.gauge.sampleRegionHeight = getInt("SampleRegionHeight", params.gauge.sampleRegionHeight);
    params.gauge.sampleRegionInterval = getInt("SampleRegionInterval", params.gauge.sampleRegionInterval);
    params.gauge.maxSamplePointCount = getInt("MaxSamplePointCount", params.gauge.maxSamplePointCount);
    params.gauge.fitDistThreshold = getDouble("FitDistThreshold", params.gauge.fitDistThreshold);
    params.gauge.fitCountThreshold = getInt("FitCountThreshold", params.gauge.fitCountThreshold);
    params.gauge.orientationDeg = params.angleDeg;

    const int profileCount = getInt("RoiProfileCount", 0);
    params.roiProfiles.clear();
    for (int i = 0; i < profileCount; ++i) {
        std::ostringstream key;
        key << "RoiProfile" << i;
        const auto it = values.find(key.str());
        if (it == values.end()) {
            continue;
        }
        const auto fields = splitCsvLine(it->second);
        if (fields.size() < 3) {
            continue;
        }
        RoiProfileRange profile;
        profile.profileIndex = std::stoi(fields[0]);
        profile.startColumn = std::stoi(fields[1]);
        profile.endColumn = std::stoi(fields[2]);
        params.roiProfiles.push_back(profile);
    }
    const int columnProfileCount = getInt("ColumnProfileCount", 0);
    params.columnProfiles.clear();
    for (int i = 0; i < columnProfileCount; ++i) {
        std::ostringstream key;
        key << "ColumnProfile" << i;
        const auto it = values.find(key.str());
        if (it == values.end()) {
            continue;
        }
        const auto fields = splitCsvLine(it->second);
        if (fields.size() < 2) {
            continue;
        }
        RoiColumnProfile profile;
        profile.columnLabel = fields[0];
        profile.profileIndex = std::stoi(fields[1]);
        params.columnProfiles.push_back(profile);
    }
    return true;
}

ViewTransform makeViewTransform(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    const ViewState& state)
{
    ViewTransform transform;
    if (imageWidth <= 0 || imageHeight <= 0 || viewWidth <= 0 || viewHeight <= 0) {
        return transform;
    }

    const double fitZoomX = static_cast<double>(viewWidth) / imageWidth;
    const double fitZoomY = static_cast<double>(viewHeight) / imageHeight;
    const double fitZoom = fitZoomX < fitZoomY ? fitZoomX : fitZoomY;
    transform.zoomX = fitZoom * state.scale;
    transform.zoomY = transform.zoomX;
    transform.panX = (viewWidth - imageWidth * transform.zoomX) * 0.5 + state.panX;
    transform.panY = (viewHeight - imageHeight * transform.zoomY) * 0.5 + state.panY;
    return transform;
}

ViewTransform makeViewportViewTransform(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    double viewportX, double viewportY, const ViewState& state)
{
    ViewTransform transform = makeViewTransform(imageWidth, imageHeight, viewWidth, viewHeight, state);
    transform.panX += viewportX;
    transform.panY += viewportY;
    return transform;
}

ViewState zoomViewAtCursor(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    const ViewState& state, double cursorX, double cursorY, double factor, double minScale, double maxScale)
{
    if (imageWidth <= 0 || imageHeight <= 0 || viewWidth <= 0 || viewHeight <= 0) {
        return state;
    }

    ViewState result = state;
    result.scale *= factor;
    if (result.scale < minScale) {
        result.scale = minScale;
    }
    if (result.scale > maxScale) {
        result.scale = maxScale;
    }

    const ViewTransform oldTransform = makeViewTransform(imageWidth, imageHeight, viewWidth, viewHeight, state);
    const ViewTransform newTransform = makeViewTransform(imageWidth, imageHeight, viewWidth, viewHeight, result);
    const double oldZoom = oldTransform.zoomX;
    const double newZoom = newTransform.zoomX;
    if (oldZoom <= 0.0 || newZoom <= 0.0) {
        return result;
    }

    const double imageX = (cursorX - oldTransform.panX) / oldZoom;
    const double imageY = (cursorY - oldTransform.panY) / oldZoom;

    result.panX += cursorX - (imageX * newZoom + newTransform.panX);
    result.panY += cursorY - (imageY * newZoom + newTransform.panY);
    return result;
}

ViewState zoomViewAtViewportCursor(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    double viewportX, double viewportY, const ViewState& state, double cursorX, double cursorY,
    double factor, double minScale, double maxScale)
{
    return zoomViewAtCursor(
        imageWidth,
        imageHeight,
        viewWidth,
        viewHeight,
        state,
        cursorX - viewportX,
        cursorY - viewportY,
        factor,
        minScale,
        maxScale);
}

std::string sideName(RoiSide side)
{
    switch (side) {
    case RoiSide::Top: return "Top";
    case RoiSide::Bottom: return "Bottom";
    case RoiSide::Left: return "Left";
    case RoiSide::Right: return "Right";
    }
    return "";
}

RoiSide sideFromName(const std::string& name)
{
    if (name == "Bottom") return RoiSide::Bottom;
    if (name == "Left") return RoiSide::Left;
    if (name == "Right") return RoiSide::Right;
    return RoiSide::Top;
}
}
