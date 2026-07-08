#include "QtHoleMeasureWidget.h"

#include "LPVDisplayWidget.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWheelEvent>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

using namespace LPVCoreLib;
using namespace LPVCalibLib;
using namespace LPVGaugeLib;
using namespace LPVGeomLib;
using namespace LPVLocateLib;

namespace
{
QString qstr(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

QTableWidgetItem* item(const QString& text)
{
    auto* tableItem = new QTableWidgetItem(text);
    tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
    return tableItem;
}

QTableWidgetItem* checkItem(bool checked)
{
    auto* tableItem = new QTableWidgetItem;
    tableItem->setFlags((tableItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    tableItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return tableItem;
}

int columnNumber(const std::string& label)
{
    try {
        return std::stoi(label);
    }
    catch (...) {
        return 0;
    }
}

QDoubleSpinBox* doubleSpin(double min, double max, double value, int decimals = 2)
{
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSingleStep(1.0);
    return spin;
}

bool hasValidWorldPoint(const hm::HoleMeasurement& measurement)
{
    return measurement.ok
        && measurement.measuredWorldX != hm::InvalidMeasurementValue
        && measurement.measuredWorldY != hm::InvalidMeasurementValue;
}

void invalidateAlignment(hm::HoleMeasurement& measurement)
{
    measurement.measuredWorldX = hm::InvalidMeasurementValue;
    measurement.measuredWorldY = hm::InvalidMeasurementValue;
    measurement.alignedWorldX = hm::InvalidMeasurementValue;
    measurement.alignedWorldY = hm::InvalidMeasurementValue;
    measurement.deltaX = hm::InvalidMeasurementValue;
    measurement.deltaY = hm::InvalidMeasurementValue;
}

QSpinBox* intSpin(int min, int max, int value)
{
    auto* spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}

QString polarityName(int value)
{
    switch (value) {
    case 1: return "White2Black";
    case 2: return "EitherEdge";
    case 3: return "BlackOnWhite";
    case 4: return "WhiteOnBlack";
    case 5: return "EitherObject";
    case 6: return "Either";
    case 0:
    default: return "Black2White";
    }
}

int polarityValue(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == "1" || trimmed.compare("White2Black", Qt::CaseInsensitive) == 0) return 1;
    if (trimmed == "2" || trimmed.compare("EitherEdge", Qt::CaseInsensitive) == 0) return 2;
    if (trimmed == "3" || trimmed.compare("BlackOnWhite", Qt::CaseInsensitive) == 0) return 3;
    if (trimmed == "4" || trimmed.compare("WhiteOnBlack", Qt::CaseInsensitive) == 0) return 4;
    if (trimmed == "5" || trimmed.compare("EitherObject", Qt::CaseInsensitive) == 0) return 5;
    if (trimmed == "6" || trimmed.compare("Either", Qt::CaseInsensitive) == 0) return 6;
    return 0;
}

QComboBox* polarityCombo()
{
    auto* combo = new QComboBox;
    for (int i = 0; i <= 6; ++i) {
        combo->addItem(polarityName(i), i);
    }
    return combo;
}

int polarityComboValue(const QComboBox* combo)
{
    return combo ? combo->currentData().toInt() : 0;
}

void setPolarityComboValue(QComboBox* combo, int value)
{
    if (!combo) {
        return;
    }
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

hm::GaugeLine makeGaugeLineFromLpvLine(const ILLinePtr& detected, double score)
{
    hm::GaugeLine line;
    if (!detected || !detected->Valid()) {
        return line;
    }

    ILPointPtr mid = detected->GetMidPoint();
    ILPointPtr start = detected->GetStartPoint();
    ILPointPtr end = detected->GetEndPoint();
    line.ok = true;
    line.point = hm::ImagePoint{ mid->X, mid->Y };
    line.angleDeg = detected->GetAngle();
    line.score = score;
    line.start = hm::ImagePoint{ start->X, start->Y };
    line.end = hm::ImagePoint{ end->X, end->Y };
    return line;
}
}

QtHoleMeasureWidget::QtHoleMeasureWidget(QWidget* parent)
    : QWidget(parent)
{
    m_rawImage = LImage::Create();
    m_fixedImage = LImage::Create();
    m_lineGauge = LLineGauge::Create();
    m_lineDetector = LLineDetector::Create();

    buildUi();
    initializeRoiProfiles();
    resize(1500, 900);
    if (loadDefaultsFromDirectory(filesDir())) {
        if (loadSavedParams()) {
            rebuildArray();
        }
        loadSavedRoiConfig();
    }
}

QString QtHoleMeasureWidget::filesDir() const
{
    return m_dataDir;
}

void QtHoleMeasureWidget::buildUi()
{
    setWindowTitle("QtHoleMeasure - LPV Gauge Demo");

    m_display = new LPVDisplayWidget;
    if (!m_display->createDisplayControl()) {
        log(QStringLiteral("LPV display control create failed: %1").arg(m_display->lastError()));
    }
    m_display->setMinimumSize(820, 620);
    m_displayCtrl = m_display->displayControl();
    if (m_displayCtrl) {
        m_displayCtrl->DisplayFlags = LPVDisplayDefault;
        m_displayCtrl->ZoomAnchor = LPVAnchorUnderMouse;
        m_displayCtrl->KeepAspectRatio = VARIANT_TRUE;
    }
    connect(m_display, SIGNAL(RegionDragFinished(int)), this, SLOT(displayRegionDragFinished(int)));

    auto* loadButton = new QPushButton("Load File");
    auto* rebuildButton = new QPushButton("Apply Offset");
    auto* measureButton = new QPushButton("Measure All");
    auto* saveButton = new QPushButton("Save CSV");
    auto* saveParamsButton = new QPushButton("Save Params");
    auto* saveRoiButton = new QPushButton("Save ROI");
    auto* loadRoiButton = new QPushButton("Load ROI");
    auto* searchButton = new QPushButton("Search");

    connect(loadButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::loadDefaults);
    connect(rebuildButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::rebuildArray);
    connect(measureButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::measureAll);
    connect(saveButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::saveResults);
    connect(saveParamsButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::saveParams);
    connect(saveRoiButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::saveRoiConfig);
    connect(loadRoiButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::loadRoiConfig);
    connect(searchButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::searchHoleById);

    m_dx = doubleSpin(-20000, 20000, -2628.00);
    m_dy = doubleSpin(-20000, 20000, 221.00);
    m_angle = doubleSpin(-180, 180, 90.5, 3);
    connect(m_dx, SIGNAL(valueChanged(double)), this, SLOT(rebuildArray()));
    connect(m_dy, SIGNAL(valueChanged(double)), this, SLOT(rebuildArray()));
    connect(m_angle, SIGNAL(valueChanged(double)), this, SLOT(rebuildArray()));

    m_searchId = new QLineEdit;
    m_searchId->setPlaceholderText("ID");
    connect(m_searchId, &QLineEdit::returnPressed, this, &QtHoleMeasureWidget::searchHoleById);

    m_edgeOffset = doubleSpin(1, 500, 30);
    m_roiLength = doubleSpin(1, 800, 38);
    m_roiWidth = doubleSpin(1, 400, 80);
    m_micronPerPixel = doubleSpin(0.0001, 10000, 1.0, 6);
    m_sampleWidth = intSpin(1, 500, 8);
    m_sampleHeight = intSpin(1, 500, 24);
    m_sampleInterval = intSpin(1, 500, 8);
    m_acceptScore = intSpin(1, 100, 40);
    m_defaultPolarity = polarityCombo();
    connect(m_sampleWidth, SIGNAL(valueChanged(int)), this, SLOT(gaugeDefaultsChanged()));
    connect(m_sampleHeight, SIGNAL(valueChanged(int)), this, SLOT(gaugeDefaultsChanged()));
    connect(m_sampleInterval, SIGNAL(valueChanged(int)), this, SLOT(gaugeDefaultsChanged()));
    connect(m_acceptScore, SIGNAL(valueChanged(int)), this, SLOT(gaugeDefaultsChanged()));
    connect(m_defaultPolarity, SIGNAL(currentIndexChanged(int)), this, SLOT(gaugeDefaultsChanged()));
    m_roiPolarity = polarityCombo();

    m_sortOrder = new QComboBox;
    m_sortOrder->addItem("First column top-down");
    m_sortOrder->addItem("Last column top-down");
    connect(m_sortOrder, SIGNAL(currentIndexChanged(int)), this, SLOT(rebuildArray()));

    m_lineFindMethod = new QComboBox;
    m_lineFindMethod->addItem("LineGauge");
    m_lineFindMethod->addItem("LineDetector");
    connect(m_lineFindMethod, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int) {
            clearMeasurements();
            displayImage();
        });

    m_profileList = new QTableWidget(0, 3);
    m_profileList->setHorizontalHeaderLabels(QStringList() << "Profile" << "Columns" << "Master ID");
    m_profileList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_profileList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_profileList, &QTableWidget::itemSelectionChanged, this, &QtHoleMeasureWidget::selectedProfileGroupChanged);

    m_columnProfileTable = new QTableWidget(0, 5);
    m_columnProfileTable->setHorizontalHeaderLabels(QStringList() << "Col" << "P1" << "P2" << "P3" << "P4");
    m_columnProfileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_columnProfileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_columnProfileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_columnProfileTable, &QTableWidget::itemChanged, this, &QtHoleMeasureWidget::columnProfileItemChanged);

