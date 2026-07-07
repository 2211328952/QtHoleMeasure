#include "LPVDisplayWidget.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <Windows.h>
#include <combaseapi.h>

namespace
{
const wchar_t* kLpvDisplayDll = L"F:\\Source\\StoneWall\\publish\\bin\\x64\\lpvDisplay.dll";
const char* kLpvDisplayClsid = "{82c07241-7283-4957-9ee4-200d9903d13d}";
const char* kLpvDisplayDllFactory = "LPV.LDisplay.DllFactory";

QString hresultText(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(static_cast<qulonglong>(static_cast<unsigned long>(hr)), 8, 16, QLatin1Char('0'));
}
}

LPVDisplayWidget::LPVDisplayWidget(QWidget* parent)
    : QAxWidget(parent)
{
}

bool LPVDisplayWidget::createDisplayControl()
{
    m_lastError.clear();
    if (setControl(QLatin1String(kLpvDisplayClsid)) && !isNull()) {
        return true;
    }

    clear();
    const bool ok = setControl(QLatin1String(kLpvDisplayDllFactory)) && !isNull();
    if (!ok && m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("LPV LDisplay ActiveX control is not registered");
    }
    return ok;
}

ILDisplayPtr LPVDisplayWidget::displayControl() const
{
    ILDisplay* display = nullptr;
    queryInterface(QUuid(__uuidof(ILDisplay)), reinterpret_cast<void**>(&display));
    return ILDisplayPtr(display, false);
}

QString LPVDisplayWidget::lastError() const
{
    return m_lastError;
}

bool LPVDisplayWidget::initialize(IUnknown** ptr)
{
    if (control() == QLatin1String(kLpvDisplayDllFactory)) {
        return createFromDll(ptr) && createHostWindow(true);
    }
    return QAxWidget::initialize(ptr);
}

bool LPVDisplayWidget::createFromDll(IUnknown** ptr)
{
    if (!ptr) {
        m_lastError = QStringLiteral("invalid output pointer");
        return false;
    }
    *ptr = nullptr;

    const QString oldPath = QString::fromLocal8Bit(qgetenv("PATH"));
    const QString lpvBinDir = QFileInfo(QString::fromWCharArray(kLpvDisplayDll)).absolutePath();
    qputenv("PATH", QDir::toNativeSeparators(lpvBinDir + QLatin1Char(';') + oldPath).toLocal8Bit());

    HMODULE module = LoadLibraryW(kLpvDisplayDll);
    if (!module) {
        m_lastError = QStringLiteral("LoadLibrary lpvDisplay.dll failed: %1").arg(GetLastError());
        qputenv("PATH", oldPath.toLocal8Bit());
        return false;
    }

    using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
    auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(module, "DllGetClassObject"));
    if (!getClassObject) {
        m_lastError = QStringLiteral("DllGetClassObject not found in lpvDisplay.dll");
        qputenv("PATH", oldPath.toLocal8Bit());
        return false;
    }

    IClassFactory* factory = nullptr;
    HRESULT hr = getClassObject(__uuidof(LDisplay), IID_IClassFactory, reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) {
        m_lastError = QStringLiteral("DllGetClassObject(LDisplay) failed: %1").arg(hresultText(hr));
        qputenv("PATH", oldPath.toLocal8Bit());
        return false;
    }

    hr = factory->CreateInstance(nullptr, IID_IUnknown, reinterpret_cast<void**>(ptr));
    factory->Release();
    if (FAILED(hr) || !*ptr) {
        m_lastError = QStringLiteral("CreateInstance(LDisplay) failed: %1").arg(hresultText(hr));
        qputenv("PATH", oldPath.toLocal8Bit());
        return false;
    }

    qputenv("PATH", oldPath.toLocal8Bit());
    return true;
}
