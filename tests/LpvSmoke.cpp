#include "../src/HoleMeasureCore.h"

#include "LPVCore.h"
#include "LPVCalib.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace LPVCoreLib;
using namespace LPVCalibLib;

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

ILCalibPtr loadAnyCalib(const std::wstring& path)
{
    std::vector<ILCalibPtr> candidates;
    candidates.push_back(LCalibFFD::Create());
    candidates.push_back(LCalibPinHole::Create());
    candidates.push_back(LCalibNPoints::Create());
    candidates.push_back(LCalibCustom::Create());

    for (auto& candidate : candidates) {
        LPVErrorCode err = candidate->Load(path);
        if (err == LPVNoError && candidate->IsCalibrated()) {
            return candidate;
        }
    }
    return nullptr;
}
}

int main()
{
    const std::string base = "C:/Users/22113/Desktop/files";
    ILCalibPtr calib = loadAnyCalib(L"C:/Users/22113/Desktop/files/distCharucoBoard_S_I.calib");
    require(calib && calib->IsCalibrated(), "calibration load");

    ILImagePtr raw = LImage::Create();
    require(raw->Load(L"C:/Users/22113/Desktop/files/400P65H-image.bmp") == LPVNoError && raw->Valid(), "image load");

    ILImagePtr fixed = LImage::Create();
    calib->FixImageMode = LPVFixImageUndistort;
    require(calib->FixImage(raw, fixed) == LPVNoError && fixed->Valid(), "fix image");

    const auto points = hm::loadTemplateCsv(base + "/400P_6.5H_BALL_Template.csv");
    require(points.size() == 400, "template point count");

    double x = 0.0;
    double y = 0.0;
    calib->WorldToImage(points.front().worldX, points.front().worldY, &x, &y);
    require(x > -100000.0 && x < 100000.0 && y > -100000.0 && y < 100000.0, "first point maps to finite image range");

    std::cout << "LpvSmoke passed: fixed " << fixed->Width << "x" << fixed->Height
              << ", first point (" << x << ", " << y << ")" << std::endl;
    return 0;
}