    auto* controls = new QGroupBox("Controls");
    auto* form = new QFormLayout(controls);
    form->addRow(loadButton, rebuildButton);
    form->addRow(measureButton, saveButton);
    form->addRow(saveParamsButton);
    form->addRow(loadRoiButton, saveRoiButton);
    form->addRow("Search ID", m_searchId);
    form->addRow(searchButton);
    form->addRow("Offset X", m_dx);
    form->addRow("Offset Y", m_dy);
    form->addRow("Angle", m_angle);
    form->addRow("Edge offset px", m_edgeOffset);
    form->addRow("ROI length px", m_roiLength);
    form->addRow("ROI width px", m_roiWidth);
    form->addRow("Micron / px", m_micronPerPixel);
    form->addRow("Sample width", m_sampleWidth);
    form->addRow("Sample height", m_sampleHeight);
    form->addRow("Sample interval", m_sampleInterval);
    form->addRow("Accept score", m_acceptScore);
    form->addRow("Polarity", m_defaultPolarity);
    form->addRow("Point order", m_sortOrder);
    form->addRow("Line find", m_lineFindMethod);

    m_holeTable = new QTableWidget(0, 6);
    m_holeTable->setHorizontalHeaderLabels(QStringList() << "ID" << "Row" << "Col" << "Profile" << "X" << "Y");
    m_holeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_holeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_holeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_holeTable, &QTableWidget::itemSelectionChanged, this, &QtHoleMeasureWidget::selectedHoleChanged);

    m_roiTable = new QTableWidget(0, 11);
    m_roiTable->setHorizontalHeaderLabels(QStringList() << "Side" << "X" << "Y" << "W" << "H" << "Angle"
        << "Sample W" << "Sample H" << "Interval" << "Score" << "Polarity");
    m_roiTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_roiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_roiTable, &QTableWidget::itemSelectionChanged, this, &QtHoleMeasureWidget::selectedRoiChanged);

    auto* roiBox = new QGroupBox("Selected ROI");
    auto* roiForm = new QFormLayout(roiBox);
    auto* applyRoiButton = new QPushButton("Apply ROI");
    connect(applyRoiButton, &QPushButton::clicked, this, &QtHoleMeasureWidget::applyRoiEdits);
    roiForm->addRow(m_roiTable);
    roiForm->addRow("Polarity", m_roiPolarity);
    roiForm->addRow(applyRoiButton);

    m_resultTable = new QTableWidget(0, 7);
    m_resultTable->setHorizontalHeaderLabels(QStringList() << "ID" << "Row" << "Col" << "Height px" << "Width px" << "Height um" << "Width um");
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_status = new QLabel;
    m_status->setWordWrap(true);

    auto* sideLayout = new QVBoxLayout;
    sideLayout->addWidget(controls);
    sideLayout->addWidget(new QLabel("Profiles"));
    sideLayout->addWidget(m_profileList, 1);
    sideLayout->addWidget(new QLabel("Column Profiles"));
    sideLayout->addWidget(m_columnProfileTable, 2);
    sideLayout->addWidget(new QLabel("Holes"));
    sideLayout->addWidget(m_holeTable, 2);
    sideLayout->addWidget(roiBox, 1);
    sideLayout->addWidget(new QLabel("Results"));
    sideLayout->addWidget(m_resultTable, 2);
    sideLayout->addWidget(m_status);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_display, 3);
    mainLayout->addLayout(sideLayout, 2);
}

void QtHoleMeasureWidget::initializeRoiProfiles()
{
    m_roiProfiles.clear();
    m_roiProfiles.resize(4);
    for (int i = 0; i < static_cast<int>(m_roiProfiles.size()); ++i) {
        m_roiProfiles[i].range.profileIndex = i;
        m_roiProfiles[i].range.startColumn = 0;
        m_roiProfiles[i].range.endColumn = 0;
    }
    populateProfileList();
}

bool QtHoleMeasureWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_display) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        const double factor = wheel->angleDelta().y() > 0 ? 1.20 : 1.0 / 1.10;
        const QPoint cursor = wheel->pos();
        const QRect rect = m_display->contentsRect();
        const hm::ViewState view = hm::zoomViewAtViewportCursor(
            m_fixedImage ? m_fixedImage->Width : 0,
            m_fixedImage ? m_fixedImage->Height : 0,
            rect.width(),
            rect.height(),
            rect.left(),
            rect.top(),
            hm::ViewState{ m_viewScale, m_viewPanX, m_viewPanY },
            cursor.x(),
            cursor.y(),
            factor,
            0.05,
            80.0);
        m_viewScale = view.scale;
        m_viewPanX = view.panX;
        m_viewPanY = view.panY;
        displayImage();
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            double zoomx = 1.0;
            double zoomy = 1.0;
            double panx = 0.0;
            double pany = 0.0;
            const int selectedHoleIndex = m_holeTable->currentRow();
            if (getViewTransform(zoomx, zoomy, panx, pany)
                && selectedHoleIndex >= 0
                && selectedHoleIndex < static_cast<int>(m_holes.size())) {
                for (int r = 0; r < static_cast<int>(m_holes[selectedHoleIndex].rois.size()); ++r) {
                    ILRotRectRegionPtr region = makeRegion(m_holes[selectedHoleIndex].rois[r]);
                    const LPVRoiHandle hit = region->HitTest(mouse->pos().x(), mouse->pos().y(), zoomx, zoomy, panx, pany);
                    if (hit != LPVRoiHandleNone) {
                        m_roiTable->selectRow(r);
                        m_isDraggingRoi = true;
                        m_dragHoleIndex = selectedHoleIndex;
                        m_dragRoiIndex = r;
                        m_dragHandle = hit;
                        m_display->setCursor(Qt::SizeAllCursor);
                        event->accept();
                        return true;
                    }
                }
            }

            m_isPanning = true;
            m_lastPanPos = mouse->pos();
            m_display->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && m_isDraggingRoi) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (m_dragHoleIndex >= 0
            && m_dragHoleIndex < static_cast<int>(m_holes.size())
            && m_dragRoiIndex >= 0
            && m_dragRoiIndex < static_cast<int>(m_holes[m_dragHoleIndex].rois.size())) {
            double zoomx = 1.0;
            double zoomy = 1.0;
            double panx = 0.0;
            double pany = 0.0;
            if (getViewTransform(zoomx, zoomy, panx, pany)) {
                hm::HoleRoi edited = m_holes[m_dragHoleIndex].rois[m_dragRoiIndex];
                ILRotRectRegionPtr region = makeRegion(edited);
                region->Drag(m_dragHandle, mouse->pos().x(), mouse->pos().y(), zoomx, zoomy, panx, pany);
                edited.center.x = region->CenterX;
                edited.center.y = region->CenterY;
                edited.width = region->Width;
                edited.height = region->Height;
                edited.angleDeg = region->Angle;
                applyEditedRoi(m_dragHoleIndex, m_dragRoiIndex, edited);
                clearMeasurements();
                updateSelectedRoiTableRow();
                displayImage();
            }
        }
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseMove && m_isPanning) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint delta = mouse->pos() - m_lastPanPos;
        m_lastPanPos = mouse->pos();
        m_viewPanX += delta.x();
        m_viewPanY += delta.y();
        displayImage();
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_isPanning = false;
            m_isDraggingRoi = false;
            m_dragHoleIndex = -1;
            m_dragRoiIndex = -1;
            m_dragHandle = LPVRoiHandleNone;
            m_display->setCursor(Qt::ArrowCursor);
            event->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        resetView();
        event->accept();
        return true;
    }

    if (event->type() == QEvent::Resize) {
        displayImage();
        return QWidget::eventFilter(watched, event);
    }

    return QWidget::eventFilter(watched, event);
}

void QtHoleMeasureWidget::resetView()
{
    m_viewScale = 1.0;
    m_viewPanX = 0.0;
    m_viewPanY = 0.0;
    if (m_displayCtrl) {
        m_displayCtrl->FitToWindow(LPVAlignDefault);
    }
}

bool QtHoleMeasureWidget::getViewTransform(double& zoomx, double& zoomy, double& panx, double& pany) const
{
    if (!m_displayCtrl) {
        return false;
    }

    zoomx = m_displayCtrl->ZoomX;
    zoomy = m_displayCtrl->ZoomY;
    panx = m_displayCtrl->PanX;
    pany = m_displayCtrl->PanY;
    return true;
}

ILRotRectRegionPtr QtHoleMeasureWidget::makeRegion(const hm::HoleRoi& roi) const
{
    ILRotRectRegionPtr region = LRotRectRegion::Create();
    region->SetPlacement(roi.center.x, roi.center.y, roi.width, roi.height, roi.angleDeg);
    return region;
}

hm::HoleRoi* QtHoleMeasureWidget::selectedRoi()
{
    const int holeIndex = m_holeTable->currentRow();
    const int roiIndex = m_roiTable->currentRow();
    if (holeIndex < 0 || holeIndex >= static_cast<int>(m_holes.size())) {
        return nullptr;
    }
    if (roiIndex < 0 || roiIndex >= static_cast<int>(m_holes[holeIndex].rois.size())) {
        return nullptr;
    }
    return &m_holes[holeIndex].rois[roiIndex];
}

