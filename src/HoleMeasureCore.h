#pragma once

#include <string>
#include <vector>

namespace hm
{
static const double InvalidMeasurementValue = -9999.0;

struct ImagePoint
{
    double x = 0.0;
    double y = 0.0;
};

struct ImageRect
{
    double left = 0.0;
    double top = 0.0;
    double width = 0.0;
    double height = 0.0;
    bool ok = false;
};

struct TemplatePoint
{
    int id = 0;
    std::string rowLabel;
    std::string columnLabel;
    double worldX = 0.0;
    double worldY = 0.0;
    int groupId = 0;
    bool enabled = false;
    int pinDirection = 0;
    double xOffset = 0.0;
    double yOffset = 0.0;
};

enum class ExportOrder
{
    FirstColumnTopDown,
    LastColumnTopDown
};

enum class LineFindMethod
{
    LineGauge,
    LineDetector
};

enum class RoiSide
{
    Top,
    Bottom,
    Left,
    Right
};

struct ArrayOffset
{
    double dx = 0.0;
    double dy = 0.0;
    double angleDeg = 0.0;
};

struct GaugeParams
{
    int kernelSize = 3;
    int polarity = 0;
    int acceptScore = 40;
    bool normScore = false;
    int sampleRegionWidth = 8;
    int sampleRegionHeight = 24;
    int sampleRegionInterval = 8;
    int maxSamplePointCount = 1;
    double fitDistThreshold = 3.0;
    int fitCountThreshold = 4;
};

struct GaugeDefaults : GaugeParams
{
    double edgeOffsetPx = 30.0;
    double roiLengthPx = 80.0;
    double roiWidthPx = 18.0;
    double orientationDeg = 0.0;
};

struct RoiProfileRange
{
    int profileIndex = 0;
    int startColumn = 1;
    int endColumn = 999;
};

struct RoiColumnProfile
{
    std::string columnLabel;
    int profileIndex = 0;
};

struct AppParams
{
    double offsetX = 0.0;
    double offsetY = 0.0;
    double angleDeg = 0.0;
    double micronPerPixel = 1.0;
    ExportOrder pointOrder = ExportOrder::FirstColumnTopDown;
    LineFindMethod lineFindMethod = LineFindMethod::LineGauge;
    GaugeDefaults gauge;
    std::vector<RoiProfileRange> roiProfiles;
    std::vector<RoiColumnProfile> columnProfiles;
};

struct ViewState
{
    double scale = 1.0;
    double panX = 0.0;
    double panY = 0.0;
};

struct ViewTransform
{
    double zoomX = 0.0;
    double zoomY = 0.0;
    double panX = 0.0;
    double panY = 0.0;
};

struct HoleRoi
{
    int templateId = 0;
    RoiSide side = RoiSide::Top;
    ImagePoint center;
    double width = 0.0;
    double height = 0.0;
    double angleDeg = 0.0;
    GaugeParams params;
};

struct RoiAdjustment
{
    bool enabled = false;
    ImagePoint centerDelta;
    double widthDelta = 0.0;
    double heightDelta = 0.0;
    double angleDelta = 0.0;
    bool paramsOverridden = false;
    GaugeParams params;
};

struct GaugeLine
{
    bool ok = false;
    ImagePoint point;
    double angleDeg = 0.0;
    double score = 0.0;
    ImagePoint start;
    ImagePoint end;
};

struct HoleMeasurement
{
    int templateId = 0;
    std::string rowLabel;
    std::string columnLabel;
    double centerX = 0.0;
    double centerY = 0.0;
    double widthPx = 0.0;
    double heightPx = 0.0;
    double widthMicron = 0.0;
    double heightMicron = 0.0;
    double xOffset = 0.0;
    double yOffset = 0.0;
    double templateWorldX = 0.0;
    double templateWorldY = 0.0;
    double measuredWorldX = InvalidMeasurementValue;
    double measuredWorldY = InvalidMeasurementValue;
    double alignedWorldX = InvalidMeasurementValue;
    double alignedWorldY = InvalidMeasurementValue;
    double deltaX = InvalidMeasurementValue;
    double deltaY = InvalidMeasurementValue;
    bool ok = false;
    GaugeLine top;
    GaugeLine bottom;
    GaugeLine left;
    GaugeLine right;
};

std::vector<TemplatePoint> loadTemplateCsv(const std::string& path);
std::vector<TemplatePoint> sortForExport(const std::vector<TemplatePoint>& points, ExportOrder order);
std::vector<std::string> sortColumnLabelsForOrder(const std::vector<std::string>& columns, ExportOrder order);
void sortTemplateImagePairs(std::vector<TemplatePoint>& points, std::vector<ImagePoint>& imagePoints, ExportOrder order);
std::vector<ImagePoint> applyArrayOffset(const std::vector<ImagePoint>& points, const ArrayOffset& offset);
std::vector<HoleRoi> makeDefaultHoleRois(const ImagePoint& center, int templateId, const GaugeDefaults& defaults);
ImageRect makeRoiGroupBounds(const std::vector<HoleRoi>& rois, double paddingPx);
HoleRoi makeDerivedHoleRoi(const HoleRoi& masterRoi, const ImagePoint& masterCenter,
    const ImagePoint& targetCenter, int targetTemplateId, const RoiAdjustment& adjustment);
std::vector<HoleRoi> rebaseMasterRois(const std::vector<HoleRoi>& masterRois,
    const ImagePoint& oldMasterCenter, const ImagePoint& newMasterCenter, int newMasterTemplateId);
RoiAdjustment makeRoiAdjustment(const HoleRoi& baseRoi, const HoleRoi& editedRoi);
RoiAdjustment withoutGaugeParamOverride(const RoiAdjustment& adjustment);
int selectRoiProfileIndex(const std::vector<RoiProfileRange>& profiles, const std::string& columnLabel);
int selectRoiProfileIndex(const std::vector<RoiColumnProfile>& profiles, const std::string& columnLabel);
std::vector<GaugeLine> makeCenterCrossLines(const ImagePoint& center, double radiusPx);
HoleMeasurement makeMeasurement(const TemplatePoint& point, const ImagePoint& center, const GaugeLine& top,
    const GaugeLine& bottom, const GaugeLine& left, const GaugeLine& right, double micronPerPixel);
bool isRoiMeasurementFailed(const HoleMeasurement& measurement, int roiIndex, bool measurementAvailable);
std::vector<HoleMeasurement> applyMeasurementOffsets(const std::vector<HoleMeasurement>& measurements);
double lineAngleDifferenceDeg(double firstAngleDeg, double secondAngleDeg);
double expectedLineAngleDeg(const HoleRoi& roi);
int selectLineCandidateIndex(const std::vector<GaugeLine>& candidates, const HoleRoi& roi, double maxAngleDeg);
GaugeLine selectLineCandidate(const std::vector<GaugeLine>& candidates, const HoleRoi& roi, double maxAngleDeg);
void saveMeasurementsCsv(const std::string& path, const std::vector<HoleMeasurement>& measurements);
void saveRoisCsv(const std::string& path, const std::vector<HoleRoi>& rois);
std::vector<HoleRoi> loadRoisCsv(const std::string& path);
void saveAppParams(const std::string& path, const AppParams& params);
bool loadAppParams(const std::string& path, AppParams& params);
ViewState zoomViewAtCursor(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    const ViewState& state, double cursorX, double cursorY, double factor, double minScale, double maxScale);
ViewState zoomViewAtViewportCursor(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    double viewportX, double viewportY, const ViewState& state, double cursorX, double cursorY,
    double factor, double minScale, double maxScale);
ViewTransform makeViewTransform(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    const ViewState& state);
ViewTransform makeViewportViewTransform(int imageWidth, int imageHeight, int viewWidth, int viewHeight,
    double viewportX, double viewportY, const ViewState& state);
std::string sideName(RoiSide side);
RoiSide sideFromName(const std::string& name);
}
