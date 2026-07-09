#pragma once

#include "HoleMeasureCore.h"

#include <QPoint>
#include <QWidget>
#include <QVector>

#include "LPVCore.h"
#include "LPVCalib.h"
#include "LPVGauge.h"
#include "LPVGeom.h"
#include "LPVLocate.h"
#include "LPVDisplayControl.h"

#include <map>
#include <utility>

class QLabel;
class LPVDisplayWidget;
class QTableWidget;
class QTableWidgetItem;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QLineEdit;

class QtHoleMeasureWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QtHoleMeasureWidget(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void loadDefaults();
    void rebuildArray();
    void measureAll();
    void saveResults();
    void saveParams();
    void saveRoiConfig();
    void loadRoiConfig();
    void selectedHoleChanged();
    void selectedRoiChanged();
    void applyRoiEdits();
    void applyGaugeDefaults();
    void gaugeDefaultsChanged();
    void selectedProfileChanged();
    void selectedProfileGroupChanged();
    void columnProfileItemChanged(QTableWidgetItem* item);
    void searchHoleById();
    void displayRegionDragFinished(int regionId);

private:
    struct HoleState
    {
        hm::TemplatePoint point;
        hm::ImagePoint imageCenter;
        hm::ImagePoint shiftedCenter;
        int roiProfileIndex = 0;
        std::vector<hm::HoleRoi> rois;
        std::vector<hm::RoiAdjustment> roiAdjustments;
        hm::HoleMeasurement measurement;
    };

    struct RoiProfileState
    {
        hm::RoiProfileRange range;
        int masterTemplateId = 0;
        hm::ImagePoint masterCenter;
        std::vector<hm::HoleRoi> masterRois;
    };

    void buildUi();
    void initializeRoiProfiles();
    void log(const QString& text);
    void displayImage();
    void populateHoleTable();
    void populateRoiTable(int holeIndex);
    void populateColumnProfileTable();
    void populateProfileList();
    void syncRoiEditor(const hm::HoleRoi& roi);
    hm::GaugeDefaults readDefaults() const;
    hm::ArrayOffset readOffset() const;
    bool loadDefaultsFromDirectory(const QString& base);
    bool loadCalibration(const QString& path);
    bool loadImage(const QString& path);
    bool fixImage();
    bool mapTemplateToImage();
    hm::GaugeLine detectLine(const hm::HoleRoi& roi);
    hm::GaugeLine detectLineByGauge(const hm::HoleRoi& roi, const LPVGeomLib::ILRotRectRegionPtr& region);
    hm::GaugeLine detectLineByDetector(const hm::HoleRoi& roi, const LPVGeomLib::ILRotRectRegionPtr& region);
    bool applyTemplateAlignmentWithLpv();
    void updateMeasurementTable();
    QString filesDir() const;
    QString roiConfigPath() const;
    QString paramsPath() const;
    QString existingDataFilePath(const QString& fileName) const;
    hm::AppParams readAppParams() const;
    void applyAppParams(const hm::AppParams& params);
    bool loadSavedParams();
    bool loadSavedRoiConfig();
    void resetView();
    bool getViewTransform(double& zoomx, double& zoomy, double& panx, double& pany) const;
    LPVCoreLib::ILRotRectRegionPtr makeRegion(const hm::HoleRoi& roi) const;
    hm::HoleRoi* selectedRoi();
    int selectedProfileIndex() const;
    hm::ExportOrder currentExportOrder() const;
    std::string profileGroupLabelForHole(const HoleState& hole) const;
    std::string selectedProfileGroupLabel() const;
    int columnProfileIndex(const std::string& columnLabel) const;
    void setColumnProfileIndex(const std::string& columnLabel, int profileIndex);
    int profileIndexForHole(const HoleState& hole) const;
    int firstHoleIndexForProfile(int profileIndex) const;
    void selectFirstHoleInProfile(int profileIndex);
    void ensureProfileMasters(const std::vector<int>& preferredMasterIds = std::vector<int>());
    hm::HoleRoi baseRoiForHole(int holeIndex, int roiIndex) const;
    void ensureRoiAdjustments();
    void rebuildRoisFromMaster();
    void applyEditedRoi(int holeIndex, int roiIndex, const hm::HoleRoi& editedRoi);
    void applyGaugeParamsToAllRois();
    void updateSelectedRoiTableRow();
    std::vector<hm::HoleRoi> collectRois() const;
    void applyLoadedRois(const std::vector<hm::HoleRoi>& rois);
    void clearMeasurements();

    LPVDisplayWidget* m_display = nullptr;
    QTableWidget* m_holeTable = nullptr;
    QTableWidget* m_roiTable = nullptr;
    QTableWidget* m_resultTable = nullptr;
    QLabel* m_status = nullptr;
    QDoubleSpinBox* m_dx = nullptr;
    QDoubleSpinBox* m_dy = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QDoubleSpinBox* m_edgeOffset = nullptr;
    QDoubleSpinBox* m_roiLength = nullptr;
    QDoubleSpinBox* m_roiWidth = nullptr;
    QDoubleSpinBox* m_micronPerPixel = nullptr;
    QSpinBox* m_sampleWidth = nullptr;
    QSpinBox* m_sampleHeight = nullptr;
    QSpinBox* m_sampleInterval = nullptr;
    QSpinBox* m_acceptScore = nullptr;
    QComboBox* m_defaultPolarity = nullptr;
    QComboBox* m_roiPolarity = nullptr;
    QComboBox* m_orderMajor = nullptr;
    QComboBox* m_startCorner = nullptr;
    QComboBox* m_lineFindMethod = nullptr;
    QLineEdit* m_searchId = nullptr;
    QTableWidget* m_columnProfileTable = nullptr;
    QTableWidget* m_profileList = nullptr;

    LPVCoreLib::ILImagePtr m_rawImage;
    LPVCoreLib::ILImagePtr m_fixedImage;
    LPVCalibLib::ILCalibPtr m_calib;
    LPVGaugeLib::ILLineGaugePtr m_lineGauge;
    LPVLocateLib::ILLineDetectorPtr m_lineDetector;
    ILDisplayPtr m_displayCtrl;

    std::vector<hm::TemplatePoint> m_templatePoints;
    std::vector<hm::ImagePoint> m_imagePoints;
    std::vector<HoleState> m_holes;
    std::vector<RoiProfileState> m_roiProfiles;
    std::vector<hm::RoiColumnProfile> m_columnProfiles;
    std::vector<hm::HoleMeasurement> m_measurements;
    QString m_defaultDataDir = "D:/files";
    QString m_dataDir = "D:/files";
    bool m_displayImageSet = false;

    double m_viewScale = 1.0;
    double m_viewPanX = 0.0;
    double m_viewPanY = 0.0;
    bool m_isPanning = false;
    QPoint m_lastPanPos;
    bool m_isDraggingRoi = false;
    int m_dragHoleIndex = -1;
    int m_dragRoiIndex = -1;
    LPVRoiHandle m_dragHandle = LPVRoiHandleNone;
    std::map<int, std::pair<int, int> > m_displayRegionMap;
};