int QtHoleMeasureWidget::selectedProfileIndex() const
{
    const int row = m_profileList ? m_profileList->currentRow() : 0;
    if (row >= 0 && row < static_cast<int>(m_roiProfiles.size())) {
        return row;
    }
    return 0;
}

int QtHoleMeasureWidget::columnProfileIndex(const std::string& columnLabel) const
{
    const int index = hm::selectRoiProfileIndex(m_columnProfiles, columnLabel);
    return index >= 0 && index < static_cast<int>(m_roiProfiles.size()) ? index : 0;
}

void QtHoleMeasureWidget::setColumnProfileIndex(const std::string& columnLabel, int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= static_cast<int>(m_roiProfiles.size())) {
        profileIndex = 0;
    }
    for (auto& profile : m_columnProfiles) {
        if (profile.columnLabel == columnLabel) {
            profile.profileIndex = profileIndex;
            return;
        }
    }
    m_columnProfiles.push_back(hm::RoiColumnProfile{ columnLabel, profileIndex });
}

int QtHoleMeasureWidget::profileIndexForHole(const HoleState& hole) const
{
    return columnProfileIndex(hole.point.columnLabel);
}

int QtHoleMeasureWidget::firstHoleIndexForProfile(int profileIndex) const
{
    for (int i = 0; i < static_cast<int>(m_holes.size()); ++i) {
        if (m_holes[i].roiProfileIndex == profileIndex) {
            return i;
        }
    }
    return -1;
}

void QtHoleMeasureWidget::selectFirstHoleInProfile(int profileIndex)
{
    const int holeIndex = firstHoleIndexForProfile(profileIndex);
    if (holeIndex < 0 || !m_holeTable) {
        return;
    }
    m_holeTable->selectRow(holeIndex);
    if (m_roiTable && m_roiTable->rowCount() > 0) {
        m_roiTable->selectRow(0);
    }
}

void QtHoleMeasureWidget::ensureProfileMasters()
{
    if (m_holes.empty() || m_roiProfiles.empty()) {
        return;
    }

    for (auto& profile : m_roiProfiles) {
        int masterHoleIndex = -1;
        for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
            if (m_holes[holeIndex].roiProfileIndex == profile.range.profileIndex) {
                masterHoleIndex = holeIndex;
                break;
            }
        }
        if (masterHoleIndex < 0) {
            profile.masterTemplateId = 0;
            profile.masterRois.clear();
            continue;
        }

        const auto& masterHole = m_holes[masterHoleIndex];
        if (profile.masterRois.empty()) {
            profile.masterTemplateId = masterHole.point.id;
            profile.masterCenter = masterHole.shiftedCenter;
            profile.masterRois = masterHole.rois;
            for (auto& roi : profile.masterRois) {
                roi.templateId = masterHole.point.id;
            }
            continue;
        }

        if (profile.masterTemplateId != masterHole.point.id) {
            profile.masterRois = hm::rebaseMasterRois(
                profile.masterRois,
                profile.masterCenter,
                masterHole.shiftedCenter,
                masterHole.point.id);
            profile.masterTemplateId = masterHole.point.id;
            profile.masterCenter = masterHole.shiftedCenter;
        }
    }
}

hm::HoleRoi QtHoleMeasureWidget::baseRoiForHole(int holeIndex, int roiIndex) const
{
    if (m_holes.empty()
        || holeIndex < 0
        || holeIndex >= static_cast<int>(m_holes.size())
        || roiIndex < 0) {
        return hm::HoleRoi{};
    }

    const int profileIndex = m_holes[holeIndex].roiProfileIndex;
    if (profileIndex < 0
        || profileIndex >= static_cast<int>(m_roiProfiles.size())
        || roiIndex >= static_cast<int>(m_roiProfiles[profileIndex].masterRois.size())) {
        return hm::HoleRoi{};
    }
    return hm::makeDerivedHoleRoi(
        m_roiProfiles[profileIndex].masterRois[roiIndex],
        m_roiProfiles[profileIndex].masterCenter,
        m_holes[holeIndex].shiftedCenter,
        m_holes[holeIndex].point.id,
        hm::RoiAdjustment{});
}

void QtHoleMeasureWidget::ensureRoiAdjustments()
{
    for (auto& hole : m_holes) {
        if (hole.roiAdjustments.size() != hole.rois.size()) {
            hole.roiAdjustments.resize(hole.rois.size());
        }
    }
}

void QtHoleMeasureWidget::rebuildRoisFromMaster()
{
    if (m_holes.empty()) {
        return;
    }

    ensureRoiAdjustments();
    ensureProfileMasters();
    for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
        auto& hole = m_holes[holeIndex];
        const int profileIndex = hole.roiProfileIndex;
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_roiProfiles.size())) {
            continue;
        }
        for (int roiIndex = 0; roiIndex < static_cast<int>(hole.rois.size())
            && roiIndex < static_cast<int>(m_roiProfiles[profileIndex].masterRois.size()); ++roiIndex) {
            const bool isProfileMaster = m_roiProfiles[profileIndex].masterTemplateId == hole.point.id;
            if (isProfileMaster) {
                hole.roiAdjustments[roiIndex] = hm::RoiAdjustment{};
            }
            hole.rois[roiIndex] = hm::makeDerivedHoleRoi(
                m_roiProfiles[profileIndex].masterRois[roiIndex],
                m_roiProfiles[profileIndex].masterCenter,
                hole.shiftedCenter,
                hole.point.id,
                isProfileMaster ? hm::RoiAdjustment{} : hole.roiAdjustments[roiIndex]);
        }
    }
}

void QtHoleMeasureWidget::applyEditedRoi(int holeIndex, int roiIndex, const hm::HoleRoi& editedRoi)
{
    if (m_holes.empty()
        || holeIndex < 0
        || holeIndex >= static_cast<int>(m_holes.size())
        || roiIndex < 0
        || roiIndex >= static_cast<int>(m_holes[holeIndex].rois.size())) {
        return;
    }

    ensureRoiAdjustments();
    ensureProfileMasters();
    const int profileIndex = m_holes[holeIndex].roiProfileIndex;
    if (profileIndex >= 0
        && profileIndex < static_cast<int>(m_roiProfiles.size())
        && m_roiProfiles[profileIndex].masterTemplateId == m_holes[holeIndex].point.id) {
        m_roiProfiles[profileIndex].masterRois[roiIndex] = editedRoi;
        m_roiProfiles[profileIndex].masterRois[roiIndex].templateId = m_holes[holeIndex].point.id;
        m_roiProfiles[profileIndex].masterCenter = m_holes[holeIndex].shiftedCenter;
        m_holes[holeIndex].roiAdjustments[roiIndex] = hm::RoiAdjustment{};
        rebuildRoisFromMaster();
        return;
    }
    if (profileIndex < 0 || profileIndex >= static_cast<int>(m_roiProfiles.size())) {
        return;
    }

    const hm::HoleRoi base = baseRoiForHole(holeIndex, roiIndex);
    m_holes[holeIndex].roiAdjustments[roiIndex] = hm::makeRoiAdjustment(base, editedRoi);
    m_holes[holeIndex].rois[roiIndex] = hm::makeDerivedHoleRoi(
        m_roiProfiles[profileIndex].masterRois[roiIndex],
        m_roiProfiles[profileIndex].masterCenter,
        m_holes[holeIndex].shiftedCenter,
        m_holes[holeIndex].point.id,
        m_holes[holeIndex].roiAdjustments[roiIndex]);
}

void QtHoleMeasureWidget::applyGaugeParamsToAllRois()
{
    if (m_holes.empty()) {
        return;
    }

    const hm::GaugeDefaults defaults = readDefaults();
    ensureProfileMasters();
    for (auto& profile : m_roiProfiles) {
        for (auto& roi : profile.masterRois) {
            roi.params = defaults;
        }
    }
    for (auto& hole : m_holes) {
        for (auto& adjustment : hole.roiAdjustments) {
            adjustment = hm::withoutGaugeParamOverride(adjustment);
        }
    }
    rebuildRoisFromMaster();
}

void QtHoleMeasureWidget::updateSelectedRoiTableRow()
{
    const int holeIndex = m_holeTable->currentRow();
    const int roiIndex = m_roiTable->currentRow();
    if (holeIndex < 0 || holeIndex >= static_cast<int>(m_holes.size())) {
        return;
    }
    if (roiIndex < 0 || roiIndex >= static_cast<int>(m_holes[holeIndex].rois.size())) {
        return;
    }

    const QSignalBlocker blocker(m_roiTable);
    const auto& roi = m_holes[holeIndex].rois[roiIndex];
    m_roiTable->setItem(roiIndex, 1, new QTableWidgetItem(qstr(roi.center.x)));
    m_roiTable->setItem(roiIndex, 2, new QTableWidgetItem(qstr(roi.center.y)));
    m_roiTable->setItem(roiIndex, 3, new QTableWidgetItem(qstr(roi.width)));
    m_roiTable->setItem(roiIndex, 4, new QTableWidgetItem(qstr(roi.height)));
    m_roiTable->setItem(roiIndex, 5, new QTableWidgetItem(qstr(roi.angleDeg, 3)));
    m_roiTable->setItem(roiIndex, 10, new QTableWidgetItem(polarityName(roi.params.polarity)));
}

void QtHoleMeasureWidget::syncRoiEditor(const hm::HoleRoi& roi)
{
    const QSignalBlocker blocker(m_roiPolarity);
    setPolarityComboValue(m_roiPolarity, roi.params.polarity);
}

void QtHoleMeasureWidget::log(const QString& text)
{
    m_status->setText(text);
}

void QtHoleMeasureWidget::searchHoleById()
{
    if (!m_searchId) {
        return;
    }
    bool ok = false;
    const int id = m_searchId->text().trimmed().toInt(&ok);
    if (!ok) {
        log("Search ID is invalid.");
        return;
    }

    for (int i = 0; i < static_cast<int>(m_holes.size()); ++i) {
        if (m_holes[i].point.id != id) {
            continue;
        }
        if (m_holeTable) {
            m_holeTable->selectRow(i);
            QTableWidgetItem* idItem = m_holeTable->item(i, 0);
            if (idItem) {
                m_holeTable->scrollToItem(idItem, QAbstractItemView::PositionAtCenter);
            }
        }
        populateRoiTable(i);
        displayImage();
        log(QString("Found ID %1.").arg(id));
        return;
    }

    log(QString("ID %1 not found.").arg(id));
}

hm::GaugeDefaults QtHoleMeasureWidget::readDefaults() const
{
    hm::GaugeDefaults defaults;
    defaults.edgeOffsetPx = m_edgeOffset->value();
    defaults.roiLengthPx = m_roiLength->value();
    defaults.roiWidthPx = m_roiWidth->value();
    defaults.sampleRegionWidth = m_sampleWidth->value();
    defaults.sampleRegionHeight = m_sampleHeight->value();
    defaults.sampleRegionInterval = m_sampleInterval->value();
    defaults.acceptScore = m_acceptScore->value();
    defaults.polarity = polarityComboValue(m_defaultPolarity);
    defaults.orientationDeg = m_angle->value();
    return defaults;
}

hm::ArrayOffset QtHoleMeasureWidget::readOffset() const
{
    return hm::ArrayOffset{ m_dx->value(), m_dy->value(), m_angle->value() };
}

void QtHoleMeasureWidget::loadDefaults()
{
    const QString base = QFileDialog::getExistingDirectory(this, "Select data folder", filesDir());
    if (base.isEmpty()) {
        return;
    }
    if (loadDefaultsFromDirectory(base)) {
        if (loadSavedParams()) {
            rebuildArray();
        }
        loadSavedRoiConfig();
    }
}

bool QtHoleMeasureWidget::loadDefaultsFromDirectory(const QString& base)
{
    const QString selectedBase = QDir(base).absolutePath();
    const QString oldDataDir = m_dataDir;
    m_dataDir = selectedBase;

    m_rawImage = LImage::Create();
    m_fixedImage = LImage::Create();
    m_templatePoints.clear();
    m_imagePoints.clear();
    m_holes.clear();
    m_measurements.clear();
    m_columnProfiles.clear();
    initializeRoiProfiles();
    resetView();
    populateHoleTable();
    populateColumnProfileTable();
    populateProfileList();
    populateRoiTable(0);
    updateMeasurementTable();

    const bool ok = loadCalibration(existingDataFilePath("distCharucoBoard_S_I.calib"))
        && loadImage(existingDataFilePath("image.bmp"))
        && fixImage();

    if (!ok) {
        m_dataDir = oldDataDir;
        log("Load failed. Check calibration/image files.");
        return false;
    }

    m_templatePoints = hm::loadTemplateCsv(existingDataFilePath("Template.csv").toStdString());
    if (m_templatePoints.empty() || !mapTemplateToImage()) {
        m_dataDir = oldDataDir;
        log("Point template load or WorldToImage mapping failed.");
        return false;
    }

    rebuildArray();
    log(QString("Loaded %1 template points and fixed image %2x%3.")
        .arg(static_cast<int>(m_templatePoints.size())).arg(m_fixedImage->Width).arg(m_fixedImage->Height));
    return true;
}

QString QtHoleMeasureWidget::roiConfigPath() const
{
    return filesDir() + "/hole_roi_config.csv";
}

QString QtHoleMeasureWidget::paramsPath() const
{
    return filesDir() + "/QtHoleMeasure/hole_measure_params.ini";
}

QString QtHoleMeasureWidget::existingDataFilePath(const QString& fileName) const
{
    const QString currentPath = QDir(filesDir()).filePath(fileName);
    if (QFileInfo::exists(currentPath)) {
        return currentPath;
    }
    return QDir(m_defaultDataDir).filePath(fileName);
}

hm::AppParams QtHoleMeasureWidget::readAppParams() const
{
    hm::AppParams params;
    const hm::GaugeDefaults defaults = readDefaults();
    params.offsetX = m_dx->value();
    params.offsetY = m_dy->value();
    params.angleDeg = m_angle->value();
    params.micronPerPixel = m_micronPerPixel->value();
    params.pointOrder = m_sortOrder->currentIndex() == 0
        ? hm::ExportOrder::FirstColumnTopDown
        : hm::ExportOrder::LastColumnTopDown;
    params.lineFindMethod = m_lineFindMethod->currentIndex() == 1
        ? hm::LineFindMethod::LineDetector
        : hm::LineFindMethod::LineGauge;
    params.gauge = defaults;
    params.roiProfiles.clear();
    for (const auto& profile : m_roiProfiles) {
        params.roiProfiles.push_back(profile.range);
    }
    params.columnProfiles = m_columnProfiles;
    return params;
}

void QtHoleMeasureWidget::applyAppParams(const hm::AppParams& params)
{
    const QSignalBlocker dxBlocker(m_dx);
    const QSignalBlocker dyBlocker(m_dy);
    const QSignalBlocker angleBlocker(m_angle);
    const QSignalBlocker edgeBlocker(m_edgeOffset);
    const QSignalBlocker lengthBlocker(m_roiLength);
    const QSignalBlocker widthBlocker(m_roiWidth);
    const QSignalBlocker micronBlocker(m_micronPerPixel);
    const QSignalBlocker sampleWidthBlocker(m_sampleWidth);
    const QSignalBlocker sampleHeightBlocker(m_sampleHeight);
    const QSignalBlocker sampleIntervalBlocker(m_sampleInterval);
    const QSignalBlocker acceptScoreBlocker(m_acceptScore);
    const QSignalBlocker polarityBlocker(m_defaultPolarity);
    const QSignalBlocker sortBlocker(m_sortOrder);
    const QSignalBlocker lineFindBlocker(m_lineFindMethod);

    m_dx->setValue(params.offsetX);
    m_dy->setValue(params.offsetY);
    m_angle->setValue(params.angleDeg);
    m_edgeOffset->setValue(params.gauge.edgeOffsetPx);
    m_roiLength->setValue(params.gauge.roiLengthPx);
    m_roiWidth->setValue(params.gauge.roiWidthPx);
    m_micronPerPixel->setValue(params.micronPerPixel);
    m_sampleWidth->setValue(params.gauge.sampleRegionWidth);
    m_sampleHeight->setValue(params.gauge.sampleRegionHeight);
    m_sampleInterval->setValue(params.gauge.sampleRegionInterval);
    m_acceptScore->setValue(params.gauge.acceptScore);
    setPolarityComboValue(m_defaultPolarity, params.gauge.polarity);
    m_sortOrder->setCurrentIndex(params.pointOrder == hm::ExportOrder::LastColumnTopDown ? 1 : 0);
    m_lineFindMethod->setCurrentIndex(params.lineFindMethod == hm::LineFindMethod::LineDetector ? 1 : 0);
    if (!params.roiProfiles.empty()) {
        if (m_roiProfiles.size() < params.roiProfiles.size()) {
            m_roiProfiles.resize(params.roiProfiles.size());
        }
        for (int i = 0; i < static_cast<int>(params.roiProfiles.size()); ++i) {
            m_roiProfiles[i].range = params.roiProfiles[i];
            m_roiProfiles[i].range.profileIndex = i;
            m_roiProfiles[i].masterTemplateId = 0;
            m_roiProfiles[i].masterRois.clear();
        }
    }
    m_columnProfiles = params.columnProfiles;
    populateColumnProfileTable();
    populateProfileList();
}

bool QtHoleMeasureWidget::loadSavedParams()
{
    hm::AppParams params = readAppParams();
    QStringList paths;
    paths << paramsPath()
          << QDir(filesDir()).filePath("hole_measure_params.ini");
    if (QDir(filesDir()).absolutePath() != QDir(m_defaultDataDir).absolutePath()) {
        paths << QDir(m_defaultDataDir).filePath("QtHoleMeasure/hole_measure_params.ini")
              << QDir(m_defaultDataDir).filePath("hole_measure_params.ini");
    }

    QString loadedPath;
    for (const QString& path : paths) {
        if (hm::loadAppParams(path.toStdString(), params)) {
            loadedPath = path;
            break;
        }
    }
    if (loadedPath.isEmpty()) {
        log("No params loaded.");
        return false;
    }

    applyAppParams(params);
    log("Loaded params: " + loadedPath);
    return true;
}

bool QtHoleMeasureWidget::loadSavedRoiConfig()
{
    if (m_holes.empty()) {
        return false;
    }

    QStringList paths;
    paths << roiConfigPath()
          << QDir(filesDir()).filePath("QtHoleMeasure/hole_roi_config.csv");

    QString loadedPath;
    std::vector<hm::HoleRoi> rois;
    for (const QString& path : paths) {
        rois = hm::loadRoisCsv(path.toStdString());
        if (!rois.empty()) {
            loadedPath = path;
            break;
        }
    }
    if (rois.empty()) {
        log("No ROI config loaded.");
        return false;
    }

    applyLoadedRois(rois);
    clearMeasurements();
    populateProfileList();
    populateRoiTable(m_holeTable->currentRow() >= 0 ? m_holeTable->currentRow() : 0);
    displayImage();
    log(QString("Loaded ROI: %1 rows from %2").arg(static_cast<int>(rois.size())).arg(loadedPath));
    return true;
}

bool QtHoleMeasureWidget::loadCalibration(const QString& path)
{
    std::vector<ILCalibPtr> candidates;
    candidates.push_back(LCalibFFD::Create());
    candidates.push_back(LCalibPinHole::Create());
    candidates.push_back(LCalibNPoints::Create());
    candidates.push_back(LCalibCustom::Create());

    for (auto& candidate : candidates) {
        const LPVErrorCode err = candidate->Load(path.toStdWString());
        if (err == LPVNoError && candidate->IsCalibrated()) {
            m_calib = candidate;
            return true;
        }
    }
    return false;
}

bool QtHoleMeasureWidget::loadImage(const QString& path)
{
    m_displayImageSet = false;
    return m_rawImage->Load(path.toStdWString()) == LPVNoError && m_rawImage->Valid();
}

bool QtHoleMeasureWidget::fixImage()
{
    if (!m_calib || !m_calib->IsCalibrated() || !m_rawImage->Valid()) {
        return false;
    }
    m_calib->FixImageMode = LPVFixImageUndistort;
    const LPVErrorCode err = m_calib->FixImage(m_rawImage, m_fixedImage);
    return err == LPVNoError && m_fixedImage->Valid();
}

bool QtHoleMeasureWidget::mapTemplateToImage()
{
    if (!m_calib || !m_calib->IsCalibrated()) {
        return false;
    }

    m_imagePoints.clear();
    m_imagePoints.reserve(m_templatePoints.size());
    for (const auto& point : m_templatePoints) {
        double x = 0.0;
        double y = 0.0;
        m_calib->WorldToImage(point.worldX, point.worldY, &x, &y);
        m_imagePoints.push_back(hm::ImagePoint{ x, y });
    }

    if (m_templatePoints.size() > 1 && m_imagePoints.size() > 1) {
        const double worldDx = std::fabs(m_templatePoints[1].worldX - m_templatePoints[0].worldX);
        const double worldDy = std::fabs(m_templatePoints[1].worldY - m_templatePoints[0].worldY);
        const double imageDx = m_imagePoints[1].x - m_imagePoints[0].x;
        const double imageDy = m_imagePoints[1].y - m_imagePoints[0].y;
        const double worldDist = std::sqrt(worldDx * worldDx + worldDy * worldDy);
        const double imageDist = std::sqrt(imageDx * imageDx + imageDy * imageDy);
        if (worldDist > 0 && imageDist > 0) {
            m_micronPerPixel->setValue(worldDist / imageDist);
        }
    }
    return true;
}

void QtHoleMeasureWidget::rebuildArray()
{
    if (m_templatePoints.empty() || m_imagePoints.empty()) {
        return;
    }

    std::vector<RoiProfileState> previousProfiles = m_roiProfiles;
    std::map<std::pair<int, int>, hm::RoiAdjustment> previousAdjustments;
    if (!m_holes.empty()) {
        for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
            const auto& hole = m_holes[holeIndex];
            for (int roiIndex = 0; roiIndex < static_cast<int>(hole.roiAdjustments.size())
                && roiIndex < static_cast<int>(hole.rois.size()); ++roiIndex) {
                previousAdjustments[std::make_pair(hole.point.id, static_cast<int>(hole.rois[roiIndex].side))]
                    = hole.roiAdjustments[roiIndex];
            }
        }
    }

    const hm::ExportOrder order = m_sortOrder->currentIndex() == 0
        ? hm::ExportOrder::FirstColumnTopDown
        : hm::ExportOrder::LastColumnTopDown;
    hm::sortTemplateImagePairs(m_templatePoints, m_imagePoints, order);

    const auto shifted = hm::applyArrayOffset(m_imagePoints, readOffset());
    const hm::GaugeDefaults defaults = readDefaults();

    m_holes.clear();
    m_holes.reserve(m_templatePoints.size());
    for (int i = 0; i < static_cast<int>(m_templatePoints.size()); ++i) {
        HoleState state;
        state.point = m_templatePoints[i];
        state.imageCenter = m_imagePoints[i];
        state.shiftedCenter = shifted[i];
        state.rois = hm::makeDefaultHoleRois(state.shiftedCenter, state.point.id, defaults);
        state.roiProfileIndex = profileIndexForHole(state);
        state.measurement.templateId = state.point.id;
        state.measurement.rowLabel = state.point.rowLabel;
        state.measurement.columnLabel = state.point.columnLabel;
        state.measurement.centerX = state.shiftedCenter.x;
        state.measurement.centerY = state.shiftedCenter.y;
        m_holes.push_back(state);
    }

    for (auto& profile : m_roiProfiles) {
        if (profile.range.profileIndex < 0
            || profile.range.profileIndex >= static_cast<int>(previousProfiles.size())) {
            continue;
        }
        const auto& previous = previousProfiles[profile.range.profileIndex];
        profile.masterTemplateId = previous.masterTemplateId;
        profile.masterCenter = previous.masterCenter;
        profile.masterRois = previous.masterRois;
        if (!profile.masterRois.empty()) {
            int masterHoleIndex = -1;
            for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
                if (m_holes[holeIndex].point.id == profile.masterTemplateId) {
                    masterHoleIndex = holeIndex;
                    break;
                }
            }
            if (masterHoleIndex >= 0) {
                const double dx = m_holes[masterHoleIndex].shiftedCenter.x - previous.masterCenter.x;
                const double dy = m_holes[masterHoleIndex].shiftedCenter.y - previous.masterCenter.y;
                profile.masterCenter = m_holes[masterHoleIndex].shiftedCenter;
                for (auto& roi : profile.masterRois) {
                    roi.templateId = m_holes[masterHoleIndex].point.id;
                    roi.center.x += dx;
                    roi.center.y += dy;
                }
            }
            else {
                profile.masterTemplateId = 0;
                profile.masterRois.clear();
            }
        }
    }
    ensureRoiAdjustments();
    for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
        auto& hole = m_holes[holeIndex];
        for (int roiIndex = 0; roiIndex < static_cast<int>(hole.rois.size()); ++roiIndex) {
            const auto key = std::make_pair(hole.point.id, static_cast<int>(hole.rois[roiIndex].side));
            const auto it = previousAdjustments.find(key);
            if (it != previousAdjustments.end()) {
                hole.roiAdjustments[roiIndex] = it->second;
            }
        }
    }
    rebuildRoisFromMaster();

    populateHoleTable();
    populateColumnProfileTable();
    populateProfileList();
    populateRoiTable(m_holeTable->currentRow() >= 0 ? m_holeTable->currentRow() : 0);
    clearMeasurements();
    displayImage();
}

void QtHoleMeasureWidget::populateHoleTable()
{
    const QSignalBlocker blocker(m_holeTable);
    m_holeTable->setRowCount(static_cast<int>(m_holes.size()));
    for (int i = 0; i < static_cast<int>(m_holes.size()); ++i) {
        const auto& hole = m_holes[i];
        m_holeTable->setItem(i, 0, item(QString::number(hole.point.id)));
        m_holeTable->setItem(i, 1, item(QString::fromStdString(hole.point.rowLabel)));
        m_holeTable->setItem(i, 2, item(QString::fromStdString(hole.point.columnLabel)));
        m_holeTable->setItem(i, 3, item(QString::number(hole.roiProfileIndex + 1)));
        m_holeTable->setItem(i, 4, item(qstr(hole.shiftedCenter.x)));
        m_holeTable->setItem(i, 5, item(qstr(hole.shiftedCenter.y)));
    }
    if (!m_holes.empty() && m_holeTable->currentRow() < 0) {
        m_holeTable->selectRow(0);
    }
}

void QtHoleMeasureWidget::populateRoiTable(int holeIndex)
{
    if (holeIndex < 0 || holeIndex >= static_cast<int>(m_holes.size())) {
        m_roiTable->setRowCount(0);
        const QSignalBlocker polarityBlocker(m_roiPolarity);
        setPolarityComboValue(m_roiPolarity, 0);
        return;
    }

    const QSignalBlocker blocker(m_roiTable);
    const auto& rois = m_holes[holeIndex].rois;
    m_roiTable->setRowCount(static_cast<int>(rois.size()));
    for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
        const auto& roi = rois[i];
        m_roiTable->setItem(i, 0, item(QString::fromStdString(hm::sideName(roi.side))));
        m_roiTable->setItem(i, 1, new QTableWidgetItem(qstr(roi.center.x)));
        m_roiTable->setItem(i, 2, new QTableWidgetItem(qstr(roi.center.y)));
        m_roiTable->setItem(i, 3, new QTableWidgetItem(qstr(roi.width)));
        m_roiTable->setItem(i, 4, new QTableWidgetItem(qstr(roi.height)));
        m_roiTable->setItem(i, 5, new QTableWidgetItem(qstr(roi.angleDeg, 3)));
        m_roiTable->setItem(i, 6, new QTableWidgetItem(QString::number(roi.params.sampleRegionWidth)));
        m_roiTable->setItem(i, 7, new QTableWidgetItem(QString::number(roi.params.sampleRegionHeight)));
        m_roiTable->setItem(i, 8, new QTableWidgetItem(QString::number(roi.params.sampleRegionInterval)));
        m_roiTable->setItem(i, 9, new QTableWidgetItem(QString::number(roi.params.acceptScore)));
        m_roiTable->setItem(i, 10, new QTableWidgetItem(polarityName(roi.params.polarity)));
    }
    if (!rois.empty() && m_roiTable->currentRow() < 0) {
        m_roiTable->selectRow(0);
    }
    const int roiIndex = m_roiTable->currentRow();
    if (roiIndex >= 0 && roiIndex < static_cast<int>(rois.size())) {
        syncRoiEditor(rois[roiIndex]);
    }
}

void QtHoleMeasureWidget::populateColumnProfileTable()
{
    if (!m_columnProfileTable) {
        return;
    }

    std::set<std::string> columnSet;
    for (const auto& hole : m_holes) {
        columnSet.insert(hole.point.columnLabel);
    }
    std::vector<std::string> columns(columnSet.begin(), columnSet.end());
    columns = hm::sortColumnLabelsForOrder(columns, m_sortOrder->currentIndex() == 0
        ? hm::ExportOrder::FirstColumnTopDown
        : hm::ExportOrder::LastColumnTopDown);

    const QSignalBlocker blocker(m_columnProfileTable);
    m_columnProfileTable->setRowCount(static_cast<int>(columns.size()));
    for (int row = 0; row < static_cast<int>(columns.size()); ++row) {
        const std::string& column = columns[row];
        const int profileIndex = columnProfileIndex(column);
        m_columnProfileTable->setItem(row, 0, item(QString::fromStdString(column)));
        for (int profile = 0; profile < static_cast<int>(m_roiProfiles.size()); ++profile) {
            m_columnProfileTable->setItem(row, profile + 1, checkItem(profile == profileIndex));
        }
    }
}

void QtHoleMeasureWidget::populateProfileList()
{
    if (!m_profileList) {
        return;
    }

    const int selected = selectedProfileIndex();
    const QSignalBlocker blocker(m_profileList);
    m_profileList->setRowCount(static_cast<int>(m_roiProfiles.size()));
    for (int profile = 0; profile < static_cast<int>(m_roiProfiles.size()); ++profile) {
        std::set<std::string> columnSet;
        for (const auto& hole : m_holes) {
            if (hole.roiProfileIndex == profile) {
                columnSet.insert(hole.point.columnLabel);
            }
        }
        std::vector<std::string> sortedColumns(columnSet.begin(), columnSet.end());
        sortedColumns = hm::sortColumnLabelsForOrder(sortedColumns, m_sortOrder->currentIndex() == 0
            ? hm::ExportOrder::FirstColumnTopDown
            : hm::ExportOrder::LastColumnTopDown);
        QStringList columns;
        for (const auto& column : sortedColumns) {
            columns << QString::fromStdString(column);
        }

        const int masterId = m_roiProfiles[profile].masterTemplateId;
        m_profileList->setItem(profile, 0, item(QString("Profile %1").arg(profile + 1)));
        m_profileList->setItem(profile, 1, item(columns.join(",")));
        m_profileList->setItem(profile, 2, item(masterId > 0 ? QString::number(masterId) : "-"));
    }
    if (!m_roiProfiles.empty()) {
        m_profileList->selectRow(selected >= 0 && selected < static_cast<int>(m_roiProfiles.size()) ? selected : 0);
    }
}

void QtHoleMeasureWidget::selectedHoleChanged()
{
    const int holeIndex = m_holeTable->currentRow();
    if (holeIndex >= 0 && holeIndex < static_cast<int>(m_holes.size()) && m_profileList) {
        const QSignalBlocker blocker(m_profileList);
        m_profileList->selectRow(m_holes[holeIndex].roiProfileIndex);
    }
    populateRoiTable(m_holeTable->currentRow());
    displayImage();
}

void QtHoleMeasureWidget::selectedRoiChanged()
{
    if (auto* roi = selectedRoi()) {
        syncRoiEditor(*roi);
    }
    displayImage();
}

void QtHoleMeasureWidget::applyRoiEdits()
{
    const int holeIndex = m_holeTable->currentRow();
    if (holeIndex < 0 || holeIndex >= static_cast<int>(m_holes.size())) {
        return;
    }
    const hm::GaugeDefaults defaults = readDefaults();
    const int selectedRoiIndex = m_roiTable->currentRow();
    for (int i = 0; i < m_roiTable->rowCount() && i < static_cast<int>(m_holes[holeIndex].rois.size()); ++i) {
        hm::HoleRoi edited = m_holes[holeIndex].rois[i];
        edited.center.x = m_roiTable->item(i, 1)->text().toDouble();
        edited.center.y = m_roiTable->item(i, 2)->text().toDouble();
        edited.width = m_roiTable->item(i, 3)->text().toDouble();
        edited.height = m_roiTable->item(i, 4)->text().toDouble();
        edited.angleDeg = m_roiTable->item(i, 5)->text().toDouble();
        edited.params.kernelSize = defaults.kernelSize;
        edited.params.normScore = defaults.normScore;
        edited.params.maxSamplePointCount = defaults.maxSamplePointCount;
        edited.params.fitDistThreshold = defaults.fitDistThreshold;
        edited.params.fitCountThreshold = defaults.fitCountThreshold;
        edited.params.sampleRegionWidth = m_roiTable->item(i, 6)->text().toInt();
        edited.params.sampleRegionHeight = m_roiTable->item(i, 7)->text().toInt();
        edited.params.sampleRegionInterval = m_roiTable->item(i, 8)->text().toInt();
        edited.params.acceptScore = m_roiTable->item(i, 9)->text().toInt();
        edited.params.polarity = i == selectedRoiIndex
            ? polarityComboValue(m_roiPolarity)
            : polarityValue(m_roiTable->item(i, 10)->text());
        applyEditedRoi(holeIndex, i, edited);
    }
    populateRoiTable(holeIndex);
    updateSelectedRoiTableRow();
    clearMeasurements();
    displayImage();
}

void QtHoleMeasureWidget::applyGaugeDefaults()
{
    if (m_holes.empty()) {
        return;
    }

    applyGaugeParamsToAllRois();
    populateRoiTable(m_holeTable->currentRow());
    clearMeasurements();
    displayImage();
}

void QtHoleMeasureWidget::gaugeDefaultsChanged()
{
    applyGaugeDefaults();
}

void QtHoleMeasureWidget::selectedProfileChanged()
{
    selectedProfileGroupChanged();
}

void QtHoleMeasureWidget::selectedProfileGroupChanged()
{
    const int profileIndex = selectedProfileIndex();
    selectFirstHoleInProfile(profileIndex);
    displayImage();
}

void QtHoleMeasureWidget::columnProfileItemChanged(QTableWidgetItem* changedItem)
{
    if (!changedItem || !m_columnProfileTable || changedItem->column() <= 0) {
        return;
    }
    if (changedItem->checkState() != Qt::Checked) {
        const QSignalBlocker blocker(m_columnProfileTable);
        changedItem->setCheckState(Qt::Checked);
        return;
    }

    const int row = changedItem->row();
    const int profileIndex = changedItem->column() - 1;
    QTableWidgetItem* columnItem = m_columnProfileTable->item(row, 0);
    if (!columnItem) {
        return;
    }

    const std::string columnLabel = columnItem->text().toStdString();
    setColumnProfileIndex(columnLabel, profileIndex);
    {
        const QSignalBlocker blocker(m_columnProfileTable);
        for (int col = 1; col < m_columnProfileTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_columnProfileTable->item(row, col);
            if (item) {
                item->setCheckState(col == changedItem->column() ? Qt::Checked : Qt::Unchecked);
            }
        }
    }
    for (auto& profile : m_roiProfiles) {
        profile.masterTemplateId = 0;
        profile.masterRois.clear();
    }
    for (auto& hole : m_holes) {
        hole.roiProfileIndex = profileIndexForHole(hole);
        hole.roiAdjustments.assign(hole.rois.size(), hm::RoiAdjustment{});
    }
    ensureProfileMasters();
    rebuildRoisFromMaster();
    populateHoleTable();
    populateProfileList();
    populateRoiTable(m_holeTable->currentRow());
    clearMeasurements();
    displayImage();
    log(QString("Column %1 assigned to Profile %2.")
        .arg(QString::fromStdString(columnLabel))
        .arg(profileIndex + 1)
    );
}

hm::GaugeLine QtHoleMeasureWidget::detectLine(const hm::HoleRoi& roi)
{
    hm::GaugeLine line;
    if (!m_fixedImage || !m_fixedImage->Valid()) {
        return line;
    }

    ILRotRectRegionPtr region = LRotRectRegion::Create();
    region->SetPlacement(roi.center.x, roi.center.y, roi.width, roi.height, roi.angleDeg);

    return m_lineFindMethod && m_lineFindMethod->currentIndex() == 1
        ? detectLineByDetector(roi, region)
        : detectLineByGauge(roi, region);
}

hm::GaugeLine QtHoleMeasureWidget::detectLineByGauge(const hm::HoleRoi& roi, const ILRotRectRegionPtr& region)
{
    if (!m_lineGauge) {
        return hm::GaugeLine{};
    }

    m_lineGauge->KernelSize = roi.params.kernelSize;
    m_lineGauge->Polarity = static_cast<LPVPolarity>(roi.params.polarity);
    m_lineGauge->AcceptScore = roi.params.acceptScore;
    m_lineGauge->NormScore = false;
    m_lineGauge->SampleRegionWidth = roi.params.sampleRegionWidth;
    m_lineGauge->SampleRegionHeight = roi.params.sampleRegionHeight;
    m_lineGauge->SampleRegionInterval = roi.params.sampleRegionInterval;
    m_lineGauge->MaxSamplePointCount = roi.params.maxSamplePointCount;
    m_lineGauge->FitDistThreshold = roi.params.fitDistThreshold;
    m_lineGauge->FitCountThreshold = roi.params.fitCountThreshold;

    ILShapeGaugeResultPtr result;
    const LPVErrorCode err = m_lineGauge->Detect(m_fixedImage, region, &result);
    if ((err != LPVNoError && err != LPVNoResult) || !result || !result->Result) {
        return hm::GaugeLine{};
    }

    ILLinePtr detected = LLine::Cast(result->Result);
    if (!detected) {
        return hm::GaugeLine{};
    }

    return hm::selectLineCandidate(std::vector<hm::GaugeLine>{ makeGaugeLineFromLpvLine(detected, result->Score) },
        roi, 10.0);
}

hm::GaugeLine QtHoleMeasureWidget::detectLineByDetector(const hm::HoleRoi& roi, const ILRotRectRegionPtr& region)
{
    if (!m_lineDetector) {
        return hm::GaugeLine{};
    }

    m_lineDetector->Polarity = static_cast<LPVPolarity>(roi.params.polarity);
    m_lineDetector->FindBy = LPVFindBest;
    m_lineDetector->AcceptScore = roi.params.acceptScore;
    m_lineDetector->EdgeWidth = roi.params.kernelSize > 1 ? roi.params.kernelSize : 1;
    m_lineDetector->NormScore = roi.params.normScore;
    m_lineDetector->MaxCount = 16;

    ILLineResultsPtr results;
    const LPVErrorCode err = m_lineDetector->Detect(m_fixedImage, region, &results);
    if (err != LPVNoError || !results || results->Count() <= 0) {
        return hm::GaugeLine{};
    }

    std::vector<hm::GaugeLine> candidates;
    candidates.reserve(results->Count());
    for (int i = 0; i < results->Count(); ++i) {
        ILLineResultPtr result = results->Item(i);
        if (!result) {
            continue;
        }
        const hm::GaugeLine candidate = makeGaugeLineFromLpvLine(result->Line, result->Score);
        if (candidate.ok) {
            candidates.push_back(candidate);
        }
    }

    return hm::selectLineCandidate(candidates, roi, 10.0);
}

bool QtHoleMeasureWidget::applyTemplateAlignmentWithLpv()
{
    std::vector<int> validIndexes;
    validIndexes.reserve(m_measurements.size());

    ILPointsPtr measuredPoints = LPoints::Create();
    ILPointsPtr templatePoints = LPoints::Create();
    if (!measuredPoints || !templatePoints) {
        for (auto& measurement : m_measurements) {
            invalidateAlignment(measurement);
        }
        return false;
    }

    for (int i = 0; i < static_cast<int>(m_measurements.size()); ++i) {
        hm::HoleMeasurement& measurement = m_measurements[i];
        if (!hasValidWorldPoint(measurement)) {
            invalidateAlignment(measurement);
            continue;
        }

        validIndexes.push_back(i);
        measuredPoints->Add(measurement.measuredWorldX, measurement.measuredWorldY);
        templatePoints->Add(measurement.templateWorldX, measurement.templateWorldY);
    }

    if (validIndexes.size() < 2) {
        for (int index : validIndexes) {
            invalidateAlignment(m_measurements[index]);
        }
        return false;
    }

    try {
        ILTransformPtr transform = LTransform::Create();
        if (!transform) {
            for (int index : validIndexes) {
                invalidateAlignment(m_measurements[index]);
            }
            return false;
        }

        const double mapError = transform->Build(measuredPoints, templatePoints, LPVTransformRigid);
        if (mapError < 0.0) {
            for (int index : validIndexes) {
                invalidateAlignment(m_measurements[index]);
            }
            return false;
        }

        ILPointsPtr alignedPoints = measuredPoints->Transform(transform);
        if (!alignedPoints || alignedPoints->Count() != static_cast<int>(validIndexes.size())) {
            for (int index : validIndexes) {
                invalidateAlignment(m_measurements[index]);
            }
            return false;
        }

        for (int i = 0; i < static_cast<int>(validIndexes.size()); ++i) {
            ILPointPtr alignedPoint = alignedPoints->Item(i);
            if (!alignedPoint) {
                invalidateAlignment(m_measurements[validIndexes[i]]);
                continue;
            }

            hm::HoleMeasurement& measurement = m_measurements[validIndexes[i]];
            measurement.alignedWorldX = alignedPoint->GetX();
            measurement.alignedWorldY = alignedPoint->GetY();
            measurement.deltaX = measurement.alignedWorldX - measurement.templateWorldX;
            measurement.deltaY = measurement.alignedWorldY - measurement.templateWorldY;
        }
    }
    catch (...) {
        for (int index : validIndexes) {
            invalidateAlignment(m_measurements[index]);
        }
        return false;
    }

    return true;
}

void QtHoleMeasureWidget::measureAll()
{
    if (m_holes.empty()) {
        return;
    }

    m_measurements.clear();
    for (auto& hole : m_holes) {
        hm::GaugeLine lines[4];
        for (int i = 0; i < 4 && i < static_cast<int>(hole.rois.size()); ++i) {
            lines[i] = detectLine(hole.rois[i]);
        }
        hole.measurement = hm::makeMeasurement(hole.point, hole.shiftedCenter,
            lines[0], lines[1], lines[2], lines[3], m_micronPerPixel->value());
        if (hole.measurement.ok && m_calib && m_calib->IsCalibrated()) {
            m_calib->ImageToWorld(hole.measurement.centerX, hole.measurement.centerY,
                &hole.measurement.measuredWorldX, &hole.measurement.measuredWorldY);
        }
        m_measurements.push_back(hole.measurement);
    }
    const bool alignmentOk = applyTemplateAlignmentWithLpv();
    for (int i = 0; i < static_cast<int>(m_measurements.size()) && i < static_cast<int>(m_holes.size()); ++i) {
        m_holes[i].measurement = m_measurements[i];
    }
    updateMeasurementTable();
    displayImage();
    log(QString("Measured %1 holes. OK count: %2.")
        .arg(static_cast<int>(m_measurements.size()))
        .arg(std::count_if(m_measurements.begin(), m_measurements.end(), [](const hm::HoleMeasurement& m) { return m.ok; })));
    if (!alignmentOk) {
        log("Template alignment failed. Alignment and delta values are set to -9999.");
    }
}

void QtHoleMeasureWidget::updateMeasurementTable()
{
    m_resultTable->setRowCount(static_cast<int>(m_measurements.size()));
    const auto displayMeasurements = hm::applyMeasurementOffsets(m_measurements);
    for (int i = 0; i < static_cast<int>(m_measurements.size()); ++i) {
        const auto& m = displayMeasurements[i];
        m_resultTable->setItem(i, 0, item(QString::number(m.templateId)));
        m_resultTable->setItem(i, 1, item(QString::fromStdString(m.rowLabel)));
        m_resultTable->setItem(i, 2, item(QString::fromStdString(m.columnLabel)));
        m_resultTable->setItem(i, 3, item(qstr(m.widthPx)));
        m_resultTable->setItem(i, 4, item(qstr(m.heightPx)));
        m_resultTable->setItem(i, 5, item(qstr(m.widthMicron, 3)));
        m_resultTable->setItem(i, 6, item(qstr(m.heightMicron, 3)));
    }
}

void QtHoleMeasureWidget::clearMeasurements()
{
    m_measurements.clear();
    for (auto& hole : m_holes) {
        hm::HoleMeasurement measurement;
        measurement.templateId = hole.point.id;
        measurement.rowLabel = hole.point.rowLabel;
        measurement.columnLabel = hole.point.columnLabel;
        measurement.centerX = hole.shiftedCenter.x;
        measurement.centerY = hole.shiftedCenter.y;
        measurement.xOffset = hole.point.xOffset;
        measurement.yOffset = hole.point.yOffset;
        measurement.templateWorldX = hole.point.worldX;
        measurement.templateWorldY = hole.point.worldY;
        hole.measurement = measurement;
    }
    updateMeasurementTable();
}

void QtHoleMeasureWidget::saveResults()
{
    if (m_measurements.empty()) {
        measureAll();
    }

    const QString path = filesDir() + "/hole_measurements.csv";
    hm::saveMeasurementsCsv(path.toStdString(), hm::applyMeasurementOffsets(m_measurements));
    log("Saved: " + path);
}

void QtHoleMeasureWidget::saveParams()
{
    applyRoiEdits();
    const QString path = paramsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    hm::saveAppParams(path.toStdString(), readAppParams());
    const QString roiPath = roiConfigPath();
    if (!m_holes.empty()) {
        QDir().mkpath(QFileInfo(roiPath).absolutePath());
        hm::saveRoisCsv(roiPath.toStdString(), collectRois());
    }
    log("Saved params and ROI: " + path);
}

std::vector<hm::HoleRoi> QtHoleMeasureWidget::collectRois() const
{
    std::vector<hm::HoleRoi> rois;
    for (const auto& hole : m_holes) {
        for (const auto& roi : hole.rois) {
            rois.push_back(roi);
        }
    }
    return rois;
}

void QtHoleMeasureWidget::saveRoiConfig()
{
    applyRoiEdits();
    const QString path = roiConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    hm::saveRoisCsv(path.toStdString(), collectRois());
    log("Saved ROI: " + path);
}

void QtHoleMeasureWidget::applyLoadedRois(const std::vector<hm::HoleRoi>& rois)
{
    std::map<std::pair<int, int>, hm::HoleRoi> byKey;
    for (const auto& roi : rois) {
        byKey[std::make_pair(roi.templateId, static_cast<int>(roi.side))] = roi;
    }

    if (m_holes.empty()) {
        return;
    }

    for (auto& hole : m_holes) {
        hole.roiProfileIndex = profileIndexForHole(hole);
    }
    for (auto& profile : m_roiProfiles) {
        profile.masterTemplateId = 0;
        profile.masterRois.clear();
    }
    ensureRoiAdjustments();
    for (auto& profile : m_roiProfiles) {
        int masterHoleIndex = -1;
        for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
            if (m_holes[holeIndex].roiProfileIndex == profile.range.profileIndex) {
                masterHoleIndex = holeIndex;
                break;
            }
        }
        if (masterHoleIndex < 0) {
            continue;
        }
        profile.masterTemplateId = m_holes[masterHoleIndex].point.id;
        profile.masterCenter = m_holes[masterHoleIndex].shiftedCenter;
        profile.masterRois = m_holes[masterHoleIndex].rois;
        for (auto& roi : profile.masterRois) {
            auto it = byKey.find(std::make_pair(profile.masterTemplateId, static_cast<int>(roi.side)));
            if (it != byKey.end()) {
                roi = it->second;
            }
            roi.templateId = profile.masterTemplateId;
        }
    }

    for (int holeIndex = 0; holeIndex < static_cast<int>(m_holes.size()); ++holeIndex) {
        auto& hole = m_holes[holeIndex];
        for (int roiIndex = 0; roiIndex < static_cast<int>(hole.rois.size())
            && hole.roiProfileIndex >= 0
            && hole.roiProfileIndex < static_cast<int>(m_roiProfiles.size())
            && roiIndex < static_cast<int>(m_roiProfiles[hole.roiProfileIndex].masterRois.size()); ++roiIndex) {
            const hm::HoleRoi base = baseRoiForHole(holeIndex, roiIndex);
            hm::HoleRoi edited = base;
            auto it = byKey.find(std::make_pair(hole.point.id, static_cast<int>(base.side)));
            if (it != byKey.end()) {
                edited = it->second;
            }
            hole.roiAdjustments[roiIndex] = hm::makeRoiAdjustment(base, edited);
            hole.rois[roiIndex] = hm::makeDerivedHoleRoi(
                m_roiProfiles[hole.roiProfileIndex].masterRois[roiIndex],
                m_roiProfiles[hole.roiProfileIndex].masterCenter,
                hole.shiftedCenter,
                hole.point.id,
                hole.roiAdjustments[roiIndex]);
        }
    }
}

void QtHoleMeasureWidget::loadRoiConfig()
{
    if (m_holes.empty()) {
        loadDefaults();
    }

    loadSavedRoiConfig();
}

void QtHoleMeasureWidget::displayImage()
{
    if (!m_fixedImage || !m_fixedImage->Valid() || !m_displayCtrl) {
        return;
    }

    if (!m_displayImageSet) {
        m_displayCtrl->SetImage(*m_fixedImage);
        m_displayCtrl->FitToWindow(LPVAlignDefault);
        m_displayImageSet = true;
    }
    m_displayCtrl->RemoveAllRegions();
    m_displayCtrl->RemoveAllObjects();
    m_displayRegionMap.clear();

    const int selectedHole = m_holeTable->currentRow();
    for (int i = 0; i < static_cast<int>(m_holes.size()); ++i) {
        const auto& hole = m_holes[i];
        const bool measurementAvailable = i < static_cast<int>(m_measurements.size());
        for (int r = 0; r < static_cast<int>(hole.rois.size()); ++r) {
            const auto& roi = hole.rois[r];
            const bool selectedRoi = i == selectedHole && r == m_roiTable->currentRow();
            const bool failedRoi = hm::isRoiMeasurementFailed(hole.measurement, r, measurementAvailable);
            ILRotRectRegionPtr region = makeRegion(roi);
            ILDrawable::Cast(region)->SetPenColor((failedRoi || i == selectedHole) ? LPVRed : LPVGreen);
            if (failedRoi || selectedRoi) {
                ILDrawable::Cast(region)->SetPenWidth(failedRoi ? 3 : 2);
            }
            if (failedRoi || i == selectedHole) {
                const int regionId = m_displayCtrl->AddRegion(*region, VARIANT_TRUE);
                if (regionId >= 0) {
                    m_displayRegionMap[regionId] = std::make_pair(i, r);
                }
            } else {
                m_displayCtrl->AddObject(*region, 0);
            }
        }
    }

    if (selectedHole >= 0 && selectedHole < static_cast<int>(m_holes.size())) {
        const hm::ImageRect bounds = hm::makeRoiGroupBounds(m_holes[selectedHole].rois, 12.0);
        if (bounds.ok) {
            ILRectRegionPtr highlight = LRectRegion::Create();
            highlight->SetPlacement(bounds.left, bounds.top, bounds.width, bounds.height);
            ILDrawable::Cast(highlight)->SetPen(LPVPenSolid, 4, LPVRed);
            m_displayCtrl->AddObject(*highlight, 0);
        }
    }

    for (const auto& hole : m_holes) {
        const hm::GaugeLine lines[4] = {
            hole.measurement.top,
            hole.measurement.bottom,
            hole.measurement.left,
            hole.measurement.right,
        };
        for (const auto& line : lines) {
            if (!line.ok) {
                continue;
            }
            ILLinePtr displayLine = LLine::Create();
            displayLine->Set(line.start.x, line.start.y, line.end.x, line.end.y);
            ILDrawable::Cast(displayLine)->SetPen(LPVPenSolid, 2, LPVBlue);
            m_displayCtrl->AddObject(*displayLine, 0);
        }
        if (hole.measurement.ok) {
            const hm::ImagePoint center{ hole.measurement.centerX, hole.measurement.centerY };
            const auto centerLines = hm::makeCenterCrossLines(center, 6.0);
            for (const auto& line : centerLines) {
                ILLinePtr centerLine = LLine::Create();
                centerLine->Set(line.start.x, line.start.y, line.end.x, line.end.y);
                ILDrawable::Cast(centerLine)->SetPen(LPVPenSolid, 3, LPVRed);
                m_displayCtrl->AddObject(*centerLine, 0);
            }
        }
    }
    m_displayCtrl->Refresh();
}

void QtHoleMeasureWidget::displayRegionDragFinished(int regionId)
{
    if (!m_displayCtrl) {
        return;
    }

    const auto it = m_displayRegionMap.find(regionId);
    if (it == m_displayRegionMap.end()) {
        return;
    }

    const int holeIndex = it->second.first;
    const int roiIndex = it->second.second;
    if (holeIndex < 0
        || holeIndex >= static_cast<int>(m_holes.size())
        || roiIndex < 0
        || roiIndex >= static_cast<int>(m_holes[holeIndex].rois.size())) {
        return;
    }

    IDispatchPtr regionDispatch = m_displayCtrl->GetRegionByID(regionId);
    ILRotRectRegionPtr region = LRotRectRegion::Cast(regionDispatch.GetInterfacePtr());
    if (!region) {
        return;
    }

    hm::HoleRoi edited = m_holes[holeIndex].rois[roiIndex];
    edited.center.x = region->CenterX;
    edited.center.y = region->CenterY;
    edited.width = region->Width;
    edited.height = region->Height;
    edited.angleDeg = region->Angle;
    applyEditedRoi(holeIndex, roiIndex, edited);

    if (m_holeTable && m_holeTable->currentRow() != holeIndex) {
        m_holeTable->selectRow(holeIndex);
    }
    if (m_roiTable) {
        m_roiTable->selectRow(roiIndex);
    }
    clearMeasurements();
    populateRoiTable(holeIndex);
    updateSelectedRoiTableRow();
    displayImage();
}
